#include "RagEffect.h"

#include "World.h"
#include "DebugLog.h"
#include "audio/Audio.h"
#include "core/File.h"
#include "core/Timer.h"
#include "gamemode/GameMode.h"
#include "gamemode/View.h"
#include "gamemode/Mode.h"
#include "render/DC.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include "res/Texture.h"
#include "session/Session.h"
#include "world/RagEffectDurationTable.generated.inc"

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <vector>

namespace {

constexpr float kNearPlane = 10.0f;
constexpr float kSubmitNearPlane = 80.0f;
constexpr float kPi = 3.14159265f;
constexpr float kEffectTickMs = 24.0f;
constexpr float kEffectPixelRatioScale = 0.14285715f;
constexpr float kEffectSpriteDepthBias = 0.0002f;
constexpr float kWaterfallSegmentHeight = 40.0f;
constexpr int kWaterfallVisibleSegments = 5;
constexpr int kWaterfallTextureCycle = 3;

struct RagEffectCatalogEntry {
    int effectId;
    const char* strName;
    const char* minStrName;
    const char* variantPrefix;
    int variantFirst;
    int variantCount;
};

#include "RagEffectCatalog.generated.inc"

vector3d AddVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x + b.x, a.y + b.y, a.z + b.z };
}

bool IsLikelyLiveRenderObject(const CRenderObject* object)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(object);
    return value >= 0x10000ull && value <= 0x00007FFFFFFFFFFFull;
}

vector3d ScaleVec3(const vector3d& v, float scale)
{
    return vector3d{ v.x * scale, v.y * scale, v.z * scale };
}

vector3d NormalizeVec3(const vector3d& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

vector3d CrossVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

bool ContainsIgnoreCaseAscii(const std::string& text, const char* needle)
{
    if (!needle || !*needle || text.empty()) {
        return false;
    }
    std::string loweredText = text;
    std::string loweredNeedle = needle;
    std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(loweredNeedle.begin(), loweredNeedle.end(), loweredNeedle.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return loweredText.find(loweredNeedle) != std::string::npos;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string CollapseUtf8Latin1ToBytes(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    bool changed = false;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (ch == 0xC2 && i + 1 < value.size()) {
            out.push_back(value[i + 1]);
            ++i;
            changed = true;
            continue;
        }
        if (ch == 0xC3 && i + 1 < value.size()) {
            out.push_back(static_cast<char>(static_cast<unsigned char>(value[i + 1]) + 0x40u));
            ++i;
            changed = true;
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    return changed ? out : value;
}

const RagEffectCatalogEntry* FindRagEffectCatalogEntry(int effectId)
{
    const auto begin = std::begin(kRagEffectCatalog);
    const auto end = std::end(kRagEffectCatalog);
    const auto it = std::lower_bound(begin, end, effectId, [](const RagEffectCatalogEntry& entry, int value) {
        return entry.effectId < value;
    });
    if (it == end || it->effectId != effectId) {
        return nullptr;
    }
    return it;
}

std::string ResolveCatalogStrName(const RagEffectCatalogEntry& entry)
{
    const char* directName = g_session.m_isMinEffect && entry.minStrName ? entry.minStrName : entry.strName;
    if (directName && *directName) {
        return directName;
    }

    if (!entry.variantPrefix || entry.variantCount <= 0) {
        return std::string();
    }

    const int offset = rand() % entry.variantCount;
    char buffer[64]{};
    std::snprintf(buffer,
        sizeof(buffer),
        "%s%d.str",
        entry.variantPrefix,
        entry.variantFirst + offset);
    return buffer;
}

int ResolveEffectRenderFlags(u32 renderFlag, int fallbackFlags)
{
    return renderFlag != 0 ? static_cast<int>(renderFlag) : fallbackFlags;
}

D3DBLEND ResolveEffectDestBlend(u32 renderFlag, D3DBLEND additiveBlend = D3DBLEND_ONE)
{
    return (renderFlag & (2u | 4u)) != 0 ? additiveBlend : D3DBLEND_INVSRCALPHA;
}

unsigned int PackColor(unsigned int alpha, COLORREF color)
{
    return (alpha << 24)
        | (static_cast<unsigned int>(GetRValue(color)) << 16)
        | (static_cast<unsigned int>(GetGValue(color)) << 8)
        | static_cast<unsigned int>(GetBValue(color));
}

float WrapUnit(float value)
{
    value = std::fmod(value, 1.0f);
    if (value < 0.0f) {
        value += 1.0f;
    }
    return value;
}

bool ProjectPoint(const vector3d& point, const matrix& viewMatrix, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= kSubmitNearPlane) {
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

    outVertex->x = g_renderer.m_xoffset + projectedX * g_renderer.m_hpc * oow;
    outVertex->y = g_renderer.m_yoffset + projectedY * g_renderer.m_vpc * oow;
    const float depth = (1500.0f / (1500.0f - kNearPlane)) * ((1.0f / oow) - kNearPlane) * oow;
    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, depth));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return std::isfinite(outVertex->x) && std::isfinite(outVertex->y) && std::isfinite(outVertex->z);
}

float ResolveEffectPixelRatio(const tlvertex3d& anchor)
{
    return anchor.oow * g_renderer.m_hpc * kEffectPixelRatioScale;
}

float WrapAngle360(float angle)
{
    while (angle >= 360.0f) {
        angle -= 360.0f;
    }
    while (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

float SinDeg(float angleDegrees)
{
    return std::sin(angleDegrees * (kPi / 180.0f));
}

const char* RefRuwachSpriteStem()
{
    return "\xC0\xCC\xC6\xD1\xC6\xAE";
}


const char* RefParticle2SpriteStem()
{
    return "particle2";
}

const char* RefParticle3SpriteStem()
{
    return "particle3";
}


bool ConfigureEffectSpritePrim(CEffectPrim* prim,
    std::initializer_list<const char*> stems,
    int actionIndex,
    float scale,
    bool repeat,
    float frameDelay,
    int motionBase);

CTexture* TryResolveEffectTextureCandidates(std::initializer_list<const char*> paths);
CTexture* ResolveEffectTextureCandidates(std::initializer_list<const char*> paths, bool cloudFallback);

void SubmitScreenQuad(const tlvertex3d& anchor,
    CTexture* texture,
    float offsetX,
    float offsetY,
    float width,
    float height,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags);

const std::string& ResolveDataPathByBasename(const char* basename);
CTexture* GetSoftGlowTexture(bool cloudVariant = false);
float ResolveGroundHeight(const vector3d& position);

void SubmitScreenQuadPivot(const tlvertex3d& anchor,
    CTexture* texture,
    float pivotX,
    float pivotY,
    float width,
    float height,
    float angleDegrees,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags,
    float depthBias = 0.0f)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    if (!std::isfinite(angleDegrees) || std::fabs(angleDegrees) <= 0.001f) {
        SubmitScreenQuad(anchor,
            texture,
            width * 0.5f - pivotX,
            height * 0.5f - pivotY,
            width,
            height,
            color,
            destBlend,
            alphaSortKey,
            renderFlags);
        return;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = destBlend;
    face->alphaSortKey = alphaSortKey;

    const float radians = angleDegrees * (kPi / 180.0f);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float xs[4] = { -pivotX, width - pivotX, -pivotX, width - pivotX };
    const float ys[4] = { -pivotY, -pivotY, height - pivotY, height - pivotY };
    const float uvs[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };

    for (int index = 0; index < 4; ++index) {
        tlvertex3d& vert = face->m_verts[index];
        vert = anchor;
        if (depthBias > 0.0f) {
            vert.z = (std::max)(0.0f, anchor.z - depthBias);
        }
        vert.x = anchor.x + xs[index] * c - ys[index] * s;
        vert.y = anchor.y + xs[index] * s + ys[index] * c;
        vert.color = color;
        vert.specular = 0xFF000000u;
        vert.tu = uvs[index][0];
        vert.tv = uvs[index][1];
    }

    g_renderer.AddRP(face, renderFlags);
}

void SubmitScreenQuad(const tlvertex3d& anchor,
    CTexture* texture,
    float offsetX,
    float offsetY,
    float width,
    float height,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = destBlend;
    face->alphaSortKey = alphaSortKey;

    const float left = anchor.x + offsetX - width * 0.5f;
    const float top = anchor.y + offsetY - height * 0.5f;
    const float right = left + width;
    const float bottom = top + height;

    tlvertex3d& v0 = face->m_verts[0];
    tlvertex3d& v1 = face->m_verts[1];
    tlvertex3d& v2 = face->m_verts[2];
    tlvertex3d& v3 = face->m_verts[3];

    v0 = anchor;
    v1 = anchor;
    v2 = anchor;
    v3 = anchor;
    v0.x = left;  v0.y = top;    v0.tu = 0.0f; v0.tv = 0.0f;
    v1.x = right; v1.y = top;    v1.tu = 1.0f; v1.tv = 0.0f;
    v2.x = left;  v2.y = bottom; v2.tu = 0.0f; v2.tv = 1.0f;
    v3.x = right; v3.y = bottom; v3.tu = 1.0f; v3.tv = 1.0f;
    v0.color = color;
    v1.color = color;
    v2.color = color;
    v3.color = color;
    v0.specular = 0xFF000000u;
    v1.specular = 0xFF000000u;
    v2.specular = 0xFF000000u;
    v3.specular = 0xFF000000u;

    g_renderer.AddRP(face, renderFlags);
}

void SubmitBillboard(const vector3d& center,
    const matrix& viewMatrix,
    CTexture* texture,
    float width,
    float height,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
    tlvertex3d anchor{};
    if (!ProjectPoint(center, viewMatrix, &anchor)) {
        return;
    }
    const float pixelRatio = ResolveEffectPixelRatio(anchor);
    if (!std::isfinite(pixelRatio) || pixelRatio <= 0.0f) {
        return;
    }
    SubmitScreenQuad(anchor, texture, 0.0f, 0.0f, width * pixelRatio, height * pixelRatio, color, destBlend, alphaSortKey, renderFlags);
}

void SubmitGroundQuad(const vector3d (&quad)[4],
    const matrix& viewMatrix,
    CTexture* texture,
    unsigned int color0,
    unsigned int color1,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = destBlend;
    face->alphaSortKey = alphaSortKey;

    for (int index = 0; index < 4; ++index) {
        if (!ProjectPoint(quad[index], viewMatrix, &face->m_verts[index])) {
            return;
        }
        face->m_verts[index].color = index >= 2 ? color1 : color0;
        face->m_verts[index].specular = 0xFF000000u;
        face->m_verts[index].tu = (index == 0 || index == 2) ? 0.0f : 1.0f;
        face->m_verts[index].tv = index <= 1 ? 0.0f : 1.0f;
    }

    g_renderer.AddRP(face, renderFlags);
}

void SubmitWorldQuad(const vector3d (&quad)[4],
    const matrix& viewMatrix,
    CTexture* texture,
    const unsigned int (&colors)[4],
    const float (&uvs)[4][2],
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = destBlend;
    face->alphaSortKey = alphaSortKey;

    for (int index = 0; index < 4; ++index) {
        if (!ProjectPoint(quad[index], viewMatrix, &face->m_verts[index])) {
            return;
        }
        face->m_verts[index].color = colors[index];
        face->m_verts[index].specular = 0xFF000000u;
        face->m_verts[index].tu = uvs[index][0];
        face->m_verts[index].tv = uvs[index][1];
    }

    g_renderer.AddRP(face, renderFlags);
}

void SubmitWorldTriangle(const vector3d (&triangle)[3],
    const matrix& viewMatrix,
    CTexture* texture,
    unsigned int color,
    const float (&uvs)[3][2],
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLELIST;
    face->verts = face->m_verts;
    face->numVerts = 3;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = destBlend;
    face->alphaSortKey = alphaSortKey;

    for (int index = 0; index < 3; ++index) {
        if (!ProjectPoint(triangle[index], viewMatrix, &face->m_verts[index])) {
            return;
        }
        face->m_verts[index].color = color;
        face->m_verts[index].specular = 0xFF000000u;
        face->m_verts[index].tu = uvs[index][0];
        face->m_verts[index].tv = uvs[index][1];
    }

    g_renderer.AddRP(face, renderFlags);
}

void RenderQuadHornPrimitive(const CEffectPrim& prim,
    const vector3d& base,
    const matrix& viewMatrix,
    float normalizedAlpha)
{
    CTexture* texture = !prim.m_texture.empty()
        ? prim.m_texture[(std::min)(prim.m_curMotion, static_cast<int>(prim.m_texture.size()) - 1)]
        : nullptr;
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    matrix rotation = prim.m_matrix;
    if (rotation.m[0][0] == 1.0f
        && rotation.m[1][1] == 1.0f
        && rotation.m[2][2] == 1.0f
        && rotation.m[0][1] == 0.0f
        && rotation.m[0][2] == 0.0f
        && rotation.m[1][0] == 0.0f
        && rotation.m[1][2] == 0.0f
        && rotation.m[2][0] == 0.0f
        && rotation.m[2][1] == 0.0f) {
        MatrixIdentity(rotation);
        MatrixAppendXRotation(rotation, prim.m_latitude * (kPi / 180.0f));
        MatrixAppendYRotation(rotation, prim.m_longitude * (kPi / 180.0f));
    }

    const vector3d origin = {
        base.x + prim.m_deltaPos2.x,
        ResolveGroundHeight(vector3d{ base.x + prim.m_deltaPos2.x, base.y, base.z + prim.m_deltaPos2.z }),
        base.z + prim.m_deltaPos2.z,
    };
    const float halfSize = (std::max)(0.9f, prim.m_size * 1.7f);
    const float spikeHeight = (std::max)(12.0f, (std::max)(prim.m_heightSize * 1.35f, halfSize * 5.0f));
    const int renderFlags = 1;
    const D3DBLEND destBlend = D3DBLEND_INVSRCALPHA;
    const unsigned int color = PackColor(static_cast<unsigned int>(normalizedAlpha), prim.m_tintColor);

    const vector3d localCorners[4] = {
        { -halfSize, -halfSize, 0.0f },
        { halfSize, -halfSize, 0.0f },
        { halfSize, halfSize, 0.0f },
        { -halfSize, halfSize, 0.0f },
    };
    const vector3d localTip = { 0.0f, 0.0f, spikeHeight };

    auto transformPoint = [&](const vector3d& local) -> vector3d {
        return {
            origin.x + local.x * rotation.m[0][0] + local.y * rotation.m[1][0] + local.z * rotation.m[2][0],
            origin.y + local.x * rotation.m[0][1] + local.y * rotation.m[1][1] + local.z * rotation.m[2][1],
            origin.z + local.x * rotation.m[0][2] + local.y * rotation.m[1][2] + local.z * rotation.m[2][2],
        };
    };

    for (int faceIndex = 0; faceIndex < 4; ++faceIndex) {
        const int nextIndex = (faceIndex + 1) % 4;
        const float u0 = static_cast<float>(faceIndex) * 0.2f;
        const float u1 = (std::min)(1.0f, u0 + 0.2f);
        const vector3d triangle[3] = {
            transformPoint(localCorners[faceIndex]),
            transformPoint(localTip),
            transformPoint(localCorners[nextIndex]),
        };
        const float uvs[3][2] = {
            { u0, 1.0f },
            { u0, 0.0f },
            { u1, 1.0f },
        };
        SubmitWorldTriangle(triangle, viewMatrix, texture, color, uvs, destBlend, 0.0f, renderFlags);
    }
}

// vec1=base0 (angle0, ground), vec2=base1 (angle1, ground), vec3=top1, vec4=top0.
// U spans circumference (u0..u1); V spans ribbon inner..outer (vBottom..vTop, e.g. 1..0).
void SubmitWorldTeiRect(const vector3d& vec1,
    const vector3d& vec2,
    const vector3d& vec3,
    const vector3d& vec4,
    const matrix& viewMatrix,
    CTexture* texture,
    unsigned int color,
    float u0,
    float u1,
    float vBottom,
    float vTop,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    const struct TriangleDef {
        const vector3d* positions[3];
        float uvs[3][2];
    } triangles[2] = {
        { { &vec1, &vec2, &vec3 }, { { u0, vBottom }, { u1, vBottom }, { u1, vTop } } },
        { { &vec4, &vec3, &vec1 }, { { u0, vTop }, { u1, vTop }, { u0, vBottom } } },
    };

    for (const TriangleDef& triangle : triangles) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return;
        }

        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = destBlend;
        face->alphaSortKey = alphaSortKey;

        for (int index = 0; index < 3; ++index) {
            if (!ProjectPoint(*triangle.positions[index], viewMatrix, &face->m_verts[index])) {
                return;
            }
            face->m_verts[index].color = color;
            face->m_verts[index].specular = 0xFF000000u;
            face->m_verts[index].tu = triangle.uvs[index][0];
            face->m_verts[index].tv = triangle.uvs[index][1];
        }

        g_renderer.AddRP(face, renderFlags);
    }
}

void SubmitTeiHit1Rect(const vector3d corners[4], const vector3d& translation,
    const matrix& viewMatrix, CTexture* texture, unsigned int color, int texturePart, int renderFlags)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }
    const float frame0 = static_cast<float>(static_cast<double>(texturePart) * 0.047619049);
    const float frame1 = static_cast<float>(static_cast<double>(texturePart + 1) * 0.047619049);
    const vector3d positions[2][3] = {
        { AddVec3(corners[0], translation), AddVec3(corners[1], translation), AddVec3(corners[2], translation) },
        { AddVec3(corners[3], translation), AddVec3(corners[2], translation), AddVec3(corners[0], translation) },
    };
    const float uvs[2][3][2] = {
        { { 1.0f, frame0 }, { 1.0f, frame1 }, { 0.0f, frame1 } },
        { { 0.0f, frame0 }, { 0.0f, frame1 }, { 1.0f, frame0 } },
    };
    for (int triangle = 0; triangle < 2; ++triangle) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return;
        }
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_ONE;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            if (!ProjectPoint(positions[triangle][vertex], viewMatrix, &face->m_verts[vertex])) {
                return;
            }
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
}

void SubmitCloudRect(const vector3d corners[4], const matrix& viewMatrix, CTexture* texture,
    unsigned int color, int texturePart, bool reverse, bool inverseSourceAlpha, int renderFlags)
{
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }
    const float frame0 = static_cast<float>(static_cast<double>(texturePart) * 0.047619049);
    const float frame1 = static_cast<float>(static_cast<double>(texturePart + 1) * 0.047619049);
    const int order[2][4] = { { 0, 1, 2, 3 }, { 1, 0, 3, 2 } };
    const int* indices = order[reverse ? 1 : 0];
    const vector3d positions[2][3] = {
        { corners[indices[0]], corners[indices[1]], corners[indices[2]] },
        { corners[indices[3]], corners[indices[2]], corners[indices[0]] },
    };
    const float uvs[2][3][2] = {
        { { 1.0f, frame0 }, { 1.0f, frame1 }, { 0.0f, frame1 } },
        { { 0.0f, frame0 }, { 0.0f, frame1 }, { 1.0f, frame0 } },
    };
    for (int triangle = 0; triangle < 2; ++triangle) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return;
        }
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = inverseSourceAlpha ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            if (!ProjectPoint(positions[triangle][vertex], viewMatrix, &face->m_verts[vertex])) {
                return;
            }
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
}


float ResolveGroundHeight(const vector3d& position);

// Ref geometry: Render3DCasting_LowPolygon (10 segments). UV: one texture wrap around the ring
// (U per segment); full V along ribbon height — not 1/21 Ref strips (those read as thin stripes here).
void RenderBandLowPolygonTeiRect(const CEffectPrim& prim,
    const vector3d& base,
    const EffectBandState& band,
    const matrix& viewMatrix,
    CTexture* texture,
    float groundRadius)
{
    if (!band.active || !texture || texture == &CTexMgr::s_dummy_texture || band.alpha <= 0.0f) {
        return;
    }

    const int renderFlags = ResolveEffectRenderFlags(prim.m_renderFlag, 1 | 2);
    const D3DBLEND destBlend = ResolveEffectDestBlend(prim.m_renderFlag);
    constexpr int kSegments = 10;
    constexpr int kFullDisplayAngle = 360;

    const vector3d center = base;
    const float groundY = ResolveGroundHeight(center) + prim.m_deltaPos2.y;
    const float riseRadians = band.riseAngle * (kPi / 180.0f);
    const float cosRise = std::cos(riseRadians);
    const float sinRise = std::sin(riseRadians);
    const float radius = (std::max)(0.0f, groundRadius);
    const unsigned int color = PackColor(static_cast<unsigned int>((std::min)(255.0f, band.alpha)), prim.m_tintColor);

    const int basicAngle = kFullDisplayAngle / kSegments;
    for (int segmentIndex = 0; segmentIndex < kSegments; ++segmentIndex) {
        const int count0 = segmentIndex * basicAngle;
        const int count1 = (segmentIndex + 1) * basicAngle;
        const float angle0Deg = segmentIndex == kSegments ? band.rotStart : WrapAngle360(band.rotStart + static_cast<float>(count0));
        const float angle1Deg = (segmentIndex + 1) == kSegments ? band.rotStart : WrapAngle360(band.rotStart + static_cast<float>(count1));
        const float radians0 = angle0Deg * (kPi / 180.0f);
        const float radians1 = angle1Deg * (kPi / 180.0f);

        const size_t heightIndex0 = (std::min)(static_cast<size_t>(segmentIndex), band.heights.size() - 1);
        const size_t heightIndex1 = (std::min)(static_cast<size_t>(segmentIndex + 1), band.heights.size() - 1);
        const float height0 = band.heights[heightIndex0];
        const float height1 = band.heights[heightIndex1];

        const vector3d base0 = {
            center.x + radius * std::cos(radians0),
            groundY,
            center.z + radius * std::sin(radians0)
        };
        const vector3d base1 = {
            center.x + radius * std::cos(radians1),
            groundY,
            center.z + radius * std::sin(radians1)
        };
        const vector3d top0 = {
            base0.x + cosRise * height0 * std::cos(radians0),
            groundY - sinRise * height0,
            base0.z + cosRise * height0 * std::sin(radians0)
        };
        const vector3d top1 = {
            base1.x + cosRise * height1 * std::cos(radians1),
            groundY - sinRise * height1,
            base1.z + cosRise * height1 * std::sin(radians1)
        };

        const float invSeg = 1.0f / static_cast<float>(kSegments);
        const float segU0 = static_cast<float>(segmentIndex) * invSeg;
        const float segU1 = static_cast<float>(segmentIndex + 1) * invSeg;
        SubmitWorldTeiRect(base0, base1, top1, top0, viewMatrix, texture, color, segU0, segU1, 1.0f, 0.0f, destBlend, 0.0f, renderFlags);
    }
}

void UpdatePortalBands(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    if (prim->m_master) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }

    const int subtype = static_cast<int>(prim->m_param[0]);
    for (int bandIndex = 0; bandIndex < static_cast<int>(prim->m_bands.size()); ++bandIndex) {
        EffectBandState& band = prim->m_bands[static_cast<size_t>(bandIndex)];
        if (!band.active) {
            continue;
        }

        ++band.process;
        if (bandIndex == 0) {
            ++prim->m_bandController;
        }
        if (band.process <= 0) {
            continue;
        }

        if (subtype == 3) {
            band.radius = (std::max)(0.0f, band.radius - 0.25f);
            if (band.radius <= 0.0f) {
                band.alpha = (std::max)(0.0f, band.alpha - 7.0f);
                if (band.alpha <= 0.0f) {
                    if (prim->m_bandController <= 1400) {
                        band.process = 0;
                        band.radius = 12.0f;
                    } else {
                        band.active = false;
                    }
                }
            }
        } else {
            band.radius += 0.5f;
            if (band.radius > 7.0f) {
                band.alpha = (std::max)(0.0f, band.alpha - 15.0f);
                if (band.alpha <= 0.0f) {
                    if ((subtype == 1 && prim->m_bandController > 120) || prim->m_bandController > 1400) {
                        band.active = false;
                    } else {
                        band.process = 0;
                        band.radius = 0.0f;
                    }
                }
            }
        }

        if (subtype == 3) {
            if (band.process < 20) {
                band.alpha = (std::min)(240.0f, band.alpha + 12.0f);
            }
        } else if (band.process < 10) {
            band.alpha = (std::min)(240.0f, band.alpha + 24.0f);
        }

        float sampleHeight = band.radius;
        if (band.process <= 10) {
            sampleHeight *= (std::max)(0.0f, SinDeg(static_cast<float>(band.process * 9)));
        }
        sampleHeight = (std::max)(0.0f, sampleHeight);
        band.heights.fill(sampleHeight);
    }
}

void UpdateWindBands(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    for (EffectBandState& band : prim->m_bands) {
        if (!band.active) {
            continue;
        }

        ++band.process;
        band.rotStart = WrapAngle360(band.rotStart + 5.0f);
        band.alpha = (std::min)(band.modes[0] == 2 ? 180.0f : 120.0f, band.alpha + 3.0f);

        const u8 mode = band.modes[0];
        if (mode == 1) {
            if (band.process > 20) {
                band.alpha = (std::max)(0.0f, band.alpha - 2.0f);
            }
        } else if (mode == 2) {
            if (band.process > 50) {
                band.alpha = (std::max)(0.0f, band.alpha - 1.0f);
            }
            if (band.process > 12) {
                band.radius += 0.1f;
            }
        } else if (mode != 3 && band.process > 20) {
            band.radius += 0.1f;
        }

        if (band.process > 1400) {
            if (mode != 3) {
                band.alpha = (std::max)(0.0f, band.alpha - 3.0f);
            }
            if (band.alpha <= 0.0f) {
                band.active = false;
            }
        }

        if (band.process < 12) {
            if (mode == 2) {
                band.alpha = (std::min)(180.0f, band.alpha + 1.0f);
            } else {
                band.alpha = (std::min)(250.0f, band.alpha + 10.0f);
            }
        }
    }
}

float ResolveGroundHeight(const vector3d& position)
{
    if (!g_world.m_attr) {
        return position.y;
    }
    return g_world.m_attr->GetHeight(position.x, position.z);
}

CTexture* GetSoftGlowTexture(bool cloud)
{
    static CTexture* glowTexture = nullptr;
    static CTexture* cloudTexture = nullptr;
    CTexture*& selected = cloud ? cloudTexture : glowTexture;
    if (selected) {
        return selected;
    }

    constexpr int kSize = 64;
    std::vector<unsigned int> pixels(static_cast<size_t>(kSize) * kSize, 0u);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(fx * fx + fy * fy);
            const float angle = std::atan2(fy, fx);
            float alpha = 0.0f;
            if (cloud) {
                const float plume = (std::max)(0.0f, 1.0f - radius);
                alpha = plume * plume * (0.55f + 0.45f * std::sin(angle * 4.0f + radius * 9.0f));
            } else {
                const float core = (std::max)(0.0f, 1.0f - radius);
                const float ring = (std::max)(0.0f, 1.0f - std::fabs(radius - 0.58f) * 3.2f);
                alpha = (std::max)(core * core * core, ring * 0.9f);
            }
            const unsigned int alphaByte = static_cast<unsigned int>((std::min)(255.0f, alpha * 255.0f));
            pixels[static_cast<size_t>(y) * kSize + static_cast<size_t>(x)]
                = (alphaByte << 24) | (alphaByte << 16) | (alphaByte << 8) | alphaByte;
        }
    }

    selected = g_texMgr.CreateTexture(kSize, kSize, pixels.data(), PF_A8R8G8B8, false);
    return selected;
}

CTexture* GetSoftDiscTexture()
{
    static CTexture* discTexture = nullptr;
    if (discTexture) {
        return discTexture;
    }

    constexpr int kSize = 64;
    std::vector<unsigned int> pixels(static_cast<size_t>(kSize) * kSize, 0u);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(fx * fx + fy * fy);
            const float falloff = (std::max)(0.0f, 1.0f - radius);
            const float alpha = falloff * falloff * (0.65f + 0.35f * falloff);
            const unsigned int alphaByte = static_cast<unsigned int>((std::min)(255.0f, alpha * 255.0f));
            pixels[static_cast<size_t>(y) * kSize + static_cast<size_t>(x)]
                = (alphaByte << 24) | (alphaByte << 16) | (alphaByte << 8) | alphaByte;
        }
    }

    discTexture = g_texMgr.CreateTexture(kSize, kSize, pixels.data(), PF_A8R8G8B8, false);
    return discTexture;
}

CTexture* ResolveEffectTextureOrFallback(const std::string& path, bool cloudFallback)
{
    CTexture* texture = g_texMgr.GetTexture(path.c_str(), false);
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return GetSoftGlowTexture(cloudFallback);
    }
    return texture;
}

CTexture* TryResolveEffectTextureCandidates(std::initializer_list<const char*> paths)
{
    for (const char* path : paths) {
        if (!path || !*path) {
            continue;
        }
        CTexture* texture = g_texMgr.GetTexture(path, false);
        if (texture && texture != &CTexMgr::s_dummy_texture) {
            return texture;
        }

        const std::string rawPath = path;
        const size_t slash = rawPath.find_last_of("\\/");
        const std::string basename = slash == std::string::npos ? rawPath : rawPath.substr(slash + 1);
        const std::string& resolvedPath = ResolveDataPathByBasename(basename.c_str());
        if (!resolvedPath.empty()) {
            texture = g_texMgr.GetTexture(resolvedPath.c_str(), false);
            if (texture && texture != &CTexMgr::s_dummy_texture) {
                return texture;
            }
        }
    }
    return nullptr;
}

CTexture* ResolveEffectTextureCandidates(std::initializer_list<const char*> paths, bool cloudFallback)
{
    if (CTexture* texture = TryResolveEffectTextureCandidates(paths)) {
        return texture;
    }
    return GetSoftGlowTexture(cloudFallback);
}

CTexture* ResolveWeatherCloudTexture(int mapVariant)
{
    static const char* const kFogTextures[] = {
        "effect\\fog1.tga",
        "effect\\fog2.tga",
        "effect\\fog3.tga",
    };
    static const char* const kCloudTextures[] = {
        "effect\\cloud4.tga",
        "effect\\cloud1.tga",
        "effect\\cloud2.tga",
    };

    const char* const* textureNames = mapVariant == 3 ? kFogTextures : kCloudTextures;
    return g_texMgr.GetTexture(textureNames[rand() % 3], false);
}

std::array<CTexture*, 3> ResolveBlueFallTextures(int textureSet)
{
    if (textureSet == 1) {
        return {
            ResolveEffectTextureCandidates({ "effect\\waterfall31.tga" }, false),
            ResolveEffectTextureCandidates({ "effect\\waterfall32.tga" }, false),
            ResolveEffectTextureCandidates({ "effect\\waterfall33.tga" }, false),
        };
    }

    return {
        ResolveEffectTextureCandidates({ "effect\\waterfall11.tga" }, false),
        ResolveEffectTextureCandidates({ "effect\\waterfall12.tga" }, false),
        ResolveEffectTextureCandidates({ "effect\\waterfall13.tga" }, false),
    };
}

void ConfigureBlueFallBand(CEffectPrim* prim, int bandIndex, int axisMode, int heightMode)
{
    if (!prim || bandIndex < 0 || bandIndex >= static_cast<int>(prim->m_bands.size())) {
        return;
    }

    EffectBandState& band = prim->m_bands[static_cast<size_t>(bandIndex)];
    band = {};
    band.active = true;
    band.alpha = 120.0f;
    band.distance = 36.0f + static_cast<float>(bandIndex);
    band.radius = static_cast<float>(bandIndex) * 0.25f - 1.0f;
    band.maxHeight = heightMode == 1
        ? 30.0f - static_cast<float>(bandIndex) * 6.0f
        : 80.0f - static_cast<float>(bandIndex) * 13.0f;
    band.modes.fill(0);
    band.modes[0] = static_cast<u8>(axisMode != 0 ? 1 : 0);
    band.modes[1] = 1;
    for (int segmentIndex = 0; segmentIndex <= 20; ++segmentIndex) {
        band.heights[static_cast<size_t>(segmentIndex)] = static_cast<float>(segmentIndex) * kWaterfallSegmentHeight;
    }
}

bool UpdateWeatherCloudPrimitive(CEffectPrim* prim)
{
    if (!prim) {
        return false;
    }

    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_view) {
        EffectGi& first = prim->m_gi[0];
        EffectGi& second = prim->m_gi[1];
        first.alphaT = static_cast<short>(360 - static_cast<int>(mode->m_view->GetCameraLongitude()));
        short latitudeAngle = static_cast<short>(450 - static_cast<int>(mode->m_view->GetCameraLatitude()));
        if (latitudeAngle >= 360) latitudeAngle = static_cast<short>(latitudeAngle - 360);
        second.alphaT = latitudeAngle;
    }
    if (prim->m_master) prim->m_pos = prim->m_master->ResolveBasePosition();

    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;
        if (gi.height[7] < 99.0f) {
            ++gi.process;
            const int modeType = static_cast<int>(gi.height[7]);
            if (modeType == 0) {
                if (gi.process < 80 && prim->m_pos.y < gi.height[6]) gi.alphaB = static_cast<short>(gi.alphaB + 2);
            } else if (modeType == 2 || modeType == 5) {
                if (gi.process < 80) gi.alphaB = static_cast<short>(gi.alphaB + 2);
            } else if (modeType == 3) {
                if (gi.process < 170) ++gi.alphaB;
            } else if (gi.process < 80) {
                gi.alphaB = static_cast<short>(gi.alphaB + 3);
            }

            if (gi.process > gi.rotStart && --gi.alphaB <= 0) {
                gi.process = 0;
                gi.alphaB = 0;
                if (modeType == 0) {
                    if (prim->m_pos.y > gi.height[6]) gi.process = 300;
                } else if (modeType == 2 || modeType == 5) {
                    gi.sparkX = static_cast<float>(rand() % 300 - 150);
                    gi.sparkZ = static_cast<float>(rand() % 300 - 150);
                } else if (modeType == 3) {
                    gi.sparkX = static_cast<float>(rand() % 300 - 150);
                    gi.sparkZ = static_cast<float>(rand() % 300 - 150);
                    CGameMode* cloudMode = g_modeMgr.GetCurrentGameMode();
                    if (cloudMode && cloudMode->m_world && cloudMode->m_world->m_attr) {
                        const int heightRandom = rand() % 5;
                        gi.sparkY = cloudMode->m_world->m_attr->GetHeight(prim->m_pos.x + gi.sparkX, prim->m_pos.z + gi.sparkZ) - 20.0f - static_cast<float>(heightRandom);
                    } else {
                        (void)rand();
                    }
                } else {
                    gi.sparkX = static_cast<float>(rand() % 200 + 25);
                    if ((rand() & 1) == 0) gi.sparkX = -gi.sparkX;
                    gi.sparkZ = static_cast<float>(rand() % 200 + 25);
                    if ((rand() & 1) == 0) gi.sparkZ = -gi.sparkZ;
                }
                gi.sparkX += prim->m_pos.x;
                gi.sparkZ += prim->m_pos.z;
            }

            bool angle0Decreasing = false;
            for (int angleIndex : {0, 2}) {
                float& angle = gi.height[static_cast<size_t>(angleIndex)];
                float& target = gi.height[static_cast<size_t>(angleIndex + 1)];
                const bool decreasing = angle > target;
                if (angleIndex == 0) angle0Decreasing = decreasing;
                if (decreasing) {
                    angle = (std::max)(0.0f, angle - 1.0f);
                    if (angle <= target) {
                        const int angleLimit = static_cast<int>(angle);
                        target = static_cast<float>(angleLimit ? 359 - rand() % angleLimit : 359);
                    }
                } else {
                    angle = (std::min)(359.0f, angle + 1.0f);
                    if (angle > target) {
                        const int angleLimit = static_cast<int>(angle);
                        target = static_cast<float>(angleLimit ? rand() % angleLimit : 0);
                    }
                }
            }

            const int angleX = static_cast<int>(gi.height[0]);
            const int angleZ = static_cast<int>(gi.height[2]);
            if (modeType == 3) {
                gi.sparkX += g_sinTable[angleX] * 0.015f;
                gi.sparkZ += g_sinTable[angleZ] * 0.015f;
            } else if (modeType == 4) {
                const float sway = g_sinTable[angleX] * 0.2f;
                gi.sparkX += angle0Decreasing ? -sway : sway;
                gi.sparkZ += g_sinTable[angleZ] * 0.05f;
            } else if (modeType == 5) {
                gi.sparkX += g_sinTable[angleX] * 0.035f;
                gi.sparkZ += g_sinTable[angleZ] * 0.035f;
            } else {
                const float sway = g_sinTable[angleX] * 0.05f;
                gi.sparkX += angle0Decreasing ? -sway : sway;
                gi.sparkZ += g_sinTable[angleZ] * 0.05f;
            }
        }
        gi.height[8] += 1.0f;
        if (gi.height[8] >= 360.0f) gi.height[8] -= 360.0f;
    }
    return true;
}

bool UpdateWaterfallPrimitive(CEffectPrim* prim)
{
    if (!prim) {
        return false;
    }

    for (EffectBandState& band : prim->m_bands) {
        if (band.active) {
            ++band.process;
        }
    }

    return prim->m_stateCnt <= prim->m_duration;
}

bool UpdateMapPillarPrimitive(CEffectPrim* prim)
{
    if (!prim) {
        return false;
    }

    if (prim->m_master) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }

    bool anyActive = false;
    for (EffectBandState& band : prim->m_bands) {
        if (!band.active) {
            continue;
        }

        ++band.process;
        band.rotStart = WrapAngle360(band.rotStart + 1.0f);

        float heightScale = 0.0f;
        if (band.process >= 200) {
            if (band.process > 290) {
                heightScale = 1.0f;
            } else {
                const float growT = (std::clamp)(static_cast<float>(band.process - 200) / 90.0f, 0.0f, 1.0f);
                heightScale = SinDeg(growT * 90.0f);
            }
        }

        float fadeScale = 1.0f;
        if (band.process >= 800) {
            fadeScale = (std::max)(0.0f, 1.0f - static_cast<float>(band.process - 800) / 80.0f);
        }

        const float baseAlpha = band.fadeThreshold >= 0.0f ? band.fadeThreshold : 50.0f;
        band.alpha = baseAlpha * fadeScale;
        band.heights.fill(band.maxHeight * heightScale * fadeScale);
        if (band.alpha <= 0.0f && band.process > 880) {
            band.active = false;
            continue;
        }

        anyActive = anyActive || band.alpha > 0.0f;
    }

    return prim->m_stateCnt <= prim->m_duration || anyActive;
}

static bool TryPlaySingleEffectWave(const vector3d& soundPos, const std::string& relativePath)
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || !mode->m_world || !mode->m_world->m_player) {
        return false;
    }
    const vector3d& listener = mode->m_world->m_player->m_pos;
    return PlayWave(relativePath.c_str(), soundPos.x - listener.x, soundPos.y - listener.y,
        soundPos.z - listener.z);
}

struct ProvenClipWave {
    int type;
    int frame;
    float yOffset;
    const char* wave;
    int chance;
    int quakeFrame;
};

const ProvenClipWave kProvenClipWaves[] = {
    { 30, 0, 0.0f, "effect\\magician_thunderstorm.wav", 0, -1 },
    { 65, 0, 0.0f, "effect\\thief_invenom.wav", 0, -1 },
    { 66, 0, 0.0f, "effect\\Acolyte_cure.wav", 0, -1 },
    { 68, 0, 0.0f, "effect\\st_mvp.wav", 0, -1 },
    { 69, 0, 0.0f, "effect\\hunter_skidtrap.wav", 0, -1 },
    { 75, 0, 1.0f, "effect\\priest_gloria.wav", 0, -1 },
    { 76, 0, 0.0f, "effect\\priest_magnificat.wav", 0, -1 },
    { 77, 0, 0.0f, "effect\\priest_resurrection.wav", 0, -1 },
    { 78, 0, 0.0f, "effect\\priest_recovery.wav", 0, -1 },
    { 83, 0, 0.0f, "effect\\priest_sanctuary.wav", 0, -1 },
    { 84, 60, 0.0f, "effect\\priest_impositio.wav", 0, -1 },
    { 85, 15, 0.0f, "effect\\priest_lexaeterna.wav", 0, -1 },
    { 88, 0, 0.0f, "effect\\priest_suffragium.wav", 0, -1 },
    { 91, 0, 0.0f, "effect\\priest_benedictio.wav", 0, -1 },
    { 92, 0, 0.0f, "effect\\wizard_meteor.wav", 0, 40 },
    { 95, 0, -100.0f, "effect\\wizard_quagmire.wav", 12, -1 },
    { 96, 75, 0.0f, "effect\\wizard_fire_pillar_a.wav", 0, -1 },
    { 97, 0, 0.0f, "effect\\wizard_fire_pillar_b.wav", 0, -1 },
    { 102, 30, 0.0f, "effect\\wizard_fire_pillar_b.wav", 0, 35 },
    { 103, 0, 0.0f, "effect\\black_weapon_perfection.wav", 0, -1 },
    { 106, 20, 0.0f, "effect\\hunter_blastmine.wav", 0, -1 },
    { 107, 0, 0.0f, "effect\\hunter_claymoretrap.wav", 0, -1 },
    { 108, 0, 0.0f, "effect\\hunter_freezingtrap.wav", 0, -1 },
    { 111, 0, 0.0f, "effect\\hunter_springtrap.wav", 0, -1 },
    { 113, 0, 1.0f, "effect\\priest_magnus.wav", 0, -1 },
    { 124, 0, 0.0f, "effect\\assasin_poisonreact.wav", 0, -1 },
    { 127, 0, 0.0f, "effect\\assasin_poisonreact.wav", 0, -1 },
    { 129, 10, 0.0f, "effect\\assasin_venomsplasher.wav", 0, -1 },
    { 131, 0, 0.0f, "effect\\knight_autocounter.wav", 0, -1 },
    { 153, 0, 0.0f, "effect\\ac_concentration.wav", 0, -1 },
    { 154, 15, 0.0f, "effect\\bs_refinesuccess.wav", 0, -1 },
    { 155, 5, 0.0f, "effect\\bs_refinefailed.wav", 0, -1 },
    { 204, 0, -80.0f, "_heal_effect.wav", 0, -1 },
    { 205, 0, -80.0f, "_heal_effect.wav", 0, -1 },
    { 206, 0, -80.0f, "_heal_effect.wav", 0, -1 },
    { 207, 0, -80.0f, "_heal_effect.wav", 0, -1 },
    { 210, 0, -80.0f, "_heal_effect.wav", 0, -1 },
    { 234, 0, 0.0f, "effect\\sage_spell breake.wav", 0, -1 },
    { 245, 0, 0.0f, "effect\\cru_holy cross.wav", 0, -1 },
    { 255, 10, -150.0f, "_enemy_hit_wind1.wav", 0, -1 },
    { 256, 10, -150.0f, "_enemy_hit_wind1.wav", 0, -1 },
    { 257, 10, -150.0f, "_enemy_hit_wind1.wav", 0, -1 },
    { 258, 10, -150.0f, "_enemy_hit_wind1.wav", 0, -1 },
    { 390, 0, 0.0f, "effect\\black_overthrust.wav", 0, -1 },
    { 391, 0, 0.0f, "effect\\EF_IncAgility.wav", 0, -1 },
    { 273, 0, 0.0f, "effect\\mon_\xBF\xAC\xC8\xAF.wav", 0, -1 },
    { 208, 0, -80.0f, "effect\\\xC8\xED\xB1\xE2.wav", 0, -1 },
    { 209, 0, -80.0f, "effect\\\xC8\xED\xB1\xE2.wav", 0, -1 },
    { 211, 0, -80.0f, "effect\\\xC8\xED\xB1\xE2.wav", 0, -1 },
    { 130, 0, 0.0f, "effect\\knight_twohandquicken.wav", 0, -1 },
    { 250, 0, 0.0f, "effect\\knight_twohandquicken.wav", 0, -1 },
    { 269, 6, 0.0f, "effect\\\xBA\xB9\xC8\xA3\xB0\xDD.wav", 0, -1 },
    { 270, 6, 0.0f, "effect\\\xBA\xB9\xC8\xA3\xB0\xDD.wav", 0, -1 },
    { 271, 6, 0.0f, "effect\\\xBA\xB9\xC8\xA3\xB0\xDD.wav", 0, -1 },
    { 272, 6, 0.0f, "effect\\\xBA\xB9\xC8\xA3\xB0\xDD.wav", 0, -1 },
    { 440, 0, 0.0f, "effect\\\xBE\xC6\xBC\xFB\xC7\xC1\xC6\xBC\xBF\xC0.wav", 0, -1 },
};



bool TryPlayEffectWaveAt(const vector3d& soundPos, const char* name)
{
    return name && *name && TryPlaySingleEffectWave(soundPos, name);
}

std::string ResolveWorldStrName(const std::string& rawName)
{
    if (rawName.empty()) {
        return std::string();
    }

    std::string normalized = CollapseUtf8Latin1ToBytes(rawName);
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    if (normalized.find('.') == std::string::npos) {
        normalized += ".str";
    }

    std::vector<std::string> variants;
    variants.push_back(normalized);
    variants.push_back(ToLowerAscii(normalized));

    std::string underscoreVariant = ToLowerAscii(normalized);
    std::replace(underscoreVariant.begin(), underscoreVariant.end(), ' ', '_');
    variants.push_back(underscoreVariant);

    std::string spaceVariant = ToLowerAscii(normalized);
    std::replace(spaceVariant.begin(), spaceVariant.end(), '_', ' ');
    variants.push_back(spaceVariant);

    for (const std::string& variant : variants) {
        const std::array<std::string, 3> candidates = {
            variant,
            std::string("effect\\") + variant,
            std::string("data\\texture\\effect\\") + variant,
        };
        for (const std::string& candidate : candidates) {
            if (g_fileMgr.IsDataExist(candidate.c_str())) {
                if (candidate.rfind("effect\\", 0) == 0) {
                    return candidate.substr(7);
                }
                if (candidate.rfind("data\\texture\\effect\\", 0) == 0) {
                    return candidate.substr(20);
                }
                return candidate;
            }
        }

        std::string basename = variant;
        const size_t slash = basename.find_last_of("\\/");
        if (slash != std::string::npos) {
            basename = basename.substr(slash + 1);
        }
        const std::string lookupKey = ToLowerAscii(basename);
        const std::string& resolvedByBasename = ResolveDataPathByBasename(lookupKey.c_str());
        if (!resolvedByBasename.empty()) {
            return resolvedByBasename;
        }
    }

    return std::string();
}

std::string LowercaseAsciiCopy(const std::string& value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::string ExtractBasenameLower(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/");
    const std::string basename = slash == std::string::npos ? path : path.substr(slash + 1);
    return LowercaseAsciiCopy(basename);
}

const std::string& ResolveDataPathByBasename(const char* basename)
{
    static const std::string empty;
    static std::map<std::string, std::string> pathByBasename;
    static bool isInitialized = false;
    if (!basename || !*basename) {
        return empty;
    }

    if (!isInitialized) {
        isInitialized = true;
        std::vector<std::string> names;
        g_fileMgr.CollectDataNamesByExtension("act", names);
        g_fileMgr.CollectDataNamesByExtension("spr", names);
        g_fileMgr.CollectDataNamesByExtension("str", names);
        g_fileMgr.CollectDataNamesByExtension("bmp", names);
        g_fileMgr.CollectDataNamesByExtension("tga", names);
        for (const std::string& name : names) {
            const std::string key = ExtractBasenameLower(name);
            if (!key.empty() && pathByBasename.find(key) == pathByBasename.end()) {
                pathByBasename.emplace(key, name);
            }
        }
    }

    const auto it = pathByBasename.find(LowercaseAsciiCopy(basename));
    return it != pathByBasename.end() ? it->second : empty;
}

bool FindOpaqueBounds(const unsigned int* pixels, int width, int height, RECT* outBounds)
{
    if (!pixels || width <= 0 || height <= 0 || !outBounds) {
        return false;
    }

    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned int pixel = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
            if ((pixel >> 24) == 0u) {
                continue;
            }
            minX = (std::min)(minX, x);
            minY = (std::min)(minY, y);
            maxX = (std::max)(maxX, x);
            maxY = (std::max)(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return false;
    }

    outBounds->left = minX;
    outBounds->top = minY;
    outBounds->right = maxX + 1;
    outBounds->bottom = maxY + 1;
    return true;
}

void UnpremultiplyPixels(std::vector<unsigned int>& pixels)
{
    for (unsigned int& pixel : pixels) {
        const unsigned int alpha = (pixel >> 24) & 0xFFu;
        if (alpha == 0u || alpha == 0xFFu) {
            continue;
        }

        unsigned int red = (pixel >> 16) & 0xFFu;
        unsigned int green = (pixel >> 8) & 0xFFu;
        unsigned int blue = pixel & 0xFFu;
        red = (std::min)(255u, (red * 255u + alpha / 2u) / alpha);
        green = (std::min)(255u, (green * 255u + alpha / 2u) / alpha);
        blue = (std::min)(255u, (blue * 255u + alpha / 2u) / alpha);
        pixel = (alpha << 24) | (red << 16) | (green << 8) | blue;
    }
}

struct BakedEffectSpriteFrame {
    CTexture* texture = nullptr;
    int width = 0;
    int height = 0;
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    bool isValid = false;
};

unsigned int BakeEffectClipColor(unsigned int src, const CSprClip& clip)
{
    const unsigned int srcA = (src >> 24) & 0xFFu;
    const unsigned int srcR = (src >> 16) & 0xFFu;
    const unsigned int srcG = (src >> 8) & 0xFFu;
    const unsigned int srcB = src & 0xFFu;

    const unsigned int outA = srcA * clip.a / 255u;
    const unsigned int outR = srcR * clip.r / 255u;
    const unsigned int outG = srcG * clip.g / 255u;
    const unsigned int outB = srcB * clip.b / 255u;
    return (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

unsigned int BakeEffectPremultiplyColor(unsigned int color)
{
    const unsigned int alpha = (color >> 24) & 0xFFu;
    if (alpha == 0u || alpha == 0xFFu) {
        return color;
    }

    const unsigned int red = ((color >> 16) & 0xFFu) * alpha / 255u;
    const unsigned int green = ((color >> 8) & 0xFFu) * alpha / 255u;
    const unsigned int blue = (color & 0xFFu) * alpha / 255u;
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

void BakeEffectAlphaBlendPixel(unsigned int& dst, unsigned int src)
{
    const unsigned int srcA = (src >> 24) & 0xFFu;
    if (srcA == 0u) {
        return;
    }
    if (srcA == 0xFFu) {
        dst = src;
        return;
    }

    const unsigned int dstA = (dst >> 24) & 0xFFu;
    const unsigned int srcR = (src >> 16) & 0xFFu;
    const unsigned int srcG = (src >> 8) & 0xFFu;
    const unsigned int srcB = src & 0xFFu;
    const unsigned int dstR = (dst >> 16) & 0xFFu;
    const unsigned int dstG = (dst >> 8) & 0xFFu;
    const unsigned int dstB = dst & 0xFFu;

    const unsigned int invA = 255u - srcA;
    const unsigned int outA = srcA + (dstA * invA) / 255u;
    const unsigned int outR = srcR + (dstR * invA) / 255u;
    const unsigned int outG = srcG + (dstG * invA) / 255u;
    const unsigned int outB = srcB + (dstB * invA) / 255u;
    dst = (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

void BlitScaledEffectMotionToArgb(unsigned int* dest,
    int destW,
    int destH,
    int baseX,
    int baseY,
    CSprRes* sprRes,
    const CMotion* motion,
    unsigned int* palette)
{
    if (!dest || !sprRes || !motion || !palette) {
        return;
    }

    for (const CSprClip& clip : motion->sprClips) {
        if (clip.sprIndex < 0) {
            continue;
        }

        const SprImg* image = sprRes->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const float clipZoomX = clip.zoomX > 0.0f ? clip.zoomX : 1.0f;
        const float clipZoomY = clip.zoomY > 0.0f ? clip.zoomY : 1.0f;
        const bool flipX = (clip.flags & 1) != 0;
        const int logicalWidth = image->width * (image->isHalfW + 1);
        const int logicalHeight = image->height * (image->isHalfH + 1);
        if (logicalWidth <= 0 || logicalHeight <= 0) {
            continue;
        }

        const float left = static_cast<float>(clip.x) - static_cast<float>(logicalWidth) * clipZoomX * 0.5f;
        const float top = static_cast<float>(clip.y) - static_cast<float>(logicalHeight) * clipZoomY * 0.5f;
        const int drawLeft = static_cast<int>(std::floor(left));
        const int drawTop = static_cast<int>(std::floor(top));
        const int drawRight = static_cast<int>(std::ceil(left + static_cast<float>(logicalWidth) * clipZoomX));
        const int drawBottom = static_cast<int>(std::ceil(top + static_cast<float>(logicalHeight) * clipZoomY));

        for (int dy = drawTop; dy < drawBottom; ++dy) {
            const int destY = baseY + dy;
            if (destY < 0 || destY >= destH) {
                continue;
            }

            const int logicalY = static_cast<int>(std::floor((static_cast<float>(dy) + 0.5f - top) / clipZoomY));
            if (logicalY < 0 || logicalY >= logicalHeight) {
                continue;
            }
            const int sourceY = logicalY / (image->isHalfH + 1);

            for (int dx = drawLeft; dx < drawRight; ++dx) {
                const int destX = baseX + dx;
                if (destX < 0 || destX >= destW) {
                    continue;
                }

                int logicalX = static_cast<int>(std::floor((static_cast<float>(dx) + 0.5f - left) / clipZoomX));
                if (logicalX < 0 || logicalX >= logicalWidth) {
                    continue;
                }
                if (flipX) {
                    logicalX = logicalWidth - 1 - logicalX;
                }
                const int sourceX = logicalX / (image->isHalfW + 1);

                unsigned int srcColor = 0u;
                if (clip.clipType == 0) {
                    const unsigned char index = image->indices[static_cast<size_t>(sourceY) * image->width + sourceX];
                    if (index == 0u) {
                        continue;
                    }
                    srcColor = 0xFF000000u | (palette[index] & 0x00FFFFFFu);
                } else {
                    srcColor = image->rgba[static_cast<size_t>(sourceY) * image->width + sourceX];
                    if ((srcColor >> 24) == 0u) {
                        continue;
                    }
                }

                srcColor = BakeEffectClipColor(srcColor, clip);
                srcColor = BakeEffectPremultiplyColor(srcColor);
                BakeEffectAlphaBlendPixel(dest[static_cast<size_t>(destY) * destW + destX], srcColor);
            }
        }
    }
}

const BakedEffectSpriteFrame* GetBakedEffectSpriteFrame(const std::string& actPath,
    const std::string& sprPath,
    int actionIndex,
    int motionIndex)
{
    static std::map<std::string, BakedEffectSpriteFrame> cache;
    const std::string cacheKey = actPath + "|" + sprPath + "|" + std::to_string(actionIndex) + "|" + std::to_string(motionIndex);
    auto it = cache.find(cacheKey);
    if (it != cache.end()) {
        return it->second.isValid ? &it->second : nullptr;
    }

    BakedEffectSpriteFrame baked;
    CActRes* actRes = g_resMgr.GetAs<CActRes>(actPath.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprPath.c_str());
    if (actRes && sprRes) {
        if (actRes->GetMotionCount(actionIndex) > 0) {
            if (const CMotion* motion = actRes->GetMotion(actionIndex, motionIndex)) {
                constexpr int kCanvasSize = 384;
                constexpr int kCanvasCenter = kCanvasSize / 2;
                std::vector<unsigned int> canvasPixels(static_cast<size_t>(kCanvasSize) * static_cast<size_t>(kCanvasSize), 0u);
                BlitScaledEffectMotionToArgb(canvasPixels.data(), kCanvasSize, kCanvasSize, kCanvasCenter, kCanvasCenter, sprRes, motion, sprRes->m_pal);

                RECT bounds{};
                if (FindOpaqueBounds(canvasPixels.data(), kCanvasSize, kCanvasSize, &bounds)) {
                    baked.width = (std::max)(1, static_cast<int>(bounds.right - bounds.left));
                    baked.height = (std::max)(1, static_cast<int>(bounds.bottom - bounds.top));
                    baked.pivotX = static_cast<float>(kCanvasCenter - bounds.left);
                    baked.pivotY = static_cast<float>(kCanvasCenter - bounds.top);

                    std::vector<unsigned int> pixels(static_cast<size_t>(baked.width) * static_cast<size_t>(baked.height), 0u);
                    for (int y = 0; y < baked.height; ++y) {
                        const unsigned int* src = canvasPixels.data() + static_cast<size_t>(bounds.top + y) * kCanvasSize + bounds.left;
                        unsigned int* dst = pixels.data() + static_cast<size_t>(y) * baked.width;
                        std::memcpy(dst, src, static_cast<size_t>(baked.width) * sizeof(unsigned int));
                    }
                    UnpremultiplyPixels(pixels);
                    baked.texture = g_texMgr.CreateTexture(baked.width, baked.height, pixels.data(), PF_A8R8G8B8, true);
                    baked.isValid = baked.texture && baked.texture != &CTexMgr::s_dummy_texture;
                }
            }
        }
    }

    auto inserted = cache.emplace(cacheKey, baked);
    return inserted.first->second.isValid ? &inserted.first->second : nullptr;
}

bool ConfigureEffectSpritePrim(CEffectPrim* prim,
    std::initializer_list<const char*> stems,
    int actionIndex,
    float scale,
    bool repeat,
    float frameDelay,
    int motionBase)
{
    if (!prim) {
        return false;
    }

    for (const char* stem : stems) {
        if (!stem || !*stem) {
            continue;
        }

        const std::string actPath = ResolveDataPathByBasename((std::string(stem) + ".act").c_str());
        const std::string sprPath = ResolveDataPathByBasename((std::string(stem) + ".spr").c_str());
        if (actPath.empty() || sprPath.empty()) {
            continue;
        }

        prim->m_spriteActName = actPath;
        prim->m_spriteSprName = sprPath;
        prim->m_spriteAction = actionIndex;
        prim->m_spriteMotionBase = motionBase;
        prim->m_spriteFrameDelay = frameDelay;
        prim->m_spriteScale = scale;
        prim->m_spriteRepeat = repeat;
        return true;
    }

    return false;
}

bool SubmitAnimatedSpriteParticle(const CEffectPrim& prim,
    const vector3d& base,
    matrix* viewMatrix,
    unsigned int color,
    D3DBLEND destBlend,
    int renderFlags)
{
    const bool logEnchantPoison = prim.m_spriteSprName.find("particle3.spr") != std::string::npos
        || prim.m_spriteSprName.find("particle2.spr") != std::string::npos;
    if (!viewMatrix || prim.m_spriteActName.empty() || prim.m_spriteSprName.empty()) {
        if (logEnchantPoison) {
            DbgLog("[EnchantPoisonDbg] sprite submit skipped missing view/paths act='%s' spr='%s'\n",
                prim.m_spriteActName.c_str(),
                prim.m_spriteSprName.c_str());
        }
        return false;
    }

    CActRes* actRes = g_resMgr.GetAs<CActRes>(prim.m_spriteActName.c_str());
    if (!actRes) {
        if (logEnchantPoison) {
            DbgLog("[EnchantPoisonDbg] sprite submit missing act '%s'\n", prim.m_spriteActName.c_str());
        }
        return false;
    }

    int actionIndex = prim.m_spriteAction;
    if (actRes->GetMotionCount(actionIndex) <= 0) {
        actionIndex = 0;
    }

    const int motionCount = actRes->GetMotionCount(actionIndex);
    if (motionCount <= 0) {
        if (logEnchantPoison) {
            DbgLog("[EnchantPoisonDbg] sprite submit no motions action=%d act='%s'\n",
                actionIndex,
                prim.m_spriteActName.c_str());
        }
        return false;
    }

    const float frameDelay = prim.m_spriteFrameDelay > 0.0f
        ? prim.m_spriteFrameDelay
        : (std::max)(1.0f, actRes->GetDelay(actionIndex));
    const int motionAdvance = static_cast<int>(static_cast<float>(prim.m_stateCnt) / frameDelay);
    const int clampedBase = (std::max)(0, (std::min)(prim.m_spriteMotionBase, motionCount - 1));
    const int motionIndex = prim.m_spriteRepeat ? (clampedBase + motionAdvance) % motionCount
        : (std::min)(motionCount - 1, clampedBase + motionAdvance);

    const BakedEffectSpriteFrame* frame = GetBakedEffectSpriteFrame(
        prim.m_spriteActName,
        prim.m_spriteSprName,
        actionIndex,
        motionIndex);
    if (!frame || !frame->texture) {
        if (logEnchantPoison) {
            DbgLog("[EnchantPoisonDbg] sprite submit no baked frame action=%d motion=%d act='%s' spr='%s'\n",
                actionIndex,
                motionIndex,
                prim.m_spriteActName.c_str(),
                prim.m_spriteSprName.c_str());
        }
        return false;
    }

    (void)base;

    vector3d anchorPos = prim.m_pos;
    anchorPos.y += prim.m_param[2];
    tlvertex3d anchor{};
    if (!ProjectPoint(anchorPos, *viewMatrix, &anchor)) {
        if (logEnchantPoison) {
            DbgLog("[EnchantPoisonDbg] sprite submit project fail pos=(%.2f,%.2f,%.2f) base=(%.2f,%.2f,%.2f) state=%d size=%.3f\n",
                anchorPos.x,
                anchorPos.y,
                anchorPos.z,
                base.x,
                base.y,
                base.z,
                prim.m_stateCnt,
                prim.m_size);
        }
        return false;
    }

    const float pixelRatio = ResolveEffectPixelRatio(anchor);
    const float scale = (std::max)(0.1f, prim.m_spriteScale) * (std::max)(0.1f, prim.m_size);
    if (!std::isfinite(pixelRatio) || pixelRatio <= 0.0f) {
        if (logEnchantPoison) {
            DbgLog("[EnchantPoisonDbg] sprite submit bad pixelRatio=%.6f state=%d\n",
                pixelRatio,
                prim.m_stateCnt);
        }
        return false;
    }

    const float width = static_cast<float>(frame->width) * pixelRatio * scale;
    const float height = static_cast<float>(frame->height) * pixelRatio * scale;
    const float pivotX = frame->pivotX * pixelRatio * scale;
    const float pivotY = frame->pivotY * pixelRatio * scale;
    SubmitScreenQuadPivot(anchor,
        frame->texture,
        pivotX,
        pivotY,
        width,
        height,
        prim.m_roll,
        color,
        destBlend,
        0.0f,
        renderFlags,
        kEffectSpriteDepthBias);
    if (logEnchantPoison && prim.m_stateCnt <= 3) {
        DbgLog("[EnchantPoisonDbg] sprite submit ok state=%d pos=(%.2f,%.2f,%.2f) screen=(%.2f,%.2f) size=(%.2f,%.2f) pivot=(%.2f,%.2f) roll=%.2f alpha=%u\n",
            prim.m_stateCnt,
            anchorPos.x,
            anchorPos.y,
            anchorPos.z,
            anchor.x,
            anchor.y,
            width,
            height,
            pivotX,
            pivotY,
            prim.m_roll,
            (color >> 24) & 0xFFu);
    }
    return true;
}

void AddXformDelta(KAC_XFORMDATA* target, const KAC_XFORMDATA& delta)
{
    if (!target) {
        return;
    }

    target->x += delta.x;
    target->y += delta.y;
    target->u += delta.u;
    target->v += delta.v;
    target->us += delta.us;
    target->vs += delta.vs;
    target->u2 += delta.u2;
    target->v2 += delta.v2;
    target->us2 += delta.us2;
    target->vs2 += delta.vs2;
    for (int index = 0; index < 4; ++index) {
        target->ax[index] += delta.ax[index];
        target->ay[index] += delta.ay[index];
    }
    target->rz += delta.rz;
    target->crR += delta.crR;
    target->crG += delta.crG;
    target->crB += delta.crB;
    target->crA += delta.crA;
}

std::string ResolveStrName(int effectId)
{
    const RagEffectCatalogEntry* entry = FindRagEffectCatalogEntry(effectId);
    if (!entry) {
        return std::string();
    }

    const std::string resolved = ResolveCatalogStrName(*entry);
    return resolved.empty() ? resolved : ResolveWorldStrName(resolved);
}

COLORREF ResolvePortalTint(const std::string& name)
{
    if (ContainsIgnoreCaseAscii(name, "red")) {
        return RGB(255, 128, 128);
    }
    if (ContainsIgnoreCaseAscii(name, "green")) {
        return RGB(160, 255, 170);
    }
    return RGB(86, 196, 255);
}

void ShiftCircleSegments(CEffectPrim* prim)
{
    if (!prim || prim->m_numSegments <= 0) {
        return;
    }

    const int historyCount = (std::min)(prim->m_numSegments, static_cast<int>(prim->m_segments.size()));
    for (int index = historyCount - 1; index > 0; --index) {
        prim->m_segments[static_cast<size_t>(index)] = prim->m_segments[static_cast<size_t>(index - 1)];
        const double step = index;
        prim->m_segments[static_cast<size_t>(index)].alpha = static_cast<float>(prim->m_alpha
            - prim->m_alpha / static_cast<double>(prim->m_numSegments) * step);
        prim->m_segments[static_cast<size_t>(index)].argb = PackColor(
            static_cast<u8>(static_cast<long long>(prim->m_segments[static_cast<size_t>(index)].alpha)), prim->m_tintColor);
        prim->m_segments[static_cast<size_t>(index)].size = static_cast<float>(prim->m_size
            - prim->m_size / static_cast<double>(prim->m_numSegments) * step * 0.5);
    }

    prim->m_segments[0].pos = prim->m_pos;
    prim->m_segments[0].radius = prim->m_radius;
    prim->m_segments[0].longitude = prim->m_longitude;
    prim->m_segments[0].size = prim->m_size;
    prim->m_segments[0].alpha = prim->m_alpha;
    prim->m_segments[0].argb = PackColor(static_cast<u8>(static_cast<long long>(prim->m_alpha)), prim->m_tintColor);
    prim->m_segments[0].transform = prim->m_matrix;
}

// HighPriest.exe.c:466090-466174. ShiftSegment copies each segment's full
// payload, attenuates alpha/size, then always writes the current slot zero.
void ShiftReferenceSegments(CEffectPrim* prim)
{
    const int count = prim->m_numSegments;
    for (int index = count - 1; index > 0; --index) {
        EffectSegmentState& destination = prim->m_segments[static_cast<size_t>(index)];
        destination = prim->m_segments[static_cast<size_t>(index - 1)];
        prim->m_segmentQuadPositions[static_cast<size_t>(index)]
            = prim->m_segmentQuadPositions[static_cast<size_t>(index - 1)];
        const double step = index;
        destination.alpha = static_cast<float>(static_cast<double>(prim->m_alpha)
            - static_cast<double>(prim->m_alpha) / count * step);
        destination.argb = PackColor(static_cast<u8>(static_cast<long long>(destination.alpha)),
            RGB(static_cast<u8>(prim->m_red), static_cast<u8>(prim->m_green), static_cast<u8>(prim->m_blue)));
        destination.size = static_cast<float>(static_cast<double>(prim->m_size)
            - static_cast<double>(prim->m_size) / count * step * 0.5);
    }

    EffectSegmentState& current = prim->m_segments[0];
    current.pos = prim->m_pos;
    prim->m_segmentQuadPositions[0] = prim->m_segPos;
    current.size = prim->m_size;
    current.radius = prim->m_radius;
    current.alpha = prim->m_alpha;
    current.transform = prim->m_matrix;
    current.longitude = prim->m_longitude;
    current.argb = PackColor(static_cast<u8>(static_cast<long long>(prim->m_alpha)),
        RGB(static_cast<u8>(prim->m_red), static_cast<u8>(prim->m_green), static_cast<u8>(prim->m_blue)));
}

// Ref/RagEffectPrim.cpp:1660-1698 (0x640520). Initialization copies
// full state to every slot; it does not apply ShiftSegment's trail attenuation.
void InitReferenceSegments(CEffectPrim* prim)
{
    const int count = (std::min)(prim->m_numSegments, static_cast<int>(prim->m_segments.size()));
    for (int i = 0; i < count; ++i) {
        EffectSegmentState& segment = prim->m_segments[static_cast<size_t>(i)];
        segment.pos = prim->m_pos;
        segment.radius = prim->m_radius;
        segment.longitude = prim->m_longitude;
        segment.size = prim->m_size;
        segment.alpha = prim->m_alpha;
        segment.argb = PackColor(static_cast<u8>(static_cast<long long>(prim->m_alpha)), prim->m_tintColor);
        segment.transform = prim->m_matrix;
    }
}

// HighPriest.exe.c:55640-55652 (0x4131B0). Ref/Renderer.cpp repeats
// the other overload's signature; this overload uses half-screen offsets and
// device z, with no invented near-plane cutoff, clamp or pixel displacement.
void ProjectReferenceEffectVertex(const vector3d& point, const matrix& view, tlvertex3d& vertex)
{
    const double reciprocal = 1.0 / (static_cast<double>(view.m[2][2]) * point.z
        + static_cast<double>(view.m[1][2]) * point.y + static_cast<double>(view.m[0][2]) * point.x + view.m[3][2]);
    vertex.oow = static_cast<float>(reciprocal);
    vertex.x = static_cast<float>((static_cast<double>(view.m[2][0]) * point.z
        + static_cast<double>(view.m[1][0]) * point.y + static_cast<double>(point.x) * view.m[0][0] + view.m[3][0])
        * reciprocal * g_renderer.m_hpc + g_renderer.m_halfWidth);
    vertex.y = static_cast<float>((static_cast<double>(view.m[2][1]) * point.z
        + static_cast<double>(view.m[1][1]) * point.y + static_cast<double>(view.m[0][1]) * point.x + view.m[3][1])
        * g_renderer.m_vpc * vertex.oow + g_renderer.m_halfHeight);
    vertex.z = static_cast<float>(1500.0 / (1500.0 - 10.0) * (1.0 / vertex.oow - 10.0) * vertex.oow);
}

void SubmitReferenceEffectTriangle(const vector3d (&points)[3], const matrix& view,
    CTexture* texture, u32 color, const float (&uvs)[3][2], int renderFlag)
{
    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) return;
    face->primType = D3DPT_TRIANGLELIST;
    face->verts = face->m_verts;
    face->numVerts = 3;
    face->indices = nullptr; face->numIndices = 0;
    face->tex = texture; face->mtPreset = 0; face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = (renderFlag & 2) != 0 ? D3DBLEND_ONE : D3DBLEND_SRCALPHA;
    face->destAlphaMode = (renderFlag & 6) != 0 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0;
    for (int i = 0; i < 3; ++i) {
        ProjectReferenceEffectVertex(points[i], view, face->m_verts[i]);
        face->m_verts[i].color = color;
        face->m_verts[i].specular = 0xFF000000u;
        face->m_verts[i].tu = uvs[i][0]; face->m_verts[i].tv = uvs[i][1];
    }
    g_renderer.AddReferenceEffectTriangle(face, renderFlag);
}

// Ref/RagEffectPrim.cpp:2855-3187 (0x6476E0). The 2008 loop truncates
// each next angle to an int. Use primitive matrix/width with segment radius,
// position and stored ARGB; do not reconstruct state from current alpha.
void RenderReferenceCircle(const CEffectPrim& prim, const matrix& view)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const float stop = 360.0f - prim.m_arcAngle;
    float u = 0.0f;
    for (int j = 0; j < prim.m_numSegments && j < static_cast<int>(prim.m_segments.size()); ++j) {
        const EffectSegmentState& segment = prim.m_segments[static_cast<size_t>(j)];
        const float inner = segment.radius - prim.m_size;
        for (int angle = 0; angle <= stop;) {
            const int next = static_cast<int>(static_cast<double>(angle) + prim.m_arcAngle);
            if (next <= angle || next > 360) return; // Invalid arc, no replacement geometry.
            const auto place = [&](float radius, int degrees) {
                const double x = static_cast<double>(g_cosTable[degrees]) * radius;
                const double y = static_cast<double>(g_sinTable[degrees]) * radius;
                const vector3d transformed{
                    static_cast<float>(y * prim.m_matrix.m[1][0] + x * prim.m_matrix.m[0][0] + prim.m_matrix.m[3][0]),
                    static_cast<float>(y * prim.m_matrix.m[1][1] + x * prim.m_matrix.m[0][1] + prim.m_matrix.m[3][1]),
                    static_cast<float>(y * prim.m_matrix.m[1][2] + x * prim.m_matrix.m[0][2] + prim.m_matrix.m[3][2])
                };
                return AddVec3(transformed, segment.pos);
            };
            const vector3d a = place(inner, angle), b = place(inner, next);
            const vector3d c = place(segment.radius, angle), d = place(segment.radius, next);
            const vector3d first[3] = {a,b,c}, second[3] = {d,c,b};
            const float firstUV[3][2] = {{u,1.0f},{u+0.25f,1.0f},{u,0.0f}};
            const float secondUV[3][2] = {{u+0.25f,0.0f},{u,0.0f},{u+0.25f,1.0f}};
            SubmitReferenceEffectTriangle(first, view, texture, segment.argb, firstUV, static_cast<int>(prim.m_renderFlag));
            SubmitReferenceEffectTriangle(second, view, texture, segment.argb, secondUV, static_cast<int>(prim.m_renderFlag));
            u += 0.25f;
            if (u >= 1.0f) u = 0.0f;
            angle = next;
        }
    }
}

// Ref/RagEffectPrim.cpp:3521-3801 (0x647E50), original full cylinder.
// Current primitive matrix/position and ProcessAlpha's stored color; source
// submits flag 1 regardless of m_renderFlag. Unlike circles, angles stay float.
void RenderReferenceCylinder(const CEffectPrim& prim, const matrix& view)
{
    if (prim.m_curMotion < 0 || prim.m_curMotion >= static_cast<int>(prim.m_texture.size())) return;
    CTexture* texture = prim.m_texture[static_cast<size_t>(prim.m_curMotion)];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    if (!(prim.m_arcAngle > 0.0f && prim.m_arcAngle <= 360.0f)) return;
    const float stop = 360.0f - prim.m_arcAngle;
    float angle = 0.0f, u = 0.0f;
    for (;;) {
        const double next = static_cast<double>(angle) + prim.m_arcAngle;
        const int firstAngle = static_cast<int>(angle), nextAngle = static_cast<int>(next);
        if (nextAngle > 360) return;
        const auto place = [&](int degrees, bool top, bool end) {
            const double radius = top ? prim.m_outerSize : prim.m_innerSize;
            // Source stores the top cosine products to float before matrix use.
            const double x = top ? static_cast<float>(g_cosTable[degrees] * radius) : g_cosTable[degrees] * radius;
            const double z = g_sinTable[degrees] * radius;
            const double y = top ? -static_cast<double>(prim.m_heightSize) : 0.0;
            const vector3d transformed{
                static_cast<float>(z * prim.m_matrix.m[2][0] + y * prim.m_matrix.m[1][0] + x * prim.m_matrix.m[0][0] + prim.m_matrix.m[3][0]),
                static_cast<float>((top && end
                    ? x * prim.m_matrix.m[0][1] + z * prim.m_matrix.m[2][1] + y * prim.m_matrix.m[1][1]
                    : z * prim.m_matrix.m[2][1] + y * prim.m_matrix.m[1][1] + x * prim.m_matrix.m[0][1]) + prim.m_matrix.m[3][1]),
                static_cast<float>((top && end
                    ? x * prim.m_matrix.m[0][2] + z * prim.m_matrix.m[2][2] + y * prim.m_matrix.m[1][2]
                    : z * prim.m_matrix.m[2][2] + y * prim.m_matrix.m[1][2] + x * prim.m_matrix.m[0][2]) + prim.m_matrix.m[3][2])
            };
            return AddVec3(transformed, prim.m_pos);
        };
        const vector3d a = place(firstAngle, false, false), b = place(nextAngle, false, true);
        const vector3d c = place(firstAngle, true, false), d = place(nextAngle, true, true);
        const vector3d first[3] = {a,c,b}, second[3] = {b,c,d};
        const float nextU = u + 0.25f;
        const float firstUV[3][2] = {{u,1.0f},{u,0.0f},{nextU,1.0f}};
        const float secondUV[3][2] = {{nextU,1.0f},{u,0.0f},{nextU,0.0f}};
        SubmitReferenceEffectTriangle(first, view, texture, prim.m_argb, firstUV, 1);
        SubmitReferenceEffectTriangle(second, view, texture, prim.m_argb, secondUV, 1);
        u = nextU >= 1.0f ? 0.0f : nextU;
        angle = static_cast<float>(next);
        if (next > stop) break;
    }
}

// Ref/RagEffectPrim.cpp:3195-3514 (0x646EC0). Two perpendicular strips,
// four independent triangles, original stored color/texture and flag 1.
void RenderReferenceCrossTexture(const CEffectPrim& prim, const matrix& view)
{
    if (prim.m_curMotion < 0 || prim.m_curMotion >= static_cast<int>(prim.m_texture.size())) return;
    CTexture* texture = prim.m_texture[static_cast<size_t>(prim.m_curMotion)];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const float h = prim.m_heightSize, w = prim.m_widthSize;
    const vector3d local[2][4] = {
        {{-h,0,w},{-h,0,-w},{h,0,w},{h,0,-w}},
        {{0,-h,w},{0,-h,-w},{0,h,w},{0,h,-w}}
    };
    const float firstUV[3][2] = {{0,1},{1,1},{0,0}};
    const float secondUV[3][2] = {{1,0},{0,0},{1,1}};
    for (int strip = 0; strip < 2; ++strip) {
        vector3d points[4];
        for (int i = 0; i < 4; ++i) {
            const vector3d& v = local[strip][i];
            float transformed[3];
            for (int axis = 0; axis < 3; ++axis) {
                const double yz = i == 0
                    ? static_cast<double>(prim.m_matrix.m[1][axis]) * v.y + static_cast<double>(prim.m_matrix.m[2][axis]) * v.z
                    : static_cast<double>(prim.m_matrix.m[2][axis]) * v.z + static_cast<double>(prim.m_matrix.m[1][axis]) * v.y;
                const double coordinate = yz + static_cast<double>(prim.m_matrix.m[0][axis]) * v.x + prim.m_matrix.m[3][axis];
                // Source keeps first-vertex x in x87 through world translation.
                transformed[axis] = static_cast<float>(coordinate);
                if (i == 0 && axis == 0) transformed[axis] = static_cast<float>(coordinate + prim.m_pos.x);
            }
            points[i] = AddVec3(vector3d{transformed[0],transformed[1],transformed[2]}, prim.m_pos);
            if (i == 0) points[i].x = transformed[0];
        }
        const vector3d first[3] = {points[0],points[1],points[2]};
        const vector3d second[3] = {points[3],points[2],points[1]};
        SubmitReferenceEffectTriangle(first, view, texture, prim.m_argb, firstUV, 1);
        SubmitReferenceEffectTriangle(second, view, texture, prim.m_argb, secondUV, 1);
    }
}

// Ref/RagEffectPrim.cpp:4965-5166 (0x646770), reachable SprayPond profiles.
void RenderReferenceTexture(const CEffectPrim& prim, const matrix& view)
{
    if (prim.m_curMotion < 0 || prim.m_curMotion >= static_cast<int>(prim.m_texture.size())) return;
    CTexture* texture = prim.m_texture[static_cast<size_t>(prim.m_curMotion)];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const float w = prim.m_widthSize, h = prim.m_heightSize;
    const vector3d local[4] = {{0,-h,w},{0,-h,-w},{0,h,w},{0,h,-w}};
    vector3d points[4];
    for (int i = 0; i < 4; ++i) {
        float rotated[3];
        for (int axis = 0; axis < 3; ++axis) {
            rotated[axis] = static_cast<float>(static_cast<double>(local[i].z)*prim.m_rotMat.m[2][axis]
                + static_cast<double>(local[i].y)*prim.m_rotMat.m[1][axis] + prim.m_rotMat.m[3][axis]);
        }
        points[i] = AddVec3(vector3d{rotated[0],rotated[1],rotated[2]},prim.m_pos);
    }
    const vector3d first[3] = {points[0],points[1],points[2]}, second[3] = {points[3],points[2],points[1]};
    const float firstUV[3][2] = {{0,prim.m_v1},{1,prim.m_v1},{0,prim.m_v2}};
    const float secondUV[3][2] = {{1,prim.m_v2},{0,prim.m_v2},{1,prim.m_v1}};
    SubmitReferenceEffectTriangle(first,view,texture,prim.m_argb,firstUV,static_cast<int>(prim.m_renderFlag));
    SubmitReferenceEffectTriangle(second,view,texture,prim.m_argb,secondUV,static_cast<int>(prim.m_renderFlag));
}

// Ref/RagEffectPrim.cpp:2610-2715 (0x644B20), ColdHit's two screen triangles.
void RenderReferenceScreenQuad(const CEffectPrim& prim)
{
    if (prim.m_curMotion < 0 || prim.m_curMotion >= static_cast<int>(prim.m_texture.size())) return;
    CTexture* texture = prim.m_texture[static_cast<size_t>(prim.m_curMotion)];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const float sine = GetSin(static_cast<int>(prim.m_roll)), cosine = GetCos(static_cast<int>(prim.m_roll));
    const float halfWidth = static_cast<float>(prim.m_widthSize*0.5);
    const float halfHeight = static_cast<float>(prim.m_heightSize*0.5);
    const float inset = static_cast<float>(prim.m_heightSize*0.5*0.83333331);
    const float local[2][3][2] = {
        {{-halfWidth,inset-halfHeight},{0,halfHeight},{0,-halfHeight}},
        {{halfWidth,halfHeight-inset},{0,halfHeight},{0,-halfHeight}}
    };
    for (int side = 0; side < 2; ++side) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) return;
        face->primType = D3DPT_TRIANGLELIST; face->numVerts = 3;
        face->verts = face->m_verts; face->indices = nullptr; face->numIndices = 0;
        face->tex = texture; face->mtPreset = 0; face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA; face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        for (int i = 0; i < 3; ++i) {
            const double x = local[side][i][0], y = local[side][i][1];
            tlvertex3d& vertex = face->m_verts[i];
            vertex.x = static_cast<float>(x*cosine-y*sine+prim.m_pos.x);
            vertex.y = static_cast<float>(y*cosine+x*sine+prim.m_pos.y);
            vertex.z = 0.000001f; vertex.oow = 0.99999899f;
            vertex.color = i == 1 ? 0xFFFFFFFFu : prim.m_argb;
            vertex.specular = 0xFF000000u;
            vertex.tu = i == 0 ? static_cast<float>(side) : 0.5f;
            vertex.tv = i == 0 ? 0.5f : (i == 1 ? 1.0f : 0.0f);
        }
        g_renderer.AddReferenceEffectTriangle(face,1);
    }
}

// Ref/RagEffectPrim.cpp:2722-2850 (0x644DD0), centered/bottom-pivot
// screen triangles with integer-degree roll; no world projection/scaling.
void RenderReferenceScreenTexture(const CEffectPrim& prim)
{
    if (prim.m_curMotion < 0 || prim.m_curMotion >= static_cast<int>(prim.m_texture.size())) return;
    CTexture* texture = prim.m_texture[static_cast<size_t>(prim.m_curMotion)];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const float x = prim.m_pos.x, y = prim.m_pos.y, w = prim.m_widthSize, h = prim.m_heightSize;
    const float sine = GetSin(static_cast<int>(prim.m_roll));
    const double cosine = GetCos(static_cast<int>(prim.m_roll));
    const double top = (prim.m_pattern & 1) != 0 ? static_cast<double>(h) * -2.0 : -static_cast<double>(h);
    const float bottom = (prim.m_pattern & 1) != 0 ? 0.0f : h;
    const float leftCos = static_cast<float>(-static_cast<double>(w) * cosine);
    const float topSin = static_cast<float>(top * sine), topCos = static_cast<float>(top * cosine);
    const double leftSin = -static_cast<double>(w) * sine, rightCos = static_cast<double>(w) * cosine;
    const float rightCosStored = static_cast<float>(rightCos);
    const double rightSin = static_cast<double>(w) * sine;
    const float rightSinStored = static_cast<float>(rightSin), bottomSin = bottom * sine;
    const double bottomCos = bottom * cosine;
    const float bottomCosStored = static_cast<float>(bottomCos);
    const float localX[4] = {leftCos-bottomSin,leftCos-topSin,rightCosStored-bottomSin,static_cast<float>(rightCos-topSin)};
    const double localY[4] = {bottomCos+leftSin,static_cast<float>(leftSin+topCos),
        static_cast<float>(bottomCosStored+rightSinStored),static_cast<float>(rightSin+topCos)};
    const int order[2][3] = {{0,1,2},{2,1,3}};
    const float uv[4][2] = {{0,1},{0,0},{1,1},{1,0}};
    for (const auto& triangle : order) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) return;
        face->primType = D3DPT_TRIANGLELIST; face->numVerts = 3;
        face->verts = face->m_verts; face->indices = nullptr; face->numIndices = 0;
        face->tex = texture; face->mtPreset = 0; face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA; face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        for (int i = 0; i < 3; ++i) {
            const int corner = triangle[i];
            tlvertex3d& vertex = face->m_verts[i];
            vertex.x = static_cast<float>(static_cast<double>(localX[corner]) + x - 0.5);
            vertex.y = static_cast<float>(localY[corner] + y - 0.5);
            vertex.z = 0.000001f; vertex.oow = 0.99999899f;
            vertex.color = prim.m_argb; vertex.specular = 0xFF000000u;
            vertex.tu = uv[corner][0]; vertex.tv = uv[corner][1];
        }
        g_renderer.AddReferenceEffectTriangle(face, 1);
    }
}

// Ref/RagEffectPrim.cpp:4077-4251 (0x646240), four faces, source UV bands.
void RenderReferenceQuadHorn(const CEffectPrim& prim, const matrix& view)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const float size = prim.m_size, height = prim.m_heightSize;
    const vector3d faces[4][3] = {
        {{-size,-size,0},{0,0,height},{size,-size,0}},
        {{-size,-size,0},{0,0,height},{-size,size,0}},
        {{-size,size,0},{0,0,height},{size,size,0}},
        {{size,size,0},{0,0,height},{size,-size,0}}
    };
    float u = 0.0f;
    for (const auto& face : faces) {
        vector3d world[3];
        for (int i = 0; i < 3; ++i) {
            const vector3d& p = face[i];
            const matrix& m = prim.m_rotMat;
            const float x = static_cast<float>(static_cast<double>(m.m[2][0])*p.z + static_cast<double>(m.m[1][0])*p.y + static_cast<double>(m.m[0][0])*p.x + m.m[3][0]);
            const float y = i == 2
                ? static_cast<float>(static_cast<double>(m.m[2][1])*p.z + static_cast<double>(m.m[0][1])*p.x + static_cast<double>(m.m[1][1])*p.y + m.m[3][1])
                : static_cast<float>(static_cast<double>(m.m[2][1])*p.z + static_cast<double>(m.m[1][1])*p.y + static_cast<double>(m.m[0][1])*p.x + m.m[3][1]);
            const float z = i == 2
                ? static_cast<float>(static_cast<double>(m.m[2][2])*p.z + static_cast<double>(m.m[0][2])*p.x + static_cast<double>(m.m[1][2])*p.y + m.m[3][2])
                : static_cast<float>(static_cast<double>(m.m[2][2])*p.z + static_cast<double>(m.m[1][2])*p.y + static_cast<double>(m.m[0][2])*p.x + m.m[3][2]);
            world[i] = {prim.m_pos.x+x,prim.m_pos.y+y,prim.m_pos.z+z};
        }
        const float nextU = static_cast<float>(u + 0.2);
        const float uv[3][2] = {{u,1},{u,0},{nextU,1}};
        SubmitReferenceEffectTriangle(world,view,texture,prim.m_argb,uv,1);
        u = nextU;
    }
}

// Ref/RagEffectPrim.cpp:2551-2603 (0x6450D0), original screen ray triangle.
void RenderReferenceFlash(const CEffectPrim& prim)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    const int first = static_cast<int>(static_cast<double>(prim.m_longitude)-prim.m_arcAngle);
    const int second = static_cast<int>(static_cast<double>(prim.m_arcAngle)+prim.m_longitude);
    const float cosine1 = GetCos(first), sine1 = GetSin(first), cosine2 = GetCos(second);
    const double sine2 = GetSin(second);
    const float x[3] = {prim.m_pos.x,
        static_cast<float>(static_cast<double>(cosine1)*prim.m_heightSize+prim.m_pos.x),
        static_cast<float>(static_cast<double>(cosine2)*prim.m_heightSize+prim.m_pos.x)};
    const float y[3] = {prim.m_pos.y,
        static_cast<float>(prim.m_pos.y-static_cast<double>(sine1)*prim.m_heightSize),
        static_cast<float>(prim.m_pos.y-sine2*prim.m_heightSize)};
    const float uv[3][2] = {{0.5f,0},{1,1},{0,1}};
    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) return;
    face->primType = D3DPT_TRIANGLELIST; face->numVerts = 3;
    face->verts = face->m_verts; face->indices = nullptr; face->numIndices = 0;
    face->tex = texture; face->mtPreset = 0; face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA; face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    for (int i = 0; i < 3; ++i) {
        auto& vertex = face->m_verts[i];
        vertex.x = x[i]; vertex.y = y[i]; vertex.z = 0.000001f; vertex.oow = 0.99999899f;
        vertex.color = prim.m_argb; vertex.specular = 0xFF000000u;
        vertex.tu = uv[i][0]; vertex.tv = uv[i][1];
    }
    g_renderer.AddReferenceEffectTriangle(face,1);
}

// Ref/RagEffectPrim.cpp:2342-2544 (0x645520), original screen annulus.
void RenderReferenceScreenCircle(const CEffectPrim& prim)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    if (!(prim.m_arcAngle > 0.0f && prim.m_arcAngle <= 360.0f)) return;
    const float inner = prim.m_radius-prim.m_size, stop = 360.0f-prim.m_arcAngle;
    float angle = 0.0f, u = 0.0f;
    for (;;) {
        const double next = static_cast<double>(angle)+prim.m_arcAngle;
        const int first = static_cast<int>(angle), second = static_cast<int>(next);
        if (second > 360) return;
        const double cosine1 = g_cosTable[first];
        const float sine1 = g_sinTable[first], cosine2 = g_cosTable[second], sine2 = g_sinTable[second];
        const float innerX = static_cast<float>(cosine1*inner), innerY = sine1*inner;
        const float x[6] = {
            static_cast<float>(cosine1*inner+prim.m_pos.x),
            static_cast<float>(static_cast<double>(cosine2)*prim.m_radius+prim.m_pos.x),
            static_cast<float>(cosine1*prim.m_radius+prim.m_pos.x),
            innerX+prim.m_pos.x,
            static_cast<float>(static_cast<double>(cosine2)*inner+prim.m_pos.x),
            static_cast<float>(static_cast<double>(cosine2)*prim.m_radius+prim.m_pos.x)
        };
        const float y[6] = {
            prim.m_pos.y-innerY,
            static_cast<float>(prim.m_pos.y-static_cast<double>(sine2)*prim.m_radius),
            static_cast<float>(prim.m_pos.y-static_cast<double>(sine1)*prim.m_radius),
            prim.m_pos.y-innerY,
            static_cast<float>(prim.m_pos.y-static_cast<double>(sine2)*inner),
            static_cast<float>(prim.m_pos.y-static_cast<double>(sine2)*prim.m_radius)
        };
        const float nextU = u+0.25f;
        const float uv[6][2] = {{u,1},{nextU,0},{u,0},{u,1},{nextU,1},{nextU,0}};
        for (int triangle = 0; triangle < 2; ++triangle) {
            RPFace* face = g_renderer.BorrowNullRP();
            if (!face) return;
            face->primType = D3DPT_TRIANGLELIST; face->numVerts = 3;
            face->verts = face->m_verts; face->indices = nullptr; face->numIndices = 0;
            face->tex = texture; face->mtPreset = 0; face->cullMode = D3DCULL_NONE;
            face->srcAlphaMode = D3DBLEND_SRCALPHA; face->destAlphaMode = D3DBLEND_INVSRCALPHA;
            for (int i = 0; i < 3; ++i) {
                const int j = triangle*3+i;
                auto& vertex = face->m_verts[i];
                vertex.x = x[j]; vertex.y = y[j]; vertex.z = 0.000001f; vertex.oow = 0.99999899f;
                vertex.color = prim.m_argb; vertex.specular = 0xFF000000u;
                vertex.tu = uv[j][0]; vertex.tv = uv[j][1];
            }
            g_renderer.AddReferenceEffectTriangle(face,1);
        }
        u = nextU >= 1.0f ? 0.0f : nextU;
        angle = static_cast<float>(next);
        if (next > stop) break;
    }
}

// Ref/RagEffectPrim.cpp:4650-4958 (0x6489E0), original sphere mesh.
void RenderReferenceSphere(const CEffectPrim& prim, const matrix& view)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    if (!texture || texture == &CTexMgr::s_dummy_texture) return;
    if (!(prim.m_arcAngle > 0.0f && prim.m_arcAngle <= 180.0f)) return;
    const float incU = static_cast<float>(1.0/(360.0/prim.m_arcAngle));
    const float incV = static_cast<float>(1.0/(180.0/prim.m_arcAngle));
    const float stopYaw = 360.0f-prim.m_arcAngle, stopPitch = 90.0f-prim.m_arcAngle;
    float pitch = -90.0f, v = 0.0f;
    for (;;) {
        float yaw = 0.0f, u = 0.0f;
        const float nextV = v+incV;
        for (;;) {
            const int firstYaw = static_cast<int>(static_cast<double>(yaw)+prim.m_longitude);
            const int secondYaw = static_cast<int>(static_cast<double>(yaw)+prim.m_arcAngle+0.25+prim.m_longitude);
            const int firstPitch = static_cast<int>(pitch), secondPitch = static_cast<int>(static_cast<double>(pitch)+prim.m_arcAngle);
            const auto point = [&](int latitude, int longitude, bool last) {
                matrix rotation; MatrixIdentity(rotation);
                MatrixAppendXRotation(rotation,GetRadian(latitude));
                MatrixAppendYRotation(rotation,GetRadian(longitude));
                const double x = static_cast<double>(rotation.m[2][0])*prim.m_radius+rotation.m[3][0];
                const double y = static_cast<double>(rotation.m[2][1])*prim.m_radius+rotation.m[3][1];
                const double z = static_cast<double>(rotation.m[2][2])*prim.m_radius+rotation.m[3][2];
                return vector3d{static_cast<float>(x)+prim.m_pos.x,static_cast<float>(y)+prim.m_pos.y,
                    last ? static_cast<float>(z+prim.m_pos.z) : static_cast<float>(z)+prim.m_pos.z};
            };
            const vector3d a = point(firstPitch,firstYaw,false), b = point(secondPitch,firstYaw,false);
            const vector3d c = point(firstPitch,secondYaw,false), d = point(secondPitch,secondYaw,true);
            const vector3d first[3] = {a,b,c}, second[3] = {d,c,b};
            const float nextU = u+incU;
            const float firstUV[3][2] = {{u,v},{u,nextV},{nextU,v}};
            const float secondUV[3][2] = {{nextU,nextV},{nextU,v},{u,nextV}};
            SubmitReferenceEffectTriangle(first,view,texture,prim.m_argb,firstUV,1);
            SubmitReferenceEffectTriangle(second,view,texture,prim.m_argb,secondUV,1);
            u = nextU;
            const double nextYaw = static_cast<double>(yaw)+prim.m_arcAngle;
            yaw = static_cast<float>(nextYaw);
            if (nextYaw > stopYaw) break;
        }
        v = nextV;
        const double nextPitch = static_cast<double>(pitch)+prim.m_arcAngle;
        pitch = static_cast<float>(nextPitch);
        if (nextPitch > stopPitch) break;
    }
}

void RenderCircleLayer(const CEffectPrim& prim,
    const EffectSegmentState& state,
    matrix& viewMatrix,
    CTexture* texture,
    float arcAngle)
{
    if (!texture) {
        return;
    }

    const float alpha = (std::max)(0.0f, (std::min)(255.0f, state.alpha));
    if (alpha <= 0.0f) {
        return;
    }

    const float outerRadius = state.radius;
    const float innerRadius = state.radius - state.size;
    const int stepDegrees = (std::max)(1, static_cast<int>(arcAngle));
    const unsigned int color = PackColor(static_cast<unsigned int>(alpha), prim.m_tintColor);
    const D3DBLEND destBlend = prim.m_renderFlag == 1u ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE;

    float uInc = 0.0f;
    for (int degrees = 0; degrees <= 360 - stepDegrees; degrees += stepDegrees) {
        const int nextDegrees = degrees + stepDegrees;
        const float cs1 = GetCos(degrees);
        const float sn1 = GetSin(degrees);
        const float cs2 = GetCos(nextDegrees);
        const float sn2 = GetSin(nextDegrees);
        const vector3d quad[4] = {
            vector3d{
                state.pos.x + sn1 * innerRadius * state.transform.m[1][0] + cs1 * innerRadius * state.transform.m[0][0],
                state.pos.y + sn1 * innerRadius * state.transform.m[1][1] + cs1 * innerRadius * state.transform.m[0][1],
                state.pos.z + sn1 * innerRadius * state.transform.m[1][2] + cs1 * innerRadius * state.transform.m[0][2]
            },
            vector3d{
                state.pos.x + sn2 * innerRadius * state.transform.m[1][0] + cs2 * innerRadius * state.transform.m[0][0],
                state.pos.y + sn2 * innerRadius * state.transform.m[1][1] + cs2 * innerRadius * state.transform.m[0][1],
                state.pos.z + sn2 * innerRadius * state.transform.m[1][2] + cs2 * innerRadius * state.transform.m[0][2]
            },
            vector3d{
                state.pos.x + sn1 * outerRadius * state.transform.m[1][0] + cs1 * outerRadius * state.transform.m[0][0],
                state.pos.y + sn1 * outerRadius * state.transform.m[1][1] + cs1 * outerRadius * state.transform.m[0][1],
                state.pos.z + sn1 * outerRadius * state.transform.m[1][2] + cs1 * outerRadius * state.transform.m[0][2]
            },
            vector3d{
                state.pos.x + sn2 * outerRadius * state.transform.m[1][0] + cs2 * outerRadius * state.transform.m[0][0],
                state.pos.y + sn2 * outerRadius * state.transform.m[1][1] + cs2 * outerRadius * state.transform.m[0][1],
                state.pos.z + sn2 * outerRadius * state.transform.m[1][2] + cs2 * outerRadius * state.transform.m[0][2]
            },
        };
        if (prim.m_master->GetEffectType() == kRagEffectPortal && prim.m_stateCnt <= 3 && degrees == 0) {
            tlvertex3d projected[4]{};
            const bool ok0 = ProjectPoint(quad[0], viewMatrix, &projected[0]);
            const bool ok1 = ProjectPoint(quad[1], viewMatrix, &projected[1]);
            const bool ok2 = ProjectPoint(quad[2], viewMatrix, &projected[2]);
            const bool ok3 = ProjectPoint(quad[3], viewMatrix, &projected[3]);
            DbgLog("[RagEffect] circle render effect=%d step=%d proj=%d%d%d%d q0=(%.2f,%.2f,%.2f) q2=(%.2f,%.2f,%.2f)\n",
                prim.m_master->GetEffectType(),
                prim.m_stateCnt,
                ok0 ? 1 : 0,
                ok1 ? 1 : 0,
                ok2 ? 1 : 0,
                ok3 ? 1 : 0,
                quad[0].x,
                quad[0].y,
                quad[0].z,
                quad[2].x,
                quad[2].y,
                quad[2].z);
        }
        const float u0 = uInc;
        const float u1 = uInc + 0.25f;
        const unsigned int colors[4] = { color, color, color, color };
        const float uvs[4][2] = { { u0, 1.0f }, { u1, 1.0f }, { u0, 0.0f }, { u1, 0.0f } };
        SubmitWorldQuad(quad, viewMatrix, texture, colors, uvs, destBlend, 0.0f, static_cast<int>(prim.m_renderFlag));
        uInc = u1 >= 1.0f ? 0.0f : u1;
    }
}

bool AdvanceTextureMotion(CEffectPrim* prim)
{
    if (!prim) {
        return false;
    }

    if (!prim->m_spriteActName.empty() && !prim->m_spriteSprName.empty() && !prim->m_spriteRepeat) {
        CActRes* actRes = g_resMgr.GetAs<CActRes>(prim->m_spriteActName.c_str());
        if (actRes) {
            int actionIndex = prim->m_spriteAction;
            if (actRes->GetMotionCount(actionIndex) <= 0) {
                actionIndex = 0;
            }

            const int motionCount = actRes->GetMotionCount(actionIndex);
            if (motionCount > 0) {
                const float frameDelay = prim->m_spriteFrameDelay > 0.0f
                    ? prim->m_spriteFrameDelay
                    : (std::max)(1.0f, actRes->GetDelay(actionIndex));
                const int motionAdvance = static_cast<int>(static_cast<float>(prim->m_stateCnt) / frameDelay);
                const int clampedBase = (std::max)(0, (std::min)(prim->m_spriteMotionBase, motionCount - 1));
                if (clampedBase + motionAdvance >= motionCount) {
                    return false;
                }
            }
        }
    }

    const int textureCount = (std::max)(1, static_cast<int>(prim->m_texture.size()));
    if (textureCount <= 1) {
        prim->m_curMotion = 0;
        return true;
    }

    const int animSpeed = (std::max)(1, prim->m_animSpeed);
    if ((prim->m_stateCnt % animSpeed) != 0) {
        return true;
    }

    ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= textureCount;
    } else if (prim->m_curMotion >= textureCount) {
        return false;
    }

    return true;
}

void UpdateAlphaFade(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    if (!prim->m_isDisappear && prim->m_master) {
        const int masterRemaining = prim->m_master->GetDuration() - prim->m_master->GetStateCount();
        const int primRemaining = prim->m_duration - prim->m_stateCnt;
        if (masterRemaining > 0 && primRemaining > masterRemaining + 1) {
            prim->m_pattern &= ~0x80;
            prim->m_isDisappear = true;
            prim->m_alphaSpeed = -prim->m_alpha / static_cast<float>(masterRemaining);
        }
    }

    if (prim->m_fadeOutCnt > 0 && prim->m_stateCnt == prim->m_fadeOutCnt) {
        const int fadeFrames = (std::max)(1, prim->m_duration - prim->m_stateCnt);
        prim->m_pattern &= ~0x80;
        prim->m_alphaSpeed = -(prim->m_alpha / static_cast<float>(fadeFrames) * prim->m_alphaDelta);
    }

    prim->m_alpha += prim->m_alphaSpeed;
    if (prim->m_alpha < prim->m_minAlpha) {
        prim->m_alpha = prim->m_minAlpha;
    } else if (!prim->m_isDisappear && prim->m_alphaSpeed >= 0.0f && prim->m_alpha > prim->m_maxAlpha) {
        prim->m_alpha = prim->m_maxAlpha;
    }
}

bool UpdateScreenTexturePrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    prim->m_speed += prim->m_accel;
    prim->m_rollSpeed += prim->m_rollAccel;
    prim->m_roll = WrapAngle360(prim->m_roll + prim->m_rollSpeed);
    prim->m_size += prim->m_sizeSpeed;
    prim->m_sizeSpeed += prim->m_sizeAccel;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_heightSize += prim->m_heightSpeed;
    prim->m_heightSpeed += prim->m_heightAccel;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_gravSpeed += prim->m_gravAccel;

    if (prim->m_speed != 0.0f) {
        const float radians = prim->m_longitude * (kPi / 180.0f);
        prim->m_deltaPos2.x += std::sin(radians) * prim->m_speed;
        prim->m_deltaPos2.y -= std::cos(radians) * prim->m_speed;
    }

    UpdateAlphaFade(prim);
    if (!AdvanceTextureMotion(prim)) {
        return false;
    }
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateCrossTexturePrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    prim->m_speed += prim->m_accel;
    prim->m_size += prim->m_sizeSpeed;
    prim->m_sizeSpeed += prim->m_sizeAccel;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_heightSize += prim->m_heightSpeed;
    prim->m_heightSpeed += prim->m_heightAccel;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_gravSpeed += prim->m_gravAccel;

    if (prim->m_speed != 0.0f) {
        prim->m_deltaPos.x += prim->m_matrix.m[2][0] * prim->m_speed;
        prim->m_deltaPos.y += prim->m_matrix.m[2][1] * prim->m_speed;
        prim->m_deltaPos.z += prim->m_matrix.m[2][2] * prim->m_speed;
    }
    if (prim->m_master != nullptr && (prim->m_pattern & 0x10) != 0) {
        prim->m_master->AbsorbPrimDelta(AddVec3(prim->m_deltaPos, prim->m_deltaPos2));
    }
    if (prim->m_master != nullptr && (prim->m_pattern & 4) != 0) {
        const vector3d world = AddVec3(prim->m_master->ActorPosition(), AddVec3(prim->m_deltaPos2, prim->m_deltaPos));
        const float ground = g_world.m_attr != nullptr ? g_world.m_attr->GetHeight(world.x, world.z) : world.y;
        if (ground < world.y) {
            prim->m_speed = 0.0f;
            prim->m_stateCnt = prim->m_duration - 25;
            prim->m_pattern &= ~4;
            prim->m_master->SetGroundHitFlag();
        }
    }

    UpdateAlphaFade(prim);
    if (!AdvanceTextureMotion(prim)) {
        return false;
    }
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateCirclePrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    prim->m_orgPos = prim->m_master->ResolveBasePosition();
    prim->m_orgPos.x += prim->m_deltaPos2.x;
    prim->m_orgPos.z += prim->m_deltaPos2.z;
    prim->m_orgPos.y = ResolveGroundHeight(prim->m_orgPos) + prim->m_deltaPos2.y;
    prim->m_speed += prim->m_accel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_innerSpeed += prim->m_innerAccel;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_innerSize += prim->m_innerSpeed;

    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, prim->m_latitude * (kPi / 180.0f));
    MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));

    prim->m_speed3d = {
        prim->m_speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
        prim->m_speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
        prim->m_speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_size = ((prim->m_pattern & 1) != 0 || prim->m_radius < prim->m_innerSize) ? prim->m_radius : prim->m_innerSize;

    if ((prim->m_master->GetEffectType() == kRagEffectPortal || prim->m_master->GetEffectType() == kRagEffectReadyPortal)
        && prim->m_stateCnt <= 3) {
        DbgLog("[RagEffect] circle update effect=%d step=%d org=(%.2f,%.2f,%.2f) pos=(%.2f,%.2f,%.2f) radius=%.2f inner=%.2f size=%.2f alpha=%.2f tex=%p\n",
            prim->m_master->GetEffectType(),
            prim->m_stateCnt,
            prim->m_orgPos.x,
            prim->m_orgPos.y,
            prim->m_orgPos.z,
            prim->m_pos.x,
            prim->m_pos.y,
            prim->m_pos.z,
            prim->m_radius,
            prim->m_innerSize,
            prim->m_size,
            prim->m_alpha,
            prim->m_texture.empty() ? nullptr : prim->m_texture[0]);
    }

    UpdateAlphaFade(prim);
    if (prim->m_numSegments > 0) {
        ShiftCircleSegments(prim);
    }
    if (!AdvanceTextureMotion(prim)) {
        return false;
    }
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateCylinderPrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    prim->m_orgPos = prim->m_master->ResolveBasePosition();
    prim->m_orgPos.x += prim->m_deltaPos2.x;
    prim->m_orgPos.z += prim->m_deltaPos2.z;
    prim->m_orgPos.y = ResolveGroundHeight(prim->m_orgPos) + prim->m_deltaPos2.y;
    prim->m_speed += prim->m_accel;
    prim->m_outerSpeed += prim->m_outerAccel;
    prim->m_innerSpeed += prim->m_innerAccel;
    prim->m_heightSpeed += prim->m_heightAccel;

    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, prim->m_latitude * (kPi / 180.0f));
    MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));

    const float move = -prim->m_speed;
    prim->m_speed3d = {
        move * prim->m_matrix.m[1][0] + prim->m_matrix.m[3][0],
        move * prim->m_matrix.m[1][1] + prim->m_matrix.m[3][1],
        move * prim->m_matrix.m[1][2] + prim->m_matrix.m[3][2]
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_longitude += prim->m_longSpeed;
    prim->m_outerSize += prim->m_outerSpeed;
    prim->m_innerSize += prim->m_innerSpeed;
    prim->m_heightSize += prim->m_heightSpeed;

    if (prim->m_master->GetEffectType() == kRagEffectPortal && prim->m_stateCnt <= 3) {
        DbgLog("[RagEffect] cylinder update effect=%d step=%d org=(%.2f,%.2f,%.2f) pos=(%.2f,%.2f,%.2f) inner=%.2f outer=%.2f height=%.2f long=%.2f alpha=%.2f tex=%p\n",
            prim->m_master->GetEffectType(),
            prim->m_stateCnt,
            prim->m_orgPos.x,
            prim->m_orgPos.y,
            prim->m_orgPos.z,
            prim->m_pos.x,
            prim->m_pos.y,
            prim->m_pos.z,
            prim->m_innerSize,
            prim->m_outerSize,
            prim->m_heightSize,
            prim->m_longitude,
            prim->m_alpha,
            prim->m_texture.empty() ? nullptr : prim->m_texture[0]);
    }

    UpdateAlphaFade(prim);
    if (!AdvanceTextureMotion(prim)) {
        return false;
    }
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateParticleOrbitPrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    prim->m_orgPos = prim->m_master->ResolveBasePosition();
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_gravSpeed += prim->m_gravAccel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_deltaPos2.y += prim->m_gravSpeed;
    prim->m_orgPos = AddVec3(prim->m_orgPos, prim->m_deltaPos2);

    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));

    prim->m_deltaPos = {
        prim->m_radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
        prim->m_radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
        prim->m_radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
    };
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_size += prim->m_sizeSpeed;

    UpdateAlphaFade(prim);
    if (!AdvanceTextureMotion(prim)) {
        return false;
    }
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateFreeParticlePrimitive(CEffectPrim* prim, bool applyGravity, bool splineMotion)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    prim->m_orgPos = prim->m_master->ResolveBasePosition();
    prim->m_orgPos = AddVec3(prim->m_orgPos, prim->m_deltaPos2);

    prim->m_speed += prim->m_accel;
    prim->m_rollSpeed += prim->m_rollAccel;
    prim->m_roll = WrapAngle360(prim->m_roll + prim->m_rollSpeed);
    prim->m_size += prim->m_sizeSpeed;
    prim->m_sizeSpeed += prim->m_sizeAccel;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_longSpeed += prim->m_longAccel;

    if (applyGravity) {
        prim->m_gravSpeed += prim->m_gravAccel;
        prim->m_deltaPos.y += prim->m_gravSpeed;
    }

    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, prim->m_latitude * (kPi / 180.0f));
    MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));
    prim->m_speed3d = {
        prim->m_speed * prim->m_matrix.m[2][0],
        prim->m_speed * prim->m_matrix.m[2][1],
        prim->m_speed * prim->m_matrix.m[2][2]
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);

    if (splineMotion) {
        const float timeSeconds = static_cast<float>(prim->m_stateCnt) * (kEffectTickMs / 1000.0f);
        const float lateral = std::sin(timeSeconds * 4.0f + prim->m_param[0]) * (std::max)(0.0f, prim->m_param[1]);
        prim->m_deltaPos.x += prim->m_matrix.m[0][0] * lateral;
        prim->m_deltaPos.z += prim->m_matrix.m[0][2] * lateral;
    }

    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    UpdateAlphaFade(prim);
    if (!AdvanceTextureMotion(prim)) {
        return false;
    }
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

} // namespace

CEffectPrim::CEffectPrim()
    : m_master(nullptr)
    , m_type(PP_3DCIRCLE)
    , m_renderFlag(1u)
    , m_stateCnt(0)
    , m_duration(1)
    , m_fadeOutCnt(0)
    , m_pattern(0)
    , m_totalTexture(1)
    , m_spawnCount(12)
    , m_alpha(0.0f)
    , m_alphaSpeed(0.0f)
    , m_maxAlpha(255.0f)
    , m_alphaDelta(1.0f)
    , m_minAlpha(0.0f)
    , m_widthSize(0.0f)
    , m_widthSpeed(0.0f)
    , m_widthAccel(0.0f)
    , m_size(1.0f)
    , m_sizeSpeed(0.0f)
    , m_sizeAccel(0.0f)
    , m_radius(0.0f)
    , m_radiusSpeed(0.0f)
    , m_radiusAccel(0.0f)
    , m_innerSize(0.0f)
    , m_outerSize(0.0f)
    , m_heightSize(0.0f)
    , m_maxHeightSize(100.0f)
    , m_argb(0xFFFFFFFFu)
    , m_heightSpeed(0.0f)
    , m_heightAccel(0.0f)
    , m_longitude(0.0f)
    , m_longSpeed(0.0f)
    , m_longAccel(0.0f)
    , m_speed(0.0f)
    , m_accel(0.0f)
    , m_gravSpeed(0.0f)
    , m_gravAccel(0.0f)
    , m_emitSpeed(1.0f)
    , m_arcAngle(36.0f)
    , m_latitude(0.0f)
    , m_latiSpeed(0.0f)
    , m_outerSpeed(0.0f)
    , m_outerAccel(0.0f)
    , m_innerSpeed(0.0f)
    , m_innerAccel(0.0f)
    , m_roty(0.0f)
    , m_roll(0.0f)
    , m_rollSpeed(0.0f)
    , m_rollAccel(0.0f)
    , m_tintColor(RGB(255, 255, 255))
    , m_curMotion(0)
    , m_animSpeed(4)
    , m_spriteAction(0)
    , m_spriteMotionBase(0)
    , m_spriteFrameDelay(0.0f)
    , m_spriteScale(1.0f)
    , m_particleSprRes(nullptr)
    , m_particleActRes(nullptr)
    , m_spriteRepeat(false)
    , m_repeatAnim(true)
    , m_isDisappear(false)
    , m_bandController(0)
    , m_numSegments(0)
{
    m_param.fill(0.0f);
    m_deltaPos = vector3d{};
    m_deltaPos2 = vector3d{};
    m_deltaPos3 = vector3d{};
    m_deltaPosAccel = vector3d{};
    m_orgPos = vector3d{};
    m_pos = vector3d{};
    m_speed3d = vector3d{};
    MatrixIdentity(m_matrix);
    MatrixIdentity(m_rotMat);
    for (EffectSegmentState& segment : m_segments) {
        MatrixIdentity(segment.transform);
    }
}

void CEffectPrim::Init(CRagEffect* master, EFFECTPRIMID effectPrimId, const vector3d& deltaPos)
{
    m_master = master;
    m_deltaPos = AddVec3(m_deltaPos, deltaPos);
    m_type = effectPrimId;
    m_orgPos = master->EffectPosition();
    m_pos = m_orgPos;
}

static_assert(offsetof(EffectGi, height) == 20, "GI height offset");
static_assert(offsetof(EffectGi, flag) == 104, "GI flag offset");
static_assert(offsetof(EffectGi, distance) == 128, "GI distance offset");
static_assert(offsetof(EffectGi, sparkX) == 132, "GI spark position offset");
static_assert(offsetof(EffectGi, arcBaselineY) == 160, "GI arc baseline offset");
static_assert(offsetof(EffectGi, vecTNowX) == 144, "GI vecT_now offset");
static_assert(offsetof(EffectGi, vecTPreX) == 168, "GI vecT_pre offset");
static_assert(sizeof(EffectGi) == 180, "GI stride");

float BitsToFloat(u32 bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

u32 FloatBits(float value)
{
    u32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

int TableAngle(int angle);

void UpdateCastingHeights(EffectGi& gi)
{
    for (int sample = 0; sample < 21; ++sample) {
        if (gi.flag[static_cast<size_t>(sample)] != 0) {
            continue;
        }
        const float sineScale = g_sinTable[9 * sample];
        if (gi.process <= 90) {
            gi.height[static_cast<size_t>(sample)] = g_sinTable[gi.process] * gi.maxHeight * sineScale;
        }
        const float cap = gi.maxHeight * sineScale;
        if (cap < gi.height[static_cast<size_t>(sample)]) {
            gi.height[static_cast<size_t>(sample)] = cap;
            gi.flag[static_cast<size_t>(sample)] = 1;
        }
        if (gi.height[static_cast<size_t>(sample)] < 0.0f) {
            gi.height[static_cast<size_t>(sample)] = 0.0f;
        }
    }
}

void SpinCastingBand(EffectGi& gi, int band)
{
    if (band < 3) {
        gi.rotStart = static_cast<short>(gi.rotStart + band + 3);
    }
    if (gi.rotStart >= 360) {
        gi.rotStart = static_cast<short>(gi.rotStart - 360);
    }
}

void Update3DCastingPrimitive(CEffectPrim* prim)
{
    const int firstBand = prim->m_size == 3.0f ? 3 : 0;
    for (int band = firstBand; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        SpinCastingBand(gi, band);
        if (gi.process < prim->m_duration - 40) {
            if (gi.process < 20 && gi.alphaT != 1) {
                if (band == 3) {
                    gi.alphaB = static_cast<short>(gi.alphaB + 8);
                    if (gi.alphaB > 60) {
                        gi.alphaB = 60;
                    }
                } else {
                    gi.alphaB = static_cast<short>(gi.alphaB + 10);
                    if (gi.alphaB > 180) {
                        gi.alphaB = 180;
                    }
                }
            }
        } else if (band >= 3) {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 5);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        UpdateCastingHeights(gi);
    }
}

void Update3DCasting3Primitive(CEffectPrim* prim)
{
    const int firstBand = prim->m_size == 3.0f ? 3 : 0;
    for (int band = firstBand; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        SpinCastingBand(gi, band);
        const bool early = gi.process < prim->m_duration - 40;
        if (early && gi.process < 20 && band == 3) {
            gi.alphaB = static_cast<short>(gi.alphaB + 8);
            if (gi.alphaB > 100) {
                gi.alphaB = 100;
            }
        } else if (early && gi.process < 20) {
            gi.alphaB = static_cast<short>(gi.alphaB + 10);
            if (gi.alphaB > 180) {
                gi.alphaB = 180;
            }
        } else if (!early && band >= 3) {
            gi.alphaB = static_cast<short>(gi.alphaB - 3);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        } else if (!early) {
            gi.alphaB = static_cast<short>(gi.alphaB - 5);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        if (band < 3) {
            UpdateCastingHeights(gi);
            continue;
        }
        for (int sample = 0; sample < 21; ++sample) {
            const float sineScale = g_sinTable[9 * sample];
            if (gi.flag[static_cast<size_t>(sample)] != 0) {
                const float base = gi.maxHeight * sineScale * 0.55000001f;
                gi.height[static_cast<size_t>(sample)] = base
                    + g_sinTable[TableAngle(gi.process)] * gi.maxHeight * sineScale * 0.44999999f;
                continue;
            }
            if (gi.process > 90) {
                gi.flag[static_cast<size_t>(sample)] = 1;
            } else {
                gi.height[static_cast<size_t>(sample)] = g_sinTable[gi.process] * gi.maxHeight * sineScale;
            }
            if (gi.maxHeight * sineScale < gi.height[static_cast<size_t>(sample)]) {
                gi.flag[static_cast<size_t>(sample)] = 1;
            }
            if (gi.height[static_cast<size_t>(sample)] < 0.0f) {
                gi.height[static_cast<size_t>(sample)] = 0.0f;
            }
        }
    }
}

void UpdateGi5Primitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.alphaT = static_cast<short>(gi.alphaT + 1);
        if (gi.alphaT >= 360) {
            gi.alphaT = static_cast<short>(gi.alphaT - 360);
        }
        gi.distance = g_sinTable[TableAngle(gi.alphaT)] + 3.0f;
        gi.rotStart = static_cast<short>(gi.rotStart + 3);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        const int bandBase = band * 90;
        const int phase = bandBase >= 180 ? 2 * (gi.process + band * 45) : gi.process + bandBase;
        const float wave = g_sinTable[TableAngle(phase)];
        for (int sample = 0; sample < 21; ++sample) {
            const float scale = g_sinTable[9 * sample];
            gi.height[static_cast<size_t>(sample)] = gi.maxHeight * scale
                + wave * gi.maxHeight * scale * 0.30000001f;
        }
    }
}

void UpdateGi1Primitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        gi.distance += 0.07f;
        gi.riseAngle = static_cast<short>(gi.riseAngle - 1);
        if (gi.riseAngle < 10) {
            gi.riseAngle = 10;
            gi.alphaB = 0;
        }
        if (gi.distance < 4.0f) {
            gi.alphaB = static_cast<short>(gi.alphaB + (gi.alphaT == 22 ? 5 : 10));
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 3);
            if (gi.alphaB <= 0) {
                gi.alphaB = 0;
                if (gi.process < prim->m_duration - 30) {
                    gi.distance = BitsToFloat(1079488020u);
                    gi.riseAngle = gi.alphaT == 22 ? 64 : 74;
                }
            }
        }
        const int bandBase = band * 90;
        const int phase = bandBase >= 180 ? 2 * (gi.process + band * 45) : gi.process + bandBase;
        const float wave = g_sinTable[TableAngle(phase)];
        for (int sample = 0; sample < 21; ++sample) {
            gi.height[static_cast<size_t>(sample)] = gi.maxHeight
                + wave * g_sinTable[9 * sample] * gi.maxHeight * 0.30000001f;
        }
    }
}

void UpdateHealPrimitive(CEffectPrim* prim)
{
    if (prim->m_gi[0].flag[0] == 33) {
        int longitude = 0;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_view != nullptr) {
                longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            }
        }
        prim->m_gi[2].alphaT = static_cast<short>(360 - longitude);
    }
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }

    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        const u8 mode = gi.flag[0];
        if (mode == 1) {
            gi.rotStart = static_cast<short>(gi.rotStart + band + 8);
        } else if (mode == 2) {
            gi.rotStart = static_cast<short>(gi.rotStart + (band >= 2 ? band + 2 : band + 4));
        } else if (mode == 3) {
            gi.rotStart = static_cast<short>(gi.rotStart + band + 6);
        }
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }

        if (gi.process < gi.alphaT || gi.alphaT < 0) {
            if (gi.process < 16) {
                if (mode == 2) {
                    gi.alphaB = static_cast<short>(gi.alphaB + 3);
                } else {
                    gi.alphaB = static_cast<short>(gi.alphaB + (mode == 3 ? 2 : 5));
                }
                if (gi.alphaB > 180) {
                    gi.alphaB = 180;
                }
            }
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }

        int waveBase = 0;
        for (int sample = 0; sample < 21; ++sample) {
            const u8 sampleMode = gi.flag[static_cast<size_t>(sample)];
            float height = gi.maxHeight;
            if (sampleMode == 0 || sampleMode == 33) {
                int wave = waveBase + 2 * gi.process;
                if (band == 0) {
                    wave = waveBase + 4 * gi.process;
                } else if (band == 1) {
                    wave = waveBase + 3 * prim->m_gi[1].process;
                }
                const int wrapped = wave % 360;
                const int angle = wrapped < 0 ? wrapped + 360 : wrapped;
                height = g_sinTable[angle] * gi.maxHeight * 0.25f + gi.maxHeight * 0.75f;
                if (gi.process <= 90) {
                    height *= g_sinTable[gi.process];
                }
                if (height < 0.0f) {
                    height = 0.0f;
                }
            } else if (sampleMode == 1 || sampleMode == 3) {
                if (gi.process <= 90) {
                    height *= g_sinTable[gi.process];
                }
            } else if (sampleMode == 2) {
                if (gi.process > 45) {
                    height = 0.0f;
                } else {
                    height *= g_sinTable[4 * gi.process];
                }
            } else {
                waveBase += 34;
                continue;
            }
            gi.height[static_cast<size_t>(sample)] = height;
            waveBase += 34;
        }
    }
}

unsigned int CastingSizeRgb(float size)
{
    switch (static_cast<unsigned int>(static_cast<long long>(size))) {
    case 2u:
        return 0x00C3C3FFu;
    case 1u:
        return 0x00FFAFAFu;
    case 4u:
        return 0x006464FFu;
    case 6u:
        return 0x00FF6464u;
    case 7u:
        return 0x0064FF64u;
    case 8u:
        return 0x00FFFFAAu;
    case 9u:
        return 0x008CD28Cu;
    case 10u:
        return 0x00FF59B6u;
    case 11u:
        return 0x00AAAAFFu;
    case 12u:
        return 0x00323232u;
    case 13u:
        return 0x00FF32FFu;
    case 14u:
        return 0x007878FFu;
    case 15u:
        return 0x000AFF0Au;
    case 100u:
        return 0x00FAFAFFu;
    default:
        return 0x00FFFFFFu;
    }
}

void CastingLowPolygonRise(int riseAngle, float height, double* radialExtended, float* radialStored, float* vertical)
{
    const int tableIndex = riseAngle * 4;
    *radialExtended = static_cast<double>(g_sinTable[tableIndex]) * height;
    *radialStored = static_cast<float>(*radialExtended);
    *vertical = static_cast<float>(-static_cast<double>(g_cosTable[tableIndex]) * height);
}

void CastingGi3Sample(int band, int angle, int riseAngle, float height, float distance, vector3d* inner, vector3d* outer)
{
    const int wrappedAngle = ((angle % 360) + 360) % 360;
    inner->x = static_cast<float>(static_cast<double>(g_cosTable[wrappedAngle]) * distance);
    inner->y = 0.0f;
    inner->z = static_cast<float>(static_cast<double>(g_sinTable[wrappedAngle]) * distance);
    const double radial = static_cast<double>(g_cosTable[riseAngle]) * height;
    const float vertical = static_cast<float>(static_cast<double>(g_sinTable[riseAngle]) * height);
    const int rotationBase[] = { 450, 270, 90, 630 };
    const int side = ((rotationBase[band] - wrappedAngle) % 360 + 360) % 360;
    outer->x = static_cast<float>(static_cast<double>(g_cosTable[side]) * radial + inner->x);
    outer->y = -vertical;
    outer->z = static_cast<float>(static_cast<double>(g_sinTable[side]) * radial + inner->z);
}

unsigned int CastingSizeAlpha(float size, unsigned int alpha)
{
    return size == 3.0f ? 80u : alpha;
}

D3DBLEND CastingSizeBlend(float size)
{
    switch (static_cast<unsigned int>(static_cast<long long>(size))) {
    case 0u:
    case 2u:
    case 3u:
    case 12u:
    case 13u:
    case 14u:
    case 100u:
        return D3DBLEND_INVSRCALPHA;
    default:
        return D3DBLEND_ONE;
    }
}

void RenderHealLow(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const unsigned int rgb = CastingSizeRgb(prim.m_size);
    const D3DBLEND blend = CastingSizeBlend(prim.m_size);
    const int spin = TableAngle(prim.m_gi[2].alphaT);
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.fullDisplayAngle < 0) {
            continue;
        }
        const int basicAngle = gi.fullDisplayAngle / 10;
        int count = 0;
        int sample = 0;
        bool havePrevious = false;
        vector3d previousInner{};
        vector3d previousOuter{};
        bool keepGoing = true;
        while (keepGoing) {
            int angle = (count + gi.rotStart) % 360;
            if (gi.fullDisplayAngle == 360 && sample == 10) {
                angle = gi.rotStart;
            }
            const float cosine = g_cosTable[angle];
            const float sine = g_sinTable[angle];
            vector3d inner{ gi.distance * cosine, 0.0f, gi.distance * sine };
            double radialExtended = 0.0;
            float radialStored = 0.0f, vertical = 0.0f;
            CastingLowPolygonRise(gi.riseAngle, gi.height[static_cast<size_t>(sample)], &radialExtended, &radialStored, &vertical);
            vector3d outer{
                static_cast<float>(radialExtended * cosine + inner.x),
                vertical,
                radialStored * sine + inner.z
            };
            if (gi.sparkY == 1234.5f) {
                const float offsetX = gi.sparkX * g_cosTable[spin] - gi.sparkZ * g_sinTable[spin];
                const float offsetZ = gi.sparkX * g_sinTable[spin] + gi.sparkZ * g_cosTable[spin];
                inner.x += offsetX;
                inner.z += offsetZ;
                outer.x += offsetX;
                outer.z += offsetZ;
            }
            if (havePrevious) {
                const int part = sample - 1;
                const float u0 = static_cast<float>(static_cast<double>(part) * 0.047619049);
                const float u1 = static_cast<float>(static_cast<double>(part + 1) * 0.047619049);
                const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
                const vector3d previousInnerWorld{ prim.m_pos.x + previousInner.x, prim.m_pos.y + previousInner.y, prim.m_pos.z + previousInner.z };
                const vector3d innerWorld{ prim.m_pos.x + inner.x, prim.m_pos.y + inner.y, prim.m_pos.z + inner.z };
                const vector3d outerWorld{ prim.m_pos.x + outer.x, prim.m_pos.y + outer.y, prim.m_pos.z + outer.z };
                const vector3d previousOuterWorld{ prim.m_pos.x + previousOuter.x, prim.m_pos.y + previousOuter.y, prim.m_pos.z + previousOuter.z };
                SubmitWorldTeiRect(previousInnerWorld, innerWorld, outerWorld, previousOuterWorld, viewMatrix, texture, color, u0, u1, 1.0f, 0.0f, blend, 0.0f, static_cast<int>(prim.m_renderFlag));
            }
            if (++sample > 10) {
                sample = 10;
            }
            previousInner = inner;
            previousOuter = outer;
            havePrevious = true;
            keepGoing = basicAngle + count <= gi.fullDisplayAngle;
            count += basicAngle;
        }
    }
}

void RenderGi3(CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const unsigned int rgb = CastingSizeRgb(prim.m_size);
    const D3DBLEND blend = CastingSizeBlend(prim.m_size);
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.fullDisplayAngle <= 0) {
            continue;
        }
        const int basicAngle = gi.fullDisplayAngle / 9;
        int count = 0;
        int sample = 0;
        bool havePrevious = false;
        while (count < gi.fullDisplayAngle) {
            int angle = (gi.rotStart + count) % 360;
            if (gi.fullDisplayAngle == 360 && sample == 20) {
                angle = gi.rotStart;
            }
            vector3d inner{}, outer{};
            CastingGi3Sample(band, angle, gi.riseAngle, gi.height[static_cast<size_t>(sample)], gi.distance, &inner, &outer);
            gi.sparkX = inner.x;
            gi.sparkY = inner.y;
            gi.sparkZ = inner.z;
            gi.vecTNowX = outer.x;
            gi.vecTNowY = outer.y;
            gi.vecTNowZ = outer.z;
            if (havePrevious) {
                const int part = sample - 1;
                const float u0 = static_cast<float>(static_cast<double>(part) * 0.047619049);
                const float u1 = static_cast<float>(static_cast<double>(part + 1) * 0.047619049);
                const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
                const vector3d previousInnerWorld{ prim.m_pos.x + gi.vecBPreX, prim.m_pos.y + gi.arcBaselineY, prim.m_pos.z + gi.vecBPreZ };
                const vector3d innerWorld{ prim.m_pos.x + inner.x, prim.m_pos.y + inner.y, prim.m_pos.z + inner.z };
                const vector3d outerWorld{ prim.m_pos.x + outer.x, prim.m_pos.y + outer.y, prim.m_pos.z + outer.z };
                const vector3d previousOuterWorld{ prim.m_pos.x + gi.vecTPreX, prim.m_pos.y + gi.vecTPreY, prim.m_pos.z + gi.vecTPreZ };
                SubmitWorldTeiRect(previousInnerWorld, innerWorld, outerWorld, previousOuterWorld, viewMatrix, texture, color, u0, u1, 1.0f, 0.0f, blend, 0.0f, static_cast<int>(prim.m_renderFlag));
            }
            gi.vecBPreX = gi.sparkX;
            gi.arcBaselineY = gi.sparkY;
            gi.vecBPreZ = gi.sparkZ;
            gi.vecTPreX = gi.vecTNowX;
            gi.vecTPreY = gi.vecTNowY;
            gi.vecTPreZ = gi.vecTNowZ;
            if (++sample >= 21) {
                sample = 20;
            }
            havePrevious = true;
            count += basicAngle;
        }
    }
}

void RenderHealCasting(const CEffectPrim& prim, const matrix& viewMatrix)
{
        for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.fullDisplayAngle < 0) {
            continue;
        }

        const int basicAngle = gi.fullDisplayAngle / 20;
        int count = 0;
        int sample = 0;
        bool havePrevious = false;
        vector3d previousBase{};
        vector3d previousTop{};
        do {
            int angle = (count + gi.rotStart) % 360;
            if (gi.fullDisplayAngle == 360 && sample == 20) {
                angle = gi.rotStart;
            }
            const float cosine = g_cosTable[angle];
            const float sine = g_sinTable[angle];
            const u32 heightBits = FloatBits(gi.maxHeight);
            float baseY = 0.0f;
            float topLift = 0.0f;
            if (heightBits == 1086326833u) {
                baseY = -2.0f;
                topLift = 2.0f;
            } else if (heightBits == 1103102476u) {
                baseY = -1.0f;
                topLift = 1.0f;
            }
            const int riseAngle = gi.riseAngle;
            const float sampleHeight = gi.height[static_cast<size_t>((std::min)(sample, 20))];
            // Ref uses x87 temporaries for these products, then stores each local
            // vertex and its later parent translation back to float separately.
            const double baseLocalXWide = static_cast<double>(cosine) * gi.distance;
            const double baseLocalZWide = static_cast<double>(sine) * gi.distance;
            const float baseLocalX = static_cast<float>(baseLocalXWide);
            const float baseLocalZ = static_cast<float>(baseLocalZWide);
            const double radial = static_cast<double>(g_cosTable[riseAngle]) * sampleHeight;
            const float vertical = static_cast<float>(static_cast<double>(g_sinTable[riseAngle]) * sampleHeight);
            const float topLocalX = static_cast<float>(radial * cosine + baseLocalX);
            const float topLocalY = static_cast<float>(-(static_cast<double>(vertical) + topLift));
            const float topLocalZ = static_cast<float>(radial * sine + baseLocalZ);
            const vector3d base = {
                static_cast<float>(static_cast<double>(prim.m_pos.x) + baseLocalX),
                static_cast<float>(static_cast<double>(prim.m_pos.y) + baseY),
                static_cast<float>(static_cast<double>(prim.m_pos.z) + baseLocalZ)
            };
            const vector3d top = {
                static_cast<float>(static_cast<double>(prim.m_pos.x) + topLocalX),
                static_cast<float>(static_cast<double>(prim.m_pos.y) + topLocalY),
                static_cast<float>(static_cast<double>(prim.m_pos.z) + topLocalZ)
            };
            if (sample > 0 && havePrevious) {
                const int texPart = sample - 1;
                float u0 = static_cast<float>(static_cast<double>(texPart) * 0.047619049);
                float u1 = static_cast<float>(static_cast<double>(texPart + 1) * 0.047619049);
                if (texPart == 0) {
                    u0 = 0.0f;
                }
                const unsigned int alpha = CastingSizeAlpha(prim.m_size,
                    static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)));
                const unsigned int color = (alpha << 24) | CastingSizeRgb(prim.m_size);
                SubmitWorldTeiRect(previousBase,
                    base,
                    top,
                    previousTop,
                    viewMatrix,
                    prim.m_texture.empty() ? nullptr : prim.m_texture[0],
                    color,
                    u0,
                    u1,
                    1.0f,
                    0.0f,
                    CastingSizeBlend(prim.m_size),
                    0.0f,
                    static_cast<int>(prim.m_renderFlag));
            }
            previousBase = base;
            previousTop = top;
            havePrevious = true;
            ++sample;
            if (sample >= 21) {
                sample = 20;
            }
            count += basicAngle;
        } while (basicAngle + count <= gi.fullDisplayAngle);
    }
}

int TableAngle(int angle)
{
    angle %= 360;
    if (angle < 0) {
        angle += 360;
    }
    return angle;
}

void RotateCornerTowardScreen(vector3d* corner, int rot0, int rot1)
{
    const float cosLat = g_cosTable[rot1];
    const float sinLat = g_sinTable[rot1];
    const float z = corner->z;
    const float y = corner->y;
    corner->z = z * cosLat - y * sinLat;
    corner->y = z * sinLat + y * cosLat;
    const float cosLon = g_cosTable[rot0];
    const float sinLon = g_sinTable[rot0];
    const float x = corner->x;
    const float z2 = corner->z;
    corner->x = x * cosLon - z2 * sinLon;
    corner->z = x * sinLon + z2 * cosLon;
}

void UpdateGiExplosionPrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(gi.rotStart + 4);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.process <= 30) {
            gi.alphaB = static_cast<short>(gi.alphaB + 8);
            if (gi.alphaB > 160) {
                gi.alphaB = 160;
            }
        }
        gi.distance -= 0.2f;
        for (int sample = 0; sample < 21; ++sample) {
            if (gi.flag[static_cast<size_t>(sample)] != 0) {
                continue;
            }
            const float envelope = g_sinTable[9 * sample];
            if (gi.process <= 90) {
                gi.height[static_cast<size_t>(sample)] = g_sinTable[gi.process] * gi.maxHeight * envelope;
            }
            const float cap = gi.maxHeight * envelope;
            if (cap < gi.height[static_cast<size_t>(sample)]) {
                gi.height[static_cast<size_t>(sample)] = cap;
                gi.flag[static_cast<size_t>(sample)] = 1;
            }
            if (gi.height[static_cast<size_t>(sample)] < 0.0f) {
                gi.height[static_cast<size_t>(sample)] = 0.0f;
            }
        }
    }
}

void UpdateAcidDemonPrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(gi.rotStart + (band == 3 ? rand() % 2 + 2 : rand() % 3 + 4));
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.process < 20) {
            gi.alphaB = static_cast<short>(gi.alphaB + 5);
            if (gi.alphaB > 80) {
                gi.alphaB = 80;
            }
        } else if (gi.process >= 40) {
            gi.alphaB = static_cast<short>(gi.alphaB - 15);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        gi.maxHeight *= band == 3 ? 1.025f : 1.005f;
        for (int sample = 0; sample < 21; ++sample) {
            const float envelope = g_sinTable[9 * sample];
            if (gi.process > 90) {
                gi.flag[static_cast<size_t>(sample)] = 1;
            } else {
                gi.height[static_cast<size_t>(sample)] = g_sinTable[gi.process] * gi.maxHeight * envelope;
            }
            if (gi.maxHeight * envelope < gi.height[static_cast<size_t>(sample)]) {
                gi.flag[static_cast<size_t>(sample)] = 1;
            }
            if (gi.height[static_cast<size_t>(sample)] < 0.0f) {
                gi.height[static_cast<size_t>(sample)] = 0.0f;
            }
        }
    }
}

void UpdateFlowerCastPrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        const int pulse = 4 * (gi.process % 90);
        gi.maxHeight = g_sinTable[pulse] * 2.5f + gi.vecTNowY;
        if (gi.riseAngle >= 360) {
            gi.riseAngle = static_cast<short>(gi.riseAngle - 360);
        } else if (gi.riseAngle < 0) {
            gi.riseAngle = static_cast<short>(gi.riseAngle + 360);
        }
        if (gi.process <= 110) {
            if (gi.process < 20) {
                gi.alphaB = static_cast<short>(gi.alphaB + 5);
                if (gi.alphaB > 180) {
                    gi.alphaB = 180;
                }
            }
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 1);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        for (int sample = 0; sample < 21; ++sample) {
            if (gi.flag[static_cast<size_t>(sample)] != 0) {
                continue;
            }
            const float envelope = g_sinTable[9 * sample];
            float height = envelope * gi.maxHeight;
            if (gi.process <= 90) {
                height *= g_sinTable[gi.process];
            }
            gi.height[static_cast<size_t>(sample)] = height;
            if (envelope * gi.maxHeight < height) {
                gi.height[static_cast<size_t>(sample)] = envelope * gi.maxHeight;
                gi.flag[static_cast<size_t>(sample)] = 1;
            }
            if (gi.height[static_cast<size_t>(sample)] < 0.0f) {
                gi.height[static_cast<size_t>(sample)] = 0.0f;
            }
        }
    }
}

void UpdateDefenderPrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(gi.rotStart + (band == 0 || band == 2 ? 1 : 2));
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.process < prim->m_duration - 20) {
            if (gi.process < 100) {
                gi.alphaB = static_cast<short>(gi.alphaB + 1);
                if (gi.alphaB > 80) {
                    gi.alphaB = 80;
                }
            }
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 5);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        const int phase = band >= 2 ? 2 * (gi.process + (band == 2 ? 90 : 135)) : gi.process + band * 90;
        const int wave = TableAngle(phase);
        for (int sample = 0; sample < 21; ++sample) {
            const float envelope = g_sinTable[9 * sample];
            gi.height[static_cast<size_t>(sample)] = envelope * gi.maxHeight * (1.0f + 0.30000001f * g_sinTable[wave]);
        }
    }
}

void UpdateMapPillarGi(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(gi.rotStart + 1);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.alphaT == 1 && gi.process > 250) {
            gi.alphaB = static_cast<short>(gi.alphaB - 1);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        if (gi.process >= 800) {
            gi.maxHeight -= 1.0f;
            if (gi.maxHeight < 0.0f) {
                gi.maxHeight = 0.0f;
            }
        }
        for (int sample = 0; sample < 21; ++sample) {
            float height = 0.0f;
            if (gi.process >= 200) {
                height = gi.process > 290 ? gi.maxHeight : g_sinTable[gi.process - 200] * gi.maxHeight;
            }
            if (height < 0.0f) {
                height = 0.0f;
            }
            gi.height[static_cast<size_t>(sample)] = height;
        }
    }
}

void UpdateGi2Primitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    const bool fastSpin = prim->m_accel == 99.0f;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(gi.rotStart + (fastSpin ? 3 : 5));
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        gi.distance += fastSpin ? 0.1f : 0.30000001f;
        gi.riseAngle = static_cast<short>(gi.riseAngle - 1);
        const short riseFloor = fastSpin ? 30 : 40;
        if (gi.riseAngle < riseFloor) {
            gi.riseAngle = riseFloor;
        }
        if (gi.process <= 10) {
            gi.alphaB = static_cast<short>(gi.alphaB + (gi.alphaT == 1 ? 10 : 20));
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - (gi.alphaT == 2 ? 8 : 2));
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        const int wave = TableAngle(gi.process + band * 90);
        for (int sample = 0; sample < 21; ++sample) {
            const float envelope = g_sinTable[9 * sample];
            gi.height[static_cast<size_t>(sample)] = envelope * gi.maxHeight * (1.0f + 0.15000001f * g_sinTable[wave]);
        }
    }
}

void UpdateGi4Primitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.distance += 0.039999999f;
        gi.riseAngle = static_cast<short>(gi.riseAngle - 1);
        if (gi.riseAngle < 10) {
            gi.riseAngle = 10;
            gi.alphaB = 0;
        }
        if (gi.distance < 2.5f) {
            gi.alphaB = static_cast<short>(gi.alphaB + 20);
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
            if (gi.alphaB <= 0) {
                gi.alphaB = 0;
                gi.distance = 2.1399999f;
                gi.riseAngle = 74;
            }
        }
        const int wave = TableAngle(band >= 2 ? 2 * (gi.process + (band == 2 ? 90 : 135)) : gi.process + band * 90);
        for (int sample = 0; sample < 21; ++sample) {
            const float envelope = g_sinTable[9 * sample];
            gi.height[static_cast<size_t>(sample)] = gi.maxHeight * (1.0f + 0.30000001f * g_sinTable[wave] * envelope);
        }
    }
}

void UpdateStormKickPrimitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        gi.rotStart = static_cast<short>(gi.rotStart + rand() % 3 + 3);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.process > 60) {
            gi.alphaB = static_cast<short>(gi.alphaB - 1);
        }
        if (gi.process <= 40) {
            gi.alphaB = static_cast<short>(gi.alphaB + 1);
        }
        if (gi.alphaB < 0) {
            gi.alphaB = 0;
        }
    }
}

void RenderStormKick(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        unsigned char red = 0;
        unsigned char green = 0;
        unsigned char blue = 0;
        D3DBLEND blend = D3DBLEND_INVSRCALPHA;
        switch (gi.flag[4]) {
        case 0:
            red = 50;
            green = 50;
            blue = 255;
            blend = D3DBLEND_ONE;
            break;
        case 1:
            red = 164;
            green = 136;
            blue = 55;
            break;
        case 2:
            red = 155;
            green = 255;
            blue = 155;
            break;
        case 3:
            red = 130;
            green = 130;
            blue = 255;
            break;
        case 4:
            red = 255;
            green = 50;
            blue = 50;
            break;
        case 5:
            red = 5;
            green = 5;
            blue = 5;
            break;
        case 6:
            red = 255;
            green = 255;
            blue = 255;
            break;
        case 7:
            red = 255;
            green = 125;
            blue = 255;
            break;
        default:
            continue;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24)
            | (static_cast<unsigned int>(red) << 16)
            | (static_cast<unsigned int>(green) << 8)
            | blue;
        float radius = gi.maxHeight;
        float height = -gi.height[0];
        const int segments = gi.flag[0];
        for (int sample = 0; sample < segments; ++sample) {
            if (sample > 0) {
                height += gi.height[1];
                radius -= (2.2f - static_cast<float>(sample) * 0.2f) * gi.distance;
            }
            const int angle = TableAngle(gi.rotStart + 45 * sample);
            const auto place = [&](int cornerAngle, float y) {
                const int wrapped = TableAngle(cornerAngle);
                return vector3d{
                    prim.m_pos.x + g_cosTable[wrapped] * radius,
                    prim.m_pos.y + y,
                    prim.m_pos.z + g_sinTable[wrapped] * radius
                };
            };
            SubmitWorldTeiRect(
                place(angle, height - 1.0f),
                place(angle + 90, height - 1.0f),
                place(angle + 180, height),
                place(angle + 270, height),
                viewMatrix,
                texture,
                color,
                0.0f,
                1.0f,
                1.0f,
                0.0f,
                blend,
                0.0f,
                static_cast<int>(prim.m_renderFlag));
        }
    }
}

void UpdateHalfSpherePrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    const int facing = prim->m_master != nullptr ? static_cast<int>(prim->m_master->ResolveBaseRotation() + 180.0f) : 0;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(facing);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        const int side = gi.rotStart + 90 >= 360 ? gi.rotStart - 270 : gi.rotStart + 90;
        gi.vecTNowX = g_cosTable[side] * 5.0f;
        gi.vecTNowZ = g_sinTable[side] * 5.0f;
        if (gi.process <= 50 && gi.alphaB < 120) {
            gi.alphaB = static_cast<short>(gi.alphaB + 5);
        }
    }
}

void RenderHalfSphere(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        const int basicAngle = gi.fullDisplayAngle / 10;
        if (basicAngle <= 0) {
            continue;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FAFAFFu;
        const auto place = [&](int latitude, int longitude) {
            const int lat = TableAngle(latitude);
            const int lon = TableAngle(longitude);
            const float ring = gi.distance * g_sinTable[lat];
            return vector3d{
                prim.m_pos.x + ring * g_cosTable[lon] + gi.vecTNowX,
                prim.m_pos.y + gi.distance * g_cosTable[lat] - gi.maxHeight,
                prim.m_pos.z + ring * g_sinTable[lon] + gi.vecTNowZ
            };
        };
        for (int latitude = 0; latitude < 180; latitude += 18) {
            bool havePrevious = false;
            vector3d previousNear{};
            vector3d previousFar{};
            for (int longitude = 0; longitude <= gi.fullDisplayAngle; longitude += basicAngle) {
                int spin = gi.rotStart + longitude;
                if (gi.fullDisplayAngle == 360 && longitude == gi.fullDisplayAngle) {
                    spin = gi.rotStart;
                }
                const vector3d inner = place(latitude, spin);
                const vector3d outer = place(latitude + 18, spin);
                if (havePrevious) {
                    SubmitWorldTeiRect(previousNear, inner, outer, previousFar, viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_INVSRCALPHA, 0.0f, static_cast<int>(prim.m_renderFlag));
                }
                previousNear = inner;
                previousFar = outer;
                havePrevious = true;
            }
        }
    }
}

void UpdateAttackEnergyPrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    const int facing = prim->m_master != nullptr ? static_cast<int>(prim->m_master->ResolveBaseRotation() + 180.0f) : 0;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        if (band < 2) {
            gi.riseAngle = static_cast<short>(gi.riseAngle + 1);
        }
        if (gi.riseAngle >= 360) {
            gi.riseAngle = static_cast<short>(gi.riseAngle - 360);
        }
        gi.rotStart = static_cast<short>(facing);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        const int side = gi.rotStart + 90 >= 360 ? gi.rotStart - 270 : gi.rotStart + 90;
        gi.vecTNowX = g_cosTable[side] * 5.0f;
        gi.vecTNowZ = g_sinTable[side] * 5.0f;
        if (gi.alphaB < 40) {
            gi.alphaB = static_cast<short>(gi.alphaB + 5);
        }
    }
}

void RenderAttackEnergy(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0 || gi.fullDisplayAngle < 0) {
            continue;
        }
        const int basicAngle = gi.fullDisplayAngle / 10;
        if (basicAngle <= 0) {
            continue;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FAFAFFu;
        const int latitude = TableAngle(gi.rotStart);
        const float cosLat = g_cosTable[latitude];
        const float sinLat = g_sinTable[latitude];
        const float outerDistance = gi.height[0] * gi.distance * 0.1f + gi.distance;
        const auto place = [&](int longitude, float radius, float offsetScale) {
            const int lon = TableAngle(longitude);
            const float ring = radius * g_sinTable[lon];
            return vector3d{
                prim.m_pos.x + ring * cosLat + offsetScale * gi.vecTNowX,
                prim.m_pos.y + radius * g_cosTable[lon] - gi.maxHeight,
                prim.m_pos.z + ring * sinLat + offsetScale * gi.vecTNowZ
            };
        };
        bool havePrevious = false;
        vector3d previousInner{};
        vector3d previousOuter{};
        int part = 0;
        for (int step = 0; step <= gi.fullDisplayAngle; step += basicAngle) {
            int spin = gi.riseAngle + step;
            if (gi.fullDisplayAngle == 360 && step == gi.fullDisplayAngle) {
                spin = gi.riseAngle;
            }
            const vector3d inner = place(spin, gi.distance, 1.0f);
            const vector3d outer = place(spin, outerDistance, -gi.height[0]);
            if (havePrevious) {
                const int slice = part - 1;
                const float u0 = slice == 0 ? 0.0f : static_cast<float>(slice) * 0.047619049f;
                const float u1 = static_cast<float>(slice + 1) * 0.047619049f;
                SubmitWorldTeiRect(previousInner, inner, outer, previousOuter, viewMatrix, texture, color, u0, u1, 0.89999998f, 0.0f, D3DBLEND_INVSRCALPHA, 0.0f, static_cast<int>(prim.m_renderFlag));
            }
            previousInner = inner;
            previousOuter = outer;
            havePrevious = true;
            ++part;
            if (part > 20) {
                part = 20;
            }
        }
    }
}

void UpdateElectricPrimitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        gi.distance += 0.40000001f;
        if (gi.distance > 15.0f) {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
        }
        if (gi.process <= 25) {
            gi.alphaB = static_cast<short>(gi.alphaB + rand() % 4 + 5);
        }
        if (gi.alphaB < 0) {
            gi.alphaB = 0;
        }
        if (gi.process == 1) {
            for (int step = 0; step < 21; ++step) {
                gi.height[0] += 9.0f;
                if (gi.height[0] <= 180.0f) {
                    int index = static_cast<int>(gi.vecTNowY);
                    while (index >= 1) {
                        --index;
                        gi.height[static_cast<size_t>(index + 1)] = gi.height[static_cast<size_t>(index)];
                        gi.flag[static_cast<size_t>(index + 1)] = gi.flag[static_cast<size_t>(index)];
                    }
                } else {
                    gi.height[0] = 180.0f;
                }
            }
        }
    }
}

void ElectricPathPoint(int riseAngle, float angle, float heightFactor, float distance,
    float originX, float originY, float originZ, float jitterX, float jitterY, float jitterZ, vector3d* point)
{
    int angleIndex = static_cast<int>(angle) % 360;
    if (angleIndex < 0) {
        angleIndex += 360;
    }
    const int facing = 4 * riseAngle;
    const double radial = angle > 0.0f ? static_cast<double>(angle) * 0.011111111 * distance : 0.0;
    point->x = static_cast<float>(radial * g_cosTable[facing]);
    point->y = static_cast<float>(-static_cast<double>(g_sinTable[angleIndex]) * heightFactor * distance);
    point->z = static_cast<float>(radial * g_sinTable[facing]);
    point->x = static_cast<float>(point->x + jitterX);
    point->y = static_cast<float>(static_cast<double>(jitterY) - 2.0 + point->y);
    point->z = static_cast<float>(point->z + jitterZ);
    point->x += originX;
    point->y += originY;
    point->z += originZ;
}

void RenderElectric(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        float jitterY[21]{};
        float jitterX[21]{};
        float jitterZ[21]{};
        int jitterSample = 0;
        for (; static_cast<float>(jitterSample) < gi.vecTNowY; ++jitterSample) {
            const float y = static_cast<float>(static_cast<double>(rand() % 7) * 0.1 - 0.30000001);
            const float x = static_cast<float>(static_cast<double>(rand() % 7) * 0.1 - 0.30000001);
            const float z = static_cast<float>(static_cast<double>(rand() % 7) * 0.1 - 0.30000001);
            if (jitterSample < 21) {
                jitterY[jitterSample] = y;
                jitterX[jitterSample] = x;
                jitterZ[jitterSample] = z;
            }
        }
        if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0 || gi.vecTNowY <= 0.0f) {
            continue;
        }
        const int samples = (std::min)(static_cast<int>(std::ceil(gi.vecTNowY)), 21);
        const auto pathPoint = [&](int sample) {
            vector3d point{};
            ElectricPathPoint(gi.riseAngle, gi.height[static_cast<size_t>(sample)], gi.vecTNowX, gi.distance,
                gi.sparkX, gi.sparkY, gi.sparkZ, jitterX[sample], jitterY[sample], jitterZ[sample], &point);
            return point;
        };
        int fade = 0;
        for (int sample = 0; static_cast<float>(sample) < gi.vecTNowY && sample < samples; ++sample) {
            const vector3d pointA = pathPoint(sample);
            const vector3d pointB = pathPoint((std::min)(sample + 1, 20));
            if (sample > 0 && sample < 19) {
                const float dx = pointA.x - pointB.x;
                const float dz = pointA.z - pointB.z;
                const float length = std::sqrt(dx * dx + dz * dz);
                int heading = 0;
                if (length > 0.0f) {
                    const float nx = dx / length;
                    const float nz = dz / length;
                    int angle = 0;
                    bool capped = false;
                    while (nx < g_cosTable[angle]) {
                        if (++angle > 180) {
                            heading = 180;
                            capped = true;
                            break;
                        }
                    }
                    if (!capped) {
                        heading = nz < 0.0f ? 360 - angle : angle;
                    }
                }
                const int side = TableAngle(heading + 90);
                const int alpha = gi.alphaB - fade;
                vector3d previousA{};
                vector3d previousB{};
                bool havePrevious = false;
                for (int around = 0; around <= 360; around += 120) {
                    const float up = gi.maxHeight * g_sinTable[around];
                    const float horizontal = gi.maxHeight * g_cosTable[around];
                    const float sideX = horizontal * g_cosTable[side];
                    const float sideZ = horizontal * g_sinTable[side];
                    const vector3d currentA{ pointA.x + sideX, pointA.y + up, pointA.z + sideZ };
                    const vector3d currentB{ pointB.x + sideX, pointB.y + up, pointB.z + sideZ };
                    if (havePrevious && alpha > 0) {
                        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned char>(alpha)) << 24)
                            | (static_cast<unsigned int>(6 * sample) << 16)
                            | (static_cast<unsigned int>(6 * sample) << 8)
                            | 255u;
                        SubmitWorldTeiRect(currentA, currentB, previousB, previousA, viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
                    }
                    previousA = currentA;
                    previousB = currentB;
                    havePrevious = true;
                }
            }
            fade += 5;
        }
    }
}

int ElectricHeading(float xDelta, float zDelta)
{
    const float length = std::sqrt(xDelta * xDelta + zDelta * zDelta);
    if (length <= 0.0f) {
        return 0;
    }
    const float nx = xDelta / length;
    const float nz = zDelta / length;
    int angle = 0;
    while (nx < g_cosTable[angle]) {
        if (++angle > 180) {
            return 180;
        }
    }
    return nz < 0.0f ? 360 - angle : angle;
}

void UpdateElectric2Primitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        gi.distance += 0.60000002f;
        if (gi.process <= 10) {
            gi.alphaB = static_cast<short>(gi.alphaB + rand() % 6 + 15);
        }
        if (gi.process > 20) {
            gi.alphaB = static_cast<short>(gi.alphaB - 5);
        }
        if (gi.alphaB < 0) {
            gi.alphaB = 0;
        }
        if (gi.process == 1) {
            for (int step = 0; step < 21; ++step) {
                gi.height[0] += 18.0f;
                if (gi.height[0] >= 360.0f) {
                    gi.height[0] -= 360.0f;
                } else {
                    int index = static_cast<int>(gi.vecTNowY);
                    while (index >= 1) {
                        --index;
                        gi.height[static_cast<size_t>(index + 1)] = gi.height[static_cast<size_t>(index)];
                    }
                }
            }
        } else if (gi.vecTNowY >= 0.0f) {
            const int last = static_cast<int>(gi.vecTNowY);
            for (int sample = 0; sample <= last && sample < 21; ++sample) {
                gi.height[static_cast<size_t>(sample)] += 8.0f;
                if (gi.height[static_cast<size_t>(sample)] >= 360.0f) {
                    gi.height[static_cast<size_t>(sample)] -= 360.0f;
                }
                if (gi.height[static_cast<size_t>(sample)] < 0.0f) {
                    gi.height[static_cast<size_t>(sample)] += 360.0f;
                }
            }
        }
    }
}

void Electric2PathPoint(int riseAngle, int sample, float angle, float heightFactor, float distance,
    float originX, float originY, float originZ, float jitterX, float jitterY, float jitterZ, vector3d* point)
{
    int angleIndex = static_cast<int>(angle) % 360;
    if (angleIndex < 0) {
        angleIndex += 360;
    }
    const int facing = 4 * riseAngle;
    const double radial = static_cast<double>(sample) * 0.2 * distance;
    point->x = static_cast<float>(radial * g_cosTable[facing]);
    point->y = static_cast<float>(static_cast<double>(g_sinTable[angleIndex]) * heightFactor * -15.0);
    point->z = static_cast<float>(radial * g_sinTable[facing]);
    point->x = static_cast<float>(point->x + jitterX);
    point->y = static_cast<float>(static_cast<double>(jitterY) - 8.0 + point->y);
    point->z = static_cast<float>(point->z + jitterZ);
    point->x += originX;
    point->y += originY;
    point->z += originZ;
}

void RenderElectric2(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        const int samples = static_cast<int>(gi.vecTNowY);
        float jitterY[21]{};
        float jitterX[21]{};
        float jitterZ[21]{};
        int jitterSample = 0;
        for (; static_cast<float>(jitterSample) < gi.vecTNowY; ++jitterSample) {
            const float y = static_cast<float>(static_cast<double>(rand() % 11) * 0.1 - 0.5);
            const float x = static_cast<float>(static_cast<double>(rand() % 11) * 0.1 - 0.5);
            const float z = static_cast<float>(static_cast<double>(rand() % 11) * 0.1 - 0.5);
            if (jitterSample < 21) {
                jitterY[jitterSample] = y;
                jitterX[jitterSample] = x;
                jitterZ[jitterSample] = z;
            }
        }
        if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0 || gi.vecTNowY <= 0.0f || samples <= 0) {
            continue;
        }
        const auto pathPoint = [&](int sample) {
            vector3d point{};
            Electric2PathPoint(gi.riseAngle, sample, gi.height[static_cast<size_t>(sample)], gi.vecTNowX, gi.distance,
                gi.sparkX, gi.sparkY, gi.sparkZ, jitterX[sample], jitterY[sample], jitterZ[sample], &point);
            return point;
        };
        int spread = -135;
        int span = 150;
        for (int sample = 0; sample < samples; ++sample) {
            const vector3d pointA = pathPoint(sample);
            const vector3d pointB = pathPoint((std::min)(sample + 1, 20));
            if (span < 150 && span > -135) {
                const int alpha = span <= 0 ? gi.alphaB + span : gi.alphaB + spread;
                const int side = TableAngle(ElectricHeading(pointA.x - pointB.x, pointA.z - pointB.z) + 90);
                vector3d previousA{};
                vector3d previousB{};
                bool havePrevious = false;
                for (int around = 0; around <= 360; around += 120) {
                    const float up = gi.maxHeight * g_sinTable[around];
                    const float horizontal = gi.maxHeight * g_cosTable[around];
                    const float sideX = horizontal * g_cosTable[side];
                    const float sideZ = horizontal * g_sinTable[side];
                    const vector3d currentA{ pointA.x + sideX, pointA.y + up, pointA.z + sideZ };
                    const vector3d currentB{ pointB.x + sideX, pointB.y + up, pointB.z + sideZ };
                    if (havePrevious && alpha > 0) {
                        const int green = rand() % 121;
                        const int red = rand() % 121;
                        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned char>(alpha)) << 24)
                            | (static_cast<unsigned int>(red) << 16)
                            | (static_cast<unsigned int>(green) << 8)
                            | 255u;
                        SubmitWorldTeiRect(currentA, currentB, previousB, previousA, viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
                    }
                    previousA = currentA;
                    previousB = currentB;
                    havePrevious = true;
                }
            }
            spread += 15;
            span -= 15;
        }
    }
}

void UpdateAttackEnergy2Primitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ActorPosition();
    }
    const int facing = prim->m_master != nullptr ? static_cast<int>(prim->m_master->ResolveBaseRotation() + 180.0f) : 0;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        if (gi.height[1] >= 90.0f) {
            gi.alphaB = 10;
            gi.height[1] = 0.0f;
        } else {
            gi.height[1] += 1.0f;
            gi.alphaB = static_cast<short>(gi.alphaB + 10);
            if (gi.alphaB > 60) {
                gi.alphaB = 60;
            }
        }
        if (gi.process > 30 && gi.alphaB >= 4 * (45 - gi.process)) {
            gi.alphaB = static_cast<short>(4 * (45 - gi.process));
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        gi.riseAngle = static_cast<short>(gi.riseAngle + 1);
        if (gi.riseAngle >= 360) {
            gi.riseAngle = static_cast<short>(gi.riseAngle - 360);
        }
        gi.rotStart = static_cast<short>(facing);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        const int side = gi.rotStart + 90 >= 360 ? gi.rotStart - 270 : gi.rotStart + 90;
        gi.vecTNowX = g_cosTable[side];
        gi.vecTNowZ = g_sinTable[side];
    }
}

void RenderAttackEnergy2(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const unsigned int rgb = CastingSizeRgb(prim.m_size);
    const D3DBLEND blend = CastingSizeBlend(prim.m_size);
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0 || gi.fullDisplayAngle < 0) {
            continue;
        }
        const int basicAngle = gi.fullDisplayAngle / 10;
        if (basicAngle <= 0) {
            continue;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
        const int latitude = TableAngle(gi.rotStart);
        const auto place = [&](float tilt, int longitude) {
            int tiltIndex = static_cast<int>(tilt);
            if (tiltIndex > 90) {
                tiltIndex = 90;
            }
            if (tiltIndex < 0) {
                tiltIndex = 0;
            }
            const int lon = TableAngle(longitude);
            const float radial = g_sinTable[tiltIndex] * gi.distance;
            const float along = radial * g_sinTable[lon];
            return vector3d{
                prim.m_pos.x + gi.vecTNowX * 3.0f + along * g_cosTable[latitude] + gi.maxHeight * gi.vecTNowX * g_cosTable[tiltIndex],
                prim.m_pos.y + radial * g_cosTable[lon] - gi.maxHeight,
                prim.m_pos.z + gi.vecTNowZ * 3.0f + along * g_sinTable[latitude] + gi.maxHeight * gi.vecTNowZ * g_cosTable[tiltIndex]
            };
        };
        bool havePrevious = false;
        vector3d previousInner{};
        vector3d previousOuter{};
        int part = 0;
        for (int step = 0; step <= gi.fullDisplayAngle; step += basicAngle) {
            int spin = gi.riseAngle + step;
            if (gi.fullDisplayAngle == 360 && step == gi.fullDisplayAngle) {
                spin = gi.riseAngle;
            }
            const vector3d inner = place(gi.height[1], spin);
            const vector3d outer = place(gi.height[1] + 10.0f, spin);
            if (havePrevious) {
                const int slice = part - 1;
                const float u0 = slice == 0 ? 0.0f : static_cast<float>(slice) * 0.047619049f;
                const float u1 = static_cast<float>(slice + 1) * 0.047619049f;
                for (int pass = 0; pass < 2; ++pass) {
                    SubmitWorldTeiRect(previousInner, inner, outer, previousOuter, viewMatrix, texture, color, u0, u1, 1.0f, 0.0f, blend, 0.0f, static_cast<int>(prim.m_renderFlag));
                }
            }
            previousInner = inner;
            previousOuter = outer;
            havePrevious = true;
            ++part;
            if (part > 20) {
                part = 20;
            }
        }
    }
}

void UpdateBottomOutPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;

    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process > 10) {
            gi.alphaB = static_cast<short>(gi.alphaB - 5);
            if (gi.alphaB <= 0) {
                gi.alphaB = 0;
                gi.process = 0;
                gi.sparkY = gi.flag[1] != 0 ? prim->m_pos.y - 5.0f : prim->m_pos.y;
            }
            gi.sparkY -= 0.2f;
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB + 25);
        }
        gi.height[8] += 5.0f;
        if (gi.height[8] >= 360.0f) {
            gi.height[8] -= 360.0f;
        }
        if (gi.flag[1] == 1) {
            gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle + 5);
            if (gi.fullDisplayAngle >= 360) {
                gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle - 360);
            }
        }
    }
}

void UpdateBottomPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;

    EffectGi& head = prim->m_gi[0];
    if (head.life != 1) {
        return;
    }
    ++head.process;
    if (head.process % 6 == 0) {
        for (int band = 3; band >= 1; --band) {
            EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
            if (gi.life == 0) {
                continue;
            }
            const EffectGi& previous = prim->m_gi[static_cast<size_t>(band - 1)];
            gi.distance = previous.distance + 0.5f;
            gi.height[8] = previous.height[8];
            gi.alphaB = static_cast<short>(previous.alphaB - 50);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
            gi.sparkX = previous.sparkX;
            gi.sparkY = previous.sparkY;
            gi.sparkZ = previous.sparkZ;
            gi.fullDisplayAngle = previous.fullDisplayAngle;
            gi.flag[0] = previous.flag[0];
            gi.flag[2] = 2;
            gi.height[7] = 0.0f;
        }
    }
    head.height[8] += 5.0f;
    if (head.height[8] >= 360.0f) {
        head.height[8] -= 360.0f;
    }
    head.rotStart = static_cast<short>(head.rotStart + 1);
    if (head.rotStart >= 360) {
        head.rotStart = static_cast<short>(head.rotStart - 360);
    }
    head.sparkX = prim->m_pos.x;
    head.sparkZ = prim->m_pos.z;
    head.arcBaselineY = prim->m_pos.y - 6.0f;
    head.sparkY = g_sinTable[TableAngle(head.rotStart)] * 3.0f + head.arcBaselineY;
    if (head.flag[2] == 7) {
        head.fullDisplayAngle = static_cast<short>(head.fullDisplayAngle + 10);
        if (head.fullDisplayAngle >= 360) {
            head.fullDisplayAngle = static_cast<short>(head.fullDisplayAngle - 360);
        }
    }
}

void UpdateParticleUpPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;

    int markA = 36;
    int markB = 35;
    for (int band = 0; band < 4 && markB > 7; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life == 1) {
            ++gi.process;
            if (gi.process > 0) {
                gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle - 5);
                if (gi.fullDisplayAngle < 0) {
                    gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle + 360);
                }
                if (gi.process == 1) {
                    gi.rotStart = static_cast<short>(rand() % 360);
                } else if (gi.process > 10) {
                    if (gi.flag[1] == 1) {
                        gi.flag[0] = static_cast<u8>(gi.process == markB || gi.process == markA);
                    }
                    gi.alphaB = static_cast<short>(gi.alphaB - 3);
                    if (gi.alphaB < 0) {
                        gi.alphaB = 0;
                        gi.life = 0;
                    }
                } else {
                    gi.alphaB = static_cast<short>(gi.alphaB + 15);
                }
                const int spin = TableAngle(gi.rotStart);
                gi.sparkY -= gi.height[2];
                gi.sparkX += g_cosTable[spin] * gi.height[1];
                gi.sparkZ += g_sinTable[spin] * gi.height[1];
            }
        }
        markA -= 7;
        markB -= 7;
    }
}

void RenderParticleUp(const CEffectPrim& prim, const matrix& viewMatrix)
{
    const int rot0 = TableAngle(prim.m_gi[0].alphaT);
    const int rot1 = TableAngle(prim.m_gi[1].alphaT);
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0
            || (gi.flag[2] != 0 && gi.flag[2] != 1 && gi.flag[2] != 2 && gi.flag[2] != 3 && gi.flag[2] != 4
                && gi.flag[2] != 7 && gi.flag[2] != 8 && gi.flag[2] != 9
                && gi.flag[2] != 11 && gi.flag[2] != 10 && gi.flag[2] != 15)) {
            continue;
        }
        const int spin = TableAngle(gi.fullDisplayAngle);
        const int tilt = TableAngle(static_cast<int>(gi.height[8]));
        const float radius = gi.distance + g_sinTable[tilt] * gi.distance * 0.050000001f;
        const float sideShift = FloatBits(gi.height[9]) == 0x41200000u ? 4.0f : 0.0f;
        vector3d corners[4]{};
        int angle = spin;
        for (vector3d& corner : corners) {
            corner.x = g_cosTable[angle] * radius + sideShift;
            corner.y = 0.0f;
            corner.z = g_sinTable[angle] * radius;
            RotateCornerTowardScreen(&corner, rot0, rot1);
            corner.x += gi.sparkX;
            corner.y += gi.sparkY;
            corner.z += gi.sparkZ;
            angle = TableAngle(angle + 90);
        }
        const int textureIndex = gi.flag[0];
        CTexture* texture = textureIndex >= 0 && textureIndex < static_cast<int>(prim.m_texture.size())
            ? prim.m_texture[static_cast<size_t>(textureIndex)]
            : nullptr;
        const bool straightCorners = gi.flag[2] == 0 || gi.flag[2] == 1 || gi.flag[2] == 15;
        unsigned int rgb = 0x00DCFADCu;
        D3DBLEND blend = D3DBLEND_ONE;
        if (gi.flag[2] == 15) {
            rgb = 0x00FFC8FFu;
            blend = D3DBLEND_INVSRCALPHA;
        } else if (gi.flag[2] == 0) {
            rgb = 0x00FFFFFFu;
            blend = D3DBLEND_INVSRCALPHA;
        } else if (gi.flag[2] == 1) {
            rgb = 0x00FA9696u;
        } else if (gi.flag[2] == 2) {
            rgb = 0x008282FAu;
        } else if (gi.flag[2] == 3) {
            rgb = 0x00C8C864u;
        } else if (gi.flag[2] == 4 || gi.flag[2] == 7) {
            rgb = 0x00FFFFFFu;
            if (gi.flag[2] == 4) {
                blend = D3DBLEND_INVSRCALPHA;
            }
        } else if (gi.flag[2] == 8) {
            rgb = 0x0087FF87u;
            blend = D3DBLEND_INVSRCALPHA;
        } else if (gi.flag[2] == 9) {
            blend = D3DBLEND_INVSRCALPHA;
            switch (gi.flag[3]) {
            case 1:
                rgb = 0x008787FFu;
                break;
            case 2:
                rgb = 0x0087CDFFu;
                break;
            case 3:
                rgb = 0x0087FFFFu;
                break;
            case 4:
                rgb = 0x00FFFFFFu;
                break;
            case 5:
                rgb = 0x00FF8787u;
                break;
            case 6:
                rgb = 0x00FFFFFFu;
                blend = D3DBLEND_ONE;
                break;
            default:
                continue;
            }
        } else if (gi.flag[2] == 10) {
            rgb = 0x00FAFA96u;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
        const vector3d& corner0 = straightCorners ? corners[0] : corners[1];
        const vector3d& corner1 = straightCorners ? corners[1] : corners[0];
        const vector3d& corner2 = straightCorners ? corners[2] : corners[3];
        const vector3d& corner3 = straightCorners ? corners[3] : corners[2];
        SubmitWorldTeiRect(corner0,
            corner1,
            corner2,
            corner3,
            viewMatrix,
            texture,
            color,
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            blend,
            0.0f,
            static_cast<int>(prim.m_renderFlag));
    }
}

void UpdateTeleportation2Primitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.rotStart = static_cast<short>(gi.rotStart - band + 5);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.process <= 0) {
            continue;
        }
        const int doubled = gi.process * 2;
        if (gi.flag[0] != 0) {
            gi.alphaB = static_cast<short>(gi.alphaB - 10);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        } else {
            if (doubled < 180) {
                if (doubled >= 135) {
                    gi.alphaB = static_cast<short>(gi.alphaB - 1);
                    if (gi.alphaB < 0) {
                        gi.alphaB = 0;
                    }
                }
            } else {
                gi.flag[0] = 1;
            }
            gi.maxHeight = static_cast<float>(static_cast<signed char>(gi.flag[1])) * g_sinTable[TableAngle(doubled)];
        }
        const int bandBase = band * 90;
        const int phase = band * 30;
        for (int sample = 0; sample < 21; ++sample) {
            const int angle = bandBase >= 180
                ? (3 * (gi.process + phase)) % 360
                : (gi.process + bandBase) % 360;
            const int sampleAngle = angle < 0 ? angle + 360 : angle;
            gi.height[static_cast<size_t>(sample)] = gi.maxHeight;
            gi.height[static_cast<size_t>(sample)] += g_sinTable[sampleAngle] * gi.maxHeight * g_sinTable[TableAngle(sample * 9)] * 0.30000001f;
        }
    }
}

void CopyThrowTrail(EffectGi& destination, const EffectGi& source, int alphaDrop)
{
    destination.fullDisplayAngle = source.fullDisplayAngle;
    destination.distance = source.distance;
    destination.height[8] = source.height[8];
    destination.alphaB = static_cast<short>(source.alphaB - alphaDrop);
    if (destination.alphaB < 0) {
        destination.alphaB = 0;
    }
    destination.sparkX = source.sparkX;
    destination.sparkY = source.sparkY;
    destination.sparkZ = source.sparkZ;
    destination.flag[0] = source.flag[0];
}

short ThrowHeading(float dx, float dz)
{
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length == 0.0f) {
        return 0;
    }
    const float nx = dx / length;
    const float nz = dz / length;
    int angle = 0;
    while (nx < g_cosTable[angle]) {
        ++angle;
        if (angle > 180) {
            return 180;
        }
    }
    return nz < 0.0f ? static_cast<short>(360 - angle) : static_cast<short>(angle);
}

void UpdateThrowItemPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;
    EffectGi& head = prim->m_gi[0];
    if (head.life != 1) {
        return;
    }
    ++head.process;
    if ((head.process % 2) == 1) {
        if (head.process > 3) {
            CopyThrowTrail(prim->m_gi[3], prim->m_gi[2], 25);
        }
        if (head.process > 2) {
            CopyThrowTrail(prim->m_gi[2], prim->m_gi[1], 25);
        }
        if (head.process > 1) {
            CopyThrowTrail(prim->m_gi[1], head, 150);
        }
    }
    head.fullDisplayAngle = static_cast<short>(head.fullDisplayAngle + (head.flag[2] == 15 ? 45 : 10));
    if (head.fullDisplayAngle >= 360) {
        head.fullDisplayAngle = static_cast<short>(head.fullDisplayAngle - 360);
    }
    if (head.process == 1) {
        head.rotStart = ThrowHeading(prim->m_deltaPos.x - head.sparkX, prim->m_deltaPos.z - head.sparkZ);
    }
    const float progress = head.flag[1] == 0
        ? 2.0f
        : static_cast<float>(head.process) / static_cast<float>(head.flag[1]);
    if (progress <= 1.05f) {
        const float clamped = progress > 1.0f ? 1.0f : progress;
        const int arc = static_cast<int>(static_cast<double>(clamped) * 180.0);
        head.arcBaselineY += head.height[2];
        float lift = g_sinTable[TableAngle(arc)];
        if (head.flag[2] == 0) {
            lift *= 10.0f;
        } else if (head.flag[2] != 15) {
            lift *= 30.0f;
        }
        head.sparkY = head.arcBaselineY - lift;
        const int heading = TableAngle(head.rotStart);
        head.sparkX += g_cosTable[heading] * head.height[1];
        head.sparkZ += g_sinTable[heading] * head.height[1];
    } else {
        head.alphaB = static_cast<short>(head.alphaB - 20);
        if (head.alphaB < 0) {
            head.alphaB = 0;
        }
    }
}

void UpdateEffectTexturePrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;
    EffectGi& gi = prim->m_gi[0];
    if (gi.life != 1) {
        return;
    }
    ++gi.process;
    if (gi.process <= 300 && (gi.flag[4] == 1 || gi.flag[4] == 3)) {
        if (gi.process >= 200) {
            gi.alphaB = static_cast<short>(gi.alphaB - 5);
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB + 10);
            if (gi.alphaB > 220) {
                gi.alphaB = 220;
            }
        }
    }
    if (gi.flag[4] == 2 && gi.process > 40) {
        if (gi.process >= 62) {
            gi.alphaB = static_cast<short>(gi.alphaB - 8);
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB + 8);
        }
        if (gi.alphaB > 0) {
            gi.distance += 0.69999999f;
        }
    }
    if ((gi.flag[4] == 1 || gi.flag[4] == 2 || gi.flag[4] == 3) && gi.process > 300 && gi.alphaB > 0) {
        gi.alphaB = static_cast<short>(gi.alphaB - 1);
    }
    if (gi.alphaB > 255) {
        gi.alphaB = 255;
    }
    if (gi.alphaB < 0) {
        gi.alphaB = 0;
    }
    if (gi.flag[4] != 100) {
        gi.height[8] += 2.0f;
        if (gi.height[8] >= 360.0f) {
            gi.height[8] -= 360.0f;
        }
    }
    gi.sparkY = gi.arcBaselineY;
    if (gi.flag[4] < 100) {
        gi.sparkX = prim->m_pos.x;
        gi.sparkZ = prim->m_pos.z;
    }
}

int EffectTextureStep(float heightMode, int which)
{
    const u32 bits = FloatBits(heightMode);
    if (bits == 0x3F800000u) {
        return which == 1 ? 120 : 60;
    }
    if (bits == 0x40000000u) {
        return which == 1 ? 50 : 130;
    }
    return 90;
}

void RenderEffectTexture(const CEffectPrim& prim, const matrix& viewMatrix)
{
    const int rot0 = TableAngle(prim.m_gi[0].alphaT);
    const int rot1 = TableAngle(prim.m_gi[1].alphaT);
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0 || (gi.flag[2] != 0 && gi.flag[2] != 2 && gi.flag[2] != 3)) {
            continue;
        }
        const int tilt = TableAngle(static_cast<int>(gi.height[8]));
        const float radius = g_sinTable[tilt] * gi.distance * 0.050000001f + gi.distance;
        int angle = TableAngle(gi.fullDisplayAngle);
        vector3d corners[4]{};
        for (int corner = 0; corner < 4; ++corner) {
            corners[corner].x = g_cosTable[angle] * radius;
            corners[corner].z = g_sinTable[angle] * radius;
            if (corner < 3) {
                angle = TableAngle(angle - EffectTextureStep(gi.height[10], corner));
            }
            RotateCornerTowardScreen(&corners[corner], rot0, rot1);
            corners[corner].x += gi.sparkX;
            corners[corner].y += gi.sparkY;
            corners[corner].z += gi.sparkZ;
        }
        const int textureIndex = gi.flag[0];
        CTexture* texture = textureIndex >= 0 && textureIndex < static_cast<int>(prim.m_texture.size())
            ? prim.m_texture[static_cast<size_t>(textureIndex)]
            : nullptr;
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FFFFFFu;
        if (gi.flag[2] == 2) {
            const unsigned int tinted = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FA8282u;
            SubmitWorldTeiRect(corners[1], corners[0], corners[3], corners[2], viewMatrix, texture, tinted, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
            SubmitWorldTeiRect(corners[1], corners[0], corners[3], corners[2], viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
            continue;
        }
        SubmitWorldTeiRect(corners[0], corners[1], corners[2], corners[3], viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_INVSRCALPHA, 0.0f, static_cast<int>(prim.m_renderFlag));
    }
}

void UpdateChemicalPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        const int previous = gi.process;
        ++gi.process;
        if (previous < 10) {
            gi.alphaB = static_cast<short>(gi.alphaB + 18);
        } else if (gi.process > 20) {
            gi.alphaB = static_cast<short>(gi.alphaB - 15);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
    }
}

void RenderChemicalWedge(const CEffectPrim& prim,
    const matrix& viewMatrix,
    const EffectGi& gi,
    int spread,
    short alpha,
    unsigned int rgb)
{
    if (alpha <= 0) {
        return;
    }
    const int rot0 = TableAngle(prim.m_gi[0].alphaT);
    const int rot1 = TableAngle(prim.m_gi[1].alphaT);
    const int heading = TableAngle(gi.riseAngle);
    vector3d origin{};
    vector3d left{};
    vector3d right{};
    left.x = g_cosTable[TableAngle(heading - spread)] * gi.distance;
    left.z = g_sinTable[TableAngle(heading - spread)] * gi.distance;
    right.x = g_cosTable[TableAngle(heading + spread)] * gi.distance;
    right.z = g_sinTable[TableAngle(heading + spread)] * gi.distance;
    RotateCornerTowardScreen(&origin, rot0, rot1);
    RotateCornerTowardScreen(&left, rot0, rot1);
    RotateCornerTowardScreen(&right, rot0, rot1);
    origin.x += prim.m_pos.x;
    origin.y += prim.m_pos.y;
    origin.z += prim.m_pos.z;
    left.x += prim.m_pos.x;
    left.y += prim.m_pos.y;
    left.z += prim.m_pos.z;
    right.x += prim.m_pos.x;
    right.y += prim.m_pos.y;
    right.z += prim.m_pos.z;
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(alpha)) << 24) | rgb;
    SubmitWorldTeiRect(origin, left, right, origin, viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
}

void RenderChemical(const CEffectPrim& prim, const matrix& viewMatrix)
{
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        RenderChemicalWedge(prim, viewMatrix, gi, 3, static_cast<short>(static_cast<double>(gi.alphaB) * 0.5), 0x00FFFFFFu);
        RenderChemicalWedge(prim, viewMatrix, gi, 5, gi.alphaB, 0x00FF8282u);
    }
}

void UpdateChemical2Primitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 20) {
            gi.alphaB = static_cast<short>(gi.alphaB + 4);
        } else if (gi.process > 50) {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
    }
}

void RenderChemical2(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        const int heading = TableAngle(gi.riseAngle);
        const int across = TableAngle(heading - 90);
        const int opposite = TableAngle(heading - 180);
        const int along = TableAngle(heading + 90);
        const float span = gi.height[0] + gi.maxHeight;
        const float innerX = g_cosTable[across] * span;
        const float innerZ = g_sinTable[across] * span;
        const float outerX = g_cosTable[across] * gi.height[0] + g_cosTable[along] * gi.maxHeight;
        const float outerZ = g_sinTable[across] * gi.height[0] + g_sinTable[along] * gi.maxHeight;
        vector3d nearOuter{
            prim.m_pos.x + g_cosTable[heading] * gi.distance + outerX,
            prim.m_pos.y,
            prim.m_pos.z + g_sinTable[heading] * gi.distance + outerZ
        };
        vector3d farOuter{
            prim.m_pos.x + g_cosTable[opposite] * gi.distance + outerX,
            prim.m_pos.y,
            prim.m_pos.z + g_sinTable[opposite] * gi.distance + outerZ
        };
        vector3d farInner{
            prim.m_pos.x + g_cosTable[opposite] * gi.distance + innerX,
            prim.m_pos.y,
            prim.m_pos.z + g_sinTable[opposite] * gi.distance + innerZ
        };
        vector3d nearInner{
            prim.m_pos.x + g_cosTable[heading] * gi.distance + innerX,
            prim.m_pos.y,
            prim.m_pos.z + g_sinTable[heading] * gi.distance + innerZ
        };
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FFFFFFu;
        SubmitWorldTeiRect(nearOuter, farOuter, farInner, nearInner, viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_INVSRCALPHA, 0.0f, static_cast<int>(prim.m_renderFlag));
    }
}

void UpdateChemical3Primitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    short pitch = static_cast<short>(450 - latitude);
    if (pitch >= 360) {
        pitch = static_cast<short>(pitch - 360);
    }
    prim->m_gi[1].alphaT = pitch;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 10) {
            gi.alphaB = static_cast<short>(gi.alphaB + (gi.flag[0] >= 10 ? 20 : 12));
        }
        if (gi.flag[0] >= 10) {
            gi.height[3] += 0.05f;
        }
        gi.height[1] += gi.height[3];
        if (gi.height[1] >= 200.0f) {
            gi.height[1] -= 200.0f;
        }
        if (gi.process > 20) {
            gi.alphaB = static_cast<short>(gi.alphaB - (gi.flag[0] >= 10 ? 2 : 4));
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
    }
}

bool Chemical3Rgb(u8 flag, unsigned int* rgb)
{
    switch (flag) {
    case 0:
        *rgb = 0x00E1E1FFu;
        return true;
    case 1:
        *rgb = 0x00CDCFFFu;
        return true;
    case 2:
        *rgb = 0x00B9B9FFu;
        return true;
    case 10:
        *rgb = 0x00FFFFE1u;
        return true;
    case 11:
        *rgb = 0x00FFFFCDu;
        return true;
    case 12:
        *rgb = 0x00FFFFB9u;
        return true;
    case 20:
        *rgb = 0x00E1FFE1u;
        return true;
    case 21:
        *rgb = 0x00CDFFCDu;
        return true;
    case 22:
        *rgb = 0x00B9FFB9u;
        return true;
    default:
        return false;
    }
}

void Chemical3Point(int heading, float travel, float distance, float baseX, float baseZ, float* outX, float* outZ)
{
    int direction = 0;
    float along = 0.0f;
    if (travel >= distance) {
        direction = TableAngle(heading);
        along = travel - distance;
    } else {
        direction = TableAngle(heading - 180);
        along = distance - travel;
    }
    *outX = g_cosTable[direction] * along + baseX;
    *outZ = g_sinTable[direction] * along + baseZ;
}

void RenderChemical3(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        unsigned int rgb = 0;
        if (!Chemical3Rgb(gi.flag[0], &rgb)) {
            continue;
        }
        const int heading = gi.riseAngle;
        const int across = TableAngle(heading - 90);
        const int along = TableAngle(heading + 90);
        const float innerX = g_cosTable[across] * (gi.height[0] + gi.maxHeight);
        const float innerZ = g_sinTable[across] * (gi.height[0] + gi.maxHeight);
        const float outerX = g_cosTable[across] * gi.height[0] + g_cosTable[along] * gi.maxHeight;
        const float outerZ = g_sinTable[across] * gi.height[0] + g_sinTable[along] * gi.maxHeight;
        float nearInnerX = 0.0f;
        float nearInnerZ = 0.0f;
        float farInnerX = 0.0f;
        float farInnerZ = 0.0f;
        float nearOuterX = 0.0f;
        float nearOuterZ = 0.0f;
        float farOuterX = 0.0f;
        float farOuterZ = 0.0f;
        Chemical3Point(heading, gi.height[1], gi.distance, innerX, innerZ, &nearInnerX, &nearInnerZ);
        Chemical3Point(heading, gi.height[1] + gi.height[2], gi.distance, innerX, innerZ, &farInnerX, &farInnerZ);
        Chemical3Point(heading, gi.height[1], gi.distance, outerX, outerZ, &nearOuterX, &nearOuterZ);
        Chemical3Point(heading, gi.height[1] + gi.height[2], gi.distance, outerX, outerZ, &farOuterX, &farOuterZ);
        const float y = prim.m_pos.y - 6.0f;
        const vector3d farOuter{ prim.m_pos.x + farOuterX, y, prim.m_pos.z + farOuterZ };
        const vector3d nearOuter{ prim.m_pos.x + nearOuterX, y, prim.m_pos.z + nearOuterZ };
        const vector3d nearInner{ prim.m_pos.x + nearInnerX, y, prim.m_pos.z + nearInnerZ };
        const vector3d farInner{ prim.m_pos.x + farInnerX, y, prim.m_pos.z + farInnerZ };
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
        SubmitWorldTeiRect(farOuter, nearOuter, nearInner, farInner, viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_INVSRCALPHA, 0.0f, static_cast<int>(prim.m_renderFlag));
    }
}

void UpdateFirstAidPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(TableAngle(360 - longitude));
    int latitudeComplement = 450 - latitude;
    if (latitudeComplement >= 360) {
        latitudeComplement -= 360;
    }
    prim->m_gi[1].alphaT = static_cast<short>(TableAngle(latitudeComplement));
    EffectGi& gi = prim->m_gi[0];
    if (gi.life != 1) {
        return;
    }
    ++gi.process;
    gi.height[8] += 3.0f;
    if (gi.height[8] >= 360.0f) {
        gi.height[8] -= 360.0f;
    }
    if (gi.process <= 20) {
        gi.alphaB = static_cast<short>(gi.alphaB + 4);
    }
    if (gi.process > 100) {
        gi.alphaB = static_cast<short>(gi.alphaB - 1);
        if (gi.alphaB < 0) {
            gi.alphaB = 0;
        }
    }
}

void UpdateForestPrimitive(CEffectPrim* prim)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.flag[0] == 11) {
            if (gi.process > 40) {
                if (gi.process > 140) {
                    gi.alphaB = static_cast<short>(gi.alphaB - 1);
                }
            } else {
                gi.alphaB = static_cast<short>(gi.alphaB + 1);
            }
            if (gi.alphaB <= 0) {
                gi.alphaB = 0;
            }
        }
        if (band == 1 || band == 3) {
            const int sample = (gi.process % 720) / 2;
            gi.maxHeight = gi.height[0] + g_sinTable[TableAngle(sample)] * 0.5f;
        }
    }
}

void RenderForest(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        const float radius = gi.maxHeight;
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00E6FFE6u;
        vector3d previousNow{};
        vector3d previousPre{};
        for (int step = 0; step <= 5; ++step) {
            const int angle = TableAngle(gi.rotStart + 72 * step);
            const float alongX = radius * g_cosTable[angle];
            const float alongZ = radius * g_sinTable[angle];
            const vector3d now{
                gi.vecTNowX + alongX,
                gi.vecTNowY,
                gi.vecTNowZ + alongZ
            };
            const vector3d pre{
                gi.vecTPreX + alongX,
                gi.vecTPreY,
                gi.vecTPreZ + alongZ
            };
            if (step > 0) {
                SubmitWorldTeiRect(previousNow,
                    now,
                    pre,
                    previousPre,
                    viewMatrix,
                    texture,
                    color,
                    0.0f,
                    1.0f,
                    1.0f,
                    0.0f,
                    D3DBLEND_ONE,
                    0.0f,
                    static_cast<int>(prim.m_renderFlag));
            }
            previousNow = now;
            previousPre = pre;
        }
    }
}

void UpdateColorPaperPrimitive(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.riseAngle = static_cast<short>(gi.riseAngle + 2);
        if (gi.riseAngle >= 360) {
            gi.riseAngle = static_cast<short>(gi.riseAngle - 360);
        }
        gi.sparkY += gi.distance;
        if (gi.sparkY > -10.0f) {
            gi.alphaB = static_cast<short>(gi.alphaB - 10);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
                gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle + 1);
                if (gi.fullDisplayAngle < 3) {
                    gi.sparkX = static_cast<float>(rand() % 101 - 50);
                    gi.sparkZ = static_cast<float>(rand() % 101 - 50);
                    gi.sparkY = static_cast<float>(-110 - rand() % 50);
                    gi.process = 0;
                }
            }
        }
        if (gi.process < 60) {
            gi.alphaB = static_cast<short>(gi.alphaB + 5);
            if (gi.alphaB > 250) {
                gi.alphaB = 250;
            }
        }
    }
}

void RenderColorPaper(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1) {
            continue;
        }
        const int rise = TableAngle(gi.riseAngle);
        const float radius = gi.maxHeight;
        auto corner = [&](int spin) {
            const int angle = TableAngle(spin);
            const float across = g_sinTable[angle] * radius;
            return vector3d{
                g_cosTable[angle] * radius + gi.sparkX + prim.m_pos.x,
                across * g_cosTable[rise] + gi.sparkY + prim.m_pos.y,
                across * g_sinTable[rise] + gi.sparkZ + prim.m_pos.z
            };
        };
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24)
            | (static_cast<unsigned int>(static_cast<unsigned char>(static_cast<int>(gi.height[0]))) << 16)
            | (static_cast<unsigned int>(static_cast<unsigned char>(static_cast<int>(gi.height[1]))) << 8)
            | static_cast<unsigned int>(static_cast<unsigned char>(static_cast<int>(gi.height[2])));
        SubmitWorldTeiRect(corner(gi.rotStart),
            corner(gi.rotStart + 90),
            corner(gi.rotStart + 180),
            corner(gi.rotStart + 270),
            viewMatrix,
            texture,
            color,
            0.0f,
            1.0f,
            1.0f,
            0.0f,
            D3DBLEND_ONE,
            0.0f,
            static_cast<int>(prim.m_renderFlag));
    }
}

void UpdateSphereWindPrimitive(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        const int spun = static_cast<int>(static_cast<double>(gi.rotStart) + gi.height[1]);
        gi.rotStart = static_cast<short>(spun);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
    }
}

void RenderSphereWind(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const unsigned int rgb = prim.m_size == 10.0f ? 0x00FF59B6u : 0x006464FFu;
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1) {
            continue;
        }
        const int basic = gi.fullDisplayAngle / 20;
        if (basic <= 0) {
            continue;
        }
        int count = 0;
        int step = 0;
        vector3d previousNear{};
        vector3d previousFar{};
        while (count <= gi.fullDisplayAngle) {
            int angle = count + gi.rotStart;
            if (angle < 360) {
                if (angle < 0) {
                    angle += 360;
                }
            } else {
                angle -= 360;
            }
            if (gi.fullDisplayAngle == 360 && step == 20) {
                angle = gi.rotStart;
            }
            angle = TableAngle(angle);
            const int rise = TableAngle(gi.riseAngle);
            const float cosine = g_cosTable[angle];
            const float sine = g_sinTable[angle];
            const float riseCos = g_cosTable[rise];
            const float riseSin = g_sinTable[rise];
            const vector3d nearPoint{
                gi.distance * riseCos * cosine - riseSin * gi.maxHeight,
                riseCos * gi.maxHeight + gi.distance * riseSin * cosine + gi.height[0],
                sine * gi.distance
            };
            const vector3d farPoint{
                gi.distance * riseCos * cosine + riseSin * gi.maxHeight,
                gi.distance * riseSin * cosine - riseCos * gi.maxHeight + gi.height[0],
                sine * gi.distance
            };
            if (step > 0 && gi.alphaB > 0) {
                const int part = step - 1;
                const float u0 = part == 0 ? 0.0f : static_cast<float>(part) * 0.047619049f;
                const float u1 = static_cast<float>(part + 1) * 0.047619049f;
                const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
                auto place = [&](vector3d corner) {
                    corner.x += prim.m_pos.x;
                    corner.y += prim.m_pos.y;
                    corner.z += prim.m_pos.z;
                    return corner;
                };
                SubmitWorldTeiRect(place(previousNear),
                    place(nearPoint),
                    place(farPoint),
                    place(previousFar),
                    viewMatrix,
                    texture,
                    color,
                    u0,
                    u1,
                    1.0f,
                    0.0f,
                    D3DBLEND_ONE,
                    0.0f,
                    static_cast<int>(prim.m_renderFlag));
            }
            if (++step >= 21) {
                step = 20;
            }
            previousNear = nearPoint;
            previousFar = farPoint;
            count += basic;
        }
    }
}

void UpdateMagnum2Primitive(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.riseAngle < 0) {
            gi.riseAngle = 0;
        }
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        if (gi.process >= 9) {
            gi.alphaB = static_cast<short>(gi.alphaB - 30);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
                gi.life = 0;
            }
        } else if (gi.alphaT != 0) {
            gi.alphaB = static_cast<short>(gi.alphaB + 5);
            if (gi.alphaB > 40) {
                gi.alphaB = 40;
            }
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB + 10);
            if (gi.alphaB > 80) {
                gi.alphaB = 80;
            }
        }
    }
}

void RenderMagnum2(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.fullDisplayAngle < 0) {
            continue;
        }
        const int basic = gi.fullDisplayAngle / 20;
        int sample = 0;
        int order = 0;
        int count = 0;
        vector3d previousInner{};
        vector3d previousOuter{};
        while (count <= gi.fullDisplayAngle) {
            int angle = sample + gi.rotStart;
            if (angle >= 360) {
                angle -= 360;
            }
            if (gi.fullDisplayAngle == 360 && order == 20) {
                angle = gi.rotStart;
            }
            angle = TableAngle(angle);
            const int rise = TableAngle(gi.riseAngle);
            const vector3d inner{
                g_cosTable[angle] * gi.distance,
                -3.0f,
                g_sinTable[angle] * gi.distance
            };
            const float reach = g_cosTable[rise] * gi.maxHeight;
            const float lift = g_sinTable[rise] * gi.maxHeight;
            const vector3d outer{
                reach * g_cosTable[angle] + inner.x,
                -(lift + 3.0f),
                reach * g_sinTable[angle] + inner.z
            };
            if (order > 0 && gi.alphaB > 0) {
                const int part = order - 1;
                const float u0 = part == 0 ? 0.0f : static_cast<float>(part) * 0.047619049f;
                const float u1 = static_cast<float>(part + 1) * 0.047619049f;
                const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FFFFFFu;
                auto place = [&](vector3d corner) {
                    corner.x += prim.m_pos.x;
                    corner.y += prim.m_pos.y;
                    corner.z += prim.m_pos.z;
                    return corner;
                };
                SubmitWorldTeiRect(place(previousOuter),
                    place(inner),
                    place(outer),
                    place(previousInner),
                    viewMatrix,
                    texture,
                    color,
                    u0,
                    u1,
                    1.0f,
                    0.0f,
                    D3DBLEND_ONE,
                    0.0f,
                    static_cast<int>(prim.m_renderFlag));
            }
            if (++order >= 21) {
                order = 20;
            }
            previousInner = inner;
            previousOuter = outer;
            sample = basic + count;
            count += basic;
            if (basic <= 0) {
                break;
            }
        }
    }
}

void UpdateSaintWingPrimitive(CEffectPrim* prim)
{
    float playerYaw = 0.0f;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
            playerYaw = mode->m_world->m_player->m_roty;
        }
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.riseAngle = static_cast<short>(gi.riseAngle + 1);
        if (gi.riseAngle >= 360) {
            gi.riseAngle = static_cast<short>(gi.riseAngle - 360);
        }
        const float anchor = -9.0f - (static_cast<float>(prim->m_gi[1].alphaT) - 115.0f) * 0.17f;
        gi.height[0] = anchor;
        gi.fullDisplayAngle = static_cast<short>((g_sinTable[TableAngle(gi.riseAngle)] + 1.0f) * 20.0f);
        const float yaw = playerYaw + ((band == 0 || band == 2) ? 135.0f : 45.0f);
        gi.rotStart = static_cast<short>(static_cast<int>(yaw));
    }
}

void RenderSaintWing(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band = 0; band < 2; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        int along = 0;
        for (int step = 0; along < 90; ++step) {
            const int heading = TableAngle(gi.rotStart);
            const int left = TableAngle(gi.rotStart - 65);
            const int right = TableAngle(gi.rotStart + 65);
            const vector3d edgeA{
                g_cosTable[left] * gi.distance,
                gi.height[0],
                g_sinTable[left] * gi.distance
            };
            const vector3d edgeB{
                g_cosTable[right] * gi.distance,
                gi.height[0],
                g_sinTable[right] * gi.distance
            };
            const int slot = step % 5;
            const int spread = TableAngle(along >= 45 ? along - 6 * slot : along + gi.fullDisplayAngle - 6 * slot);
            const float span = along >= 45
                ? g_sinTable[TableAngle(gi.riseAngle)] * 0.5f + 0.5f + gi.maxHeight
                : gi.maxHeight;
            const float across = span * g_cosTable[spread];
            const float drop = span * g_sinTable[spread];
            const float side = g_cosTable[heading] * across;
            const float forward = g_sinTable[heading] * across;
            const vector3d innerA{ edgeA.x + side, edgeA.y - drop, edgeA.z + forward };
            const vector3d innerB{ edgeB.x + side, edgeB.y - drop, edgeB.z + forward };
            auto place = [&](vector3d corner) {
                corner.x += prim.m_pos.x;
                corner.y += prim.m_pos.y;
                corner.z += prim.m_pos.z;
                return corner;
            };
            int alpha = gi.alphaB;
            D3DBLEND blend = D3DBLEND_INVSRCALPHA;
            unsigned int rgb = 0x00FFCDCDu;
            if (slot < 4) {
                for (int repeat = 0; repeat < 4 - slot; ++repeat) {
                    alpha = static_cast<int>(static_cast<double>(alpha) * 0.69999999);
                }
                blend = D3DBLEND_ONE;
                rgb = 0x00FF6969u;
            }
            const unsigned int color = (static_cast<unsigned int>(alpha) << 24) | rgb;
            if (band == 0) {
                SubmitWorldTeiRect(place(edgeB), place(innerB), place(innerA), place(edgeA), viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, blend, 0.0f, static_cast<int>(prim.m_renderFlag));
            } else {
                SubmitWorldTeiRect(place(edgeA), place(innerA), place(innerB), place(edgeB), viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, blend, 0.0f, static_cast<int>(prim.m_renderFlag));
            }
            along += 9;
        }
    }
}

void UpdateRectUpPrimitive(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle + 1);
        if (gi.process > gi.alphaT) {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        const int spin = TableAngle(gi.fullDisplayAngle);
        float height = g_sinTable[spin] * gi.maxHeight * 0.34999999f + gi.maxHeight * 0.64999998f;
        if (gi.process < 90) {
            height *= g_sinTable[gi.process];
        }
        gi.height[2] = height;
    }
}

void RenderRectUp(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        const int angle = TableAngle(gi.rotStart);
        const float cosine = g_cosTable[angle];
        const float sine = g_sinTable[angle];
        const float halfX = gi.height[0];
        const float halfZ = gi.height[1];
        const float lowY = -1.0f - gi.height[2];
        const vector3d highA{
            prim.m_pos.x + halfX * cosine - halfZ * sine,
            prim.m_pos.y - 1.0f,
            prim.m_pos.z + halfX * sine + halfZ * cosine
        };
        const vector3d highB{
            prim.m_pos.x + halfX * cosine + halfZ * sine,
            prim.m_pos.y - 1.0f,
            prim.m_pos.z + halfX * sine - halfZ * cosine
        };
        const vector3d highC{
            prim.m_pos.x - halfX * cosine + halfZ * sine,
            prim.m_pos.y - 1.0f,
            prim.m_pos.z - halfX * sine - halfZ * cosine
        };
        const vector3d highD{
            prim.m_pos.x - halfX * cosine - halfZ * sine,
            prim.m_pos.y - 1.0f,
            prim.m_pos.z - halfX * sine + halfZ * cosine
        };
        const vector3d lowA{ highA.x, prim.m_pos.y + lowY, highA.z };
        const vector3d lowB{ highB.x, prim.m_pos.y + lowY, highB.z };
        const vector3d lowC{ highC.x, prim.m_pos.y + lowY, highC.z };
        const vector3d lowD{ highD.x, prim.m_pos.y + lowY, highD.z };
        unsigned int rgb = 0x00FFFFFFu;
        D3DBLEND destBlend = D3DBLEND_ONE;
        const u32 heightBits = FloatBits(gi.height[3]);
        if (heightBits == 0x3F800000u) {
            rgb = 0x00EBFFEBu;
            destBlend = D3DBLEND_INVSRCALPHA;
        } else if (heightBits == 0x40000000u) {
            rgb = 0x00505050u;
            destBlend = D3DBLEND_INVSRCALPHA;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
        const auto submit = [&](const vector3d& a, const vector3d& b, const vector3d& c, const vector3d& d, unsigned int faceColor) {
            SubmitWorldTeiRect(a, b, c, d, viewMatrix, texture, faceColor, 0.0f, 1.0f, 1.0f, 0.0f, destBlend, 0.0f, static_cast<int>(prim.m_renderFlag));
        };
        submit(highA, highB, lowB, lowA, color);
        submit(highB, highC, lowC, lowB, color);
        submit(highC, highD, lowD, lowC, color);
        submit(highD, highA, lowA, lowD, color);
        if (heightBits == 0x40000000u) {
            const int doubled = gi.alphaB * 2 > 255 ? 255 : gi.alphaB * 2;
            const unsigned int capColor = (static_cast<unsigned int>(doubled) << 24) | rgb;
            submit(highA, highC, lowC, lowA, capColor);
            submit(highB, highD, lowD, lowB, capColor);
        }
    }
}

void UpdateBlindPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(TableAngle(360 - longitude));
    int latitudeComplement = 450 - latitude;
    if (latitudeComplement >= 360) {
        latitudeComplement -= 360;
    }
    prim->m_gi[1].alphaT = static_cast<short>(TableAngle(latitudeComplement));
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.alphaB < 255) {
            ++gi.alphaB;
        }
    }
}

void UpdatePoisonPrimitive(CEffectPrim* prim)
{
    int longitude = 0;
    int latitude = 0;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_view != nullptr) {
            longitude = static_cast<int>(mode->m_view->GetCameraLongitude());
            latitude = static_cast<int>(mode->m_view->GetCameraLatitude());
        }
    }
    prim->m_gi[0].alphaT = static_cast<short>(TableAngle(360 - longitude));
    int latitudeComplement = 450 - latitude;
    if (latitudeComplement >= 360) {
        latitudeComplement -= 360;
    }
    prim->m_gi[1].alphaT = static_cast<short>(TableAngle(latitudeComplement));
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.maxHeight >= 30.0f) {
            gi.maxHeight -= 30.0f;
        }
        if (gi.flag[10] != 1) {
            if (gi.alphaB <= 45) {
                gi.alphaB = static_cast<short>(gi.alphaB + 5);
            }
        } else if (gi.process < 10) {
            if (gi.alphaB <= 45) {
                gi.alphaB = static_cast<short>(gi.alphaB + 10);
            }
        } else if (gi.process > 65 && gi.alphaB >= 10) {
            gi.alphaB = static_cast<short>(gi.alphaB - 10);
        }
    }
}

void RenderPoison(const CEffectPrim& prim, const matrix& viewMatrix)
{
    const int rot0 = TableAngle(prim.m_gi[0].alphaT);
    const int rot1 = TableAngle(prim.m_gi[1].alphaT);
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        unsigned int rgb = 0x00FFFFFFu;
        if (gi.flag[10] == 1) {
            rgb = 0x00FF0000u;
        } else if (gi.flag[10] != 0) {
            rgb = 0x000000FFu;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | rgb;
        for (int cellX = -120; cellX < 90; cellX += 30) {
            for (int cellZ = -90; cellZ < 90; cellZ += 30) {
                const float x = gi.maxHeight + static_cast<float>(cellX);
                const float z = gi.maxHeight + static_cast<float>(cellZ);
                const float edge = gi.maxHeight + gi.distance;
                vector3d corners[4] = {
                    { x, -10.0f, z },
                    { edge + static_cast<float>(cellX), -10.0f, z },
                    { edge + static_cast<float>(cellX), -10.0f, edge + static_cast<float>(cellZ) },
                    { x, -10.0f, edge + static_cast<float>(cellZ) },
                };
                for (vector3d& corner : corners) {
                    RotateCornerTowardScreen(&corner, rot0, rot1);
                    corner.x += prim.m_pos.x;
                    corner.y += prim.m_pos.y;
                    corner.z += prim.m_pos.z;
                }
                SubmitWorldTeiRect(corners[0],
                    corners[1],
                    corners[2],
                    corners[3],
                    viewMatrix,
                    texture,
                    color,
                    0.0f,
                    1.0f,
                    1.0f,
                    0.0f,
                    D3DBLEND_INVSRCALPHA,
                    0.0f,
                    static_cast<int>(prim.m_renderFlag));
            }
        }
    }
}

void RenderBlind(const CEffectPrim& prim, const matrix& viewMatrix)
{
    const int rot0 = TableAngle(prim.m_gi[0].alphaT);
    const int rot1 = TableAngle(prim.m_gi[1].alphaT);
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        const unsigned int channel = gi.flag[0] == 1 ? 30u : 10u;
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24)
            | (channel << 16)
            | (channel << 8)
            | channel;
        const float span = gi.distance;
        const float outer = span + 50.0f;
        const vector3d faces[8][4] = {
            { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, span }, { span, 0.0f, span }, { span, 0.0f, 0.0f } },
            { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -span }, { span, 0.0f, -span }, { span, 0.0f, 0.0f } },
            { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, span }, { -span, 0.0f, span }, { -span, 0.0f, 0.0f } },
            { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -span }, { -span, 0.0f, -span }, { -span, 0.0f, 0.0f } },
            { { span, 0.0f, -span }, { outer, 0.0f, -span }, { outer, 0.0f, outer }, { span, 0.0f, outer } },
            { { span, 0.0f, span }, { span, 0.0f, outer }, { -outer, 0.0f, outer }, { -outer, 0.0f, span } },
            { { -span, 0.0f, span }, { -outer, 0.0f, span }, { -outer, 0.0f, -outer }, { -span, 0.0f, -outer } },
            { { -span, 0.0f, -span }, { -span, 0.0f, -outer }, { outer, 0.0f, -outer }, { outer, 0.0f, -span } },
        };
        for (int face = 0; face < 8; ++face) {
            vector3d corners[4] = { faces[face][0], faces[face][1], faces[face][2], faces[face][3] };
            for (vector3d& corner : corners) {
                RotateCornerTowardScreen(&corner, rot0, rot1);
                corner.x += prim.m_pos.x;
                corner.y += prim.m_pos.y;
                corner.z += prim.m_pos.z;
            }
            const int textureIndex = face < 4 ? 0 : 1;
            CTexture* texture = textureIndex < static_cast<int>(prim.m_texture.size())
                ? prim.m_texture[static_cast<size_t>(textureIndex)]
                : nullptr;
            SubmitWorldTeiRect(corners[0],
                corners[1],
                corners[2],
                corners[3],
                viewMatrix,
                texture,
                color,
                0.0f,
                1.0f,
                1.0f,
                0.0f,
                D3DBLEND_INVSRCALPHA,
                0.0f,
                static_cast<int>(prim.m_renderFlag));
        }
    }
}

void UpdatePortalPrimitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (band == 0) {
            ++prim->m_gi[3].process;
        }
        if (gi.process <= 0) {
            continue;
        }
        const bool variant3 = prim->m_gi[3].alphaT == 3;
        if (variant3) {
            gi.distance -= 0.25f;
            if (gi.distance < 7.0f) {
                if (gi.distance < 0.0f) {
                    gi.distance = 0.0f;
                }
                gi.alphaB = static_cast<short>(gi.alphaB - 7);
                if (gi.alphaB < 0) {
                    gi.alphaB = 0;
                    if (prim->m_gi[3].process <= 1400) {
                        gi.process = 0;
                        gi.distance = 14.0f;
                    } else {
                        gi.life = 0;
                    }
                }
            }
        } else {
            gi.distance += 0.5f;
            if (gi.distance > 7.0f) {
                gi.alphaB = static_cast<short>(gi.alphaB - 15);
                if (gi.alphaB < 0) {
                    gi.alphaB = 0;
                    const int clock = prim->m_gi[3].process;
                    if (clock > 120 && prim->m_gi[3].alphaT == 1) {
                        gi.life = 0;
                    } else if (clock <= 1400) {
                        gi.process = 0;
                        gi.distance = 0.0f;
                    } else {
                        gi.life = 0;
                    }
                }
            }
        }
        if (variant3) {
            if (gi.process < 20) {
                gi.alphaB = static_cast<short>(gi.alphaB + 12);
                if (gi.alphaB > 240) {
                    gi.alphaB = 240;
                }
            }
        } else if (gi.process < 10) {
            gi.alphaB = static_cast<short>(gi.alphaB + 24);
            if (gi.alphaB > 240) {
                gi.alphaB = 240;
            }
        }
        const float baseHeight = gi.maxHeight;
        for (int sample = 0; sample < 21; ++sample) {
            float height = baseHeight;
            if (gi.process <= 10) {
                height *= g_sinTable[9 * gi.process];
            }
            gi.height[static_cast<size_t>(sample)] = height;
        }
    }
}

void UpdateCasting4Primitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.distance -= 0.050000001f;
        if (gi.distance <= 0.0f) {
            gi.distance = 10.0f;
            gi.alphaB = 0;
        }
        gi.riseAngle = static_cast<short>(90.0f - gi.distance * 9.0f);
        gi.maxHeight = gi.distance;
        if (gi.process < prim->m_duration - 40) {
            if (gi.alphaB < 70) {
                gi.alphaB = static_cast<short>(gi.alphaB + 1);
            }
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 3);
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        for (int sample = 0; sample < 21; ++sample) {
            if (gi.flag[static_cast<size_t>(sample)] != 0) {
                continue;
            }
            float height = gi.maxHeight;
            if (height < 0.0f) {
                height = 0.0f;
            }
            gi.height[static_cast<size_t>(sample)] = height;
        }
    }
}

void UpdateCasting5Primitive(CEffectPrim* prim)
{
    if (prim->m_master != nullptr) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        gi.distance += 0.1f;
        if (gi.distance >= 13.0f) {
            gi.distance = 3.0f;
            gi.alphaB = 0;
            if (gi.process > 50) {
                gi.life = 0;
            }
        }
        float height = gi.distance - 3.0f;
        if (height <= 5.0f) {
            gi.alphaB = static_cast<short>(gi.alphaB + 2);
        } else {
            height = 10.0f - height;
            if (gi.alphaB > 0) {
                gi.alphaB = static_cast<short>(gi.alphaB - 2);
            }
        }
        gi.maxHeight = height + height;
        gi.riseAngle = static_cast<short>(90.0f - (gi.distance - 3.0f) * 5.0f);
        for (int sample = 0; sample < 21; ++sample) {
            if (gi.flag[static_cast<size_t>(sample)] != 0) {
                continue;
            }
            float sampleHeight = gi.maxHeight;
            if (sampleHeight < 0.0f) {
                sampleHeight = 0.0f;
            }
            gi.height[static_cast<size_t>(sample)] = sampleHeight;
        }
    }
}

void UpdateWindPrimitive(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) {
            continue;
        }
        ++gi.process;
        if (gi.process <= 0) {
            continue;
        }
        gi.rotStart = static_cast<short>(gi.rotStart + 5);
        if (gi.rotStart >= 360) {
            gi.rotStart = static_cast<short>(gi.rotStart - 360);
        }
        gi.fullDisplayAngle = static_cast<short>(gi.fullDisplayAngle + 3);
        const short angleCap = gi.alphaT == 2 ? 180 : 120;
        if (gi.fullDisplayAngle > angleCap) {
            gi.fullDisplayAngle = angleCap;
        }
        if (gi.alphaT == 1) {
            if (gi.process > 20) {
                gi.alphaB = static_cast<short>(gi.alphaB - 2);
                if (gi.alphaB < 0) {
                    gi.alphaB = 0;
                }
                gi.distance += 0.1f;
            }
        } else if (gi.alphaT == 2) {
            if (gi.process > 50) {
                gi.alphaB = static_cast<short>(gi.alphaB - 1);
                if (gi.alphaB < 0) {
                    gi.alphaB = 0;
                }
            }
            if (gi.process > 12) {
                gi.distance += 0.1f;
            }
        }
        if (gi.process > 1400) {
            if (gi.alphaT != 3) {
                gi.alphaB = static_cast<short>(gi.alphaB - 3);
            }
            if (gi.alphaB < 0) {
                gi.alphaB = 0;
            }
        }
        if (gi.alphaT == 2) {
            if (gi.process < 12) {
                gi.alphaB = static_cast<short>(gi.alphaB + 1);
            }
        } else if (gi.process < 12) {
            gi.alphaB = static_cast<short>(gi.alphaB + 10);
            if (gi.alphaB > 250) {
                gi.alphaB = 250;
            }
        }
    }
}

void WindRenderSample(int angle, int riseAngle, float height, float distance, float maxHeight, vector3d* base, vector3d* top)
{
    const int spin = ((angle % 360) + 360) % 360;
    const int rise = ((riseAngle % 360) + 360) % 360;
    const float radial = static_cast<float>(static_cast<double>(g_cosTable[rise]) * height);
    const float vertical = static_cast<float>(static_cast<double>(g_sinTable[rise]) * height);
    base->x = static_cast<float>(static_cast<double>(g_cosTable[spin]) * distance);
    base->y = -maxHeight;
    base->z = static_cast<float>(static_cast<double>(g_sinTable[spin]) * distance);
    top->x = static_cast<float>(static_cast<double>(radial) * g_cosTable[spin] + base->x);
    top->y = static_cast<float>(-(static_cast<double>(vertical) + maxHeight));
    top->z = static_cast<float>(static_cast<double>(radial) * g_sinTable[spin] + base->z);
}

void RenderWind(CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const unsigned int rgb = CastingSizeRgb(prim.m_size);
    const D3DBLEND blend = CastingSizeBlend(prim.m_size);
    for (EffectGi& gi : prim.m_gi) {
        if (gi.life != 1) {
            continue;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(CastingSizeAlpha(prim.m_size, static_cast<unsigned int>(gi.alphaB)))) << 24) | rgb;
        const int basicAngle = gi.fullDisplayAngle / 21;
        if (basicAngle <= 0) {
            continue;
        }
        bool havePrevious = false;
        int order = 0;
        for (int count = 0; count < gi.fullDisplayAngle; count += basicAngle) {
            int angle = (count + gi.rotStart) % 360;
            if (gi.fullDisplayAngle == 360 && order == 20) {
                angle = TableAngle(gi.rotStart);
            }
            const float sampleHeight = gi.height[static_cast<size_t>((std::min)(order, 20))];
            vector3d base{}, top{};
            WindRenderSample(angle, gi.riseAngle, sampleHeight, gi.distance, gi.maxHeight, &base, &top);
            gi.sparkX = base.x;
            gi.sparkY = base.y;
            gi.sparkZ = base.z;
            gi.vecTNowX = top.x;
            gi.vecTNowY = top.y;
            gi.vecTNowZ = top.z;
            if (havePrevious) {
                const int part = order - 1;
                const float u0 = static_cast<float>(static_cast<double>(part) * 0.047619049);
                const float u1 = static_cast<float>(static_cast<double>(part + 1) * 0.047619049);
                const vector3d previousBase{ gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ };
                const vector3d previousTop{ gi.vecTPreX, gi.vecTPreY, gi.vecTPreZ };
                const vector3d worldPreviousBase = AddVec3(previousBase, prim.m_pos);
                const vector3d worldBase = AddVec3(base, prim.m_pos);
                const vector3d worldTop = AddVec3(top, prim.m_pos);
                const vector3d worldPreviousTop = AddVec3(previousTop, prim.m_pos);
                SubmitWorldTeiRect(worldPreviousBase,
                    worldBase,
                    worldTop,
                    worldPreviousTop,
                    viewMatrix,
                    texture,
                    color,
                    u0,
                    u1,
                    1.0f,
                    0.0f,
                    blend,
                    0.0f,
                    static_cast<int>(prim.m_renderFlag));
            }
            gi.vecBPreX = gi.sparkX;
            gi.arcBaselineY = gi.sparkY;
            gi.vecBPreZ = gi.sparkZ;
            gi.vecTPreX = gi.vecTNowX;
            gi.vecTPreY = gi.vecTNowY;
            gi.vecTPreZ = gi.vecTNowZ;
            havePrevious = true;
            ++order;
            if (order >= 21) {
                order = 20;
            }
        }
    }
}

void GrandCross2Corners(int rotStart, float horizontal, float diagonal, vector3d (&corners)[4])
{
    const int angle = ((rotStart % 360) + 360) % 360;
    const double cosine = g_cosTable[angle];
    const double sine = g_sinTable[angle];
    const float hx = static_cast<float>(static_cast<double>(horizontal) * cosine);
    const float hz = static_cast<float>(static_cast<double>(horizontal) * sine);
    const float dx = static_cast<float>(static_cast<double>(diagonal) * cosine);
    const float dz = static_cast<float>(static_cast<double>(diagonal) * sine);
    corners[0] = { static_cast<float>(hx - dz), -1.0f, static_cast<float>(hz + dx) };
    corners[1] = { static_cast<float>(hx + dz), -1.0f, static_cast<float>(hz - dx) };
    corners[2] = { static_cast<float>(-hx + dz), -1.0f, static_cast<float>(-hz - dx) };
    corners[3] = { static_cast<float>(-hx - dz), -1.0f, static_cast<float>(-hz + dx) };
}

void RenderGrandCross2(CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        vector3d corners[4]{};
        vector3d top[4]{};
        GrandCross2Corners(gi.rotStart, gi.height[0], gi.height[1], corners);
        const float topY = static_cast<float>(-1.0 - static_cast<double>(gi.height[2]));
        for (int corner = 0; corner < 4; ++corner) {
            top[corner] = corners[corner];
            top[corner].y = topY;
        }
        // The original renderer leaves its generated corners in GI scratch slots.
        gi.sparkX = corners[1].x; gi.sparkY = corners[1].y; gi.sparkZ = corners[1].z;
        gi.vecTNowX = corners[2].x; gi.vecTNowY = corners[2].y; gi.vecTNowZ = corners[2].z;
        gi.vecBPreX = corners[0].x; gi.arcBaselineY = corners[0].y; gi.vecBPreZ = corners[0].z;
        gi.vecTPreX = corners[3].x; gi.vecTPreY = corners[3].y; gi.vecTPreZ = corners[3].z;
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | 0x00FFFFFFu;
        for (int side = 0; side < 4; ++side) {
            const int next = (side + 1) % 4;
            const vector3d worldBase{ prim.m_pos.x + corners[side].x, prim.m_pos.y + corners[side].y, prim.m_pos.z + corners[side].z };
            const vector3d worldNextBase{ prim.m_pos.x + corners[next].x, prim.m_pos.y + corners[next].y, prim.m_pos.z + corners[next].z };
            const vector3d worldNextTop{ prim.m_pos.x + top[next].x, prim.m_pos.y + top[next].y, prim.m_pos.z + top[next].z };
            const vector3d worldTop{ prim.m_pos.x + top[side].x, prim.m_pos.y + top[side].y, prim.m_pos.z + top[side].z };
            SubmitWorldTeiRect(worldBase, worldNextBase, worldNextTop, worldTop, viewMatrix, texture, color,
                0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_INVSRCALPHA, 0.0f, static_cast<int>(prim.m_renderFlag));
        }
        const unsigned int innerColor = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB / 2)) << 24) | 0x00FFFFFFu;
        const vector3d floor0 = AddVec3(corners[0], prim.m_pos);
        const vector3d floor1 = AddVec3(corners[1], prim.m_pos);
        const vector3d floor2 = AddVec3(corners[2], prim.m_pos);
        const vector3d floor3 = AddVec3(corners[3], prim.m_pos);
        SubmitWorldTeiRect(floor0, floor1, floor2, floor3, viewMatrix, texture, innerColor,
            0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
        SubmitWorldTeiRect(floor3, floor2, floor1, floor0, viewMatrix, texture, innerColor,
            0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
    }
}

void LineLinkInterpolatedEnd(const vector3d& current, const vector3d& previous, int process, vector3d* endpoint)
{
    if (process >= 15) {
        *endpoint = previous;
        return;
    }
    const float wave = g_sinTable[6 * process];
    endpoint->x = static_cast<float>(static_cast<double>(previous.x - current.x) * wave + current.x);
    endpoint->y = static_cast<float>(static_cast<double>(previous.y - current.y) * wave + current.y);
    endpoint->z = static_cast<float>(static_cast<double>(previous.z - current.z) * wave + current.z);
}

int LineLinkHeading(float xDelta, float zDelta)
{
    const float input = xDelta * xDelta + zDelta * zDelta;
    const double length = std::sqrt(input);
    const float normalizedZ = static_cast<float>(zDelta / length);
    const float normalizedX = static_cast<float>(xDelta / length);
    int angle = 0;
    while (normalizedX < g_cosTable[angle]) {
        if (++angle > 180) {
            angle = 180;
            break;
        }
    }
    return normalizedZ < 0.0f ? 360 - angle : angle;
}

void LineLinkRingPoint(const vector3d& center, float radius, int around, int side, vector3d* point)
{
    const int ringAngle = around / 4;
    const int angle = ((side % 360) + 360) % 360;
    const double vertical = static_cast<double>(radius) * g_sinTable[ringAngle];
    const double horizontal = static_cast<double>(radius) * g_cosTable[ringAngle];
    const float xOffset = static_cast<float>(horizontal * g_cosTable[angle]);
    const float zOffset = static_cast<float>(horizontal * g_sinTable[angle]);
    point->x = center.x + xOffset;
    point->y = static_cast<float>(static_cast<double>(center.y) + vertical);
    point->z = center.z + zOffset;
}

void RenderLineLink(const CEffectPrim& prim, const matrix& viewMatrix)
{
    const EffectGi& gi = prim.m_gi[0];
    if (gi.life != 1 || gi.alphaB <= 0) {
        return;
    }
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    const vector3d current{ gi.vecTNowX, gi.vecTNowY, gi.vecTNowZ };
    const vector3d previous{ gi.vecTPreX, gi.vecTPreY, gi.vecTPreZ };
    vector3d endpoint{};
    LineLinkInterpolatedEnd(current, previous, gi.process, &endpoint);
    const int side = LineLinkHeading(current.x - previous.x, current.z - previous.z) + 90;
    const bool modeTwo = gi.height[4] == 2.0f;
    const bool modeThree = gi.height[4] == 3.0f;
    for (int layer = 0; layer < 2; ++layer) {
        const float radius = gi.maxHeight * (layer == 0 ? 1.0f : 2.0f);
        vector3d previousCurrent{}, previousEnd{};
        bool havePrevious = false;
        for (int around = 0; around <= 1440; around += 144) {
            vector3d currentPoint{}, endPoint{};
            LineLinkRingPoint(current, radius, around, side, &currentPoint);
            LineLinkRingPoint(endpoint, radius, around, side, &endPoint);
            if (havePrevious) {
                int alpha = gi.alphaB;
                unsigned int tint = 0x00FF0000u;
                if (layer == 0) {
                    if (modeTwo) {
                        tint = 0x00006496u;
                    } else if (modeThree) {
                        tint = 0x00FF59B6u;
                    }
                } else {
                    alpha *= 2;
                    if (modeTwo) {
                        tint = 0x00FF3200u;
                    } else if (modeThree) {
                        tint = 0x00FF59B6u;
                    } else {
                        tint = 0x00FFFF00u;
                    }
                }
                const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned char>(alpha)) << 24) | tint;
                SubmitWorldTeiRect(previousCurrent, currentPoint, endPoint, previousEnd, viewMatrix, texture, color,
                    0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
            }
            previousCurrent = currentPoint;
            previousEnd = endPoint;
            havePrevious = true;
        }
    }
}

void IntimidateTowardScreen(vector3d corners[4], int rotationX, int rotationYZ)
{
    for (int i = 0; i < 4; ++i) {
        const double y = corners[i].y;
        const double z = corners[i].z;
        corners[i].z = static_cast<float>(z * g_cosTable[rotationYZ] - y * g_sinTable[rotationYZ]);
        corners[i].y = static_cast<float>(z * g_sinTable[rotationYZ] + y * g_cosTable[rotationYZ]);
    }
    for (int i = 0; i < 4; ++i) {
        const double x = corners[i].x;
        const double z = corners[i].z;
        corners[i].x = static_cast<float>(x * g_cosTable[rotationX] - z * g_sinTable[rotationX]);
        corners[i].z = static_cast<float>(x * g_sinTable[rotationX] + z * g_cosTable[rotationX]);
    }
}

void IntimidateCorners(const EffectGi& gi, vector3d corners[4])
{
    const int angleXZ = static_cast<int>(gi.height[2]);
    int angle = gi.rotStart;
    for (int i = 0; i < 4; ++i) {
        const double radius = gi.height[1];
        const double radialX = static_cast<double>(g_cosTable[angle]) * radius;
        corners[i].y = static_cast<float>(static_cast<double>(g_sinTable[angle]) * radius);
        corners[i].x = static_cast<float>(static_cast<double>(g_cosTable[angleXZ]) * radialX);
        corners[i].z = static_cast<float>(static_cast<double>(g_sinTable[angleXZ]) * radialX);
        angle += 90;
        if (angle >= 360) {
            angle -= 360;
        }
    }
    if (gi.flag[0] == 2) {
        int angle = gi.rotStart;
        for (int i = 0; i < 4; ++i) {
            corners[i].y = 0.0f;
            corners[i].x = static_cast<float>(static_cast<double>(g_cosTable[angle]) * gi.height[1]);
            corners[i].z = static_cast<float>(static_cast<double>(g_sinTable[angle]) * gi.height[1]);
            angle += 90;
            if (angle >= 360) {
                angle -= 360;
            }
        }
    }
}

vector3d IntimidateCenter(const EffectGi& gi)
{
    const float fullX = static_cast<float>(static_cast<double>(g_cosTable[gi.fullDisplayAngle]) * gi.distance);
    const float fullY = static_cast<float>(static_cast<double>(g_sinTable[gi.fullDisplayAngle]) * gi.distance);
    return {static_cast<float>(static_cast<double>(g_cosTable[gi.riseAngle]) * fullX), fullY,
        static_cast<float>(static_cast<double>(g_sinTable[gi.riseAngle]) * fullX)};
}

void RenderIntimidate(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        vector3d corners[4]{};
        IntimidateCorners(gi, corners);
        if (gi.flag[0] == 2) {
            IntimidateTowardScreen(corners, prim.m_gi[0].alphaT, prim.m_gi[1].alphaT);
        }
        const vector3d center = IntimidateCenter(gi);
        for (vector3d& corner : corners) {
            corner.x = static_cast<float>(static_cast<double>(corner.x) + center.x + gi.sparkX);
            corner.y = static_cast<float>(static_cast<double>(corner.y) + center.y);
            corner.y = static_cast<float>(static_cast<double>(corner.y) + gi.sparkY);
            corner.z = static_cast<float>(static_cast<double>(corner.z) + center.z);
            corner.z = static_cast<float>(static_cast<double>(corner.z) + gi.sparkZ);
        }
        unsigned int tint = 0;
        switch (gi.flag[0]) {
        case 0: tint = 0x006496FFu; break;
        case 1: tint = 0x00FAFA9Bu; break;
        case 2: tint = 0x00FA6464u; break;
        default: continue;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | tint;
        SubmitWorldTeiRect(corners[0], corners[1], corners[2], corners[3], viewMatrix, texture, color,
            0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
    }
}

struct GumGangBandOffset {
    float* x;
    float* y;
    float* z;
};

bool GumGangFrontFacing(int angle, int screenAngle, float orientation)
{
    const int difference = static_cast<int>(static_cast<double>(angle) - screenAngle);
    const bool front = (difference < 70 && difference > -80)
        || difference < -270 || difference > 270;
    const bool normalOrder = orientation == 0.0f || orientation == 2.0f;
    return normalOrder ? front : !front;
}

void SubmitGumGangRect(const vector3d corners[4], const vector3d& translation,
    const matrix& viewMatrix, CTexture* texture, short alpha,
    unsigned char red, unsigned char green, unsigned char blue, int renderFlags)
{
    if (!texture) {
        return;
    }
    const vector3d positions[2][3] = {
        { AddVec3(corners[0], translation), AddVec3(corners[1], translation), AddVec3(corners[2], translation) },
        { AddVec3(corners[3], translation), AddVec3(corners[2], translation), AddVec3(corners[0], translation) },
    };
    const float uvs[2][3][2] = {
        { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } },
        { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } },
    };
    const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(alpha)) << 24)
        | (static_cast<unsigned int>(red) << 16)
        | (static_cast<unsigned int>(green) << 8)
        | blue;
    for (int triangle = 0; triangle < 2; ++triangle) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return;
        }
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_ONE;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            ProjectReferenceEffectVertex(positions[triangle][vertex], viewMatrix, face->m_verts[vertex]);
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
}

void RenderGumGangLayer(CEffectPrim& prim, EffectGi& gi, const matrix& viewMatrix,
    float radiusScale, int alphaCap, bool capAlpha,
    unsigned char red, unsigned char green, unsigned char blue)
{
    const int angleSlots[4] = { 3, 7, 11, 15 };
    const int alphaSlots[4] = { 1, 5, 9, 13 };
    const int orientationSlots[4] = { 2, 6, 10, 14 };
    const int textureSlots[4] = { 2, 6, 10, 14 };
    const double radius = static_cast<double>(gi.distance) * radiusScale;
    vector3d baseCorners[4]{};
    for (int corner = 0; corner < 4; ++corner) {
        int angle = 45 + 90 * corner;
        if (angle >= 360) {
            angle -= 360;
        }
        baseCorners[corner] = {
            static_cast<float>(static_cast<double>(g_cosTable[angle]) * radius),
            0.0f,
            static_cast<float>(static_cast<double>(g_sinTable[angle]) * radius),
        };
    }
    IntimidateTowardScreen(baseCorners, prim.m_gi[0].alphaT, prim.m_gi[1].alphaT);

    GumGangBandOffset offsets[4] = {
        { &gi.vecBPreX, &gi.arcBaselineY, &gi.vecBPreZ },
        { &gi.sparkX, &gi.sparkY, &gi.sparkZ },
        { &gi.vecTPreX, &gi.vecTPreY, &gi.vecTPreZ },
        { &gi.vecTNowX, &gi.vecTNowY, &gi.vecTNowZ },
    };
    for (int strip = 0; strip < 4; ++strip) {
        const int angle = static_cast<int>(gi.height[static_cast<size_t>(angleSlots[strip])]);
        const double scale = 4.0;
        *offsets[strip].x = static_cast<float>(static_cast<double>(g_cosTable[angle]) * scale);
        *offsets[strip].z = static_cast<float>(static_cast<double>(g_sinTable[angle]) * scale);

        vector3d stripCorners[4]{};
        for (int corner = 0; corner < 4; ++corner) {
            stripCorners[corner] = {
                static_cast<float>(static_cast<double>(baseCorners[corner].x) + *offsets[strip].x),
                static_cast<float>(static_cast<double>(baseCorners[corner].y) + *offsets[strip].y),
                static_cast<float>(static_cast<double>(baseCorners[corner].z) + *offsets[strip].z),
            };
        }
        if (!GumGangFrontFacing(angle, prim.m_gi[0].alphaT,
                gi.height[static_cast<size_t>(orientationSlots[strip])])) {
            std::swap(stripCorners[0], stripCorners[1]);
            std::swap(stripCorners[2], stripCorners[3]);
        }

        const int textureIndex = static_cast<int>(gi.height[static_cast<size_t>(textureSlots[strip])]);
        CTexture* texture = textureIndex >= 0 && textureIndex < static_cast<int>(prim.m_texture.size())
            ? prim.m_texture[static_cast<size_t>(textureIndex)] : nullptr;
        int alpha = static_cast<int>(gi.height[static_cast<size_t>(alphaSlots[strip])]);
        if (capAlpha && alpha > alphaCap) {
            alpha = alphaCap;
        }
        SubmitGumGangRect(stripCorners, prim.m_pos, viewMatrix, texture,
            static_cast<short>(alpha), red, green, blue, static_cast<int>(prim.m_renderFlag));
    }
}

void RenderGumGang(CEffectPrim& prim, const matrix& viewMatrix, float radiusScale,
    unsigned char red, unsigned char green, unsigned char blue)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        if (gi.flag[4] == 1) {
            red = 255;
            green = 20;
            blue = 20;
        }
        RenderGumGangLayer(prim, gi, viewMatrix, radiusScale, 0, false, red, green, blue);
    }
}

void RenderDoubleGumGang(CEffectPrim& prim, const matrix& viewMatrix)
{
    const float radialScales[3] = { 1.0f, 1.2f, 2.0f };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) {
            continue;
        }
        for (int layer = 0; layer < 3; ++layer) {
            unsigned char red = 0;
            unsigned char green = 0;
            unsigned char blue = 0;
            if (gi.flag[4] == 0) {
                if (layer == 0) { red = 100; green = 100; blue = 255; }
                else { red = 0; green = 0; blue = 120; }
            } else if (gi.flag[4] == 1) {
                if (layer == 0) { red = 90; green = 90; blue = 80; }
                else if (layer == 1) { red = 60; green = 60; blue = 50; }
                else { red = 30; green = 30; blue = 20; }
            } else if (layer == 0) {
                red = 255; green = 100; blue = 100;
            } else {
                red = 120; green = 0; blue = 0;
            }
            RenderGumGangLayer(prim, gi, viewMatrix, radialScales[layer], gi.process, true, red, green, blue);
        }
    }
}

void SubmitPokJukRect(const vector3d corners[4], const vector3d& translation,
    const matrix& viewMatrix, CTexture* firstTexture, CTexture* secondTexture,
    short alpha, unsigned char red, unsigned char green, unsigned char blue,
    int renderFlags)
{
    const vector3d positions[2][3] = {
        { AddVec3(corners[0], translation), AddVec3(corners[1], translation), AddVec3(corners[2], translation) },
        { AddVec3(corners[3], translation), AddVec3(corners[2], translation), AddVec3(corners[0], translation) },
    };
    const float uvs[2][3][2] = {
        { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } },
        { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } },
    };
    const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(alpha)) << 24)
        | (static_cast<unsigned int>(red) << 16)
        | (static_cast<unsigned int>(green) << 8)
        | blue;
    CTexture* textures[2] = { firstTexture, secondTexture };
    for (int triangle = 0; triangle < 2; ++triangle) {
        if (!textures[triangle]) {
            continue;
        }
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return;
        }
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = textures[triangle];
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            ProjectReferenceEffectVertex(positions[triangle][vertex], viewMatrix, face->m_verts[vertex]);
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
}

void RenderPokJuk(CEffectPrim& prim, const matrix& viewMatrix)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        unsigned char red = 255, green = 70, blue = 255;
        switch (gi.flag[0]) {
        case 0: red = 70; green = 70; blue = 255; break;
        case 1: red = 255; green = 70; blue = 70; break;
        case 2: red = 70; green = 255; blue = 70; break;
        case 3: red = 255; green = 255; blue = 90; break;
        default: break;
        }
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        float minus = 0.0f;
        for (int pass = 0; pass < 2; ++pass) {
            const float radius = gi.maxHeight - minus;
            vector3d corners[4]{};
            for (int corner = 0; corner < 4; ++corner) {
                int angle = static_cast<int>(gi.rotStart) + 90 * corner;
                if (corner > 0 && angle >= 360) angle -= 360;
                int zAngle = corner == 3 ? angle : static_cast<int>(gi.rotStart) + 90 * (corner + 1);
                zAngle %= 360;
                if (zAngle < 0) zAngle += 360;
                corners[corner] = {
                    static_cast<float>(static_cast<double>(g_cosTable[angle]) * radius),
                    0.0f,
                    static_cast<float>(static_cast<double>(g_sinTable[zAngle]) * radius),
                };
            }
            IntimidateTowardScreen(corners, prim.m_gi[0].alphaT, prim.m_gi[1].alphaT);

            vector3d offset{};
            if (pass == 0) offset = { gi.sparkX, gi.sparkY, gi.sparkZ };
            else if (pass == 1) offset = { gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ };
            else if (pass == 2) offset = { gi.vecTNowX, gi.vecTNowY, gi.vecTNowZ };
            else offset = { gi.vecTPreX, gi.vecTPreY, gi.vecTPreZ };
            for (vector3d& corner : corners) {
                corner = AddVec3(corner, offset);
            }

            int alpha = static_cast<int>(gi.alphaB) / (pass + 1);
            const bool textured = gi.flag[3] != 0;
            if (textured) alpha /= 4;
            const int textureIndex = textured ? static_cast<int>(gi.flag[3]) : 0;
            CTexture* firstTexture = textureIndex >= 0 && textureIndex < static_cast<int>(prim.m_texture.size())
                ? prim.m_texture[static_cast<size_t>(textureIndex)] : nullptr;
            CTexture* secondTexture = !prim.m_texture.empty() ? prim.m_texture[0] : nullptr;
            SubmitPokJukRect(corners, prim.m_pos, viewMatrix, firstTexture, secondTexture,
                static_cast<short>(alpha), textured ? 255 : red, textured ? 255 : green,
                textured ? 255 : blue, static_cast<int>(prim.m_renderFlag));
            minus += pass == 0 ? 0.5f : 0.25f;
        }
    }
}

int GuardTableAngle(int angle)
{
    angle %= 360;
    return angle < 0 ? angle + 360 : angle;
}

void GuardRingCorners(const EffectGi& gi, int rowAngle, int sliceAngle,
    float radialExtent, float verticalOffset, int crossRadius, vector3d corners[4])
{
    const int row = GuardTableAngle(rowAngle);
    int orientation = static_cast<int>(gi.rotStart) + sliceAngle;
    if (orientation >= 360) orientation -= 360;
    if (orientation < 0) orientation += 360;
    int perpendicular = orientation - 90;
    if (orientation - 90 >= 360) perpendicular = orientation - 450;
    if (perpendicular < 0) perpendicular += 360;

    const double radius = gi.maxHeight;
    const double rowSin = g_sinTable[row];
    const double rowCos = g_cosTable[row];
    const double centerRadius = radialExtent;
    const double centerX = g_cosTable[orientation] * rowCos * centerRadius;
    const double centerZ = g_sinTable[orientation] * rowCos * centerRadius;
    const double centerY = rowSin * centerRadius + verticalOffset;
    const int crossAngle = GuardTableAngle(rowAngle + 90);
    const double sinA = g_sinTable[crossAngle];
    const double cosA = g_cosTable[crossAngle];
    const double sinB = g_sinTable[perpendicular];
    const double cosB = g_cosTable[perpendicular];
    const double r = crossRadius;
    const vector3d local[4] = {
        { static_cast<float>(r * cosB - r * cosA * sinB), static_cast<float>(r * sinA), static_cast<float>(r * cosA * cosB + r * sinB) },
        { static_cast<float>(-r * cosB - r * cosA * sinB), static_cast<float>(r * sinA), static_cast<float>(r * cosA * cosB - r * sinB) },
        { static_cast<float>(-r * cosB + r * cosA * sinB), static_cast<float>(-r * sinA), static_cast<float>(-r * cosA * cosB - r * sinB) },
        { static_cast<float>(r * cosB + r * cosA * sinB), static_cast<float>(-r * sinA), static_cast<float>(-r * cosA * cosB + r * sinB) },
    };
    for (int corner = 0; corner < 4; ++corner) {
        corners[corner] = {
            static_cast<float>(static_cast<double>(local[corner].x) + centerX),
            static_cast<float>(static_cast<double>(local[corner].y) + centerY),
            static_cast<float>(static_cast<double>(local[corner].z) + centerZ),
        };
    }
}

void SubmitGuardRect(const vector3d corners[4], const vector3d& translation,
    const matrix& viewMatrix, CTexture* firstTexture, CTexture* secondTexture,
    short alpha, unsigned char red, unsigned char green, unsigned char blue,
    D3DBLEND destinationBlend, int renderFlags)
{
    const vector3d positions[2][3] = {
        { AddVec3(corners[0], translation), AddVec3(corners[1], translation), AddVec3(corners[2], translation) },
        { AddVec3(corners[3], translation), AddVec3(corners[2], translation), AddVec3(corners[0], translation) },
    };
    const float uvs[2][3][2] = {
        { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } },
        { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } },
    };
    CTexture* textures[2] = { firstTexture, secondTexture };
    const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(alpha)) << 24)
        | (static_cast<unsigned int>(red) << 16)
        | (static_cast<unsigned int>(green) << 8)
        | blue;
    for (int triangle = 0; triangle < 2; ++triangle) {
        if (!textures[triangle]) continue;
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) return;
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = textures[triangle];
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = destinationBlend;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            ProjectReferenceEffectVertex(positions[triangle][vertex], viewMatrix, face->m_verts[vertex]);
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
}

void RenderGuard(CEffectPrim& prim, const matrix& viewMatrix)
{
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1) continue;
        for (int row = 0; row < 3; ++row) {
            const int rowAngle = -45 + row * 45;
            gi.distance = static_cast<float>(3 - (std::abs(row - 1)));
            const int innerRadius = static_cast<int>(gi.distance);
            for (int slice = 0; slice < 5; ++slice) {
                const int sliceAngle = -90 + slice * 45;
                vector3d corners[4]{};
                const float firstRadius = gi.maxHeight;
                GuardRingCorners(gi, rowAngle, sliceAngle, firstRadius,
                    -(firstRadius + 2.0f), innerRadius, corners);
                SubmitGuardRect(corners, prim.m_pos, viewMatrix,
                    prim.m_texture.empty() ? nullptr : prim.m_texture[0],
                    prim.m_texture.empty() ? nullptr : prim.m_texture[0],
                    gi.alphaB, gi.flag[0] == 2 ? 255 : 205, gi.flag[0] == 2 ? 155 : 255,
                    gi.flag[0] == 2 ? 0 : 205, D3DBLEND_ONE, static_cast<int>(prim.m_renderFlag));

                const float secondRadius = gi.maxHeight + 0.2f;
                GuardRingCorners(gi, rowAngle, sliceAngle, secondRadius,
                    -gi.maxHeight, innerRadius, corners);
                SubmitGuardRect(corners, prim.m_pos, viewMatrix,
                    prim.m_texture.empty() ? nullptr : prim.m_texture[0],
                    prim.m_texture.empty() ? nullptr : prim.m_texture[0],
                    gi.alphaB, gi.flag[0] == 2 ? 255 : 155, 255,
                    gi.flag[0] == 2 ? 155 : 155, D3DBLEND_ONE, static_cast<int>(prim.m_renderFlag));

                const float thirdRadius = gi.maxHeight + gi.height[0] + 0.4f;
                GuardRingCorners(gi, rowAngle, sliceAngle, thirdRadius,
                    -gi.maxHeight, innerRadius, corners);
                SubmitGuardRect(corners, prim.m_pos, viewMatrix,
                    prim.m_texture.size() > 1 ? prim.m_texture[1] : nullptr,
                    prim.m_texture.empty() ? nullptr : prim.m_texture[0],
                    gi.alphaB, gi.flag[0] == 2 ? 255 : 225, gi.flag[0] == 2 ? 155 : 255,
                    gi.flag[0] == 2 ? 0 : 225, D3DBLEND_INVSRCALPHA, static_cast<int>(prim.m_renderFlag));
            }
        }
    }
}

void SubmitRainbowRect(int& rpCount, const vector3d& vec1, const vector3d& vec2,
    const vector3d& vec3, const vector3d& vec4, const matrix& viewMatrix,
    const vector3d& translation, CTexture* texture, unsigned int color,
    float frame0, float frame1, int renderFlags)
{
    if (rpCount >= 510) return;
    const vector3d positions[2][3] = {
        { AddVec3(vec1, translation), AddVec3(vec2, translation), AddVec3(vec3, translation) },
        { AddVec3(vec4, translation), AddVec3(vec3, translation), AddVec3(vec1, translation) },
    };
    const float uvs[2][3][2] = {
        { { 1.0f, frame0 }, { 1.0f, frame1 }, { 0.0f, frame1 } },
        { { 0.0f, frame0 }, { 0.0f, frame1 }, { 1.0f, frame0 } },
    };
    for (int triangle = 0; triangle < 2; ++triangle) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) return;
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_ONE;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            ProjectReferenceEffectVertex(positions[triangle][vertex], viewMatrix, face->m_verts[vertex]);
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
    rpCount += 2;
}

void RenderRainbow(CEffectPrim& prim, const matrix& viewMatrix)
{
    int rpCount = 0;
    for (int bandIndex = 0; bandIndex < 4; ++bandIndex) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(bandIndex)];
        if (gi.life != 1 || gi.alphaB <= 0) continue;

        const int basicAngle = static_cast<int>(gi.fullDisplayAngle) / 20;
        for (int pass = 0; pass < 7; ++pass) {
            int segment = 0;
            int count = 0;
            if (gi.fullDisplayAngle < 0) continue;
            const double outerScale = 1.0 - static_cast<double>(pass) * 0.050000001;
            const double innerScale = 1.0 - static_cast<double>(pass + 1) * 0.050000001;
            do {
                const int angle0 = (count + gi.rotStart) >= 360 ? count + gi.rotStart - 360 : count + gi.rotStart;
                const int angle1Raw = basicAngle + count + gi.rotStart;
                const int angle1 = angle1Raw >= 360 ? angle1Raw - 360 : angle1Raw;
                const double radius = gi.distance;
                const double cos0 = g_cosTable[angle0], sin0 = g_sinTable[angle0];
                const double cos1 = g_cosTable[angle1], sin1 = g_sinTable[angle1];
                const double orientCos = g_cosTable[gi.riseAngle];
                const double orientSin = g_sinTable[gi.riseAngle];
                // RenderTeiRect(vec2, vector3d, vec4, vec1): next outer, next inner, current inner, current outer.
                const vector3d nextOuter = { static_cast<float>(cos1 * radius * outerScale * orientCos), static_cast<float>(2.0 * sin1 * radius * outerScale), static_cast<float>(cos1 * radius * outerScale * orientSin) };
                const vector3d nextInner = { static_cast<float>(cos1 * radius * innerScale * orientCos), static_cast<float>(2.0 * sin1 * radius * innerScale), static_cast<float>(cos1 * radius * innerScale * orientSin) };
                const vector3d currentInner = { static_cast<float>(cos0 * radius * innerScale * orientCos), static_cast<float>(2.0 * sin0 * radius * innerScale), static_cast<float>(cos0 * radius * innerScale * orientSin) };
                const vector3d currentOuter = { static_cast<float>(cos0 * radius * outerScale * orientCos), static_cast<float>(2.0 * sin0 * radius * outerScale), static_cast<float>(cos0 * radius * outerScale * orientSin) };

                unsigned char red = 255, green = 0, blue = 0;
                switch (pass) {
                case 0: break;
                case 1: red = 255; green = 126; break;
                case 2: red = 255; green = 255; break;
                case 3: red = 0; green = 255; break;
                case 4: red = 0; blue = 255; break;
                case 5: red = 115; green = 50; blue = 200; break;
                case 6: red = 255; blue = 255; break;
                }
                const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24)
                    | (static_cast<unsigned int>(red) << 16) | (static_cast<unsigned int>(green) << 8) | blue;
                CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
                const float frame0 = static_cast<float>(static_cast<double>(segment) * 0.047619049);
                const float frame1 = static_cast<float>(static_cast<double>(segment + 1) * 0.047619049);
                SubmitRainbowRect(rpCount, nextOuter, nextInner, currentInner, currentOuter, viewMatrix, prim.m_pos,
                    texture, color, frame0, frame1, static_cast<int>(prim.m_renderFlag));
                if (++segment >= 21) segment = 20;
                count += basicAngle;
            } while (count <= gi.fullDisplayAngle);
        }
    }
}

void SubmitTripleAttackRect(int& rpCount, const vector3d& bottomPrevious,
    const vector3d& bottomCurrent, const vector3d& topCurrent, const vector3d& topPrevious,
    const matrix& viewMatrix, CTexture* texture, short alpha,
    bool inverseSourceAlpha, unsigned char red, unsigned char green, unsigned char blue,
    int renderFlags)
{
    if (rpCount >= 510) return;
    const vector3d positions[2][3] = {
        { bottomPrevious, bottomCurrent, topCurrent },
        { topPrevious, topCurrent, bottomPrevious },
    };
    const float uvs[2][3][2] = {
        { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } },
        { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } },
    };
    const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(alpha)) << 24)
        | (static_cast<unsigned int>(red) << 16) | (static_cast<unsigned int>(green) << 8) | blue;
    for (int triangle = 0; triangle < 2; ++triangle) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) return;
        face->primType = D3DPT_TRIANGLELIST;
        face->verts = face->m_verts;
        face->numVerts = 3;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = inverseSourceAlpha ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE;
        face->alphaSortKey = 0.0f;
        for (int vertex = 0; vertex < 3; ++vertex) {
            ProjectReferenceEffectVertex(positions[triangle][vertex], viewMatrix, face->m_verts[vertex]);
            face->m_verts[vertex].color = color;
            face->m_verts[vertex].specular = 0xFF000000u;
            face->m_verts[vertex].tu = uvs[triangle][vertex][0];
            face->m_verts[vertex].tv = uvs[triangle][vertex][1];
        }
        g_renderer.AddRP(face, renderFlags);
    }
    rpCount += 2;
}

void RenderTripleAttack(CEffectPrim& prim, const matrix& viewMatrix)
{
    int rpCount = 0;
    for (int bandIndex = 0; bandIndex < 4; ++bandIndex) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(bandIndex)];
        if (gi.life != 1 || gi.alphaB <= 0) continue;
        const float mode = gi.height[4];
        const int orientation = static_cast<short>(static_cast<int>(gi.rotStart) + 90) % 360;
        for (int pass = 0; pass < 2; ++pass) {
            float radius = gi.maxHeight;
            if (pass == 1) radius = mode == 3.0f ? radius + radius : radius + 0.25f;
            vector3d previousBottom{};
            vector3d previousTop{};
            for (int sample = 0; sample <= 5; ++sample) {
                const int angle = sample * 72;
                const double circleCos = g_cosTable[angle];
                const double circleSin = g_sinTable[angle];
                const double rx = static_cast<double>(radius) * circleCos * g_cosTable[orientation];
                const double rz = static_cast<double>(radius) * circleCos * g_sinTable[orientation];
                const double vertical = static_cast<double>(radius) * circleSin - gi.height[0];
                const vector3d currentBottom = {
                    static_cast<float>(gi.sparkX + rx), static_cast<float>(gi.sparkY + vertical), static_cast<float>(gi.sparkZ + rz) };
                const vector3d currentTop = {
                    static_cast<float>(gi.vecTNowX + rx), static_cast<float>(gi.vecTNowY + vertical), static_cast<float>(gi.vecTNowZ + rz) };
                if (sample > 0) {
                    unsigned char red = 255, green = 255, blue = 55;
                    if (mode == 2.0f) {
                        red = green = blue = 255;
                    } else if (mode == 3.0f) {
                        red = 255; green = 89; blue = 182;
                    } else if (pass == 1) {
                        blue = 200;
                    }
                    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
                    SubmitTripleAttackRect(rpCount, previousBottom, currentBottom, currentTop, previousTop,
                        viewMatrix, texture, gi.alphaB, pass == 0, red, green, blue,
                        static_cast<int>(prim.m_renderFlag));
                }
                previousBottom = currentBottom;
                previousTop = currentTop;
            }
        }
    }
}

void RenderGroundSample(CEffectPrim& prim, const matrix& viewMatrix)
{
    for (int bandIndex = 0; bandIndex < 4; ++bandIndex) {
        EffectGi& gi = prim.m_gi[static_cast<size_t>(bandIndex)];
        if (gi.life != 1) continue;

        CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        CGameMode* worldMode = g_modeMgr.GetCurrentGameMode();
        C3dGround* ground = worldMode->m_world->m_ground;
        int attrX = 0, attrY = 0;
        gameMode->GetServerCoor(prim.m_pos.x, prim.m_pos.z, attrX, attrY);
        attrY -= 3;
        attrX -= 3;
        const float sinRotation = g_sinTable[gi.rotStart];
        const float cosRotation = g_cosTable[gi.rotStart];
        CTexture* texture = nullptr;

        for (int i = 0; i < 7; ++i) {
            const double row0 = static_cast<double>(i) - 3.5;
            const double row1 = static_cast<double>(i + 1) - 3.5;
            for (int j = 0; j < 7; ++j) {
                const double col0 = static_cast<double>(j) - 3.5;
                const double col1 = static_cast<double>(j + 1) - 3.5;
                const vector2d rawUv[4] = {
                    { static_cast<float>(row0 * sinRotation + col0 * cosRotation), static_cast<float>(row0 * cosRotation - col0 * sinRotation) },
                    { static_cast<float>(row0 * sinRotation + col1 * cosRotation), static_cast<float>(row0 * cosRotation - col1 * sinRotation) },
                    { static_cast<float>(row1 * sinRotation + col0 * cosRotation), static_cast<float>(row1 * cosRotation - col0 * sinRotation) },
                    { static_cast<float>(row1 * sinRotation + col1 * cosRotation), static_cast<float>(row1 * cosRotation - col1 * sinRotation) },
                };
                vector2d uv[4] = { rawUv[0], rawUv[1], rawUv[2], rawUv[3] };
                for (vector2d& point : uv) {
                    point.x = static_cast<float>((point.x * gi.maxHeight + 3.5) * 0.14285715);
                    point.y = static_cast<float>((point.y * gi.maxHeight + 3.5) * 0.14285715);
                }
                texture = g_texMgr.GetTexture("effect\\magic_target.tga", false);
                ground->RenderGroundTile(viewMatrix, attrX + j, attrY + i, 0xAA9999FFu, texture, uv);

                const double outerScale = static_cast<double>(gi.maxHeight) + 0.1;
                for (int corner = 0; corner < 4; ++corner) {
                    uv[corner].x = static_cast<float>((outerScale * rawUv[corner].x + 3.5) * 0.14285715);
                    uv[corner].y = static_cast<float>((outerScale * rawUv[corner].y + 3.5) * 0.14285715);
                }
                texture = g_texMgr.GetTexture("effect\\magic_target.tga", false);
                ground->RenderGroundTile(viewMatrix, attrX + j, attrY + i, 0x559999FFu, texture, uv);
            }
        }
    }
}

short GroundSampleShort(int value)
{
    const int bits = value & 0xFFFF;
    return static_cast<short>(bits < 0x8000 ? bits : bits - 0x10000);
}

void UpdateGroundSample(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;
        const short previous = gi.rotStart;
        short next = GroundSampleShort(static_cast<int>(previous) + 1);
        const short before359 = GroundSampleShort(static_cast<int>(previous) - 359);
        const short subtract360 = GroundSampleShort(static_cast<int>(next) - 360);
        const bool overflow = next < 0 && subtract360 >= 0;
        gi.rotStart = next;
        if ((before359 < 0) == overflow) {
            next = subtract360;
            gi.rotStart = next;
        }
        gi.maxHeight = static_cast<float>(static_cast<double>(g_sinTable[gi.rotStart]) * 0.5 + 1.5);
    }
}

void TeiHit1Corners(const EffectGi& gi, vector3d corners[4])
{
    const double height = gi.height[1];
    const double offset = static_cast<double>(gi.flag[1]) * height + gi.distance;
    const int fullAngle = gi.fullDisplayAngle;
    const int riseAngle = gi.riseAngle;
    const double cosineFull = g_cosTable[fullAngle];
    const double sineFull = g_sinTable[fullAngle];
    const double cosineRise = g_cosTable[riseAngle];
    const double sineRise = g_sinTable[riseAngle];
    const double leftX = cosineFull * offset - sineFull * height;
    const double leftY = cosineFull * height + sineFull * offset;
    const double rightOffset = static_cast<double>(-static_cast<int>(gi.flag[1])) * height + gi.distance;
    const double rightX = cosineFull * rightOffset - sineFull * height;
    const double rightY = cosineFull * height + sineFull * rightOffset;
    const double rightLowerX = cosineFull * rightOffset + sineFull * height;
    const double rightLowerY = sineFull * rightOffset - cosineFull * height;
    const double leftLowerX = cosineFull * offset + sineFull * height;
    const double leftLowerY = sineFull * offset - cosineFull * height;
    const double xValues[4]{leftLowerX, leftX, rightX, rightLowerX};
    const double yValues[4]{leftLowerY, leftY, rightY, rightLowerY};
    for (int i = 0; i < 4; ++i) {
        const float flatX = static_cast<float>(xValues[i]);
        corners[i].x = static_cast<float>(static_cast<double>(flatX) * cosineRise);
        corners[i].y = static_cast<float>(yValues[i]);
        corners[i].z = static_cast<float>(sineRise * flatX);
    }
}

struct CloudRenderStyle {
    unsigned int rgb=0;
    int texturePart=0;
    bool reverse=false;
    bool valid=false;
};

CloudRenderStyle CloudStyle(const EffectGi& gi)
{
    const bool reverse=true;
    switch (gi.flag[2]) {
    case 0: return {0x00FFFFFFu,1,false,true};
    case 1: return {0x009696FAu,0,false,true};
    case 2: return {0x008282FAu,0,reverse,true};
    case 3: return {0x00C8C864u,0,reverse,true};
    case 4: return {0x00FFFFFFu,1,reverse,true};
    case 7: return {0x00FFFFFFu,0,reverse,true};
    case 8: return {0x0087FF87u,1,reverse,true};
    case 9:
        switch (gi.flag[3]) {
        case 1: return {0x00FF8787u,1,reverse,true};
        case 2: return {0x00FFCD87u,1,reverse,true};
        case 3: return {0x00FFFF87u,1,reverse,true};
        case 4: return {0x00FFFFFFu,1,reverse,true};
        case 5: return {0x008787FFu,1,reverse,true};
        case 6: return {0x00FFFFFFu,0,reverse,true};
        default: return {};
        }
    case 10: return {0x00FAFA96u,0,reverse,true};
    case 11: return {0x00DCFADCu,0,reverse,true};
    case 12: return {0x00FCAB8Fu,1,reverse,true};
    case 13: return {0x00FA6464u,0,reverse,true};
    case 14: return {0x005E0000u,1,reverse,true};
    case 15: return {0x00FFC8FFu,1,false,true};
    case 16: return {0x00000000u,1,reverse,true};
    case 17: return {0x00FFB4B4u,1,reverse,true};
    default: return {};
    }
}

void CloudCorners(EffectGi& gi, vector3d corners[4])
{
    int phase = 0;
    if (gi.height[8] >= 0.0f && gi.height[8] < 360.0f) {
        phase = static_cast<int>(gi.height[8]);
    } else {
        gi.height[8] = 0.0f;
    }
    const double pulseRadius = static_cast<double>(gi.distance)
        + static_cast<double>(g_sinTable[phase]) * gi.distance * 0.050000001;
    int angle = gi.fullDisplayAngle + 90;
    if (angle >= 360) {
        angle -= 360;
    }
    for (int i=0;i<4;++i) {
        corners[i].x = static_cast<float>(static_cast<double>(g_cosTable[angle]) * pulseRadius);
        corners[i].y = 0.0f;
        corners[i].z = static_cast<float>(static_cast<double>(g_sinTable[angle]) * pulseRadius);
        angle += 90;
        if (angle >= 360) {
            angle -= 360;
        }
    }
    if (gi.height[9] == 10.0f) {
        for (int i=0;i<4;++i) {
            corners[i].x = static_cast<float>(static_cast<double>(corners[i].x) + 4.0);
        }
    }
}

bool IsCloudRendererType(EFFECTPRIMID type)
{
    switch (type) {
    case PP_CLOUD: case PP_SHIELDBOOMERANG: case PP_TEIHIT2: case PP_BOTTOM:
    case PP_BOTTOMOUT: case PP_THROWITEM: case PP_FIRSTAID: case PP_PARTICLE_UP:
    case PP_PRESSURE: case PP_TANJI2: case PP_SHIELDBOOMERANG3: case PP_TWILIGHT:
    case PP_BUBBLE_DROP: return true;
    default: return false;
    }
}

void RenderReferenceCloud(CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band=0;band<4;++band) {
        EffectGi& gi=prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        vector3d corners[4]{};
        CloudCorners(gi,corners);
        IntimidateTowardScreen(corners,prim.m_gi[0].alphaT,prim.m_gi[1].alphaT);
        for (vector3d& corner : corners) {
            corner.x=static_cast<float>(static_cast<double>(corner.x)+gi.sparkX);
            corner.y=static_cast<float>(static_cast<double>(corner.y)+gi.sparkY);
            corner.z=static_cast<float>(static_cast<double>(corner.z)+gi.sparkZ);
        }
        const CloudRenderStyle style=CloudStyle(prim.m_gi[0]);
        if (!style.valid) {
            continue;
        }
        const unsigned int color=(static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB))<<24)|style.rgb;
        SubmitCloudRect(corners,viewMatrix,texture,color,style.texturePart,style.reverse,gi.flag[0]!=0,
            static_cast<int>(prim.m_renderFlag));
    }
}

void RenderTeiHit1(const CEffectPrim& prim, const matrix& viewMatrix)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (int band = 0; band < 4; ++band) {
        const EffectGi& gi = prim.m_gi[static_cast<size_t>(band)];
        if (gi.life != 1 || gi.alphaB <= 0) {
            continue;
        }
        vector3d corners[4]{};
        TeiHit1Corners(gi, corners);
        int texturePart = 0;
        unsigned int tint = 0x00E1E150u;
        switch (gi.flag[0]) {
        case 0:
            break;
        case 1:
            tint = 0x00FFFFFFu;
            break;
        default:
            texturePart = 1;
            tint = 0x00C8C8FFu;
            break;
        }
        const unsigned int color = (static_cast<unsigned int>(static_cast<unsigned short>(gi.alphaB)) << 24) | tint;
        SubmitTeiHit1Rect(corners, prim.m_pos, viewMatrix, texture, color, texturePart,
            static_cast<int>(prim.m_renderFlag));
    }
}

// Ref/RagEffectPrim.cpp:5679, 6377. The world owns primitive objects;
// the parent's list only tracks attachments.
// HighPriest.exe.c:451706-451754 (0x625C30). Counter belongs to OnProcess.
void UpdateSlash1Primitive(CEffectPrim* prim)
{
    prim->m_pos = prim->m_master->EffectPosition();
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;
        ++gi.process;
        gi.height[0] += 1.0f;
        if (gi.height[0] > 10.0f) {
            gi.alphaB = static_cast<short>(gi.alphaB - 2);
            if (gi.alphaB < 0) { gi.alphaB = 0; gi.life = 0; }
        }
        if (gi.flag[0] == 1) {
            gi.distance += 1.0f;
            if (gi.maxHeight < 2.0f) gi.maxHeight = static_cast<float>(gi.maxHeight + 0.2);
            if (gi.process <= 5) gi.alphaB = static_cast<short>(gi.alphaB + 5);
        } else {
            gi.distance += 2.0f;
            if (gi.maxHeight < 4.0f) gi.maxHeight = static_cast<float>(gi.maxHeight + 0.40000001);
            if (gi.process <= 7) gi.alphaB = static_cast<short>(gi.alphaB + 5);
        }
    }
}

// HighPriest.exe.c:447104-447220 (0x620210). Three adjacent radial ribbons
// per live band, with the center ribbon taller and more opaque than the sides.
void RenderSlash1Primitive(const CEffectPrim& prim, const matrix& view)
{
    CTexture* texture = prim.m_texture.empty() ? nullptr : prim.m_texture[0];
    for (const EffectGi& gi : prim.m_gi) {
        if (gi.life != 1 || gi.alphaB <= 0) continue;
        for (int sample = 0; sample <= 2; ++sample) {
            int angle = gi.rotStart - 1 + sample;
            if (angle < 0) angle += 360;
            else if (angle >= 360) angle -= 360;
            const float cosine = GetCos(angle), sine = GetSin(angle);
            const float innerZ = gi.height[0] * sine;
            const double outerRadius = static_cast<double>(gi.distance) + gi.height[0];
            const float outerZ = static_cast<float>(sine * outerRadius);
            const float innerY = static_cast<float>(-gi.maxHeight * (sample == 1 ? 0.2 : 0.1));
            const float outerY = sample == 1 ? -gi.maxHeight : static_cast<float>(-gi.maxHeight * 0.40000001);
            const vector3d inner{ gi.height[0] * cosine, 0.0f, innerZ };
            const vector3d outer{ static_cast<float>(outerRadius * cosine), 0.0f, outerZ };
            const short alpha = sample == 1 ? gi.alphaB : static_cast<short>(gi.alphaB / 3);
            const u32 color = (static_cast<u32>(static_cast<u8>(alpha)) << 24) | 0xFFFFFFu;
            SubmitWorldTeiRect(AddVec3(prim.m_pos, inner), AddVec3(prim.m_pos, outer),
                AddVec3(prim.m_pos, {outer.x, outerY, outer.z}), AddVec3(prim.m_pos, {inner.x, innerY, inner.z}),
                view, texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag));
        }
    }
}

// HighPriest.exe.c:434160-434328. Preserve the GI clock, signed byte
// controls, alpha overshoot and fade order; the outer processor owns frame count.
void UpdateSuperAngelPrimitive(CEffectPrim* prim)
{
    prim->m_pos = prim->m_master->EffectPosition();
    EffectGi& gi = prim->m_gi[0];
    if (gi.life == 1) {
        ++gi.process;
        gi.sparkY = static_cast<float>(gi.sparkY - 0.1);
        gi.vecBPreX = gi.sparkX; gi.arcBaselineY = gi.sparkY; gi.vecBPreZ = gi.sparkZ;
        prim->m_deltaPos = {gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ};
        const int variant = static_cast<signed char>(gi.flag[0]);
        const int birth = static_cast<signed char>(gi.flag[1]);
        if (gi.process < 50) {
            if (birth != 0) {
                if (prim->m_alpha < 40 - birth) prim->m_alpha += 1.0f;
            } else if (variant == 1) {
                if (prim->m_alpha < 200.0f) prim->m_alpha += 5.0f;
            } else if (variant == 10) {
                if (prim->m_alpha < 255.0f) prim->m_alpha += 10.0f;
                if (prim->m_alpha > 255.0f) prim->m_alpha = 255.0f;
            } else if (variant == 11) {
                prim->m_alpha = 0.0f;
            } else if (variant < 2) {
                if (prim->m_alpha < 245.0f) prim->m_alpha += 5.0f;
            } else if (gi.process < 30 && prim->m_alpha < 150.0f) {
                prim->m_alpha += 5.0f;
            }
        }
        if (gi.process > 30) {
            if (variant == 10 || variant == 11) {
                if ((gi.process & 3) == 0) ++prim->m_curMotion;
                if (prim->m_curMotion > 15) prim->m_curMotion = 15;
            } else if (static_cast<u8>(variant) <= 1 || variant >= 2) {
                if (gi.process % 3 == 0) ++prim->m_curMotion;
                if (prim->m_curMotion > 29) prim->m_curMotion = 29;
            }
        }
        float fade = 0.0f;
        switch (variant) {
        case 0: if (gi.process > 100) fade = 4.0f; break;
        case 2: if (gi.process > 30) fade = 3.0f; break;
        case 3: if (gi.process > 60) fade = 3.0f; break;
        case 4: if (gi.process > 70) fade = 3.0f; break;
        case 10: if (gi.process > 100) fade = 6.0f; break;
        default: if (gi.process > 75) fade = birth != 0 ? 3.0f : 6.0f; break;
        }
        if (fade != 0.0f) {
            prim->m_alpha -= fade;
            if (prim->m_alpha < 0.0f) prim->m_alpha = 0.0f;
        }
    }
    prim->m_pos = prim->m_deltaPos;
    ShiftCircleSegments(prim);
}

// Ref/RagEffectPrim.cpp:7-40. Called after segment storage, before the
// outer primitive counter increment. Frame zero advances the motion too.
bool AnimateReferenceParticle(CEffectPrim* prim)
{
    if (prim->m_animSpeed == 0 || prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    ++prim->m_curMotion;
    if (!prim->m_particleActRes) return true;
    const int count = prim->m_particleActRes->GetMotionCount(prim->m_spriteAction);
    if (count <= 0) return true;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= count;
    } else if ((prim->m_pattern & 0x800) != 0) {
        if (prim->m_curMotion == count) --prim->m_curMotion;
    } else if (prim->m_curMotion == count) {
        return false;
    }
    return true;
}

// Ref/RagEffectPrim.cpp:2184-2328, ordinary/keyed alpha and speed keys.
void ProcessReferenceParticleAlpha(CEffectPrim* prim)
{
    if (prim->m_AlphaShow) {
        if (prim->m_chCnt < 0 || prim->m_chCnt >= static_cast<int>(prim->m_chPoint.size())) prim->m_chCnt = 0;
        const size_t key = static_cast<size_t>(prim->m_chCnt);
        if (key < prim->m_chPoint.size() && key < prim->m_chVal1.size() && key < prim->m_chVal2.size()
            && static_cast<double>(prim->m_stateCnt) == prim->m_chPoint[key]) {
            prim->m_alpha = prim->m_chVal1[key];
            prim->m_maxAlpha = prim->m_chVal2[key];
            ++prim->m_chCnt;
        }
    } else if (prim->m_chSpeed) {
        const size_t key = static_cast<size_t>(prim->m_chCnt);
        if (key < prim->m_chPoint.size() && key < prim->m_chVal1.size()
            && static_cast<double>(prim->m_stateCnt) == prim->m_chPoint[key]) {
            prim->m_alphaSpeed = prim->m_chVal1[key];
            ++prim->m_chCnt;
        }
    }
    if ((prim->m_pattern & 0x80) != 0) {
        if (prim->m_alphaSpeed < 0.0f) {
            if (prim->m_alpha <= prim->m_minAlpha) prim->m_alphaSpeed *= -1.0f;
        } else if (prim->m_alpha >= prim->m_maxAlpha) {
            prim->m_alphaSpeed *= -1.0f;
        }
    }
    const int remaining = prim->m_master->GetDuration() - prim->m_master->GetStateCount();
    if (!prim->m_isDisappear && prim->m_duration - prim->m_stateCnt > remaining + 1) {
        prim->m_isDisappear = true;
        prim->m_pattern &= ~0x80;
        prim->m_alphaSpeed = static_cast<float>(-(prim->m_alpha / static_cast<double>(remaining)));
    }
    if (prim->m_stateCnt == prim->m_fadeOutCnt) {
        prim->m_pattern &= ~0x80;
        prim->m_alphaSpeed = static_cast<float>(-(prim->m_alpha
            / static_cast<double>(prim->m_duration - prim->m_stateCnt) * prim->m_alphaDelta));
    }
    prim->m_alpha += prim->m_alphaSpeed;
    if (prim->m_alpha < prim->m_minAlpha) prim->m_alpha = prim->m_minAlpha;
    if (prim->m_alpha > prim->m_maxAlpha) prim->m_alpha = prim->m_maxAlpha;
    prim->m_argb = PackColor(static_cast<u8>(static_cast<long long>(prim->m_alpha)), prim->m_tintColor);
}

// Ref/RagEffectPrim.cpp:602-671 (0x642AB0), original gravity particle.
bool UpdateReferenceGravityParticle(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);
    const double integratedSpeed = static_cast<double>(prim->m_accel) + prim->m_speed;
    const double speed = integratedSpeed < 0.0 ? 0.0 : integratedSpeed;
    prim->m_speed = static_cast<float>(speed);
    prim->m_gravSpeed += prim->m_gravAccel;
    prim->m_speed3d = {
        static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos.x += prim->m_speed3d.x;
    prim->m_deltaPos.y += prim->m_gravSpeed;
    prim->m_deltaPos.z += prim->m_speed3d.z;
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_size += prim->m_sizeSpeed;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (prim->m_gravSpeed > 0.0f && mode && mode->m_world && mode->m_world->m_attr
        && mode->m_world->m_attr->GetHeight(prim->m_pos.x, prim->m_pos.z) < prim->m_pos.y) {
        prim->m_gravSpeed = static_cast<float>(prim->m_gravSpeed * -0.66666669);
        if (prim->m_playWaveType == 1) {
            char waveName[56];
            std::snprintf(waveName, sizeof(waveName), "effect\\EF_Coin%d.wav", rand() % 3 + 1); // 0x71D44C
            if (mode->m_world->m_player) {
                const vector3d& listener = mode->m_world->m_player->m_pos;
                PlayWave(waveName, prim->m_pos.x - listener.x, 0.0f, prim->m_pos.z - listener.z, 250, 40, 1.0f);
            }
        }
    }
    ProcessReferenceParticleAlpha(prim);
    ShiftCircleSegments(prim);
    return AnimateReferenceParticle(prim);
}

// Ref/RagEffectPrim.cpp:77-224 (0x641760), source cases 0 and 2:
// screen/absolute anchors, dimension stop, alpha bounce, RGB and drift.
bool UpdateReferenceScreenTexture(CEffectPrim* prim)
{
    if (prim->m_stateCnt == prim->m_changePoint) {
        if (prim->m_changeWidthSpeed != 0.000099999997f) prim->m_widthSpeed = prim->m_changeWidthSpeed;
        if (prim->m_changeHeightSpeed != 0.000099999997f) prim->m_heightSpeed = prim->m_changeHeightSpeed;
        if (prim->m_changeSpeed != 0.000099999997f) prim->m_speed = prim->m_changeSpeed;
        if (prim->m_changeWidthAccel != 0.000099999997f) prim->m_widthAccel = prim->m_changeWidthAccel;
        if (prim->m_changeHeightAccel != 0.000099999997f) prim->m_heightAccel = prim->m_changeHeightAccel;
        if (prim->m_changeAccel != 0.000099999997f) prim->m_accel = prim->m_changeAccel;
    }
    const int pattern = prim->m_pattern;
    if ((pattern & 4) != 0) {
        prim->m_pos.x = prim->m_deltaPos2.x;
        prim->m_pos.y = prim->m_deltaPos2.y;
    } else {
        const vector3d anchor = prim->m_master->ScreenPosition();
        prim->m_pos.x = anchor.x + prim->m_deltaPos2.x;
        prim->m_pos.y = anchor.y + prim->m_deltaPos2.y;
    }
    prim->m_speed += prim->m_accel;
    const double rollSpeed = static_cast<double>(prim->m_rollAccel) + prim->m_rollSpeed;
    prim->m_rollSpeed = static_cast<float>(rollSpeed);
    prim->m_widthSpeed += prim->m_widthAccel;
    prim->m_heightSpeed += prim->m_heightAccel;
    if ((pattern & 2) != 0 && prim->m_stateCnt >= prim->m_count) {
        prim->m_widthAccel = 0.0f;
        prim->m_widthSpeed = 0.0f;
        prim->m_heightAccel = 0.0f;
        prim->m_heightSpeed = 0.0f;
    }
    if ((pattern & 0x80) != 0) {
        if ((prim->m_alphaSpeed < 0.0f && prim->m_alpha <= prim->m_minAlpha)
            || (prim->m_alphaSpeed >= 0.0f && prim->m_alpha >= prim->m_maxAlpha)) {
            prim->m_alphaSpeed *= -1.0f;
        }
    }
    prim->m_roll = static_cast<float>(rollSpeed + prim->m_roll);
    prim->m_widthSize += prim->m_widthSpeed;
    prim->m_heightSize += prim->m_heightSpeed;
    prim->m_deltaPos.x = static_cast<float>(static_cast<double>(GetSin(static_cast<int>(prim->m_longitude))) * prim->m_speed + prim->m_deltaPos.x);
    const double deltaY = prim->m_deltaPos.y - static_cast<double>(GetCos(static_cast<int>(prim->m_longitude))) * prim->m_speed;
    prim->m_deltaPos.y = static_cast<float>(deltaY);
    prim->m_pos.x += prim->m_deltaPos.x;
    prim->m_pos.y = static_cast<float>(deltaY+prim->m_pos.y);
    prim->m_red += prim->m_redSpeed;
    prim->m_green += prim->m_greenSpeed;
    prim->m_blue += prim->m_blueSpeed;
    if (prim->m_red > 255) prim->m_red = 0; else if (prim->m_red < 0) prim->m_red = 255;
    if (prim->m_green > 255) prim->m_green = 255; else if (prim->m_green < 0) prim->m_green = 0;
    if (prim->m_blue > 255) prim->m_blue = 255; else if (prim->m_blue < 0) prim->m_blue = 0;
    prim->m_tintColor = RGB(static_cast<u8>(prim->m_red), static_cast<u8>(prim->m_green), static_cast<u8>(prim->m_blue));
    if ((pattern & 8) != 0 && prim->m_stateCnt % prim->m_count == 0) {
        prim->m_longitude = static_cast<float>(prim->m_longitude - 8.0 + rand() % 16);
        prim->m_count = rand() % 10 + 10;
    }
    ProcessReferenceParticleAlpha(prim);
    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= prim->m_totalTexture;
        return true;
    }
    return prim->m_curMotion != prim->m_totalTexture;
}

// Ref/RagEffectPrim.cpp:2097-2118 (0x644A80), original sphere state.
void UpdateReferenceSphere(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(),prim->m_deltaPos2);
    prim->m_pos = prim->m_orgPos;
    const double speed = static_cast<double>(prim->m_radiusAccel)+prim->m_radiusSpeed;
    prim->m_radiusSpeed = static_cast<float>(speed);
    prim->m_radius = static_cast<float>(speed+prim->m_radius);
    prim->m_longitude += prim->m_longSpeed;
    ProcessReferenceParticleAlpha(prim);
}

// Ref/RagEffectPrim.cpp:1702-1722 (0x641D70), original 2D circle state.
void UpdateReferenceScreenCircle(CEffectPrim* prim)
{
    const vector3d anchor = prim->m_master->ScreenPosition();
    prim->m_pos.x = anchor.x + prim->m_deltaPos2.x;
    prim->m_pos.y = anchor.y + prim->m_deltaPos2.y;
    const double speed = static_cast<double>(prim->m_radiusAccel) + prim->m_radiusSpeed;
    prim->m_radiusSpeed = static_cast<float>(speed);
    const double radius = speed + prim->m_radius;
    prim->m_radius = static_cast<float>(radius);
    prim->m_size = (prim->m_pattern & 1) == 0 && radius >= prim->m_innerSize
        ? prim->m_innerSize : static_cast<float>(radius);
    ProcessReferenceParticleAlpha(prim);
}

// Ref/RagEffectPrim.cpp:1729-1773 (0x641DF0), original 2D flash state.
void UpdateReferenceFlash(CEffectPrim* prim)
{
    const vector3d anchor = prim->m_master->ScreenPosition();
    prim->m_pos.y = anchor.y + prim->m_deltaPos2.y;
    prim->m_pos.x = anchor.x + prim->m_deltaPos2.x;
    if ((prim->m_pattern & 1) != 0 && prim->m_heightSize <= 0.0f) {
        prim->m_heightSpeed = 0.0f; prim->m_heightAccel = 0.0f;
        const double heightSize = prim->m_heightSize;
        u32 traceValue = 0;
        std::memcpy(&traceValue, &heightSize, sizeof(traceValue));
        Trace("m_heightSize  = %d", static_cast<int>(traceValue));
    }
    if (prim->m_stateCnt == prim->m_changePoint) {
        prim->m_heightSpeed = prim->m_changeHeightSpeed;
        prim->m_longSpeed = prim->m_changeSpeed;
        prim->m_longAccel = prim->m_changeAccel;
    }
    if (prim->m_heightSize < 0.0f) {
        prim->m_heightSize = 0.0f; prim->m_heightSpeed *= -1.0f;
    }
    const double angularSpeed = static_cast<double>(prim->m_longAccel) + prim->m_longSpeed;
    prim->m_longSpeed = static_cast<float>(angularSpeed);
    prim->m_heightSpeed += prim->m_heightAccel;
    prim->m_arcAngle += prim->m_widthSpeed;
    prim->m_longitude = static_cast<float>(angularSpeed + prim->m_longitude);
    prim->m_heightSize += prim->m_heightSpeed;
    ProcessReferenceParticleAlpha(prim);
}

// HighPriest.exe.c:443213; source dispatch case 0x1A has no Ref body.
bool UpdateReferenceMapMagicZone(CEffectPrim* prim)
{
    prim->m_pos = prim->m_master->EffectPosition();
    u32 heightBits = 0;
    std::memcpy(&heightBits, &prim->m_gi[0].height[4], sizeof(heightBits));
    if (heightBits == 1096810496u) {
        const short rotStart = static_cast<short>(prim->m_gi[0].rotStart - 1);
        prim->m_gi[0].rotStart = rotStart;
        if (rotStart < 0) prim->m_gi[0].rotStart = static_cast<short>(rotStart + 360);
    } else {
        const short rotStart = static_cast<short>(prim->m_gi[0].rotStart + 1);
        prim->m_gi[0].rotStart = rotStart;
        if (rotStart >= 360) prim->m_gi[0].rotStart = static_cast<short>(rotStart - 360);
    }

    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    const int motion = ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= prim->m_totalTexture;
        return true;
    }
    return motion != prim->m_totalTexture;
}

// Ref/dhxj/RagEffect2.cpp:9641 (case 0x1B). The four y fields below are
// aliases in the original 180-byte GI payload: B-now=sparkY, B-pre=arcBaselineY,
// T-now=vecTNowY, and T-pre=vecTPreY.
bool UpdateReferenceMapParticle(CEffectPrim* prim)
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    CView* view = mode->m_view;
    const short longitude = static_cast<short>(view->GetCameraLongitude());
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    const short latitude = static_cast<short>(view->GetCameraLatitude());
    short cameraLatitude = static_cast<short>(360 - latitude + 90);
    if (cameraLatitude >= 360) cameraLatitude = static_cast<short>(cameraLatitude - 360);
    prim->m_gi[1].alphaT = cameraLatitude;

    prim->m_pos = prim->m_master->EffectPosition();
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;

        for (int angle = 0; angle < 4; angle += 2) {
            float& current = gi.height[angle];
            float& target = gi.height[angle + 1];
            if (target > current) {
                current += 1.0f;
                if (current > 359.0f) current = 359.0f;
                if (current > target) {
                    const int range = static_cast<int>(current);
                    target = static_cast<float>(range == 0 ? 0 : rand() % range);
                }
            } else {
                current -= 1.0f;
                if (current < 0.0f) current = 0.0f;
                if (current <= target) {
                    const int range = static_cast<int>(current);
                    target = static_cast<float>(359 - (range == 0 ? 0 : rand() % range));
                }
            }
        }
        for (int angle = 4; angle < 8; ++angle) {
            gi.height[angle] -= 1.0f;
            if (gi.height[angle] < 0.0f) gi.height[angle] += 360.0f;
        }

        gi.vecBPreX = GetSin(static_cast<int>(gi.height[4])) * 20.0f;
        gi.vecBPreZ = GetCos(static_cast<int>(gi.height[4])) * 20.0f;
        gi.sparkX = GetSin(static_cast<int>(gi.height[5])) * 20.0f;
        gi.sparkZ = GetCos(static_cast<int>(gi.height[5])) * 20.0f;
        gi.vecTPreX = GetSin(static_cast<int>(gi.height[6])) * 20.0f;
        gi.vecTPreZ = GetCos(static_cast<int>(gi.height[6])) * 20.0f;
        gi.vecTNowX = GetSin(static_cast<int>(gi.height[7])) * 20.0f;
        gi.vecTNowZ = GetCos(static_cast<int>(gi.height[7])) * 20.0f;

        if (gi.arcBaselineY < 0.0f) {
            gi.vecBPreX += 2.5f * GetSin(static_cast<int>(gi.height[0]));
            gi.vecBPreZ += 2.5f * GetSin(static_cast<int>(gi.height[2]));
        }
        if (gi.sparkY < 0.0f) {
            gi.sparkX -= 2.5f * GetSin(static_cast<int>(gi.height[0]));
            gi.sparkZ -= 2.5f * GetSin(static_cast<int>(gi.height[2]));
        }
        if (gi.vecTPreY < 0.0f) {
            gi.vecTPreX += 2.5f * GetSin(static_cast<int>(gi.height[0]));
            gi.vecTPreZ -= 2.5f * GetSin(static_cast<int>(gi.height[2]));
        }
        if (gi.vecTNowY < 0.0f) {
            gi.vecTNowX -= 2.5f * GetSin(static_cast<int>(gi.height[0]));
            gi.vecTNowZ += 2.5f * GetSin(static_cast<int>(gi.height[2]));
        }
    }
    return true;
}

// Ref/dhxj/RagEffect2.cpp:9320 (case 0x1C). The source dispatcher ignores
// the AnimTexture completion result and continues through the shared tick.
bool UpdateReference3DAura(CEffectPrim* prim)
{
    prim->m_pos = prim->m_master->EffectPosition();
    for (int index = 0; index < 2; ++index) {
        const short riseAngle = static_cast<short>(prim->m_gi[index].riseAngle + 3);
        prim->m_gi[index].riseAngle = riseAngle;
        if (riseAngle >= 360) prim->m_gi[index].riseAngle = static_cast<short>(riseAngle - 360);
    }

    if (prim->m_stateCnt % prim->m_animSpeed == 0) {
        const int motion = ++prim->m_curMotion;
        if (prim->m_repeatAnim) {
            prim->m_curMotion = motion % prim->m_totalTexture;
        } else if (motion == prim->m_totalTexture) {
            return false;
        }
    }
    return true;
}

// Ref/dhxj/RagEffect2.cpp:9486 (case 0x1D). Keep the eight angular pairs
// and four falling-vector groups in their source order for deterministic RNG.
bool UpdateReference3DAura2(CEffectPrim* prim)
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    CView* view = mode->m_view;
    const short longitude = static_cast<short>(view->GetCameraLongitude());
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    const short latitude = static_cast<short>(view->GetCameraLatitude());
    short cameraLatitude = static_cast<short>(360 - latitude + 90);
    if (cameraLatitude >= 360) cameraLatitude = static_cast<short>(cameraLatitude - 360);
    prim->m_gi[1].alphaT = cameraLatitude;

    prim->m_pos = prim->m_master->EffectPosition();
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;

        for (int angle = 0; angle < 16; angle += 2) {
            float& current = gi.height[angle];
            float& target = gi.height[angle + 1];
            if (target > current) {
                current += 3.0f;
                if (current > 359.0f) current = 359.0f;
                if (current > target) {
                    const int range = static_cast<int>(current);
                    target = static_cast<float>(range == 0 ? 0 : rand() % range);
                }
            } else {
                current -= 2.0f;
                if (current < 0.0f) current = 0.0f;
                if (current <= target) {
                    const int range = static_cast<int>(current);
                    target = static_cast<float>(359 - (range == 0 ? 0 : rand() % range));
                }
            }
        }

        if (gi.arcBaselineY < 0.0f) {
            gi.vecBPreX += 0.15f * GetSin(static_cast<int>(gi.height[0]));
            gi.vecBPreZ += 0.15f * GetSin(static_cast<int>(gi.height[2]));
            if (gi.flag[0] == 11) {
                if (gi.arcBaselineY < -50.0f) {
                    gi.vecBPreX = static_cast<float>(rand() % 15 - 8);
                    gi.vecBPreZ = static_cast<float>(rand() % 15 - 8);
                    gi.arcBaselineY = static_cast<float>(rand() % 100);
                }
            } else if (gi.arcBaselineY < -30.0f) {
                gi.vecBPreX = 0.0f;
                gi.vecBPreZ = 0.0f;
                gi.arcBaselineY = static_cast<float>(rand() % 100);
            }
        }
        gi.arcBaselineY -= gi.maxHeight;

        if (gi.sparkY < 0.0f) {
            gi.sparkX -= 0.15f * GetSin(static_cast<int>(gi.height[4]));
            gi.sparkZ -= 0.15f * GetSin(static_cast<int>(gi.height[6]));
            if (gi.sparkY < -30.0f) {
                if (gi.flag[0] == 11) {
                    gi.sparkX = static_cast<float>(rand() % 15 - 8);
                    gi.sparkZ = static_cast<float>(rand() % 15 - 8);
                } else {
                    gi.sparkX = 0.0f;
                    gi.sparkZ = 0.0f;
                }
                gi.sparkY = static_cast<float>(rand() % 100);
            }
        }
        gi.sparkY -= gi.maxHeight;

        if (gi.vecTPreY < 0.0f) {
            gi.vecTPreX += 0.15f * GetSin(static_cast<int>(gi.height[8]));
            gi.vecTPreZ -= 0.15f * GetSin(static_cast<int>(gi.height[10]));
            if (gi.vecTPreY < -30.0f) {
                if (gi.flag[0] == 11) {
                    gi.vecTPreX = static_cast<float>(rand() % 15 - 8);
                    gi.vecTPreZ = static_cast<float>(rand() % 15 - 8);
                } else {
                    gi.vecTPreX = 0.0f;
                    gi.vecTPreZ = 0.0f;
                }
                gi.vecTPreY = static_cast<float>(rand() % 100);
            }
        }
        gi.vecTPreY -= gi.maxHeight;

        if (gi.vecTNowY < 0.0f) {
            gi.vecTNowX -= 0.15f * GetSin(static_cast<int>(gi.height[12]));
            gi.vecTNowZ += 0.15f * GetSin(static_cast<int>(gi.height[14]));
            if (gi.vecTNowY < -30.0f) {
                if (gi.flag[0] == 11) {
                    gi.vecTNowX = static_cast<float>(rand() % 15 - 8);
                    gi.vecTNowZ = static_cast<float>(rand() % 15 - 8);
                } else {
                    gi.vecTNowX = 0.0f;
                    gi.vecTNowZ = 0.0f;
                }
                gi.vecTNowY = static_cast<float>(rand() % 100);
            }
        }
        gi.vecTNowY -= gi.maxHeight;
    }
    return true;
}

// Ref/dhxj/RagEffect2.cpp:22806 (case 0x20).
bool UpdateReferenceChookGi(CEffectPrim* prim)
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    CView* view = mode->m_view;
    const short longitude = static_cast<short>(view->GetCameraLongitude());
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    const short latitude = static_cast<short>(view->GetCameraLatitude());
    short cameraLatitude = static_cast<short>(360 - latitude + 90);
    if (cameraLatitude >= 360) cameraLatitude = static_cast<short>(cameraLatitude - 360);
    prim->m_gi[1].alphaT = cameraLatitude;
    prim->m_pos = prim->m_master->EffectPosition();

    EffectGi& gi = prim->m_gi[0];
    if (gi.life != 1) return true;

    ++gi.process;
    if (gi.process <= 200) ++gi.alphaB;

    for (int angle = 0; angle < 6; angle += 2) {
        float& current = gi.height[angle];
        float& target = gi.height[angle + 1];
        if (target > current) {
            current += 1.0f;
            if (current > 359.0f) current = 359.0f;
            if (current > target) {
                const int range = static_cast<int>(current);
                target = static_cast<float>(range == 0 ? 0 : rand() % range);
            }
        } else {
            current -= 1.0f;
            if (current < 0.0f) current = 0.0f;
            if (current <= target) {
                const int range = static_cast<int>(current);
                target = static_cast<float>(359 - (range == 0 ? 0 : rand() % range));
            }
        }
    }

    if (gi.flag[0] == 0 || gi.process % 2 == 0) {
        gi.height[6] += 1.0f;
        if (gi.height[6] >= 360.0f) gi.height[6] -= 360.0f;
    }

    if (gi.height[10] == 1.0f) {
        gi.sparkX = 0.7f * GetSin(static_cast<int>(gi.height[0]));
        gi.sparkZ = 0.7f * GetSin(static_cast<int>(gi.height[2]));
        gi.sparkY = 0.7f * GetSin(static_cast<int>(gi.height[4]));
        gi.sparkX += 5.5f * GetSin(static_cast<int>(gi.height[6]));
        gi.sparkZ += 5.5f * GetCos(static_cast<int>(gi.height[6]));
    } else {
        const float radialScale = gi.flag[0] == 0 ? 1.0f : 0.5f;
        gi.sparkX = radialScale * GetSin(static_cast<int>(gi.height[0]));
        gi.sparkZ = radialScale * GetSin(static_cast<int>(gi.height[2]));
        gi.sparkY = radialScale * GetSin(static_cast<int>(gi.height[4]));
        gi.sparkX += 7.0f * GetSin(static_cast<int>(gi.height[6]));
        gi.sparkZ += 7.0f * GetCos(static_cast<int>(gi.height[6]));
    }
    gi.sparkY += gi.height[7];

    gi.height[8] += static_cast<float>(5 + rand() % 5);
    if (gi.height[8] >= 360.0f) gi.height[8] -= 360.0f;
    return true;
}

// Ref/dhxj/RagEffect2.cpp:13777 (case 0x22). This updater does not move the
// primitive origin; the source loop only mutates live GI bands.
void UpdateReferenceGi3(CEffectPrim* prim)
{
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;

        ++gi.process;
        gi.distance = static_cast<float>(static_cast<double>(gi.distance) - 0.0099999998);
        if (gi.process < 10) {
            gi.alphaB = static_cast<short>(gi.alphaB + 10);
        } else {
            gi.alphaB = static_cast<short>(gi.alphaB - 1);
            if (gi.alphaB <= 0) {
                gi.alphaB = 0;
                if (gi.process > 30) gi.life = 0;
                gi.distance = 20.0f;
                gi.riseAngle = static_cast<short>(81 + 9);
            }
        }
        for (int index = 0; index < 21; ++index) {
            gi.height[static_cast<size_t>(index)] = gi.maxHeight * g_sinTable[gi.process];
        }
    }
}

// Ref/dhxj/RagEffect2.cpp:10162. Source ids 0x23 and 0x63 share this updater.
bool UpdateReferenceGumGang(CEffectPrim* prim)
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    CView* view = mode->m_view;
    const short longitude = static_cast<short>(view->GetCameraLongitude());
    prim->m_gi[0].alphaT = static_cast<short>(360 - longitude);
    const short latitude = static_cast<short>(view->GetCameraLatitude());
    short cameraLatitude = static_cast<short>(360 - latitude + 90);
    if (cameraLatitude >= 360) cameraLatitude = static_cast<short>(cameraLatitude - 360);
    prim->m_gi[1].alphaT = cameraLatitude;
    prim->m_pos = prim->m_master->EffectPosition();

    constexpr int he = 8;
    constexpr int tn = 5;
    for (EffectGi& gi : prim->m_gi) {
        if (gi.life != 1) continue;
        ++gi.process;
        for (int index = 0; index <= 12; index += 4) {
            gi.height[index] += 1.0f;
            if (gi.height[index] < 10.0f) {
                gi.height[index + 1] += 25.0f;
            } else {
                gi.height[index + 1] -= 12.0f;
                if (gi.height[index + 1] < 12.0f) {
                    gi.height[index] = 0.0f;
                    gi.height[index + 1] = 0.0f;
                    gi.height[index + 3] = static_cast<float>(rand() % 360);
                    gi.height[index + 2] = static_cast<float>(rand() % tn);
                    const float fallingHeight = static_cast<float>(rand() % he);
                    switch (index) {
                    case 0: gi.arcBaselineY = -fallingHeight - 3.0f; break;
                    case 4: gi.sparkY = -fallingHeight - 3.0f; break;
                    case 8: gi.vecTPreY = -fallingHeight - 3.0f; break;
                    default: gi.vecTNowY = -fallingHeight - 3.0f; break;
                    }
                }
            }
        }
    }
    return true;
}

// Ref/RagEffectPrim.cpp:1919-1980 (0x642F10), quad-horn change points, stop and pattern2 restart.
void UpdateReferenceQuadHorn(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(),prim->m_deltaPos2);
    if (prim->m_stateCnt == prim->m_changePoint) {
        prim->m_speed = prim->m_changeSpeed; prim->m_accel = prim->m_changeAccel;
    }
    const double speed = static_cast<double>(prim->m_accel) + prim->m_speed;
    prim->m_speed = static_cast<float>(speed);
    prim->m_speed3d = {
        static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos,prim->m_speed3d);
    prim->m_pos = AddVec3(prim->m_orgPos,prim->m_deltaPos);
    if ((prim->m_pattern & 1) != 0 && prim->m_stateCnt == prim->m_count) {
        prim->m_accel = 0.0f; prim->m_speed = 0.0f;
    }
    if ((prim->m_pattern & 2) != 0) {
        if (prim->m_stateCnt == prim->m_duration-30) {
            prim->m_speed = prim->m_changeSpeedY; prim->m_accel = prim->m_changeAccelY;
        }
    } else {
        ProcessReferenceParticleAlpha(prim);
    }
}

// Ref/RagEffectPrim.cpp:2125-2170 (0x6448e0), shared by source ids 0x0A/0x0B.
void UpdateReferenceSpiral(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);

    const float longitude = prim->m_longitude;
    const float nextLongitude = prim->m_longSpeed + longitude;
    const float x0 = static_cast<float>(prim->m_orgPos.x
        + static_cast<double>(GetSin(static_cast<int>(longitude))) * prim->m_radius);
    const float z0 = static_cast<float>(prim->m_orgPos.z
        - static_cast<double>(GetCos(static_cast<int>(longitude))) * prim->m_radius);
    const float x1 = static_cast<float>(prim->m_orgPos.x
        + static_cast<double>(GetSin(static_cast<int>(nextLongitude))) * prim->m_radius);
    const float z1 = static_cast<float>(prim->m_orgPos.z
        - static_cast<double>(GetCos(static_cast<int>(nextLongitude))) * prim->m_radius);
    const float y0 = prim->m_orgPos.y;
    prim->m_segPos[0] = {x0, y0, z0};
    const float y1 = y0 + prim->m_heightSpeed;
    prim->m_segPos[1] = {x1, y1, z1};
    const float y2 = y0 - prim->m_size;
    prim->m_segPos[2] = {x0, y2, z0};
    const float y3 = y0 + prim->m_heightSpeed - prim->m_size;
    prim->m_segPos[3] = {x1, y3, z1};

    ProcessReferenceParticleAlpha(prim);
    ShiftReferenceSegments(prim);
    prim->m_longitude = prim->m_longSpeed + prim->m_longitude;
    prim->m_deltaPos2.y = prim->m_deltaPos2.y + prim->m_heightSpeed;
}

// Ref/RagEffectPrim.cpp:966-1310 (0x643340), source cases 0x0F/0x10.
bool UpdateReference3DTexture(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);
    const float sentinel = 0.000099999997f;
    if (prim->m_changePoint == prim->m_stateCnt) {
        prim->m_isChangeProcessed = true;
        if (prim->m_changeWidthSpeed != sentinel) prim->m_widthSpeed = prim->m_changeWidthSpeed;
        if (prim->m_changeHeightSpeed != sentinel) prim->m_heightSpeed = prim->m_changeHeightSpeed;
        if (prim->m_changeSpeed != sentinel) prim->m_speed = prim->m_changeSpeed;
        if (prim->m_changeWidthAccel != sentinel) prim->m_widthAccel = prim->m_changeWidthAccel;
        if (prim->m_changeHeightAccel != sentinel) prim->m_heightAccel = prim->m_changeHeightAccel;
        if (prim->m_changeAccel != sentinel) prim->m_accel = prim->m_changeAccel;
        if (prim->m_changeSpeed3d.x != sentinel) prim->m_speed3d.x = prim->m_changeSpeed3d.x;
        if (prim->m_changeSpeed3d.y != sentinel) prim->m_speed3d.y = prim->m_changeSpeed3d.y;
        if (prim->m_changeSpeed3d.z != sentinel) prim->m_speed3d.z = prim->m_changeSpeed3d.z;
        prim->m_changeWidthSpeed = prim->m_changeHeightSpeed = prim->m_changeSpeed = sentinel;
        prim->m_changeWidthAccel = prim->m_changeHeightAccel = prim->m_changeAccel = sentinel;
        prim->m_changeSpeed3d = {sentinel, sentinel, sentinel};
        prim->m_changeAccel3d = {sentinel, sentinel, sentinel};
    }

    prim->m_speed3d = AddVec3(prim->m_speed3d, prim->m_deltaPosAccel);
    const int pattern = prim->m_pattern;
    prim->m_widthSpeed += prim->m_widthAccel;
    prim->m_heightSpeed += prim->m_heightAccel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_latiSpeed += prim->m_latiAccel;
    prim->m_speed = static_cast<float>(static_cast<double>(prim->m_accel) + prim->m_speed);
    if ((pattern & 0x400) != 0) {
        prim->m_speed3d = {};
        prim->m_speed3d = {
            static_cast<float>(static_cast<double>(prim->m_speed) * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(static_cast<double>(prim->m_speed) * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(static_cast<double>(prim->m_speed) * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
    } else if ((pattern & 0x100) == 0) {
        prim->m_speed3d = {
            static_cast<float>(static_cast<double>(prim->m_speed) * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(static_cast<double>(prim->m_speed) * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(static_cast<double>(prim->m_speed) * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
    }
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    if ((pattern & 0x400) != 0) {
        const vector3d& target = prim->m_master->TargetPosition();
        const double dx = static_cast<double>(target.x) - prim->m_pos.x;
        const double dz = static_cast<double>(target.z) - prim->m_pos.z;
        const float squared = static_cast<float>(dx * dx + dz * dz);
        if (std::sqrt(static_cast<double>(squared)) <= 3.0) {
            prim->m_isHit = true;
            Trace("Hit!!");
            return false;
        }
        prim->m_longitude = prim->m_roty;
    }

    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_widthSize += prim->m_widthSpeed;
    prim->m_heightSize += prim->m_heightSpeed;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_latitude += prim->m_latiSpeed;
    if (prim->m_heightSize > prim->m_maxHeightSize) {
        prim->m_heightAccel = 0.0f;
        prim->m_heightSpeed = 0.0f;
    }

    if ((pattern & 0x200) != 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        MatrixIdentity(prim->m_rotMat);
        if (mode && mode->m_view) MatrixAppendZRotation(prim->m_rotMat, mode->m_view->GetCameraLatitude());
    } else {
        if ((pattern & 0x40) != 0) {
            CGameMode* mode = g_modeMgr.GetCurrentGameMode();
            if (mode && mode->m_view) {
                prim->m_longitude = mode->m_view->GetCameraLongitude() + 90.0f;
                prim->m_roll = mode->m_view->GetCameraLatitude();
            }
            MatrixIdentity(prim->m_rotMat);
            MatrixAppendXRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_latitude)));
            MatrixAppendZRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_roll)));
        } else {
            MatrixIdentity(prim->m_rotMat);
            MatrixAppendZRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_roll)));
            MatrixAppendXRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_latitude)));
        }
        prim->m_roty = prim->m_longitude;
    }
    MatrixAppendYRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_roty)));
    if ((pattern & 2) != 0) {
        const int count = prim->m_count++;
        const double v = static_cast<double>(count) * 0.1;
        prim->m_v1 = static_cast<float>(v);
        prim->m_v2 = static_cast<float>(v + 1.0);
        if (count == 9) prim->m_count = 0;
    }
    if ((pattern & 0x10) != 0) {
        prim->m_master->AbsorbPrimDelta(AddVec3(prim->m_deltaPos, prim->m_deltaPos2));
    }
    if ((pattern & 4) != 0) {
        CGameMode* currentMode = g_modeMgr.GetCurrentGameMode();
        if (currentMode && currentMode->m_world && currentMode->m_world->m_attr
            && currentMode->m_world->m_attr->GetHeight(prim->m_pos.x, prim->m_pos.z) < prim->m_pos.y) {
            prim->m_speed = 0.0f;
            prim->m_stateCnt = prim->m_duration - 25;
            prim->m_pattern &= ~4;
            prim->m_master->SetGroundHitFlag();
        }
    }
    if ((pattern & 0x20) != 0 && prim->m_stateCnt % prim->m_any == 0) {
        prim->m_heightSpeed *= -1.0f;
    }
    if ((pattern & 0x800) != 0
        && (prim->m_red == 254 || prim->m_blue == 254 || prim->m_green == 254)) {
        if (prim->m_rgbCycleDelay != 0) ++prim->m_rgbCycleCnt;
        if (prim->m_rgbCycleDelay == prim->m_rgbCycleCnt) {
            prim->m_rgbCycleCnt = 0;
            prim->m_red += prim->m_redSpeed;
            prim->m_green += prim->m_greenSpeed;
            prim->m_blue += prim->m_blueSpeed;
        }
    } else {
        prim->m_red += prim->m_redSpeed;
        prim->m_green += prim->m_greenSpeed;
        prim->m_blue += prim->m_blueSpeed;
    }
    if ((pattern & 0x800) != 0) {
        if (prim->m_red > 254) prim->m_red = 0; else if (prim->m_red < 0) prim->m_red = 254;
        if (prim->m_green > 254) prim->m_green = 0; else if (prim->m_green < 0) prim->m_green = 254;
        if (prim->m_blue > 254) prim->m_blue = 0; else if (prim->m_blue < 0) prim->m_blue = 254;
    } else {
        if (prim->m_red > 255) prim->m_red = 0; else if (prim->m_red < 0) prim->m_red = 255;
        if (prim->m_green > 255) prim->m_green = 0; else if (prim->m_green < 0) prim->m_green = 0;
        if (prim->m_blue > 255) prim->m_blue = 255; else if (prim->m_blue < 0) prim->m_blue = 0;
    }
    ProcessReferenceParticleAlpha(prim);
    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    const int motion = ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion = motion % prim->m_totalTexture;
        return true;
    }
    return motion != prim->m_totalTexture;
}

// The source OnProcess cases 0x0F/0x10 ignore this return and still tick.
bool UpdateReferenceCrossTexture(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);
    const double speed = static_cast<double>(prim->m_accel)+prim->m_speed;
    prim->m_speed = static_cast<float>(speed);
    prim->m_speed3d = {
        static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    if (prim->m_type == PP_3DTEXTURE) {
        MatrixIdentity(prim->m_rotMat);
        MatrixAppendZRotation(prim->m_rotMat,GetRadian(static_cast<int>(prim->m_roll)));
        MatrixAppendXRotation(prim->m_rotMat,GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_rotMat,GetRadian(static_cast<int>(prim->m_longitude)));
        if ((prim->m_pattern & 2) != 0) {
            const int count = prim->m_count++;
            const double v = count*0.1;
            prim->m_v1 = static_cast<float>(v); prim->m_v2 = static_cast<float>(v+1.0);
            if (count == 9) prim->m_count = 0;
        }
    }
    if ((prim->m_pattern & 0x10) != 0) {
        prim->m_master->AbsorbPrimDelta(AddVec3(prim->m_deltaPos, prim->m_deltaPos2));
    }
    if ((prim->m_pattern & 4) != 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_attr
            && mode->m_world->m_attr->GetHeight(prim->m_pos.x, prim->m_pos.z) < prim->m_pos.y) {
            prim->m_speed = 0.0f;
            prim->m_stateCnt = prim->m_duration - 25;
            prim->m_pattern &= ~4;
            prim->m_master->SetGroundHitFlag();
        }
    }
    ProcessReferenceParticleAlpha(prim);
    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= prim->m_totalTexture;
        return true;
    }
    return prim->m_curMotion != prim->m_totalTexture;
}

// Ref/RagEffectPrim.cpp:288-365 (0x6443E0), used by source case 0x12 and Portal.
// The source label says CylinderHalf; the body is shared with its duplicate.
bool UpdateReferenceCylinderState(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);
    prim->m_speed += prim->m_accel;
    prim->m_outerSpeed += prim->m_outerAccel;
    prim->m_innerSpeed += prim->m_innerAccel;
    prim->m_heightSpeed += prim->m_heightAccel;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    const double move = -static_cast<double>(prim->m_speed);
    prim->m_speed3d = {
        static_cast<float>(move * prim->m_matrix.m[1][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(move * prim->m_matrix.m[1][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(move * prim->m_matrix.m[1][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_longitude += prim->m_longSpeed;
    prim->m_outerSize += prim->m_outerSpeed;
    prim->m_innerSize += prim->m_innerSpeed;
    prim->m_heightSize += prim->m_heightSpeed;
    if (prim->m_heightSize > prim->m_maxHeightSize) prim->m_heightSize = prim->m_maxHeightSize;
    if ((prim->m_pattern & 1) != 0 && prim->m_stateCnt == prim->m_fadeOutCnt) {
        prim->m_outerSpeed = static_cast<float>(-(prim->m_outerSize
            / static_cast<double>(prim->m_duration - prim->m_stateCnt + 1)));
    }
    ProcessReferenceParticleAlpha(prim);
    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= prim->m_totalTexture;
        return true;
    }
    return prim->m_curMotion != prim->m_totalTexture;
}

// Ref/RagEffectPrim.cpp:1780-1852. ReadyPortal/Blessing/WarpZone2 have no change
// points; preserve their reference state without terrain-height substitution.
bool UpdateReferenceCircleState(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);
    prim->m_speed += prim->m_accel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_innerSpeed += prim->m_innerAccel;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_innerSize += prim->m_innerSpeed;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_speed3d = {
        prim->m_speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
        prim->m_speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
        prim->m_speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_size = ((prim->m_pattern & 1) != 0 || prim->m_radius < prim->m_innerSize) ? prim->m_radius : prim->m_innerSize;
    ProcessReferenceParticleAlpha(prim);
    ShiftCircleSegments(prim);
    return true;
}

// Ref/RagEffectPrim.cpp:816-891 (0x642CE0), reachable WarpZone/WarpZone2 profiles.
// It has no change point or keyed radius schedule. Position is computed before
// the raw-radian Z rotation, then alpha/segments precede motion advancement.
bool UpdateWarpZoneOrbit(CEffectPrim* prim)
{
    prim->m_orgPos = prim->m_master->EffectPosition();
    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_gravSpeed += prim->m_gravAccel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_deltaPos2.y += prim->m_gravSpeed;
    prim->m_orgPos = AddVec3(prim->m_orgPos, prim->m_deltaPos2);
    prim->m_roll += prim->m_rollSpeed;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_deltaPos = {
        prim->m_radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
        prim->m_radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
        prim->m_radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
    };
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    prim->m_size += prim->m_sizeSpeed;
    prim->m_latitude += prim->m_latiSpeed;
    MatrixAppendZRotation(prim->m_matrix, prim->m_latitude);
    ProcessReferenceParticleAlpha(prim);
    ShiftCircleSegments(prim);
    return AnimateReferenceParticle(prim);
}

// Ref/RagEffectPrim.cpp:456-594,2184-2328. Poison, Ruwach, Smoke and Torch use
// linear PP_3DPARTICLE state with no change points or movement-pattern bits.
// Blessing adds the 0x80 alpha-twinkle pattern handled by the shared alpha port.
// FireBall adds 0x100 target termination and 0x20 camera-relative roll.
// FireFly adds periodic 0x4 steering and keyed alpha/max-alpha changes.
// Torch's 0x200 bit selects clip color only. Other callers still need their
// additional pattern/control branches.
bool UpdateLinearReferenceParticle(CEffectPrim* prim)
{
    prim->m_orgPos = AddVec3(prim->m_master->EffectPosition(), prim->m_deltaPos2);
    const double speed = static_cast<double>(prim->m_accel) + prim->m_speed;
    prim->m_speed = static_cast<float>(speed);
    prim->m_sizeSpeed += prim->m_sizeAccel;
    prim->m_speed3d = {
        static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
    prim->m_roll += prim->m_rollSpeed;
    prim->m_size += prim->m_sizeSpeed;
    if ((prim->m_pattern & 4) != 0 && prim->m_stateCnt % prim->m_count == 0) {
        prim->m_speed = static_cast<float>(static_cast<double>(rand() % 35 + 15) * 0.0099999998);
        prim->m_longitude = static_cast<float>(prim->m_longitude - 40.0 + rand() % 80);
        prim->m_latitude = static_cast<float>(prim->m_latitude - 40.0 + rand() % 80);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        prim->m_count = rand() % 10 + 5;
    }
    prim->m_pos = AddVec3(prim->m_orgPos, prim->m_deltaPos);
    if ((prim->m_pattern & 0x100) != 0) {
        const vector3d& target = prim->m_master->TargetPosition();
        const double dx = static_cast<double>(target.x) - prim->m_pos.x;
        const double dz = static_cast<double>(target.z) - prim->m_pos.z;
        const float squared = static_cast<float>(dz * dz + dx * dx);
        if (std::sqrt(static_cast<double>(squared)) <= 3.0) return false;
    }
    if ((prim->m_pattern & 0x20) != 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_view) prim->m_roll = prim->m_roty - mode->m_view->GetCameraLongitude();
    }
    ProcessReferenceParticleAlpha(prim);
    ShiftCircleSegments(prim);
    return AnimateReferenceParticle(prim);
}

// Ref/RagEffectPrim.cpp:678-808. PP_3DPARTICLE_NOMASTER moves from its
// stored origin and has distinct change-point and pattern-bit semantics.
bool UpdateReferenceNoMasterParticle(CEffectPrim* prim)
{
    const vector3d origin = AddVec3(prim->m_orgPos, prim->m_deltaPos2);
    if (prim->m_stateCnt == prim->m_changePoint) {
        prim->m_speed3d = prim->m_changeSpeed3d;
        prim->m_deltaPosAccel = prim->m_changeAccel3d;
    }

    const double speed = static_cast<double>(prim->m_accel) + prim->m_speed;
    prim->m_speed = static_cast<float>(speed);
    prim->m_sizeSpeed += prim->m_sizeAccel;
    if ((prim->m_pattern & 0x400) != 0) {
        prim->m_speed3d = AddVec3(prim->m_speed3d, prim->m_deltaPosAccel);
        prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
        prim->m_roll += prim->m_rollSpeed;
        prim->m_size += prim->m_sizeSpeed;
    } else if ((prim->m_pattern & 1) != 0 || prim->m_stateCnt > prim->m_fadeInCnt) {
        prim->m_speed3d = {
            static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
        prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);
        prim->m_roll += prim->m_rollSpeed;
        prim->m_size += prim->m_sizeSpeed;
    }

    if ((prim->m_pattern & 2) != 0 && prim->m_stateCnt % prim->m_count == 0) {
        prim->m_count = rand() % 8 + 12;
        prim->m_deltaPos3.x = static_cast<float>((rand() % 5 + 5) * 0.0099999998);
        prim->m_deltaPos3.z = static_cast<float>((rand() % 5 + 5) * 0.0099999998);
        const auto reverseDirection = []() {
            const int value = rand() & 0x80000001;
            if (value == 0) return true;
            return value < 0
                && (((static_cast<int>(static_cast<unsigned char>(value)) - 1) | 0xFFFFFFFE) == -1);
        };
        if (reverseDirection()) prim->m_deltaPos3.x *= -1.0f;
        if (reverseDirection()) prim->m_deltaPos3.z *= -1.0f;
        prim->m_deltaPos.x += prim->m_deltaPos3.x;
    }
    if ((prim->m_pattern & 4) != 0 && prim->m_stateCnt % prim->m_count == 0) {
        prim->m_speed = static_cast<float>((rand() % 35 + 15) * 0.0099999998);
        prim->m_longitude = static_cast<float>(prim->m_longitude - 40.0 + rand() % 80);
        prim->m_latitude = static_cast<float>(prim->m_latitude - 40.0 + rand() % 80);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        prim->m_count = rand() % 10 + 5;
    }
    if ((prim->m_pattern & 8) != 0 && prim->m_stateCnt % prim->m_count == 0) {
        prim->m_longitude = static_cast<float>(prim->m_longitude - 5.0 + rand() % 10);
        prim->m_latitude = static_cast<float>(prim->m_latitude - 5.0 + rand() % 10);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_count = rand() % 15 + 5;
    }

    prim->m_pos = AddVec3(origin, prim->m_deltaPos);
    if ((prim->m_pattern & 0x20) != 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_view) prim->m_roll = prim->m_roty - mode->m_view->GetCameraLongitude();
    }
    ProcessReferenceParticleAlpha(prim);
    ShiftCircleSegments(prim);
    return AnimateReferenceParticle(prim);
}

// Ref/RagEffectPrim.cpp:1316-1651. This texture primitive keeps its own
// stored origin and source-specific collision, orientation, RGB and motion state.
bool UpdateReferenceNoMasterTexture(CEffectPrim* prim)
{
    const vector3d origin = AddVec3(prim->m_orgPos, prim->m_deltaPos2);
    if (prim->m_stateCnt == prim->m_changePoint) {
        const float sentinel = 0.000099999997f;
        if (prim->m_changeWidthSpeed != sentinel) prim->m_widthSpeed = prim->m_changeWidthSpeed;
        if (prim->m_changeHeightSpeed != sentinel) prim->m_heightSpeed = prim->m_changeHeightSpeed;
        if (prim->m_changeSpeed != sentinel) prim->m_speed = prim->m_changeSpeed;
        if (prim->m_changeWidthAccel != sentinel) prim->m_widthAccel = prim->m_changeWidthAccel;
        if (prim->m_changeHeightAccel != sentinel) prim->m_heightAccel = prim->m_changeHeightAccel;
        if (prim->m_changeAccel != sentinel) prim->m_accel = prim->m_changeAccel;
        if (prim->m_changeSpeed3d.x != sentinel) prim->m_speed3d.x = prim->m_changeSpeed3d.x;
        if (prim->m_changeSpeed3d.y != sentinel) prim->m_speed3d.y = prim->m_changeSpeed3d.y;
        if (prim->m_changeSpeed3d.z != sentinel) prim->m_speed3d.z = prim->m_changeSpeed3d.z;
        if (prim->m_changeAccel3d.x != sentinel) prim->m_deltaPosAccel.x = prim->m_changeAccel3d.x;
        if (prim->m_changeAccel3d.y != sentinel) prim->m_deltaPosAccel.y = prim->m_changeAccel3d.y;
        if (prim->m_changeAccel3d.z != sentinel) prim->m_deltaPosAccel.z = prim->m_changeAccel3d.z;
    }

    prim->m_speed3d = AddVec3(prim->m_speed3d, prim->m_deltaPosAccel);
    const int pattern = prim->m_pattern;
    prim->m_widthSpeed += prim->m_widthAccel;
    prim->m_heightSpeed += prim->m_heightAccel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_latiSpeed += prim->m_latiAccel;
    const double speed = static_cast<double>(prim->m_accel) + prim->m_speed;
    prim->m_speed = static_cast<float>(speed);
    if ((pattern & 0x400) != 0) {
        prim->m_speed3d = {};
        prim->m_speed3d = {
            static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
    } else if ((pattern & 0x100) == 0) {
        prim->m_speed3d = {
            static_cast<float>(speed * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(speed * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(speed * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
    }
    prim->m_deltaPos = AddVec3(prim->m_deltaPos, prim->m_speed3d);

    if ((pattern & 0x400) != 0) {
        const vector3d& target = prim->m_master->TargetPosition();
        const double dx = static_cast<double>(target.x) - prim->m_pos.x;
        const double dz = static_cast<double>(target.z) - prim->m_pos.z;
        const float squared = static_cast<float>(dx * dx + dz * dz);
        if (std::sqrt(static_cast<double>(squared)) <= 3.0) {
            prim->m_isHit = true;
            Trace("Hit!!");
            return false;
        }
        prim->m_longitude = prim->m_roty;
    }

    prim->m_pos = AddVec3(origin, prim->m_deltaPos);
    prim->m_widthSize += prim->m_widthSpeed;
    prim->m_heightSize += prim->m_heightSpeed;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_latitude += prim->m_latiSpeed;
    if (prim->m_heightSize > prim->m_maxHeightSize) {
        prim->m_heightAccel = 0.0f;
        prim->m_heightSpeed = 0.0f;
    }

    if ((pattern & 0x200) != 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        MatrixIdentity(prim->m_rotMat);
        if (mode && mode->m_view) MatrixAppendZRotation(prim->m_rotMat, mode->m_view->GetCameraLatitude());
    } else {
        if ((pattern & 0x40) != 0) {
            CGameMode* mode = g_modeMgr.GetCurrentGameMode();
            if (mode && mode->m_view) {
                prim->m_longitude = mode->m_view->GetCameraLongitude() + 90.0f;
                prim->m_roll = mode->m_view->GetCameraLatitude();
            }
            MatrixIdentity(prim->m_rotMat);
            MatrixAppendXRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_latitude)));
            MatrixAppendZRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_roll)));
        } else {
            MatrixIdentity(prim->m_rotMat);
            MatrixAppendZRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_roll)));
            MatrixAppendXRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_latitude)));
        }
        prim->m_roty = prim->m_longitude;
    }
    MatrixAppendYRotation(prim->m_rotMat, GetRadian(static_cast<int>(prim->m_roty)));

    if ((pattern & 2) != 0) {
        const int count = prim->m_count++;
        const double v = static_cast<double>(count) * 0.1;
        prim->m_v1 = static_cast<float>(v);
        prim->m_v2 = static_cast<float>(v + 1.0);
        if (count == 9) prim->m_count = 0;
    }
    if ((pattern & 0x10) != 0) {
        prim->m_master->AbsorbPrimDelta(AddVec3(prim->m_deltaPos2, prim->m_deltaPos));
    }
    if ((pattern & 4) != 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_attr
            && mode->m_world->m_attr->GetHeight(prim->m_pos.x, prim->m_pos.z) < prim->m_pos.y) {
            prim->m_speed = 0.0f;
            prim->m_pattern &= ~4;
            prim->m_stateCnt = prim->m_duration - 25;
            prim->m_master->SetGroundHitFlag();
        }
    }
    if ((prim->m_pattern & 0x20) != 0 && prim->m_stateCnt % prim->m_any == 0) {
        prim->m_heightSpeed *= -1.0f;
    }

    if ((prim->m_pattern & 0x800) != 0
        && (prim->m_red == 254 || prim->m_blue == 254 || prim->m_green == 254)) {
        if (prim->m_rgbCycleDelay != 0) ++prim->m_rgbCycleCnt;
        if (prim->m_rgbCycleDelay == prim->m_rgbCycleCnt) {
            prim->m_rgbCycleCnt = 0;
            prim->m_red += prim->m_redSpeed;
            prim->m_green += prim->m_greenSpeed;
            prim->m_blue += prim->m_blueSpeed;
        }
    } else {
        prim->m_red += prim->m_redSpeed;
        prim->m_green += prim->m_greenSpeed;
        prim->m_blue += prim->m_blueSpeed;
    }
    if ((prim->m_pattern & 0x800) != 0) {
        if (prim->m_red > 254) prim->m_red = 0; else if (prim->m_red < 0) prim->m_red = 254;
        if (prim->m_green > 254) prim->m_green = 0; else if (prim->m_green < 0) prim->m_green = 254;
        if (prim->m_blue > 254) prim->m_blue = 0; else if (prim->m_blue < 0) prim->m_blue = 254;
    } else {
        if (prim->m_red > 255) prim->m_red = 0; else if (prim->m_red < 0) prim->m_red = 255;
        if (prim->m_green > 255) prim->m_green = 255; else if (prim->m_green < 0) prim->m_green = 0;
        if (prim->m_blue > 255) prim->m_blue = 255; else if (prim->m_blue < 0) prim->m_blue = 0;
    }
    prim->m_tintColor = RGB(static_cast<u8>(prim->m_red), static_cast<u8>(prim->m_green), static_cast<u8>(prim->m_blue));
    ProcessReferenceParticleAlpha(prim);
    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    const int motion = ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion = motion % prim->m_totalTexture;
        return true;
    }
    return motion != prim->m_totalTexture;
}

// Ref/RagEffectPrim.cpp:233-281. This orbit is anchored to the parent's
// projected screen coordinates and has its own texture-motion completion.
bool UpdateReference2DTextureOrbit(CEffectPrim* prim)
{
    const vector3d anchor = prim->m_master->ScreenPosition();
    prim->m_orgPos.x = anchor.x + prim->m_deltaPos2.x;
    prim->m_orgPos.y = anchor.y + prim->m_deltaPos2.y;

    prim->m_radiusSpeed += prim->m_radiusAccel;
    prim->m_longSpeed += prim->m_longAccel;
    prim->m_widthSpeed += prim->m_widthAccel;
    prim->m_heightSpeed += prim->m_heightAccel;
    prim->m_rollSpeed += prim->m_rollAccel;
    prim->m_radius += prim->m_radiusSpeed;
    prim->m_longitude += prim->m_longSpeed;
    prim->m_roll += prim->m_rollSpeed;
    prim->m_widthSize += prim->m_widthSpeed;
    prim->m_heightSize += prim->m_heightSpeed;
    prim->m_pos.x = GetSin(static_cast<int>(prim->m_longitude)) * prim->m_radius + prim->m_orgPos.x;
    prim->m_pos.y = prim->m_orgPos.y - GetCos(static_cast<int>(prim->m_longitude)) * prim->m_radius;

    ProcessReferenceParticleAlpha(prim);
    if (prim->m_stateCnt % prim->m_animSpeed != 0) return true;
    const int motion = ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion = motion % prim->m_totalTexture;
        return true;
    }
    return motion != prim->m_totalTexture;
}

// Ref/GameActor.cpp:10748-10928. Each sprite retains its own texture and
// palette; clip color/zoom/rotation are never baked into a motion canvas.
struct ParticleSprite {
    SprImg* image;
    float left, top, right, bottom;
    float oow, oowup;
    u32 color;
    float angle;
};

CTexture* ResolveParticleTexture(SprImg& image, const unsigned int* palette)
{
    if (image.tex) return image.tex;
    if (image.width <= 0 || image.height <= 0) return nullptr;
    const size_t count = static_cast<size_t>(image.width) * image.height;
    std::vector<unsigned int> pixels;
    if (!image.indices.empty()) {
        if (image.indices.size() < count) return nullptr;
        pixels.resize(count);
        for (size_t i = 0; i < count; ++i) {
            const unsigned char index = image.indices[i];
            pixels[i] = index == 0 ? 0u : 0xFF000000u | (palette[index] & 0xFFFFFFu);
        }
    } else {
        if (image.rgba.size() < count) return nullptr;
        pixels.assign(image.rgba.begin(), image.rgba.begin() + count);
    }
    CTexture* texture = g_texMgr.CreateTexture(image.width, image.height, pixels.data(), PF_A8R8G8B8, true);
    if (!texture || texture == &CTexMgr::s_dummy_texture) return nullptr;
    image.tex = texture; // Owned and released by CSprRes::Reset.
    return texture;
}

void SubmitParticleSprite(const ParticleSprite& sprite, const unsigned int* palette, int renderFlag)
{
    CTexture* texture = ResolveParticleTexture(*sprite.image, palette);
    if (!texture) return;
    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) return;
    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = renderFlag == 5 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0;
    // The current texture backend stores physical upload extents separately
    // when upscaling; these represent Ref's m_updateWidth/m_updateHeight.
    const float u1 = static_cast<float>(texture->m_surfaceUpdateWidth) / texture->m_w;
    const float v1 = static_cast<float>(texture->m_surfaceUpdateHeight) / texture->m_h;
    const float width = (sprite.right - sprite.left) * 0.5f;
    const float height = (sprite.bottom - sprite.top) * 0.5f;
    const float centerX = width + sprite.left;
    const float centerY = height + sprite.top;
    // Ref truncates the summed clip/primitive angle before table lookup.
    const float cosine = sprite.angle == 0.0f ? 1.0f : GetCos(static_cast<int>(sprite.angle));
    const float sine = sprite.angle == 0.0f ? 0.0f : GetSin(static_cast<int>(sprite.angle));
    for (int i = 0; i < 4; ++i) {
        tlvertex3d& vertex = face->m_verts[i];
        if (sprite.angle == 0.0f) {
            vertex.x = ((i & 1) ? sprite.right : sprite.left) + 0.5f;
            vertex.y = ((i & 2) ? sprite.bottom : sprite.top) + 0.5f;
        } else {
            const float x = (i & 1) ? width : -width;
            const float y = (i & 2) ? height : -height;
            vertex.x = x * cosine - y * sine + centerX + 0.5f;
            vertex.y = y * cosine + x * sine + centerY + 0.5f;
        }
        vertex.oow = (i & 2) ? sprite.oow : sprite.oowup;
        vertex.z = static_cast<float>(1500.0 / (1500.0 - 10.0)
            * (1.0 / vertex.oow - 10.0) * vertex.oow);
        vertex.color = sprite.color;
        vertex.specular = 0xFF000000u;
        vertex.tu = (i & 1) ? u1 : 0.0f;
        vertex.tv = (i & 2) ? v1 : 0.0f;
    }
    g_renderer.AddRP(face, renderFlag);
}

// Ref/RagEffectPrim.cpp:3939-4072 (0x645A20). The decompiler's
// v36[1].m_lastTlvertY aliases m_pattern, not m_renderFlag.
void RenderParticleSprites(const CEffectPrim& prim, CSprRes& sprites, const CActRes& actions,
    const matrix& view, float slope)
{
    for (int j = 0; j < prim.m_numSegments && j < static_cast<int>(prim.m_segments.size()); ++j) {
        const EffectSegmentState& segment = prim.m_segments[static_cast<size_t>(j)];
        const vector3d& pos = segment.pos;
        const float z = pos.z * view.m[2][2] + view.m[1][2] * pos.y + view.m[0][2] * pos.x + view.m[3][2];
        const float oow = static_cast<float>(1.0 / z);
        const float x = static_cast<float>((view.m[2][0] * pos.z + view.m[1][0] * pos.y
            + pos.x * view.m[0][0] + view.m[3][0]) * (1.0 / z) * g_renderer.m_hpc + g_renderer.m_xoffset);
        const float y = (view.m[2][1] * pos.z + view.m[1][1] * pos.y
            + view.m[0][1] * pos.x + view.m[3][1]) * g_renderer.m_vpc * oow + g_renderer.m_yoffset;
        const float ratio = static_cast<float>(static_cast<double>(oow) * g_renderer.m_hpc * 0.14285715);
        const CMotion* motion = actions.GetMotion(prim.m_spriteAction, prim.m_curMotion);
        if (!motion) continue;
        for (const CSprClip& clip : motion->sprClips) {
            const SprImg* image = sprites.GetSprite(clip.clipType, clip.sprIndex);
            if (!image) continue;
            const float top = static_cast<float>(static_cast<double>(segment.size) * clip.y * ratio * clip.zoomY + y);
            const float bottom = static_cast<float>(static_cast<double>(clip.y + image->height * (image->isHalfH + 1))
                * ratio * segment.size * clip.zoomY + y);
            if (!(g_renderer.m_height > top && bottom > 0.0f)) continue;
            const int endX = clip.x + image->width * (image->isHalfW + 1);
            const double left = static_cast<double>(segment.size) * ((clip.flags & 1) ? endX : clip.x) * clip.zoomX * ratio + x;
            const float right = static_cast<float>(static_cast<double>((clip.flags & 1) ? clip.x : endX)
                * segment.size * clip.zoomX * ratio + x);
            if (clip.flags & 1) {
                if (right >= g_renderer.m_width || left <= 0.0) continue;
            } else if (left >= g_renderer.m_width || right <= 0.0f) continue;
            ParticleSprite sprite{};
            sprite.image = const_cast<SprImg*>(image);
            sprite.left = static_cast<float>(left); sprite.top = top;
            sprite.right = right; sprite.bottom = bottom;
            sprite.oow = static_cast<float>(1.0 / ((bottom - y) * slope + z) + 0.0000099999997);
            sprite.oowup = static_cast<float>(1.0 / ((top - y) * slope + z) + 0.0000099999997);
            sprite.color = (prim.m_pattern & 0x200) != 0
                ? (static_cast<u32>(clip.a) << 24) | (static_cast<u32>(clip.r) << 16)
                    | (static_cast<u32>(clip.g) << 8) | clip.b
                : segment.argb;
            sprite.angle = static_cast<float>(static_cast<double>(clip.angle) + prim.m_roll);
            SubmitParticleSprite(sprite, sprites.m_pal, static_cast<int>(prim.m_renderFlag));
        }
    }
}

void RenderReferenceParticlePrimitive(const CEffectPrim& prim, matrix& view)
{
    if (!prim.m_particleSprRes || !prim.m_particleActRes) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || !mode->m_view) return;
    // Ref/View.cpp:286-293 and HighPriest.exe.c g_gradient = 0.16.
    const int latitude = static_cast<int>(-mode->m_view->GetCameraLatitude());
    const float tangent = GetSin(latitude) / GetCos(latitude);
    const float tangenta = 0.16f * tangent;
    const float slope = tangenta / g_renderer.m_screenYFactor;
    RenderParticleSprites(prim, *prim.m_particleSprRes, *prim.m_particleActRes, view, slope);
}

// Ref SetSprName/SetActName retain each caller's resource/lock order.
void CEffectPrim::SetParticleResources(const char* sprName, const char* actName, bool actionFirst)
{
    m_spriteSprName = sprName;
    m_spriteActName = actName;
    const auto loadSprite = [&] {
        if (m_particleSprRes) m_particleSprRes->Unlock();
        m_particleSprRes = g_resMgr.GetAs<CSprRes>(sprName);
        if (m_particleSprRes) m_particleSprRes->Lock();
    };
    const auto loadAction = [&] {
        if (m_particleActRes) m_particleActRes->Unlock();
        m_particleActRes = g_resMgr.GetAs<CActRes>(actName);
        if (m_particleActRes) m_particleActRes->Lock();
    };
    if (actionFirst) { loadAction(); loadSprite(); }
    else { loadSprite(); loadAction(); }
}

bool CEffectPrim::UsesReferenceCircle() const
{
    return m_type == PP_3DCIRCLE && m_master
        && (m_master->GetEffectType() == 17 || m_master->GetEffectType() == 31 || m_master->GetEffectType() == 26 || m_master->GetEffectType() == 35 || m_master->GetEffectType() == 36 || m_master->GetEffectType() == 42 || m_master->GetEffectType() == 48 || m_master->GetEffectType() == 61 || m_master->GetEffectType() == 321);
}

bool CEffectPrim::UsesReferenceTexture() const
{
    return m_type == PP_3DTEXTURE && m_master && m_master->GetEffectType() == 48;
}

bool CEffectPrim::UsesReferenceCylinder() const
{
    if (m_type != PP_3DCYLINDER || !m_master) return false;
    switch (m_master->GetEffectType()) {
    case 0: case 2: case 3: case 14: case 34: case 36: case 81: case 122: case 180: case 184: case 218: case 219: case 220: case 221: return true;
    default: return false;
    }
}

bool CEffectPrim::UsesReferenceCrossTexture() const
{
    return m_type == PP_3DCROSSTEXTURE && m_master
        && (m_master->GetEffectType() == 38 || m_master->GetEffectType() == 37 || m_master->GetEffectType() == 43 || m_master->GetEffectType() == 31 || m_master->GetEffectType() == 26 || m_master->GetEffectType() == 220);
}

bool CEffectPrim::UsesReferenceSphere() const
{
    return m_type == PP_3DSPHERE && m_master && m_master->GetEffectType() == 17;
}

bool CEffectPrim::UsesReferenceScreenCircle() const
{
    return m_type == PP_2DCIRCLE && m_master && (m_master->GetEffectType() == 16 || m_master->GetEffectType() == 40 || m_master->GetEffectType() == 121 || m_master->GetEffectType() == 128);
}

bool CEffectPrim::UsesReferenceFlash() const
{
    return m_type == PP_2DFLASH && m_master && (m_master->GetEffectType() == 16 || m_master->GetEffectType() == 40 || m_master->GetEffectType() == 98);
}

bool CEffectPrim::UsesReferenceQuadHorn() const
{
    return m_type == PP_3DQUADHORN && m_master
        && (m_master->GetEffectType() == 27 || m_master->GetEffectType() == 28 || m_master->GetEffectType() == 123 || m_master->GetEffectType() == 132 || m_master->GetEffectType() == 142);
}

bool CEffectPrim::UsesReferenceScreenTexture() const
{
    return m_type == PP_2DTEXTURE && m_master
        && (m_master->GetEffectType() == 38 || m_master->GetEffectType() == 37 || m_master->GetEffectType() == 43 || m_master->GetEffectType() == 32 || m_master->GetEffectType() == 4 || m_master->GetEffectType() == 5 || m_master->GetEffectType() == 1 || m_master->GetEffectType() == 183 || m_master->GetEffectType() == 46 || m_master->GetEffectType() == 50 || m_master->GetEffectType() == 51);
}

bool CEffectPrim::UsesReferenceScreenQuad() const
{
    return m_type == PP_2DQUAD && m_master && m_master->GetEffectType() == 51;
}

bool CEffectPrim::UsesReferenceParticle() const
{
    return m_type == PP_SUPERANGEL
        || (m_type == PP_3DPARTICLEGRAVITY && m_master
            && (m_master->GetEffectType() == 81 || m_master->GetEffectType() == 180 || m_master->GetEffectType() == 184 || m_master->GetEffectType() == 0 || m_master->GetEffectType() == 10 || m_master->GetEffectType() == 18))
        || (m_type == PP_3DPARTICLEORBIT && m_master
            && (m_master->GetEffectType() == 14 || m_master->GetEffectType() == 98 || m_master->GetEffectType() == 61 || m_master->GetEffectType() == 321))
        || (m_type == PP_3DPARTICLE && m_master
            && (m_master->GetEffectType() == 53 || m_master->GetEffectType() == 39 || m_master->GetEffectType() == 31 || m_master->GetEffectType() == 3 || m_master->GetEffectType() == 2 || m_master->GetEffectType() == 22 || m_master->GetEffectType() == 601 || m_master->GetEffectType() == 81 || m_master->GetEffectType() == 180 || m_master->GetEffectType() == 184 || m_master->GetEffectType() == 0 || m_master->GetEffectType() == 45 || m_master->GetEffectType() == 24 || m_master->GetEffectType() == 20 || m_master->GetEffectType() == 125 || m_master->GetEffectType() == 493
                || m_master->GetEffectType() == 42 || m_master->GetEffectType() == 33 || m_master->GetEffectType() == 44 || m_master->GetEffectType() == 47));
}

CEffectPrim::~CEffectPrim()
{
    if (m_master) {
        m_master->SendMsg(this, 25, 0, 0, 0);
    }
    if (m_particleSprRes) m_particleSprRes->Unlock();
    if (m_particleActRes) m_particleActRes->Unlock();
}

void CEffectPrim::SendMsg(CGameObject*, int msg, msgparam_t, msgparam_t, msgparam_t)
{
    if (msg == 24) {
        m_master = nullptr;
    }
}

// The retained RSW/synthetic-map emitters use legacy band state. Skill and
// packet effects use the reference GI payload for these same canonical ids.
bool CEffectPrim::UsesLegacyBandPayload() const
{
    return m_master && m_master->IsWorldEffect()
        && (m_type == PP_MAPPILLAR || m_type == PP_PORTAL || m_type == PP_WIND);
}

// Ref/RagEffectPrim.cpp:5733-5770, 6198-6206. Unlike effects, primitives
// include their final duration frame. Only the two life-99 texture kinds
// bypass the zero-skip-frame gate.
u8 CEffectPrim::OnProcess()
{
    if (!m_master) {
        return 0;
    }
    // Ref/RagEffectPrim.cpp:5748-5765: copy before the skip-frame gate.
    switch (static_cast<int>(m_type)) {
    case 0x17: case 0x1C: case 0x1D: case 0x1E: case 0x20:
    case 0x23: case 0x3C: case 0x44: case 0x63: case 0x68:
    case 0x69: case 0x6A: case 0x6C: case 0x6D: case 0x6F:
        m_pos = m_master->EffectPosition();
        break;
    default:
        break;
    }
    if (!g_isSkipFrame && !((m_type == PP_2DTEXTURE || m_type == PP_3DCROSSTEXTURE) && m_gi[0].life == 99)) {
        return 1;
    }
    if (!ProcessPrimitiveFrame()) {
        return 0;
    }
    ++m_stateCnt;
    return m_stateCnt <= m_duration;
}

bool CEffectPrim::ProcessPrimitiveFrame()
{
    if (m_type == PP_3DSHINELIGHT) {
        return true;
    }
    if (m_type == PP_3DLINE) {
        m_pos.y += 5.0f;
        return true;
    }
    if (m_type == PP_2DCIRCLE) {
        UpdateReferenceScreenCircle(this);
        return true;
    }
    if (m_type == PP_3DSPHERE) {
        UpdateReferenceSphere(this);
        return true;
    }
    if (m_type == PP_3DPARTICLE_NOMASTER) {
        return UpdateReferenceNoMasterParticle(this);
    }
    if (m_type == PP_3DTEXTURE_NOMASTER) {
        return UpdateReferenceNoMasterTexture(this);
    }
    if (m_type == PP_2DTEXTUREORBIT) {
        return UpdateReference2DTextureOrbit(this);
    }
    if (m_type == PP_3DQUADHORN) {
        UpdateReferenceQuadHorn(this);
        return true;
    }
    if (m_type == PP_3DSPLINE || m_type == PP_3DSPIRAL) {
        UpdateReferenceSpiral(this);
        return true;
    }
    if (m_type == PP_3DTEXTURE || m_type == PP_3DCROSSTEXTURE) {
        UpdateReference3DTexture(this); // Source case 0x0F/0x10 ignores the bool result.
        return true;
    }
    if (m_type == PP_3DCYLINDERHALF) {
        UpdateReferenceCylinderState(this); // Source case 0x12 ignores the bool result.
        return true;
    }
    if (m_type == PP_3DFLASH) {
        UpdateReferenceFlash(this);
        return true;
    }
    if (m_type == PP_MAPMAGICZONE) {
        UpdateReferenceMapMagicZone(this); // Source case 0x1A ignores the bool result.
        return true;
    }
    if (m_type == PP_MAPPARTICLE) {
        UpdateReferenceMapParticle(this); // Source case 0x1B ignores the bool result.
        return true;
    }
    if (m_type == PP_3DAURA) {
        UpdateReference3DAura(this); // Source case 0x1C ignores the bool result.
        return true;
    }
    if (m_type == PP_3DAURA_2) {
        UpdateReference3DAura2(this); // Source case 0x1D ignores the bool result.
        return true;
    }
    if (m_type == PP_CHOOKGI) {
        return UpdateReferenceChookGi(this); // Source tests this return; all helper paths return true.
    }
    if (m_type == PP_GI_3) {
        UpdateReferenceGi3(this);
        return true;
    }
    if (m_type == PP_GUMGANG || m_type == PP_DOUBLEGUMGANG) {
        UpdateReferenceGumGang(this); // Source ids 0x23/0x63 ignore the bool result.
        return true;
    }
    if (m_type == PP_2DTEXTURE || m_type == PP_2DQUAD) {
        return UpdateReferenceScreenTexture(this);
    }
    if (m_type == PP_GROUNDSAMPLE) {
        UpdateGroundSample(this);
        return true;
    }
    if (m_type == PP_SUPERANGEL) {
        UpdateSuperAngelPrimitive(this);
        return true;
    }
    if (m_type == PP_SLASH1) {
        UpdateSlash1Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting)) {
        Update3DCastingPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting3)) {
        Update3DCasting3Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi5)) {
        UpdateGi5Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi1)) {
        UpdateGi1Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy2)) {
        UpdateAttackEnergy2Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric2)) {
        UpdateElectric2Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric)) {
        UpdateElectricPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy)) {
        UpdateAttackEnergyPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeHalfSphere)) {
        UpdateHalfSpherePrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeStormKick)) {
        UpdateStormKickPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi4)) {
        UpdateGi4Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi2)) {
        UpdateGi2Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeMapPillar) && !UsesLegacyBandPayload()) {
        UpdateMapPillarGi(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting2)) {
        UpdateDefenderPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeFlowerCast)) {
        UpdateFlowerCastPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeAcidDemon)) {
        UpdateAcidDemonPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeGiExplosion)) {
        UpdateGiExplosionPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeHeal) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeHealLow)) {
        UpdateHealPrimitive(this);
        if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeHealLow) && m_master != nullptr) {
            m_pos = m_master->ActorPosition();
        }
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeEffectTexture)) {
        UpdateEffectTexturePrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeThrowItem)) {
        UpdateThrowItemPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeTeleport2)) {
        UpdateTeleportation2Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical)) {
        UpdateChemicalPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical2)) {
        UpdateChemical2Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical3)) {
        UpdateChemical3Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeFirstAid)) {
        UpdateFirstAidPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeBottomOut)) {
        UpdateBottomOutPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeBottom)) {
        UpdateBottomPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeParticleUp)) {
        UpdateParticleUpPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeColorPaper)) {
        UpdateColorPaperPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeSphereWind)) {
        UpdateSphereWindPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeMagnum2)) {
        UpdateMagnum2Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeSaintWing)) {
        UpdateSaintWingPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeForest)) {
        UpdateForestPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeRectUp)) {
        UpdateRectUpPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeBlind)) {
        UpdateBlindPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypePoison)) {
        UpdatePoisonPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypePortal) && !UsesLegacyBandPayload()) {
        UpdatePortalPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeWind) && !UsesLegacyBandPayload()) {
        UpdateWindPrimitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting4)) {
        UpdateCasting4Primitive(this);
        return true;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting5)) {
        UpdateCasting5Primitive(this);
        return true;
    }
    if (UsesReferenceCircle()) {
        return UpdateReferenceCircleState(this);
    }
    if (m_type == PP_3DCIRCLE) {
        return UpdateCirclePrimitive(this);
    }
    if (UsesReferenceCylinder()) {
        return UpdateReferenceCylinderState(this);
    }
    if (m_type == PP_3DCYLINDER) {
        return UpdateCylinderPrimitive(this);
    }
    if (m_type == PP_3DPARTICLEORBIT && UsesReferenceParticle()) {
        return UpdateWarpZoneOrbit(this);
    }
    if (m_type == PP_3DPARTICLEORBIT) {
        return UpdateParticleOrbitPrimitive(this);
    }
    if (m_type == PP_3DPARTICLE && UsesReferenceParticle()) {
        return UpdateLinearReferenceParticle(this);
    }
    if (m_type == PP_3DPARTICLE) {
        return UpdateFreeParticlePrimitive(this, false, false);
    }
    if (m_type == PP_3DPARTICLEGRAVITY && UsesReferenceParticle()) {
        return UpdateReferenceGravityParticle(this);
    }
    if (m_type == PP_3DPARTICLEGRAVITY) {
        return UpdateFreeParticlePrimitive(this, true, false);
    }
    if (m_type == PP_3DPARTICLESPLINE) {
        return UpdateFreeParticlePrimitive(this, true, true);
    }
    if (UsesReferenceSphere()) {
        UpdateReferenceSphere(this);
        return true;
    }
    if (UsesReferenceScreenCircle()) {
        UpdateReferenceScreenCircle(this);
        return true;
    }
    if (UsesReferenceFlash()) {
        UpdateReferenceFlash(this);
        return true;
    }
    if (UsesReferenceQuadHorn()) {
        UpdateReferenceQuadHorn(this);
        return true;
    }
    if (UsesReferenceCrossTexture()) {
        return UpdateReferenceCrossTexture(this);
    }
    if (UsesReferenceTexture()) {
        UpdateReferenceCrossTexture(this); // Original OnProcess ignores Prim3DTexture's return.
        return true;
    }
    if (m_type == PP_3DCROSSTEXTURE) {
        return UpdateCrossTexturePrimitive(this);
    }
    if (UsesReferenceScreenTexture() || UsesReferenceScreenQuad()) {
        return UpdateReferenceScreenTexture(this);
    }
    if (m_type == PP_2DFLASH || m_type == PP_2DCIRCLE) {
        return UpdateScreenTexturePrimitive(this);
    }
    if (m_type == PP_PORTAL) {
        UpdatePortalBands(this);
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active; });
    }
    if (m_type == PP_WIND) {
        UpdateWindBands(this);
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active && band.alpha > 0.0f; });
    }
    if (m_type == PP_WATERFALL) {
        return UpdateWaterfallPrimitive(this);
    }
    if (m_type == PP_CLOUD) {
        return UpdateWeatherCloudPrimitive(this);
    }
    if (m_type == PP_MAPPILLAR) {
        return UpdateMapPillarPrimitive(this);
    }
    m_size += m_sizeSpeed;
    m_sizeSpeed += m_sizeAccel;
    m_radius += m_radiusSpeed;
    m_radiusSpeed += m_radiusAccel;
    m_heightSize += m_heightSpeed;
    m_heightSpeed += m_heightAccel;
    m_longitude += m_longSpeed;
    m_gravSpeed += m_gravAccel;

    UpdateAlphaFade(this);

    return m_stateCnt <= m_duration || m_alpha > 0.0f;
}

void CEffectPrim::Render(matrix* viewMatrix)
{
    if (!m_master || !viewMatrix) {
        return;
    }
    if (IsCloudRendererType(m_type)) {
        RenderReferenceCloud(*this, *viewMatrix);
        return;
    }
    if (UsesReferenceSphere()) {
        RenderReferenceSphere(*this,*viewMatrix);
        return;
    }
    if (UsesReferenceScreenCircle()) {
        RenderReferenceScreenCircle(*this);
        return;
    }
    if (UsesReferenceFlash()) {
        RenderReferenceFlash(*this);
        return;
    }
    if (UsesReferenceQuadHorn()) {
        RenderReferenceQuadHorn(*this,*viewMatrix);
        return;
    }
    if (UsesReferenceScreenTexture()) {
        RenderReferenceScreenTexture(*this);
        return;
    }
    if (UsesReferenceScreenQuad()) {
        RenderReferenceScreenQuad(*this);
        return;
    }
    if (UsesReferenceCircle()) {
        RenderReferenceCircle(*this, *viewMatrix);
        return;
    }
    if (UsesReferenceCrossTexture()) {
        RenderReferenceCrossTexture(*this, *viewMatrix);
        return;
    }
    if (UsesReferenceTexture()) {
        RenderReferenceTexture(*this,*viewMatrix);
        return;
    }
    if (UsesReferenceCylinder()) {
        RenderReferenceCylinder(*this, *viewMatrix);
        return;
    }
    if (UsesReferenceParticle()) {
        RenderReferenceParticlePrimitive(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_SLASH1) {
        RenderSlash1Primitive(*this, *viewMatrix);
        return;
    }
    if ((m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting3)
        || m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi5) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi1) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeTeleport2) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeHeal) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeGiExplosion) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeAcidDemon) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeFlowerCast) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting2) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeMapPillar) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi2)) && !UsesLegacyBandPayload()) {
        RenderHealCasting(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_GI_3) {
        RenderGi3(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_GRANDCROSS2) {
        RenderGrandCross2(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_LINELINK) {
        RenderLineLink(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_INTIMIDATE) {
        RenderIntimidate(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_TEIHIT1) {
        RenderTeiHit1(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_GUMGANG) {
        RenderGumGang(*this, *viewMatrix, 1.0f, 100, 100, 255);
        return;
    }
    if (m_type == PP_DOUBLEGUMGANG) {
        RenderDoubleGumGang(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_POKJUK) {
        RenderPokJuk(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_GUARD) {
        RenderGuard(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_RAINBOW) {
        RenderRainbow(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_TRIPLEATTACK) {
        RenderTripleAttack(*this, *viewMatrix);
        return;
    }
    if (m_type == PP_GROUNDSAMPLE) {
        RenderGroundSample(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeHealLow) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi4)) {
        RenderHealLow(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical)) {
        RenderChemical(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical2)) {
        RenderChemical2(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical3)) {
        RenderChemical3(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeEffectTexture)) {
        RenderEffectTexture(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeThrowItem)
        || m_type == static_cast<EFFECTPRIMID>(kPrimTypeFirstAid)
        || m_type == static_cast<EFFECTPRIMID>(kPrimTypeParticleUp)
        || m_type == static_cast<EFFECTPRIMID>(kPrimTypeBottom)
        || m_type == static_cast<EFFECTPRIMID>(kPrimTypeBottomOut)) {
        RenderParticleUp(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeColorPaper)) {
        RenderColorPaper(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeSphereWind)) {
        RenderSphereWind(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeMagnum2)) {
        RenderMagnum2(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeSaintWing)) {
        RenderSaintWing(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeForest)) {
        RenderForest(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeRectUp)) {
        RenderRectUp(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeBlind)) {
        RenderBlind(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypePoison)) {
        RenderPoison(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypePortal) && !UsesLegacyBandPayload()) {
        RenderHealCasting(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy2)) {
        RenderAttackEnergy2(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric2)) {
        RenderElectric2(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeElectric)) {
        RenderElectric(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy)) {
        RenderAttackEnergy(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeHalfSphere)) {
        RenderHalfSphere(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeStormKick)) {
        RenderStormKick(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeWind) && !UsesLegacyBandPayload()) {
        RenderWind(*this, *viewMatrix);
        return;
    }
    if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting4) || m_type == static_cast<EFFECTPRIMID>(kPrimTypeCasting5)) {
        RenderHealCasting(*this, *viewMatrix);
        return;
    }

    const vector3d base = AddVec3(m_master->ResolveBasePosition(), m_deltaPos);
    const float normalizedAlpha = (std::max)(0.0f, (std::min)(255.0f, m_alpha));
    if (normalizedAlpha <= 0.0f
        && m_type != PP_MAPMAGICZONE
        && m_type != PP_MAPPARTICLE
        && m_type != PP_PORTAL
        && m_type != PP_WIND
        && m_type != PP_WATERFALL
        && m_type != PP_MAPPILLAR
        && m_type != PP_CLOUD) {
        return;
    }

    const unsigned int color = PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor);
    const float timeSeconds = static_cast<float>(m_stateCnt) * (kEffectTickMs / 1000.0f);

    switch (m_type) {
    case PP_3DCIRCLE:
    case PP_MAPMAGICZONE: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : nullptr;
        if (m_type == PP_3DCIRCLE && m_numSegments > 1) {
            const int historyCount = (std::min)(m_numSegments, static_cast<int>(m_segments.size()));
            const int renderCount = (std::min)(historyCount, (std::max)(1, m_stateCnt));
            for (int index = renderCount - 1; index >= 0; --index) {
                RenderCircleLayer(*this,
                    m_segments[static_cast<size_t>(index)],
                    *viewMatrix,
                    texture,
                    m_arcAngle);
            }
        } else {
            EffectSegmentState state{};
            state.pos = m_type == PP_3DCIRCLE ? m_pos : vector3d{ base.x, ResolveGroundHeight(base) - 0.05f + m_deltaPos2.y, base.z };
            state.radius = m_radius;
            state.size = (m_pattern & 1) != 0 || m_radius < m_innerSize ? m_radius : m_innerSize;
            state.alpha = normalizedAlpha;
            state.transform = m_type == PP_3DCIRCLE ? m_matrix : matrix{};
            if (m_type != PP_3DCIRCLE) {
                MatrixIdentity(state.transform);
            }
            RenderCircleLayer(*this, state, *viewMatrix, texture, m_arcAngle);
        }
        break;
    }
    case PP_3DCYLINDER: {
        CTexture* texture = !m_texture.empty() ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)] : nullptr;
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        const int stepDegrees = (std::max)(1, static_cast<int>(m_arcAngle));
        float uInc = 0.0f;
        for (int degrees = 0; degrees <= 360 - stepDegrees; degrees += stepDegrees) {
            const int nextDegrees = degrees + stepDegrees;
            const float cs1 = GetCos(degrees);
            const float sn1 = GetSin(degrees);
            const float cs2 = GetCos(nextDegrees);
            const float sn2 = GetSin(nextDegrees);
            const vector3d quad[4] = {
                vector3d{
                    m_pos.x + sn1 * m_innerSize * m_matrix.m[2][0] + cs1 * m_innerSize * m_matrix.m[0][0],
                    m_pos.y + sn1 * m_innerSize * m_matrix.m[2][1] + cs1 * m_innerSize * m_matrix.m[0][1],
                    m_pos.z + sn1 * m_innerSize * m_matrix.m[2][2] + cs1 * m_innerSize * m_matrix.m[0][2]
                },
                vector3d{
                    m_pos.x + sn2 * m_innerSize * m_matrix.m[2][0] + cs2 * m_innerSize * m_matrix.m[0][0],
                    m_pos.y + sn2 * m_innerSize * m_matrix.m[2][1] + cs2 * m_innerSize * m_matrix.m[0][1],
                    m_pos.z + sn2 * m_innerSize * m_matrix.m[2][2] + cs2 * m_innerSize * m_matrix.m[0][2]
                },
                vector3d{
                    m_pos.x + sn1 * m_outerSize * m_matrix.m[2][0] - m_heightSize * m_matrix.m[1][0] + cs1 * m_outerSize * m_matrix.m[0][0],
                    m_pos.y + sn1 * m_outerSize * m_matrix.m[2][1] - m_heightSize * m_matrix.m[1][1] + cs1 * m_outerSize * m_matrix.m[0][1],
                    m_pos.z + sn1 * m_outerSize * m_matrix.m[2][2] - m_heightSize * m_matrix.m[1][2] + cs1 * m_outerSize * m_matrix.m[0][2]
                },
                vector3d{
                    m_pos.x + sn2 * m_outerSize * m_matrix.m[2][0] - m_heightSize * m_matrix.m[1][0] + cs2 * m_outerSize * m_matrix.m[0][0],
                    m_pos.y + sn2 * m_outerSize * m_matrix.m[2][1] - m_heightSize * m_matrix.m[1][1] + cs2 * m_outerSize * m_matrix.m[0][1],
                    m_pos.z + sn2 * m_outerSize * m_matrix.m[2][2] - m_heightSize * m_matrix.m[1][2] + cs2 * m_outerSize * m_matrix.m[0][2]
                },
            };
            if (m_master->GetEffectType() == kRagEffectPortal && m_stateCnt <= 3 && degrees == 0) {
                tlvertex3d projected[4]{};
                const bool ok0 = ProjectPoint(quad[0], *viewMatrix, &projected[0]);
                const bool ok1 = ProjectPoint(quad[1], *viewMatrix, &projected[1]);
                const bool ok2 = ProjectPoint(quad[2], *viewMatrix, &projected[2]);
                const bool ok3 = ProjectPoint(quad[3], *viewMatrix, &projected[3]);
                DbgLog("[RagEffect] cylinder render effect=%d step=%d proj=%d%d%d%d q0=(%.2f,%.2f,%.2f) q2=(%.2f,%.2f,%.2f)\n",
                    m_master->GetEffectType(),
                    m_stateCnt,
                    ok0 ? 1 : 0,
                    ok1 ? 1 : 0,
                    ok2 ? 1 : 0,
                    ok3 ? 1 : 0,
                    quad[0].x,
                    quad[0].y,
                    quad[0].z,
                    quad[2].x,
                    quad[2].y,
                    quad[2].z);
            }
            const unsigned int quadColor = PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor);
            const unsigned int colors[4] = { quadColor, quadColor, quadColor, quadColor };
            const float nextU = uInc + 0.25f;
            const float uvs[4][2] = { { uInc, 1.0f }, { nextU, 1.0f }, { uInc, 0.0f }, { nextU, 0.0f } };
            SubmitWorldQuad(quad, *viewMatrix, texture, colors, uvs, destBlend, 0.0f, renderFlags);
            uInc = nextU >= 1.0f ? 0.0f : nextU;
        }
        break;
    }
    case PP_PORTAL: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(false);
        for (const EffectBandState& band : m_bands) {
            const float groundRadius = (std::max)(0.01f, m_size + band.radius);
            RenderBandLowPolygonTeiRect(*this, base, band, *viewMatrix, texture, groundRadius);
        }
        break;
    }
    case PP_MAPPILLAR: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(false);
        for (const EffectBandState& band : m_bands) {
            if (!band.active || band.alpha <= 0.0f) {
                continue;
            }
            RenderBandLowPolygonTeiRect(*this, base, band, *viewMatrix, texture, (std::max)(0.01f, band.distance));
        }
        break;
    }
    case PP_WIND: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(true);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        for (const EffectBandState& band : m_bands) {
            if (!band.active || band.alpha <= 0.0f) {
                continue;
            }
            const float angle = band.rotStart * (kPi / 180.0f);
            const vector3d pos = {
                base.x + std::cos(angle) * ((std::max)(1.0f, band.distance) * 0.75f),
                ResolveGroundHeight(base) - band.radius + m_deltaPos2.y,
                base.z + std::sin(angle) * ((std::max)(1.0f, band.distance) * 0.75f)
            };
            SubmitBillboard(pos,
                *viewMatrix,
                texture,
                (m_size + band.distance) * 18.0f,
                (m_size + band.distance * 0.7f) * 18.0f,
                PackColor(static_cast<unsigned int>((std::min)(255.0f, band.alpha)), m_tintColor),
                destBlend,
                0.0f,
                renderFlags);
        }
        break;
    }
    case PP_WATERFALL: {
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        const float groundY = ResolveGroundHeight(base) + m_deltaPos2.y;
        for (const EffectBandState& band : m_bands) {
            if (!band.active || band.alpha <= 0.0f || band.maxHeight <= 0.0f) {
                continue;
            }

            const int cycleLength = (std::max)(1, static_cast<int>(std::round(band.maxHeight)));
            float scroll = static_cast<float>(band.process % cycleLength) * (kWaterfallSegmentHeight / static_cast<float>(cycleLength));
            const bool reverseScroll = band.modes[1] != 0;
            if (reverseScroll) {
                scroll = kWaterfallSegmentHeight - scroll;
            }

            const float crop = (std::clamp)(scroll / kWaterfallSegmentHeight, 0.0f, 1.0f);
            const int phase = (band.process % (kWaterfallTextureCycle * cycleLength)) / cycleLength;
            const bool swapAxes = band.modes[0] != 0;
            const float halfWidth = band.distance * 0.5f;
            const unsigned int waterfallColor = PackColor(static_cast<unsigned int>((std::clamp)(band.alpha, 0.0f, 255.0f)), m_tintColor);

            for (int stripIndex = 0; stripIndex < kWaterfallVisibleSegments; ++stripIndex) {
                const float yTop = scroll - band.heights[static_cast<size_t>(stripIndex)];
                const float yBottom = scroll - band.heights[static_cast<size_t>(stripIndex + 1)];
                float visibleTop = yTop;
                float visibleBottom = yBottom;
                float vTop = 0.0f;
                float vBottom = 1.0f;

                if (!reverseScroll) {
                    if (stripIndex == 0) {
                        visibleTop = yTop + (yBottom - yTop) * crop;
                        vTop = crop;
                    } else if (stripIndex == kWaterfallVisibleSegments - 1) {
                        visibleBottom = yTop + (yBottom - yTop) * crop;
                        vBottom = crop;
                    }
                } else {
                    if (stripIndex == 0) {
                        visibleBottom = yTop + (yBottom - yTop) * crop;
                        vBottom = crop;
                    } else if (stripIndex == kWaterfallVisibleSegments - 1) {
                        visibleTop = yTop + (yBottom - yTop) * crop;
                        vTop = crop;
                    }
                }

                if (std::fabs(visibleTop - visibleBottom) <= 0.001f) {
                    continue;
                }

                int textureIndex = stripIndex % kWaterfallTextureCycle;
                if (reverseScroll) {
                    textureIndex = (textureIndex - phase) % kWaterfallTextureCycle;
                    if (textureIndex < 0) {
                        textureIndex += kWaterfallTextureCycle;
                    }
                } else {
                    textureIndex = (textureIndex + phase) % kWaterfallTextureCycle;
                }

                CTexture* texture = textureIndex < static_cast<int>(m_texture.size())
                    ? m_texture[static_cast<size_t>(textureIndex)]
                    : nullptr;
                if (!texture || texture == &CTexMgr::s_dummy_texture) {
                    continue;
                }

                vector3d base0{};
                vector3d base1{};
                vector3d top1{};
                vector3d top0{};
                if (!swapAxes) {
                    base0 = { base.x - halfWidth, groundY + visibleBottom, base.z + band.radius };
                    base1 = { base.x + halfWidth, groundY + visibleBottom, base.z + band.radius };
                    top1 = { base.x + halfWidth, groundY + visibleTop, base.z + band.radius };
                    top0 = { base.x - halfWidth, groundY + visibleTop, base.z + band.radius };
                } else {
                    base0 = { base.x + band.radius, groundY + visibleBottom, base.z - halfWidth };
                    base1 = { base.x + band.radius, groundY + visibleBottom, base.z + halfWidth };
                    top1 = { base.x + band.radius, groundY + visibleTop, base.z + halfWidth };
                    top0 = { base.x + band.radius, groundY + visibleTop, base.z - halfWidth };
                }

                SubmitWorldTeiRect(base0,
                    base1,
                    top1,
                    top0,
                    *viewMatrix,
                    texture,
                    waterfallColor,
                    0.0f,
                    1.0f,
                    vBottom,
                    vTop,
                    destBlend,
                    0.0f,
                    renderFlags);
            }
        }
        break;
    }
    case PP_3DPARTICLEORBIT: {
        CTexture* texture = GetSoftDiscTexture();
        const vector3d pos = m_pos;
        const float size = (std::max)(0.6f, m_size);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        // Ref uses sprite clip * m_size * pixelRatio; soft-disc fallback reads small at 18x — nudge up.
        constexpr float kOrbitBillboardScale = 23.0f;
        SubmitBillboard(pos,
            *viewMatrix,
            texture,
            size * kOrbitBillboardScale,
            size * kOrbitBillboardScale,
            PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    case PP_3DPARTICLE:
    case PP_3DPARTICLEGRAVITY:
    case PP_3DPARTICLESPLINE: {
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        const unsigned int color = PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor);
        if (!SubmitAnimatedSpriteParticle(*this, base, viewMatrix, color, destBlend, renderFlags)) {
            CTexture* texture = !m_texture.empty()
                ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)]
                : GetSoftDiscTexture();
            const vector3d pos = m_pos;
            const float size = (std::max)(0.8f, m_size);
            SubmitBillboard(pos,
                *viewMatrix,
                texture,
                size * 20.0f,
                size * 20.0f,
                color,
                destBlend,
                0.0f,
                renderFlags);
        }
        break;
    }
    case PP_3DTEXTURE:
    case PP_3DSPHERE: {
        CTexture* texture = !m_texture.empty()
            ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)]
            : GetSoftGlowTexture(false);
        const vector3d pos = AddVec3(base, m_deltaPos2);
        const float width = (std::max)(8.0f, m_outerSize > 0.0f ? m_outerSize * 12.0f : m_size * 32.0f);
        const float height = (std::max)(8.0f, m_heightSize > 0.0f ? m_heightSize * 12.0f : width);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitBillboard(pos,
            *viewMatrix,
            texture,
            width,
            height,
            PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    case PP_3DCROSSTEXTURE: {
        CTexture* texture = !m_texture.empty()
            ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)]
            : GetSoftGlowTexture(false);
        const vector3d pos = AddVec3(base, m_deltaPos2);
        const float width = (std::max)(0.5f, m_size * 34.0f + m_outerSize * 8.0f);
        const float height = (std::max)(0.1f, m_heightSize > 0.0f ? m_heightSize * 12.0f : width * 1.6f);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitBillboard(pos,
            *viewMatrix,
            texture,
            width,
            height,
            PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        SubmitBillboard(pos,
            *viewMatrix,
            texture,
            height,
            width,
            PackColor(static_cast<unsigned int>(normalizedAlpha * 0.75f), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    case PP_3DQUADHORN:
        RenderQuadHornPrimitive(*this, base, *viewMatrix, normalizedAlpha);
        break;
    case PP_MAPPARTICLE: {
        const int particles = (std::max)(4, m_spawnCount);
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(true);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        for (int index = 0; index < particles; ++index) {
            const float seed = (static_cast<float>(index) + 0.5f) / static_cast<float>(particles);
            const float life = WrapUnit(timeSeconds / (std::max)(0.3f, m_emitSpeed) + seed);
            const float angle = (2.0f * kPi * seed) + life * 6.0f + timeSeconds * (m_longSpeed * 0.03f + 1.2f);
            const float radius = (std::max)(1.0f, m_radius + m_param[0] * 0.05f) * (0.85f + 0.25f * std::sin(timeSeconds * 2.3f + seed * 9.0f));
            const vector3d pos = {
                base.x + std::cos(angle) * radius,
                ResolveGroundHeight(base) - life * (m_heightSize > 0.0f ? m_heightSize : 4.0f),
                base.z + std::sin(angle) * radius
            };
            const float size = (std::max)(0.6f, m_size + seed * 0.3f);
            const float alphaScale = std::sin(life * kPi);
            SubmitBillboard(pos,
                *viewMatrix,
                texture,
                size * 18.0f,
                size * 18.0f,
                PackColor(static_cast<unsigned int>(normalizedAlpha * (std::max)(0.0f, alphaScale)), m_tintColor),
                destBlend,
                0.0f,
                renderFlags);
        }
        break;
    }
    case PP_2DTEXTURE:
    case PP_2DFLASH:
    case PP_2DCIRCLE: {
        vector3d anchorPos = base;
        anchorPos.y += m_param[2];
        tlvertex3d anchor{};
        if (!ProjectPoint(anchorPos, *viewMatrix, &anchor)) {
            break;
        }
        const float pixelRatio = ResolveEffectPixelRatio(anchor);
        if (!std::isfinite(pixelRatio) || pixelRatio <= 0.0f) {
            break;
        }
        CTexture* texture = !m_texture.empty()
            ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)]
            : GetSoftGlowTexture(m_type == PP_2DFLASH);
        const float width = (std::max)(16.0f, (m_outerSize > 0.0f ? m_outerSize * 12.0f : m_size * 38.0f)) * pixelRatio;
        const float height = (std::max)(16.0f, (m_heightSize > 0.0f ? m_heightSize * 12.0f : m_size * 38.0f)) * pixelRatio;
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitScreenQuad(anchor,
            texture,
            m_deltaPos2.x * pixelRatio,
            m_deltaPos2.y * pixelRatio,
            width,
            height,
            PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    default:
        break;
    }
}

CRagEffect::CWorldAnchor::CWorldAnchor(const vector3d& position)
{
    m_pos = position;
    m_roty = 0.0f;
    m_zoom = 1.0f;
    m_shadowZoom = 1.0f;
    m_isVisible = 1;
    m_sprRes = nullptr;
    m_actRes = nullptr;
}

CRagEffect::CRagEffect()
    : m_master(nullptr)
    , m_ownsMaster(false)
    , m_masterIsActor(false)
    , m_masterActorGid(0)
    , m_handler(Handler::None)
    , m_type(0)
    , m_flag(0)
    , m_count(0)
    , m_level(1)
    , m_stateCnt(0)
    , m_duration(0)
    , m_cEndLayer(0)
    , m_iCurLayer(0)
    , m_worldType(0)
    , m_loop(false)
    , m_isWorldEffect(false)
    , m_strFrameActive(true)
    , m_lastProcessTick(timeGetTime())
    , m_tickCarryMs(kEffectTickMs)
    , m_strScreenOffset{ 0.0f, -80.0f, 0.0f }
    , m_strAnchorPosition{}
    , m_effectPos{}
    , m_cachedPos{}
    , m_deltaPos{}
    , m_deltaPos2{}
    , m_deltaPos3{}
    , m_targetPos{}
    , m_param{ 0.0f, 0.0f, 0.0f, 0.0f }
    , m_emitSpeed(1.0f)
    , m_latitude(0.0f)
    , m_longitude(0.0f)
    , m_roty(0.0f)
    , m_tlvertX(0.0f)
    , m_tlvertY(0.0f)
    , m_tlvertZ(0.0f)
    , m_tintColor(RGB(255, 255, 255))
    , m_ezEffectRes(nullptr)
    , m_aniClips(nullptr)
{
    m_isLayerDrawn.fill(0);
    m_aiCurAniKey.fill(0);
}

CRagEffect::~CRagEffect()
{
    m_ezEffectRes = nullptr;
    ClearPrims();

    if (CAbleToMakeEffect* owner = ResolveCurrentEffectOwner()) {
        owner->SendMsg(this, 23, 0, 0, 0);
    }

    if (m_ownsMaster) {
        delete m_master;
        m_master = nullptr;
    }
}

void CRagEffect::ClearPrims()
{
    // Ref/RagEffect.cpp:10058-10078 / message 25: detach; the world deletes.
    for (CEffectPrim* prim : m_primList) {
        if (prim) {
            prim->SendMsg(this, 24, 0, 0, 0);
        }
    }
    m_primList.clear();
}

void CRagEffect::LoadEzEffect(const char* fName)
{
    if (!fName || !*fName) {
        return;
    }

    m_ezEffectRes = g_resMgr.GetAs<CEZeffectRes>(fName);
    if (!m_ezEffectRes) {
        DbgLog("[RagEffect] missing STR '%s' for effect %d\n", fName, m_type);
        return;
    }

    m_aniClips = &m_ezEffectRes->m_aniClips;
    m_cEndLayer = m_aniClips->cLayer;
    if (m_aniClips->cLayer > 0 && m_aniClips->aLayer[0].cAniKey == 0) {
        m_cEndLayer = m_aniClips->cLayer - 1;
    }
}

// Ref/RagEffect.cpp:9666-9695. Restart the key cursors without resetting
// transform accumulators: Ref carries those values across a restart.
// Ref/Renderer.cpp:7067-7084 (0x413130). The STR path projects without
// the reconstructed billboard near-plane/depth clamping. z carries reciprocal depth.
vector3d ProjectStrPoint(const vector3d& point, const matrix& view, int screenX, int screenY)
{
    const double reciprocal = 1.0 / (static_cast<double>(view.m[2][2]) * point.z
        + static_cast<double>(view.m[1][2]) * point.y
        + static_cast<double>(view.m[0][2]) * point.x + view.m[3][2]);
    vector3d projected;
    projected.z = static_cast<float>(reciprocal);
    projected.x = static_cast<float>((static_cast<double>(view.m[2][0]) * point.z
        + static_cast<double>(view.m[1][0]) * point.y
        + static_cast<double>(point.x) * view.m[0][0] + view.m[3][0])
        * reciprocal * g_renderer.m_hpc + screenX);
    projected.y = static_cast<float>((static_cast<double>(view.m[2][1]) * point.z
        + static_cast<double>(view.m[1][1]) * point.y
        + static_cast<double>(view.m[0][1]) * point.x + view.m[3][1])
        * g_renderer.m_vpc * projected.z + screenY);
    return projected;
}

void CRagEffect::InitEZ2STRFrame()
{
    m_iCurLayer = 0;
    m_stateCnt = 0;
    m_cEndLayer = m_aniClips ? m_aniClips->cLayer : 0;
    if (m_aniClips && m_aniClips->aLayer[0].cAniKey == 0) {
        m_cEndLayer = m_aniClips->cLayer - 1;
    }
    if (m_aniClips) {
        for (int layer = 0; layer < m_aniClips->cLayer && layer < static_cast<int>(m_aiCurAniKey.size()); ++layer) {
            m_aiCurAniKey[static_cast<size_t>(layer)] = 0;
        }
    }
}

bool CRagEffect::ProcessEZ2STR(float x, float y, float z)
{
    m_strFrameActive = false;
    if (!m_ezEffectRes || !m_aniClips) {
        return false;
    }

    // Ref/RagEffect.cpp:9505-9508: retain the final frame's state on
    // termination; callers decide whether a false result ends the effect.
    if (m_iCurLayer >= m_cEndLayer) {
        return false;
    }

    // Ref projects the master's position during processing. Keep the world
    // anchor from this call for the split renderer, including temporary targets.
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return false;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || !mode->m_view) return false;
    m_strAnchorPosition = master->m_pos;
    const vector3d projected = ProjectStrPoint(m_strAnchorPosition, mode->m_view->GetViewMatrix(), g_renderer.m_halfWidth, g_renderer.m_halfHeight);
    m_tlvertX = projected.x + x;
    m_tlvertY = projected.y + y;
    // Processing calls the tlvertex overload (0x4131B0), whose z is device
    // depth. Rendering below calls the reciprocal-depth overload (0x413130).
    m_tlvertZ = static_cast<float>(1500.0 / (1500.0 - 10.0)
        * (1.0 / projected.z - 10.0) * projected.z) + z;
    m_strScreenOffset = { x, y, z };
    // Binary 0x5F1358/0x5F138A reuses the z argument slot as the loop
    // quotient. Reverse-loop mode reads its preceding value before replacing it.
    float loopQuotient = z;
    m_isLayerDrawn.fill(0);
    for (int layerIndex = 0; layerIndex < m_aniClips->cLayer && layerIndex < static_cast<int>(m_actXformData.size()); ++layerIndex) {
        const KAC_LAYER& layer = m_aniClips->aLayer[static_cast<size_t>(layerIndex)];
        if (layer.cAniKey <= 0 || !layer.aAniKey) {
            continue;
        }

        int& curKeyIndex = m_aiCurAniKey[static_cast<size_t>(layerIndex)];
        if (curKeyIndex < 0) {
            continue;
        }
        if (curKeyIndex >= layer.cAniKey) {
            continue;
        }

        const KAC_KEYFRAME* key = &layer.aAniKey[static_cast<size_t>(curKeyIndex)];
        const int frameOffset = m_stateCnt - key->iFrame;
        if (frameOffset >= 0 && curKeyIndex < layer.cAniKey - 1) {
            const KAC_KEYFRAME& nextKey = layer.aAniKey[static_cast<size_t>(curKeyIndex + 1)];
            if (frameOffset > 0 && nextKey.iFrame == m_stateCnt) {
                ++curKeyIndex;
                key = &layer.aAniKey[static_cast<size_t>(curKeyIndex)];
                if (curKeyIndex >= layer.cAniKey - 1) {
                    curKeyIndex = -1;
                    ++m_iCurLayer;
                }
            }
        }

        if (frameOffset < 0) {
            continue;
        }

        KAC_XFORMDATA& xform = m_actXformData[static_cast<size_t>(layerIndex)];
        if (key->dwType != 0) {
            AddXformDelta(&xform, key->XformData);
            switch (key->XformData.anitype) {
            case 1:
                xform.aniframe += key->XformData.aniframe;
                break;
            case 2: {
                const double nextFrame = static_cast<double>(xform.aniframe) + key->XformData.anidelta;
                xform.aniframe = static_cast<float>(nextFrame);
                if (nextFrame >= static_cast<float>(layer.cTex)) {
                    xform.aniframe = static_cast<float>(layer.cTex) - 1.0f;
                }
                break;
            }
            case 3: {
                const double nextFrame = static_cast<double>(xform.aniframe) + key->XformData.anidelta;
                xform.aniframe = static_cast<float>(nextFrame);
                const float frame = xform.aniframe;
                const float texCount = static_cast<float>(layer.cTex);
                if (static_cast<double>(texCount) <= nextFrame) {
                    loopQuotient = static_cast<float>(static_cast<int>(static_cast<double>(frame) / texCount));
                    xform.aniframe = static_cast<float>(frame - static_cast<double>(texCount) * loopQuotient);
                }
                break;
            }
            case 4: {
                const double nextFrame = static_cast<double>(xform.aniframe) - key->XformData.anidelta;
                xform.aniframe = static_cast<float>(nextFrame);
                if (nextFrame < 0.0) {
                    const float texCount = static_cast<float>(layer.cTex);
                    double remainder = nextFrame - static_cast<double>(texCount) * loopQuotient;
                    loopQuotient = static_cast<float>(static_cast<int>(remainder / texCount));
                    if (remainder < 0.0) remainder += texCount;
                    xform.aniframe = static_cast<float>(remainder);
                }
                break;
            }
            case 5: {
                // Ref uses the pre-transition key age even when this call just
                // advanced the cursor to a new key; it does not recompute age.
                const int motion = static_cast<int>(static_cast<double>(frameOffset) * key->XformData.anidelta + xform.aniframe);
                const int span = layer.cTex - 1;
                xform.aniframe = ((motion / span) & 1) != 0
                    ? static_cast<float>(span * ((motion / span) + 1) - motion)
                    : static_cast<float>(motion % span);
                break;
            }
            default:
                break;
            }
        } else {
            xform = key->XformData;
            if (curKeyIndex != -1 && curKeyIndex < layer.cAniKey - 1) {
                const KAC_KEYFRAME& nextKey = layer.aAniKey[static_cast<size_t>(curKeyIndex + 1)];
                if (nextKey.iFrame == key->iFrame) {
                    ++curKeyIndex;
                }
            }
        }

        m_isLayerDrawn[static_cast<size_t>(layerIndex)] = 1;

    }

    m_strFrameActive = true;
    return true;
}

// HighPriest.exe.c:420542-420562, case 185. Restore the actor on both
// completion and continuation; STR processing captures the temporary anchor.
bool CRagEffect::ProcessTargetPositionStr()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return false;
    m_deltaPos2 = master->m_pos;
    master->m_pos = m_deltaPos;
    const bool active = ProcessEZ2STR();
    master->m_pos = m_deltaPos2;
    return active;
}

// Ref/RagEffect.cpp:9725-9862 (0x5F1420). Preserve Ref's independent
// pixel-ratio projection, conditional corner/UV order and byte color conversion.
void CRagEffect::RenderAniClip(int, const KAC_LAYER& layer, const KAC_XFORMDATA& xform, matrix* viewMatrix)
{
    if (!viewMatrix) return;
    const vector3d projected = ProjectStrPoint(m_effectPos, *viewMatrix, g_renderer.m_xoffset, g_renderer.m_yoffset);
    const float pixelRatio = static_cast<float>(static_cast<double>(projected.z) * g_renderer.m_hpc * static_cast<double>(0.14285715f));
    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) return;
    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->cullMode = D3DCULL_NONE;
    face->alphaSortKey = 0.0f;
    tlvertex3d* verts = face->m_verts;
    if (xform.ax[0] >= static_cast<double>(xform.ax[1])) {
        verts[1].x = xform.ax[1]; verts[1].tu = xform.us + xform.u;
        verts[3].x = xform.ax[0]; verts[3].tu = xform.u;
        verts[0].x = xform.ax[2]; verts[0].tu = xform.us + xform.u;
        verts[2].x = xform.ax[3]; verts[2].tu = xform.u;
    } else {
        verts[1].x = xform.ax[0]; verts[1].tu = xform.u;
        verts[3].x = xform.ax[1]; verts[3].tu = xform.us + xform.u;
        verts[0].x = xform.ax[3]; verts[0].tu = xform.u;
        verts[2].x = xform.ax[2]; verts[2].tu = xform.us + xform.u;
    }
    if (xform.ay[1] >= static_cast<double>(xform.ay[2])) {
        verts[3].y = xform.ay[2]; verts[3].tv = xform.v + xform.vs;
        verts[2].y = xform.ay[1]; verts[2].tv = xform.v;
        verts[1].y = xform.ay[3]; verts[1].tv = xform.v + xform.vs;
        verts[0].y = xform.ay[0]; verts[0].tv = xform.v;
    } else {
        verts[3].y = xform.ay[1]; verts[3].tv = xform.v;
        verts[2].y = xform.ay[2]; verts[2].tv = xform.v + xform.vs;
        verts[1].y = xform.ay[0]; verts[1].tv = xform.v;
        verts[0].y = xform.ay[3]; verts[0].tv = xform.v + xform.vs;
    }
    const int frame = static_cast<int>(xform.aniframe);
    CTexture* texture = const_cast<KAC_LAYER&>(layer).GetTexture(frame <= 0 ? 0 : frame);
    if (!texture) return;
    const double uScale = static_cast<double>(texture->m_updateWidth) / texture->m_w;
    const float vScale = static_cast<float>(static_cast<double>(texture->m_updateHeight) / texture->m_h);
    const float angle = static_cast<float>(static_cast<double>(xform.rz) * 0.3515625000000055 * 3.141592 * 0.0055555557);
    const float sine = static_cast<float>(std::sin(static_cast<double>(angle)));
    const double cosine = std::cos(static_cast<double>(angle));
    const auto colorByte = [](float value) { return static_cast<u8>(static_cast<long long>(value)); };
    const u32 color = (static_cast<u32>(colorByte(xform.crA)) << 24)
        | (static_cast<u32>(colorByte(xform.crR)) << 16)
        | (static_cast<u32>(colorByte(xform.crG)) << 8) | colorByte(xform.crB);
    for (int index = 0; index < 4; ++index) {
        tlvertex3d& vert = verts[index];
        vert.tu = static_cast<float>(uScale * vert.tu);
        vert.tv = vScale * vert.tv;
        const double vx = vert.x, vy = vert.y;
        vert.x = static_cast<float>((vx * cosine - vy * sine + xform.x - 320.0) * pixelRatio + m_tlvertX);
        vert.y = static_cast<float>((vy * cosine + vx * sine + xform.y - 240.0) * pixelRatio + m_tlvertY);
        vert.color = color;
        vert.specular = 0xFFFFFFFFu;
        vert.z = 0.00003102122f;
        vert.oow = projected.z;
    }
    face->tex = texture;
    face->srcAlphaMode = static_cast<D3DBLEND>(xform.srcalpha);
    face->destAlphaMode = static_cast<D3DBLEND>(xform.destalpha);
    face->mtPreset = static_cast<int>(xform.mtpreset);
    g_renderer.AddRP(face, 5);
}

void CRagEffect::Init(CRenderObject* master, int effectId, const vector3d& deltaPos)
{
    m_master = master;
    m_ownsMaster = false;
    if (const CGameActor* actorMaster = dynamic_cast<const CGameActor*>(master)) {
        m_masterIsActor = true;
        m_masterActorGid = actorMaster->m_gid;
    } else {
        m_masterIsActor = false;
        m_masterActorGid = 0;
    }
    m_type = effectId;
    m_deltaPos = deltaPos;
    m_cachedPos = master ? master->m_pos : vector3d{};
    m_effectPos = m_cachedPos;
    m_longitude = master ? master->m_roty : 0.0f;
    m_handler = Handler::None;
    m_loop = false;
    m_isWorldEffect = effectId == kRagEffectPortal || effectId == kRagEffectReadyPortal || effectId == kRagEffectWarpZone;
    m_duration = (effectId >= 0 && effectId < kRagEffectDurationCount)
        ? kRagEffectDurationTable[effectId]
        : 100;
    m_stateCnt = 0;
    m_lastProcessTick = timeGetTime();
    m_tickCarryMs = kEffectTickMs;
    m_targetPos = vector3d{};
    m_hasTargetPos = false;

    switch (effectId) {
    case 40: case 41: case 49:
        // Direct helpers own STR completion; load the reference catalog clip below.
        m_handler = Handler::None;
        break;
    case 48: case 50: case 51: case 53: case 54: case 55: case 56: case 57: case 58: case 59:
        // Direct reference emitters own their primitive schedules.
        m_handler = Handler::None;
        break;
    case 371: {
        m_handler = Handler::EzStr;
        const std::string strName = ResolveStrName(effectId);
        LoadEzEffect(strName.c_str());
        InitEZ2STRFrame();
        break;
    }
    case 0: case 1: case 2: case 3: case 4: case 5: case 14: case 16: case 17: case 18: case 27: case 28: case 31: case 32: case 34: case 35: case 36: case 37: case 38: case 39: case 43: case 45: case 46: case 61: case 81: case 98: case 121: case 122: case 123: case 128: case 132: case 142: case 180: case 184: case 401:
        // ProcessEffectFrame owns reference primitive-only dispatch for these ids.
        m_handler = Handler::None;
        break;
    case 312: case 313: case 325: case 331: case 332:
        // Specialized reference helpers are dispatched by OnProcess2.
        // Keep these out of generic STR-handler selection during Init.
        m_handler = Handler::None;
        break;
    case 159:
        m_handler = Handler::HideStart;
        break;
    case 22:
        m_handler = Handler::Sight;
        break;
    case 24:
        m_handler = Handler::FireBall;
        break;
    case 229:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 0.0f;
        m_param[1] = 40.0f;
        break;
    case 230:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 1.0f;
        m_param[1] = 60.0f;
        break;
    case 231:
        m_handler = Handler::MapPillar;
        break;
    case 233:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 2.0f;
        m_param[1] = 40.0f;
        break;
    case 515:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 3.0f;
        m_param[1] = 80.0f;
        break;
    case 516:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 4.0f;
        m_param[1] = 80.0f;
        break;
    case 592:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 5.0f;
        m_param[1] = 80.0f;
        break;
    case 697:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 7.0f;
        m_param[1] = 80.0f;
        break;
    case 698:
        m_handler = Handler::WeatherCloud;
        m_param[0] = 8.0f;
        m_param[1] = 80.0f;
        break;
    case 33:
        m_handler = Handler::Ruwach;
        break;
    case 247:
        m_handler = Handler::MapPillar;
        break;
    case 20:
    case 125:
        m_handler = Handler::EnchantPoison;
        break;
    case 493:
        m_handler = Handler::EnchantPoison2;
        break;
    case 42:
        m_handler = Handler::Blessing;
        break;
    case 44:
        m_handler = Handler::Smoke;
        break;
    case 47:
        m_handler = Handler::Torch;
        break;
    case 601:
        m_handler = Handler::Sight2;
        break;
    case kRagEffectPortal:
        m_handler = Handler::Portal;
        m_loop = true;
        m_duration = 1000000;
        break;
    case kRagEffectReadyPortal:
        m_handler = Handler::ReadyPortal;
        m_loop = true;
        m_duration = 1000000;
        break;
    case kRagEffectWarpZone:
        m_handler = Handler::WarpZone;
        m_loop = true;
        m_duration = 1000000;
        break;
    default: {
        const std::string strName = ResolveStrName(effectId);
        if (!strName.empty()) {
            m_handler = Handler::EzStr;
            LoadEzEffect(strName.c_str());
            InitEZ2STRFrame();
        }
        break;
    }
    }

    if (m_ezEffectRes == nullptr) {
        const std::string strName = ResolveStrName(effectId);
        if (!strName.empty()) {
            LoadEzEffect(strName.c_str());
            InitEZ2STRFrame();
        }
    }
}

void CRagEffect::InitAtWorldPosition(int effectId, const vector3d& position)
{
    Init(new CWorldAnchor(position), effectId, vector3d{});
    m_ownsMaster = true;
    m_cachedPos = position;
}

void CRagEffect::InitWorld(const C3dWorldRes::effectSrcInfo& source)
{
    m_isWorldEffect = true;
    m_master = new CWorldAnchor(source.pos);
    m_ownsMaster = true;
    m_masterIsActor = false;
    m_masterActorGid = 0;
    m_cachedPos = source.pos;
    m_effectPos = source.pos;
    m_effectName = source.name;
    m_worldType = source.type;
    m_emitSpeed = source.emitSpeed;
    for (int index = 0; index < 4; ++index) {
        m_param[index] = source.param[index];
    }
    m_tintColor = ResolvePortalTint(source.name);
    m_loop = true;
    m_duration = 1000000;
    m_stateCnt = 0;
    m_lastProcessTick = timeGetTime();
    m_tickCarryMs = kEffectTickMs;

    const std::string worldStrName = ResolveWorldStrName(source.name);
    if (!worldStrName.empty()) {
        m_handler = Handler::EzStr;
        LoadEzEffect(worldStrName.c_str());
        InitEZ2STRFrame();
        if (m_aniClips && m_aniClips->cFrame > 0) {
            m_duration = (std::max)(m_duration, m_aniClips->cFrame + 12);
        }
    } else if (source.type == 80) {
        m_type = kRagEffectPortal;
        m_handler = Handler::Portal;
    } else if (source.type == 81 || ContainsIgnoreCaseAscii(source.name, "ready")) {
        m_type = kRagEffectReadyPortal;
        m_handler = Handler::ReadyPortal;
    } else if (source.type == 44) {
        m_type = 44;
        m_handler = Handler::Smoke;
    } else if (source.type == 47) {
        m_type = 47;
        m_handler = Handler::Torch;
    } else if (ContainsIgnoreCaseAscii(source.name, "warp") || ContainsIgnoreCaseAscii(source.name, "portal")) {
        m_type = kRagEffectWarpZone;
        m_handler = Handler::WarpZone;
    } else if (std::fabs(source.param[0]) > 20.0f) {
        m_handler = Handler::MapMagicZone;
    } else {
        m_handler = Handler::MapParticle;
    }
}

CEffectPrim* CRagEffect::LaunchEffectPrim(EFFECTPRIMID effectPrimId, const vector3d& deltaPos)
{
    CEffectPrim* prim = new CEffectPrim();
    if (!prim) {
        return nullptr;
    }
    prim->Init(this, effectPrimId, deltaPos);
    m_primList.push_back(prim);
    // Ref/RagEffect.cpp:9937-10023: primitives are processed/rendered by World.
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    CWorld* world = mode && mode->m_world ? mode->m_world : &g_world;
    world->m_gameObjectList.push_back(prim);
    return prim;
}

// Registered primitives remain world-owned even if resource setup fails.
// Use the Ref message-25 / message-24 attachment protocol for cancellation.
void CRagEffect::DiscardPrimitive(CEffectPrim* prim)
{
    if (!prim) return;
    SendMsg(prim, 25, 0, 0, 0);
    prim->SendMsg(this, 24, 0, 0, 0);
}

void CRagEffect::OnActorDeleted(const CGameActor* actor)
{
    if (!actor) {
        return;
    }

    if (m_masterIsActor && actor->m_gid == m_masterActorGid) {
        SendMsg(nullptr, 24, 0, 0, 0);
        return;
    }

    if (static_cast<const CRenderObject*>(actor) == m_master) {
        SendMsg(nullptr, 24, 0, 0, 0);
    }
}

CRenderObject* CRagEffect::ResolveCurrentMaster() const
{
    if (!m_masterIsActor || m_ownsMaster) {
        return IsLikelyLiveRenderObject(m_master) ? m_master : nullptr;
    }

    if (g_world.m_player && g_world.m_player->m_gid == m_masterActorGid) {
        return IsLikelyLiveRenderObject(g_world.m_player) ? g_world.m_player : nullptr;
    }

    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        const auto it = gameMode->m_runtimeActors.find(m_masterActorGid);
        if (it != gameMode->m_runtimeActors.end() && it->second) {
            return IsLikelyLiveRenderObject(it->second) ? it->second : nullptr;
        }
    }

    for (CGameActor* actor : g_world.m_actorList) {
        if (actor && actor->m_gid == m_masterActorGid) {
            return IsLikelyLiveRenderObject(actor) ? actor : nullptr;
        }
    }

    return nullptr;
}

CAbleToMakeEffect* CRagEffect::ResolveCurrentEffectOwner() const
{
    return dynamic_cast<CAbleToMakeEffect*>(ResolveCurrentMaster());
}

vector3d CRagEffect::ResolveBasePosition() const
{
    if (m_type == 123 || m_type == 132) return m_effectPos;
    if (CRenderObject* master = ResolveCurrentMaster()) {
        return AddVec3(master->m_pos, m_deltaPos);
    }
    return AddVec3(m_cachedPos, m_deltaPos);
}

vector3d CRagEffect::ActorPosition() const
{
    if (CRenderObject* master = ResolveCurrentMaster()) {
        return master->m_pos;
    }
    return m_cachedPos;
}

float CRagEffect::ResolveBaseRotation() const
{
    if (CRenderObject* master = ResolveCurrentMaster()) {
        return master->m_roty;
    }
    return m_longitude;
}

bool CRagEffect::ResolveCullSphere(vector3d* outCenter, float* outRadius) const
{
    if (!outCenter || !outRadius) {
        return false;
    }

    vector3d base = ResolveBasePosition();
    vector3d center = base;
    float radius = 96.0f;
    switch (m_handler) {
    case Handler::Portal:
    case Handler::ReadyPortal:
    case Handler::WarpZone:
        radius = 180.0f;
        break;
    case Handler::MapPillar:
        radius = 180.0f;
        center.y += 56.0f;
        break;
    case Handler::MapMagicZone:
        radius = (std::max)(120.0f, std::fabs(m_param[0]) * 2.0f + 48.0f);
        break;
    case Handler::MapParticle:
        radius = 128.0f;
        break;
    case Handler::BlueFall:
        radius = 220.0f;
        center.y += 40.0f;
        break;
    case Handler::Smoke:
    case Handler::Torch:
    case Handler::EzStr:
    case Handler::HideStart:
    case Handler::Sight:
    case Handler::Sight2:
    case Handler::FireBall:
    case Handler::Ruwach:
        radius = 140.0f;
        break;
    case Handler::None:
    default:
        // These ids use OnProcess2 directly; retain their existing cull extent.
        radius = (m_type == 123 || m_type == 132 || m_type == 312 || m_type == 313 || m_type == 325 || m_type == 331 || m_type == 332)
            ? 140.0f : 96.0f;
        break;
    }

    if (m_hasTargetPos) {
        center.x = (base.x + m_targetPos.x) * 0.5f;
        center.y = (base.y + m_targetPos.y) * 0.5f;
        center.z = (base.z + m_targetPos.z) * 0.5f;
        const float dx = m_targetPos.x - base.x;
        const float dy = m_targetPos.y - base.y;
        const float dz = m_targetPos.z - base.z;
        radius += std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
    }

    *outCenter = center;
    *outRadius = radius;
    return true;
}

void CRagEffect::SpawnPortal()
{
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);

    if (m_stateCnt == 0) {
        const vector3d base = ResolveBasePosition();
        DbgLog("[RagEffect] spawn portal base=(%.2f,%.2f,%.2f) ringBlue=%p\n",
            base.x,
            base.y,
            base.z,
            ringBlue);
    }

    if (m_stateCnt == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_arcAngle = 36.0f;
            prim->m_outerSize = 3.5f;
            prim->m_innerSize = 3.5f;
            prim->m_longSpeed = 4.0f;
            prim->m_heightSize = 40.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 128.0f;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.16666667f;
            prim->m_fadeOutCnt = prim->m_duration - 6;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(86, 196, 255);
        }
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_arcAngle = 36.0f;
            prim->m_outerSize = 4.0f;
            prim->m_innerSize = 4.0f;
            prim->m_longSpeed = 5.0f;
            prim->m_heightSize = 50.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 128.0f;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.16666667f;
            prim->m_fadeOutCnt = prim->m_duration - 6;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(96, 196, 255);
        }
    }

    if ((m_stateCnt % 14) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = 30;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 8.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.0f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.5f;
            prim->m_alpha = 128.0f;
            prim->m_maxAlpha = 128.0f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
            prim->m_longSpeed = 0.0f;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(86, 196, 255);
        }
    }
}

void CRagEffect::SpawnReadyPortal()
{
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);

    if ((m_stateCnt % 14) == 0) {
        TryPlayEffectWaveAt(ResolveBasePosition(), "effect\\EF_ReadyPortal.wav");
    }

    if ((m_stateCnt % 14) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = 30;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 8.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.0f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.5f;
            prim->m_alpha = 120.0f;
            prim->m_maxAlpha = 120.0f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(120, 220, 255);
        }
    }
}

void CRagEffect::SpawnWarpZone()
{
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);
    CTexture* alphaDown = TryResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    });
    CTexture* magicBlue = TryResolveEffectTextureCandidates({
        "effect\\magic_blue.tga",
        "effect\\magic_blue.bmp",
        "effect\\magic_sky.tga",
        "effect\\magic_sky.bmp",
    });
    CTexture* warpBandTex = magicBlue ? magicBlue : ringBlue;

    if (m_stateCnt == 0 && alphaDown) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = 158;
            prim->m_radius = 15.0f;
            prim->m_alpha = 0.0f;
            prim->m_alphaSpeed = 12.8f;
            prim->m_maxAlpha = 128.0f;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_latitude = 90.0f;
            prim->m_pattern |= 1;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(255, 255, 255);
        }
    }

    if ((m_stateCnt % 28) == 0 && m_stateCnt < 117) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = 80;
            prim->m_innerSize = 4.0f;
            prim->m_latitude = 90.0f;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_numSegments = 3;
            prim->m_radius = 14.0f;
            prim->m_radiusSpeed = -0.15f;
            prim->m_alpha = 5.0f;
            prim->m_maxAlpha = 200.0f;
            prim->m_alphaSpeed = 10.0f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5;
            prim->m_texture.push_back(warpBandTex);
            prim->m_tintColor = RGB(255, 255, 255);
        }
    }

    if ((m_stateCnt % 10) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = 130;
            prim->m_radius = 5.0f + static_cast<float>((rand() % 200) / 100) * 0.75f;
            prim->m_radiusSpeed = 0.0005f;
            prim->m_longitude = static_cast<float>(rand() % 360);
            prim->m_longSpeed = 2.0f;
            prim->m_gravSpeed = -0.2f;
            prim->m_gravAccel = (-prim->m_gravSpeed / static_cast<float>(prim->m_duration)) * 0.66666669f;
            prim->m_heightSize = 4.0f;
            prim->m_alpha = 96.0f;
            prim->m_maxAlpha = 96.0f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
            prim->m_size = static_cast<float>(rand() % 10) * 0.1f + 0.7f;
            prim->m_tintColor = RGB(255, 255, 255);
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));
            prim->m_deltaPos2 = {
                prim->m_radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
                prim->m_radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
                prim->m_radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
            };
        }
    }

    if (m_stateCnt >= 145) {
        m_stateCnt = 0;
    }
}

// HighPriest.exe.c:459558-459668 (0x635BB0). Entry2 launches PP_HEAL,
// not a casting-ring approximation, and uses four distinct ring profiles.
void CRagEffect::ApplyEntry2()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_Portal.wav", m_effectPos.x - listener.x, -150.0f,
            m_effectPos.z - listener.z, 250, 40, 1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false));
    prim->m_size = 4.0f;
    const float heights[] = { 30.0f, 30.0f, 4.0f, 4.0f };
    const float distances[] = { 3.7f, 3.4000001f, 3.5999999f, 3.7f };
    const short rises[] = { 90, 90, 10, 5 };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = heights[band];
        gi.rotStart = static_cast<short>(rand() % 360);
        gi.alphaB = 0;
        gi.distance = distances[band];
        gi.riseAngle = rises[band];
        gi.alphaT = 1400;
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(2);
    }
}

// HighPriest.exe.c:434741-434782 (0x60D710). Only time zero processes STR;
// the tier dispatcher deliberately ignores this helper's completion result.
bool CRagEffect::ApplyJobLvUp50(int time)
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return false;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 5) {
        CEffectPrim* prim = LaunchEffectPrim(PP_SLASH1, vector3d{});
        prim->m_renderFlag = 5;
        prim->m_duration = m_duration;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\magic_green.tga", false));
        prim->m_size = 9.0f;
        for (int band = 0; band < 4; ++band) {
            EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
            gi.life = 1; gi.process = 0; gi.maxHeight = 2.0f;
            gi.rotStart = static_cast<short>(time + band * 90);
            gi.alphaB = 0; gi.distance = 2.0f; gi.height[0] = 1.0f; gi.flag[0] = 0;
        }
    }
    return time != 0 || ProcessEZ2STR(0.0f, 0.0f, 0.0f);
}

// HighPriest.exe.c:434785-434822 (0x60D8A0): every call emits.
bool CRagEffect::ApplyJobLvUp50_2(short time)
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return false;
    m_effectPos = master->m_pos;
    CEffectPrim* prim = LaunchEffectPrim(PP_SLASH1, vector3d{});
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\magic_blue.tga", false));
    prim->m_size = 4.0f;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1; gi.process = 0; gi.maxHeight = 1.5f;
        gi.rotStart = static_cast<short>(time + band * 90);
        gi.alphaB = 0; gi.distance = 2.0f; gi.height[0] = 1.0f; gi.flag[0] = 0;
    }
    return true;
}

// Ref/RagEffect.cpp:11977-11983, OnProcess3 case 590 (0x24E).
void CRagEffect::ProcessJobLvUp50_2()
{
    if (m_stateCnt == 15) {
        ApplyJobLvUp50_2(0);
        ApplyJobLvUp50_2(45);
    }
}

void CRagEffect::SpawnMapMagicZone()
{
    if (!m_primList.empty()) {
        return;
    }
    if (CEffectPrim* prim = LaunchEffectPrim(PP_MAPMAGICZONE, vector3d{})) {
        prim->m_duration = 120;
        prim->m_radius = m_param[0] > 0.0f ? m_param[0] : 30.0f;
        prim->m_innerSize = prim->m_radius * 0.72f;
        prim->m_outerSize = prim->m_radius;
        prim->m_alpha = 180.0f;
        prim->m_maxAlpha = 180.0f;
        prim->m_texture.push_back(ResolveEffectTextureOrFallback(std::string("effect\\") + m_effectName, false));
        prim->m_tintColor = ResolvePortalTint(m_effectName);
    }
}

void CRagEffect::SpawnMapPillar()
{
    if (m_stateCnt != 0 || !m_master) {
        return;
    }

    const int variant = m_type == 247 ? 1 : 0;
    if (CEffectPrim* prim = LaunchEffectPrim(PP_MAPPILLAR, vector3d{})) {
        prim->m_renderFlag = 5u;
        prim->m_duration = m_duration;
        prim->m_size = 4.0f;
        prim->m_alpha = 255.0f;
        prim->m_maxAlpha = 255.0f;
        prim->m_tintColor = RGB(110, 175, 255);
        prim->m_texture.push_back(ResolveEffectTextureCandidates({ "effect\\ring_blue.tga" }, false));

        for (int bandIndex = 0; bandIndex < 4; ++bandIndex) {
            EffectBandState& band = prim->m_bands[static_cast<size_t>(bandIndex)];
            band = {};
            band.active = true;
            band.process = bandIndex * 30;
            band.maxHeight = 120.0f;
            band.rotStart = static_cast<float>(bandIndex * 90);
            band.distance = (variant == 1 ? 11.0f : 2.0f) + static_cast<float>(bandIndex) * 0.5f;
            band.riseAngle = 89.0f;
            band.fadeThreshold = variant == 1 ? 70.0f : 50.0f;
            band.alpha = band.fadeThreshold;
            band.heights.fill(0.0f);
        }
    }
}

void CRagEffect::SpawnMapParticle()
{
    if (!m_primList.empty()) {
        return;
    }
    if (CEffectPrim* prim = LaunchEffectPrim(PP_MAPPARTICLE, vector3d{})) {
        prim->m_duration = 120;
        prim->m_spawnCount = 20;
        prim->m_radius = 5.0f + std::fabs(m_param[0]) * 0.08f;
        prim->m_heightSize = 6.0f + std::fabs(m_param[1]) * 0.04f;
        prim->m_emitSpeed = (std::max)(0.4f, m_emitSpeed);
        prim->m_alpha = 164.0f;
        prim->m_maxAlpha = 164.0f;
        prim->m_size = 0.8f;
        prim->m_texture.push_back(ResolveEffectTextureOrFallback(std::string("effect\\") + m_effectName, true));
        prim->m_tintColor = ResolvePortalTint(m_effectName);
        prim->m_param[0] = m_param[0];
        prim->m_param[1] = m_param[1];
    }
}


void CRagEffect::SpawnWeatherCloud()
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    CRenderObject* player = mode && mode->m_world ? mode->m_world->m_player : nullptr;
    if (player) {
        m_effectPos = player->m_pos;
    } else if (m_master) {
        m_effectPos = m_master->m_pos;
    } else {
        return;
    }

    if (m_stateCnt == 99999998) {
        m_duration = 99999999;
        m_stateCnt = 0;
    }
    if (m_stateCnt != 0) {
        return;
    }

    const int mapVariant = static_cast<int>(m_param[0]);
    const int cloudCount = (std::max)(1, static_cast<int>(m_param[1]));

    for (int cloudIndex = 0; cloudIndex < cloudCount; ++cloudIndex) {
        CEffectPrim* prim = LaunchEffectPrim(PP_CLOUD, vector3d{});
        if (!prim) {
            continue;
        }

        prim->m_renderFlag = 5u;
        prim->m_duration = m_duration;
        prim->m_totalTexture = 3;
        prim->m_texture.resize(3, nullptr);
        prim->m_texture[0] = ResolveWeatherCloudTexture(mapVariant);
        for (EffectGi& gi : prim->m_gi) {
            gi.life = 1;
            gi.process = 0;
            gi.height[7] = static_cast<float>(mapVariant);
            gi.fullDisplayAngle = 45;
            if (mapVariant == 0) {
                gi.height[6] = -152.0f;
                if (m_effectPos.y > gi.height[6]) gi.process = 300;
            }
            gi.alphaB = 0;
            gi.flag[0] = 0;
            gi.rotStart = static_cast<short>(rand() % 200 + 300);

            if (mapVariant == 0) {
                gi.sparkY = static_cast<float>(rand() % 10 - 125);
                gi.sparkX = static_cast<float>(rand() % 300 - 150);
                gi.sparkZ = static_cast<float>(rand() % 300 - 150);
            } else if (mapVariant == 2) {
                gi.sparkY = static_cast<float>(rand() % 10);
                gi.sparkX = static_cast<float>(rand() % 300 - 150);
                gi.sparkZ = static_cast<float>(rand() % 300 - 150);
            } else if (mapVariant == 3) {
                gi.sparkX = static_cast<float>(rand() % 300 - 150);
                gi.sparkZ = static_cast<float>(rand() % 300 - 150);
                const int heightRandom = rand() % 5;
                C3dAttr* attr = mode && mode->m_world ? mode->m_world->m_attr : nullptr;
                gi.sparkY = attr
                    ? attr->GetHeight(m_effectPos.x + gi.sparkX, m_effectPos.z + gi.sparkZ) - 20.0f - static_cast<float>(heightRandom)
                    : m_effectPos.y - 20.0f - static_cast<float>(heightRandom);
            } else if (mapVariant == 5) {
                gi.sparkY = static_cast<float>(rand() % 10 + 20);
                gi.sparkX = static_cast<float>(rand() % 300 - 150);
                gi.sparkZ = static_cast<float>(rand() % 300 - 150);
            } else {
                gi.sparkY = static_cast<float>(rand() % 10 + 40);
                gi.sparkX = static_cast<float>(rand() % 200 + 25);
                if ((rand() & 1) == 0) gi.sparkX = -gi.sparkX;
                gi.sparkZ = static_cast<float>(rand() % 200 + 25);
                if ((rand() & 1) == 0) gi.sparkZ = -gi.sparkZ;
            }
            gi.sparkX += m_effectPos.x;
            gi.sparkZ += m_effectPos.z;
            gi.height[0] = static_cast<float>(rand() % 360);
            gi.height[1] = static_cast<float>(rand() % 360);
            gi.height[2] = static_cast<float>(rand() % 360);
            gi.height[3] = static_cast<float>(rand() % 360);
            gi.distance = mapVariant == 3
                ? static_cast<float>(rand() % 10 + 35)
                : static_cast<float>(rand() % 20 + 30);
            gi.height[8] = static_cast<float>(rand() % 360);
            switch (mapVariant) {
            case 3: gi.flag[2] = 12; break;
            case 5: gi.flag[2] = 14; break;
            case 7: gi.flag[2] = 16; break;
            case 8: gi.flag[2] = 17; break;
            default: gi.flag[2] = 0; break;
            }
        }
    }
}

void CRagEffect::ProcessGroundSample()
{
    m_effectPos = m_master->m_pos;
    if (m_stateCnt != 0) return;

    CEffectPrim* prim = LaunchEffectPrim(PP_GROUNDSAMPLE, vector3d{});
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_texture.resize(static_cast<size_t>(prim->m_totalTexture), nullptr);
    prim->m_texture[0] = g_texMgr.GetTexture("effect\\magic_target.tga", false);
    for (int band = 0; band < 4; ++band) {
        prim->m_gi[static_cast<size_t>(band)].life = 0;
    }
    prim->m_gi[0].life = 1;
    prim->m_gi[0].rotStart = 0;
}

void CRagEffect::SpawnBlueFall()
{
    if (m_stateCnt != 0 || !m_master) {
        return;
    }

    const vector3d base = ResolveBasePosition();

    if (CEffectPrim* prim = LaunchEffectPrim(PP_WATERFALL, vector3d{})) {
        prim->m_renderFlag = 5u;
        prim->m_duration = m_duration;
        prim->m_alpha = 255.0f;
        prim->m_maxAlpha = 255.0f;
        prim->m_tintColor = RGB(55, 55, 255);

        const std::array<CTexture*, 3> textures = ResolveBlueFallTextures(static_cast<int>(m_param[2]));
        for (CTexture* texture : textures) {
            prim->m_texture.push_back(texture);
        }

        DbgLog("[RagEffect] BlueFall spawn effect=%d base=(%.2f,%.2f,%.2f) tex=%p/%p/%p axis=%d height=%d set=%d\n",
            m_type,
            base.x,
            base.y,
            base.z,
            textures[0],
            textures[1],
            textures[2],
            static_cast<int>(m_param[0]),
            static_cast<int>(m_param[1]),
            static_cast<int>(m_param[2]));

        for (int bandIndex = 0; bandIndex < 4; ++bandIndex) {
            ConfigureBlueFallBand(prim, bandIndex, static_cast<int>(m_param[0]), static_cast<int>(m_param[1]));
        }
    }
}

// HighPriest.exe.c:433666-433827 (0x60B580). Asset bytes are read from
// the listed executable VAs; CResMgr supplies its registered sprite directory.
void CRagEffect::ApplySuperAngel(signed char variant, signed char)
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 0 || (m_stateCnt <= 18 && (variant == 1 || variant == 11) && m_stateCnt % 3 == 0)) {
        CEffectPrim* prim = LaunchEffectPrim(PP_SUPERANGEL, vector3d{});
        prim->m_duration = m_duration;
        EffectGi& gi = prim->m_gi[0];
        gi.process = 0; gi.life = 1; prim->m_size = 1.0f;
        gi.sparkX = m_effectPos.x; gi.sparkY = m_effectPos.y - 40.0f; gi.sparkZ = m_effectPos.z;
        gi.flag[0] = static_cast<u8>(variant); gi.flag[1] = static_cast<u8>(m_stateCnt);
        prim->m_alpha = 0.0f;
        struct Assets { int variant; const char* spr; const char* act; };
        static const Assets assets[] = {
        { 0, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xB5\xBB\xE7.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xB5\xBB\xE7.act" }, // 0x71C13C / 0x71C12C
        { 1, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xB5\xBB\xE7\xB3\xAF\xB0\xB3.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xB5\xBB\xE7\xB3\xAF\xB0\xB3.act" }, // 0x71C118 / 0x71C104
        { 2, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xEA\xC5\xD0" "1.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xEA\xC5\xD0" "1.act" }, // 0x71C0F0 / 0x71C0DC
        { 3, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xEA\xC5\xD0" "2.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xEA\xC5\xD0" "2.act" }, // 0x71C0C8 / 0x71C0B4
        { 4, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xEA\xC5\xD0" "3.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xEA\xC5\xD0" "3.act" }, // 0x71C0A0 / 0x71C08C
        { 10, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7.act" }, // 0x71C078 / 0x71C064
        { 11, "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7(\xB3\xAF\xB0\xB3).spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC7\xD1\xBA\xB9\xC3\xB5\xBB\xE7(\xB3\xAF\xB0\xB3).act" }, // 0x71C048 / 0x71C02C
        };
        for (const Assets& asset : assets) {
            if (asset.variant != variant) continue;
            prim->SetParticleResources(asset.spr, asset.act);
            prim->m_renderFlag = 5;
            break;
        }
        if (variant == 10) {
            prim->m_renderFlag = 0x201;
            gi.sparkY = m_effectPos.y - 30.0f;
        } else if (variant == 11) {
            gi.sparkY = m_effectPos.y - 30.0f;
        }
        prim->m_spriteAction = 0; prim->m_animSpeed = 16; prim->m_curMotion = 0;
        prim->m_repeatAnim = true; prim->m_spriteRepeat = true;
        prim->m_numSegments = 1; // Ref primitive constructor, followed by InitSegment.
        prim->m_segments[0].pos = prim->m_pos;
        prim->m_segments[0].size = prim->m_size;
        prim->m_segments[0].radius = prim->m_radius;
    prim->m_segments[0].longitude = prim->m_longitude;
        prim->m_segments[0].alpha = prim->m_alpha;
        prim->m_segments[0].argb = PackColor(static_cast<u8>(static_cast<long long>(prim->m_alpha)), prim->m_tintColor);
        prim->m_segments[0].transform = prim->m_matrix;
        for (int band = 1; band < 4; ++band) prim->m_gi[static_cast<size_t>(band)].life = 0;
    }
    if ((variant == 0 || variant == 10) && (m_stateCnt == 65 || m_stateCnt == 66)) {
        CEffectPrim* prim = LaunchEffectPrim(PP_SLASH1, vector3d{});
        prim->m_renderFlag = 5; prim->m_duration = m_duration; prim->m_size = 4.0f;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false));
        for (int band = 0; band < 4; ++band) {
            EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
            gi.life = 1; gi.process = 0; gi.maxHeight = 2.0f;
            gi.rotStart = static_cast<short>(90 * band + (m_stateCnt == 66 ? 45 : 0));
            gi.alphaB = 0; gi.distance = 2.0f; gi.height[0] = 1.0f; gi.flag[0] = 0;
        }
    }
}

// Ref/RagEffect.cpp:10838-10844,11947-11950: source call order matters.
void CRagEffect::ProcessSuperAngelEffect()
{
    if (m_type == 338) {
        if (m_stateCnt == 0) PlayWave("levelup.wav", 0.0f, 0.0f, 0.0f, 250, 40, 1.0f);
        ApplySuperAngel(0, 0); ApplySuperAngel(1, 0); ApplySuperAngel(2, 0); ApplySuperAngel(3, 0);
    } else if (m_type == 582) {
        ApplySuperAngel(11, 0); ApplySuperAngel(10, 0);
    }
}

void CRagEffect::SpawnHideStart()
{
    if (m_stateCnt != 0) {
        return;
    }

    CTexture* wingTexture = ResolveEffectTextureCandidates({
        "effect\\wing003.bmp",
        "effect\\wing003.tga",
    }, false);
    if (!wingTexture) {
        return;
    }

    const int offsets[2] = { -14, 14 };
    for (int index = 0; index < 2; ++index) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE, vector3d{})) {
            prim->m_renderFlag = 5;
            prim->m_duration = m_duration;
            prim->m_alpha = 220.0f;
            prim->m_size = 0.8f;
            prim->m_heightSize = 4.5f;
            prim->m_fadeOutCnt = 6;
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_texture.push_back(wingTexture);
            prim->m_deltaPos2.x = static_cast<float>(offsets[index]);
            prim->m_deltaPos2.y = -10.0f;
            prim->m_sizeSpeed = 0.02f;
            prim->m_alphaSpeed = -12.0f;
        }
    }

    if (CEffectPrim* prim = LaunchEffectPrim(PP_2DFLASH, vector3d{})) {
        prim->m_renderFlag = 5;
        prim->m_duration = 10;
        prim->m_alpha = 180.0f;
        prim->m_size = 0.65f;
        prim->m_fadeOutCnt = 4;
        prim->m_tintColor = RGB(235, 245, 255);
        prim->m_alphaSpeed = -18.0f;
    }
}

// Ref/RagEffect.cpp:4613-4702 (0x601450). Keep both discarded RNG draws.
void CRagEffect::SpawnGrimTooth()
{
    const vector3d base = m_effectPos;
    if (m_stateCnt == 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_FrostDiver.wav",base.x-listener.x,0.0f,base.z-listener.z,250,40,1.0f);
        }
        m_roty = static_cast<float>(std::atan2(static_cast<double>(m_deltaPos.x)-base.x,
            -(static_cast<double>(m_deltaPos.z)-base.z))/3.141592*180.0);
        if (m_roty >= 360.0f) m_roty -= 360.0f;
        if (m_roty < 0.0f) m_roty += 360.0f;
        matrix direction; MatrixIdentity(direction);
        MatrixAppendYRotation(direction,GetRadian(static_cast<int>(-m_roty)));
        m_deltaPos2 = {
            direction.m[2][0]+direction.m[2][0]+direction.m[3][0],
            direction.m[2][1]+direction.m[2][1]+direction.m[3][1],
            direction.m[2][2]+direction.m[2][2]+direction.m[3][2]
        };
        m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x,m_deltaPos3.y-m_deltaPos2.y,m_deltaPos3.z-m_deltaPos2.z};
    }
    if (m_flag != 0) return;
    if (m_stateCnt % 3 == 0) {
        const double dx = static_cast<double>(m_deltaPos.x)-base.x, dz = static_cast<double>(m_deltaPos.z)-base.z;
        const float squared = static_cast<float>(dz*dz+dx*dx);
        if (std::sqrt(static_cast<double>(squared)) <= 2.5) m_flag = 1;
        CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN,vector3d{});
        prim->m_pattern |= 1; prim->m_duration = 40;
        const int angle = rand() % 360;
        rand(); GetSin(angle); GetCos(angle);
        prim->m_deltaPos2 = {m_deltaPos3.x,m_deltaPos3.y+20.0f,m_deltaPos3.z};
        prim->m_size = static_cast<float>((rand() % 40 + 60)*0.0099999998);
        prim->m_heightSize = 10.0f;
        prim->m_longitude = static_cast<float>(rand() % 360);
        prim->m_latitude = static_cast<float>(rand() % 30 + 75);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_rotMat = prim->m_matrix;
        prim->m_speed = 3.0f; prim->m_accel = static_cast<float>(prim->m_speed/static_cast<double>(prim->m_duration)*-2.0);
        prim->m_count = 6; prim->m_fadeOutCnt = prim->m_duration-10;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\stone.bmp",false));
    }
    m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x,m_deltaPos3.y-m_deltaPos2.y,m_deltaPos3.z-m_deltaPos2.z};
}

// Ref/RagEffect.cpp:4709-4783 (0x601850), original effect 132.
void CRagEffect::SpawnGrimToothAtk()
{
    if (m_stateCnt != 0) return;
    const vector3d offsets[3] = {{0,40,-12},{12,40,6},{-12,40,6}};
    for (int i = 0; i < 3; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN,vector3d{});
        prim->m_pattern |= 1; prim->m_renderFlag |= 0x200;
        m_duration = 1000; prim->m_duration = 1000;
        GetSin(120*i); GetCos(120*i);
        prim->m_deltaPos2 = offsets[i];
        prim->m_size = 0.89999998f; prim->m_heightSize = 25.0f;
        prim->m_longitude = static_cast<float>(120*(6-i)); prim->m_latitude = 75.0f;
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_rotMat = prim->m_matrix;
        prim->m_speed = 3.5f; prim->m_accel = 0.001f; prim->m_count = 10;
        prim->m_fadeOutCnt = prim->m_duration-10;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\stone.bmp",false));
        m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x,m_deltaPos3.y-m_deltaPos2.y,m_deltaPos3.z-m_deltaPos2.z};
    }
}

// Ref/RagEffect.cpp:6063-6153/6160-6250 (0x5F46F0/0x5FA640), ids43/37.
void CRagEffect::SpawnIncAgility()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    const bool dex = m_type == 43;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 0) {
        if (mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave(dex ? "effect\\EF_IncAgiDex.wav" : "effect\\EF_IncAgility.wav",
                m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
        }
        CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE,vector3d{});
        prim->m_duration = m_duration; prim->m_longitude = 0.0f;
        prim->m_speed = 1.5f;
        prim->m_accel = static_cast<float>(prim->m_speed/static_cast<double>(prim->m_duration)*-1.2);
        prim->m_widthSize = 40.0f; prim->m_heightSize = 20.0f;
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 200.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.06666667);
        prim->m_fadeOutCnt = prim->m_duration-15;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_texture.push_back(g_texMgr.GetTexture(dex ? "effect\\dex_agi_up.bmp" : "effect\\agi_up.bmp",false));
        prim->m_gi[0].life = dex ? 0 : 99;
    }
    if (m_stateCnt % 2 != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCROSSTEXTURE,vector3d{});
    prim->m_duration = 50;
    const int angle = rand() % 360;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix,GetRadian(angle));
    const double radius = rand() % 7 + 2;
    prim->m_deltaPos2 = {
        static_cast<float>(radius*prim->m_matrix.m[2][0]+prim->m_matrix.m[3][0]),
        static_cast<float>(radius*prim->m_matrix.m[2][1]+prim->m_matrix.m[3][1]),
        static_cast<float>(radius*prim->m_matrix.m[2][2]+prim->m_matrix.m[3][2])
    };
    // Source replaces the radial Y matrix with a pure X rotation for motion.
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix,GetRadian(90));
    prim->m_speed = static_cast<float>((rand() % 50 + 20)*0.0099999998);
    prim->m_widthSize = static_cast<float>((rand() % 60 + 30)*0.1);
    prim->m_heightSize = 0.18000001f;
    prim->m_alpha = 0.0f; prim->m_maxAlpha = 200.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.050000001);
    prim->m_fadeOutCnt = prim->m_duration-20;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ac_center2.tga",false));
    prim->m_gi[0].life = dex ? 0 : 99;
}

// Ref/RagEffect.cpp:3016-3107 (0x5FAA20), original effect 38.
void CRagEffect::SpawnDecAgility()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 0) {
        if (mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_DecAgility.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
        }
        CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE,vector3d{});
        prim->m_duration = m_duration; prim->m_deltaPos2.y -= 80.0f;
        prim->m_longitude = 180.0f; prim->m_speed = 1.0f;
        prim->m_accel = static_cast<float>(-(prim->m_speed/static_cast<double>(prim->m_duration)));
        prim->m_widthSize = 40.0f; prim->m_heightSize = 10.0f;
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 200.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.06666667);
        prim->m_fadeOutCnt = prim->m_duration-15;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\slow.bmp",false));
        prim->m_gi[0].life = 0;
    }
    if (m_stateCnt % 2 != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCROSSTEXTURE,vector3d{});
    prim->m_duration = 50;
    const int angle = rand() % 360;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix,GetRadian(angle));
    const double radius = rand() % 7+2;
    prim->m_deltaPos2 = {
        static_cast<float>(radius*prim->m_matrix.m[2][0]+prim->m_matrix.m[3][0]),
        static_cast<float>(radius*prim->m_matrix.m[2][1]+prim->m_matrix.m[3][1]),
        static_cast<float>(radius*prim->m_matrix.m[2][2]+prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos2.y -= 20.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix,GetRadian(-90));
    prim->m_accel = 0.015f;
    prim->m_widthSize = static_cast<float>((rand() % 60+30)*0.1);
    prim->m_heightSize = 0.18000001f;
    prim->m_alpha = 0.0f; prim->m_maxAlpha = 200.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.050000001);
    prim->m_fadeOutCnt = prim->m_duration-20;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ac_center2.tga",false));
    prim->m_gi[0].life = 0;
}

// Ref/RagEffect.cpp:6954-6974 (0x5FB5C0), original effect 53.
void CRagEffect::SpawnPoisonHit()
{
    if (m_stateCnt != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE,vector3d{});
    prim->m_duration = m_duration; prim->m_size = 1.5f;
    prim->m_animSpeed = 2; prim->m_pattern |= 0x200; prim->m_repeatAnim = false;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
    prim->m_numSegments = 1;
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\PoisonHit.spr",
        "\xC0\xCC\xC6\xD1\xC6\xAE\\PoisonHit.act"); // 0x71A718 / 0x71A700
    InitReferenceSegments(prim);
}

// Ref/RagEffect.cpp:1774-1798 (0x5FAE10), original effect 39.
void CRagEffect::SpawnAqua()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_Aqua.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE,vector3d{});
    prim->m_duration = m_duration; prim->m_deltaPos2.y = -20.0f;
    prim->m_animSpeed = 2; prim->m_repeatAnim = false;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
    prim->m_numSegments = 1;
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xBA\xBC\xF6\xB6\xDF\xB1\xE2.spr",
        "\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xBA\xBC\xF6\xB6\xDF\xB1\xE2.act");
    InitReferenceSegments(prim);
}

// Ref/RagEffect.cpp:2125-2219 (0x5F4260). Preserve all three emission
// branches and the recurring particles' action-before-sprite resource order.
void CRagEffect::SpawnBlessing()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_Blessing.wav", m_effectPos.x - listener.x, 0.0f,
                m_effectPos.z - listener.z, 250, 40, 1.0f); // 0x712828
        }
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
        prim->m_duration = m_duration;
        prim->m_pattern |= 1;
        prim->m_renderFlag |= 0x200;
        prim->m_radius = 10.0f; prim->m_latitude = 90.0f;
        prim->m_tintColor = RGB(32,176,232);
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 100.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.033333335);
        prim->m_fadeOutCnt = prim->m_duration - 30;
        prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga", false)); // 0x719498
    }
    if (m_stateCnt % 3 == 0 && m_stateCnt < 10) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
        prim->m_duration = m_duration;
        prim->m_animSpeed = 2;
        prim->m_alpha = static_cast<float>(200 - 6 * m_stateCnt);
        prim->m_deltaPos2 = {0.0f,-25.0f,0.0f};
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xE0\xBA\xB9.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xE0\xBA\xB9.act"); // 0x71B2C4 / 0x71B2B4
        prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
        prim->m_repeatAnim = true; prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
    }
    if (m_stateCnt % 4 == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
        prim->m_duration = 80;
        prim->m_pattern |= 0x80;
        const float angle = GetRadian(rand() % 360);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, angle);
        const double radius = static_cast<double>(rand() % 7 + 2);
        prim->m_deltaPos2 = {
            static_cast<float>(radius * prim->m_matrix.m[2][0] - prim->m_matrix.m[1][0] * 25.0 + prim->m_matrix.m[3][0]),
            static_cast<float>(radius * prim->m_matrix.m[2][1] - prim->m_matrix.m[1][1] * 25.0 + prim->m_matrix.m[3][1]),
            static_cast<float>(radius * prim->m_matrix.m[2][2] - prim->m_matrix.m[1][2] * 25.0 + prim->m_matrix.m[3][2])
        };
        const float riseAngle = GetRadian(-90);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix, riseAngle);
        prim->m_speed = static_cast<float>(static_cast<double>(rand() % 20 + 20) * 0.0099999998);
        prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
        prim->m_alpha = 0.0f; prim->m_size = 0.5f;
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5;
        prim->m_maxAlpha = 254.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.1);
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle6.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle6.act", true); // 0x71B284 / 0x71B29C
        prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
    }
}

// Ref/RagEffect.cpp:7721-7761 (0x5F6250). Three RNG draws per smoke
// follow the initial count draw, in X-angle / Y-angle / speed order.
void CRagEffect::SpawnSmoke()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt != 0) return;
    const int count = rand() % 4 + 1;
    for (int i = 0; i < count; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
        prim->m_duration = m_duration;
        const float xAngle = GetRadian(rand() % 30 + 75);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix, xAngle);
        const float yAngle = GetRadian(rand() % 360);
        MatrixAppendYRotation(prim->m_matrix, yAngle);
        prim->m_deltaPos2.y = -9.0f;
        prim->m_rollSpeed = 0.75f;
        prim->m_speed = static_cast<float>(static_cast<double>(rand() % 3 + 3) * 0.1);
        prim->m_size = 1.5f;
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.act"); // 0x71B440 / 0x71B42C
        // Ref constructor defaults, unlike the replaced synthetic fade-in.
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_alphaSpeed = 0.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
    }
}

// Ref/RagEffect.cpp:8826-8852 (0x5F69B0). Size and divisor defaults modify
// the parent's parameters; constructor alpha remains 254, without a fade-in.
void CRagEffect::SpawnTorch()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
    prim->m_pattern |= 0x200;
    prim->m_duration = m_duration;
    prim->m_deltaPos2.y = -10.0f;
    if (m_param[0] <= 0.0f) m_param[0] = 1.0f;
    if (m_param[1] < 1.0f) m_param[1] = 1.0f;
    prim->m_size = m_param[0];
    prim->m_animSpeed = static_cast<int>(m_param[1]);
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\torch_01.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\torch_01.act"); // 0x71B47C / 0x71B468
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_alphaSpeed = 0.0f;
    prim->m_fadeOutCnt = -1; prim->m_repeatAnim = true; prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
}

// Ref/RagEffect.cpp:3442-3555 (0x601B10/0x601D70): the two emitters
// share this profile setup, with four RNG draws in angle/radius/speed/size order.
void ConfigureEnchantPoisonParticle(CEffectPrim* prim, int speedSpan, int sizeBase, int sizeSpan)
{
    prim->m_duration = 40;
    const float angle = GetRadian(rand() % 360);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, angle);
    const double radius = static_cast<double>(rand() % 6 + 2);
    prim->m_deltaPos2 = {
        static_cast<float>(radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    const float riseAngle = GetRadian(90);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, riseAngle);
    prim->m_speed = static_cast<float>(static_cast<double>(rand() % speedSpan + 30) * 0.0099999998);
    prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
    prim->m_size = static_cast<float>(static_cast<double>(rand() % sizeSpan + sizeBase) * 0.0099999998);
    prim->m_rollSpeed = 3.0f;
    prim->m_alpha = 0.0f;
    // Reference constructor defaults for these linear particle callers.
    prim->m_maxAlpha = 254.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.1);
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5;
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.act"); // 0x71BC24 / 0x71BC0C
    prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
}

// Ref preserves this effect's stored origin, rather than copying its actor.
void CRagEffect::SpawnEnchantPoison()
{
    if (m_stateCnt == 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\assasin_enchantpoison.wav", m_effectPos.x - listener.x, 0.0f,
                m_effectPos.z - listener.z, 250, 40, 1.0f); // 0x71BC58
            PlayWave("effect\\EF_PoisonAttack.wav", m_effectPos.x - listener.x, 0.0f,
                m_effectPos.z - listener.z, 250, 40, 1.0f); // 0x71BC3C
        }
    }
    if (m_stateCnt % 5 == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
        ConfigureEnchantPoisonParticle(prim, 30, 40, 50);
    }
}

void CRagEffect::SpawnEnchantPoison2()
{
    if (m_stateCnt == 0 || m_stateCnt == 5 || m_stateCnt == 11 || m_stateCnt == 18 || m_stateCnt == 26 || m_stateCnt == 40) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\assasin_enchantpoison.wav", m_effectPos.x - listener.x, 0.0f,
                m_effectPos.z - listener.z, 250, 40, 1.0f); // 0x71BC58
        }
    }
    if (m_stateCnt % 3 == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
        ConfigureEnchantPoisonParticle(prim, 50, 20, 20);
    }
}

// Ref/RagEffect.cpp:11325-11342 (OnProcess3 case 0x1ED); LABEL_206 at
// 12053 sets white on odd frames. The helper above has no tint side effect.
void CRagEffect::ProcessEnchantPoison2()
{
    SpawnEnchantPoison2();
    if (m_stateCnt >= 81) return;
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    if (m_stateCnt % 2 != 0) {
        master->SetArgb(-1, 255, 255, 255);
    } else {
        if ((master->m_BodyLight & 1) == 0) ++master->m_BodyLight;
        master->SetArgb(-1, 255, 0, 255);
    }
}

// Ref/RagEffect.cpp:7327-7411 / 7418-7477 (0x5F7E70/0x5F8120).
// Both use PP_3DPARTICLE; the stored acceleration is inactive without 0x400.
void CRagEffect::ApplySight(float initialAlpha, bool includeShadow)
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos, mode->m_view->GetViewMatrix(), projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt % 2 != 0) return;
    const int angle = static_cast<int>(static_cast<double>(m_stateCnt) * -5.0);
    const float sine = GetSin(angle), cosine = GetCos(angle);
    GetRadian(angle); // Ref discards this result.
    const float offsetX = sine * 15.0f, offsetZ = cosine * -15.0f;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
    prim->m_duration = 20;
    prim->m_deltaPos2 = {offsetX,-20.0f,offsetZ};
    prim->m_deltaPosAccel.y = 0.1f;
    prim->m_animSpeed = 3;
    prim->m_size = 2.5f; prim->m_sizeSpeed = -0.1f;
    prim->m_alpha = initialAlpha; prim->m_alphaSpeed = -3.0f;
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\Sight.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\Sight.act"); // 0x71B578 / 0x71B564
    prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
    prim->m_repeatAnim = true; prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
    if (includeShadow) {
        prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
        prim->m_duration = 20;
        prim->m_deltaPos2 = {offsetX,0.0f,offsetZ};
        prim->m_animSpeed = 2;
        prim->m_size = 1.5f; prim->m_sizeSpeed = -0.059999999f;
        prim->m_alpha = 120.0f; prim->m_alphaSpeed = -1.0f;
        prim->SetParticleResources("Shadow.spr", "Shadow.act"); // 0x71B558 / 0x71B54C
        prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
        prim->m_repeatAnim = true; prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
    }
}

void CRagEffect::SpawnSight()
{
    ApplySight(150.0f, true);
}

void CRagEffect::SpawnSight2()
{
    ApplySight(50.0f, false);
}

// Ref/RagEffect.cpp:4057-4125 (0x5F8310), original effect 24.
// CalcDir/CalcDist use the primitive's stored world origin and absolute target.
void CRagEffect::SpawnFireBall()
{
    if (m_stateCnt != 20 && m_stateCnt != 24 && m_stateCnt != 28 && m_stateCnt != 32) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_FireBall.wav", m_effectPos.x - listener.x, 0.0f,
            m_effectPos.z - listener.z, 250, 40, 1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
    prim->m_duration = 20;
    prim->m_pattern |= 0x120;
    prim->m_deltaPos2 = {0.0f,-15.0f,0.0f};
    const double dx = static_cast<double>(m_deltaPos.x) - prim->m_pos.x;
    const double dz = static_cast<double>(m_deltaPos.z) - prim->m_pos.z;
    const double angle = std::atan2(dx, -dz) / 3.141592 * 180.0;
    prim->m_roty = static_cast<float>(angle);
    if (angle >= 360.0) prim->m_roty = static_cast<float>(angle - 360.0);
    if (prim->m_roty < 0.0f) prim->m_roty = static_cast<float>(prim->m_roty + 360.0);
    prim->m_longitude = -prim->m_roty;
    prim->m_roty = static_cast<float>(180.0 - prim->m_roty);
    if (prim->m_roty < 0.0f) prim->m_roty = static_cast<float>(prim->m_roty + 360.0);
    const float squared = static_cast<float>(dz * dz + dx * dx);
    prim->m_speed = static_cast<float>(-(std::sqrt(static_cast<double>(squared)) / prim->m_duration));
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_size = 1.3f;
    prim->m_animSpeed = 1;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_fadeOutCnt = -1; prim->m_repeatAnim = true; prim->m_numSegments = 1;
    if (m_stateCnt != 20) {
        prim->m_tintColor = RGB(246,199,76);
        if (m_stateCnt == 24) prim->m_alpha = 180.0f;
        else if (m_stateCnt == 28) prim->m_alpha = 130.0f;
        else prim->m_alpha = 80.0f;
    }
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\fireball.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\fireball.act"); // 0x71B5A0 / 0x71B58C
    ShiftCircleSegments(prim);
}

// Ref/RagEffect.cpp:4127-4192 (0x5F63F0), original effect 45.
void CRagEffect::SpawnFireFly()
{
    if (m_stateCnt != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
    prim->m_pattern |= 0x84;
    prim->m_duration = 140; m_duration = 140;
    prim->m_speed = static_cast<float>(static_cast<double>(rand() % 35 + 15) * 0.0099999998);
    prim->m_longitude = static_cast<float>(rand() % 360);
    prim->m_latitude = static_cast<float>(rand() % 360);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    const double radius = rand() % 13 + 2;
    prim->m_deltaPos2 = {
        static_cast<float>(radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos2.y - 10.0);
    prim->m_count = rand() % 25 + 5;
    prim->m_size = static_cast<float>(static_cast<double>(rand() % 70 + 30) * 0.0099999998);
    prim->m_alpha = 0.0f; prim->m_maxAlpha = 80.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.66666669);
    prim->m_AlphaShow = true; prim->m_chCnt = 0;
    prim->m_chPoint = {40.0f,100.0f};
    prim->m_chVal1 = {120.0f,0.0f};
    prim->m_chVal2 = {200.0f,80.0f};
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act"); // 0x71B200 / 0x71B1E8
    prim->m_fadeOutCnt = -1; prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
}

// Ref/RagEffect.cpp:7260-7321 (0x5F6740), original effect 46.
void CRagEffect::SpawnSandWind()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos, mode->m_view->GetViewMatrix(), projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt >= 60 || m_stateCnt % 2 != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE, vector3d{});
    prim->m_pattern = 8;
    prim->m_duration = m_duration - rand() % 100;
    prim->m_speed = static_cast<float>(static_cast<double>(rand() % 300 + 260) * 0.0099999998);
    prim->m_longitude = static_cast<float>(rand() % 15 + 130);
    prim->m_widthSize = 30.0f; prim->m_heightSize = 30.0f;
    prim->m_deltaPos2.x = static_cast<float>(rand() % 640 - 500);
    prim->m_deltaPos2.y = static_cast<float>(rand() % 50 - 240);
    prim->m_count = rand() % 10 + 10;
    prim->m_alpha = 128.0f; prim->m_maxAlpha = 254.0f;
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
    prim->m_totalTexture = 1; prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\sand1.bmp", false));
    prim->m_gi[0].life = 0;
}

// Ref/RagEffect.cpp:8461-8525 (0x5F5F40), original effect 18.
void CRagEffect::SpawnSteal()
{
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_Steal.wav", m_effectPos.x - listener.x, 0.0f,
            m_effectPos.z - listener.z, 250, 40, 1.0f); // 0x71B418
    }
    for (int i = 0; i < 10; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEGRAVITY, vector3d{});
        prim->m_longitude = static_cast<float>(rand() % 360);
        prim->m_latitude = static_cast<float>(rand() % 100 - 50);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        const double radius = rand() % 4 + 4;
        prim->m_deltaPos3 = {
            static_cast<float>(radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
        prim->m_deltaPos2 = prim->m_deltaPos3;
        prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos3.y - 10.0);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_duration = m_duration;
        prim->m_speed = static_cast<float>(static_cast<double>(rand() % 50 + 50) * 0.0099999998);
        prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
        prim->m_gravSpeed = static_cast<float>(static_cast<double>(rand() % 90 + 60) * -0.0099999998);
        prim->m_gravAccel = static_cast<float>(prim->m_gravSpeed / static_cast<double>(prim->m_duration) * -2.0);
        prim->m_size = static_cast<float>(static_cast<double>(rand() % 100 + 100) * 0.0099999998);
        prim->m_sizeSpeed = static_cast<float>(-(prim->m_size / static_cast<double>(prim->m_duration)));
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle7.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle7.act"); // 0x71B400 / 0x71B3E8
        ShiftCircleSegments(prim);
    }
}

// Ref/RagEffect.cpp:225-277 (0x5F7770), original effect 10.
bool CRagEffect::ProcessCoin()
{
    if (!g_session.m_isMinEffect && m_stateCnt == 20) {
        for (int i = 0; i < 5; ++i) {
            CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEGRAVITY, vector3d{});
            prim->m_duration = rand() % 30 + 40;
            prim->m_longitude = static_cast<float>(30 * (rand() % 12));
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
            const double radius = rand() % 2 + 2;
            prim->m_deltaPos2 = {
                static_cast<float>(radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
                static_cast<float>(radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
                static_cast<float>(radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
            };
            prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos2.y - 10.0);
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
            prim->m_speed = static_cast<float>(static_cast<double>(rand() % 50 + 50) * 0.0099999998);
            prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.2857143);
            prim->m_gravSpeed = static_cast<float>(static_cast<double>(rand() % 100 + 50) * -0.039999999);
            prim->m_gravAccel = static_cast<float>(-(prim->m_gravSpeed / static_cast<double>(prim->m_duration) * 4.0));
            prim->m_size = 2.0f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
            prim->m_playWaveType = 1; prim->m_animSpeed = 1;
            prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_repeatAnim = true;
            prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\\xB5\xBF\xC0\xFC.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\\xB5\xBF\xC0\xFC.act"); // 0x71B510 / 0x71B500
            prim->m_numSegments = 2;
            InitReferenceSegments(prim);
        }
    }
    ProcessEZ2STR(0.0f,-80.0f,0.0f); // Original deliberately ignores completion.
    return m_stateCnt < 100;
}

// Ref/RagEffect.cpp:5141-5220,5802-5882: Hit1/HitDark particle groups.
void LaunchHitParticle(CRagEffect* effect, int facing, bool gravity, bool dark, bool shrink, int speedRange = 90, int speedBase = 60)
{
    CEffectPrim* prim = effect->LaunchEffectPrim(gravity ? PP_3DPARTICLEGRAVITY : PP_3DPARTICLE, vector3d{});
    prim->m_longitude = static_cast<float>(facing + rand() % 80 + (gravity ? 140 : -40));
    prim->m_latitude = static_cast<float>(rand() % 100 - 50);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    const double radius = rand() % 4 + 4;
    prim->m_deltaPos3 = {
        static_cast<float>(radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
        static_cast<float>(radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
        static_cast<float>(radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
    };
    prim->m_deltaPos2 = prim->m_deltaPos3;
    prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos3.y - 10.0);
    if (gravity) {
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_duration = rand() % 24 + 6;
    }
    prim->m_speed = static_cast<float>(static_cast<double>(rand() % speedRange + speedBase) * 0.0099999998);
    if (!gravity) prim->m_duration = rand() % 24 + 6;
    prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
    if (gravity) {
        prim->m_gravSpeed = static_cast<float>(static_cast<double>(rand() % 90 + 30) * -0.0099999998);
        const double acceleration = prim->m_gravSpeed / static_cast<double>(prim->m_duration);
        prim->m_gravAccel = static_cast<float>(-(acceleration + acceleration));
    }
    prim->m_size = static_cast<float>(static_cast<double>(rand() % 100 + 60) * 0.0099999998);
    if (shrink) prim->m_sizeSpeed = static_cast<float>(-(prim->m_size / static_cast<double>(prim->m_duration)));
    if (dark) prim->m_tintColor = RGB(20,20,20);
    prim->m_fadeOutCnt = 0;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act");
    prim->m_numSegments = 3;
    InitReferenceSegments(prim);
}

// Ref/RagEffect.cpp:5087-5224 (0x5F1A40), original effect 0.
void CRagEffect::SpawnHit(bool dark)
{
    if (m_stateCnt != 0) return;
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    int facing = static_cast<int>(180.0 - master->m_roty);
    if (facing < 0) facing += 360;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{});
    prim->m_duration = 10;
    prim->m_longitude = static_cast<float>(facing);
    prim->m_latitude = -90.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_deltaPos2 = {0.0f,-10.0f,0.0f};
    prim->m_speed = 0.69999999f;
    prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
    prim->m_outerSize = 10.0f; prim->m_innerSize = 5.0f; prim->m_heightSize = 3.5f;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false));
    if (dark || !g_session.m_isMinEffect) {
        for (int i = 0; i < 2; ++i) LaunchHitParticle(this, facing, false, dark, false);
        for (int i = 0; i < 2; ++i) LaunchHitParticle(this, facing, true, dark, false);
    }
}

void CRagEffect::SpawnHit1()
{
    SpawnHit(false);
}

// Ref/RagEffect.cpp:5747-5885 (0x603A60), ids 180/184.
void CRagEffect::SpawnHitDark()
{
    SpawnHit(true);
}

// Ref/RagEffect.cpp:6808-6947 (0x5FDF90), original effect 81.
void CRagEffect::SpawnPierce()
{
    if (m_stateCnt % 20 != 0 || m_stateCnt >= 20 * m_level) return;
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    int facing = static_cast<int>(180.0 - master->m_roty);
    if (facing < 0) facing += 360;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{});
    prim->m_duration = 15;
    prim->m_longitude = static_cast<float>(facing); prim->m_latitude = -90.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_deltaPos2 = {0.0f,-10.0f,0.0f};
    prim->m_speed = 0.69999999f;
    prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
    prim->m_outerSize = static_cast<float>(static_cast<double>(m_stateCnt) * 0.1 + 11.0);
    prim->m_innerSize = static_cast<float>(static_cast<double>(m_stateCnt) * 0.1 + 6.0);
    prim->m_heightSize = 2.5f;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_yellow.tga", false));
    if (m_stateCnt > 0) {
        for (int i = 0; i < 4; ++i) LaunchHitParticle(this, facing, false, false, true);
        for (int i = 0; i < 4; ++i) LaunchHitParticle(this, facing, true, false, true);
    }
}

// Ref/RagEffect.cpp:5230-5323 (0x5F2100), original Hit2 options 0/1.
void CRagEffect::SpawnHit2(int option)
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos, mode->m_view->GetViewMatrix(), projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    if (mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_hit2.wav", m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i * 45);
        CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE, vector3d{});
        prim->m_pattern = 1;
        prim->m_duration = rand() % 20 + 10;
        prim->m_roll = static_cast<float>(angle - 15.0 + rand() % 30);
        prim->m_longitude = prim->m_roll;
        prim->m_speed = static_cast<float>(static_cast<double>(rand() % 45 + 5) * 0.1);
        prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
        prim->m_widthSize = static_cast<float>(rand() % 15 + 5);
        prim->m_heightSize = static_cast<float>(rand() % 20 + 20);
        prim->m_widthSpeed = static_cast<float>(-(prim->m_widthSize / static_cast<double>(prim->m_duration)));
        prim->m_heightSpeed = 1.5f; prim->m_heightAccel = 0.25f;
        const float length = static_cast<float>(rand() % 5);
        prim->m_deltaPos2.x = GetSin(static_cast<int>(angle)) * length;
        prim->m_deltaPos2.y = static_cast<float>(-(GetCos(static_cast<int>(angle)) * length));
        prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos2.y - 20.0);
        if (option != 0) prim->m_tintColor = RGB(120,120,255);
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 254.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.125);
        prim->m_gi[0].life = 0;
        prim->m_fadeOutCnt = -1; prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_texture.push_back(g_texMgr.GetTexture(i % 2 == 0 ? "effect\\lens1.tga" : "effect\\lens2.tga", false));
    }
}

// HighPriest.exe.c:420528-420542, original id 183.
bool CRagEffect::ProcessHit2Blue()
{
    if (m_stateCnt == 0) SpawnHit2(1);
    else if (m_stateCnt == 3 || m_stateCnt == 7 || m_stateCnt == 9) {
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) mode->SendMsg(135,1,0,0);
    }
    return ProcessEZ2STR(0.0f,-80.0f,0.0f);
}

// Ref/RagEffect.cpp:6626-6702 (0x5F9CD0), original id 32.
void CRagEffect::SpawnNapalmBeat()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    // Ref retains this comparison against the remainder, not an equality gate.
    if (!(m_stateCnt < 15 && m_stateCnt >= m_stateCnt % 5)) return;
    if (m_stateCnt == 0 && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_NapalmBeat.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE,vector3d{});
    prim->m_duration = 30;
    const float length = static_cast<float>(rand() % 50 + 10);
    const int angle = rand() % 360;
    prim->m_deltaPos2.x = GetSin(angle) * length;
    prim->m_deltaPos2.y = static_cast<float>(-(GetCos(angle) * length));
    prim->m_deltaPos2.y = static_cast<float>(prim->m_deltaPos2.y - 20.0);
    prim->m_heightSize = static_cast<float>(rand() % 40 + 20);
    prim->m_widthSize = prim->m_heightSize;
    prim->m_alpha = 0.0f; prim->m_maxAlpha = 254.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.1);
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
    prim->m_repeatAnim = false; prim->m_animSpeed = 2; prim->m_totalTexture = 8;
    for (int i = 0; i < prim->m_totalTexture; ++i) {
        char path[128];
        // Original format bytes at 0x71B730 (CP949), including uppercase extension.
        std::snprintf(path,sizeof(path),"effect\\\xC6\xF8\xB9\xDF%d.TGA",i+1);
        prim->m_texture.push_back(g_texMgr.GetTexture(path,false));
    }
    prim->m_gi[0].life = 0;
}

// HighPriest.exe.c:421318-421337, original id 401.
void CRagEffect::ApplyNapalmHit()
{
    if (m_stateCnt != 20 && m_stateCnt != 30 && m_stateCnt != 40
        && m_stateCnt != 50 && m_stateCnt != 60) return;
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_NapalmBeat.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    // The original helper only emits at frame zero; do not reset its frame here.
    SpawnHit2(0);
}

// Ref/RagEffect.cpp:5330-5442 (0x5F2480), original effect 2.
void CRagEffect::SpawnHit3()
{
    if (m_stateCnt != 0) return;
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_hit3.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    int facing = static_cast<int>(180.0 - master->m_roty);
    if (facing < 0) facing += 360;
    for (int i = 0; i < 2; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER,vector3d{});
        prim->m_duration = 15;
        prim->m_longitude = static_cast<float>(facing); prim->m_latitude = -90.0f;
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_deltaPos2 = {0.0f,-10.0f,0.0f};
        prim->m_speed = 0.69999999f;
        prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
        prim->m_outerSize = i == 0 ? 1.5f : 4.0f;
        prim->m_innerSize = 1.5f;
        prim->m_heightSpeed = i == 0 ? 0.5f : 0.25f;
        prim->m_heightAccel = 0.2f;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\lens2.tga",false));
    }
    for (int i = 0; i < 8; ++i) LaunchHitParticle(this,facing,false,false,true,120,80);
}

// Ref/RagEffect.cpp:5449-5578 (0x5F2AB0), original effect 3.
void CRagEffect::SpawnHit4()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    if (mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_hit4.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    int facing = static_cast<int>(180.0 - master->m_roty);
    if (facing < 0) facing += 360;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER,vector3d{});
    prim->m_duration = 15;
    prim->m_longitude = static_cast<float>(facing); prim->m_latitude = -90.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_deltaPos2 = {0.0f,-10.0f,0.0f};
    prim->m_speed = 0.69999999f;
    prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.5);
    prim->m_outerSize = 4.0f; prim->m_innerSize = 0.5f;
    prim->m_heightSpeed = 0.25f; prim->m_heightAccel = 0.15000001f;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\lens2.tga",false));
    for (int i = 0; i < 5; ++i) LaunchHitParticle(this,facing,false,false,true);
}

// Ref/RagEffect.cpp:5585-5739 (0x5F2FA0/0x5F3220), Hit5/Hit6.
void CRagEffect::SpawnCrossLensHit(bool compact)
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    if (mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave(compact ? "effect\\EF_hit6.wav" : "effect\\EF_hit5.wav",
            m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    const int angle = rand() % 360;
    for (int i = 0; i < 2; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE,vector3d{});
        prim->m_duration = 17;
        prim->m_deltaPos2.y = -20.0f;
        prim->m_roll = static_cast<float>(angle + 90 * i);
        prim->m_rollSpeed = -5.0f;
        prim->m_rollAccel = static_cast<float>(prim->m_rollSpeed / static_cast<double>(prim->m_duration) * -0.66666669);
        prim->m_widthSize = static_cast<float>(rand() % 5 + (compact ? 10 : 20));
        prim->m_heightSize = static_cast<float>(rand() % (compact ? 5 : 10) + (compact ? 15 : 30));
        prim->m_widthSpeed = static_cast<float>(-(prim->m_widthSize / static_cast<double>(prim->m_duration)));
        prim->m_heightSpeed = compact ? 1.7f : 2.5f;
        prim->m_heightAccel = 0.5f;
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\lens2.tga",false));
        prim->m_gi[0].life = 0;
    }
}

void CRagEffect::SpawnHit5()
{
    SpawnCrossLensHit(false);
}

void CRagEffect::SpawnHit6()
{
    SpawnCrossLensHit(true);
}

// Ref/RagEffect.cpp:7202-7261 (0x5F9170). The angle truncates before
// the trig-table calls. Particle2 and Shadow retain their original profiles.
void CRagEffect::SpawnRuwach()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt % 3 != 0) return;
    const int angle = static_cast<int>(static_cast<double>(m_stateCnt) * 0.7692307692307692 * -5.0);
    const float sine = GetSin(angle);
    const float cosine = GetCos(angle);
    GetRadian(angle); // Ref consumes the result without using it.
    const float offsetX = sine * 15.0f;
    const float offsetZ = cosine * -15.0f;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
    prim->m_duration = 25;
    prim->m_deltaPos2 = {offsetX, -10.0f, offsetZ};
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle2.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle2.act"); // 0x71B334 / 0x71B31C
    prim->m_size = 3.0f; prim->m_sizeSpeed = -0.1f;
    prim->m_alpha = 250.0f; prim->m_alphaSpeed = -3.0f;
    prim->m_fadeOutCnt = prim->m_duration - 6;
    // Ref constructor defaults not yet shared by the remaining legacy emitters.
    prim->m_maxAlpha = 254.0f; prim->m_animSpeed = 4;
    prim->m_repeatAnim = true; prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
    prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{});
    prim->m_duration = 25;
    prim->m_deltaPos2 = {offsetX, 0.0f, offsetZ};
    prim->SetParticleResources("Shadow.spr", "Shadow.act"); // 0x71B558 / 0x71B54C
    prim->m_animSpeed = 2;
    prim->m_size = 1.5f; prim->m_sizeSpeed = -0.059999999f;
    prim->m_alpha = 150.0f; prim->m_alphaSpeed = -2.5f;
    prim->m_fadeOutCnt = prim->m_duration - 6;
    prim->m_maxAlpha = 254.0f; prim->m_repeatAnim = true; prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
}

bool EffectTicksWhenNoFrameTime(int type)
{
    switch (type) {
    case 25:
    case 37:
    case 49:
    case 51:
    case 52:
    case 53:
    case 147:
    case 152:
    case 200:
    case 201:
    case 202:
    case 203:
    case 228:
    case 254:
    case 312:
    case 313:
    case 320:
    case 325:
    case 328:
    case 362:
    case 397:
    case 398:
    case 418:
    case 436:
    case 437:
    case 438:
    case 441:
    case 454:
    case 467:
    case 468:
    case 469:
    case 470:
    case 471:
    case 472:
    case 473:
    case 488:
    case 489:
    case 490:
    case 503:
    case 504:
    case 506:
    case 523:
    case 524:
    case 525:
    case 526:
    case 527:
    case 528:
    case 529:
    case 530:
    case 531:
    case 532:
    case 533:
    case 534:
    case 535:
    case 536:
    case 590:
    case 603:
    case 629:
    case 678:
    case 679:
    case 680:
        return true;
    default:
        return false;
    }
}

void ConfigureHealBand(EffectGi& gi, float maxHeight, short rotStart, float distance, short riseAngle, short alphaPeak)
{
    gi.life = 1;
    gi.process = 0;
    gi.fullDisplayAngle = 360;
    gi.maxHeight = maxHeight;
    gi.rotStart = rotStart;
    gi.alphaB = 0;
    gi.distance = distance;
    gi.riseAngle = riseAngle;
    gi.sparkY = 0.0f;
    gi.alphaT = alphaPeak;
    gi.height.fill(0.0f);
    gi.flag.fill(0);
}

void CRagEffect::LaunchParticleUp(const vector3d& origin, float sizeBase, float speedBase, bool secondTexture, u8 switchTexture)
{
    CEffectPrim* spark = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeParticleUp), vector3d{});
    if (spark == nullptr) {
        return;
    }
    spark->m_renderFlag = 5;
    spark->m_duration = m_duration;
    spark->m_size = 0.0f;
    spark->m_pos = origin;
    spark->m_texture.push_back(g_texMgr.GetTexture("effect\\pok1.tga", false));
    if (secondTexture) {
        spark->m_texture.push_back(g_texMgr.GetTexture("effect\\pok3.tga", false));
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = spark->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.distance = static_cast<float>(rand() % 10) * 0.1f + sizeBase;
        gi.height[8] = 0.0f;
        gi.height[1] = 0.0f;
        gi.height[2] = static_cast<float>(rand() % 40) * 0.0099999998f + speedBase;
        gi.alphaB = 0;
        gi.sparkX = static_cast<float>(rand() % 13) + origin.x - 6.0f;
        gi.sparkZ = static_cast<float>(rand() % 13) + origin.z - 6.0f;
        gi.sparkY = origin.y;
        gi.rotStart = 0;
        gi.fullDisplayAngle = 0;
        gi.flag[0] = 0;
        gi.flag[1] = switchTexture;
        gi.flag[2] = 11;
    }
}

void CRagEffect::ApplyParticleUpBurst()
{
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    for (int burst = 0; burst < 20; ++burst) {
        CEffectPrim* spark = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeParticleUp), vector3d{});
        if (spark == nullptr) {
            return;
        }
        spark->m_renderFlag = 5;
        spark->m_duration = m_duration;
        spark->m_size = 0.0f;
        spark->m_pos = origin;
        spark->m_texture.push_back(g_texMgr.GetTexture("effect\\thunder_center.bmp", false));
        for (int band = 0; band < 4; ++band) {
            EffectGi& gi = spark->m_gi[static_cast<size_t>(band)];
            gi.life = 1;
            gi.process = rand() % 10 - 10;
            gi.distance = static_cast<float>(rand() % 16) * 0.029999999f + 0.1f;
            gi.height[8] = 0.0f;
            gi.height[1] = static_cast<float>(rand() % 10) * 0.0099999998f;
            gi.height[2] = static_cast<float>(rand() % 40) * 0.0099999998f + 0.2f;
            gi.alphaB = 0;
            gi.sparkX = static_cast<float>(rand() % 9) + origin.x - 4.0f;
            gi.sparkZ = static_cast<float>(rand() % 9) + origin.z - 4.0f;
            gi.sparkY = origin.y;
            gi.rotStart = 0;
            gi.fullDisplayAngle = 0;
            gi.flag[0] = 0;
            gi.flag[1] = 0;
            gi.flag[2] = 10;
        }
    }
}

void LaunchWarpCasting(CRagEffect* effect, EFFECTPRIMID primType, const vector3d& origin, const char* textureName, float size, const float* distance, const short* spin, const short* rise)
{
    CEffectPrim* prim = effect->LaunchEffectPrim(primType, vector3d{});
    if (prim == nullptr || textureName == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = effect->GetDuration();
    prim->m_pos = origin;
    prim->m_size = size;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        ConfigureHealBand(gi, distance[band], spin[band], distance[band], rise[band], 0);
    }
}

void CRagEffect::ApplyPortalWind(int variant)
{
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeWind), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = variant == 1 ? 8.0f : 5.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\cloud11.tga", false));
    for (int band = 0; band < 4; ++band) {
        const float index = static_cast<float>(band);
        float maxHeight = index + index + 5.0f;
        float distance = static_cast<float>(rand() % 101) * 0.029999999f + 4.5f;
        if (variant == 1) {
            maxHeight = index + index + 3.0f;
            distance = static_cast<float>(rand() % 101) * 0.0099999998f + 2.5f;
        } else if (variant == 2) {
            maxHeight = 48.0f - index * 5.0f;
            distance = 14.0f - index * 2.0f;
        } else if (variant == 3) {
            maxHeight = 28.0f - index * 5.0f;
            distance = 6.0f - index;
        }
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        ConfigureHealBand(gi, maxHeight, static_cast<short>(90 * band), distance, static_cast<short>(rand() % 21 + 80), variant < 2 ? 1 : 2);
        gi.fullDisplayAngle = 30;
        gi.height.fill(1.0f);
    }
}

void CRagEffect::ApplyKyrieGuard()
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    if (m_stateCnt == 0) {
        vector3d wavePos = origin;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                wavePos.y = mode->m_world->m_player->m_pos.y;
            }
        }
        TryPlayEffectWaveAt(wavePos, "effect\\kyrie_guard.wav");
    }
    if (master != nullptr && m_stateCnt <= 29) {
        if ((master->m_BodyLight & 8) != 8) {
            master->m_BodyLight += 8;
        }
        if ((m_stateCnt % 6) >= 3) {
            master->SetArgb(-1, 250, 250, 250);
        } else {
            master->SetArgb(-1, 100, 100, 100);
        }
    }
    ProcessEZ2STR();
}

void CRagEffect::ApplyBodyLight442()
{
    if (CRenderObject* master = ResolveCurrentMaster()) {
        if ((master->m_BodyLight & 0x8000) != 0x8000) {
            master->m_BodyLight += 0x8000;
            master->m_BodyTime3 = static_cast<u8>(m_stateCnt);
        }
    }
}

void CRagEffect::ApplyBodyLight446()
{
    if (CRenderObject* master = ResolveCurrentMaster()) {
        if ((master->m_BodyLight & 0x20000) != 0x20000) {
            master->m_BodyLight += 0x20000;
            master->m_BodyTime2 = static_cast<u8>(m_stateCnt);
        }
        if ((master->m_BodyLight & 0x40) != 0x40 && m_stateCnt < 25) {
            master->m_BodyLight += 64;
            master->m_BodyTime = static_cast<u16>(static_cast<unsigned short>(m_stateCnt) + 11);
        }
    }
}

void CRagEffect::ApplyTarot(const char* textureName, int variant)
{
    if (textureName == nullptr) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeEffectTexture)) {
            existing->m_pos = origin;
        }
    }
    if (m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeEffectTexture), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    if (variant == 2) {
        gi.distance = 10.0f;
        gi.arcBaselineY = origin.y - 10.0f;
        gi.flag[2] = 2;
        gi.height[10] = 0.0f;
        gi.fullDisplayAngle = 135;
    } else if (variant == 3) {
        gi.distance = 8.0f;
        gi.arcBaselineY = origin.y - 30.0f;
        gi.flag[2] = 3;
        gi.height[10] = 2.0f;
        gi.fullDisplayAngle = 155;
    } else if (variant == 4) {
        gi.distance = 6.0f;
        gi.arcBaselineY = origin.y - 30.0f;
        gi.flag[2] = 0;
        gi.height[10] = 0.0f;
        gi.fullDisplayAngle = 135;
    } else {
        gi.distance = 8.0f;
        gi.arcBaselineY = origin.y - 30.0f;
        gi.flag[2] = 0;
        gi.height[10] = 1.0f;
        gi.fullDisplayAngle = 120;
    }
    gi.height[8] = 0.0f;
    gi.alphaB = 0;
    gi.sparkX = origin.x;
    gi.sparkZ = origin.z;
    gi.rotStart = static_cast<short>(rand() % 360);
    gi.flag[0] = 0;
    gi.flag[4] = static_cast<u8>(variant == 4 ? 1 : variant);
}

void CRagEffect::ApplyActorLight()
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    auto playAtListener = [&](const char* wave) {
        vector3d wavePos = origin;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                wavePos.y = mode->m_world->m_player->m_pos.y;
            }
        }
        TryPlayEffectWaveAt(wavePos, wave);
    };
    auto flashBody = [&](int red, int green, int blue) {
        if (master == nullptr) {
            return;
        }
        if ((master->m_BodyLight & 8) != 8) {
            master->m_BodyLight += 8;
        }
        if ((m_stateCnt % 2) != 0) {
            master->SetArgb(-1, 255, 255, 255);
            return;
        }
        if ((master->m_BodyLight & 1) != 1) {
            ++master->m_BodyLight;
        }
        master->SetArgb(-1, red, green, blue);
    };
    const unsigned short frame = static_cast<unsigned short>(m_stateCnt);
    switch (m_type) {
    case 414:
        if (master != nullptr && (master->m_BodyLight & 0x40) != 0x40 && m_stateCnt < 18) {
            master->m_BodyLight += 64;
            master->m_BodyTime = static_cast<u16>(2 * frame);
        }
        break;
    case 415:
        if (m_stateCnt == 17) {
            playAtListener("effect\\EF_hit2.wav");
        }
        if (m_stateCnt == 25) {
            playAtListener("effect\\EF_hit4.wav");
        }
        if (master != nullptr && (master->m_BodyLight & 0x80) != 0x80 && m_stateCnt >= 30) {
            master->m_BodyLight += 128;
            master->m_BodyTime = static_cast<u16>(frame - 30);
        }
        break;
    case 417:
        if (master == nullptr || (master->m_BodyLight & 0x100) == 0x100) {
            break;
        }
        master->m_BodyLight += 256;
        if (m_stateCnt < 15) {
            master->m_BodyTime = frame;
        }
        break;
    case 419:
        if (master != nullptr && (master->m_BodyLight & 0x200) != 0x200) {
            master->m_BodyLight += 512;
        }
        break;
    case 420:
        if (master != nullptr && (master->m_BodyLight & 0x10) != 0x10) {
            master->m_BodyLight += 16;
            if (m_stateCnt <= 45) {
                master->m_BodySin3 = static_cast<u8>(m_stateCnt);
            }
        }
        break;
    case 421:
        if (master != nullptr && (master->m_BodyLight & 0x10) != 0x10) {
            master->m_BodyLight += 16;
            master->m_BodySin3 = 45;
        }
        break;
    case 422:
        if (master != nullptr && (master->m_BodyLight & 0x400) != 0x400) {
            master->m_BodyLight += 1024;
            if (m_stateCnt <= 45) {
                master->m_BodySin4 = static_cast<u8>(m_stateCnt);
            }
        }
        break;
    case 423:
        if (master != nullptr && (master->m_BodyLight & 0x400) != 0x400) {
            master->m_BodyLight += 1024;
            master->m_BodySin4 = 45;
        }
        break;
    case 424:
        if (master != nullptr && (master->m_BodyLight & 0x800) != 0x800) {
            master->m_BodyLight += 2048;
            master->m_BodySin5 = m_stateCnt >= 200 ? static_cast<u8>(-56) : static_cast<u8>(m_stateCnt);
        }
        break;
    case 425:
        if (master != nullptr && m_stateCnt >= 30) {
            master->m_BodyLight |= 0x1000;
            master->m_BodyTime = static_cast<u16>(frame - 30);
        }
        break;
    case 426:
    case 458:
        if (master == nullptr || (master->m_BodyLight & 0x2000) == 0x2000) {
            break;
        }
        master->m_BodyLight += 0x2000;
        master->m_BodyTime = frame;
        break;
    case 427:
        if (master != nullptr && (master->m_BodyLight & 0x800) != 0x800) {
            master->m_BodyLight += 2048;
            if (m_stateCnt < 150) {
                master->m_BodyTime = static_cast<u16>(frame + 50);
            }
        }
        break;
    case 538:
        if (master == nullptr || m_stateCnt >= 31) {
            break;
        }
        if ((master->m_BodyLight & 8) != 8) {
            master->m_BodyLight += 8;
        }
        if ((m_stateCnt % 2) != 0) {
            master->SetArgb(-1, 255, 255, 255);
            break;
        }
        if ((master->m_BodyLight & 1) != 1) {
            ++master->m_BodyLight;
        }
        master->SetArgb(-1, 0, 255, 0);
        break;
    case 540:
        if (master == nullptr || m_stateCnt > 15 || (master->m_BodyLight & 0x10) == 0x10) {
            break;
        }
        master->m_BodyLight += 16;
        master->m_BodySin3 = static_cast<u8>(45 - 3 * (m_stateCnt & 0xFF));
        break;
    case 542:
        if (master == nullptr || (master->m_BodyLight & 0x4000) == 0x4000 || m_stateCnt > 150) {
            break;
        }
        master->m_BodyLight += 0x4000;
        master->m_BodyTime2 = static_cast<u8>(m_stateCnt / 3);
        break;
    case 544:
        if (master == nullptr || (master->m_BodyLight & 0x40000) == 0x40000) {
            break;
        }
        master->m_BodyLight += 0x40000;
        master->m_BodyTime2 = m_stateCnt > 200
            ? static_cast<u8>(25)
            : static_cast<u8>(static_cast<double>(m_stateCnt) * 0.125);
        break;
    case 375:
        if (master != nullptr && (master->m_BodyLight & 8) != 8) {
            master->m_BodyLight += 8;
        }
        break;
    case 379:
        if (master != nullptr && m_stateCnt <= 200) {
            master->SetArgb(-1, 205 - m_stateCnt, 205 - m_stateCnt, 255);
        }
        break;
    case 382:
        if (master != nullptr && (master->m_BodyLight & 4) != 4) {
            master->m_BodyLight += 4;
        }
        break;
    case 385:
        if (master != nullptr && m_stateCnt >= 40 && m_stateCnt <= 70) {
            if ((master->m_BodyLight & 1) != 1) {
                ++master->m_BodyLight;
            }
            master->SetArgb(-1, 200, 150, 50);
        }
        break;
    case 387:
        if (m_stateCnt == 30) {
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                mode->SendMsg(135, 0, 0, 0);
            }
        }
        if (master != nullptr && m_stateCnt >= 30 && m_stateCnt <= 54) {
            master->m_roty += 30.0f;
            if (master->m_roty > 360.0f) {
                master->m_roty -= 360.0f;
            }
        }
        if (master != nullptr && m_stateCnt >= 30 && m_stateCnt <= 50 && (m_stateCnt % 10) < 5
            && (master->m_BodyLight & 1) != 1) {
            ++master->m_BodyLight;
        }
        break;
    case 396:
        if (master != nullptr) {
            if ((master->m_BodyLight & 1) != 1) {
                ++master->m_BodyLight;
            }
            if ((master->m_BodyLight & 8) != 8) {
                master->m_BodyLight += 8;
            }
            master->SetArgb(-1, 255, 89, 182);
        }
        break;
    case 500:
        if (m_stateCnt < 31) {
            flashBody(0, 0, 255);
        }
        break;
    case 521:
        if (m_stateCnt >= 10 && m_stateCnt < 51) {
            flashBody(255, 0, 0);
        }
        break;
    case 548:
        if (master != nullptr && m_stateCnt <= 16) {
            master->m_BodyLight |= 0x40000;
            master->m_BodyTime2 = static_cast<u8>(3 * (m_stateCnt & 0xFF));
        }
        break;
    case 549:
        if (master != nullptr && m_stateCnt <= 16) {
            master->m_BodyLight |= 0x4000;
            master->m_BodyTime2 = static_cast<u8>(3 * (m_stateCnt & 0xFF));
        }
        break;
    case 550:
        if (master != nullptr && m_stateCnt > 30 && m_stateCnt <= 50) {
            master->m_BodyLight |= 0x2000;
            master->m_BodyTime = frame;
        }
        break;
    case 625:
        if (master != nullptr && (m_stateCnt % 4) == 0 && (master->m_BodyLight & 0x4000) != 0x4000) {
            master->m_BodyLight += 0x4000;
            master->m_BodyTime2 = 20;
        }
        break;
    case 626:
        if (master != nullptr && (m_stateCnt % 4) == 0 && (master->m_BodyLight & 0x40000) != 0x40000) {
            master->m_BodyLight += 0x40000;
            master->m_BodyTime2 = 20;
        }
        break;
    case 655:
        if (master != nullptr && (master->m_BodyLight & 0x80000) != 0x80000) {
            master->m_BodyLight += 0x80000;
            master->m_BodySin5 = m_stateCnt >= 200 ? static_cast<u8>(-56) : static_cast<u8>(m_stateCnt);
        }
        break;
    case 656:
        if (master != nullptr && m_stateCnt < 40 && (master->m_BodyLight & 0x80000) != 0x80000) {
            master->m_BodyLight += 0x80000;
            master->m_BodySin5 = static_cast<u8>(-56 - 5 * (m_stateCnt & 0xFF));
        }
        break;
    case 667:
        if (master != nullptr && (master->m_BodyLight & 2) != 2) {
            master->m_BodyLight += 2;
            master->m_BodySin5 = m_stateCnt >= 25 ? static_cast<u8>(-56) : static_cast<u8>(8 * m_stateCnt);
        }
        break;
    default:
        break;
    }
}

void CRagEffect::ApplyBodyLight449()
{
    if (CRenderObject* master = ResolveCurrentMaster()) {
        if ((master->m_BodyLight & 0x4000) != 0x4000) {
            master->m_BodyLight += 0x4000;
            if (m_stateCnt <= 50) {
                master->m_BodyTime2 = static_cast<u8>(m_stateCnt);
            }
        }
    }
}

void CRagEffect::ApplyPortalWind402()
{
    ApplyPortalWind(1);
    if (m_stateCnt < 5 || m_stateCnt > 65) {
        return;
    }
    if (CRenderObject* master = ResolveCurrentMaster()) {
        if ((master->m_BodyLight & 1) != 1) {
            ++master->m_BodyLight;
        }
        master->SetArgb(-1, 250, 250, 200);
    }
}

void CRagEffect::ApplyBlackDevil(const char* textureName)
{
    if (textureName == nullptr || (m_stateCnt != 1 && m_stateCnt != 2)) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    if (m_stateCnt == 1) {
        const float distance[4] = { 2.5f, 5.0f, 7.5f, 10.0f };
        const short spin[4] = { 270, 0, 90, 180 };
        const short rise[4] = { 53, 60, 55, 50 };
        LaunchWarpCasting(this, static_cast<EFFECTPRIMID>(kPrimTypeCasting5), origin, textureName, 12.0f, distance, spin, rise);
        return;
    }
    const float distance[4] = { 2.7f, 5.1999998f, 7.6999998f, 10.2f };
    const short spin[4] = { 271, 1, 91, 181 };
    const short rise[4] = { 52, 59, 54, 49 };
    LaunchWarpCasting(this, static_cast<EFFECTPRIMID>(kPrimTypeCasting5), origin, textureName, 12.0f, distance, spin, rise);
}

// Ref/RagEffect2.cpp:33659-33923 (0x635F90), effect 321. Preserve the
// source's conditional parent copies, five orbit RNG draws and integer division.
void CRagEffect::ApplyWarpZone2(const char* textureName)
{
    if (m_stateCnt == 99999998) m_stateCnt = 0;
    if (m_stateCnt == 1 || m_stateCnt == 2) {
        CRenderObject* master = ResolveCurrentMaster();
        if (!master) return;
        m_effectPos = master->m_pos;
        if (m_stateCnt == 1) {
            const float distance[4] = {2.5f,5.0f,7.5f,10.0f};
            const short spin[4] = {270,0,90,180}, rise[4] = {53,60,55,50};
            LaunchWarpCasting(this, PP_3DCASTING_4, m_effectPos, textureName, 11.0f, distance, spin, rise);
        } else {
            const float distance[4] = {2.7f,5.1999998f,7.6999998f,10.2f};
            const short spin[4] = {271,1,91,181}, rise[4] = {52,59,54,49};
            LaunchWarpCasting(this, PP_3DCASTING_4, m_effectPos, textureName, 4.0f, distance, spin, rise);
        }
    }
    if (m_stateCnt == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
        prim->m_duration = 99999999;
        prim->m_pattern |= 1;
        prim->m_radius = 15.0f; prim->m_deltaPos2.y -= 1.5f;
        prim->m_latitude = 90.0f;
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 128.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.1);
        prim->m_fadeOutCnt = prim->m_duration - 10;
        prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga", false)); // 0x719498
    }
    if (m_stateCnt % 10 == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{});
        prim->m_radius = static_cast<float>(static_cast<double>(rand() % 200 / 100) * 0.75 + 4.5);
        prim->m_radiusSpeed = 0.00050000002f;
        prim->m_duration = 70;
        prim->m_longitude = static_cast<float>(rand() % 360);
        if ((rand() & 1) == 0) prim->m_longSpeed = static_cast<float>(2.5 - static_cast<double>(rand() % 11) * 0.1);
        else prim->m_longSpeed = static_cast<float>(static_cast<double>(rand() % 11) * 0.1 - 2.5);
        prim->m_gravSpeed = -0.30000001f;
        prim->m_gravAccel = static_cast<float>(prim->m_gravSpeed / static_cast<double>(prim->m_duration) * -0.66666669);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_deltaPos2 = {
            static_cast<float>(static_cast<double>(prim->m_radius) * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(static_cast<double>(prim->m_radius) * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(static_cast<double>(prim->m_radius) * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
        prim->m_size = static_cast<float>(static_cast<double>(rand() % 10 / 10) + 0.69999999);
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act"); // 0x71B200 / 0x71B1E8
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
    }
}

// Ref/RagEffect.cpp:9095-9186 (0x5FBA00), original effect 61.
// Stored origin, pre-emission reset and inactive alpha-key data are intentional.
void CRagEffect::ApplyWarpZone()
{
    if (m_stateCnt == 145) m_stateCnt = 0;
    if (m_stateCnt == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
        prim->m_duration = 158;
        prim->m_pattern |= 1;
        prim->m_radius = 15.0f;
        prim->m_deltaPos2.y -= 1.0f;
        prim->m_latitude = 90.0f;
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 128.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.1);
        prim->m_fadeOutCnt = prim->m_duration - 10;
        prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga", false)); // 0x719498
    }
    if (m_stateCnt % 28 == 0 && m_stateCnt < 117) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
        prim->m_duration = 80;
        prim->m_innerSize = 4.0f;
        prim->m_latitude = 90.0f;
        prim->m_deltaPos2.y -= 1.0f;
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        prim->m_radius = 14.0f;
        prim->m_radiusSpeed = -0.15000001f;
        prim->m_alpha = 5.0f; prim->m_maxAlpha = 200.0f; prim->m_alphaSpeed = 10.0f;
        prim->m_chPoint.push_back(20.0f);
        prim->m_chVal1.push_back(0.0f); // m_chSpeed remains constructor zero.
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5;
        prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false)); // 0x71943C
    }
    if (m_stateCnt % 10 == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{});
        prim->m_radius = static_cast<float>(static_cast<double>(rand() % 200 / 100) * 0.75 + 5.0);
        prim->m_radiusSpeed = 0.00050000002f;
        prim->m_duration = 130;
        prim->m_longitude = static_cast<float>(rand() % 360);
        rand(); // Original discarded draw before fixed angular speed.
        prim->m_longSpeed = 2.0f;
        prim->m_gravSpeed = -0.2f;
        prim->m_gravAccel = static_cast<float>(prim->m_gravSpeed / static_cast<double>(prim->m_duration) * -0.66666669);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_deltaPos2 = {
            static_cast<float>(static_cast<double>(prim->m_radius) * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
            static_cast<float>(static_cast<double>(prim->m_radius) * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
            static_cast<float>(static_cast<double>(prim->m_radius) * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
        };
        prim->m_size = static_cast<float>(static_cast<double>(rand() % 10 / 10) + 0.69999999);
        prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr", "\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act"); // 0x71B200 / 0x71B1E8
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
        ShiftCircleSegments(prim);
    }
}

// Ref/RagEffect.cpp:7066-7098 (0x5FA100), effect 35. This is distinct
// from retained synthetic RSW ReadyPortal scheduling. Parent origin is stored.
void CRagEffect::ApplyReadyPortal()
{
    if (m_stateCnt % 14 != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_ReadyPortal.wav", m_effectPos.x - listener.x, 0.0f,
            m_effectPos.z - listener.z, 250, 40, 1.0f); // 0x71B760
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
    prim->m_duration = 30;
    prim->m_deltaPos2.y = -1.0f;
    prim->m_innerSize = 8.0f;
    prim->m_latitude = 90.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    prim->m_radiusSpeed = 1.0f;
    prim->m_radiusAccel = static_cast<float>(prim->m_radiusSpeed / static_cast<double>(prim->m_duration) * -0.5);
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
    // Original constructor alpha, radius/pattern and segment defaults.
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false)); // 0x71943C
}

// Ref/RagEffect.cpp:6981-7039 (0x5FA2A0), original effect 36.
// Distinct from retained synthetic RSW portal scheduling; no wave call.
void CRagEffect::ApplyPortal()
{
    if (m_stateCnt == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{});
        prim->m_duration = m_duration;
        prim->m_arcAngle = 36.0f;
        prim->m_outerSize = 3.5f;
        prim->m_innerSize = 3.5f;
        prim->m_longSpeed = 4.0f;
        prim->m_heightSize = 40.0f;
        prim->m_alpha = 0.0f;
        prim->m_maxAlpha = 128.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.16666667);
        prim->m_fadeOutCnt = prim->m_duration - 6;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false)); // 0x71943C
        prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{});
        prim->m_duration = m_duration;
        prim->m_arcAngle = 36.0f;
        prim->m_outerSize = 4.0f;
        prim->m_innerSize = 4.0f;
        prim->m_longSpeed = 5.0f;
        prim->m_heightSize = 50.0f;
        prim->m_alpha = 0.0f;
        prim->m_maxAlpha = 128.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.16666667);
        prim->m_fadeOutCnt = prim->m_duration - 6;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false)); // 0x71943C
    }
    if (m_stateCnt % 14 != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
    prim->m_duration = 30;
    prim->m_deltaPos2.y = -1.0f;
    prim->m_innerSize = 8.0f;
    prim->m_latitude = 90.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
    prim->m_radiusSpeed = 1.0f;
    prim->m_radiusAccel = static_cast<float>(prim->m_radiusSpeed / static_cast<double>(prim->m_duration) * -0.5);
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 2;
    // Original constructor alpha, radius/pattern and segment defaults.
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_numSegments = 1;
    ShiftCircleSegments(prim);
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false)); // 0x71943C
}

void CRagEffect::ApplyReadyPortal2()
{
    if ((m_stateCnt % 14) == 0) {
        CRenderObject* master = ResolveCurrentMaster();
        vector3d soundPos = master != nullptr ? master->m_pos : m_cachedPos;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                soundPos.y = mode->m_world->m_player->m_pos.y;
            }
        }
        TryPlayEffectWaveAt(soundPos, "effect\\EF_ReadyPortal.wav");
    }
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypePortal), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = 5.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false));
    ConfigureHealBand(prim->m_gi[0], 6.0009999f, 0, 0.0f, 2, 100);
    ConfigureHealBand(prim->m_gi[1], 6.0009999f, 25, 0.0f, 3, 100);
    ConfigureHealBand(prim->m_gi[2], 6.0009999f, 50, 0.0f, 4, 100);
    prim->m_gi[1].process = -10;
    prim->m_gi[2].process = -20;
    prim->m_gi[3].life = 0;
    prim->m_gi[3].process = 0;
    prim->m_gi[3].alphaT = 1;
    for (int band = 0; band < 3; ++band) {
        prim->m_gi[static_cast<size_t>(band)].flag.fill(1);
    }
}

void CRagEffect::ApplyPortal2(int variant)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    if (m_stateCnt == 0) {
        CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
        if (prim == nullptr) {
            return;
        }
        prim->m_renderFlag = 5;
        prim->m_duration = m_duration;
        prim->m_pos = origin;
        const bool red = variant == 3;
        prim->m_size = red ? 10.0f : 4.0f;
        prim->m_texture.push_back(g_texMgr.GetTexture(red ? "effect\\ring_red.tga" : "effect\\magic_violet.tga", false));
        ConfigureHealBand(prim->m_gi[0], 50.0f, static_cast<short>(rand() % 360), 4.0f, 90, 1400);
        ConfigureHealBand(prim->m_gi[1], 50.0f, static_cast<short>(rand() % 360), 3.0f, 90, 1400);
        ConfigureHealBand(prim->m_gi[2], 50.0f, static_cast<short>(rand() % 360), 2.0f, 90, 1400);
        prim->m_gi[3].life = 0;
        for (int band = 0; band < 3; ++band) {
            prim->m_gi[static_cast<size_t>(band)].flag.fill(1);
        }
        return;
    }
    if (m_stateCnt == 1) {
        CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypePortal), vector3d{});
        if (prim == nullptr) {
            return;
        }
        prim->m_renderFlag = 5;
        prim->m_duration = m_duration;
        prim->m_pos = origin;
        const bool red = variant == 3;
        prim->m_size = red ? 10.0f : 4.0f;
        prim->m_texture.push_back(g_texMgr.GetTexture(red ? "effect\\ring_red.tga" : "effect\\ring_blue.tga", false));
        ConfigureHealBand(prim->m_gi[0], 6.0009999f, 0, 0.0f, 2, 3111);
        ConfigureHealBand(prim->m_gi[1], 6.0009999f, 25, 0.0f, 3, 3111);
        ConfigureHealBand(prim->m_gi[2], 6.0009999f, 50, 0.0f, 4, 3111);
        prim->m_gi[1].process = red ? -20 : -10;
        prim->m_gi[2].process = red ? -40 : -20;
        prim->m_gi[3].life = 0;
        prim->m_gi[3].process = 0;
        prim->m_gi[3].alphaT = static_cast<short>(variant);
        for (int band = 0; band < 3; ++band) {
            prim->m_gi[static_cast<size_t>(band)].flag.fill(1);
        }
        return;
    }
    if (m_stateCnt != 2) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeWind), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = variant == 3 ? 10.0f : 4.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\cloud11.tga", false));
    const float heights[4] = { 1.5f, 3.5f, 5.5f, 7.5f };
    const short spins[4] = { 0, 90, 180, 270 };
    const float radii[4] = { 9.0f, 7.5f, 6.0f, 4.5f };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        ConfigureHealBand(gi, heights[band], spins[band], radii[band], 90, 0);
        gi.fullDisplayAngle = 30;
        gi.height.fill(1.5f);
    }
}

void CRagEffect::ApplyPoison(int variant)
{
    if (m_stateCnt == 99999998) {
        m_stateCnt = 0;
        m_duration = 99999999;
    }
    CRenderObject* master = ResolveCurrentMaster();
    vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (m_stateCnt == 0) {
        origin.y -= 9.0f;
        m_cachedPos.y -= 9.0f;
        CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypePoison), origin);
        if (prim == nullptr) {
            return;
        }
        prim->m_renderFlag |= 0x200u;
        prim->m_duration = m_duration;
        const char* textureName = variant != 0 ? "effect\\white02.bmp" : "effect\\poison_f.bmp";
        prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
        EffectGi& gi = prim->m_gi[0];
        gi.life = 1;
        gi.process = 0;
        gi.distance = 30.0f;
        gi.maxHeight = 0.0f;
        gi.alphaB = 0;
        gi.flag[10] = static_cast<u8>(variant);
        prim->m_pos = origin;
        return;
    }
    for (CEffectPrim* prim : m_primList) {
        if (prim != nullptr && prim->m_type == static_cast<EFFECTPRIMID>(kPrimTypePoison)) {
            prim->m_pos = origin;
        }
    }
}

void CRagEffect::ApplyBlind(int level)
{
    if (m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeBlind), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag |= 0x200u;
    prim->m_duration = m_duration;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\fullb.tga", false));
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\white02.bmp", false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.alphaB = 0;
    if (level != 0) {
        gi.distance = 140.0f - static_cast<float>(10 * level);
        gi.flag[0] = 1;
    } else {
        gi.distance = 50.0f;
        gi.flag[0] = 0;
    }
}

// Ref/RagEffect2.cpp:31333-31494 (0x6298B0).
void CRagEffect::ApplySaintCasting(short time, const char* textureName, int variant)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    if (m_duration < 56) {
        m_duration = 56;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeGi1), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    prim->m_size = 5.0f;
    switch (variant) {
    case 1:
        prim->m_size = 8.0f;
        break;
    case 2:
        prim->m_size = 5.0f;
        break;
    case 3:
        prim->m_size = 10.0f;
        break;
    case 4:
        prim->m_size = 11.0f;
        break;
    case 5:
        prim->m_size = 4.0f;
        break;
    case 6:
        prim->m_size = 12.0f;
        break;
    default:
        break;
    }
    const float bandHeight = variant == 2 ? 25.0f : variant == 22 ? 15.0f : 20.0f;
    const short spins[4] = { 180, 270, 0, 90 };
    const short alphaLift[4] = { 135, 90, 45, 0 };
    const bool stagger = m_duration <= 60;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = stagger ? -5 * band : 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = bandHeight - static_cast<float>(band);
        gi.rotStart = spins[band];
        gi.alphaB = stagger ? 0 : static_cast<short>(time + alphaLift[band]);
        gi.distance = 4.0999999f;
        gi.riseAngle = 80;
        gi.alphaT = static_cast<short>(variant);
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyBottomOut(const char* textureName, int variant)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeBottomOut)) {
            existing->m_pos = origin;
        }
    }
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeBottomOut), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const int bands = variant == 1 ? 1 : 2;
    for (int band = 0; band < bands; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = rand() % 11;
        gi.distance = variant == 1
            ? static_cast<float>(rand() % 20) * 0.1f + 3.0f
            : static_cast<float>(rand() % 6 + 5);
        gi.height[8] = 0.0f;
        gi.height[7] = 0.0f;
        gi.alphaB = 0;
        gi.sparkX = origin.x;
        gi.sparkZ = origin.z;
        gi.arcBaselineY = variant == 1 ? origin.y - 5.0f : origin.y;
        gi.fullDisplayAngle = 45;
        gi.flag[0] = 0;
        gi.flag[1] = static_cast<u8>(variant);
        gi.flag[2] = 2;
    }
}

void CRagEffect::ApplyBottomLight(const char* textureName, int size)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeGi5)) {
            existing->m_pos = origin;
        }
    }
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeGi5), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = static_cast<float>(size);
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.fullDisplayAngle = 315;
    gi.maxHeight = static_cast<float>(rand() % 10 + 25);
    gi.rotStart = static_cast<short>(rand() % 360);
    gi.alphaB = static_cast<short>(50 - rand() % 10);
    gi.riseAngle = 90;
    gi.alphaT = static_cast<short>(rand() % 360);
    gi.sparkY = 0.0f;
}

void CRagEffect::ApplyBottomMusic(const char* textureName, int variant)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeBottom)) {
            existing->m_pos = origin;
        }
    }
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeBottom), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.distance = 3.0f;
    if (variant == 1) {
        gi.distance = 5.0f;
    } else if (variant == 4) {
        gi.distance = 15.0f;
    } else if (variant == 6) {
        gi.distance = 8.0f;
    } else if (variant == 9) {
        gi.distance = 1.5f;
    }
    gi.height[8] = 0.0f;
    gi.height[7] = 0.0f;
    gi.alphaB = 200;
    gi.sparkX = origin.x;
    gi.sparkZ = origin.z;
    gi.arcBaselineY = origin.y - 6.0f;
    gi.rotStart = static_cast<short>(rand() % 360);
    gi.fullDisplayAngle = 45;
    gi.flag[0] = 0;
    gi.flag[2] = 2;
    if (variant == 4) {
        gi.height[9] = 10.0f;
    } else if (variant == 2) {
        for (int band = 1; band < 4; ++band) {
            prim->m_gi[static_cast<size_t>(band)].life = 1;
            prim->m_gi[static_cast<size_t>(band)].alphaB = 0;
        }
    } else if (variant == 3) {
        gi.flag[2] = 3;
    } else if (variant == 5 || variant == 6) {
        gi.flag[2] = 4;
    } else if (variant == 7) {
        gi.flag[2] = 7;
    } else if (variant == 9) {
        gi.flag[2] = 9;
        gi.flag[3] = 6;
    }
}

// Ref/RagEffect.cpp:1982-2108: BeginSpell2 through BeginSpell7.
void CRagEffect::SpawnBeginSpell()
{
    switch (m_type) {
    case 54: ApplyBeginCasting("effect\\ring_blue.tga",2,true); return;
    case 55: ApplyBeginCasting("effect\\ring_yellow.tga",1,true); return;
    case 56: ApplyBeginCasting("effect\\Magic_Green.tga",33,true); return;
    case 57: ApplyBeginCasting("effect\\ring_yellow.tga",0,true); return;
    case 58: {
        CRenderObject* master = ResolveCurrentMaster();
        if (!master) return;
        m_effectPos = master->m_pos;
        if (m_stateCnt != 0) return;
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_BeginSpell.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
        }
        ApplySaintCasting(45,"effect\\ring_white.tga",0);
        ApplySaintCasting(25,"effect\\ring_white.tga",0);
        return;
    }
    case 59: ApplyBeginCasting("effect\\Magic_Violet.tga",33,true); return;
    }
}

// Ref/RagEffect2.cpp:23440-23549 (0x607950), with caller zero-frame/audio gate.
void CRagEffect::ApplyBeginCasting(const char* textureName, int option, bool playWave)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    if (playWave) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_BeginSpell.wav",origin.x-listener.x,0.0f,origin.z-listener.z,250,40,1.0f);
        }
    }
    if (m_duration < 70) {
        m_duration = 70;
    }
    const bool third = option == 33;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(third ? kPrimTypeCasting3 : kPrimTypeCasting), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = third ? 0.0f : static_cast<float>(option);
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const float heights[4] = { 25.0f, 22.0f, 19.0f, 250.0f };
    const float distances[4] = { 4.5f, 5.0f, 5.5f, 4.0f };
    const short rises[4] = { 70, 57, 45, 89 };
    const short spins[4] = { 0, 90, 180, 0 };
    const short alphas[4] = { 180, 180, 180, 70 };
    const short spans[4] = { 315, 315, 315, 360 };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = spans[band];
        gi.maxHeight = heights[band];
        gi.rotStart = spins[band];
        gi.alphaB = alphas[band];
        gi.distance = distances[band];
        gi.riseAngle = rises[band];
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyPairedSaintCasting(const char* textureName, int variant, bool playWave, float waveYOffset)
{
    if (playWave && m_stateCnt == 0) {
        CRenderObject* master = ResolveCurrentMaster();
        vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                wavePos.y = mode->m_world->m_player->m_pos.y + waveYOffset;
            } else {
                wavePos.y += waveYOffset;
            }
        } else {
            wavePos.y += waveYOffset;
        }
        TryPlayEffectWaveAt(wavePos, "effect\\EF_BeginSpell.wav");
    }
    ApplySaintCasting(45, textureName, variant);
    ApplySaintCasting(25, textureName, variant);
}

void CRagEffect::ApplyCoupleCasting(const char* textureName)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (m_stateCnt == 0) {
        vector3d wavePos = origin;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                wavePos.y = mode->m_world->m_player->m_pos.y;
            }
        }
        TryPlayEffectWaveAt(wavePos, "effect\\EF_BeginSpell.wav");
    }
    ApplySaintCasting(45, textureName, 3);
    ApplySaintCasting(25, textureName, 3);
}

void CRagEffect::ApplyHeartCasting(const char* textureName, float offsetX, float offsetZ)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHealLow), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 10.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const short fadeAt = static_cast<short>(static_cast<unsigned short>(m_duration) - 25);
    for (int band = 0; band < 2; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = band == 0 ? 12.0f : 25.0f;
        gi.rotStart = static_cast<short>(rand() % 360);
        gi.alphaB = 0;
        gi.distance = band == 0 ? 2.3f : 2.0f;
        gi.riseAngle = static_cast<short>(band == 0 ? 86 : 90);
        gi.alphaT = fadeAt;
        gi.sparkX = offsetX;
        gi.sparkZ = offsetZ;
        gi.sparkY = 1234.5f;
        gi.height.fill(0.0f);
        gi.flag.fill(33);
    }
    prim->m_gi[2].life = 0;
    prim->m_gi[3].life = 0;
}

void CRagEffect::ApplyGiExplosion(const char* textureName)
{
    if (textureName == nullptr) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeGiExplosion), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 4.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 315;
        gi.maxHeight = 15.0f;
        gi.rotStart = static_cast<short>(rand() % 360);
        gi.alphaB = 0;
        gi.distance = 20.0f - static_cast<float>(band) * 0.5f;
        gi.riseAngle = 50;
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyAcidDemon(const char* textureName)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    vector3d wavePos = origin;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
            wavePos.y = mode->m_world->m_player->m_pos.y;
        }
    }
    TryPlayEffectWaveAt(wavePos, "effect\\EF_FireWall.wav");
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeAcidDemon), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 13.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const short fullAngles[4] = { 315, 315, 315, 360 };
    const float heights[4] = { 25.0f, 22.0f, 19.0f, 30.0f };
    const short spins[4] = { 0, 90, 180, 0 };
    const float distances[4] = { 2.4000001f, 2.7f, 3.0f, 2.2f };
    const short rises[4] = { 70, 57, 45, 90 };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = fullAngles[band];
        gi.maxHeight = heights[band];
        gi.rotStart = spins[band];
        gi.alphaB = 0;
        gi.distance = distances[band];
        gi.riseAngle = rises[band];
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyFlowerCasting(const char* textureName, float size, int variant)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeFlowerCast), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = size;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const float distances[2][4] = {
        { 4.5f, 5.0f, 5.5f, 5.0f },
        { 5.5f, 6.0f, 6.5f, 6.0f },
    };
    const short rises[2][4] = {
        { 85, 80, 70, 75 },
        { 50, 55, 65, 60 },
    };
    const float heights[2][4] = {
        { 27.0f, 28.0f, 29.0f, 30.0f },
        { 30.0f, 29.0f, 28.0f, 27.0f },
    };
    const short spins[2][4] = {
        { 0, 90, 180, 270 },
        { 90, 180, 270, 0 },
    };
    const int kind = variant == 1 ? 1 : 0;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 315;
        gi.alphaB = 0;
        gi.sparkY = 0.0f;
        gi.distance = distances[kind][band];
        gi.riseAngle = rises[kind][band];
        gi.maxHeight = heights[kind][band];
        gi.rotStart = spins[kind][band];
        gi.vecTNowX = static_cast<float>(gi.riseAngle);
        gi.vecTNowY = gi.maxHeight;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyDefender(const char* textureName)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeCasting2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const float heights[4] = { 40.0f, 39.0f, 38.0f, 37.0f };
    const short spins[4] = { 180, 270, 0, 90 };
    const float distances[4] = { 8.0f, 7.9000001f, 7.8000002f, 7.6999998f };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = heights[band];
        gi.rotStart = spins[band];
        gi.alphaB = 0;
        gi.distance = distances[band];
        gi.riseAngle = 90;
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyMapPillar(const char* textureName, float size, bool wide)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeMapPillar), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = size;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = band * 30;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = 120.0f;
        gi.rotStart = static_cast<short>(90 * band);
        gi.alphaB = static_cast<short>(wide ? 70 : 50);
        gi.distance = static_cast<float>(band) * 0.5f + (wide ? 11.0f : 2.0f);
        gi.alphaT = 0;
        gi.riseAngle = 89;
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyProvidence(const char* textureName)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeMapPillar), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 5.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 200;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = 120.0f;
        gi.rotStart = static_cast<short>(90 * band);
        gi.alphaB = 40;
        gi.distance = static_cast<float>(band) * 0.5f + 6.0f;
        gi.riseAngle = static_cast<short>(83 - band);
        gi.alphaT = 1;
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyVolcano(const char* textureName, short variant, bool fastSpin)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeGi2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_accel = fastSpin ? 99.0f : 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const bool compact = variant == 2;
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = compact ? 15.0f : 25.0f;
        gi.riseAngle = compact ? 70 : 80;
        gi.rotStart = static_cast<short>(90 * band);
        gi.alphaB = 0;
        gi.distance = static_cast<float>(band) + 1.0f;
        gi.alphaT = variant;
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
}

void CRagEffect::ApplyBottomVolcano(const char* textureName, float size)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeGi4), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = size;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.fullDisplayAngle = 360;
    gi.maxHeight = 15.0f;
    gi.rotStart = static_cast<short>(rand() % 360);
    gi.alphaB = static_cast<short>(rand() % 100);
    gi.distance = 2.5999999f;
    gi.riseAngle = 80;
    gi.sparkY = 0.0f;
    gi.height.fill(0.0f);
    gi.flag.fill(0);
}

void CRagEffect::ApplyWind()
{
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeWind), vector3d{});
    if (prim != nullptr) {
        prim->m_renderFlag = 5;
        prim->m_duration = m_duration;
        prim->m_size = 4.0f;
        prim->m_pos = origin;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\cloud11.tga", false));
        const short angles[4] = { 90, 70, 50, 30 };
        const short spins[4] = { 0, 10, 20, 30 };
        const float distances[4] = { 3.0f, 3.2f, 3.4000001f, 3.5999999f };
        const float heights[4] = { 3.0f, 2.5f, 2.0f, 1.5f };
        for (int band = 0; band < 4; ++band) {
            EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
            gi.life = 1;
            gi.process = 0;
            gi.fullDisplayAngle = angles[band];
            gi.maxHeight = 8.0f;
            gi.rotStart = spins[band];
            gi.alphaB = 0;
            gi.alphaT = 0;
            gi.distance = distances[band];
            gi.riseAngle = 90;
            gi.height.fill(heights[band]);
            gi.flag.fill(0);
        }
    }
    vector3d wavePos = origin;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
            wavePos.y = mode->m_world->m_player->m_pos.y;
        }
    }
    TryPlayEffectWaveAt(wavePos, "_heal_effect.wav");
}

void CRagEffect::ApplyStormKick(int variant)
{
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeStormKick), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\storm2.tga", false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.maxHeight = 40.0f;
    gi.alphaB = 0;
    gi.rotStart = static_cast<short>(rand() % 360);
    gi.distance = 2.0f;
    gi.height[0] = 50.0f;
    gi.height[1] = 5.0f;
    gi.flag[0] = 10;
    gi.flag[4] = static_cast<u8>(variant);
}

void CRagEffect::ApplyHalfSphere(const char* textureName)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHalfSphere), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 513;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.maxHeight = 8.0f;
    gi.alphaB = 0;
    gi.distance = 8.0f;
    gi.fullDisplayAngle = 180;
}

void CRagEffect::ApplyAttackEnergy(const char* textureName)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 513;
    prim->m_duration = m_duration;
    prim->m_size = 100.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const short rises[4] = { 0, 45, 90, 135 };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.alphaB = 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = 8.0f;
        gi.distance = 8.0f;
        gi.height[0] = 20.0f;
        gi.riseAngle = rises[band];
    }
}

void CRagEffect::ApplyElectric(short baseRise)
{
    if (m_stateCnt < 20 || m_stateCnt >= 24) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeElectric), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\elec1.tga", false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.maxHeight = 0.1f;
        gi.alphaB = 0;
        gi.alphaT = 0;
        gi.riseAngle = static_cast<short>(baseRise + 45 * band);
        gi.distance = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
        gi.vecTNowX = static_cast<float>(rand() % 11) * 0.0099999998f;
        gi.vecTNowY = 20.0f;
        gi.sparkX = origin.x;
        gi.sparkY = origin.y;
        gi.sparkZ = origin.z;
    }
}

void CRagEffect::ApplyElectric2()
{
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    const short rise = static_cast<short>(ElectricHeading(m_deltaPos.x - origin.x, m_deltaPos.z - origin.z));
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeElectric2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\elec1.tga", false));
    const float starts[4] = { 0.0f, 180.0f, 0.0f, 180.0f };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.maxHeight = 0.1f;
        gi.alphaB = 0;
        gi.alphaT = 0;
        gi.riseAngle = rise;
        gi.distance = 0.0f;
        gi.height.fill(0.0f);
        gi.flag.fill(0);
        gi.height[0] = starts[band];
        gi.vecTNowX = static_cast<float>(rand() % 6) * 0.0099999998f + 0.2f;
        gi.vecTNowY = 20.0f;
        gi.sparkX = origin.x;
        gi.sparkY = origin.y;
        gi.sparkZ = origin.z;
    }
}

void CRagEffect::ApplyAttackEnergy2(const char* textureName)
{
    if (textureName == nullptr || (m_stateCnt != 0 && m_stateCnt != 4)) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeAttackEnergy2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 4.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const float tilts[4] = { 0.0f, 22.0f, 44.0f, 66.0f };
    const short rises[4] = { 0, 45, 90, 135 };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.maxHeight = 8.0f;
        gi.alphaB = 0;
        gi.distance = 8.0f;
        gi.fullDisplayAngle = 360;
        gi.height[0] = 10.0f;
        gi.height[1] = tilts[band];
        gi.riseAngle = rises[band];
    }
}

// Ref/RagEffect.cpp:7041-7060 (0x604C30). This helper preserves the
// stored effect origin; caller-specific parent copies happen before the call.
void CRagEffect::ApplyPotionPillar(float speed, int duration)
{
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{});
    prim->m_duration = duration;
    prim->m_outerSize = 4.5f;
    prim->m_innerSize = 4.5f;
    prim->m_heightSize = 10.0f;
    prim->m_heightSpeed = speed;
    prim->m_heightAccel = 0.0099999998f;
    prim->m_alpha = 0.0f;
    prim->m_maxAlpha = 90.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.050000001);
    prim->m_fadeOutCnt = prim->m_duration - 10;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga", false)); // 0x719498
}

// HighPriest.exe.c:431781-431802 (0x607350/0x6073C0), ids 218/219.
// Ref repeats Potion_Con and omits 0x6073C0; follow the original binary.
bool CRagEffect::ProcessPotionConcentration()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return false;
    m_effectPos = master->m_pos;
    const bool awakening = m_type == 219;
    if (m_stateCnt == (awakening ? 30 : 0)) {
        PlayWave("effect\\ac_concentration.wav", 0.0f, 0.0f, 0.0f, 250, 40, 1.0f); // 0x712CC8
        ApplyPotionPillar(awakening ? 1.9f : 2.0f, awakening ? 30 : 50);
    }
    return ProcessEZ2STR(0.0f, -80.0f, 0.0f);
}

// Ref/RagEffect.cpp:1081-1143 (0x607430), original effect 220.
bool CRagEffect::ApplyPotionBerserk()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return false;
    if (m_stateCnt == 0) {
        if (master->m_oldColor.a == 0 && master->m_oldColor.r == 0 && master->m_oldColor.g == 0 && master->m_oldColor.b == 0) {
            master->m_oldColor.a = master->m_u28.m_sprColor.u.s.a;
            master->m_oldColor.r = master->m_u28.m_sprColor.u.s.r;
            master->m_oldColor.g = master->m_u28.m_sprColor.u.s.g;
            master->m_oldColor.b = master->m_u28.m_sprColor.u.s.b;
        }
        master->m_isSprArgbFixed = 1;
        master->m_curColor.a = 255;
        master->m_curColor.r = 250;
        master->m_curColor.g = 120;
        master->m_curColor.b = 120;
        master->SetArgb(255, 250, 120, 120);
        ApplyPotionPillar(1.0f, 50);
    }
    if (m_stateCnt == 18) {
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) mode->SendMsg(135, 1, 0, 0);
        PlayWave("effect\\ac_concentration.wav", 0.0f, 0.0f, 0.0f, 250, 40, 1.0f); // 0x712CC8
    }
    m_effectPos = master->m_pos; // Ref Potion_Berserk, after frame-zero pillar.
    if (m_stateCnt > 12 && m_stateCnt < 110 && (m_stateCnt & 3) == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCROSSTEXTURE, vector3d{});
        if (prim != nullptr) {
            prim->m_duration = 40;
            prim->m_animSpeed = 4; prim->m_repeatAnim = true;
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix, GetRadian(rand() % 360));
            const double radius = static_cast<double>(rand() % 7 + 2);
            prim->m_deltaPos2 = {
                static_cast<float>(radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0]),
                static_cast<float>(radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1]),
                static_cast<float>(radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2])
            };
            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix, GetRadian(90));
            prim->m_speed = static_cast<float>(static_cast<double>(rand() % 50 + 20) * 0.016666668);
            prim->m_widthSize = static_cast<float>(static_cast<double>(rand() % 60 + 30) * 0.1);
            prim->m_heightSize = 0.2f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 180.0f;
            prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.050000001);
            prim->m_fadeOutCnt = prim->m_duration - 20;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ac_center2.tga", false));
            prim->m_tintColor = RGB(240, 0, 0);
            prim->m_gi[0].life = 0;
        }
    }
    ProcessEZ2STR(0.0f, -80.0f, 0.0f); // Original deliberately ignores completion.
    if (m_stateCnt <= 150) return true;
    master->m_oldColor = {};
    master->m_isSprArgbFixed = 0; // Original actor message 129; current sprite color stays.
    return false;
}

// HighPriest.exe.c:315246-315257 (0x5552D0), four-coordinate CalcDir.
// Ref/GameActor.cpp labels the two-coordinate body with this overload's signature.
double EffectHeading(float fromX, float fromZ, float toX, float toZ)
{
    double angle = std::atan2(static_cast<double>(toX) - fromX,
        -(static_cast<double>(toZ) - fromZ)) / 3.141592 * 180.0;
    if (angle >= 360.0) angle -= 360.0;
    if (angle < 0.0) angle += 360.0;
    return angle;
}

// Ref/RagEffect.cpp:5892-6006 (0x5F85A0), original effect 26.
void CRagEffect::ApplyIceArrow()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 12) {
        char waveName[64];
        std::snprintf(waveName, sizeof(waveName), "effect\\EF_IceArrow%d.wav", rand() % 3 + 1);
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave(waveName, m_effectPos.x - listener.x, 0.0f, m_effectPos.z - listener.z, 250, 40, 1.0f);
        }
        m_count *= 10;
    }
    const int flight = m_stateCnt - 12;
    if (m_stateCnt >= 12 && flight % 10 == 0 && flight < m_count) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCROSSTEXTURE, vector3d{});
        prim->m_duration = 70;
        prim->m_pattern = 20;
        const int spawnZ = rand() % 10;
        const int spawnX = rand() % 10;
        prim->m_deltaPos2 = {static_cast<float>(spawnX + 25), -60.0f, static_cast<float>(spawnZ + 15)};
        const int aimZ = rand() % 4;
        const int aimX = rand() % 4 - 2;
        prim->m_longitude = static_cast<float>(-EffectHeading(prim->m_deltaPos2.x, prim->m_deltaPos2.z,
            static_cast<float>(aimX), static_cast<float>(aimZ - 2)));
        m_longitude = prim->m_longitude;
        const double x = std::fabs(static_cast<double>(prim->m_deltaPos2.x));
        const double z = std::fabs(static_cast<double>(prim->m_deltaPos2.z));
        const float squared = static_cast<float>(x * x + z * z);
        const float horizontal = static_cast<float>(std::sqrt(static_cast<double>(squared)));
        prim->m_latitude = static_cast<float>(EffectHeading(-std::fabs(prim->m_deltaPos2.y), horizontal, 0.0f, 0.0f));
        m_latitude = prim->m_latitude;
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_speed = -2.75f;
        prim->m_widthSize = 11.5f;
        prim->m_heightSize = 3.8f;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_fadeOutCnt = prim->m_duration - 20;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\icearrow.tga", false));
        prim->m_gi[0].life = 0;
    }
    if (m_flag != 0) {
        m_flag = 0;
        CEffectPrim* ring = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
        ring->m_duration = 30;
        ring->m_latitude = 90.0f;
        ring->m_radiusSpeed = 1.2f;
        ring->m_radiusAccel = static_cast<float>(ring->m_radiusSpeed / static_cast<double>(ring->m_duration + 40) * -2.0);
        ring->m_innerSize = 10.0f;
        ring->m_alpha = 0.0f; ring->m_maxAlpha = 254.0f;
        ring->m_alphaSpeed = static_cast<float>(ring->m_maxAlpha * 0.1);
        ring->m_fadeOutCnt = ring->m_duration - 10;
        ring->m_numSegments = 1;
        ShiftCircleSegments(ring);
        ring->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga", false));
    }
}

// Ref/RagEffect.cpp:3902-4050 (0x5F95B0), original effect 31.
void CRagEffect::ApplyFireArrow()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 12) {
        char waveName[64];
        std::snprintf(waveName, sizeof(waveName), "effect\\EF_FireArrow%d.wav", rand() % 3 + 1);
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave(waveName, m_effectPos.x - listener.x, 0.0f, m_effectPos.z - listener.z, 250, 40, 1.0f);
        }
        m_count *= 10;
    }
    const int flight = m_stateCnt - 12;
    if (m_stateCnt >= 12 && flight % 10 == 0 && flight < m_count) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCROSSTEXTURE, vector3d{});
        prim->m_duration = 70;
        prim->m_pattern = 20;
        const int spawnZ = rand() % 10;
        const int spawnX = rand() % 10;
        prim->m_deltaPos2 = {static_cast<float>(spawnX + 25), -60.0f, static_cast<float>(spawnZ + 15)};
        const int aimZ = rand() % 4;
        const int aimX = rand() % 4 - 2;
        prim->m_longitude = static_cast<float>(-EffectHeading(prim->m_deltaPos2.x, prim->m_deltaPos2.z,
            static_cast<float>(aimX), static_cast<float>(aimZ - 2)));
        m_longitude = prim->m_longitude;
        const double x = std::fabs(static_cast<double>(prim->m_deltaPos2.x));
        const double z = std::fabs(static_cast<double>(prim->m_deltaPos2.z));
        const float squared = static_cast<float>(x * x + z * z);
        const float horizontal = static_cast<float>(std::sqrt(static_cast<double>(squared)));
        prim->m_latitude = static_cast<float>(EffectHeading(-std::fabs(prim->m_deltaPos2.y), horizontal, 0.0f, 0.0f));
        m_latitude = prim->m_latitude;
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix, GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_speed = -2.75f;
        prim->m_widthSize = 14.0f;
        prim->m_heightSize = 3.5f;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 1; prim->m_totalTexture = 6; prim->m_repeatAnim = true;
        prim->m_fadeOutCnt = prim->m_duration - 20;
        for (int i = 1; i <= 6; ++i) {
            char textureName[64];
            // Original CP949 names at 0x71B700 down through 0x71B69C.
            std::snprintf(textureName,sizeof(textureName),"effect\\\xBA\xD2\xC8\xAD\xBB\xEC%d.tga",i);
            prim->m_texture.push_back(g_texMgr.GetTexture(textureName,false));
        }
        prim->m_gi[0].life = 0;
    }
    if (m_stateCnt > 0 && (m_stateCnt & 3) == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE,vector3d{});
        prim->m_duration = rand() % 24 + 6;
        prim->m_longitude = static_cast<float>(m_longitude - 20.0 + rand() % 40);
        prim->m_latitude = static_cast<float>(m_latitude - 10.0 + rand() % 20);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_deltaPos2 = m_deltaPos;
        prim->m_speed = static_cast<float>((rand() % 90 + 60) * 0.0099999998);
        prim->m_accel = static_cast<float>(prim->m_speed / static_cast<double>(prim->m_duration) * -0.66666669);
        prim->m_size = static_cast<float>((rand() % 30 + 20) * 0.0099999998);
        prim->m_sizeSpeed = static_cast<float>(-(prim->m_size / static_cast<double>(prim->m_duration)));
        prim->m_fadeOutCnt = 0;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_numSegments = 1; prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle4.spr","\xC0\xCC\xC6\xD1\xC6\xAE\\particle4.act");
        InitReferenceSegments(prim);
    }
    if (m_flag != 0) {
        m_flag = 0;
        CEffectPrim* ring = LaunchEffectPrim(PP_3DCIRCLE, vector3d{});
        ring->m_duration = 30;
        ring->m_latitude = 90.0f;
        ring->m_radiusSpeed = 1.2f;
        ring->m_radiusAccel = static_cast<float>(ring->m_radiusSpeed / static_cast<double>(ring->m_duration + 40) * -2.0);
        ring->m_innerSize = 10.0f;
        ring->m_alpha = 0.0f; ring->m_maxAlpha = 254.0f;
        ring->m_alphaSpeed = static_cast<float>(ring->m_maxAlpha * 0.1);
        ring->m_fadeOutCnt = ring->m_duration - 10;
        ring->m_numSegments = 1;
        ShiftCircleSegments(ring);
        ring->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_yellow.tga", false));
    }
}

// Ref/RagEffect.cpp:4431-4544 (0x5F8A10), original effect 27.
void CRagEffect::ApplyFrostDiver()
{
    if (g_session.m_isMinEffect) return;
    if (m_stateCnt == 0) {
        m_roty = static_cast<float>(std::atan2(static_cast<double>(m_deltaPos.x)-m_effectPos.x,
            -(static_cast<double>(m_deltaPos.z)-m_effectPos.z))/3.141592*180.0);
        if (m_roty >= 360.0f) m_roty -= 360.0f;
        if (m_roty < 0.0f) m_roty += 360.0f;
        matrix direction; MatrixIdentity(direction);
        MatrixAppendYRotation(direction,GetRadian(static_cast<int>(-m_roty)));
        m_deltaPos2 = {direction.m[2][0]*5.0f+direction.m[3][0],direction.m[2][1]*5.0f+direction.m[3][1],direction.m[2][2]*5.0f+direction.m[3][2]};
        m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x,m_deltaPos3.y-m_deltaPos2.y,m_deltaPos3.z-m_deltaPos2.z};
        m_deltaPos2 = {direction.m[2][0]+direction.m[2][0]+direction.m[3][0],direction.m[2][1]+direction.m[2][1]+direction.m[3][1],direction.m[2][2]+direction.m[2][2]+direction.m[3][2]};
    }
    if (m_flag != 0) return;
    const float worldX = m_effectPos.x + m_deltaPos3.x;
    const float worldZ = m_effectPos.z + m_deltaPos3.z;
    const double dx = static_cast<double>(m_deltaPos.x)-worldX, dz = static_cast<double>(m_deltaPos.z)-worldZ;
    const float squared = static_cast<float>(dz*dz+dx*dx);
    if (std::sqrt(static_cast<double>(squared)) <= 2.5) m_flag = 1;
    // Ref emits the final spike even when this call sets the arrival flag.
    CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN,vector3d{});
    prim->m_duration = 40; prim->m_pattern |= 1;
    const int angle = rand() % 360;
    const float length = static_cast<float>((rand() % 10 + 5)*0.1);
    prim->m_deltaPos2 = {GetSin(angle)*length,30.0f,static_cast<float>(-(static_cast<double>(GetCos(angle))*length))};
    prim->m_deltaPos2 = AddVec3(m_deltaPos3,prim->m_deltaPos2);
    prim->m_size = static_cast<float>((rand() % 40 + 60)*0.0099999998);
    prim->m_heightSize = static_cast<float>((rand() % 30 + 150)*0.1);
    prim->m_longitude = static_cast<float>(rand() % 360);
    prim->m_latitude = static_cast<float>(rand() % 30 + 75);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_rotMat = prim->m_matrix;
    prim->m_speed = 3.0f; prim->m_accel = static_cast<float>(prim->m_speed/static_cast<double>(prim->m_duration)*-2.0);
    prim->m_count = 20; prim->m_fadeOutCnt = prim->m_duration-10;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ice.tga",false));
    m_deltaPos3 = {m_deltaPos3.x-m_deltaPos2.x,m_deltaPos3.y-m_deltaPos2.y,m_deltaPos3.z-m_deltaPos2.z};
}

// Ref/RagEffect.cpp:4551-4606 (0x5F8E90), original effect 28.
void CRagEffect::ApplyFrostDiver2()
{
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_FrostDiver2.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    for (int i = 0; i < 8; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN,vector3d{});
        prim->m_duration = 40; prim->m_pattern |= 1;
        const int angle = rand() % 360;
        const float length = static_cast<float>((rand() % 4 + 1)*0.1);
        prim->m_deltaPos2 = {GetSin(angle)*length,30.0f,static_cast<float>(-(static_cast<double>(GetCos(angle))*length))};
        prim->m_size = static_cast<float>((rand() % 250 + 100)*0.0099999998);
        prim->m_heightSize = static_cast<float>((rand() % 100 + 200)*0.1);
        prim->m_longitude = static_cast<float>(rand() % 360);
        prim->m_latitude = static_cast<float>(rand() % 20 + 80);
        MatrixIdentity(prim->m_matrix);
        MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
        MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_rotMat = prim->m_matrix;
        prim->m_speed = 3.0f; prim->m_accel = static_cast<float>(prim->m_speed/static_cast<double>(prim->m_duration)*-2.0);
        prim->m_count = 20; prim->m_alpha = 200.0f; prim->m_maxAlpha = 254.0f;
        prim->m_fadeOutCnt = prim->m_duration-10;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ice.tga",false));
    }
}

// Ref/RagEffect.cpp:4790-4901 (0x5FF1C0), original effect 98.
void CRagEffect::ApplyHasteUp()
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (m_stateCnt == 0) m_duration = 300;
    if ((m_stateCnt == 0 || m_stateCnt == 30) && mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave(m_stateCnt == 0 ? "effect\\black_adrenalinerush_a.wav" : "effect\\black_adrenalinerush_b.wav",
            m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CRenderObject* master = ResolveCurrentMaster();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    for (int i = 0; i < 20; ++i) {
        CEffectPrim* prim = LaunchEffectPrim(PP_2DFLASH,vector3d{});
        prim->m_duration = 80; prim->m_deltaPos2.y = -40.0f;
        prim->m_longitude = static_cast<float>(rand() % 360);
        prim->m_longSpeed = static_cast<float>((rand() % 60 + 10)*0.1);
        prim->m_longAccel = static_cast<float>(prim->m_longSpeed/static_cast<double>(prim->m_duration)*-0.66666669);
        prim->m_arcAngle = static_cast<float>((rand() % 25 + 5)*0.1);
        prim->m_heightSize = 0.0f;
        prim->m_heightSpeed = static_cast<float>((rand() % 20 + 50)*0.07692308);
        prim->m_changePoint = 40;
        prim->m_changeSpeed = static_cast<float>((rand() % 10 + 15)*0.1);
        prim->m_changeAccel = 0.0f; prim->m_changeHeightSpeed = 0.0f;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
        prim->m_texture.push_back(g_texMgr.GetTexture("EFFECT\\alpha_center.tga",false));
    }
    for (int angle = 0; angle < 360; angle += 90) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT,vector3d{});
        prim->m_duration = 100; prim->m_radius = 7.0f;
        prim->m_gravSpeed = -0.1f; prim->m_gravAccel = -0.0020000001f;
        prim->m_longitude = static_cast<float>(angle); prim->m_longAccel = 0.2f;
        prim->m_longSpeed = static_cast<float>(static_cast<double>(prim->m_longAccel)*m_stateCnt+0.30000001);
        prim->m_size = 1.2f;
        MatrixIdentity(prim->m_matrix);
        MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
        prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/10;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_numSegments = 1; prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr","\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act");
        InitReferenceSegments(prim);
    }
}

// Ref/RagEffect.cpp:4908-5001 (0x5F4AD0), original effect 14.
void CRagEffect::ApplyHealSP()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    m_effectPos = master->m_pos;
    if (m_stateCnt == 0) {
        static const float radii[] = {2.5f,3.5f,4.5f};
        static const float heights[] = {15.0f,25.0f,35.0f};
        static const float maxima[] = {16.0f,32.0f,64.0f};
        for (int i = 0; i < 3; ++i) {
            CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER,vector3d{});
            if (i == 0) m_duration = 100;
            prim->m_duration = m_duration;
            prim->m_longSpeed = 5.0f;
            prim->m_outerSize = radii[i]; prim->m_innerSize = radii[i]; prim->m_heightSize = heights[i];
            prim->m_tintColor = RGB(32,176,232);
            prim->m_alpha = 0.0f; prim->m_maxAlpha = maxima[i];
            prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.033333335);
            prim->m_fadeOutCnt = prim->m_duration-20;
            prim->m_animSpeed = 4; prim->m_repeatAnim = true;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga",false));
        }
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("_heal_effect.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
        }
    }
    if (m_stateCnt % 2 != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT,vector3d{});
    rand();
    prim->m_radius = 3.0f; prim->m_duration = 30;
    prim->m_longitude = static_cast<float>(rand() % 360);
    prim->m_gravSpeed = -1.2f;
    prim->m_gravAccel = static_cast<float>(prim->m_gravSpeed/static_cast<double>(prim->m_duration)*-0.66666669);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_deltaPos2 = {
        static_cast<float>(static_cast<double>(prim->m_radius)*prim->m_matrix.m[2][0]+prim->m_matrix.m[3][0]),
        static_cast<float>(static_cast<double>(prim->m_radius)*prim->m_matrix.m[2][1]+prim->m_matrix.m[3][1]),
        static_cast<float>(static_cast<double>(prim->m_radius)*prim->m_matrix.m[2][2]+prim->m_matrix.m[3][2])
    };
    prim->m_size = 0.55000001f; prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/3;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_numSegments = 1; prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->SetParticleResources("\xC0\xCC\xC6\xD1\xC6\xAE\\particle2.spr","\xC0\xCC\xC6\xD1\xC6\xAE\\particle2.act");
    InitReferenceSegments(prim);
}

// Ref/RagEffect.cpp:5008-5080 (0x602770), original effect 142.
void CRagEffect::ApplyHeavensDrive()
{
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\wizard_earthspike.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    if (mode) mode->SendMsg(135,0,0,0);
    m_duration += 50;
    for (int z = -10; z < 15; z += 5) {
        for (int x = -10; x < 15; x += 5) {
            CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN,vector3d{});
            prim->m_duration = m_duration + rand() % 30 - 10;
            prim->m_pattern = 3;
            const int angle = rand() % 360;
            rand(); GetSin(angle); GetCos(angle);
            const int height = rand() % 60;
            prim->m_deltaPos2 = {static_cast<float>(x),10.0f,static_cast<float>(z)};
            prim->m_size = static_cast<float>((rand() % 50 + 300)*0.0099999998);
            prim->m_heightSize = static_cast<float>((height+1)*0.1+9.0);
            prim->m_longitude = static_cast<float>(rand() % 360);
            prim->m_latitude = static_cast<float>(rand() % 20 + 80);
            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
            MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
            prim->m_rotMat = prim->m_matrix;
            prim->m_changeSpeedY = -1.0f; prim->m_changeAccelY = -0.0099999998f;
            prim->m_speed = 1.0f; prim->m_accel = 0.0099999998f;
            prim->m_changePoint = 11; prim->m_changeSpeed = -1.2f; prim->m_changeAccel = 0.0f;
            prim->m_count = 14;
            prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\stone.bmp",false));
        }
    }
}

// Ref/RagEffect.cpp:6709-6769 (0x6021B0), original effect 128.
void CRagEffect::ApplyOverThrust()
{
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (m_stateCnt == 0 && mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\black_overthrust.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CRenderObject* master = ResolveCurrentMaster();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    m_duration = 9999;
    if (mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_StoneCurse.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_2DCIRCLE,vector3d{});
    prim->m_duration = 40; prim->m_deltaPos2.y = -20.0f;
    prim->m_arcAngle = 10.0f; prim->m_innerSize = 10.0f;
    prim->m_radiusSpeed = 7.0f;
    prim->m_radiusAccel = static_cast<float>(prim->m_radiusSpeed/static_cast<double>(prim->m_duration+40)*-2.0);
    prim->m_fadeOutCnt = 0; prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_center.tga",false));
}

// Ref/RagEffect.cpp:8608-8634 (0x5F9FA0), original effect 34.
void CRagEffect::ApplyTeleportation()
{
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_Teleportation.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER,vector3d{});
    prim->m_duration = 60; prim->m_pattern |= 1;
    prim->m_outerSize = 4.0f; prim->m_innerSize = 4.0f;
    prim->m_heightSpeed = 5.0f; prim->m_heightAccel = 0.25f;
    prim->m_longSpeed = 2.0f; prim->m_fadeOutCnt = 0;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_blue.tga",false));
}

// Ref/RagEffect.cpp:6329-6385 (0x5F5B80), original effect 17.
void CRagEffect::ApplyMagnumBreak()
{
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_MagnumBreak.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    const auto circle = [&](int duration) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE,vector3d{});
        prim->m_duration = duration; prim->m_renderFlag |= 0x200;
        prim->m_latitude = 90.0f; prim->m_radiusSpeed = 1.75f;
        prim->m_radiusAccel = static_cast<float>(prim->m_radiusSpeed/static_cast<double>(prim->m_duration)*-0.5);
        prim->m_innerSize = 12.0f; prim->m_alpha = 0.0f; prim->m_maxAlpha = 254.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.06666667);
        prim->m_fadeOutCnt = prim->m_duration-15; prim->m_numSegments = 1;
        InitReferenceSegments(prim);
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ring_yellow.tga",false));
    };
    circle(m_duration);
    CEffectPrim* prim = LaunchEffectPrim(PP_3DSPHERE,vector3d{});
    prim->m_duration = m_duration; prim->m_radiusSpeed = 1.15f;
    prim->m_radiusAccel = static_cast<float>(prim->m_radiusSpeed/static_cast<double>(prim->m_duration)*-0.5);
    prim->m_longSpeed = 3.0f; prim->m_alpha = 0.0f; prim->m_maxAlpha = 254.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.06666667);
    prim->m_fadeOutCnt = prim->m_duration-15;
    // Original CP949 texture bytes at 0x71A5B4.
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\\xB4\xEB\xC6\xF8\xB9\xDF.tga",false));
    circle(30);
}

// Ref/RagEffect.cpp:7824-7873 (0x600FA0), original effect 121.
void CRagEffect::ApplySonicBlow()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    if (mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_StoneCurse.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_2DCIRCLE,vector3d{});
    prim->m_duration = m_duration; prim->m_fadeOutCnt = 0;
    prim->m_deltaPos2.y = -20.0f; prim->m_arcAngle = 10.0f; prim->m_innerSize = 10.0f;
    prim->m_radiusSpeed = 7.0f;
    prim->m_radiusAccel = static_cast<float>(prim->m_radiusSpeed/static_cast<double>(prim->m_duration+40)*-2.0);
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_center.tga",false));
}

// Ref/RagEffect.cpp:7880-7940 (0x601170), original effect 122.
void CRagEffect::ApplySonicBlowHit()
{
    if (m_stateCnt != 0) return;
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (mode && mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\assasin_sonicblow.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) return;
    int facing = static_cast<int>(180.0-master->m_roty);
    const int x = rand() % 10, y = rand() % 20;
    rand();
    CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER,vector3d{});
    prim->m_duration = 12;
    prim->m_longitude = static_cast<float>(facing+(facing < 0 ? 360 : 0)); prim->m_latitude = -90.0f;
    MatrixIdentity(prim->m_matrix);
    MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
    MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
    prim->m_speed3d = {
        prim->m_matrix.m[3][0]-(prim->m_matrix.m[1][0]+prim->m_matrix.m[1][0]),
        prim->m_matrix.m[3][1]-(prim->m_matrix.m[1][1]+prim->m_matrix.m[1][1]),
        prim->m_matrix.m[3][2]-(prim->m_matrix.m[1][2]+prim->m_matrix.m[1][2])
    };
    prim->m_deltaPos = AddVec3(prim->m_speed3d,prim->m_deltaPos);
    prim->m_deltaPos2 = {static_cast<float>((x+1)/4),static_cast<float>((y+1)/2-11),0.0f};
    prim->m_speed = 0.40000001f;
    prim->m_accel = static_cast<float>(prim->m_speed/static_cast<double>(prim->m_duration)*-0.5);
    prim->m_outerSize = 7.0f; prim->m_innerSize = 4.0f; prim->m_heightSize = 3.5f;
    prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/2;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\magic_red.tga",false));
}

// Ref/RagEffect.cpp:1866-1952 (0x5F53E0), original effect 16.
void CRagEffect::ApplyBash()
{
    if (g_session.m_isMinEffect) return;
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt != 0) return;
    if (mode->m_world && mode->m_world->m_player) {
        const vector3d& listener = mode->m_world->m_player->m_pos;
        PlayWave("effect\\EF_Bash.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
    }
    CEffectPrim* prim = LaunchEffectPrim(PP_2DCIRCLE,vector3d{});
    prim->m_duration = 40; prim->m_pattern |= 1; prim->m_deltaPos2.y = -40.0f;
    prim->m_radius = 100.0f; prim->m_alpha = 0.0f; prim->m_maxAlpha = 170.0f;
    prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.16666667);
    prim->m_fadeOutCnt = prim->m_duration-10;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga",false));
    // Ref rechecks this flag here, after the outer full-suppression gate.
    const int count = g_session.m_isMinEffect ? 10 : 20;
    for (int i = 0; i < count; ++i) {
        prim = LaunchEffectPrim(PP_2DFLASH,vector3d{});
        prim->m_duration = 40; prim->m_deltaPos2.y = -40.0f;
        prim->m_longitude = static_cast<float>(rand() % 360);
        prim->m_longSpeed = static_cast<float>((rand() % 60 + 10)*0.1);
        prim->m_longAccel = static_cast<float>(prim->m_longSpeed/static_cast<double>(prim->m_duration)*-0.66666669);
        prim->m_arcAngle = static_cast<float>((rand() % 25 + 5)*0.1);
        prim->m_heightSize = static_cast<float>(rand() % 40 + 20);
        prim->m_heightSpeed = static_cast<float>((rand() % 30 + 20)*0.1);
        prim->m_alpha = 0.0f; prim->m_maxAlpha = 200.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.1);
        prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/3;
        prim->m_texture.push_back(g_texMgr.GetTexture("EFFECT\\alpha_center.tga",false));
    }
}

// Ref/RagEffect.cpp:1390-1477 (0x604F70), original effect 40.
bool CRagEffect::ProcessSignum()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return false;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt == 30) {
        if (mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_Signum.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
            PlayWave("effect\\EF_Bash.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
        }
        CEffectPrim* prim = LaunchEffectPrim(PP_2DCIRCLE,vector3d{});
        prim->m_duration = 40; prim->m_pattern |= 1; prim->m_deltaPos2.y = -40.0f;
        prim->m_radius = 100.0f; prim->m_alpha = 0.0f; prim->m_maxAlpha = 170.0f;
        prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.16666667);
        prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/3;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga",false));
        for (int i = 0; i < 20; ++i) {
            prim = LaunchEffectPrim(PP_2DFLASH,vector3d{});
            prim->m_duration = 35; prim->m_deltaPos2.y = -40.0f;
            prim->m_alpha = 0.0f;
            prim->m_longitude = static_cast<float>(rand() % 360);
            prim->m_longSpeed = static_cast<float>((rand() % 60 + 10)*0.1);
            prim->m_longAccel = static_cast<float>(prim->m_longSpeed/static_cast<double>(prim->m_duration)*-0.66666669);
            prim->m_arcAngle = static_cast<float>((rand() % 25 + 15)*0.1);
            prim->m_heightSize = static_cast<float>(rand() % 60 + 30);
            prim->m_heightSpeed = static_cast<float>((rand() % 35 + 95)*0.1);
            prim->m_maxAlpha = 200.0f; prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha*0.1);
            prim->m_fadeOutCnt = -1; // Original constructor; Signum never assigns a ray fade gate.
            prim->m_texture.push_back(g_texMgr.GetTexture("EFFECT\\alpha_center.tga",false));
        }
    }
    return ProcessEZ2STR(0.0f,-80.0f,0.0f) != 0;
}

// Ref/RagEffect.cpp:7-22 (0x604F00), inlined in original OnProcess case 41.
bool CRagEffect::ProcessAngelus()
{
    if (m_stateCnt == 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            PlayWave("effect\\EF_Angelus.wav",m_effectPos.x-listener.x,0.0f,m_effectPos.z-listener.z,250,40,1.0f);
        }
    }
    return ProcessEZ2STR(0.0f,-80.0f,0.0f) != 0;
}

// Ref/RagEffect.cpp:8214-8312 (0x5F6AE0), original effect 48.
void CRagEffect::SpawnSprayPond()
{
    if (m_stateCnt == 0) {
        for (int degrees = 0; degrees < 360; degrees += 45) {
            CEffectPrim* prim = LaunchEffectPrim(PP_3DTEXTURE,vector3d{});
            prim->m_pattern = 2; prim->m_duration = 130;
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(degrees+22.5)));
            prim->m_deltaPos2 = {
                static_cast<float>(prim->m_matrix.m[2][0]*12.2+prim->m_matrix.m[3][0]),
                static_cast<float>(prim->m_matrix.m[2][1]*12.2+prim->m_matrix.m[3][1]),
                static_cast<float>(prim->m_matrix.m[2][2]*12.2+prim->m_matrix.m[3][2])
            };
            prim->m_deltaPos2.y -= 2.0f;
            MatrixIdentity(prim->m_rotMat);
            MatrixAppendYRotation(prim->m_rotMat,GetRadian(static_cast<int>(degrees+90+22.5)));
            prim->m_alpha = 120.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = -1;
            prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
            prim->m_widthSize = 5.0f; prim->m_heightSize = 2.7f;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\pond_water.bmp",false));
        }
    }
    if (m_stateCnt % 10 == 0) {
        for (int degrees = 0; degrees < 360; degrees += 45) {
            CEffectPrim* prim = LaunchEffectPrim(PP_3DTEXTURE,vector3d{});
            prim->m_duration = 30;
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(degrees+22.5)));
            prim->m_deltaPos2 = {
                static_cast<float>(prim->m_matrix.m[2][0]*13.8+prim->m_matrix.m[3][0]),
                static_cast<float>(prim->m_matrix.m[2][1]*13.8+prim->m_matrix.m[3][1]),
                static_cast<float>(prim->m_matrix.m[2][2]*13.8+prim->m_matrix.m[3][2])
            };
            MatrixIdentity(prim->m_rotMat);
            MatrixAppendZRotation(prim->m_rotMat,GetRadian(90));
            MatrixAppendYRotation(prim->m_rotMat,GetRadian(static_cast<int>(degrees+90+22.5)));
            prim->m_alpha = 100.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = 0;
            prim->m_animSpeed = 4; prim->m_repeatAnim = true; prim->m_numSegments = 1;
            prim->m_widthSize = 5.5f; prim->m_heightSize = 1.0f;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga",false));
        }
    }
    if (m_stateCnt % 20 == 0) {
        CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE,vector3d{});
        prim->m_duration = 25; prim->m_radius = 13.0f; prim->m_arcAngle = 10.0f;
        prim->m_size = 1.25f; prim->m_latitude = 90.0f;
        MatrixIdentity(prim->m_matrix); MatrixAppendXRotation(prim->m_matrix,GetRadian(90));
        prim->m_radiusSpeed = 0.25f;
        prim->m_radiusAccel = static_cast<float>(-(prim->m_radiusSpeed/static_cast<double>(prim->m_duration)));
        prim->m_alpha = 100.0f; prim->m_maxAlpha = 254.0f; prim->m_fadeOutCnt = 0; prim->m_numSegments = 1;
        InitReferenceSegments(prim);
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_center.tga",false));
    }
}

// Ref/RagEffect.cpp:361-377 (0x606CB0), original effect 49.
bool CRagEffect::ProcessFireHit()
{
    if (m_stateCnt == 0) {
        CGameMode* mode = g_modeMgr.GetCurrentGameMode();
        if (mode && mode->m_world && mode->m_world->m_player) {
            const vector3d& listener = mode->m_world->m_player->m_pos;
            const float x = m_effectPos.x-listener.x, z = m_effectPos.z-listener.z;
            const char* wave = g_session.GetAttrWaveName(3);
            PlayWave(wave,x,0.0f,z,250,40,1.0f);
        }
    }
    return ProcessEZ2STR(0.0f,-80.0f,0.0f) != 0;
}

// Ref/RagEffect.cpp:4270-4328 (0x5FAF30), original effect 50.
void CRagEffect::SpawnFireSplashHit()
{
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    const int angle = rand() % 360;
    if (m_stateCnt != 0) return;
    CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE,vector3d{});
    prim->m_duration = 30; prim->m_deltaPos2.y = -20.0f;
    prim->m_widthSize = 10.0f; prim->m_heightSize = 10.0f;
    prim->m_widthSpeed = 5.6999998f; prim->m_heightSpeed = 5.6999998f;
    prim->m_roll = static_cast<float>(angle); prim->m_rollSpeed = -18.0f;
    prim->m_rollAccel = static_cast<float>(prim->m_rollSpeed/static_cast<double>(prim->m_duration)*-0.40000001);
    prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/2;
    prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
    prim->m_animSpeed = 4; prim->m_repeatAnim = true;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\FireRing.tga",false));
    prim->m_gi[0].life = 0;
}

// Ref/RagEffect.cpp:2816-2915 (0x5FB100), original effect 51.
void CRagEffect::SpawnColdHit()
{
    if (m_stateCnt == 0) {
        int index = 0;
        for (float angle = 0.0f; angle < 360.0f; angle += 40.0f, ++index) {
            CEffectPrim* prim = LaunchEffectPrim(PP_2DQUAD,vector3d{});
            prim->m_duration = 15;
            const int jitter = rand() % 60;
            prim->m_roll = static_cast<float>(static_cast<double>(angle)-15.0+jitter);
            prim->m_longitude = prim->m_roll;
            if (index % 3 != 0) {
                prim->m_heightSize = static_cast<float>(rand() % 5 + 45);
                prim->m_widthSize = 4.0f;
                prim->m_speed = static_cast<float>((rand() % 45 / 10)+4.8);
            } else {
                prim->m_heightSize = static_cast<float>(rand() % 10 + 100);
                prim->m_widthSize = 10.0f;
                prim->m_speed = static_cast<float>((rand() % 45 / 10)+12.8);
            }
            prim->m_accel = static_cast<float>(-(prim->m_speed/static_cast<double>(prim->m_duration))*0.6666666666666666);
            const float distance = static_cast<float>(rand() % 5);
            prim->m_deltaPos2.x = static_cast<float>(static_cast<double>(GetSin(static_cast<int>(angle)))*distance);
            prim->m_deltaPos2.y = static_cast<float>(-(static_cast<double>(GetCos(static_cast<int>(angle)))*distance));
            prim->m_deltaPos2.y -= 20.0f;
            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_latitude)));
            MatrixAppendYRotation(prim->m_matrix,GetRadian(static_cast<int>(prim->m_longitude)));
            prim->m_rotMat = prim->m_matrix;
            prim->m_count = 20; prim->m_alpha = 200.0f; prim->m_maxAlpha = 254.0f;
            prim->m_fadeOutCnt = prim->m_duration-10;
            prim->m_animSpeed = 4; prim->m_repeatAnim = true;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\ice.tga",false));
        }
    }
    CRenderObject* master = ResolveCurrentMaster();
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!master || !mode || !mode->m_view) return;
    tlvertex3d projected{};
    ProjectReferenceEffectVertex(master->m_pos,mode->m_view->GetViewMatrix(),projected);
    m_tlvertX = projected.x; m_tlvertY = projected.y;
    if (m_stateCnt == 0 || m_stateCnt == 7) {
        CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE,vector3d{});
        prim->m_duration = 25; prim->m_speed = 0.85000002f;
        prim->m_longitude = static_cast<float>(rand() % 40 - 20);
        prim->m_widthSize = 1.0f; prim->m_heightSize = 1.0f;
        prim->m_changeWidthSpeed = 0.5f; prim->m_changeHeightSpeed = 0.5f;
        prim->m_widthSpeed = 6.0f; prim->m_heightSpeed = 6.0f; prim->m_changePoint = 10;
        prim->m_fadeOutCnt = prim->m_duration-prim->m_duration/3;
        prim->m_alpha = 254.0f; prim->m_maxAlpha = 254.0f;
        prim->m_animSpeed = 4; prim->m_repeatAnim = true;
        prim->m_texture.push_back(g_texMgr.GetTexture("effect\\smoke.tga",false));
        prim->m_gi[0].life = 0;
    }
}

void CRagEffect::ApplyAuraBladeCasting()
{
    ApplySaintCasting(45, "effect\\ring_white.tga", 22);
    ApplySaintCasting(25, "effect\\ring_yellow.tga", 22);
}

void CRagEffect::ApplyColorPaper(const char* textureName)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeColorPaper), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.maxHeight = static_cast<float>(rand() % 31) * 0.0099999998f + 0.30000001f;
        gi.rotStart = static_cast<short>(rand() % 360);
        gi.riseAngle = static_cast<short>(45 * band);
        gi.alphaB = 0;
        gi.height[0] = static_cast<float>(rand() % 360);
        gi.height[1] = static_cast<float>(rand() % 360);
        gi.height[2] = static_cast<float>(rand() % 360);
        gi.sparkX = static_cast<float>(rand() % 101 - 50);
        gi.sparkY = static_cast<float>(-110 - rand() % 50);
        gi.sparkZ = static_cast<float>(rand() % 101 - 50);
        gi.distance = static_cast<float>(rand() % 26) * 0.0099999998f + 0.25f;
        gi.fullDisplayAngle = 0;
    }
}

void CRagEffect::ApplySphereWind(const char* textureName, short time, int variant)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeSphereWind)) {
            existing->m_pos = origin;
        }
    }
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeSphereWind), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = variant == 1 ? 10.0f : 4.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.maxHeight = static_cast<float>(rand() % 11) * 0.2f + 4.0f;
        gi.distance = 14.0f;
        gi.height[0] = -14.0f;
        gi.height[1] = static_cast<float>(rand() % 4 + 3);
        gi.rotStart = static_cast<short>(rand() % 360);
        gi.riseAngle = static_cast<short>(time + 45 * band);
        gi.alphaB = 100;
        gi.fullDisplayAngle = static_cast<short>(rand() % 91 + 180);
    }
}

void CRagEffect::ApplyMagnum2(const char* textureName)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeMagnum2)) {
            existing->m_pos = origin;
        }
    }
    if (m_stateCnt == 0) {
        m_flag = 0;
    }
    auto playAtListener = [&](const char* wave) {
        vector3d wavePos = origin;
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                wavePos.y = mode->m_world->m_player->m_pos.y;
            }
        }
        TryPlayEffectWaveAt(wavePos, wave);
    };
    if (m_stateCnt == 20) {
        playAtListener("permeter_attack.wav");
    } else if (m_stateCnt == 30) {
        playAtListener("effect\\EF_MagnumBreak.wav");
    }
    if (textureName == nullptr || m_stateCnt < 25 || m_stateCnt > 55 || (m_stateCnt % 3) != 0) {
        return;
    }
    ++m_flag;
    if (m_flag >= 11) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeMagnum2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 5.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const int age = m_stateCnt - 25;
    for (int band = 0; band < 2; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.alphaB = 0;
        gi.alphaT = static_cast<short>(band);
        gi.fullDisplayAngle = 360;
        gi.maxHeight = 20.0f - static_cast<float>(age) * 0.69999999f;
        gi.rotStart = static_cast<short>(6 * age + 9 * band);
        gi.distance = static_cast<float>(age) * 0.2f + 5.0f;
        gi.riseAngle = static_cast<short>(2 * (35 - 4 * band - age));
    }
}

void CRagEffect::ApplySaintWing(const char* textureName)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* existing : m_primList) {
        if (existing != nullptr && existing->m_type == static_cast<EFFECTPRIMID>(kPrimTypeSaintWing)) {
            existing->m_pos = origin;
        }
    }
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeSaintWing), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 4.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    prim->m_gi[2].life = 0;
    prim->m_gi[3].life = 0;
    for (int band = 0; band < 2; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.maxHeight = 10.0f;
        gi.distance = 2.0f;
        gi.height[0] = -8.0f;
        gi.rotStart = 0;
        gi.fullDisplayAngle = 0;
        gi.alphaB = 200;
        gi.riseAngle = 0;
    }
}

void CRagEffect::ApplyForestLight(const char* textureName, int variant)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeForest), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    m_deltaPos = origin;
    if (variant == 10) {
        m_deltaPos.x -= 15.0f;
        m_deltaPos.y -= 60.0f;
        m_deltaPos.z -= 15.0f;
    } else {
        m_deltaPos.x -= 70.0f;
        m_deltaPos.y -= 300.0f;
        m_deltaPos.z -= 70.0f;
    }
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = band == 3 ? 180 : 0;
        if (variant == 2) {
            gi.alphaB = 30;
        } else if (variant == 10) {
            gi.alphaB = 25;
        } else if (variant == 11) {
            gi.alphaB = 0;
        } else {
            gi.alphaB = 40;
        }
        gi.distance = 0.0f;
        if (band == 3) {
            gi.maxHeight = (variant == 1 || variant == 11) ? 4.0f : variant == 2 ? 1.0f : 2.0f;
        } else if (variant == 1 || variant == 11) {
            gi.maxHeight = static_cast<float>(2 * band) + 4.0f;
        } else if (variant == 2) {
            gi.maxHeight = static_cast<float>(band) * 0.5f + 1.0f;
        } else {
            gi.maxHeight = static_cast<float>(band) + 2.0f;
        }
        gi.height[0] = gi.maxHeight;
        gi.rotStart = static_cast<short>(25 * band);
        gi.vecTNowX = origin.x;
        gi.vecTNowY = origin.y;
        gi.vecTNowZ = origin.z;
        gi.vecTPreX = m_deltaPos.x;
        gi.vecTPreY = m_deltaPos.y;
        gi.vecTPreZ = m_deltaPos.z;
        gi.flag[0] = static_cast<u8>(variant);
    }
}

void CRagEffect::ApplyBottomMagnus(const char* textureName, int variant)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeRectUp), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = 1.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.fullDisplayAngle = static_cast<short>(rand() % 360);
    gi.rotStart = 0;
    gi.height[2] = 0.0f;
    gi.height[3] = static_cast<float>(variant);
    if (variant == 1) {
        gi.maxHeight = 16.0f;
        gi.alphaB = 120;
        gi.alphaT = 1880;
        gi.height[0] = 2.5f;
        gi.height[1] = 2.5f;
    } else if (variant == 2) {
        gi.maxHeight = 32.0f;
        gi.alphaB = 30;
        gi.alphaT = 29999;
        gi.height[0] = 2.5f;
        gi.height[1] = 2.5f;
    } else {
        gi.maxHeight = 50.0f;
        gi.alphaB = 30;
        gi.alphaT = 864;
        gi.height[0] = 5.0f;
        gi.height[1] = 5.0f;
    }
    prim->m_gi[1].life = 0;
    prim->m_gi[2].life = 0;
    prim->m_gi[3].life = 0;
}

void CRagEffect::ApplyThrowItem(const char* textureName, int variant)
{
    if (textureName == nullptr) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* prim : m_primList) {
        if (prim != nullptr && prim->m_type == static_cast<EFFECTPRIMID>(kPrimTypeThrowItem)) {
            prim->m_pos = origin;
        }
    }
    int launchFrame = 0;
    if (variant == 10) {
        launchFrame = 5;
    } else if (variant == 11) {
        launchFrame = 10;
    }
    if (m_stateCnt != launchFrame) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeThrowItem), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_deltaPos = m_deltaPos;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    EffectGi& gi = prim->m_gi[0];
    const float dx = m_deltaPos.x - origin.x;
    const float dz = m_deltaPos.z - origin.z;
    gi.life = 1;
    gi.process = 0;
    gi.maxHeight = std::sqrt(dx * dx + dz * dz);
    gi.distance = variant == 2 ? 2.0f : variant == 3 ? 4.0f : 3.0f;
    gi.height[8] = 0.0f;
    gi.alphaB = 255;
    gi.sparkX = origin.x;
    gi.sparkZ = origin.z;
    gi.sparkY = 0.0f;
    gi.arcBaselineY = origin.y - 8.0f;
    gi.rotStart = 0;
    gi.height[1] = variant == 2 ? 2.0f : variant == 3 ? 3.0f : gi.maxHeight * 0.039999999f;
    gi.height[2] = gi.maxHeight == 0.0f ? 0.0f : gi.height[1] / gi.maxHeight * (m_deltaPos.y - origin.y);
    gi.fullDisplayAngle = variant == 3 ? 330 : 0;
    gi.flag[0] = 0;
    gi.height[7] = 0.0f;
    if (variant == 0) {
        gi.flag[2] = 8;
    } else if (variant == 1) {
        gi.flag[2] = 9;
        gi.flag[3] = static_cast<u8>(m_count);
    } else if (variant < 10) {
        if (variant == 2) {
            gi.flag[2] = 0;
        } else if (variant == 3) {
            gi.flag[2] = 15;
        }
    } else {
        gi.flag[2] = 9;
        gi.flag[3] = 4;
    }
    int steps = 0;
    while (static_cast<float>(steps) * gi.height[1] <= gi.maxHeight) {
        ++steps;
        if (steps >= 100) {
            steps = 0;
            break;
        }
    }
    gi.flag[1] = static_cast<u8>(steps);
    prim->m_gi[1].life = 1;
    prim->m_gi[1].alphaB = 0;
    prim->m_gi[2].life = 1;
    prim->m_gi[2].alphaB = 0;
    prim->m_gi[3].life = 1;
    prim->m_gi[3].alphaB = 0;
}

void CRagEffect::ApplyTeleportation2(const char* textureName, short time)
{
    if (m_stateCnt != 0 || textureName == nullptr) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    vector3d wavePos = origin;
    if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
        if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
            wavePos.y = mode->m_world->m_player->m_pos.y;
        }
    }
    TryPlayEffectWaveAt(wavePos, "effect\\EF_Teleportation.wav");
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeTeleport2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = 4.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    const float timeScale = static_cast<float>(2 * time);
    const float heightBase = static_cast<float>(60 * time);
    const float heights[4] = { 100.0f, 70.0f, 40.0f, 15.0f };
    const short spins[4] = { 180, 270, 0, 90 };
    const short alphas[4] = { 100, 80, 60, 40 };
    const float distances[4] = { 1.5f, 3.0f, 4.0f, 5.0f };
    const short rises[4] = { 90, 89, 88, 87 };
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 360;
        gi.maxHeight = heightBase + heights[band];
        gi.rotStart = spins[band];
        gi.alphaB = alphas[band];
        gi.distance = distances[band] - timeScale;
        gi.riseAngle = rises[band];
        gi.sparkY = 0.0f;
        gi.height.fill(0.0f);
        gi.flag[0] = 0;
        gi.flag[1] = static_cast<u8>(static_cast<int>(gi.maxHeight));
    }
}

short Chemical2RiseAngle(float dx, float dz)
{
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length == 0.0f) {
        return 0;
    }
    const float nx = dx / length;
    const float nz = dz / length;
    short angle = 0;
    while (nx < g_cosTable[angle]) {
        ++angle;
        if (angle > 180) {
            return 180;
        }
    }
    short rise = nz < 0.0f ? static_cast<short>(360 - angle) : angle;
    if (rise >= 360) {
        rise = static_cast<short>(rise - 360);
    }
    return rise;
}

void CRagEffect::ApplyChemical2(short time)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* prim : m_primList) {
        if (prim != nullptr && prim->m_type == static_cast<EFFECTPRIMID>(kPrimTypeChemical2)) {
            prim->m_pos = origin;
        }
    }
    if (m_stateCnt != 40) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeChemical2), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag |= 0x200u;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\slash01.tga", false));
    m_effectPos.y -= 9.0f;
    m_cachedPos.y -= 9.0f;
    const short rise = Chemical2RiseAngle(m_deltaPos.x - origin.x, m_deltaPos.z - origin.z);
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.distance = 100.0f;
        gi.maxHeight = 1.0f;
        gi.height[0] = time != 0 ? static_cast<float>(band) * 2.5f + 15.0f : static_cast<float>(band) * -2.5f - 15.0f;
        gi.riseAngle = rise;
        gi.alphaB = 0;
    }
}

void CRagEffect::ApplyChemical3(short time)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    if (m_stateCnt != 0) {
        return;
    }
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeChemical3), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 513u;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture(time >= 10 ? "effect\\line3y.tga" : "effect\\line3.tga", false));
    const int digit = time % 10;
    const short rise = Chemical2RiseAngle(m_deltaPos.x - origin.x, m_deltaPos.z - origin.z);
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.distance = 100.0f;
        gi.alphaB = 0;
        gi.riseAngle = rise;
        if (digit == 0) {
            gi.height[0] = -15.0f - static_cast<float>(rand() % 11);
        } else if (digit == 1) {
            gi.height[0] = static_cast<float>(rand() % 11) + 15.0f;
        }
        gi.height[1] = static_cast<float>(rand() % 200);
        if (time >= 10 && time < 20) {
            gi.maxHeight = static_cast<float>(rand() % 6) * 0.1f + 0.5f;
            gi.height[2] = static_cast<float>(rand() % 31) + 40.0f;
            gi.flag[0] = static_cast<u8>(rand() % 3 + 10);
            gi.height[3] = static_cast<float>(rand() % 11) * 0.1f + 1.0f;
        } else if (time >= 20) {
            gi.maxHeight = static_cast<float>(rand() % 4) * 0.1f + 0.40000001f;
            gi.height[2] = static_cast<float>(rand() % 31) + 30.0f;
            gi.flag[0] = static_cast<u8>(rand() % 3 + 20);
            gi.height[3] = static_cast<float>(rand() % 21) * 0.1f + 1.5f;
        } else {
            gi.maxHeight = static_cast<float>(rand() % 4) * 0.1f + 0.30000001f;
            gi.height[2] = static_cast<float>(rand() % 31) + 20.0f;
            gi.flag[0] = static_cast<u8>(rand() % 3);
            gi.height[3] = static_cast<float>(rand() % 21) * 0.1f + 1.5f;
        }
    }
}

void CRagEffect::ApplyChemicalProtection(short time)
{
    if (m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeChemical), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\shockwave_c.bmp", false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.alphaB = 0;
        gi.distance = 100.0f;
        gi.riseAngle = static_cast<short>(time + 90 * band);
    }
}

void CRagEffect::ApplyFirstAid()
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    m_cachedPos = origin;
    for (CEffectPrim* prim : m_primList) {
        if (prim != nullptr && prim->m_type == static_cast<EFFECTPRIMID>(kPrimTypeFirstAid)) {
            prim->m_pos = origin;
        }
    }
    if (m_stateCnt != 0) {
        return;
    }
    vector3d soundPos = origin;
    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        if (gameMode->m_world != nullptr && gameMode->m_world->m_player != nullptr) {
            soundPos.y = gameMode->m_world->m_player->m_pos.y + 150.0f;
        }
    }
    TryPlayEffectWaveAt(soundPos, "_heal_effect.wav");
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeFirstAid), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_size = 0.0f;
    prim->m_pos = origin;
    prim->m_texture.push_back(g_texMgr.GetTexture("effect\\pikapika2.bmp", false));
    EffectGi& gi = prim->m_gi[0];
    gi.life = 1;
    gi.process = 0;
    gi.distance = 10.0f;
    gi.alphaB = 0;
    gi.height[6] = 0.02f;
    gi.height[8] = static_cast<float>(rand() % 360);
    gi.fullDisplayAngle = static_cast<short>(rand() % 360);
    gi.flag[0] = 0;
    gi.flag[2] = 1;
    gi.sparkX = origin.x;
    gi.sparkY = origin.y - 12.0f;
    gi.sparkZ = origin.z;
}

void CRagEffect::ApplyHealTime(int variant)
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    if (m_stateCnt == 0) {
        vector3d soundPos = origin;
        CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        if (gameMode != nullptr && gameMode->m_world != nullptr && gameMode->m_world->m_player != nullptr) {
            soundPos.y = gameMode->m_world->m_player->m_pos.y + (variant != 0 ? -80.0f : -40.0f);
        }
        TryPlayEffectWaveAt(soundPos, variant != 0 ? "effect\\\xC8\xED\xB1\xE2.wav" : "effect\\_heal_effect.wav");
    }
    if (m_stateCnt > 20 || (m_stateCnt % 4) != 0) {
        return;
    }
    CEffectPrim* spark = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeParticleUp), vector3d{});
    if (spark == nullptr) {
        return;
    }
    spark->m_renderFlag = 5;
    spark->m_duration = m_duration;
    spark->m_size = 0.0f;
    spark->m_pos = origin;
    spark->m_texture.push_back(g_texMgr.GetTexture("effect\\pok1.tga", false));
    spark->m_texture.push_back(g_texMgr.GetTexture("effect\\pok3.tga", false));
    for (int band = 0; band < 4; ++band) {
        EffectGi& gi = spark->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.distance = static_cast<float>(rand() % 10) * 0.1f + 1.5f;
        gi.height[8] = 0.0f;
        gi.height[1] = 0.0f;
        gi.height[2] = static_cast<float>(rand() % 20) * 0.0099999998f + 0.2f;
        gi.alphaB = 0;
        gi.sparkX = static_cast<float>(rand() % 7) + origin.x - 3.0f;
        gi.sparkZ = static_cast<float>(rand() % 7) + origin.z - 3.0f;
        gi.sparkY = origin.y - 6.0f;
        gi.rotStart = 0;
        gi.fullDisplayAngle = 0;
        gi.flag[0] = 0;
        gi.flag[1] = 1;
        gi.flag[2] = static_cast<u8>(variant != 0 ? 1 : 11);
    }
}

void CRagEffect::ApplyGlassWall2(const char* textureName)
{
    if (textureName == nullptr) {
        return;
    }
    if (m_stateCnt == 0) {
        CRenderObject* master = ResolveCurrentMaster();
        const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
        TryPlayEffectWaveAt(origin, "effect\\EF_GlassWall.wav");
        CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
        if (prim != nullptr) {
            prim->m_renderFlag = 5;
            prim->m_duration = m_duration;
            prim->m_pos = origin;
            prim->m_size = 10.0f;
            prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
            ConfigureHealBand(prim->m_gi[0], 34.0f, static_cast<short>(rand() % 360), 3.5999999f, 90, 3080);
            ConfigureHealBand(prim->m_gi[1], 37.0f, static_cast<short>(rand() % 360), 3.3f, 90, 3080);
            ConfigureHealBand(prim->m_gi[2], 40.0f, static_cast<short>(rand() % 360), 3.0f, 90, 3080);
            prim->m_gi[3].life = 0;
        }
    }
    ProcessEZ2STR();
}

void CRagEffect::ApplyExit2(const char* textureName)
{
    if (textureName == nullptr || m_stateCnt != 0) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    TryPlayEffectWaveAt(origin, "effect\\EF_Teleportation.wav");
    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = 4.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
    ConfigureHealBand(prim->m_gi[3], 20.0f, 0, 2.4000001f, 84, 60);
    ConfigureHealBand(prim->m_gi[2], 45.0f, 90, 2.2f, 86, 60);
    ConfigureHealBand(prim->m_gi[1], 70.0f, 180, 2.0f, 88, 60);
    ConfigureHealBand(prim->m_gi[0], 180.0f, 270, 1.8f, 90, 58);
}

void CRagEffect::ApplyHeal2(const char* textureName, int variant)
{
    if (textureName == nullptr) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    if (m_stateCnt == 0) {
        if (variant < 2) {
            TryPlayEffectWaveAt(origin, "_heal_effect.wav");
        }
        CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
        if (prim == nullptr) {
            return;
        }
        prim->m_renderFlag = 5;
        prim->m_duration = m_duration;
        prim->m_pos = origin;
        prim->m_size = variant == 2 ? 4.0f : 9.0f;
        prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));
        const short alphaPeak = variant != 0 ? 40 : 60;
        const float upperHeight = variant != 0 ? 18.0f : 12.0f;
        ConfigureHealBand(prim->m_gi[0], 60.0f, 0, BitsToFloat(0x40933333u), 90, alphaPeak);
        ConfigureHealBand(prim->m_gi[1], 60.0f, 180, 5.0f, 90, alphaPeak);
        ConfigureHealBand(prim->m_gi[2], upperHeight, static_cast<short>(rand() % 360), 5.0f, 50, alphaPeak);
        ConfigureHealBand(prim->m_gi[3], upperHeight, static_cast<short>(rand() % 360), 5.1999998f, 48, alphaPeak);
    } else if (m_stateCnt <= 50 && (m_stateCnt % 6) == 0 && variant < 2) {
        const float speedBase = variant != 0 ? 0.60000002f : 0.40000001f;
        LaunchParticleUp(origin, 2.0f, speedBase, true, 1);
    }
    if (variant != 1) {
        return;
    }
    if (m_stateCnt == 1) {
        CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
        if (prim != nullptr) {
            prim->m_renderFlag = 5;
            prim->m_duration = m_duration;
            prim->m_pos = origin;
            prim->m_size = 9.0f;
            prim->m_texture.push_back(g_texMgr.GetTexture("effect\\alpha_down.tga", false));
            ConfigureHealBand(prim->m_gi[0], 90.0f, 0, 4.1999998f, 90, 40);
            ConfigureHealBand(prim->m_gi[1], 90.0f, 90, 4.3000002f, 90, 40);
            ConfigureHealBand(prim->m_gi[2], 10.0f, static_cast<short>(rand() % 360), 5.3000002f, 46, 30);
            ConfigureHealBand(prim->m_gi[3], 10.0f, static_cast<short>(rand() % 360), 5.4000001f, 44, 30);
        }
    }
    if (m_stateCnt <= 50 && (m_stateCnt % 9) == 2) {
        LaunchParticleUp(origin, 2.4000001f, 0.40000001f, true, 1);
    }
}

void CRagEffect::ApplyHeal(const char* textureName, int variant)
{
    if (textureName == nullptr) {
        return;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d origin = master != nullptr ? master->m_pos : m_cachedPos;
    m_effectPos = origin;
    if (m_stateCnt != 0) {
        const int limit = variant == 1 ? 25 : 50;
        if (m_stateCnt > limit || (m_stateCnt % 6) != 0) {
            return;
        }
        LaunchParticleUp(origin,
            2.0f,
            variant == 1 ? 0.40000001f : 0.30000001f,
            variant == 1,
            variant == 1 ? 1 : 0);
        return;
    }
    TryPlayEffectWaveAt(origin, "_heal_effect.wav");

    CEffectPrim* prim = LaunchEffectPrim(static_cast<EFFECTPRIMID>(kPrimTypeHeal), vector3d{});
    if (prim == nullptr) {
        return;
    }
    prim->m_renderFlag = 5;
    prim->m_duration = m_duration;
    prim->m_pos = origin;
    prim->m_size = variant == 1 ? 5.0f : 9.0f;
    prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false));

    const float distance = variant == 1
        ? 4.5f + static_cast<float>(rand() % 100) * 0.0049999999f
        : BitsToFloat(0x40933333u);
    const float maxHeight = variant == 1
        ? static_cast<float>(rand() % 20) + 40.0f
        : 40.0f;
    const short alphaPeak = variant == 1 ? 30 : 60;

    for (int band = 0; band < 2; ++band) {
        EffectGi& gi = prim->m_gi[static_cast<size_t>(band)];
        gi.life = 1;
        gi.process = 0;
        gi.fullDisplayAngle = 360;
        gi.rotStart = static_cast<short>(rand() % 360);
        gi.alphaB = 0;
        gi.riseAngle = 90;
        gi.sparkY = 0.0f;
        gi.alphaT = alphaPeak;
        gi.maxHeight = maxHeight;
        gi.distance = band == 0 ? distance : distance + 0.2f;
        if (variant != 1 && band == 1) {
            gi.distance = BitsToFloat(0x4099999Au);
            gi.maxHeight = 40.0f;
        }
        gi.height.fill(0.0f);
        gi.flag.fill(0);
    }
    prim->m_gi[2].life = 0;
    prim->m_gi[3].life = 0;
}

bool CRagEffect::OnProcess2()
{
    CRenderObject* master = ResolveCurrentMaster();
    const vector3d soundPos = master != nullptr ? master->m_pos : m_cachedPos;
    switch (m_type) {
    case 338:
        ProcessSuperAngelEffect();
        return true;
    case 337:
        ApplyJobLvUp50(0);
        ApplyJobLvUp50(45);
        return true;
    case 301:
        if (m_stateCnt == 300) {
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    TryPlayEffectWaveAt(mode->m_world->m_player->m_pos, "effect\\\xC6\xF8\xC1\xD7.wav");
                }
            }
        }
        if (m_stateCnt > 2300) {
            m_stateCnt = 299;
        }
        return true;
    case 312:
        ApplyHeal("effect\\alpha_down.tga", 0);
        return true;
    case 313:
        ApplyHeal2("effect\\ring_white.tga", 0);
        return true;
    case 314:
        ApplyExit2("effect\\ring_purple.tga");
        return true;
    case 308:
        ApplyThrowItem("\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xB5\xB9.bmp", 2);
        return true;
    case 304:
        ApplyTeleportation2("effect\\Magic_Violet.tga", 0);
        return true;
    case 303: {
        ApplyChemical2(0);
        ApplyChemical2(1);
        if (m_stateCnt == 44) {
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                mode->SendMsg(135, 0, 0, 0);
                vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
                TryPlayEffectWaveAt(wavePos, "effect\\\xBF\xAC\xC1\xD6\xBA\xD8\xB0\xDD.wav");
            }
        }
        if (master != nullptr && m_stateCnt >= 20 && m_stateCnt <= 120) {
            if ((master->m_BodyLight & 8) != 8) {
                master->m_BodyLight += 8;
            }
            master->SetArgb(-1, 255, 255, 150);
        }
        return true;
    }
    case 300: {
        CRenderObject* chemicalMaster = ResolveCurrentMaster();
        const vector3d chemicalOrigin = chemicalMaster != nullptr ? chemicalMaster->m_pos : m_cachedPos;
        m_cachedPos = chemicalOrigin;
        if (m_stateCnt == 0) {
            vector3d wavePos = chemicalOrigin;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "apocalips_attack.wav");
        }
        ApplyChemicalProtection(30);
        ApplyChemicalProtection(60);
        if (m_stateCnt == 0) {
            m_cachedPos.y -= 9.0f;
        }
        return true;
    }
    case 309:
        ApplyFirstAid();
        return true;
    case 307:
        ApplyForestLight("effect\\cloud11.tga", 0);
        return true;
    case 310:
        ApplyParticleUpBurst();
        return true;
    case 322:
        ApplyForestLight("effect\\cloud11.tga", 1);
        return true;
    case 323:
        ApplyForestLight("effect\\cloud11.tga", 2);
        return true;
    case 324:
        ApplyForestLight("effect\\cloud11.tga", 10);
        return true;
    case 315:
        ApplyGlassWall2("effect\\alpha_down.tga");
        return true;
    case 318:
        ApplyBottomMagnus("effect\\ring_red.tga", 0);
        return true;
    case 319:
        ApplyBottomMagnus("effect\\alpha_down.tga", 1);
        return true;
    case 320:
        ApplyHeal("effect\\alpha_down.tga", 1);
        return true;
    case 321:
        ApplyWarpZone2("effect\\ring_blue.tga");
        return true;
    case 325:
        ApplyHeal2("effect\\ring_white.tga", 1);
        return true;
    case 331:
        ApplyHealTime(0);
        return true;
    case 332:
        ApplyHealTime(1);
        return true;
    case 334:
        ApplyBlind(0);
        return true;
    case 335:
        ApplyPoison(0);
        return true;
    case 316:
        ApplyReadyPortal2();
        return true;
    case 317:
        ApplyPortal2(0);
        return true;
    case 341:
        ApplyPortal2(3);
        return true;
    case 344:
        ApplyEntry2();
        return true;
    case 339:
        ApplyMagnum2("effect\\ring_yellow.tga");
        return true;
    case 346:
        for (short windTime = 0; windTime < 45; windTime = static_cast<short>(windTime + 9)) {
            ApplySphereWind("effect\\blue_ivy.bmp", windTime, 0);
        }
        return true;
    case 342:
        ApplyCoupleCasting("effect\\ring_red.tga");
        return true;
    case 343:
        if (m_stateCnt == 0) {
            vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\EF_BeginSpell.wav");
            const float heartOffsets[][2] = {
                { 0.0f, 9.9900007f },
                { -3.5100002f, 11.88f },
                { -7.8300004f, 12.960001f },
                { -12.42f, 11.34f },
                { -15.120001f, 7.0200005f },
                { -15.39f, 1.6200001f },
                { -13.77f, -3.2400002f },
                { -10.8f, -7.0200005f },
                { -6.7500005f, -10.26f },
                { -2.7f, -12.690001f },
                { 0.0f, -16.200001f },
                { 2.97f, -12.690001f },
                { 7.0200005f, -10.26f },
                { 11.070001f, -7.0200005f },
                { 14.040001f, -3.2400002f },
                { 15.660001f, 1.6200001f },
                { 15.39f, 7.0200005f },
                { 12.690001f, 11.34f },
                { 8.1000004f, 12.960001f },
                { 3.7800002f, 11.88f },
            };
            for (const auto& offset : heartOffsets) {
                ApplyHeartCasting("effect\\ring_red.tga", offset[0], offset[1]);
            }
        }
        return true;
    case 347:
        for (int paper = 0; paper < 50; ++paper) {
            ApplyColorPaper("effect\\white02.bmp");
        }
        return true;
    case 367:
        ApplyAuraBladeCasting();
        return true;
    case 370:
        ApplyBottomMusic("effect\\cross_old.bmp", 9);
        return true;
    case 345:
        ApplySaintWing("effect\\wing003.bmp");
        return true;
    case 305:
        if (m_stateCnt == 0) {
            TryPlayEffectWaveAt(soundPos, "effect\\p_success.wav");
        }
        ProcessEZ2STR();
        return true;
    case 306:
        if (m_stateCnt == 0) {
            TryPlayEffectWaveAt(soundPos, "effect\\p_failed.wav");
        }
        ProcessEZ2STR();
        return true;
    case 311:
        if (m_stateCnt == 0) {
            vector3d wavePos = soundPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\\xB0\xED\xBC\xBA\xB9\xE6\xB0\xA1.wav");
        }
        ProcessEZ2STR();
        return true;
    case 368:
        if (master != nullptr) {
            if ((master->m_BodyLight & 1) != 1) {
                ++master->m_BodyLight;
            }
            master->SetArgb(-1, 255, 100, 100);
        }
        return true;
    case 369:
        if (m_stateCnt == 0) {
            TryPlayEffectWaveAt(soundPos, "effect\\knight_twohandquicken.wav");
        }
        if (master != nullptr) {
            master->SetArgb(-1, 255, 255, 160);
        }
        ProcessEZ2STR();
        return true;
    case 371:
    case 372:
        ProcessEZ2STR();
        return true;
    default:
        return false;
    }
}

// Ref/HighPriest.exe.c:419629-419680, 421945-422012. One accepted
// processing call is one effect frame, independent of elapsed wall-clock time.
// HighPriest.exe.c CRagEffect::OnProcess: direct ProcessEZ2STR calls whose
// result reaches LABEL_425 (plus case 185's explicit false return). Cases
// 130/136/244/250/390/391/392/406/407/440 deliberately ignore that result.
bool EffectChecksStrResult(int effectId)
{
    switch (effectId) {
    case 23: case 25: case 29: case 30: case 41: case 49: case 52: case 64:
    case 65: case 66: case 68: case 69: case 70: case 75: case 76: case 77:
    case 78: case 83: case 84: case 85: case 88: case 91: case 92: case 95:
    case 96: case 97: case 102: case 103: case 106: case 107: case 108: case 109:
    case 110: case 111: case 113: case 124: case 126: case 127: case 129: case 131:
    case 133: case 134: case 135: case 139: case 141: case 143: case 144: case 146:
    case 147: case 148: case 149: case 150: case 151: case 152: case 153: case 154:
    case 155: case 156: case 157: case 158: case 167: case 168: case 169: case 181:
    case 182: case 183: case 185: case 186: case 187: case 188: case 189: case 190:
    case 191: case 192: case 193: case 194: case 195: case 197: case 199: case 204:
    case 205: case 206: case 207: case 208: case 209: case 210: case 211: case 213:
    case 214: case 218: case 219: case 234: case 235: case 245: case 248: case 251:
    case 255: case 256: case 257: case 258: case 268: case 269: case 270: case 271:
    case 272: case 273:
        return true;
    default:
        return false;
    }
}

bool CRagEffect::FinishEffectFrame()
{
    if (!m_strFrameActive) {
        if (m_type == 145) {
            // Ref restarts the clip in this case, then the outer frame advances.
            InitEZ2STRFrame();
        } else if (EffectChecksStrResult(m_type)) {
            return false;
        }
    }
    return true;
}

u8 CRagEffect::OnProcess()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master) {
        return 0;
    }
    m_cachedPos = master->m_pos;
    m_longitude = master->m_roty;
    if (!EffectTicksWhenNoFrameTime(m_type) && !g_isSkipFrame) {
        return 1;
    }
    if (g_isSkipFrame > 0 && master->m_BeZero == 1) {
        master->m_BodyLight = 0;
        master->m_BeZero = 0;
    }
    if (m_isWorldEffect) {
        return ProcessWorldEffect();
    }
    if (!ProcessEffectFrame()) {
        return 0;
    }
    ++m_stateCnt;
    return m_stateCnt < m_duration;
}

// Legacy RSW map-emitter scheduling. Skill/packet effects use OnProcess's
// reference frame count and never extend their life for a clip or primitive.
u8 CRagEffect::ProcessWorldEffect()
{
    const u32 now = timeGetTime();
    const u32 elapsedMs = now - m_lastProcessTick;
    m_lastProcessTick = now;
    m_tickCarryMs += static_cast<float>(elapsedMs);

    const bool frameDrivenStrHandler =
        m_handler == Handler::EzStr;

    int steps = 0;
    if (frameDrivenStrHandler) {
        // Ref-style STR playback advances once per world process pass.
        steps = 1;
        m_tickCarryMs = 0.0f;
    } else {
        steps = static_cast<int>(m_tickCarryMs / kEffectTickMs);
    }
    const bool burstSensitiveHandler =
        m_handler == Handler::Portal
        || m_handler == Handler::ReadyPortal
        || m_handler == Handler::WarpZone
        || m_handler == Handler::MapPillar
        || m_handler == Handler::MapMagicZone
        || m_handler == Handler::MapParticle
        || m_handler == Handler::BlueFall
        || m_handler == Handler::WeatherCloud;
    if (steps > 1 && burstSensitiveHandler) {
        steps = 1;
    }
    if (!frameDrivenStrHandler && steps > 0) {
        m_tickCarryMs -= static_cast<float>(steps) * kEffectTickMs;
    }

    bool alive = true;
    if (steps <= 0) {
        if (m_loop) {
            return 1;
        }
        const bool ezActive = frameDrivenStrHandler
            && m_aniClips
            && m_stateCnt <= (m_aniClips->cFrame + 8);
        return (ezActive || !m_primList.empty() || m_stateCnt <= m_duration) ? 1 : 0;
    }

    for (int step = 0; step < steps && alive; ++step) {
        if (!ProcessEffectFrame()) {
            return 0;
        }
        ++m_stateCnt;

        if (m_loop && m_handler == Handler::EzStr && m_aniClips && m_stateCnt > (m_aniClips->cFrame + 1)) {
            InitEZ2STRFrame();
            continue;
        }

        if (m_loop) {
            alive = true;
            continue;
        }

        const bool ezActive = frameDrivenStrHandler
            && m_aniClips
            && m_stateCnt <= (m_aniClips->cFrame + 8);
        const bool primActive = !m_primList.empty();
        alive = ezActive || primActive || m_stateCnt <= m_duration;
    }

    return alive ? 1 : 0;
}

// HighPriest.exe.c:419760-419805: all 45 cases jump directly to LABEL_424
// (420847-420852). No sound, RNG, emission or other helper runs for these ids.
// An empty optional leaves dispatch to the remaining original cases.
std::optional<bool> CRagEffect::ProcessReferenceStrOnly()
{
    switch (m_type) {
    case 23: case 25: case 29: case 52: case 64: case 70: case 109: case 110:
    case 126: case 135: case 139: case 141: case 143: case 144: case 146:
    case 147: case 148: case 149: case 150: case 151: case 152: case 156:
    case 157: case 158: case 167: case 168: case 169: case 181: case 182:
    case 186: case 187: case 188: case 189: case 190: case 191: case 192:
    case 193: case 194: case 195: case 197: case 199: case 213: case 214:
    case 235: case 268:
        return ProcessEZ2STR(0.0f,-80.0f,0.0f);
    default:
        return std::nullopt;
    }
}

bool CRagEffect::ProcessEffectFrame()
{
    m_strFrameActive = true;
    if (const std::optional<bool> result = ProcessReferenceStrOnly()) return *result;
    if (m_type == 0x201) {
        ProcessGroundSample();
        return true;
    }
    if (m_type == 4) {
        SpawnHit5();
        return true;
    }
    if (m_type == 5) {
        SpawnHit6();
        return true;
    }
    if (m_type == 3) {
        SpawnHit4();
        return true;
    }
    if (m_type == 2) {
        SpawnHit3();
        return true;
    }
    if (m_type == 1) {
        SpawnHit2(0);
        return true;
    }
    if (m_type == 183) return ProcessHit2Blue();
    if (m_type == 40) return ProcessSignum();
    if (m_type == 41) return ProcessAngelus();
    if (m_type == 49) return ProcessFireHit();
    if (m_type == 51) {
        SpawnColdHit();
        return true;
    }
    if (m_type == 50) {
        SpawnFireSplashHit();
        return true;
    }
    if (m_type == 48) {
        SpawnSprayPond();
        return true;
    }
    if (m_type == 32) {
        SpawnNapalmBeat();
        return true;
    }
    if (m_type == 401) {
        ApplyNapalmHit();
        return true;
    }
    if (m_type == 81) {
        SpawnPierce();
        return true;
    }
    if (m_type == 180 || m_type == 184) {
        SpawnHitDark();
        return true;
    }
    if (m_type == 0) {
        SpawnHit1();
        return true;
    }
    if (m_type == 10) return ProcessCoin();
    if (m_type == 18) {
        SpawnSteal();
        return true;
    }
    if (m_type == 46) {
        SpawnSandWind();
        return true;
    }
    if (m_type == 45) {
        SpawnFireFly();
        return true;
    }
    if (m_type >= 54 && m_type <= 59) {
        SpawnBeginSpell();
        return true;
    }
    if (m_type == 53) {
        SpawnPoisonHit();
        return true;
    }
    if (m_type == 39) {
        SpawnAqua();
        return true;
    }
    if (m_type == 38) {
        SpawnDecAgility();
        return true;
    }
    if (m_type == 37 || m_type == 43) {
        SpawnIncAgility();
        return true;
    }
    if (m_type == 16) {
        ApplyBash();
        return true;
    }
    if (m_type == 121 || m_type == 122) {
        if (m_type == 121) ApplySonicBlow();
        else ApplySonicBlowHit();
        return true;
    }
    if (m_type == 17) {
        ApplyMagnumBreak();
        return true;
    }
    if (m_type == 34) {
        ApplyTeleportation();
        return true;
    }
    if (m_type == 128) {
        ApplyOverThrust();
        return true;
    }
    if (m_type == 142) {
        ApplyHeavensDrive();
        return true;
    }
    if (m_type == 14) {
        ApplyHealSP();
        return true;
    }
    if (m_type == 98) {
        ApplyHasteUp();
        return true;
    }
    if (m_type == 123 || m_type == 132) {
        if (m_type == 123) SpawnGrimTooth();
        else SpawnGrimToothAtk();
        return true;
    }
    if (m_type == 27 || m_type == 28) {
        if (m_type == 27) ApplyFrostDiver();
        else ApplyFrostDiver2();
        return true;
    }
    if (m_type == 31) {
        ApplyFireArrow();
        return true;
    }
    if (m_type == 26) {
        ApplyIceArrow();
        return true;
    }
    if (m_type == 218 || m_type == 219) return ProcessPotionConcentration();
    if (m_type == 220) return ApplyPotionBerserk();
    if (m_type == 221) {
        ApplyPotionPillar(1.0f, 50);
        return true;
    }
    if (m_type == 61) {
        ApplyWarpZone();
        return true;
    }
    if (m_type == 36) {
        ApplyPortal();
        return true;
    }
    if (m_type == 35) {
        ApplyReadyPortal();
        return true;
    }
    if (m_type == 582) {
        ProcessSuperAngelEffect();
        return true;
    }
    if (m_type == 590) {
        ProcessJobLvUp50_2();
        return true;
    }
    CRenderObject* master = ResolveCurrentMaster();
    const bool dispatchHandler = true;
    const bool kyrieGuard = dispatchHandler && m_type == 392;
    const bool bodyLight442 = dispatchHandler && m_type == 442;
    const bool bodyLight446 = dispatchHandler && m_type == 446;
    const bool bodyLight449 = dispatchHandler && m_type == 449;
    const bool tarot = dispatchHandler && m_type >= 523 && m_type <= 536;
    const bool actorLight = dispatchHandler && (
        m_type == 414 || m_type == 415 || m_type == 417 || m_type == 419 || m_type == 420
        || m_type == 421 || m_type == 422 || m_type == 423 || m_type == 424 || m_type == 425
        || m_type == 426 || m_type == 427 || m_type == 458
        || m_type == 538 || m_type == 540 || m_type == 542 || m_type == 544
        || m_type == 375 || m_type == 379 || m_type == 382 || m_type == 385 || m_type == 387 || m_type == 396
        || m_type == 500 || m_type == 521 || m_type == 548 || m_type == 549 || m_type == 550
        || m_type == 625 || m_type == 626 || m_type == 655 || m_type == 656 || m_type == 667);
    const bool portalWind402 = dispatchHandler && m_type == 402;
    const bool blackDevil = dispatchHandler && m_type == 487;
    const bool bodyLightBit = dispatchHandler && m_type == 474;
    const bool blindLevel = dispatchHandler && m_type >= 475 && m_type <= 484;
    const bool beginCast = dispatchHandler && (m_type >= 172 && m_type <= 179);
    const bool bottomLight = dispatchHandler && (m_type == 280 || m_type == 285);
    const bool bottomOut = dispatchHandler && m_type == 283;
    const bool blastMine = dispatchHandler && m_type == 676;
    const bool chemicalSlash = dispatchHandler && (m_type == 439 || m_type == 512 || m_type == 623);
    const bool giExplosion = dispatchHandler && m_type == 514;
    const bool acidDemon = dispatchHandler && m_type == 537;
    const bool flowerCast = dispatchHandler && m_type == 488;
    const bool defender = dispatchHandler && (m_type == 222 || m_type == 252);
    const bool mapPillar = dispatchHandler && (m_type == 231 || m_type == 247 || m_type == 259 || m_type == 260);
    const bool volcano = dispatchHandler && (m_type == 223 || m_type == 225 || m_type == 236 || m_type == 237 || m_type == 238 || m_type == 261 || m_type == 263);
    const bool bottomVolcano = dispatchHandler && (m_type == 239 || m_type == 240 || m_type == 241 || m_type == 620);
    const bool wind = dispatchHandler && m_type == 224;
    const bool stormKick = dispatchHandler && (m_type == 435 || m_type == 459 || m_type == 460 || m_type == 461 || m_type == 464 || m_type == 465);
    const bool halfSphere = dispatchHandler && m_type == 436;
    const bool attackEnergy = dispatchHandler && m_type == 437;
    const bool electric = dispatchHandler && m_type == 432;
    const bool electric2 = dispatchHandler && m_type == 433;
    const bool attackEnergy2 = dispatchHandler && m_type == 438;
    const bool healBlue = dispatchHandler && m_type == 253;
    const bool bottomMusic = dispatchHandler && (m_type == 278 || m_type == 279 || m_type == 282 || m_type == 286
        || m_type == 289 || m_type == 291 || m_type == 293 || m_type == 674);
    if (tarot) {
        static const char* kTarotTextures[] = {
            "effect\\Tarot01.tga", "effect\\Tarot02.tga", "effect\\Tarot03.tga", "effect\\Tarot04.tga",
            "effect\\Tarot05.tga", "effect\\Tarot06.tga", "effect\\Tarot07.tga", "effect\\Tarot08.tga",
            "effect\\Tarot09.tga", "effect\\Tarot10.tga", "effect\\Tarot11.tga", "effect\\Tarot12.tga",
            "effect\\Tarot13.tga", "effect\\Tarot14.tga"
        };
        ApplyTarot(kTarotTextures[m_type - 523], 1);
    } else if (dispatchHandler && m_type == 554) {
        ApplyTarot("effect\\freeze_a.bmp", 2);
    } else if (dispatchHandler && (m_type == 610 || m_type == 611)) {
        if (m_stateCnt == 0) {
            CRenderObject* textureMaster = ResolveCurrentMaster();
            vector3d wavePos = textureMaster != nullptr ? textureMaster->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, m_type == 610 ? "_heal_effect.wav" : "caramel_die.wav");
        }
        ApplyTarot(m_type == 610 ? "effect\\success.bmp" : "effect\\failed.bmp", 3);
    } else if (dispatchHandler && m_type == 669) {
        if (m_stateCnt == 1) {
            CRenderObject* poisonMaster = ResolveCurrentMaster();
            vector3d wavePos = poisonMaster != nullptr ? poisonMaster->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\wideb.wav");
        }
        ApplyPoison(1);
        ProcessEZ2STR();
    } else if (dispatchHandler && m_type == 673) {
        ApplyPoison(2);
    } else if (dispatchHandler && (m_type == 600 || m_type == 613 || m_type == 614 || m_type == 615 || m_type == 616)) {
        const char* throwTexture = "effect\\coin_a.bmp";
        if (m_type == 600) {
            throwTexture = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xA3\xB3\xD1\xB3\xAA\xC0\xCC\xC7\xC1.bmp";
        } else if (m_type == 613) {
            throwTexture = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBC\xF6\xB8\xAE\xB0\xCB.bmp";
        } else if (m_type == 614) {
            throwTexture = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC4\xED\xB3\xAA\xC0\xCC_\xB5\xB6.bmp";
        } else if (m_type == 615) {
            throwTexture = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC7\xB3\xB8\xB6_\xBF\xAD\xC8\xAD.bmp";
        }
        ApplyThrowItem(throwTexture, 3);
    } else if (dispatchHandler && m_type == 298) {
        ApplyThrowItem("\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xF3\xBA\xB4.bmp", 0);
    } else if (dispatchHandler && (m_type == 299 || m_type == 541)) {
        ApplyThrowItem(m_type == 299
            ? "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBA\xF3\xC6\xF7\xBC\xC7\xBA\xB4.bmp"
            : "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xB9\xF6\xBC\xAD\xC5\xA9\xC6\xF7\xBC\xC7.bmp",
            1);
    } else if (dispatchHandler && m_type == 539) {
        ApplyThrowItem("\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xC8\xAD\xBF\xB0\xBA\xB4.bmp", 10);
        ApplyThrowItem("\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\\xBF\xB0\xBB\xEA\xBA\xB4.bmp", 11);
    } else if (dispatchHandler && m_type == 405) {
        ApplyBottomMagnus("effect\\ring_white.tga", 2);
    } else if (dispatchHandler && m_type == 581) {
        ApplyForestLight("effect\\cloud11.tga", 11);
    } else if (dispatchHandler && m_type == 564) {
        ApplyCoupleCasting("effect\\ring_red.tga");
    } else if (dispatchHandler && m_type == 12) {
        ApplyPairedSaintCasting("effect\\ring_yellow.tga", 1, true, 0.0f);
    } else if (dispatchHandler && m_type == 441) {
        ApplyPairedSaintCasting("effect\\ring_blue.tga", 5, true, -100.0f);
    } else if (dispatchHandler && m_type == 454) {
        ApplyPairedSaintCasting("effect\\ring_black.tga", 6, true, 0.0f);
    } else if (beginCast) {
        switch (m_type) {
        case 172:
            ApplyBeginCasting("effect\\ring_black.tga", 3, false);
            break;
        case 173:
            ApplyBeginCasting("effect\\ring_red.tga", 3, false);
            break;
        case 174:
            ApplyBeginCasting("effect\\ring_blue.tga", 3, false);
            break;
        case 175:
            ApplyBeginCasting("effect\\ring_yellow.tga", 3, false);
            break;
        case 176:
            ApplyBeginCasting("effect\\ring_jadu.tga", 3, false);
            break;
        case 177:
            ApplyBeginCasting("effect\\ring_brown.tga", 3, false);
            break;
        case 178:
            ApplyBeginCasting("effect\\ring_white.tga", 3, false);
            break;
        case 179:
            ApplyBeginCasting("effect\\ring_purple.tga", 3, false);
            break;
        default:
            break;
        }
    } else if (healBlue) {
        if (m_stateCnt == 0) {
            CRenderObject* healMaster = ResolveCurrentMaster();
            vector3d wavePos = healMaster != nullptr ? healMaster->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y - 80.0f;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\\xC8\xED\xB1\xE2.wav");
        }
        ApplyHeal2("effect\\ring_blue.tga", 2);
    } else if (giExplosion) {
        ApplyGiExplosion("effect\\ring_blue.tga");
    } else if (acidDemon) {
        if (m_stateCnt == 5) {
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                mode->SendMsg(135, 0, 0, 0);
            }
        }
        ApplyAcidDemon("effect\\ring_black.tga");
    } else if (flowerCast) {
        if (m_stateCnt == 5) {
            vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\T_\xBE\xC8\xB6\xF4\xC7\xD1\xB8\xB6\xB9\xFD.wav");
        }
        if (master != nullptr && (master->m_BodyLight & 0x800) != 0x800) {
            master->m_BodyLight += 2048;
            if (m_stateCnt >= 200) {
                if (m_stateCnt < 400) {
                    master->m_BodySin5 = static_cast<u8>(-112 - (m_stateCnt & 0xFF));
                }
            } else {
                master->m_BodySin5 = static_cast<u8>(m_stateCnt);
            }
        }
        ApplyFlowerCasting("effect\\ring_blue.tga", 4.0f, 0);
        ApplyFlowerCasting("effect\\ring_blue.tga", 4.0f, 1);
    } else if (defender) {
        ApplyDefender(m_type == 222 ? "effect\\ring_black.tga" : "effect\\ring_yellow.tga");
    } else if (mapPillar) {
        const char* textureName = "effect\\ring_blue.tga";
        float size = 4.0f;
        bool wide = false;
        if (m_type == 247) {
            wide = true;
        } else if (m_type == 259) {
            textureName = "effect\\magic_green.tga";
            size = 7.0f;
        } else if (m_type == 260) {
            textureName = "effect\\ring_red.tga";
            size = 6.0f;
        }
        ApplyMapPillar(textureName, size, wide);
    } else if (volcano) {
        const char* textureName = "effect\\ring_white.tga";
        short variant = 0;
        bool fastSpin = false;
        if (m_type == 223) {
            variant = 2;
        } else if (m_type == 225) {
            textureName = "effect\\ring_red.tga";
        } else if (m_type == 236) {
            textureName = "effect\\ring_blue.tga";
        } else if (m_type == 237) {
            textureName = "effect\\magic_green.tga";
        } else if (m_type == 261) {
            textureName = "effect\\ring_yellow.tga";
            fastSpin = true;
        } else if (m_type == 263) {
            textureName = "effect\\ring_yellow.tga";
            variant = 1;
        }
        ApplyVolcano(textureName, variant, fastSpin);
    } else if (bottomVolcano) {
        const char* textureName = "effect\\ring_blue.tga";
        float size = 0.0f;
        if (m_type == 239) {
            textureName = "effect\\ring_red.tga";
        } else if (m_type == 241) {
            textureName = "effect\\magic_green.tga";
        } else if (m_type == 620) {
            size = 14.0f;
        }
        ApplyBottomVolcano(textureName, size);
    } else if (wind) {
        ApplyWind();
    } else if (stormKick) {
        if (m_type == 435 && m_stateCnt == 5) {
            vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\T_\xC8\xB8\xBF\xC0\xB8\xAE\xC2\xF7\xB1\xE2.wav");
        }
        const int variant = m_type == 459 ? 1 : (m_type == 460 ? 2 : (m_type == 461 ? 3 : (m_type == 464 ? 6 : (m_type == 465 ? 7 : 0))));
        ApplyStormKick(variant);
        ApplyPortalWind(2);
        ApplyPortalWind(3);
        if (master != nullptr && ((m_type == 435 && m_stateCnt <= 47) || (m_type == 459 && m_stateCnt <= 50))) {
            master->m_roty += m_type == 435 ? 30.0f : 14.0f;
            if (master->m_roty > 360.0f) {
                master->m_roty -= 360.0f;
            }
        }
    } else if (halfSphere) {
        ApplyHalfSphere("effect\\white02.bmp");
    } else if (attackEnergy) {
        ApplyHalfSphere("effect\\white02.bmp");
        ApplyAttackEnergy("effect\\ring_blue.tga");
    } else if (electric) {
        if (m_stateCnt == 25) {
            vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\T_\xC0\xFC\xB1\xE2.wav");
        }
        ApplyElectric(0);
        ApplyElectric(180);
    } else if (electric2) {
        ApplyElectric2();
        ApplyElectric2();
    } else if (attackEnergy2) {
        if (m_stateCnt == 0) {
            vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\T_\xB8\xB6\xB9\xFD\xB9\xDD\xBB\xE7.wav");
        }
        ApplyAttackEnergy2("effect\\cloud11.tga");
    } else if (chemicalSlash) {
        if (m_type == 623 && m_stateCnt == 28) {
            vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\\xC7\xC7\xBE\xEE\xBD\xCC\xBC\xA6.wav");
        }
        const short slashTime = static_cast<short>(m_type == 512 ? 10 : (m_type == 623 ? 20 : 0));
        for (int repeat = 0; repeat < 4; ++repeat) {
            ApplyChemical3(slashTime);
            ApplyChemical3(static_cast<short>(slashTime + 1));
        }
        if (m_type == 512 && m_stateCnt == 44) {
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                mode->SendMsg(135, 0, 0, 0);
                vector3d wavePos = master != nullptr ? master->m_pos : m_cachedPos;
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
                TryPlayEffectWaveAt(wavePos, "effect\\\xBF\xAC\xC1\xD6\xBA\xD8\xB0\xDD.wav");
            }
        }
        if (m_type == 512 && master != nullptr && m_stateCnt >= 20 && m_stateCnt <= 100) {
            if ((master->m_BodyLight & 8) != 8) {
                master->m_BodyLight += 8;
            }
            master->SetArgb(-1, 255, 255, 150);
        }
    } else if (blastMine) {
        ApplyTarot("effect\\blast_mine##clock.bmp", 4);
        if (m_stateCnt == 10) {
            CRenderObject* blastMaster = ResolveCurrentMaster();
            vector3d wavePos = blastMaster != nullptr ? blastMaster->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    wavePos.y = mode->m_world->m_player->m_pos.y;
                }
            }
            TryPlayEffectWaveAt(wavePos, "effect\\EF_DecAgility.wav");
        }
    } else if (bottomOut) {
        ApplyBottomOut("effect\\safeline.bmp", 0);
    } else if (bottomLight) {
        ApplyBottomLight("effect\\twirl.bmp", m_type == 280 ? 8 : 4);
    } else if (bottomMusic) {
        switch (m_type) {
        case 278:
            ApplyBottomMusic("effect\\zz.bmp", 1);
            break;
        case 279:
            ApplyBottomMusic("effect\\pocket.bmp", 3);
            break;
        case 282:
            ApplyBottomMusic("effect\\twirl.bmp", 7);
            break;
        case 286:
            ApplyBottomMusic("effect\\melody_b.bmp", 0);
            break;
        case 289:
            ApplyBottomMusic("effect\\idun_apple.bmp", 6);
            break;
        case 291:
            ApplyBottomMusic("effect\\melody_a.bmp", 0);
            break;
        case 293:
            ApplyBottomMusic("effect\\kiss.bmp", 4);
            break;
        case 674:
            ApplyBottomMusic("effect\\curse.bmp", 6);
            break;
        default:
            break;
        }
    } else if (dispatchHandler && m_type == 394) {
        for (short windTime = 0; windTime < 45; windTime = static_cast<short>(windTime + 9)) {
            ApplySphereWind("effect\\fire_ivy.bmp", windTime, 1);
        }
    } else if (kyrieGuard) {
        ApplyKyrieGuard();
    } else if (bodyLight442) {
        ApplyBodyLight442();
    } else if (bodyLight446) {
        ApplyBodyLight446();
    } else if (bodyLight449) {
        ApplyBodyLight449();
    } else if (actorLight) {
        ApplyActorLight();
    } else if (portalWind402) {
        ApplyPortalWind402();
    } else if (blackDevil) {
        ApplyBlackDevil("effect\\ring_black.tga");
    } else if (bodyLightBit && master != nullptr && (master->m_BodyLight & 2) != 2) {
        master->m_BodyLight += 2;
    } else if (blindLevel) {
        ApplyBlind(m_type - 474);
    }
    const bool tierHandled = dispatchHandler && m_type >= 300 && m_type <= 373 && OnProcess2();
    const bool textureCard = tarot || m_type == 554 || m_type == 610 || m_type == 611 || m_type == 669 || m_type == 673
        || m_type == 600 || m_type == 613 || m_type == 614 || m_type == 615 || m_type == 616
        || m_type == 298 || m_type == 299 || m_type == 539 || m_type == 541 || m_type == 405 || m_type == 581 || m_type == 394 || m_type == 564
        || m_type == 12 || m_type == 58 || m_type == 441 || m_type == 454;
    if (dispatchHandler && m_stateCnt == 0 && (m_type == 295 || m_type == 296)) {
        if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
            mode->SendMsg(179, m_type == 295 ? 1 : 0, 0, 0);
        }
    }
    if (dispatchHandler) {
        for (const ProvenClipWave& entry : kProvenClipWaves) {
            if (entry.type != m_type) {
                continue;
            }
            if (entry.quakeFrame == m_stateCnt) {
                if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                    mode->SendMsg(135, 0, 0, 0);
                }
            }
            if (entry.frame == m_stateCnt && (entry.type == 130 || entry.type == 250)) {
                m_duration = 9999;
            }
            if (entry.frame != m_stateCnt || (entry.chance != 0 && rand() % entry.chance != 0)) {
                break;
            }
            CRenderObject* waveMaster = ResolveCurrentMaster();
            vector3d wavePos = waveMaster != nullptr ? waveMaster->m_pos : m_cachedPos;
            if (CGameMode* mode = g_modeMgr.GetCurrentGameMode()) {
                if (mode->m_world != nullptr && mode->m_world->m_player != nullptr) {
                    if (entry.type == 245) {
                        wavePos = mode->m_world->m_player->m_pos;
                    }
                    wavePos.y = mode->m_world->m_player->m_pos.y + entry.yOffset;
                }
            }
            TryPlayEffectWaveAt(wavePos, entry.wave);
            break;
        }
    }
    if (dispatchHandler && m_type == 248) {
        ApplyProvidence("effect\\ring_yellow.tga");
    }
    if (dispatchHandler && !tierHandled && !textureCard && !kyrieGuard && !bodyLight442 && !bodyLight446 && !bodyLight449 && !actorLight && !portalWind402 && !blackDevil && !bodyLightBit && !blindLevel && !beginCast && !bottomLight && !bottomOut && !blastMine && !healBlue && !bottomMusic && !chemicalSlash && !giExplosion && !acidDemon && !flowerCast && !defender && !mapPillar && !volcano && !bottomVolcano && !wind && !stormKick && !halfSphere && !attackEnergy && !electric && !electric2 && !attackEnergy2) {
        switch (m_handler) {
        case Handler::EzStr:
            if (m_type == 185) {
                ProcessTargetPositionStr();
            } else {
                ProcessEZ2STR();
            }
            break;
        case Handler::Portal:
            SpawnPortal();
            break;
        case Handler::ReadyPortal:
            SpawnReadyPortal();
            break;
        case Handler::WarpZone:
            SpawnWarpZone();
            break;
        case Handler::MapPillar:
            SpawnMapPillar();
            break;
        case Handler::MapMagicZone:
            SpawnMapMagicZone();
            break;
        case Handler::MapParticle:
            SpawnMapParticle();
            break;
        case Handler::HideStart:
            SpawnHideStart();
            break;
        case Handler::EnchantPoison:
            SpawnEnchantPoison();
            break;
        case Handler::EnchantPoison2:
            ProcessEnchantPoison2();
            break;
        case Handler::Blessing:
            SpawnBlessing();
            break;
        case Handler::Smoke:
            SpawnSmoke();
            break;
        case Handler::Torch:
            SpawnTorch();
            break;
        case Handler::Sight:
            SpawnSight();
            break;
        case Handler::Sight2:
            SpawnSight2();
            break;
        case Handler::FireBall:
            SpawnFireBall();
            break;
        case Handler::Ruwach:
            SpawnRuwach();
            break;
        case Handler::BlueFall:
            SpawnBlueFall();
            break;
        case Handler::WeatherCloud:
            SpawnWeatherCloud();
            break;
        case Handler::None:
        default:
            break;
        }
    }

    return FinishEffectFrame();
}

void CRagEffect::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)
{
    switch (msg) {
    case 24:
        if (m_ownsMaster) {
            delete m_master;
        }
        m_master = nullptr;
        m_ownsMaster = false;
        m_masterIsActor = false;
        m_masterActorGid = 0;
        return;
    case 25:
        if (par1) {
            ClearPrims();
        } else if (sender) {
            const auto found = std::find_if(m_primList.begin(), m_primList.end(),
                [sender](CEffectPrim* prim) { return prim == sender; });
            if (found != m_primList.end()) {
                m_primList.erase(found);
            }
        }
        return;
    case 14:
        if (par1 != 0) {
            const vector3d* delta = reinterpret_cast<const vector3d*>(par1);
            m_deltaPos = *delta;
            m_targetPos = *delta;
            m_hasTargetPos = true;
        }
        if (par2 != 0) {
            m_deltaPos2 = *reinterpret_cast<const vector3d*>(par2);
        }
        return;
    case 44: {
        // Ref/RagEffect.cpp:12764-12814. These sentinels carry position /
        // colour control data; they do not update the ordinary count.
        const int value = static_cast<int>(par1);
        const int option = static_cast<int>(par2);
        if (value == 0xFFFF) {
            switch (option) {
            case 130:
            case 250:
                m_effectPos = { 200.0f, 200.0f, 0.0f };
                break;
            case 128:
                m_effectPos = { 250.0f, 150.0f, 150.0f };
                break;
            case 169:
                m_effectPos = { 180.0f, 180.0f, 250.0f };
                break;
            case 243:
                m_effectPos = { 250.0f, 250.0f, 250.0f };
                m_duration = static_cast<int>(par3);
                break;
            default:
                break;
            }
        } else if (value == 65534) {
            if (option == 128) {
                m_effectPos = { -1.0f, -1.0f, -1.0f };
            } else if (option == 130) {
                m_effectPos = { -2.0f, -2.0f, -2.0f };
            }
        } else {
            m_count = value > 50 ? 50 : value;
        }
        return;
    }
    case 46:
        if (par1 != 0) {
            const vector3d* params = reinterpret_cast<const vector3d*>(par1);
            m_param[0] = params[0].x;
            m_param[1] = params[0].y;
            m_param[2] = params[0].z;
            m_param[3] = params[1].x;
        }
        return;
    case 80:
        m_duration = static_cast<int>(par1);
        return;
    default:
        return;
    }
}

// HighPriest.exe.c:422017-422048: STR layers are independent of the
// reconstructed handler tag; layer zero is deliberately excluded.
void CRagEffect::Render(matrix* viewMatrix)
{
    if (!viewMatrix || !m_aniClips) return;
    for (int layer = 1; layer < m_aniClips->cLayer && layer < static_cast<int>(m_isLayerDrawn.size()); ++layer) {
        if (m_isLayerDrawn[static_cast<size_t>(layer)]) {
            RenderAniClip(layer, m_aniClips->aLayer[static_cast<size_t>(layer)],
                m_actXformData[static_cast<size_t>(layer)], viewMatrix);
        }
    }
}

CRagEffect* CreateWorldRagEffect(const C3dWorldRes::effectSrcInfo& source)
{
    CRagEffect* effect = new CRagEffect();
    if (!effect) {
        return nullptr;
    }
    effect->InitWorld(source);
    return effect;
}
