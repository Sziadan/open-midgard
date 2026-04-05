#include "View.h"

#include "render/Prim.h"
#include "render/Renderer.h"
#include "DebugLog.h"
#include "session/Session.h"
#include "world/World.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float kDragLongitudePerPixel = 0.4f;
constexpr float kDragLatitudePerPixel = -0.3f;
constexpr float kWheelDistancePerNotch = -60.0f;
// Ref\View.cpp CView::InterpolateViewInfo (~0x5097f0): each sub-step,
//   cur += (dest - cur) * 0.1
// for longitude (shortest arc), latitude, and distance. The decompile copies
// look-at from dest in one shot; we also damp m_cur.at with the same 0.1 so
// the chase pivot eases when the player starts/stops (one damping constant).
constexpr float kRefInterpolateViewFactor = 0.1f;
constexpr float kNearPlane = 10.0f;
constexpr float kGroundSubmitNearPlane = 80.0f;
constexpr DWORD kHoverReuseMs = 33;
constexpr u32 kPerfLogIntervalFrames = 120;

struct ViewPerfStats {
    u64 frames = 0;
    u64 groundMs = 0;
    u64 hoverMs = 0;
    u64 actorMs = 0;
    u64 backgroundMs = 0;
};

ViewPerfStats g_viewPerfStats;

struct ViewMovePerfStats {
    u64 frames = 0;
    u64 hoverCalls = 0;
    double interpolateMs = 0.0;
    double hoverResolveMs = 0.0;
    double groundMs = 0.0;
    double actorMs = 0.0;
    double backgroundMs = 0.0;
};

ViewMovePerfStats g_viewMovePerfStats;

struct ViewHiResStats {
    u64 frames = 0;
    double groundMs = 0.0;
    double hoverMs = 0.0;
    double actorMs = 0.0;
    double backgroundMs = 0.0;
    u64 renderedGameObjects = 0;
    u64 renderedFixedEffects = 0;
    u64 renderedItems = 0;
    u64 renderedBillboards = 0;
    u64 renderedBackgroundObjects = 0;
    u64 skippedTinyBackgroundObjects = 0;
    u64 portalBootstrapActors = 0;
    double actorGameObjectMs = 0.0;
    double actorItemMs = 0.0;
    double actorPortalMs = 0.0;
    double actorBillboardBuildMs = 0.0;
    double actorBillboardSortMs = 0.0;
    double actorBillboardRenderMs = 0.0;
    double backgroundDrawMs = 0.0;
};

ViewHiResStats g_viewHiResStats;

double QpcNowMs()
{
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
}

bool IsMovePerfActive(const CWorld* world)
{
    return world && world->m_player && world->m_player->m_isMoving;
}

void LogViewMovePerfIfNeeded()
{
    if (g_viewMovePerfStats.frames == 0 || (g_viewMovePerfStats.frames % 30u) != 0) {
        return;
    }

    const double frameCount = static_cast<double>(g_viewMovePerfStats.frames);
    const double hoverCallCount = static_cast<double>((std::max)(u64{1}, g_viewMovePerfStats.hoverCalls));
    DbgLog("[ViewPerfHiRes] moveFrames=%llu interp=%.3fms hoverFrame=%.3fms hoverCalls=%llu hoverCall=%.3fms ground=%.3fms actors=%.3fms background=%.3fms\n",
        static_cast<unsigned long long>(g_viewMovePerfStats.frames),
        g_viewMovePerfStats.interpolateMs / frameCount,
        g_viewMovePerfStats.hoverResolveMs / frameCount,
        static_cast<unsigned long long>(g_viewMovePerfStats.hoverCalls),
        g_viewMovePerfStats.hoverResolveMs / hoverCallCount,
        g_viewMovePerfStats.groundMs / frameCount,
        g_viewMovePerfStats.actorMs / frameCount,
        g_viewMovePerfStats.backgroundMs / frameCount);
    g_viewMovePerfStats = ViewMovePerfStats{};
}

void LogViewHiResPerfIfNeeded(const CWorld* world)
{
    if (!world || g_viewHiResStats.frames == 0 || (g_viewHiResStats.frames % kPerfLogIntervalFrames) != 0) {
        return;
    }

    const double frameCount = static_cast<double>(g_viewHiResStats.frames);
    DbgLog("[ViewPerfHiRes] frames=%llu ground=%.3fms hover=%.3fms actors=%.3fms background=%.3fms actorObj=%.3fms actorItems=%.3fms actorPortal=%.3fms bbBuild=%.3fms bbSort=%.3fms bbDraw=%.3fms bgDraw=%.3fms gameObjects=%.2f fixedEffects=%.2f items=%.2f billboards=%.2f bgObjs=%.2f bgTinySkip=%.2f portalActors=%.2f\n",
        static_cast<unsigned long long>(g_viewHiResStats.frames),
        g_viewHiResStats.groundMs / frameCount,
        g_viewHiResStats.hoverMs / frameCount,
        g_viewHiResStats.actorMs / frameCount,
        g_viewHiResStats.backgroundMs / frameCount,
        g_viewHiResStats.actorGameObjectMs / frameCount,
        g_viewHiResStats.actorItemMs / frameCount,
        g_viewHiResStats.actorPortalMs / frameCount,
        g_viewHiResStats.actorBillboardBuildMs / frameCount,
        g_viewHiResStats.actorBillboardSortMs / frameCount,
        g_viewHiResStats.actorBillboardRenderMs / frameCount,
        g_viewHiResStats.backgroundDrawMs / frameCount,
        static_cast<double>(g_viewHiResStats.renderedGameObjects) / frameCount,
        static_cast<double>(g_viewHiResStats.renderedFixedEffects) / frameCount,
        static_cast<double>(g_viewHiResStats.renderedItems) / frameCount,
        static_cast<double>(g_viewHiResStats.renderedBillboards) / frameCount,
        static_cast<double>(g_viewHiResStats.renderedBackgroundObjects) / frameCount,
        static_cast<double>(g_viewHiResStats.skippedTinyBackgroundObjects) / frameCount,
        static_cast<double>(g_viewHiResStats.portalBootstrapActors) / frameCount);
    g_viewHiResStats = ViewHiResStats{};
}

bool IsWalkableAttrCell(const C3dAttr* attr, int attrX, int attrY)
{
    if (!attr || attrX < 0 || attrY < 0 || attrX >= attr->m_width || attrY >= attr->m_height || attr->m_cells.empty()) {
        return false;
    }

    const size_t cellIndex = static_cast<size_t>(attrY) * static_cast<size_t>(attr->m_width) + static_cast<size_t>(attrX);
    if (cellIndex >= attr->m_cells.size()) {
        return false;
    }

    return attr->m_cells[cellIndex].flag == 0;
}

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

void WrapLongitude0To360InPlace(float* longitude)
{
    if (!longitude) {
        return;
    }
    float lon = *longitude;
    while (lon >= 360.0f) {
        lon -= 360.0f;
    }
    while (lon < 0.0f) {
        lon += 360.0f;
    }
    *longitude = lon;
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
      m_viewRevision(0),
      m_viewSnapTag(0),
      m_hoverAttrX(-1),
      m_hoverAttrY(-1),
      m_hoverCacheRevision(0),
      m_hoverCacheTick(0),
      m_hoverCacheScreenX(0),
      m_hoverCacheScreenY(0),
      m_hoverCacheAttrX(-1),
      m_hoverCacheAttrY(-1),
      m_hoverCacheResolved(false),
      m_hoverCacheHasCell(false)
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
    m_viewRevision = 0;
    m_viewSnapTag = 0;
    m_hoverAttrX = -1;
    m_hoverAttrY = -1;
    m_hoverCacheRevision = 0;
    m_hoverCacheTick = 0;
    m_hoverCacheAttrX = -1;
    m_hoverCacheAttrY = -1;
    m_hoverCacheResolved = false;
    m_hoverCacheHasCell = false;
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
    if (!outAttrX || !outAttrY) {
        return false;
    }

    const DWORD now = GetTickCount();
    if (m_hoverCacheResolved
        && m_hoverCacheScreenX == screenX
        && m_hoverCacheScreenY == screenY
        && now - m_hoverCacheTick <= kHoverReuseMs) {
        if (!m_hoverCacheHasCell) {
            return false;
        }
        *outAttrX = m_hoverCacheAttrX;
        *outAttrY = m_hoverCacheAttrY;
        return true;
    }

    int attrX = -1;
    int attrY = -1;
    const bool found = ScreenToAttrCellUncached(screenX, screenY, &attrX, &attrY);
    m_hoverCacheRevision = m_viewRevision;
    m_hoverCacheTick = now;
    m_hoverCacheScreenX = screenX;
    m_hoverCacheScreenY = screenY;
    m_hoverCacheAttrX = attrX;
    m_hoverCacheAttrY = attrY;
    m_hoverCacheResolved = true;
    m_hoverCacheHasCell = found;
    if (!found) {
        return false;
    }

    *outAttrX = attrX;
    *outAttrY = attrY;
    return true;
}

bool CView::ScreenToAttrCellUncached(int screenX, int screenY, int* outAttrX, int* outAttrY) const
{
    const double perfStartMs = IsMovePerfActive(m_world) ? QpcNowMs() : 0.0;
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
        if (perfStartMs != 0.0) {
            g_viewMovePerfStats.hoverCalls += 1;
            g_viewMovePerfStats.hoverResolveMs += QpcNowMs() - perfStartMs;
        }
        return false;
    }

    *outAttrX = bestX;
    *outAttrY = bestY;
    if (perfStartMs != 0.0) {
        g_viewMovePerfStats.hoverCalls += 1;
        g_viewMovePerfStats.hoverResolveMs += QpcNowMs() - perfStartMs;
    }
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
    const double perfStartMs = IsMovePerfActive(m_world) ? QpcNowMs() : 0.0;
    const bool wrapLongitude = !m_constraints.lockLongitude
        && (!m_constraints.constrainLongitude || (m_constraints.maxLongitude - m_constraints.minLongitude) >= 360.0f);

    if (!m_initialized) {
        ClampCameraState(&m_dest);
        m_cur = m_dest;
        if (wrapLongitude) {
            WrapLongitude0To360InPlace(&m_dest.longitude);
            WrapLongitude0To360InPlace(&m_cur.longitude);
        }
        m_initialized = true;
        return;
    }

    ClampCameraState(&m_dest);

    if (wrapLongitude) {
        WrapLongitude0To360InPlace(&m_dest.longitude);
        WrapLongitude0To360InPlace(&m_cur.longitude);
    }

    float deltaLongitude = m_dest.longitude - m_cur.longitude;
    if (wrapLongitude) {
        deltaLongitude = NormalizeLongitudeDelta(deltaLongitude);
    }

    // Ref applies the same 0.1 factor to longitude delta, latitude, and distance.
    m_cur.longitude += deltaLongitude * kRefInterpolateViewFactor;
    if (wrapLongitude) {
        WrapLongitude0To360InPlace(&m_cur.longitude);
    } else {
        m_cur.longitude = ClampLongitude(m_cur.longitude);
    }

    m_cur.at.x += (m_dest.at.x - m_cur.at.x) * kRefInterpolateViewFactor;
    m_cur.at.y += (m_dest.at.y - m_cur.at.y) * kRefInterpolateViewFactor;
    m_cur.at.z += (m_dest.at.z - m_cur.at.z) * kRefInterpolateViewFactor;
    m_cur.latitude += (m_dest.latitude - m_cur.latitude) * kRefInterpolateViewFactor;
    m_cur.distance += (m_dest.distance - m_cur.distance) * kRefInterpolateViewFactor;
    ClampCameraState(&m_cur);
    if (perfStartMs != 0.0) {
        g_viewMovePerfStats.interpolateMs += QpcNowMs() - perfStartMs;
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

u64 CView::BillboardFrameCacheSnapTag() const
{
    // Coarse buckets so damped m_cur.at (0.1 toward dest) and smooth camera motion
    // do not change the tag every frame — fine quantization caused a full billboard
    // rebuild almost every tick while moving (huge FPS cost). Hover only needs
    // roughly stable screen rects; BuildBillboardRenderEntry still uses the exact
    // viewMatrix passed into EnsureBillboardFrameCache.
    u64 h = 1469598103934665603ull;
    auto mix = [&h](float v, float scale) {
        const u64 q = static_cast<u64>(static_cast<u32>(std::lround(v * scale)));
        h ^= q;
        h *= 1099511628211ull;
    };
    constexpr float kAtBucketWorld = 0.5f; // lround(pos * 0.5) → ~2 world units per bucket
    constexpr float kEyeYBucketWorld = 1.0f;
    constexpr float kAngleBucketDeg = 1.0f; // ~1° lon/lat
    constexpr float kDistBucketWorld = 0.2f; // ~5 units per bucket
    mix(m_cur.at.x, kAtBucketWorld);
    mix(m_cur.at.y, kAtBucketWorld);
    mix(m_cur.at.z, kAtBucketWorld);
    mix(m_from.y, kEyeYBucketWorld);
    mix(m_cur.longitude, kAngleBucketDeg);
    mix(m_cur.latitude, kAngleBucketDeg);
    mix(m_cur.distance, kDistBucketWorld);
    return h;
}

void CView::OnCalcViewInfo()
{
    m_dest.at = ResolveTargetPosition();
    InterpolateViewInfo();
    BuildViewMatrix();
    ++m_viewRevision;
    m_viewSnapTag = BillboardFrameCacheSnapTag();
    if (m_world) {
        m_world->SyncBillboardFrameCacheKey(m_viewSnapTag);
    }

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

    static bool loggedFirstRealRenderStart = false;
    if (!loggedFirstRealRenderStart) {
        loggedFirstRealRenderStart = true;
        DbgLog("[View] first real render start world=%p ground=%p attr=%p player=%p\n",
            m_world,
            m_world->m_ground,
            m_world->m_attr,
            m_world->m_player);
    }

    const bool trackMovePerf = IsMovePerfActive(m_world);
    if (trackMovePerf) {
        g_viewMovePerfStats.frames += 1;
    }
    g_viewHiResStats.frames += 1;

    const DWORD groundStart = GetTickCount();
    const double groundStartMs = trackMovePerf ? QpcNowMs() : 0.0;
    const double groundHiResStartMs = QpcNowMs();
    static bool loggedGroundStageStart = false;
    if (!loggedGroundStageStart) {
        loggedGroundStageStart = true;
        DbgLog("[View] ground stage start cells=%zu width=%d height=%d zoom=%.2f\n",
            m_world->m_ground->m_cells.size(),
            m_world->m_ground->m_width,
            m_world->m_ground->m_height,
            m_world->m_ground->m_zoom);
    }
    m_world->m_ground->FlushGround(m_viewMatrix);
    static bool loggedGroundStageDone = false;
    if (!loggedGroundStageDone) {
        loggedGroundStageDone = true;
        DbgLog("[View] ground stage complete\n");
    }
    const DWORD groundEnd = GetTickCount();
    g_viewHiResStats.groundMs += QpcNowMs() - groundHiResStartMs;
    if (trackMovePerf) {
        g_viewMovePerfStats.groundMs += QpcNowMs() - groundStartMs;
    }

    const DWORD hoverStart = groundEnd;
    const double hoverHiResStartMs = QpcNowMs();
    if (m_hoverAttrX >= 0 && m_hoverAttrY >= 0 && IsWalkableAttrCell(m_world->m_attr, m_hoverAttrX, m_hoverAttrY)) {
        m_world->m_ground->RenderAttrTile(m_viewMatrix, m_hoverAttrX, m_hoverAttrY, 0x70FFFFFFu);
    }
    const DWORD hoverEnd = GetTickCount();
    g_viewHiResStats.hoverMs += QpcNowMs() - hoverHiResStartMs;

    const DWORD actorStart = hoverEnd;
    const double actorStartMs = trackMovePerf ? QpcNowMs() : 0.0;
    const double actorHiResStartMs = QpcNowMs();
    static bool loggedActorStageStart = false;
    if (!loggedActorStageStart) {
        loggedActorStageStart = true;
        DbgLog("[View] actor stage start actors=%zu items=%zu gameObjects=%zu\n",
            m_world->m_actorList.size(),
            m_world->m_itemList.size(),
            m_world->m_gameObjectList.size());
    }
    m_world->RenderActors(m_viewMatrix, m_cur.longitude);
    static bool loggedActorStageDone = false;
    if (!loggedActorStageDone) {
        loggedActorStageDone = true;
        DbgLog("[View] actor stage complete renderedActors=%u renderedItems=%u fixedEffects=%u\n",
            static_cast<unsigned int>(m_world->m_lastRenderStats.renderedBillboards),
            static_cast<unsigned int>(m_world->m_lastRenderStats.renderedItems),
            static_cast<unsigned int>(m_world->m_lastRenderStats.renderedFixedEffects));
    }
    const DWORD actorEnd = GetTickCount();
    g_viewHiResStats.actorMs += QpcNowMs() - actorHiResStartMs;
    g_viewHiResStats.renderedGameObjects += m_world->m_lastRenderStats.renderedGameObjects;
    g_viewHiResStats.renderedFixedEffects += m_world->m_lastRenderStats.renderedFixedEffects;
    g_viewHiResStats.renderedItems += m_world->m_lastRenderStats.renderedItems;
    g_viewHiResStats.renderedBillboards += m_world->m_lastRenderStats.renderedBillboards;
    g_viewHiResStats.portalBootstrapActors += m_world->m_lastRenderStats.portalBootstrapActors;
    g_viewHiResStats.actorGameObjectMs += m_world->m_lastRenderStats.gameObjectRenderMs;
    g_viewHiResStats.actorItemMs += m_world->m_lastRenderStats.itemRenderMs;
    g_viewHiResStats.actorPortalMs += m_world->m_lastRenderStats.portalBootstrapMs;
    g_viewHiResStats.actorBillboardBuildMs += m_world->m_lastRenderStats.billboardBuildMs;
    g_viewHiResStats.actorBillboardSortMs += m_world->m_lastRenderStats.billboardSortMs;
    g_viewHiResStats.actorBillboardRenderMs += m_world->m_lastRenderStats.billboardRenderMs;
    if (trackMovePerf) {
        g_viewMovePerfStats.actorMs += QpcNowMs() - actorStartMs;
    }

    const DWORD backgroundStart = actorEnd;
    const double backgroundStartMs = trackMovePerf ? QpcNowMs() : 0.0;
    const double backgroundHiResStartMs = QpcNowMs();
    static bool loggedBackgroundStageStart = false;
    if (!loggedBackgroundStageStart) {
        loggedBackgroundStageStart = true;
        DbgLog("[View] background stage start bgObjects=%zu\n", m_world->m_bgObjList.size());
    }
    m_world->RenderBackgroundObjects(m_viewMatrix);
    static bool loggedBackgroundStageDone = false;
    if (!loggedBackgroundStageDone) {
        loggedBackgroundStageDone = true;
        DbgLog("[View] background stage complete rendered=%u skippedTiny=%u\n",
            static_cast<unsigned int>(m_world->m_lastRenderStats.renderedBackgroundObjects),
            static_cast<unsigned int>(m_world->m_lastRenderStats.skippedTinyBackgroundObjects));
    }
    const DWORD backgroundEnd = GetTickCount();
    g_viewHiResStats.backgroundMs += QpcNowMs() - backgroundHiResStartMs;
    g_viewHiResStats.renderedBackgroundObjects += m_world->m_lastRenderStats.renderedBackgroundObjects;
    g_viewHiResStats.skippedTinyBackgroundObjects += m_world->m_lastRenderStats.skippedTinyBackgroundObjects;
    g_viewHiResStats.backgroundDrawMs += m_world->m_lastRenderStats.backgroundRenderMs;
    if (trackMovePerf) {
        g_viewMovePerfStats.backgroundMs += QpcNowMs() - backgroundStartMs;
    }

    g_viewPerfStats.frames += 1;
    g_viewPerfStats.groundMs += static_cast<u64>(groundEnd - groundStart);
    g_viewPerfStats.hoverMs += static_cast<u64>(hoverEnd - hoverStart);
    g_viewPerfStats.actorMs += static_cast<u64>(actorEnd - actorStart);
    g_viewPerfStats.backgroundMs += static_cast<u64>(backgroundEnd - backgroundStart);

    if (g_viewPerfStats.frames <= 20 || (g_viewPerfStats.frames % kPerfLogIntervalFrames) == 0) {
        DbgLog("[View] frame=%llu ground=%lums hover=%lums actors=%lums background=%lums\n",
            static_cast<unsigned long long>(g_viewPerfStats.frames),
            static_cast<unsigned long>(groundEnd - groundStart),
            static_cast<unsigned long>(hoverEnd - hoverStart),
            static_cast<unsigned long>(actorEnd - actorStart),
            static_cast<unsigned long>(backgroundEnd - backgroundStart));
    }

    if (trackMovePerf) {
        LogViewMovePerfIfNeeded();
    }
    LogViewHiResPerfIfNeeded(m_world);
}