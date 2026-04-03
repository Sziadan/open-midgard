#include "world/3dActor.h"

#include "render/Prim.h"
#include "render/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr float kNearPlane = 80.0f;

float DotVec3(const vector3d& a, const vector3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector3d CrossVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

vector3d SubtractVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x - b.x, a.y - b.y, a.z - b.z };
}

vector3d NormalizeVec3(const vector3d& value)
{
    const float lengthSq = DotVec3(value, value);
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

vector3d TransformPoint(const matrix& m, const vector3d& point)
{
    return vector3d{
        point.x * m.m[0][0] + point.y * m.m[1][0] + point.z * m.m[2][0] + m.m[3][0],
        point.x * m.m[0][1] + point.y * m.m[1][1] + point.z * m.m[2][1] + m.m[3][1],
        point.x * m.m[0][2] + point.y * m.m[1][2] + point.z * m.m[2][2] + m.m[3][2]
    };
}

float Matrix3x3Determinant(const matrix& m)
{
    return
        m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])
        - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])
        + m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0]);
}

float ClampFloat(float value, float minValue, float maxValue)
{
    return (std::max)(minValue, (std::min)(maxValue, value));
}

rotKeyframe MakeAxisAngleQuaternion(float axisX, float axisY, float axisZ, float angle)
{
    const float halfAngle = angle * 0.5f;
    const float sinHalf = std::sin(halfAngle);
    return rotKeyframe{
        0,
        axisX * sinHalf,
        axisY * sinHalf,
        axisZ * sinHalf,
        std::cos(halfAngle)
    };
}

rotKeyframe SlerpQuaternion(const rotKeyframe& a, const rotKeyframe& b, float t)
{
    rotKeyframe from = a;
    rotKeyframe to = b;
    float dot = from.qx * to.qx + from.qy * to.qy + from.qz * to.qz + from.qw * to.qw;
    if (dot < 0.0f) {
        dot = -dot;
        to.qx = -to.qx;
        to.qy = -to.qy;
        to.qz = -to.qz;
        to.qw = -to.qw;
    }

    if (dot > 0.9995f) {
        return rotKeyframe{
            0,
            from.qx + (to.qx - from.qx) * t,
            from.qy + (to.qy - from.qy) * t,
            from.qz + (to.qz - from.qz) * t,
            from.qw + (to.qw - from.qw) * t
        };
    }

    const float theta0 = std::acos(ClampFloat(dot, -1.0f, 1.0f));
    const float sinTheta0 = std::sin(theta0);
    if (std::fabs(sinTheta0) <= 1.0e-6f) {
        return from;
    }

    const float theta = theta0 * t;
    const float sinTheta = std::sin(theta);
    const float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
    const float s1 = sinTheta / sinTheta0;
    return rotKeyframe{
        0,
        from.qx * s0 + to.qx * s1,
        from.qy * s0 + to.qy * s1,
        from.qz * s0 + to.qz * s1,
        from.qw * s0 + to.qw * s1
    };
}

matrix MakeAxisAngleRotation(float axisX, float axisY, float axisZ, float angle)
{
    matrix rotation{};
    MatrixIdentity(rotation);

    const vector3d axis = NormalizeVec3(vector3d{ axisX, axisY, axisZ });
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0f - c;

    rotation.m[0][0] = t * axis.x * axis.x + c;
    rotation.m[0][1] = t * axis.x * axis.y + s * axis.z;
    rotation.m[0][2] = t * axis.x * axis.z - s * axis.y;
    rotation.m[1][0] = t * axis.x * axis.y - s * axis.z;
    rotation.m[1][1] = t * axis.y * axis.y + c;
    rotation.m[1][2] = t * axis.y * axis.z + s * axis.x;
    rotation.m[2][0] = t * axis.x * axis.z + s * axis.y;
    rotation.m[2][1] = t * axis.y * axis.z - s * axis.x;
    rotation.m[2][2] = t * axis.z * axis.z + c;
    return rotation;
}

matrix MakeQuaternionRotation(const rotKeyframe& rotation)
{
    const float lengthSq = rotation.qx * rotation.qx + rotation.qy * rotation.qy + rotation.qz * rotation.qz + rotation.qw * rotation.qw;
    matrix result{};
    MatrixIdentity(result);

    if (lengthSq <= 1.0e-12f) {
        return result;
    }

    const float xx = rotation.qx * rotation.qx;
    const float yy = rotation.qy * rotation.qy;
    const float zz = rotation.qz * rotation.qz;
    const float xy = rotation.qx * rotation.qy;
    const float xz = rotation.qx * rotation.qz;
    const float yz = rotation.qy * rotation.qz;
    const float wx = rotation.qw * rotation.qx;
    const float wy = rotation.qw * rotation.qy;
    const float wz = rotation.qw * rotation.qz;

    result.m[0][0] = 1.0f - 2.0f * (yy + zz);
    result.m[0][1] = 2.0f * (xy + wz);
    result.m[0][2] = 2.0f * (xz - wy);
    result.m[1][0] = 2.0f * (xy - wz);
    result.m[1][1] = 1.0f - 2.0f * (xx + zz);
    result.m[1][2] = 2.0f * (yz + wx);
    result.m[2][0] = 2.0f * (xz + wy);
    result.m[2][1] = 2.0f * (yz - wx);
    result.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return result;
}

matrix MakeNodeTransform(const C3dNodeRes& nodeRes)
{
    matrix scale{};
    matrix rotation = MakeAxisAngleRotation(nodeRes.rotaxis[0], nodeRes.rotaxis[1], nodeRes.rotaxis[2], nodeRes.rotangle);
    matrix translation{};
    matrix tmp{};

    MatrixMakeScale(scale, nodeRes.scale[0], nodeRes.scale[1], nodeRes.scale[2]);
    MatrixIdentity(translation);
    MatrixAppendTranslate(translation, vector3d{ nodeRes.pos[0], nodeRes.pos[1], nodeRes.pos[2] });
    MatrixMult(tmp, scale, rotation);
    MatrixMult(tmp, tmp, translation);
    return tmp;
}

vector3d SamplePosition(const std::vector<posKeyframe>& keys, int frame, const vector3d& fallback)
{
    if (keys.empty()) {
        return fallback;
    }
    if (keys.size() == 1) {
        return vector3d{ keys.front().px, keys.front().py, keys.front().pz };
    }
    if (frame >= keys.back().frame) {
        return vector3d{ keys.back().px, keys.back().py, keys.back().pz };
    }

    for (size_t index = 1; index < keys.size(); ++index) {
        if (frame > keys[index].frame) {
            continue;
        }

        const posKeyframe& prev = keys[index - 1];
        const posKeyframe& next = keys[index];
        const int delta = next.frame - prev.frame;
        const float t = delta > 0 ? static_cast<float>(frame - prev.frame) / static_cast<float>(delta) : 0.0f;
        return vector3d{
            prev.px + (next.px - prev.px) * t,
            prev.py + (next.py - prev.py) * t,
            prev.pz + (next.pz - prev.pz) * t
        };
    }

    return fallback;
}

vector3d SampleScale(const std::vector<scaleKeyframe>& keys, int frame, const vector3d& fallback)
{
    if (keys.empty()) {
        return fallback;
    }
    if (keys.size() == 1) {
        return vector3d{ keys.front().sx, keys.front().sy, keys.front().sz };
    }
    if (frame >= keys.back().frame) {
        return vector3d{ keys.back().sx, keys.back().sy, keys.back().sz };
    }

    for (size_t index = 1; index < keys.size(); ++index) {
        if (frame > keys[index].frame) {
            continue;
        }

        const scaleKeyframe& prev = keys[index - 1];
        const scaleKeyframe& next = keys[index];
        const int delta = next.frame - prev.frame;
        const float t = delta > 0 ? static_cast<float>(frame - prev.frame) / static_cast<float>(delta) : 0.0f;
        return vector3d{
            prev.sx + (next.sx - prev.sx) * t,
            prev.sy + (next.sy - prev.sy) * t,
            prev.sz + (next.sz - prev.sz) * t
        };
    }

    return fallback;
}

rotKeyframe SampleRotation(const std::vector<rotKeyframe>& keys, int frame, const rotKeyframe& fallback)
{
    if (keys.empty()) {
        return fallback;
    }
    if (keys.size() == 1) {
        return keys.front();
    }
    if (frame >= keys.back().frame) {
        return keys.back();
    }

    for (size_t index = 1; index < keys.size(); ++index) {
        if (frame > keys[index].frame) {
            continue;
        }

        const rotKeyframe& prev = keys[index - 1];
        const rotKeyframe& next = keys[index];
        const int delta = next.frame - prev.frame;
        const float t = delta > 0 ? static_cast<float>(frame - prev.frame) / static_cast<float>(delta) : 0.0f;
        return SlerpQuaternion(prev, next, t);
    }

    return fallback;
}

matrix BuildLocalTransform(const vector3d& position, const rotKeyframe& rotation, const vector3d& scale)
{
    matrix scaleMatrix{};
    matrix rotationMatrix = MakeQuaternionRotation(rotation);
    matrix translationMatrix{};
    matrix tmp{};

    MatrixMakeScale(scaleMatrix, scale.x, scale.y, scale.z);
    MatrixIdentity(translationMatrix);
    MatrixAppendTranslate(translationMatrix, position);
    MatrixMult(tmp, scaleMatrix, rotationMatrix);
    MatrixMult(tmp, tmp, translationMatrix);
    return tmp;
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
    if (!std::isfinite(clipZ) || clipZ <= kNearPlane) {
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
    outVertex->z = (1500.0f / (1500.0f - 10.0f)) * ((1.0f / oow) - 10.0f) * oow;
    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, outVertex->z));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return true;
}

bool ShouldRenderFace(const tlvertex3d projected[3], bool flipNormal, bool twoSided)
{
    if (twoSided) {
        return true;
    }

    const float leftSide = (projected[1].y - projected[0].y) * (projected[2].x - projected[0].x);
    const float rightSide = (projected[2].y - projected[0].y) * (projected[1].x - projected[0].x);
    if (leftSide <= rightSide) {
        return flipNormal;
    }

    return !flipNormal;
}

bool TriangleIntersectsViewport(const tlvertex3d projected[3])
{
    float minX = projected[0].x;
    float maxX = projected[0].x;
    float minY = projected[0].y;
    float maxY = projected[0].y;
    for (int index = 1; index < 3; ++index) {
        minX = (std::min)(minX, projected[index].x);
        maxX = (std::max)(maxX, projected[index].x);
        minY = (std::min)(minY, projected[index].y);
        maxY = (std::max)(maxY, projected[index].y);
    }

    return maxX >= 0.0f
        && minX <= static_cast<float>(g_renderer.m_width)
        && maxY >= 0.0f
        && minY <= static_cast<float>(g_renderer.m_height);
}

vector2d ComputeUvScale(const CTexture* texture)
{
    if (!texture) {
        return vector2d{ 1.0f, 1.0f };
    }

    const unsigned int contentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : texture->m_w;
    const unsigned int contentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : texture->m_h;
    if (contentWidth == 0 || contentHeight == 0 || texture->m_w == 0 || texture->m_h == 0) {
        return vector2d{ 1.0f, 1.0f };
    }

    return vector2d{
        static_cast<float>(contentWidth) / static_cast<float>(texture->m_w),
        static_cast<float>(contentHeight) / static_cast<float>(texture->m_h)
    };
}

u32 MultiplyColor(u32 color, const vector3d& light, float alpha)
{
    const float clampedAlpha = (std::max)(0.0f, (std::min)(1.0f, alpha));
    const unsigned int srcR = (color >> 16) & 0xFFu;
    const unsigned int srcG = (color >> 8) & 0xFFu;
    const unsigned int srcB = color & 0xFFu;
    const unsigned int dstA = static_cast<unsigned int>(clampedAlpha * 255.0f + 0.5f);
    const unsigned int dstR = static_cast<unsigned int>((std::min)(255.0f, srcR * light.x));
    const unsigned int dstG = static_cast<unsigned int>((std::min)(255.0f, srcG * light.y));
    const unsigned int dstB = static_cast<unsigned int>((std::min)(255.0f, srcB * light.z));
    return (dstA << 24) | (dstR << 16) | (dstG << 8) | dstB;
}

u32 MakeLitColor(const vector3d& light, float alpha, bool dimmed)
{
    const float scale = dimmed ? 128.0f : 255.0f;
    const unsigned int dstA = static_cast<unsigned int>(ClampFloat(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
    const unsigned int dstR = static_cast<unsigned int>(ClampFloat(light.x, 0.0f, 1.0f) * scale + 0.5f);
    const unsigned int dstG = static_cast<unsigned int>(ClampFloat(light.y, 0.0f, 1.0f) * scale + 0.5f);
    const unsigned int dstB = static_cast<unsigned int>(ClampFloat(light.z, 0.0f, 1.0f) * scale + 0.5f);
    return (dstA << 24) | (dstR << 16) | (dstG << 8) | dstB;
}

vector3d ComputeLightColor(const vector3d& normal, const vector3d& lightDir, const vector3d& diffuseCol, const vector3d& ambientCol)
{
    (void)normal;
    (void)lightDir;
    return vector3d{
        ClampFloat(ambientCol.x + diffuseCol.x, 0.0f, 1.0f),
        ClampFloat(ambientCol.y + diffuseCol.y, 0.0f, 1.0f),
        ClampFloat(ambientCol.z + diffuseCol.z, 0.0f, 1.0f)
    };
}

void ExpandBounds(vector3d* minBounds, vector3d* maxBounds, const vector3d& point)
{
    if (!minBounds || !maxBounds) {
        return;
    }

    minBounds->x = (std::min)(minBounds->x, point.x);
    minBounds->y = (std::min)(minBounds->y, point.y);
    minBounds->z = (std::min)(minBounds->z, point.z);
    maxBounds->x = (std::max)(maxBounds->x, point.x);
    maxBounds->y = (std::max)(maxBounds->y, point.y);
    maxBounds->z = (std::max)(maxBounds->z, point.z);
}

} // namespace

C3dNode::C3dNode()
    : m_mesh(nullptr),
    m_basePos{ 0.0f, 0.0f, 0.0f },
    m_baseScale{ 1.0f, 1.0f, 1.0f },
    m_baseRot{ 0, 0.0f, 0.0f, 0.0f, 1.0f },
    m_shadeType(0),
      m_opacity(1.0f)
{
    MatrixIdentity(m_ltm);
    MatrixIdentity(m_wtm);
}

C3dNode::~C3dNode()
{
    for (C3dNode* child : m_children) {
        delete child;
    }
}

bool C3dNode::AssignModel(const C3dNodeRes& nodeRes, const C3dModelRes& modelRes)
{
    m_name = nodeRes.name;
    m_mesh = nodeRes.mesh;
    m_opacity = static_cast<float>(nodeRes.alpha) / 255.0f;
    m_basePos = vector3d{ nodeRes.pos[0], nodeRes.pos[1], nodeRes.pos[2] };
    m_baseScale = vector3d{ nodeRes.scale[0], nodeRes.scale[1], nodeRes.scale[2] };
    m_baseRot = MakeAxisAngleQuaternion(nodeRes.rotaxis[0], nodeRes.rotaxis[1], nodeRes.rotaxis[2], nodeRes.rotangle);
    m_shadeType = modelRes.m_shadeType;
    m_posAnim = nodeRes.posanim.m_animdata;
    m_rotAnim = nodeRes.rotanim.m_animdata;
    m_scaleAnim = nodeRes.scaleanim.m_animdata;
    if (m_posAnim.empty()) {
        m_posAnim.push_back(posKeyframe{ 0, m_basePos.x, m_basePos.y, m_basePos.z });
    }
    if (m_rotAnim.empty()) {
        m_rotAnim.push_back(m_baseRot);
    }
    if (m_scaleAnim.empty()) {
        m_scaleAnim.push_back(scaleKeyframe{ 0, m_baseScale.x, m_baseScale.y, m_baseScale.z, 1.0f, 0.0f, 0.0f, 0.0f });
    }
    m_ltm = BuildLocalTransform(m_basePos, m_baseRot, m_baseScale);
    m_textures.clear();
    m_textures.reserve(nodeRes.textureIndices.size());
    for (int materialSlot : nodeRes.textureIndices) {
        CTexture* texture = nullptr;
        if (materialSlot >= 0 && materialSlot < static_cast<int>(modelRes.m_materialNames.size())) {
            texture = g_texMgr.GetTexture(modelRes.m_materialNames[static_cast<size_t>(materialSlot)].c_str(), false);
        }
        m_textures.push_back(texture);
    }

    m_colorInfo.clear();
    if (m_mesh && m_mesh->m_numFace > 0) {
        m_colorInfo.resize(static_cast<size_t>(m_mesh->m_numFace));
        for (FaceColor& faceColor : m_colorInfo) {
            faceColor.color = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        }
    }

    for (C3dNode* child : m_children) {
        delete child;
    }
    m_children.clear();
    m_children.reserve(nodeRes.child.size());
    for (const C3dNodeRes* childRes : nodeRes.child) {
        if (!childRes) {
            continue;
        }

        C3dNode* child = new C3dNode();
        if (!child) {
            return false;
        }
        if (!child->AssignModel(*childRes, modelRes)) {
            delete child;
            return false;
        }
        m_children.push_back(child);
    }

    return true;
}

void C3dNode::SetFrame(int frame)
{
    const vector3d position = SamplePosition(m_posAnim, frame, m_basePos);
    const rotKeyframe rotation = SampleRotation(m_rotAnim, frame, m_baseRot);
    const vector3d scale = SampleScale(m_scaleAnim, frame, m_baseScale);
    m_ltm = BuildLocalTransform(position, rotation, scale);

    for (C3dNode* child : m_children) {
        child->SetFrame(frame);
    }
}

void C3dNode::UpdateWorldMatrix(const matrix& parentWorld)
{
    MatrixMult(m_wtm, m_ltm, parentWorld);
    for (C3dNode* child : m_children) {
        child->UpdateWorldMatrix(m_wtm);
    }
}

void C3dNode::UpdateBounds(vector3d* minBounds, vector3d* maxBounds, const matrix& parentWorld) const
{
    matrix nodeWorld{};
    MatrixMult(nodeWorld, m_ltm, parentWorld);

    if (m_mesh) {
        for (const vector3d& vertex : m_mesh->m_vert) {
            ExpandBounds(minBounds, maxBounds, TransformPoint(nodeWorld, vertex));
        }
    }

    for (const C3dNode* child : m_children) {
        child->UpdateBounds(minBounds, maxBounds, nodeWorld);
    }
}

void C3dNode::UpdateVertexColor(const matrix& parentWorld,
    const vector3d& lightDir,
    const vector3d& diffuseCol,
    const vector3d& ambientCol)
{
    matrix nodeWorld{};
    MatrixMult(nodeWorld, m_ltm, parentWorld);

    if (m_mesh) {
        for (size_t faceIndex = 0; faceIndex < m_mesh->m_face.size() && faceIndex < m_colorInfo.size(); ++faceIndex) {
            const face3d& face = m_mesh->m_face[faceIndex];
            if (face.vertindex[0] >= m_mesh->m_vert.size()
                || face.vertindex[1] >= m_mesh->m_vert.size()
                || face.vertindex[2] >= m_mesh->m_vert.size()) {
                continue;
            }

            FaceColor& colorInfo = m_colorInfo[faceIndex];
            for (int corner = 0; corner < 3; ++corner) {
                const size_t tvertIndex = face.tvertindex[corner];
                const bool dimmed = tvertIndex < m_mesh->m_tvert.size() && m_mesh->m_tvert[tvertIndex].color != 0;
                if (m_shadeType == 0) {
                    colorInfo.color[corner] = MakeLitColor(vector3d{ 1.0f, 1.0f, 1.0f }, m_opacity, false);
                    continue;
                }

                vector3d normal{};
                const size_t normalIndex = faceIndex * 3u + static_cast<size_t>(corner);
                if (m_shadeType == 2 && normalIndex < m_mesh->m_vertNormal.size()) {
                    const vector3d& vertexNormal = m_mesh->m_vertNormal[normalIndex];
                    normal = NormalizeVec3(vector3d{
                        vertexNormal.x * nodeWorld.m[0][0] + vertexNormal.y * nodeWorld.m[1][0] + vertexNormal.z * nodeWorld.m[2][0],
                        vertexNormal.x * nodeWorld.m[0][1] + vertexNormal.y * nodeWorld.m[1][1] + vertexNormal.z * nodeWorld.m[2][1],
                        vertexNormal.x * nodeWorld.m[0][2] + vertexNormal.y * nodeWorld.m[1][2] + vertexNormal.z * nodeWorld.m[2][2]
                    });
                } else {
                    const vector3d worldA = TransformPoint(nodeWorld, m_mesh->m_vert[face.vertindex[0]]);
                    const vector3d worldB = TransformPoint(nodeWorld, m_mesh->m_vert[face.vertindex[1]]);
                    const vector3d worldC = TransformPoint(nodeWorld, m_mesh->m_vert[face.vertindex[2]]);
                    normal = NormalizeVec3(CrossVec3(SubtractVec3(worldB, worldA), SubtractVec3(worldC, worldA)));
                }

                colorInfo.color[corner] = MakeLitColor(
                    ComputeLightColor(normal, lightDir, diffuseCol, ambientCol),
                    m_opacity,
                    dimmed);
            }
        }
    }

    for (C3dNode* child : m_children) {
        child->UpdateVertexColor(nodeWorld, lightDir, diffuseCol, ambientCol);
    }
}

void C3dNode::Render(const matrix& parentWorld, const matrix& viewMatrix, bool forceDoubleSided) const
{
    matrix nodeWorld{};
    MatrixMult(nodeWorld, m_ltm, parentWorld);
    const bool flipNormal = Matrix3x3Determinant(nodeWorld) < 0.0f;

    if (m_mesh) {
        std::vector<tlvertex3d> projectedVerts(m_mesh->m_vert.size());
        std::vector<unsigned char> validProjectedVerts(m_mesh->m_vert.size(), 0);
        for (size_t vertexIndex = 0; vertexIndex < m_mesh->m_vert.size(); ++vertexIndex) {
            const vector3d worldVertex = TransformPoint(nodeWorld, m_mesh->m_vert[vertexIndex]);
            validProjectedVerts[vertexIndex] = ProjectPoint(g_renderer, viewMatrix, worldVertex, &projectedVerts[vertexIndex]) ? 1u : 0u;
        }

        std::vector<vector2d> uvScaleByTextureSlot(m_textures.size(), vector2d{ 1.0f, 1.0f });
        for (size_t textureIndex = 0; textureIndex < m_textures.size(); ++textureIndex) {
            if (m_textures[textureIndex]) {
                uvScaleByTextureSlot[textureIndex] = ComputeUvScale(m_textures[textureIndex]);
            }
        }

        for (size_t faceIndex = 0; faceIndex < m_mesh->m_face.size(); ++faceIndex) {
            const face3d& face = m_mesh->m_face[faceIndex];
            if (face.vertindex[0] >= m_mesh->m_vert.size()
                || face.vertindex[1] >= m_mesh->m_vert.size()
                || face.vertindex[2] >= m_mesh->m_vert.size()) {
                continue;
            }

            if (!validProjectedVerts[face.vertindex[0]]
                || !validProjectedVerts[face.vertindex[1]]
                || !validProjectedVerts[face.vertindex[2]]) {
                continue;
            }

            const CTexture* texture = nullptr;
            vector2d uvScale{ 1.0f, 1.0f };
            if (face.meshMtlId >= 0 && !m_textures.empty()) {
                const size_t textureIndex = static_cast<size_t>(face.meshMtlId) % m_textures.size();
                texture = m_textures[textureIndex];
                uvScale = uvScaleByTextureSlot[textureIndex];
            }

            tlvertex3d projected[3] = {
                projectedVerts[face.vertindex[0]],
                projectedVerts[face.vertindex[1]],
                projectedVerts[face.vertindex[2]],
            };
            for (int corner = 0; corner < 3; ++corner) {
                const size_t tvertIndex = face.tvertindex[corner];
                if (tvertIndex < m_mesh->m_tvert.size()) {
                    projected[corner].tu = m_mesh->m_tvert[tvertIndex].u * uvScale.x;
                    projected[corner].tv = m_mesh->m_tvert[tvertIndex].v * uvScale.y;
                } else {
                    projected[corner].tu = 0.0f;
                    projected[corner].tv = 0.0f;
                }

                projected[corner].color = faceIndex < m_colorInfo.size() ? m_colorInfo[faceIndex].color[corner] : 0xFFFFFFFFu;
            }

            if (!ShouldRenderFace(projected, flipNormal, forceDoubleSided || face.twoSide)) {
                continue;
            }

            if (!TriangleIntersectsViewport(projected)) {
                continue;
            }

            RPFace* renderFace = g_renderer.BorrowNullRP();
            if (!renderFace) {
                continue;
            }

            renderFace->m_verts[0] = projected[0];
            renderFace->m_verts[1] = projected[1];
            renderFace->m_verts[2] = projected[2];

            renderFace->primType = D3DPT_TRIANGLELIST;
            renderFace->verts = renderFace->m_verts;
            renderFace->numVerts = 3;
            renderFace->indices = nullptr;
            renderFace->numIndices = 0;
            renderFace->tex = const_cast<CTexture*>(texture);
            renderFace->mtPreset = 0;
            renderFace->cullMode = D3DCULL_NONE;
            renderFace->srcAlphaMode = D3DBLEND_SRCALPHA;
            renderFace->destAlphaMode = D3DBLEND_INVSRCALPHA;
            renderFace->alphaSortKey = 0.0f;

            const bool isAlpha = ((projected[0].color >> 24) & 0xFFu) < 0xFFu
                || ((projected[1].color >> 24) & 0xFFu) < 0xFFu
                || ((projected[2].color >> 24) & 0xFFu) < 0xFFu;
            if (isAlpha) {
                renderFace->alphaSortKey = (std::max)(projected[0].oow, (std::max)(projected[1].oow, projected[2].oow));
            }
            g_renderer.AddRP(renderFace, isAlpha ? 1 : 0);
        }
    }

    for (const C3dNode* child : m_children) {
        child->Render(nodeWorld, viewMatrix, forceDoubleSided);
    }
}

C3dActor::C3dActor()
    : m_node(nullptr),
      m_pos{ 0.0f, 0.0f, 0.0f },
      m_rot{ 0.0f, 0.0f, 0.0f },
      m_scale{ 1.0f, 1.0f, 1.0f },
      m_posOffset{ 0.0f, 0.0f, 0.0f },
      m_animSpeed(0.0f),
    m_animType(0),
    m_animLen(0),
    m_curMotion(0),
      m_blockType(0),
      m_isHideCheck(0),
            m_isMatrixNeedUpdate(1),
        m_boundRadius(0.0f),
        m_forceDoubleSided(false)
{
    m_name[0] = '\0';
    MatrixIdentity(m_wtm);
}

C3dActor::~C3dActor()
{
    delete m_node;
}

bool C3dActor::AssignModel(const C3dModelRes& modelRes)
{
    const C3dNodeRes* rootRes = nullptr;
    if (!modelRes.m_objectList.empty()) {
        rootRes = modelRes.m_objectList.front();
    }
    if (!rootRes && !modelRes.m_rootObjList.empty()) {
        rootRes = modelRes.FindNode(modelRes.m_rootObjList.front().c_str());
    }

    if (!rootRes) {
        return false;
    }

    delete m_node;
    m_node = new C3dNode();
    if (!m_node) {
        return false;
    }
    if (!m_node->AssignModel(*rootRes, modelRes)) {
        delete m_node;
        m_node = nullptr;
        return false;
    }

    m_animLen = modelRes.m_animLen;
    SetFrame(0);
    UpdateBound();
    UpdateMatrix();
    return true;
}

void C3dActor::SetFrame(int frame)
{
    if (!m_node) {
        return;
    }

    m_node->SetFrame(frame);
    m_curMotion = frame;
    m_isMatrixNeedUpdate = 1;
}

void C3dActor::AdvanceFrame()
{
    if (!m_node || m_animType == 0) {
        return;
    }

    const int frame = m_curMotion;
    SetFrame(frame);

    int nextFrame = frame + static_cast<int>(m_animSpeed * 100.0f);
    if (nextFrame == frame) {
        nextFrame = frame + 1;
    }

    if (m_animType == 1) {
        if (m_animLen > 0 && nextFrame >= m_animLen) {
            nextFrame = frame;
        }
    } else if (m_animType == 2) {
        const int animLen = m_animLen > 0 ? m_animLen : 1;
        nextFrame %= animLen;
    }

    m_curMotion = nextFrame;
}

void C3dActor::UpdateMatrix()
{
    matrix world{};
    MatrixIdentity(world);
    MatrixAppendTranslate(world, m_posOffset);
    MatrixAppendScale(world, m_scale.x, m_scale.y, m_scale.z);
    MatrixAppendYRotation(world, 3.14159265f * m_rot.y / 180.0f);
    MatrixAppendXRotation(world, 3.14159265f * m_rot.x / 180.0f);
    MatrixAppendZRotation(world, 3.14159265f * m_rot.z / 180.0f);
    MatrixAppendTranslate(world, m_pos);
    m_wtm = world;
    m_isMatrixNeedUpdate = 0;
}

void C3dActor::UpdateBound()
{
    if (!m_node) {
        m_posOffset = vector3d{ 0.0f, 0.0f, 0.0f };
        m_boundRadius = 0.0f;
        return;
    }

    matrix identity{};
    MatrixIdentity(identity);
    vector3d minBounds{ 100000.0f, 100000.0f, 100000.0f };
    vector3d maxBounds{ -100000.0f, -100000.0f, -100000.0f };
    m_node->UpdateBounds(&minBounds, &maxBounds, identity);
    m_posOffset = vector3d{
        (maxBounds.x + minBounds.x) * -0.5f,
        -maxBounds.y,
        (maxBounds.z + minBounds.z) * -0.5f
    };
    const vector3d halfExtents{
        (maxBounds.x - minBounds.x) * 0.5f,
        (maxBounds.y - minBounds.y) * 0.5f,
        (maxBounds.z - minBounds.z) * 0.5f
    };
    m_boundRadius = std::sqrt(
        halfExtents.x * halfExtents.x
        + halfExtents.y * halfExtents.y
        + halfExtents.z * halfExtents.z);
    m_isMatrixNeedUpdate = 1;
}

void C3dActor::UpdateVertexColor(const vector3d& lightDir,
    const vector3d& diffuseCol,
    const vector3d& ambientCol)
{
    if (!m_node) {
        return;
    }

    if (m_isMatrixNeedUpdate) {
        UpdateMatrix();
    }
    m_node->UpdateVertexColor(m_wtm, lightDir, diffuseCol, ambientCol);
}

void C3dActor::Render(const matrix& viewMatrix) const
{
    if (!m_node) {
        return;
    }
    m_node->Render(m_wtm, viewMatrix, m_forceDoubleSided);
}