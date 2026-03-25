#include "View.h"

#include "render/Prim.h"
#include "render/Renderer.h"
#include "DebugLog.h"
#include "session/Session.h"
#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float kDragLongitudePerPixel = 0.4f;
constexpr float kDragLatitudePerPixel = -0.3f;
constexpr float kWheelDistancePerNotch = -60.0f;
constexpr float kDefaultBlendFactor = 0.1f;
constexpr float kResetBlendFactor = 0.18f;
constexpr int kResetBlendFrameCount = 18;
constexpr float kNearPlane = 10.0f;
constexpr float kGroundSubmitNearPlane = 80.0f;
constexpr u32 kPerfLogIntervalFrames = 120;

struct ViewPerfStats {
    u64 frames = 0;
    u64 groundMs = 0;
    u64 hoverMs = 0;
    u64 actorMs = 0;
    u64 backgroundMs = 0;
};

ViewPerfStats g_viewPerfStats;

CView::CameraConstraints DefaultCameraConstraints()
{
    return CView::CameraConstraints{
        60.0f,
        1200.0f,
        700.0f,
        -85.0f,
        -10.0f,
        -55.0f,
        0.0f,
        false,
        false,
        -360.0f,
        360.0f,
    };
}

matrix MakeViewMatrix(const vector3d& eye, const vector3d& at, const vector3d& up)
{
    vector3d zaxis{ at.x - eye.x, at.y - eye.y, at.z - eye.z };
    const float zlengthSq = zaxis.x * zaxis.x + zaxis.y * zaxis.y + zaxis.z * zaxis.z;
    if (zlengthSq > 1.0e-12f) {
        const float invLength = 1.0f / std::sqrt(zlengthSq);
        zaxis.x *= invLength;
        zaxis.y *= invLength;
        zaxis.z *= invLength;
    }

    vector3d xaxis{
        up.y * zaxis.z - up.z * zaxis.y,
        up.z * zaxis.x - up.x * zaxis.z,
        up.x * zaxis.y - up.y * zaxis.x
    };
    const float xlengthSq = xaxis.x * xaxis.x + xaxis.y * xaxis.y + xaxis.z * xaxis.z;
    if (xlengthSq > 1.0e-12f) {
        const float invLength = 1.0f / std::sqrt(xlengthSq);
        xaxis.x *= invLength;
        xaxis.y *= invLength;
        xaxis.z *= invLength;
    }

    const vector3d yaxis{
        zaxis.y * xaxis.z - zaxis.z * xaxis.y,
        zaxis.z * xaxis.x - zaxis.x * xaxis.z,
        zaxis.x * xaxis.y - zaxis.y * xaxis.x
    };

    matrix view{};
    MatrixIdentity(view);
    view.m[0][0] = xaxis.x;
    view.m[1][0] = xaxis.y;
    view.m[2][0] = xaxis.z;
    view.m[0][1] = yaxis.x;
    view.m[1][1] = yaxis.y;
    view.m[2][1] = yaxis.z;
    view.m[0][2] = zaxis.x;
    view.m[1][2] = zaxis.y;
    view.m[2][2] = zaxis.z;
    view.m[3][0] = -(xaxis.x * eye.x + xaxis.y * eye.y + xaxis.z * eye.z);
    view.m[3][1] = -(yaxis.x * eye.x + yaxis.y * eye.y + yaxis.z * eye.z);
    view.m[3][2] = -(zaxis.x * eye.x + zaxis.y * eye.y + zaxis.z * eye.z);
    return view;
}

float ClampFloat(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(maxValue, value));
}

float NormalizeLongitudeDelta(float delta)
{
    while (delta >= 180.0f) {
        delta -= 360.0f;
    }
    while (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}

float TileToWorldCoordX(const CWorld* world, int tileX)
{
    const int width = world && world->m_attr ? world->m_attr->m_width : (world && world->m_ground ? world->m_ground->m_width : 0);
    const float zoom = world && world->m_attr ? static_cast<float>(world->m_attr->m_zoom) : (world && world->m_ground ? world->m_ground->m_zoom : 5.0f);
    return (static_cast<float>(tileX) - static_cast<float>(width) * 0.5f) * zoom + zoom * 0.5f;
}

float TileToWorldCoordZ(const CWorld* world, int tileY)
{
    const int height = world && world->m_attr ? world->m_attr->m_height : (world && world->m_ground ? world->m_ground->m_height : 0);
    const float zoom = world && world->m_attr ? static_cast<float>(world->m_attr->m_zoom) : (world && world->m_ground ? world->m_ground->m_zoom : 5.0f);
    return (static_cast<float>(tileY) - static_cast<float>(height) * 0.5f) * zoom + zoom * 0.5f;
}

float AttrCoordX(int x, int width, float zoom)
{
    return (static_cast<float>(x) - static_cast<float>(width) * 0.5f) * zoom;
}

float AttrCoordZ(int y, int height, float zoom)
{
    return (static_cast<float>(y) - static_cast<float>(height) * 0.5f) * zoom;
}

vector3d NormalizeVec3(const vector3d& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 0.0f, 1.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

vector3d TransformDirection(const matrix& m, const vector3d& dir)
{
    return vector3d{
        dir.x * m.m[0][0] + dir.y * m.m[1][0] + dir.z * m.m[2][0],
        dir.x * m.m[0][1] + dir.y * m.m[1][1] + dir.z * m.m[2][1],
        dir.x * m.m[0][2] + dir.y * m.m[1][2] + dir.z * m.m[2][2]
    };
}

bool ProjectPoint(const CRenderer& renderer, const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= kGroundSubmitNearPlane) {
        return false;
    }

    const float oow = 1.0f / clipZ;
    const float projectedX = point.x * viewMatrix.m[0][0]
        + point.y * viewMatrix.m[1][0]
        + point.z * viewMatrix.m[2][0]
        + viewMatrix.m[3][0];
    const float projectedY = point.x * viewMatrix.m[0][1]
        + point.y * viewMatrix.m[1][1]
        + point.z * viewMatrix.m[2][1]
        + viewMatrix.m[3][1];
    if (!std::isfinite(oow) || !std::isfinite(projectedX) || !std::isfinite(projectedY)) {
        return false;
    }

    outVertex->x = renderer.m_xoffset + projectedX * renderer.m_hpc * oow;
    outVertex->y = renderer.m_yoffset + projectedY * renderer.m_vpc * oow;
    const float depth = (1500.0f / (1500.0f - kNearPlane)) * ((1.0f / oow) - kNearPlane) * oow;
    if (!std::isfinite(outVertex->x) || !std::isfinite(outVertex->y) || !std::isfinite(depth)) {
        return false;
    }

    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, depth));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return true;
}

float EdgeFunction2D(float px, float py, const tlvertex3d& a, const tlvertex3d& b)
{
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

bool PointInTriangle2D(float px, float py, const tlvertex3d& a, const tlvertex3d& b, const tlvertex3d& c)
{
    const float ab = EdgeFunction2D(px, py, a, b);
    const float bc = EdgeFunction2D(px, py, b, c);
    const float ca = EdgeFunction2D(px, py, c, a);
    const bool hasNegative = (ab < 0.0f) || (bc < 0.0f) || (ca < 0.0f);
    const bool hasPositive = (ab > 0.0f) || (bc > 0.0f) || (ca > 0.0f);
    return !(hasNegative && hasPositive);
}

bool WorldToAttrCell(const C3dAttr* attr, float worldX, float worldZ, int* outAttrX, int* outAttrY)
{
    if (!attr || !outAttrX || !outAttrY || attr->m_zoom <= 0) {
        return false;
    }

    const float zoom = static_cast<float>(attr->m_zoom);
    const float localX = static_cast<float>(attr->m_width) * zoom * 0.5f + worldX;
    const float localZ = static_cast<float>(attr->m_height) * zoom * 0.5f + worldZ;
    const int cellX = static_cast<int>(localX / zoom);
    const int cellY = static_cast<int>(localZ / zoom);
    if (cellX < 0 || cellX >= attr->m_width || cellY < 0 || cellY >= attr->m_height) {
        return false;
    }

    *outAttrX = cellX;
    *outAttrY = cellY;
    return true;
}

bool TestAttrCellHit(const C3dAttr* attr,
    const matrix& viewMatrix,
    int attrX,
    int attrY,
    float screenX,
    float screenY,
    float* outDepth)
{
    if (!attr || !outDepth || attrX < 0 || attrY < 0 || attrX >= attr->m_width || attrY >= attr->m_height) {
        return false;
    }

    const float zoom = static_cast<float>(attr->m_zoom);
    const CAttrCell& cell = attr->m_cells[static_cast<size_t>(attrY) * static_cast<size_t>(attr->m_width) + static_cast<size_t>(attrX)];
    const float x0 = AttrCoordX(attrX, attr->m_width, zoom);
    const float x1 = AttrCoordX(attrX + 1, attr->m_width, zoom);
    const float z0 = AttrCoordZ(attrY, attr->m_height, zoom);
    const float z1 = AttrCoordZ(attrY + 1, attr->m_height, zoom);

    tlvertex3d projected[4]{};
    if (!ProjectPoint(g_renderer, viewMatrix, vector3d{ x0, cell.h1, z0 }, &projected[0])
        || !ProjectPoint(g_renderer, viewMatrix, vector3d{ x1, cell.h2, z0 }, &projected[1])
        || !ProjectPoint(g_renderer, viewMatrix, vector3d{ x0, cell.h3, z1 }, &projected[2])
        || !ProjectPoint(g_renderer, viewMatrix, vector3d{ x1, cell.h4, z1 }, &projected[3])) {
        return false;
    }

    if (!PointInTriangle2D(screenX, screenY, projected[0], projected[1], projected[2])
        && !PointInTriangle2D(screenX, screenY, projected[2], projected[1], projected[3])) {
        return false;
    }

    *outDepth = (std::min)(
        (std::min)(projected[0].z, projected[1].z),
        (std::min)(projected[2].z, projected[3].z));
    return true;
}

bool SearchAttrCellRegion(const C3dAttr* attr,
    const matrix& viewMatrix,
    int centerX,
    int centerY,
    int radius,
    float screenX,
    float screenY,
    float* ioBestDepth,
    int* ioBestX,
    int* ioBestY)
{
    if (!attr || !ioBestDepth || !ioBestX || !ioBestY) {
        return false;
    }

    const int minX = (std::max)(0, centerX - radius);
    const int maxX = (std::min)(attr->m_width - 1, centerX + radius);
    const int minY = (std::max)(0, centerY - radius);
    const int maxY = (std::min)(attr->m_height - 1, centerY + radius);

    bool found = false;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float depth = 1.0f;
            if (!TestAttrCellHit(attr, viewMatrix, x, y, screenX, screenY, &depth)) {
                continue;
            }

            if (!found || depth < *ioBestDepth) {
                found = true;
                *ioBestDepth = depth;
                *ioBestX = x;
                *ioBestY = y;
            }
        }
    }

    return found;
}

} // namespace

CView::CView()
    : m_world(nullptr),
      m_constraints(DefaultCameraConstraints()),
      m_from{ 0.0f, 0.0f, 0.0f },
      m_up{ 0.0f, 1.0f, 0.0f },
      m_initialized(false),
      m_resetBlendFrames(0),
      m_hoverAttrX(-1),
      m_hoverAttrY(-1)
{
    MatrixIdentity(m_viewMatrix);
    MatrixIdentity(m_invViewMatrix);
    OnEnterFrame();
}

CView::~CView() = default;

void CView::SetWorld(CWorld* world)
{
    m_world = world;
}

void CView::SetCameraConstraints(const CameraConstraints& constraints)
{
    m_constraints = constraints;
    ClampCameraState(&m_cur);
    ClampCameraState(&m_dest);
}

void CView::SetInitialCamera(float longitude, float latitude, float distance)
{
    m_cur.longitude = longitude;
    m_dest.longitude = longitude;
    m_cur.latitude = latitude;
    m_dest.latitude = latitude;
    m_cur.distance = distance;
    m_dest.distance = distance;
    ClampCameraState(&m_cur);
    ClampCameraState(&m_dest);
    m_initialized = false;
}

void CView::OnEnterFrame()
{
    m_cur.distance = m_constraints.defaultDistance;
    m_dest.distance = m_constraints.defaultDistance;
    m_cur.longitude = m_constraints.defaultLongitude;
    m_dest.longitude = m_constraints.defaultLongitude;
    m_cur.latitude = m_constraints.defaultLatitude;
    m_dest.latitude = m_constraints.defaultLatitude;
    m_cur.at = vector3d{ 0.0f, 0.0f, 0.0f };
    m_dest.at = vector3d{ 0.0f, 0.0f, 0.0f };
    m_up = vector3d{ 0.0f, 1.0f, 0.0f };
    m_initialized = false;
    m_resetBlendFrames = 0;
    m_hoverAttrX = -1;
    m_hoverAttrY = -1;
}

void CView::OnExitFrame()
{
}

void CView::AddDistance(float delta)
{
    m_dest.distance = ClampFloat(m_dest.distance + delta, m_constraints.minDistance, m_constraints.maxDistance);
}

void CView::AddLatitude(float delta)
{
    m_dest.latitude = ClampFloat(m_dest.latitude + delta, m_constraints.minLatitude, m_constraints.maxLatitude);
}

void CView::AddLongitude(float delta)
{
    m_dest.longitude = ClampLongitude(m_dest.longitude + delta);
}

void CView::RotateByDrag(int deltaX, int deltaY)
{
    AddLongitude(static_cast<float>(deltaX) * kDragLongitudePerPixel);
    AddLatitude(static_cast<float>(deltaY) * kDragLatitudePerPixel);
}

void CView::ZoomByWheel(int wheelDelta)
{
    if (wheelDelta == 0) {
        return;
    }

    const float notches = static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
    AddDistance(notches * kWheelDistancePerNotch);
}

void CView::ResetToDefaultOrientation()
{
    m_dest.longitude = ClampLongitude(m_constraints.defaultLongitude);
    m_dest.latitude = ClampFloat(m_constraints.defaultLatitude, m_constraints.minLatitude, m_constraints.maxLatitude);
    m_resetBlendFrames = kResetBlendFrameCount;
}

void CView::UpdateHoverCellFromScreen(int screenX, int screenY)
{
    int attrX = -1;
    int attrY = -1;
    if (ScreenToAttrCell(screenX, screenY, &attrX, &attrY)) {
        m_hoverAttrX = attrX;
        m_hoverAttrY = attrY;
    } else {
        ClearHoverCell();
    }
}

void CView::ClearHoverCell()
{
    m_hoverAttrX = -1;
    m_hoverAttrY = -1;
}

bool CView::ScreenToHoveredAttrCell(int screenX, int screenY, int* outAttrX, int* outAttrY) const
{
    return ScreenToAttrCell(screenX, screenY, outAttrX, outAttrY);
}

bool CView::ScreenToAttrCell(int screenX, int screenY, int* outAttrX, int* outAttrY) const
{
    if (!m_world || !m_world->m_attr || !outAttrX || !outAttrY) {
        return false;
    }

    if (g_renderer.m_width <= 0 || g_renderer.m_height <= 0 || g_renderer.m_hpc == 0.0f || g_renderer.m_vpc == 0.0f) {
        return false;
    }

    const C3dAttr* attr = m_world->m_attr;
    const float zoom = static_cast<float>(attr->m_zoom);
    if (zoom <= 0.0f || attr->m_cells.empty()) {
        return false;
    }

    const float px = static_cast<float>(screenX);
    const float py = static_cast<float>(screenY);
    float bestDepth = 1.0f;
    bool found = false;
    int bestX = -1;
    int bestY = -1;

    if (m_hoverAttrX >= 0 && m_hoverAttrY >= 0) {
        found = SearchAttrCellRegion(attr, m_viewMatrix, m_hoverAttrX, m_hoverAttrY, 4, px, py, &bestDepth, &bestX, &bestY);
    }

    if (!found) {
        const vector3d cameraDir = NormalizeVec3(vector3d{
            (static_cast<float>(screenX) - static_cast<float>(g_renderer.m_xoffset)) / g_renderer.m_hpc,
            (static_cast<float>(screenY) - static_cast<float>(g_renderer.m_yoffset)) / g_renderer.m_vpc,
            1.0f
        });
        const vector3d rayDir = NormalizeVec3(TransformDirection(m_invViewMatrix, cameraDir));
        const float terrainSpanX = static_cast<float>(attr->m_width) * zoom;
        const float terrainSpanZ = static_cast<float>(attr->m_height) * zoom;
        const float maxDistance = std::sqrt(terrainSpanX * terrainSpanX + terrainSpanZ * terrainSpanZ) + 1024.0f;
        const float step = (std::max)(zoom, 8.0f);
        int lastCellX = (std::numeric_limits<int>::min)();
        int lastCellY = (std::numeric_limits<int>::min)();

        for (float t = 0.0f; t <= maxDistance; t += step) {
            const vector3d sample{
                m_from.x + rayDir.x * t,
                m_from.y + rayDir.y * t,
                m_from.z + rayDir.z * t
            };

            int cellX = -1;
            int cellY = -1;
            if (!WorldToAttrCell(attr, sample.x, sample.z, &cellX, &cellY)) {
                continue;
            }

            if (cellX == lastCellX && cellY == lastCellY) {
                continue;
            }

            lastCellX = cellX;
            lastCellY = cellY;
            if (SearchAttrCellRegion(attr, m_viewMatrix, cellX, cellY, 1, px, py, &bestDepth, &bestX, &bestY)) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        return false;
    }

    *outAttrX = bestX;
    *outAttrY = bestY;
    return true;
}

vector3d CView::ResolveTargetPosition() const
{
    if (m_world && m_world->m_player) {
        const vector3d playerPos = m_world->m_player->m_pos;
        if (std::isfinite(playerPos.x) && std::isfinite(playerPos.y) && std::isfinite(playerPos.z)) {
            return playerPos;
        }
    }

    const float worldX = TileToWorldCoordX(m_world, g_session.m_playerPosX);
    const float worldZ = TileToWorldCoordZ(m_world, g_session.m_playerPosY);
    const float worldY = m_world && m_world->m_attr ? m_world->m_attr->GetHeight(worldX, worldZ) : 0.0f;
    return vector3d{ worldX, worldY, worldZ };
}

float CView::ClampLongitude(float longitude) const
{
    if (m_constraints.lockLongitude) {
        return m_constraints.defaultLongitude;
    }

    if (!m_constraints.constrainLongitude) {
        return longitude;
    }

    const float longitudeSpan = m_constraints.maxLongitude - m_constraints.minLongitude;
    if (longitudeSpan >= 360.0f) {
        return longitude;
    }

    return ClampFloat(longitude, m_constraints.minLongitude, m_constraints.maxLongitude);
}

void CView::ClampCameraState(CameraState* state) const
{
    if (!state) {
        return;
    }

    state->distance = ClampFloat(state->distance, m_constraints.minDistance, m_constraints.maxDistance);
    state->latitude = ClampFloat(state->latitude, m_constraints.minLatitude, m_constraints.maxLatitude);
    state->longitude = ClampLongitude(state->longitude);
}

void CView::InterpolateViewInfo()
{
    if (!m_initialized) {
        ClampCameraState(&m_dest);
        m_cur = m_dest;
        m_initialized = true;
        return;
    }

    ClampCameraState(&m_dest);

    float deltaLongitude = m_dest.longitude - m_cur.longitude;
    if (!m_constraints.lockLongitude
        && (!m_constraints.constrainLongitude || (m_constraints.maxLongitude - m_constraints.minLongitude) >= 360.0f)) {
        deltaLongitude = NormalizeLongitudeDelta(deltaLongitude);
    }

    const float blendFactor = m_resetBlendFrames > 0 ? kResetBlendFactor : kDefaultBlendFactor;
    m_cur.longitude += deltaLongitude * blendFactor;
    if (!m_constraints.lockLongitude
        && (!m_constraints.constrainLongitude || (m_constraints.maxLongitude - m_constraints.minLongitude) >= 360.0f)) {
        if (m_cur.longitude >= 360.0f) {
            m_cur.longitude -= 360.0f;
        }
        if (m_cur.longitude < 0.0f) {
            m_cur.longitude += 360.0f;
        }
    } else {
        m_cur.longitude = ClampLongitude(m_cur.longitude);
    }

    m_cur.latitude += (m_dest.latitude - m_cur.latitude) * blendFactor;
    m_cur.distance += (m_dest.distance - m_cur.distance) * blendFactor;
    m_cur.at = m_dest.at;
    ClampCameraState(&m_cur);

    if (m_resetBlendFrames > 0) {
        --m_resetBlendFrames;
    }
}

void CView::BuildViewMatrix()
{
    matrix rotation{};
    MatrixIdentity(rotation);
    MatrixAppendXRotation(rotation, m_cur.latitude * 3.14159265f / 180.0f);
    MatrixAppendYRotation(rotation, m_cur.longitude * 3.14159265f / 180.0f);

    const float distanceOffset = -m_cur.distance;
    m_from.x = rotation.m[2][0] * distanceOffset + rotation.m[3][0] + m_cur.at.x;
    m_from.y = rotation.m[2][1] * distanceOffset + rotation.m[3][1] + m_cur.at.y;
    m_from.z = rotation.m[2][2] * distanceOffset + rotation.m[3][2] + m_cur.at.z;

    if (m_world && m_world->m_attr) {
        const float terrainHeight = m_world->m_attr->GetHeight(m_from.x, m_from.z);
        if (terrainHeight < m_from.y) {
            m_from.y = terrainHeight;
        }
    }

    m_viewMatrix = MakeViewMatrix(m_from, m_cur.at, m_up);
    MatrixInverse(m_invViewMatrix, m_viewMatrix);
    g_renderer.SetLookAt(m_from, m_cur.at, m_up);
}

void CView::OnCalcViewInfo()
{
    m_dest.at = ResolveTargetPosition();
    InterpolateViewInfo();
    BuildViewMatrix();

    static bool loggedViewState = false;
    if (!loggedViewState) {
        loggedViewState = true;
        DbgLog("[View] camera from=(%.2f, %.2f, %.2f) at=(%.2f, %.2f, %.2f) dist=%.2f lon=%.2f lat=%.2f\n",
            m_from.x,
            m_from.y,
            m_from.z,
            m_cur.at.x,
            m_cur.at.y,
            m_cur.at.z,
            m_cur.distance,
            m_cur.longitude,
            m_cur.latitude);
    }
}

void CView::OnRender()
{
    if (!m_world || !m_world->m_ground) {
        return;
    }

    const DWORD groundStart = GetTickCount();
    m_world->m_ground->FlushGround(m_viewMatrix);
    const DWORD groundEnd = GetTickCount();

    const DWORD hoverStart = groundEnd;
    if (m_hoverAttrX >= 0 && m_hoverAttrY >= 0) {
        m_world->m_ground->RenderAttrTile(m_viewMatrix, m_hoverAttrX, m_hoverAttrY, 0x70FFFFFFu);
    }
    const DWORD hoverEnd = GetTickCount();

    const DWORD actorStart = hoverEnd;
    m_world->RenderActors(m_viewMatrix, m_cur.longitude);
    const DWORD actorEnd = GetTickCount();

    const DWORD backgroundStart = actorEnd;
    m_world->RenderBackgroundObjects(m_viewMatrix);
    const DWORD backgroundEnd = GetTickCount();

    g_viewPerfStats.frames += 1;
    g_viewPerfStats.groundMs += static_cast<u64>(groundEnd - groundStart);
    g_viewPerfStats.hoverMs += static_cast<u64>(hoverEnd - hoverStart);
    g_viewPerfStats.actorMs += static_cast<u64>(actorEnd - actorStart);
    g_viewPerfStats.backgroundMs += static_cast<u64>(backgroundEnd - backgroundStart);
}