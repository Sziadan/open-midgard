#include "RagEffect.h"

#include "World.h"
#include "DebugLog.h"
#include "audio/Audio.h"
#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "render/DC.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include "res/Texture.h"
#include "session/Session.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>

namespace {

constexpr float kNearPlane = 10.0f;
constexpr float kSubmitNearPlane = 80.0f;
constexpr float kPi = 3.14159265f;
constexpr float kEffectTickMs = 24.0f;
constexpr float kEffectPixelRatioScale = 0.14285715f;

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
        ToLowerAscii(entry.variantPrefix).c_str(),
        entry.variantFirst + offset);
    return buffer;
}

D3DBLEND ResolveAniClipBlendMode(u32 blendMode)
{
    switch (static_cast<D3DBLEND>(blendMode)) {
    case D3DBLEND_DESTALPHA:
        return D3DBLEND_ONE;
    case D3DBLEND_INVDESTALPHA:
        return D3DBLEND_ZERO;
    default:
        return static_cast<D3DBLEND>(blendMode);
    }
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

void SubmitScreenQuadPivot(const tlvertex3d& anchor,
    CTexture* texture,
    float pivotX,
    float pivotY,
    float width,
    float height,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags)
{
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

void SubmitWorldTeiRect(const vector3d& vec1,
    const vector3d& vec2,
    const vector3d& vec3,
    const vector3d& vec4,
    const matrix& viewMatrix,
    CTexture* texture,
    unsigned int color,
    float tv0,
    float tv1,
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
        { { &vec1, &vec2, &vec3 }, { { 1.0f, tv0 }, { 1.0f, tv1 }, { 0.0f, tv1 } } },
        { { &vec4, &vec3, &vec1 }, { { 0.0f, tv0 }, { 0.0f, tv1 }, { 1.0f, tv0 } } },
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

void SetBandMode(EffectBandState* band, u8 mode)
{
    if (!band) {
        return;
    }
    band->modes.fill(mode);
}

void ConfigureBand(CEffectPrim* prim,
    int bandIndex,
    int process,
    float maxHeight,
    float rotStart,
    float distance,
    float riseAngle,
    float fadeThreshold,
    u8 mode)
{
    if (!prim || bandIndex < 0 || bandIndex >= static_cast<int>(prim->m_bands.size())) {
        return;
    }

    EffectBandState& band = prim->m_bands[static_cast<size_t>(bandIndex)];
    band = {};
    band.active = true;
    band.process = process;
    band.maxHeight = maxHeight;
    band.rotStart = rotStart;
    band.distance = distance;
    band.riseAngle = riseAngle;
    band.fadeThreshold = fadeThreshold;
    band.radius = 0.0f;
    SetBandMode(&band, mode);
}

float ResolveGroundHeight(const vector3d& position);

void RenderBandRibbon(const CEffectPrim& prim,
    const vector3d& base,
    const EffectBandState& band,
    const matrix& viewMatrix,
    CTexture* texture,
    float radiusScale)
{
    if (!band.active || !texture || texture == &CTexMgr::s_dummy_texture || band.alpha <= 0.0f) {
        return;
    }

    const int renderFlags = ResolveEffectRenderFlags(prim.m_renderFlag, 1 | 2);
    const D3DBLEND destBlend = ResolveEffectDestBlend(prim.m_renderFlag);
    const int sampleCount = static_cast<int>(band.heights.size());
    const vector3d center = prim.m_master ? prim.m_pos : base;
    const float groundY = ResolveGroundHeight(center) + prim.m_deltaPos2.y;
    const float ringRadius = (std::max)(0.4f, prim.m_size + band.distance * radiusScale);
    const float clampedAlpha = (std::min)(255.0f, band.alpha);
    const unsigned int topColor = PackColor(static_cast<unsigned int>(clampedAlpha), prim.m_tintColor);
    const unsigned int bottomColor = PackColor(static_cast<unsigned int>((std::min)(255.0f, clampedAlpha * 0.72f)), prim.m_tintColor);

    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        const int nextIndex = (sampleIndex + 1) % sampleCount;
        const float angle0 = WrapAngle360(band.rotStart + (360.0f * static_cast<float>(sampleIndex) / static_cast<float>(sampleCount)));
        const float angle1 = WrapAngle360(band.rotStart + (360.0f * static_cast<float>(nextIndex) / static_cast<float>(sampleCount)));
        const float radians0 = angle0 * (kPi / 180.0f);
        const float radians1 = angle1 * (kPi / 180.0f);
        const float riseRadians = band.riseAngle * (kPi / 180.0f);
        const float height0 = band.heights[static_cast<size_t>(sampleIndex)];
        const float height1 = band.heights[static_cast<size_t>(nextIndex)];
        const float radialLift0 = std::cos(riseRadians) * height0;
        const float radialLift1 = std::cos(riseRadians) * height1;
        const float verticalLift0 = std::sin(riseRadians) * height0;
        const float verticalLift1 = std::sin(riseRadians) * height1;
        const float outerRadius0 = ringRadius + radialLift0;
        const float outerRadius1 = ringRadius + radialLift1;

        const vector3d quad[4] = {
            vector3d{ center.x + std::cos(radians0) * ringRadius, groundY, center.z + std::sin(radians0) * ringRadius },
            vector3d{ center.x + std::cos(radians1) * ringRadius, groundY, center.z + std::sin(radians1) * ringRadius },
            vector3d{ center.x + std::cos(radians0) * outerRadius0, groundY - verticalLift0, center.z + std::sin(radians0) * outerRadius0 },
            vector3d{ center.x + std::cos(radians1) * outerRadius1, groundY - verticalLift1, center.z + std::sin(radians1) * outerRadius1 },
        };
        const unsigned int colors[4] = { bottomColor, bottomColor, topColor, topColor };
        const float uvs[4][2] = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } };
        SubmitWorldQuad(quad, viewMatrix, texture, colors, uvs, destBlend, 0.0f, renderFlags);
    }
}

void RenderCastingBandLowPolygon(const CEffectPrim& prim,
    const vector3d& base,
    const EffectBandState& band,
    const matrix& viewMatrix,
    CTexture* texture)
{
    if (!band.active || !texture || texture == &CTexMgr::s_dummy_texture || band.alpha <= 0.0f) {
        return;
    }

    const int renderFlags = ResolveEffectRenderFlags(prim.m_renderFlag, 1 | 2);
    const D3DBLEND destBlend = ResolveEffectDestBlend(prim.m_renderFlag);
    constexpr int kTexParts = 21;
    constexpr int kSegments = 10;
    constexpr int kFullDisplayAngle = 360;

    const vector3d center = prim.m_master ? prim.m_pos : base;
    const float groundY = ResolveGroundHeight(center) + prim.m_deltaPos2.y;
    const float riseRadians = band.riseAngle * (kPi / 180.0f);
    const float cosRise = std::cos(riseRadians);
    const float sinRise = std::sin(riseRadians);
    const float radius = (std::max)(0.0f, band.distance);
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

        const float tv0 = static_cast<float>(segmentIndex) / static_cast<float>(kTexParts);
        const float tv1 = static_cast<float>(segmentIndex + 1) / static_cast<float>(kTexParts);
        SubmitWorldTeiRect(base0, base1, top1, top0, viewMatrix, texture, color, tv0, tv1, destBlend, 0.0f, renderFlags);
    }
}

void UpdateHealBands(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    for (int bandIndex = 0; bandIndex < static_cast<int>(prim->m_bands.size()); ++bandIndex) {
        EffectBandState& band = prim->m_bands[static_cast<size_t>(bandIndex)];
        if (!band.active) {
            continue;
        }

        ++band.process;
        const u8 mode = band.modes[0];
        if (mode == 1) {
            band.rotStart = WrapAngle360(band.rotStart + static_cast<float>(bandIndex + 8));
        } else if (mode == 2) {
            band.rotStart = WrapAngle360(band.rotStart + static_cast<float>(bandIndex >= 2 ? bandIndex + 2 : bandIndex + 4));
        } else if (mode == 3) {
            band.rotStart = WrapAngle360(band.rotStart + static_cast<float>(bandIndex + 6));
        }

        if (band.fadeThreshold >= 0.0f && static_cast<float>(band.process) >= band.fadeThreshold) {
            band.alpha = (std::max)(0.0f, band.alpha - 2.0f);
        } else if (band.process < 16) {
            const float alphaStep = mode == 2 ? 3.0f : (mode == 3 ? 2.0f : 5.0f);
            band.alpha = (std::min)(180.0f, band.alpha + alphaStep);
        }

        for (int sampleIndex = 0; sampleIndex < static_cast<int>(band.heights.size()); ++sampleIndex) {
            float height = 0.0f;
            switch (mode) {
            case 0:
            case 33: {
                const int phaseMul = bandIndex == 0 ? 4 : (bandIndex == 1 ? 3 : 2);
                const float wave = SinDeg(static_cast<float>(sampleIndex * 34 + band.process * phaseMul));
                height = band.maxHeight * (0.75f + wave * 0.25f);
                if (band.process <= 90) {
                    height *= (std::max)(0.0f, SinDeg(static_cast<float>(band.process)));
                }
                break;
            }
            case 1:
            case 3:
                height = band.maxHeight;
                if (band.process <= 90) {
                    height *= (std::max)(0.0f, SinDeg(static_cast<float>(band.process)));
                }
                break;
            case 2:
                height = band.process > 45
                    ? 0.0f
                    : band.maxHeight * (std::max)(0.0f, SinDeg(static_cast<float>(band.process * 4)));
                break;
            default:
                break;
            }
            band.heights[static_cast<size_t>(sampleIndex)] = (std::max)(0.0f, height);
        }
    }
}

void UpdatePortalBands(CEffectPrim* prim)
{
    if (!prim) {
        return;
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

void UpdateCastingBands(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    if (prim->m_master) {
        prim->m_pos = prim->m_master->ResolveBasePosition();
    }

    for (EffectBandState& band : prim->m_bands) {
        if (!band.active) {
            continue;
        }

        ++band.process;
        band.distance -= 0.05f;
        if (band.distance <= 0.0f) {
            band.distance = 10.0f;
            band.alpha = 0.0f;
        }

        band.riseAngle = 90.0f - band.distance * 9.0f;
        if (band.process < prim->m_duration - 40) {
            band.alpha = (std::min)(70.0f, band.alpha + 1.0f);
        } else {
            band.alpha = (std::max)(0.0f, band.alpha - 3.0f);
        }
        band.heights.fill((std::max)(0.0f, band.distance));
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
    std::vector<unsigned long> pixels(static_cast<size_t>(kSize) * kSize, 0u);
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
    std::vector<unsigned long> pixels(static_cast<size_t>(kSize) * kSize, 0u);
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

bool TryPlayEffectWaveAt(const vector3d& soundPos, std::initializer_list<const char*> paths)
{
    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    for (const char* path : paths) {
        if (!path || !*path) {
            continue;
        }

        std::array<std::string, 3> candidates = {
            std::string(path),
            std::string("wav\\") + path,
            std::string("data\\wav\\") + path,
        };

        for (const std::string& candidate : candidates) {
            if (audio->PlaySound3D(candidate.c_str(), soundPos, listenerPos)) {
                return true;
            }
        }
    }

    return false;
}

std::string ResolveWorldStrName(const std::string& rawName)
{
    if (rawName.empty()) {
        return std::string();
    }

    std::string normalized = rawName;
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
                CDCBitmap bitmap(kCanvasSize, kCanvasSize);
                bitmap.ClearSurface(nullptr, 0u);

                HDC hdc = nullptr;
                if (bitmap.GetDC(&hdc) && hdc && DrawActMotionToHdc(hdc, kCanvasCenter, kCanvasCenter, sprRes, motion, sprRes->m_pal)) {
                    RECT bounds{};
                    if (FindOpaqueBounds(bitmap.GetImageData(), kCanvasSize, kCanvasSize, &bounds)) {
                        baked.width = (std::max)(1, static_cast<int>(bounds.right - bounds.left));
                        baked.height = (std::max)(1, static_cast<int>(bounds.bottom - bounds.top));
                        baked.pivotX = static_cast<float>(kCanvasCenter - bounds.left);
                        baked.pivotY = static_cast<float>(kCanvasCenter - bounds.top);

                        std::vector<unsigned int> pixels(static_cast<size_t>(baked.width) * static_cast<size_t>(baked.height), 0u);
                        for (int y = 0; y < baked.height; ++y) {
                            const unsigned int* src = bitmap.GetImageData() + static_cast<size_t>(bounds.top + y) * kCanvasSize + bounds.left;
                            unsigned int* dst = pixels.data() + static_cast<size_t>(y) * baked.width;
                            std::memcpy(dst, src, static_cast<size_t>(baked.width) * sizeof(unsigned int));
                        }
                        UnpremultiplyPixels(pixels);
                        baked.texture = g_texMgr.CreateTexture(baked.width, baked.height, reinterpret_cast<unsigned long*>(pixels.data()), PF_A8R8G8B8, false);
                        baked.isValid = baked.texture && baked.texture != &CTexMgr::s_dummy_texture;
                    }
                }
                bitmap.ReleaseDC(hdc);
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
    target->srcalpha = delta.srcalpha;
    target->destalpha = delta.destalpha;
    target->mtpreset = delta.mtpreset;
}

std::string ResolveStrName(int effectId)
{
    const RagEffectCatalogEntry* entry = FindRagEffectCatalogEntry(effectId);
    return entry ? ResolveCatalogStrName(*entry) : std::string();
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
        const float step = static_cast<float>(index);
        prim->m_segments[static_cast<size_t>(index)].alpha = prim->m_alpha
            - prim->m_alpha / static_cast<float>((std::max)(historyCount, 1)) * step;
        prim->m_segments[static_cast<size_t>(index)].size = prim->m_size
            - prim->m_size / static_cast<float>((std::max)(historyCount, 1)) * step * 0.5f;
    }

    prim->m_segments[0].pos = prim->m_pos;
    prim->m_segments[0].radius = prim->m_radius;
    prim->m_segments[0].size = prim->m_size;
    prim->m_segments[0].alpha = prim->m_alpha;
    prim->m_segments[0].transform = prim->m_matrix;
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

void AdvanceTextureMotion(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    const int textureCount = (std::max)(1, static_cast<int>(prim->m_texture.size()));
    if (textureCount <= 1) {
        prim->m_curMotion = 0;
        return;
    }

    const int animSpeed = (std::max)(1, prim->m_animSpeed);
    if ((prim->m_stateCnt % animSpeed) != 0) {
        return;
    }

    ++prim->m_curMotion;
    if (prim->m_repeatAnim) {
        prim->m_curMotion %= textureCount;
    } else if (prim->m_curMotion >= textureCount) {
        prim->m_curMotion = textureCount - 1;
    }
}

void UpdateAlphaFade(CEffectPrim* prim)
{
    if (!prim) {
        return;
    }

    prim->m_alpha = (std::min)(prim->m_maxAlpha, prim->m_alpha + prim->m_alphaSpeed);
    if (prim->m_fadeOutCnt > 0 && prim->m_stateCnt >= prim->m_fadeOutCnt) {
        const int fadeFrames = (std::max)(1, prim->m_duration - prim->m_fadeOutCnt + 1);
        const float fadeStep = prim->m_maxAlpha / static_cast<float>(fadeFrames);
        prim->m_alpha = (std::max)(0.0f, prim->m_alpha - fadeStep);
    }
}

bool UpdateCirclePrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    ++prim->m_stateCnt;
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
    AdvanceTextureMotion(prim);
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateCylinderPrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    ++prim->m_stateCnt;
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
    AdvanceTextureMotion(prim);
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateParticleOrbitPrimitive(CEffectPrim* prim)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    ++prim->m_stateCnt;
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
    AdvanceTextureMotion(prim);
    return prim->m_stateCnt <= prim->m_duration || prim->m_alpha > 0.0f;
}

bool UpdateFreeParticlePrimitive(CEffectPrim* prim, bool applyGravity, bool splineMotion)
{
    if (!prim || !prim->m_master) {
        return false;
    }

    ++prim->m_stateCnt;
    prim->m_orgPos = prim->m_master->ResolveBasePosition();
    prim->m_orgPos = AddVec3(prim->m_orgPos, prim->m_deltaPos2);

    prim->m_speed += prim->m_accel;
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
    AdvanceTextureMotion(prim);
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
    , m_size(1.0f)
    , m_sizeSpeed(0.0f)
    , m_sizeAccel(0.0f)
    , m_radius(0.0f)
    , m_radiusSpeed(0.0f)
    , m_radiusAccel(0.0f)
    , m_innerSize(0.0f)
    , m_outerSize(0.0f)
    , m_heightSize(0.0f)
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
    , m_outerSpeed(0.0f)
    , m_outerAccel(0.0f)
    , m_innerSpeed(0.0f)
    , m_innerAccel(0.0f)
    , m_tintColor(RGB(255, 255, 255))
    , m_curMotion(0)
    , m_animSpeed(4)
    , m_spriteAction(0)
    , m_spriteMotionBase(0)
    , m_spriteFrameDelay(0.0f)
    , m_spriteScale(1.0f)
    , m_spriteRepeat(false)
    , m_repeatAnim(true)
    , m_bandController(0)
    , m_numSegments(0)
{
    m_param.fill(0.0f);
    m_deltaPos = vector3d{};
    m_deltaPos2 = vector3d{};
    m_orgPos = vector3d{};
    m_pos = vector3d{};
    m_speed3d = vector3d{};
    MatrixIdentity(m_matrix);
    for (EffectSegmentState& segment : m_segments) {
        MatrixIdentity(segment.transform);
    }
}

void CEffectPrim::Init(CRagEffect* master, EFFECTPRIMID effectPrimId, const vector3d& deltaPos)
{
    m_master = master;
    m_type = effectPrimId;
    m_deltaPos = deltaPos;
}

bool CEffectPrim::OnProcess()
{
    if (m_type == PP_3DCIRCLE) {
        return UpdateCirclePrimitive(this);
    }
    if (m_type == PP_3DCYLINDER) {
        return UpdateCylinderPrimitive(this);
    }
    if (m_type == PP_3DPARTICLEORBIT) {
        return UpdateParticleOrbitPrimitive(this);
    }
    if (m_type == PP_3DPARTICLE) {
        return UpdateFreeParticlePrimitive(this, false, false);
    }
    if (m_type == PP_3DPARTICLEGRAVITY) {
        return UpdateFreeParticlePrimitive(this, true, false);
    }
    if (m_type == PP_3DPARTICLESPLINE) {
        return UpdateFreeParticlePrimitive(this, true, true);
    }
    if (m_type == PP_HEAL) {
        UpdateHealBands(this);
        ++m_stateCnt;
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active && band.alpha > 0.0f; });
    }
    if (m_type == PP_PORTAL) {
        UpdatePortalBands(this);
        ++m_stateCnt;
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active; });
    }
    if (m_type == PP_WIND) {
        UpdateWindBands(this);
        ++m_stateCnt;
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active && band.alpha > 0.0f; });
    }
    if (m_type == PP_CASTINGRING4 && std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active; })) {
        UpdateCastingBands(this);
        ++m_stateCnt;
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active && band.alpha > 0.0f; });
    }

    ++m_stateCnt;
    m_alpha = (std::min)(m_maxAlpha, m_alpha + m_alphaSpeed);
    m_size += m_sizeSpeed;
    m_sizeSpeed += m_sizeAccel;
    m_radius += m_radiusSpeed;
    m_radiusSpeed += m_radiusAccel;
    m_heightSize += m_heightSpeed;
    m_heightSpeed += m_heightAccel;
    m_longitude += m_longSpeed;
    m_gravSpeed += m_gravAccel;

    if (m_fadeOutCnt > 0 && m_stateCnt >= m_fadeOutCnt) {
        const int fadeFrames = (std::max)(1, m_duration - m_fadeOutCnt + 1);
        const float fadeStep = m_maxAlpha / static_cast<float>(fadeFrames);
        m_alpha = (std::max)(0.0f, m_alpha - fadeStep);
    }

    return m_stateCnt <= m_duration || m_alpha > 0.0f;
}

void CEffectPrim::Render(matrix* viewMatrix)
{
    if (!m_master || !viewMatrix) {
        return;
    }

    const vector3d base = AddVec3(m_master->ResolveBasePosition(), m_deltaPos);
    const float normalizedAlpha = (std::max)(0.0f, (std::min)(255.0f, m_alpha));
    if (normalizedAlpha <= 0.0f
        && m_type != PP_MAPMAGICZONE
        && m_type != PP_MAPPARTICLE
        && m_type != PP_PORTALSTACK
        && m_type != PP_CASTINGRING4
        && m_type != PP_HEAL
        && m_type != PP_PORTAL
        && m_type != PP_WIND) {
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
    case PP_HEAL: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(false);
        for (const EffectBandState& band : m_bands) {
            RenderBandRibbon(*this, base, band, *viewMatrix, texture, 1.0f);
        }
        break;
    }
    case PP_PORTAL: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(false);
        for (const EffectBandState& band : m_bands) {
            RenderBandRibbon(*this, base, band, *viewMatrix, texture, 0.35f);
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
    case PP_3DPARTICLEORBIT: {
        CTexture* texture = GetSoftDiscTexture();
        const vector3d pos = m_pos;
        const float size = (std::max)(0.6f, m_size);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitBillboard(pos,
            *viewMatrix,
            texture,
            size * 18.0f,
            size * 18.0f,
            PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    case PP_3DPARTICLE:
    case PP_3DPARTICLEGRAVITY:
    case PP_3DPARTICLESPLINE: {
        CTexture* texture = !m_texture.empty()
            ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)]
            : GetSoftDiscTexture();
        const vector3d pos = m_pos;
        const float size = (std::max)(0.8f, m_size);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitBillboard(pos,
            *viewMatrix,
            texture,
            size * 20.0f,
            size * 20.0f,
            PackColor(static_cast<unsigned int>(normalizedAlpha), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    case PP_3DTEXTURE:
    case PP_3DSPHERE:
    case PP_SLASH1: {
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
    case PP_3DCROSSTEXTURE:
    case PP_3DQUADHORN: {
        CTexture* texture = !m_texture.empty()
            ? m_texture[(std::min)(m_curMotion, static_cast<int>(m_texture.size()) - 1)]
            : GetSoftGlowTexture(false);
        const vector3d pos = AddVec3(base, m_deltaPos2);
        const float width = (std::max)(10.0f, m_size * 34.0f + m_outerSize * 8.0f);
        const float height = (std::max)(10.0f, m_heightSize > 0.0f ? m_heightSize * 12.0f : width * 1.6f);
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
        vector3d anchorPos = AddVec3(base, m_deltaPos2);
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
    case PP_SUPERANGEL: {
        tlvertex3d anchor{};
        vector3d anchorPos = base;
        anchorPos.y += m_param[2];
        if (!ProjectPoint(anchorPos, *viewMatrix, &anchor)) {
            return;
        }
        const float pixelRatio = ResolveEffectPixelRatio(anchor);
        if (!std::isfinite(pixelRatio) || pixelRatio <= 0.0f) {
            return;
        }

        const int variant = static_cast<int>(m_param[0]);
        const int birthFrame = static_cast<int>(m_param[1]);
        const int process = m_stateCnt;
        float visualAlpha = normalizedAlpha;
        switch (variant) {
        case 1: {
            const float maxAlpha = static_cast<float>((std::max)(0, 40 - birthFrame));
            visualAlpha = (std::min)(maxAlpha, static_cast<float>(process));
            break;
        }
        case 2:
        case 3:
        case 4:
            visualAlpha = process < 30 ? (std::min)(150.0f, static_cast<float>(process) * 5.0f) : normalizedAlpha;
            break;
        case 10:
            visualAlpha = process < 50 ? (std::min)(255.0f, static_cast<float>(process) * 10.0f) : normalizedAlpha;
            break;
        case 11:
            visualAlpha = process < 50 ? (std::min)(180.0f, static_cast<float>(process) * 3.0f) : normalizedAlpha;
            break;
        case 0:
        default:
            visualAlpha = process < 50 ? (std::min)(245.0f, static_cast<float>(process) * 5.0f) : normalizedAlpha;
            break;
        }
        const float drift = -22.0f - static_cast<float>(process) * (0.9f + m_param[3] * 0.08f);
        const float sway = std::sin(timeSeconds * (1.8f + m_param[3] * 0.2f) + m_param[0] * 0.8f) * (18.0f + static_cast<float>((std::min)(process, 30)) * 0.35f);
        CTexture* wingTexture = m_texture.size() > 0 ? m_texture[0] : GetSoftGlowTexture(false);
        CTexture* ringTexture = m_texture.size() > 1 ? m_texture[1] : GetSoftGlowTexture(false);
        CTexture* accentTexture = m_texture.size() > 2 ? m_texture[2] : GetSoftGlowTexture(true);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitScreenQuad(anchor,
            wingTexture,
            sway - 26.0f,
            drift,
            (72.0f + m_size * 18.0f) * pixelRatio,
            (128.0f + m_size * 22.0f) * pixelRatio,
            PackColor(static_cast<unsigned int>((std::max)(0.0f, (std::min)(255.0f, visualAlpha))), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        SubmitScreenQuad(anchor,
            wingTexture,
            -sway + 26.0f,
            drift,
            (72.0f + m_size * 18.0f) * pixelRatio,
            (128.0f + m_size * 22.0f) * pixelRatio,
            PackColor(static_cast<unsigned int>((std::max)(0.0f, (std::min)(255.0f, visualAlpha))), m_tintColor),
            destBlend,
            0.0f,
            renderFlags);
        SubmitScreenQuad(anchor,
            ringTexture,
            0.0f,
            20.0f + std::sin(timeSeconds * 4.0f + m_param[0]) * 6.0f,
            (78.0f + m_size * 18.0f) * pixelRatio,
            (78.0f + m_size * 18.0f) * pixelRatio,
            PackColor(static_cast<unsigned int>(visualAlpha * 0.8f), RGB(220, 240, 255)),
            destBlend,
            0.0f,
            renderFlags);
        SubmitScreenQuad(anchor,
            accentTexture,
            std::sin(timeSeconds * 3.0f + m_param[0]) * 18.0f,
            8.0f - timeSeconds * 20.0f,
            (40.0f + m_size * 12.0f) * pixelRatio,
            (40.0f + m_size * 12.0f) * pixelRatio,
            PackColor(static_cast<unsigned int>(visualAlpha * 0.7f), RGB(255, 244, 196)),
            destBlend,
            0.0f,
            renderFlags);
        break;
    }
    case PP_RADIALSLASH: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(false);
        const int particles = (std::max)(4, m_spawnCount);
        const float progress = m_duration > 0
            ? (std::min)(1.0f, static_cast<float>(m_stateCnt) / static_cast<float>(m_duration))
            : 1.0f;
        const float radialBase = m_radius + progress * (m_radiusSpeed * static_cast<float>(m_duration) + m_param[1]);
        const float heightBase = m_heightSize + progress * m_param[2];
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        for (int index = 0; index < particles; ++index) {
            const float step = 360.0f / static_cast<float>(particles);
            const float angleDeg = m_param[0] + step * static_cast<float>(index);
            const float angle = angleDeg * (kPi / 180.0f);
            vector3d pos = {
                base.x + std::cos(angle) * radialBase,
                base.y - heightBase - progress * 8.0f,
                base.z + std::sin(angle) * radialBase
            };
            const float scale = 1.0f - progress * 0.35f;
            SubmitBillboard(pos,
                *viewMatrix,
                texture,
                (32.0f + m_size * 18.0f) * scale,
                (88.0f + m_size * 28.0f) * scale,
                PackColor(static_cast<unsigned int>(normalizedAlpha * (1.0f - progress * 0.45f)), m_tintColor),
                destBlend,
                0.0f,
                renderFlags);
        }
        break;
    }
    case PP_PORTALSTACK: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(false);
        const int rings = (std::max)(3, m_spawnCount);
        const float baseRadius = (std::max)(1.0f, m_outerSize);
        const float riseSpeed = m_param[2] != 0.0f ? m_param[2] : 0.5f;
        const int delayStep = m_param[1] != 0.0f ? static_cast<int>(m_param[1]) : 10;
        const int cycleTicks = 28;
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        for (int ringIndex = 0; ringIndex < rings; ++ringIndex) {
            int localTick = m_stateCnt - ringIndex * delayStep;
            if (localTick < 0) {
                continue;
            }
            localTick %= cycleTicks;
            const float grow = (std::min)(7.0f, static_cast<float>(localTick) * riseSpeed);
            const float alpha = localTick < 10
                ? (std::min)(240.0f, static_cast<float>(localTick) * 24.0f)
                : (std::max)(0.0f, 240.0f - (grow > 7.0f ? (grow - 7.0f) * 60.0f : 0.0f));
            if (alpha <= 0.0f) {
                continue;
            }
            const float width = (baseRadius + grow) * 34.0f;
            const float height = (baseRadius + grow * 0.55f) * 26.0f;
            const float yOffset = -grow * 8.0f;
            const vector3d pos = { base.x, base.y + yOffset, base.z };
            SubmitBillboard(pos,
                *viewMatrix,
                texture,
                width,
                height,
                PackColor(static_cast<unsigned int>(alpha), m_tintColor),
                destBlend,
                0.0f,
                renderFlags);
        }
        break;
    }
    case PP_CASTINGRING4: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(true);
        const bool hasBands = std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active; });
        if (hasBands) {
            for (const EffectBandState& band : m_bands) {
                RenderCastingBandLowPolygon(*this, base, band, *viewMatrix, texture);
            }
            break;
        }
        const int arms = 4;
        const float distanceStart = m_outerSize > 0.0f ? m_outerSize : 10.0f;
        const int localTick = m_duration > 0 ? m_stateCnt % m_duration : m_stateCnt;
        const float distance = (std::max)(0.0f, distanceStart - static_cast<float>(localTick) * 0.05f);
        const float alpha = localTick < m_duration - 40
            ? (std::min)(70.0f, static_cast<float>(localTick))
            : (std::max)(0.0f, 70.0f - static_cast<float>(localTick - (m_duration - 40)) * 3.0f);
        if (alpha <= 0.0f) {
            break;
        }
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        for (int arm = 0; arm < arms; ++arm) {
            const float baseAngle = m_param[0] + static_cast<float>(arm) * (kPi * 0.5f);
            const float angle = baseAngle + timeSeconds * (0.8f + m_param[1] * 0.05f);
            const float radius = (std::max)(1.0f, distance - static_cast<float>(arm) * 2.5f);
            const vector3d pos = {
                base.x + std::cos(angle) * radius,
                base.y - 2.0f - static_cast<float>(arm) * 0.6f,
                base.z + std::sin(angle) * radius
            };
            const float size = (std::max)(0.8f, radius) * 8.0f;
            SubmitBillboard(pos,
                *viewMatrix,
                texture,
                size,
                size,
                PackColor(static_cast<unsigned int>(alpha), m_tintColor),
                destBlend,
                0.0f,
                renderFlags);
        }
        break;
    }
    case PP_EFFECTSPRITE: {
        if (m_spriteActName.empty() || m_spriteSprName.empty()) {
            break;
        }

        CActRes* actRes = g_resMgr.GetAs<CActRes>(m_spriteActName.c_str());
        if (!actRes) {
            break;
        }

        int actionIndex = m_spriteAction;
        if (actRes->GetMotionCount(actionIndex) <= 0) {
            actionIndex = 0;
        }
        const int motionCount = actRes->GetMotionCount(actionIndex);
        if (motionCount <= 0) {
            break;
        }

        const float frameDelay = m_spriteFrameDelay > 0.0f
            ? m_spriteFrameDelay
            : (std::max)(1.0f, actRes->GetDelay(actionIndex));
        const int motionAdvance = static_cast<int>(static_cast<float>(m_stateCnt) / frameDelay);
        const int clampedBase = (std::max)(0, (std::min)(m_spriteMotionBase, motionCount - 1));
        const int motionIndex = m_spriteRepeat
            ? (clampedBase + motionAdvance) % motionCount
            : (std::min)(motionCount - 1, clampedBase + motionAdvance);

        const BakedEffectSpriteFrame* frame = GetBakedEffectSpriteFrame(m_spriteActName, m_spriteSprName, actionIndex, motionIndex);
        if (!frame || !frame->texture) {
            break;
        }

        vector3d anchorPos = base;
        anchorPos.y += m_param[2];
        tlvertex3d anchor{};
        if (!ProjectPoint(anchorPos, *viewMatrix, &anchor)) {
            break;
        }

        const float pixelRatio = ResolveEffectPixelRatio(anchor);
        const float scale = (std::max)(0.1f, m_spriteScale);
        if (!std::isfinite(pixelRatio) || pixelRatio <= 0.0f) {
            break;
        }

        const float width = static_cast<float>(frame->width) * pixelRatio * scale;
        const float height = static_cast<float>(frame->height) * pixelRatio * scale;
        const float pivotX = frame->pivotX * pixelRatio * scale;
        const float pivotY = frame->pivotY * pixelRatio * scale;
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        SubmitScreenQuadPivot(anchor,
            frame->texture,
            pivotX,
            pivotY,
            width,
            height,
            color,
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
    , m_handler(Handler::None)
    , m_type(0)
    , m_level(1)
    , m_flag(0)
    , m_count(0)
    , m_stateCnt(0)
    , m_duration(0)
    , m_cEndLayer(0)
    , m_iCurLayer(0)
    , m_worldType(0)
    , m_loop(false)
    , m_lastProcessTick(timeGetTime())
    , m_tickCarryMs(kEffectTickMs)
    , m_cachedPos{}
    , m_deltaPos{}
    , m_param{ 0.0f, 0.0f, 0.0f, 0.0f }
    , m_emitSpeed(1.0f)
    , m_longitude(0.0f)
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

    if (CAbleToMakeEffect* owner = dynamic_cast<CAbleToMakeEffect*>(m_master)) {
        owner->m_effectList.remove(this);
        if (owner->m_beginSpellEffect == this) {
            owner->m_beginSpellEffect = nullptr;
        }
        if (owner->m_magicTargetEffect == this) {
            owner->m_magicTargetEffect = nullptr;
        }
    }

    if (m_ownsMaster) {
        delete m_master;
        m_master = nullptr;
    }
}

void CRagEffect::ClearPrims()
{
    for (CEffectPrim* prim : m_primList) {
        delete prim;
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

void CRagEffect::InitEZ2STRFrame()
{
    m_iCurLayer = 0;
    m_stateCnt = 0;
    m_cEndLayer = m_aniClips ? m_aniClips->cLayer : 0;
    if (m_aniClips && m_aniClips->cLayer > 0 && m_aniClips->aLayer[0].cAniKey == 0) {
        m_cEndLayer = m_aniClips->cLayer - 1;
    }
    m_actXformData.fill(KAC_XFORMDATA{});
    m_aiCurAniKey.fill(0);
    m_isLayerDrawn.fill(0);
    m_activeAniKeyFrame.fill(-1);
    m_activeAniKeyAppliedState.fill(-1);
    m_activeAniKeyZeroBlend.fill(0);
}

bool CRagEffect::ProcessEZ2STR()
{
    if (!m_aniClips) {
        return false;
    }

    m_isLayerDrawn.fill(0);
    m_activeAniKeyFrame.fill(-1);
    m_activeAniKeyAppliedState.fill(-1);
    m_activeAniKeyZeroBlend.fill(0);
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
            curKeyIndex = layer.cAniKey - 1;
        }

        const KAC_KEYFRAME* key = &layer.aAniKey[static_cast<size_t>(curKeyIndex)];
        int frameOffset = m_stateCnt - key->iFrame;
        if (frameOffset >= 0 && curKeyIndex < layer.cAniKey - 1) {
            const KAC_KEYFRAME& nextKey = layer.aAniKey[static_cast<size_t>(curKeyIndex + 1)];
            if (frameOffset > 0 && nextKey.iFrame == m_stateCnt) {
                ++curKeyIndex;
                key = &layer.aAniKey[static_cast<size_t>(curKeyIndex)];
                frameOffset = m_stateCnt - key->iFrame;
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
            case 2:
                xform.aniframe += key->XformData.anidelta;
                if (xform.aniframe >= static_cast<float>(layer.cTex)) {
                    xform.aniframe = static_cast<float>((std::max)(layer.cTex - 1, 0));
                }
                break;
            case 3: {
                xform.aniframe += key->XformData.anidelta;
                const float texCount = static_cast<float>((std::max)(layer.cTex, 1));
                if (xform.aniframe >= texCount) {
                    xform.aniframe = std::fmod(xform.aniframe, texCount);
                }
                break;
            }
            case 4: {
                xform.aniframe -= key->XformData.anidelta;
                const float texCount = static_cast<float>((std::max)(layer.cTex, 1));
                while (xform.aniframe < 0.0f) {
                    xform.aniframe += texCount;
                }
                break;
            }
            case 5: {
                const int span = (std::max)(layer.cTex - 1, 1);
                const int motion = static_cast<int>(static_cast<float>(frameOffset) * key->XformData.anidelta + xform.aniframe);
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
        m_activeAniKeyFrame[static_cast<size_t>(layerIndex)] = key->iFrame;
        m_activeAniKeyAppliedState[static_cast<size_t>(layerIndex)] = m_stateCnt;
        m_activeAniKeyZeroBlend[static_cast<size_t>(layerIndex)] =
            (key->XformData.srcalpha == 0u && key->XformData.destalpha == 0u) ? 1u : 0u;
    }

    return true;
}

void CRagEffect::RenderAniClip(int layerIndex, const KAC_LAYER& layer, const KAC_XFORMDATA& xform, matrix* viewMatrix)
{
    if (!viewMatrix) {
        return;
    }

    if (layer.cTex <= 0) {
        return;
    }

    if (layerIndex >= 0 && layerIndex < static_cast<int>(m_activeAniKeyZeroBlend.size())
        && m_activeAniKeyZeroBlend[static_cast<size_t>(layerIndex)] != 0) {
        const int keyFrame = m_activeAniKeyFrame[static_cast<size_t>(layerIndex)];
        const int appliedState = m_activeAniKeyAppliedState[static_cast<size_t>(layerIndex)];
        if (keyFrame >= 0 && appliedState >= 0) {
            const int keyAgeTicks = appliedState - keyFrame;
            if (keyAgeTicks <= 1) {
                return;
            }
        }
    }

    const int aniFrame = (std::max)(0, (std::min)(layer.cTex - 1, static_cast<int>(xform.aniframe)));
    CTexture* texture = const_cast<KAC_LAYER&>(layer).GetTexture(aniFrame);
    if (!texture || texture == &CTexMgr::s_dummy_texture) {
        return;
    }

    tlvertex3d anchor{};
    if (!ProjectPoint(ResolveBasePosition(), *viewMatrix, &anchor)) {
        return;
    }
    const float pixelRatio = ResolveEffectPixelRatio(anchor);
    if (!std::isfinite(pixelRatio) || pixelRatio <= 0.0f) {
        return;
    }

    m_tlvertX = anchor.x;
    m_tlvertY = anchor.y - 80.0f;
    m_tlvertZ = anchor.z;

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
    face->mtPreset = static_cast<int>(xform.mtpreset);
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = ResolveAniClipBlendMode(xform.srcalpha);
    face->destAlphaMode = ResolveAniClipBlendMode(xform.destalpha);
    face->alphaSortKey = 0.0f;

    const float uScale = texture->m_w > 0 ? static_cast<float>(texture->m_updateWidth) / static_cast<float>(texture->m_w) : 1.0f;
    const float vScale = texture->m_h > 0 ? static_cast<float>(texture->m_updateHeight) / static_cast<float>(texture->m_h) : 1.0f;
    const float angle = xform.rz * (kPi / 512.0f);
    const float s = std::sin(angle);
    const float c = std::cos(angle);

    float xs[4] = { xform.ax[3], xform.ax[0], xform.ax[2], xform.ax[1] };
    float ys[4] = { xform.ay[3], xform.ay[0], xform.ay[2], xform.ay[1] };
    float us[4] = { xform.u, xform.u, xform.u + xform.us, xform.u + xform.us };
    float vs[4] = { xform.v + xform.vs, xform.v, xform.v + xform.vs, xform.v };

    const auto clampColor = [](float value) -> unsigned int {
        return static_cast<unsigned int>((std::max)(0.0f, (std::min)(255.0f, value)));
    };
    const unsigned int color = (clampColor(xform.crA) << 24)
        | (clampColor(xform.crR) << 16)
        | (clampColor(xform.crG) << 8)
        | clampColor(xform.crB);

    for (int index = 0; index < 4; ++index) {
        const float rotatedX = (xs[index] * c - ys[index] * s + xform.x - 320.0f) * pixelRatio;
        const float rotatedY = (ys[index] * c + xs[index] * s + xform.y - 240.0f) * pixelRatio;
        tlvertex3d& vert = face->m_verts[index];
        vert = anchor;
        vert.x = rotatedX + m_tlvertX;
        vert.y = rotatedY + m_tlvertY;
        vert.z = 0.000031f;
        vert.color = color;
        vert.specular = 0xFFFFFFFFu;
        vert.tu = us[index] * uScale;
        vert.tv = vs[index] * vScale;
    }

    g_renderer.AddRP(face, 1 | 4);
}

void CRagEffect::Init(CRenderObject* master, int effectId, const vector3d& deltaPos)
{
    m_master = master;
    m_ownsMaster = false;
    m_type = effectId;
    m_deltaPos = deltaPos;
    m_cachedPos = master ? master->m_pos : vector3d{};
    m_longitude = master ? master->m_roty : 0.0f;
    m_handler = Handler::None;
    m_loop = false;
    m_duration = 100;
    m_stateCnt = 0;
    m_lastProcessTick = timeGetTime();
    m_tickCarryMs = kEffectTickMs;

    switch (effectId) {
    case 158:
        m_handler = Handler::Entry2;
        m_duration = 72;
        break;
    case 337:
        m_handler = Handler::JobLevelUp50;
        m_duration = 108;
        LoadEzEffect("joblvup.str");
        InitEZ2STRFrame();
        if (m_aniClips && m_aniClips->cFrame > 0) {
            m_duration = (std::max)(m_duration, m_aniClips->cFrame + 36);
        }
        break;
    case 371: {
        m_handler = Handler::EzStr;
        m_duration = 120;
        const std::string strName = ResolveStrName(effectId);
        LoadEzEffect(strName.c_str());
        InitEZ2STRFrame();
        if (m_aniClips && m_aniClips->cFrame > 0) {
            m_duration = m_aniClips->cFrame + 8;
        }
        break;
    }
    case 338:
    case 582:
        m_handler = Handler::SuperAngel;
        m_duration = effectId == 582 ? 124 : 112;
        break;
    case 316:
        m_handler = Handler::ReadyPortal2;
        m_duration = 100;
        break;
    case 317:
        m_handler = Handler::Portal2;
        m_duration = 100;
        m_param[0] = 0.0f;
        break;
    case 321:
        m_handler = Handler::WarpZone2;
        m_loop = true;
        m_duration = 1000000;
        break;
    case 341:
        m_handler = Handler::Portal2;
        m_duration = 100;
        m_param[0] = 3.0f;
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
            if (m_aniClips && m_aniClips->cFrame > 0) {
                m_duration = (std::max)(m_duration, m_aniClips->cFrame + 8);
            }
        }
        break;
    }
    }
}

void CRagEffect::InitWorld(const C3dWorldRes::effectSrcInfo& source)
{
    m_master = new CWorldAnchor(source.pos);
    m_ownsMaster = true;
    m_cachedPos = source.pos;
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
    } else if (ContainsIgnoreCaseAscii(source.name, "warp") || ContainsIgnoreCaseAscii(source.name, "portal")) {
        m_type = kRagEffectWarpZone;
        m_handler = Handler::WarpZone;
    } else if (source.type == 44 || std::fabs(source.param[0]) > 20.0f) {
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
    return prim;
}

void CRagEffect::DetachFromMaster()
{
    if (m_master) {
        m_cachedPos = m_master->m_pos;
    }
    if (!m_ownsMaster) {
        m_master = nullptr;
    }
}

vector3d CRagEffect::ResolveBasePosition() const
{
    if (m_master) {
        return AddVec3(m_master->m_pos, m_deltaPos);
    }
    return AddVec3(m_cachedPos, m_deltaPos);
}

float CRagEffect::ResolveBaseRotation() const
{
    if (m_master) {
        return m_master->m_roty;
    }
    return m_longitude;
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

void CRagEffect::SpawnPortal2()
{
    CTexture* magicViolet = ResolveEffectTextureCandidates({
        "effect\\magic_violet.tga",
        "effect\\magic_violet.bmp",
        "effect\\magic_vio.tga",
        "effect\\magic_vio.bmp",
    }, false);
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);
    CTexture* cloud11 = ResolveEffectTextureCandidates({
        "effect\\cloud11.tga",
        "effect\\cloud11.bmp",
    }, true);

    if (m_stateCnt == 0) {
        const vector3d base = ResolveBasePosition();
        DbgLog("[RagEffect] spawn portal base=(%.2f,%.2f,%.2f) ringBlue=%p\n",
            base.x,
            base.y,
            base.z,
            ringBlue);
    }

    if (m_stateCnt == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_HEAL, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(magicViolet ? magicViolet : ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = 4.0f;
            ConfigureBand(prim, 0, 0, 50.0f, static_cast<float>(rand() % 360), 4.0f, 90.0f, 1400.0f, 2);
            ConfigureBand(prim, 1, 0, 50.0f, static_cast<float>(rand() % 360), 3.0f, 90.0f, 1400.0f, 2);
            ConfigureBand(prim, 2, 0, 50.0f, static_cast<float>(rand() % 360), 2.0f, 90.0f, 1400.0f, 2);
        }
    }

    if (m_stateCnt == 1) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_PORTAL, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = 4.0f;
            prim->m_param[0] = 0.0f;
            ConfigureBand(prim, 0, 0, 6.0009999f, 0.0f, 0.0f, 2.0f, 3111.0f, 0);
            ConfigureBand(prim, 1, -10, 6.0009999f, 25.0f, 0.0f, 3.0f, 3111.0f, 0);
            ConfigureBand(prim, 2, -20, 6.0009999f, 50.0f, 0.0f, 4.0f, 3111.0f, 0);
        }
    }

    if (m_stateCnt == 2) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_WIND, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(cloud11 ? cloud11 : ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = 4.0f;
            ConfigureBand(prim, 0, 0, 1.5f, 0.0f, 9.0f, 90.0f, 0.0f, 0);
            ConfigureBand(prim, 1, 0, 3.5f, 90.0f, 7.5f, 90.0f, 0.0f, 0);
            ConfigureBand(prim, 2, 0, 5.5f, 180.0f, 6.0f, 90.0f, 0.0f, 0);
            ConfigureBand(prim, 3, 0, 7.5f, 270.0f, 4.5f, 90.0f, 0.0f, 0);
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
        TryPlayEffectWaveAt(ResolveBasePosition(), {
            "effect\\ef_readyportal.wav",
            "effect\\ef_readyp.wav",
        });
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

void CRagEffect::SpawnReadyPortal2()
{
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);

    if ((m_stateCnt % 14) == 0) {
        TryPlayEffectWaveAt(ResolveBasePosition(), {
            "effect\\ef_readyportal.wav",
            "effect\\ef_readyp.wav",
        });
    }

    if (m_stateCnt == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_PORTAL, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = 5.0f;
            prim->m_param[0] = 1.0f;
            ConfigureBand(prim, 0, 0, 6.0009999f, 0.0f, 0.0f, 2.0f, 100.0f, 0);
            ConfigureBand(prim, 1, -10, 6.0009999f, 25.0f, 0.0f, 3.0f, 100.0f, 0);
            ConfigureBand(prim, 2, -20, 6.0009999f, 50.0f, 0.0f, 4.0f, 100.0f, 0);
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
    CTexture* magicBlue = ResolveEffectTextureCandidates({
        "effect\\magic_blue.tga",
        "effect\\magic_sky.tga",
    }, true);

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
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(86, 196, 255);
        }
    }

    if ((m_stateCnt % 10) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{})) {
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
            prim->m_texture.push_back(magicBlue);
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

void CRagEffect::SpawnWarpZone2()
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
    CTexture* magicBlue = ResolveEffectTextureCandidates({
        "effect\\magic_blue.tga",
        "effect\\magic_sky.tga",
    }, true);

    if (m_stateCnt == 0 && alphaDown) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = 99999999;
            prim->m_radius = 15.0f;
            prim->m_innerSize = 11.0f;
            prim->m_alpha = 0.0f;
            prim->m_alphaSpeed = 12.8f;
            prim->m_maxAlpha = 128.0f;
            prim->m_deltaPos2.y = -1.5f;
            prim->m_latitude = 90.0f;
            prim->m_pattern |= 1;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(255, 255, 255);
        }
    }

    if (m_stateCnt == 1 || m_stateCnt == 2) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_CASTINGRING4, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = m_stateCnt == 1 ? 11.0f : 4.0f;
            ConfigureBand(prim, 0, 0, m_stateCnt == 1 ? 2.5f : 2.7f, m_stateCnt == 1 ? 270.0f : 271.0f, m_stateCnt == 1 ? 2.5f : 2.7f, m_stateCnt == 1 ? 53.0f : 52.0f, -1.0f, 0);
            ConfigureBand(prim, 1, 0, m_stateCnt == 1 ? 5.0f : 5.2f, m_stateCnt == 1 ? 0.0f : 1.0f, m_stateCnt == 1 ? 5.0f : 5.2f, m_stateCnt == 1 ? 60.0f : 59.0f, -1.0f, 0);
            ConfigureBand(prim, 2, 0, m_stateCnt == 1 ? 7.5f : 7.7f, m_stateCnt == 1 ? 90.0f : 91.0f, m_stateCnt == 1 ? 7.5f : 7.7f, m_stateCnt == 1 ? 55.0f : 54.0f, -1.0f, 0);
            ConfigureBand(prim, 3, 0, 10.0f, m_stateCnt == 1 ? 180.0f : 181.0f, 10.0f, m_stateCnt == 1 ? 50.0f : 49.0f, -1.0f, 0);
        }
    }

    if ((m_stateCnt % 10) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{})) {
            prim->m_duration = 70;
            prim->m_radius = 4.5f + static_cast<float>((rand() % 200) / 100) * 0.75f;
            prim->m_radiusSpeed = 0.0005f;
            prim->m_longitude = static_cast<float>(rand() % 360);
            const float speedMagnitude = 2.5f - static_cast<float>(rand() % 11) * 0.1f;
            prim->m_longSpeed = ((rand() & 1) == 0) ? speedMagnitude : -speedMagnitude;
            prim->m_gravSpeed = -0.3f;
            prim->m_gravAccel = (-prim->m_gravSpeed / static_cast<float>(prim->m_duration)) * 0.66666669f;
            prim->m_heightSize = 4.0f;
            prim->m_alpha = 96.0f;
            prim->m_maxAlpha = 96.0f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
            prim->m_size = static_cast<float>(rand() % 10) * 0.1f + 0.7f;
            prim->m_texture.push_back(magicBlue);
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

    if (m_stateCnt >= 99999998) {
        m_stateCnt = 0;
    }
}

void CRagEffect::SpawnEntry2()
{
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(ResolveBasePosition(), {
            "effect\\ef_portal.wav",
        });

        if (CEffectPrim* prim = LaunchEffectPrim(PP_HEAL, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(96, 196, 255);
            prim->m_size = 4.0f;
            ConfigureBand(prim, 0, 0, 30.0f, static_cast<float>(rand() % 360), 3.7f, 90.0f, 1400.0f, 2);
            ConfigureBand(prim, 1, 0, 30.0f, static_cast<float>(rand() % 360), 3.4f, 90.0f, 1400.0f, 2);
            ConfigureBand(prim, 2, 0, 4.0f, static_cast<float>(rand() % 360), 3.6f, 10.0f, 1400.0f, 2);
            ConfigureBand(prim, 3, 0, 4.0f, static_cast<float>(rand() % 360), 3.7f, 5.0f, 1400.0f, 2);
        }
    }
}

void CRagEffect::SpawnJobLevelUp50()
{
    ProcessEZ2STR();

    if (m_stateCnt != 5) {
        return;
    }

    CTexture* magicGreen = ResolveEffectTextureCandidates({
        "effect\\magic_green.tga",
        "effect\\magic_green.bmp",
        "effect\\magic_ring_a.bmp",
    }, false);

    auto spawnBurst = [&](float startAngle) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_RADIALSLASH, vector3d{})) {
            prim->m_duration = 42;
            prim->m_spawnCount = 4;
            prim->m_radius = 0.8f;
            prim->m_radiusSpeed = 0.22f;
            prim->m_heightSize = 2.0f;
            prim->m_alpha = 0.0f;
            prim->m_alphaSpeed = 22.0f;
            prim->m_maxAlpha = 220.0f;
            prim->m_fadeOutCnt = 26;
            prim->m_size = 1.0f;
            prim->m_texture.push_back(magicGreen);
            prim->m_tintColor = RGB(172, 255, 188);
            prim->m_param[0] = startAngle;
            prim->m_param[1] = 7.0f;
            prim->m_param[2] = 12.0f;
        }
    };

    spawnBurst(0.0f);
    spawnBurst(45.0f);
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

void CRagEffect::SpawnSuperAngelVariant(int variant, int birthFrame)
{
    if (CEffectPrim* prim = LaunchEffectPrim(PP_EFFECTSPRITE, vector3d{})) {
        const bool taekwon = variant >= 10;
        const std::initializer_list<const char*> spriteStems = taekwon
            ? std::initializer_list<const char*>{ "fake_angel", "al_angelus" }
            : std::initializer_list<const char*>{ "fake_angel", "al_angelus" };
        prim->m_duration = m_duration;
        prim->m_alpha = 0.0f;
        prim->m_alphaSpeed = taekwon ? 10.0f : 8.0f;
        prim->m_maxAlpha = taekwon ? 255.0f : 220.0f;
        prim->m_fadeOutCnt = taekwon ? 92 : 84;
        prim->m_size = taekwon ? 1.15f : 1.0f;
        prim->m_tintColor = taekwon ? RGB(148, 255, 196) : RGB(255, 244, 188);
        prim->m_param[0] = static_cast<float>(variant);
        prim->m_param[1] = static_cast<float>(birthFrame);
        prim->m_param[2] = taekwon ? -26.0f : -34.0f;
        prim->m_param[3] = taekwon ? 1.6f : 1.0f;
        if (!ConfigureEffectSpritePrim(prim, spriteStems, 0, taekwon ? 1.15f : 1.0f, false, 0.0f, 0)) {
            m_primList.pop_back();
            delete prim;
            prim = LaunchEffectPrim(PP_SUPERANGEL, vector3d{});
            if (!prim) {
                return;
            }
            prim->m_duration = m_duration;
            prim->m_alpha = 0.0f;
            prim->m_alphaSpeed = taekwon ? 10.0f : 8.0f;
            prim->m_maxAlpha = taekwon ? 255.0f : 220.0f;
            prim->m_fadeOutCnt = taekwon ? 92 : 84;
            prim->m_size = taekwon ? 1.15f : 1.0f;
            prim->m_tintColor = taekwon ? RGB(148, 255, 196) : RGB(255, 244, 188);
            prim->m_texture.push_back(ResolveEffectTextureCandidates({
                "effect\\priest_angel_wing.bmp",
                "effect\\magnificat_angel_wing.bmp",
                "effect\\sanctuaria_angel_wing_up.bmp",
            }, false));
            prim->m_texture.push_back(ResolveEffectTextureCandidates({
                "effect\\priest_angel_ring.bmp",
                "effect\\ring_blue.tga",
                "effect\\ring_white.tga",
            }, false));
            prim->m_texture.push_back(ResolveEffectTextureCandidates({
                "effect\\priest_angel_hand.bmp",
                "effect\\lexaeterna_angel.bmp",
                "effect\\priest_angel_hair.bmp",
            }, true));
            prim->m_param[0] = static_cast<float>(variant);
            prim->m_param[1] = static_cast<float>(birthFrame);
            prim->m_param[2] = taekwon ? -30.0f : -40.0f;
            prim->m_param[3] = taekwon ? 1.6f : 1.0f;
        }
    }
}

void CRagEffect::SpawnSuperAngelBurst(int startAngle)
{
    if (CEffectPrim* prim = LaunchEffectPrim(PP_RADIALSLASH, vector3d{})) {
        prim->m_duration = 40;
        prim->m_spawnCount = 4;
        prim->m_radius = 0.9f;
        prim->m_radiusSpeed = 0.2f;
        prim->m_heightSize = 1.5f;
        prim->m_alpha = 0.0f;
        prim->m_alphaSpeed = 24.0f;
        prim->m_maxAlpha = 220.0f;
        prim->m_fadeOutCnt = 24;
        prim->m_size = 0.85f;
        prim->m_texture.push_back(ResolveEffectTextureCandidates({
            "effect\\ring_blue.tga",
            "effect\\ring_blue.bmp",
            "effect\\ring_b.bmp",
        }, false));
        prim->m_tintColor = RGB(180, 220, 255);
        prim->m_param[0] = static_cast<float>(startAngle);
        prim->m_param[1] = 6.0f;
        prim->m_param[2] = 10.0f;
    }
}

void CRagEffect::UpdateSuperAngel()
{
    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(ResolveBasePosition(), {
            "effect\\ef_angel.wav",
            "effect\\levelup.wav",
        });
        if (m_type == 338) {
            SpawnSuperAngelVariant(0, m_stateCnt);
            SpawnSuperAngelVariant(1, m_stateCnt);
            SpawnSuperAngelVariant(2, m_stateCnt);
            SpawnSuperAngelVariant(3, m_stateCnt);
        } else {
            SpawnSuperAngelVariant(11, m_stateCnt);
            SpawnSuperAngelVariant(10, m_stateCnt);
        }
    }

    if ((m_type == 338 && m_stateCnt <= 18) || (m_type == 582 && m_stateCnt <= 18)) {
        if ((m_stateCnt % 3) == 0) {
            if (m_type == 338) {
                SpawnSuperAngelVariant(1, m_stateCnt);
            } else {
                SpawnSuperAngelVariant(11, m_stateCnt);
            }
        }
    }

    if (m_stateCnt == 65) {
        SpawnSuperAngelBurst(0);
    } else if (m_stateCnt == 66) {
        SpawnSuperAngelBurst(45);
    }
}

u8 CRagEffect::OnProcess()
{
    const u32 now = timeGetTime();
    const u32 elapsedMs = now - m_lastProcessTick;
    m_lastProcessTick = now;
    m_tickCarryMs += static_cast<float>(elapsedMs);

    const bool frameDrivenStrHandler =
        m_handler == Handler::EzStr
        || m_handler == Handler::JobLevelUp50;

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
        || m_handler == Handler::Portal2
        || m_handler == Handler::ReadyPortal2
        || m_handler == Handler::WarpZone2
        || m_handler == Handler::MapMagicZone
        || m_handler == Handler::MapParticle
        || m_handler == Handler::SuperAngel;
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
        switch (m_handler) {
        case Handler::EzStr:
            ProcessEZ2STR();
            break;
        case Handler::Entry2:
            SpawnEntry2();
            break;
        case Handler::JobLevelUp50:
            SpawnJobLevelUp50();
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
        case Handler::Portal2:
            SpawnPortal2();
            break;
        case Handler::ReadyPortal2:
            SpawnReadyPortal2();
            break;
        case Handler::WarpZone2:
            SpawnWarpZone2();
            break;
        case Handler::MapMagicZone:
            SpawnMapMagicZone();
            break;
        case Handler::MapParticle:
            SpawnMapParticle();
            break;
        case Handler::SuperAngel:
            UpdateSuperAngel();
            break;
        case Handler::None:
        default:
            break;
        }

        for (auto it = m_primList.begin(); it != m_primList.end(); ) {
            CEffectPrim* prim = *it;
            if (!prim || !prim->OnProcess()) {
                delete prim;
                it = m_primList.erase(it);
                continue;
            }
            ++it;
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

void CRagEffect::Render(matrix* viewMatrix)
{
    if (!viewMatrix) {
        return;
    }

    if ((m_type == kRagEffectPortal || m_type == kRagEffectReadyPortal || m_type == kRagEffectWarpZone)
        && m_stateCnt <= 3) {
        const vector3d base = ResolveBasePosition();
        DbgLog("[RagEffect] render effect=%d step=%d prims=%zu base=(%.2f,%.2f,%.2f)\n",
            m_type,
            m_stateCnt,
            m_primList.size(),
            base.x,
            base.y,
            base.z);
    }

    if ((m_handler == Handler::EzStr || m_handler == Handler::JobLevelUp50) && m_aniClips) {
        for (int layerIndex = 1; layerIndex < m_aniClips->cLayer && layerIndex < static_cast<int>(m_isLayerDrawn.size()); ++layerIndex) {
            if (!m_isLayerDrawn[static_cast<size_t>(layerIndex)]) {
                continue;
            }
            RenderAniClip(layerIndex,
                m_aniClips->aLayer[static_cast<size_t>(layerIndex)],
                m_actXformData[static_cast<size_t>(layerIndex)],
                viewMatrix);
        }
    }

    for (CEffectPrim* prim : m_primList) {
        if (prim) {
            prim->Render(viewMatrix);
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
