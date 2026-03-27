#include "World.h"
#include "world/3dActor.h"
#include "DebugLog.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "core/File.h"
#include "res/GndRes.h"
#include "res/ModelRes.h"
#include "res/Res.h"
#include "res/Texture.h"
#include "res/WorldRes.h"
#include "session/Session.h"

#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <vector>

CWorld g_world;

namespace {
constexpr int kSceneGraphMaxLevel = 2;
constexpr float kNearPlane = 10.0f;
constexpr float kGroundSubmitNearPlane = 80.0f;
constexpr float kAttrTileDepthBias = 0.0010f;
constexpr float kPlayerBillboardWorldHeightScale = 2.0f;
constexpr float kGroundItemScreenScale = 1.6f;
constexpr unsigned short kGroundQuadIndices[6] = { 0, 1, 2, 0, 2, 3 };
constexpr bool kSubmitGroundSideFaces = true;
constexpr bool kDebugGroundFlatColors = false;
constexpr bool kDebugGroundCanonicalUvs = false;
constexpr bool kDebugGroundTestTexture = false;
constexpr bool kRejectOversizedGroundFaces = false;
constexpr bool kUseGroundLightmaps = true;
constexpr bool kLogGnd = false;
constexpr bool kLogGround = false;
constexpr int kLightmapEdge = 8;
constexpr int kLightmapAtlasEdge = 256;
constexpr float kPortalBaseHeightOffset = 0.05f;
constexpr int kPortalAuraParticleCount = 36;
constexpr int kPortalWindParticleCount = 24;
constexpr int kPortalRingSegments = 32;
constexpr float kPortalGroundDepthBias = 0.0002f;
constexpr float kFixedEffectBaseHeightOffset = 0.0f;
constexpr float kBackgroundObjectCullNearZ = kNearPlane;
constexpr float kBackgroundObjectCullFarZ = 6000.0f;
constexpr float kBackgroundObjectCullPadding = 32.0f;
constexpr u32 kBackgroundAnimNearInterval = 1;
constexpr u32 kBackgroundAnimMidInterval = 2;
constexpr u32 kBackgroundAnimFarInterval = 4;
constexpr u32 kBackgroundAnimVeryFarInterval = 8;
constexpr float kBackgroundAnimNearDistance = 30.0f;
constexpr float kBackgroundAnimMidDistance = 60.0f;
constexpr float kBackgroundAnimFarDistance = 120.0f;
constexpr u32 kWaterDefaultColor = 0x90FFFFFFu;
constexpr float kWaterTexUvScale = 0.25f;
constexpr u32 kWaterAnimationTicksPerSecond = 60;
constexpr int kLightmapsPerAtlasRow = kLightmapAtlasEdge / kLightmapEdge;
constexpr int kLightmapsPerAtlas = kLightmapsPerAtlasRow * kLightmapsPerAtlasRow;
constexpr float kGroundCullWidthFactor = 6.0f;
constexpr float kGroundCullHeightFactor = 6.0f;
constexpr int kGroundCullMarginTiles = 14;
constexpr int kJobWarpNpc = 0x2D;
constexpr int kJobWarpPortal = 0x80;
constexpr int kJobPreWarpPortal = 0x81;

enum class PortalVisualStyle {
    Ready,
    WarpZone,
    Portal,
};

bool ContainsIgnoreCaseAscii(const char* text, const char* needle)
{
    if (!text || !needle || !*needle) {
        return false;
    }

    const size_t needleLength = std::strlen(needle);
    for (const char* cursor = text; *cursor; ++cursor) {
        size_t matchLength = 0;
        while (matchLength < needleLength) {
            const unsigned char textCh = static_cast<unsigned char>(cursor[matchLength]);
            if (!textCh) {
                return false;
            }

            const unsigned char needleCh = static_cast<unsigned char>(needle[matchLength]);
            const unsigned char foldedText = static_cast<unsigned char>(std::tolower(textCh));
            const unsigned char foldedNeedle = static_cast<unsigned char>(std::tolower(needleCh));
            if (foldedText != foldedNeedle) {
                break;
            }

            ++matchLength;
        }

        if (matchLength == needleLength) {
            return true;
        }
    }

    return false;
}

vector3d NormalizeVec3(const vector3d& value);
vector3d ScaleVec3(const vector3d& value, float scale);
vector3d AddVec3(const vector3d& a, const vector3d& b);
bool ProjectPoint(const CRenderer& renderer, const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex);
bool GetGroundItemScreenRect(const matrix& viewMatrix,
    const CItem& item,
    RECT* outRect,
    int* outCenterX,
    int* outTopY,
    int* outLabelY,
    float* outDepth)
{
    if (outRect) {
        std::memset(outRect, 0, sizeof(*outRect));
    }
    if (outCenterX) {
        *outCenterX = 0;
    }
    if (outTopY) {
        *outTopY = 0;
    }
    if (outLabelY) {
        *outLabelY = 0;
    }
    if (outDepth) {
        *outDepth = 1.0f;
    }

    if (!item.m_isVisible) {
        return false;
    }

    CItem* mutableItem = const_cast<CItem*>(&item);
    if (!mutableItem->EnsureBillboardTexture()
        || item.m_billboardTextureWidth <= 0
        || item.m_billboardTextureHeight <= 0) {
        return false;
    }

    tlvertex3d projectedBase{};
    if (!ProjectPoint(g_renderer, viewMatrix, item.m_pos, &projectedBase)) {
        return false;
    }

    const float scaledAnchorX = static_cast<float>(item.m_billboardAnchorX) * kGroundItemScreenScale;
    const float scaledAnchorY = static_cast<float>(item.m_billboardAnchorY) * kGroundItemScreenScale;
    const float scaledWidth = static_cast<float>(item.m_billboardTextureWidth) * kGroundItemScreenScale;
    const float scaledHeight = static_cast<float>(item.m_billboardTextureHeight) * kGroundItemScreenScale;
    const float left = projectedBase.x - scaledAnchorX;
    const float top = projectedBase.y - scaledAnchorY;
    const float right = left + scaledWidth;
    const float bottom = top + scaledHeight;
    if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) || !std::isfinite(bottom)) {
        return false;
    }

    if (outRect) {
        outRect->left = static_cast<LONG>(std::lround(left));
        outRect->top = static_cast<LONG>(std::lround(top));
        outRect->right = static_cast<LONG>(std::lround(right));
        outRect->bottom = static_cast<LONG>(std::lround(bottom));
    }
    if (outCenterX) {
        *outCenterX = static_cast<int>(std::lround((left + right) * 0.5f));
    }
    if (outTopY) {
        *outTopY = static_cast<int>(std::lround(top));
    }
    if (outLabelY) {
        *outLabelY = static_cast<int>(std::lround(top));
    }
    if (outDepth) {
        *outDepth = projectedBase.z;
    }
    return true;
}
float GroundCoordX(int x, int width, float zoom);
float GroundCoordZ(int y, int height, float zoom);

bool IsPortalLikeEffect(const C3dWorldRes::effectSrcInfo& effect)
{
    return effect.type == 80
        || effect.type == 81
        || ContainsIgnoreCaseAscii(effect.name, "portal")
        || ContainsIgnoreCaseAscii(effect.name, "warp");
}

bool IsPortalActorJob(int job)
{
    return job == kJobWarpNpc
        || job == kJobWarpPortal
        || job == kJobPreWarpPortal;
}

float ResolveFixedEffectRadius(const C3dWorldRes::effectSrcInfo& effect)
{
    const float paramRadius = (std::max)(std::fabs(effect.param[0]), std::fabs(effect.param[1]));
    if (IsPortalLikeEffect(effect) && paramRadius > 0.0f) {
        return (std::max)(4.0f, paramRadius * 0.4f);
    }

    if (paramRadius > 0.0f) {
        return (std::max)(0.9f, paramRadius * 0.05f);
    }

    return IsPortalLikeEffect(effect) ? 6.0f : 3.5f;
}

float ResolveFixedEffectHeight(const C3dWorldRes::effectSrcInfo& effect)
{
    if (effect.type == 44) {
        return 6.0f;
    }

    if (IsPortalLikeEffect(effect)) {
        return 4.0f;
    }

    return 2.5f;
}

COLORREF ResolveFixedEffectColor(const C3dWorldRes::effectSrcInfo& effect)
{
    if (IsPortalLikeEffect(effect)) {
        return RGB(86, 196, 255);
    }

    switch (effect.type % 4) {
    case 0: return RGB(255, 196, 86);
    case 1: return RGB(140, 226, 255);
    case 2: return RGB(196, 132, 255);
    default: return RGB(255, 142, 112);
    }
}

unsigned int PackPortalColor(unsigned int alpha, COLORREF color)
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

CTexture* GetPortalParticleTexture(bool cloudTexture)
{
    static CTexture* glowTexture = nullptr;
    static CTexture* cloud = nullptr;

    CTexture*& texture = cloudTexture ? cloud : glowTexture;
    if (texture) {
        return texture;
    }

    constexpr int kTextureSize = 64;
    std::vector<unsigned long> pixels(static_cast<size_t>(kTextureSize) * static_cast<size_t>(kTextureSize), 0u);
    for (int y = 0; y < kTextureSize; ++y) {
        for (int x = 0; x < kTextureSize; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kTextureSize) * 2.0f - 1.0f;
            const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(kTextureSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(fx * fx + fy * fy);
            const float angle = std::atan2(fy, fx);

            float alpha = 0.0f;
            if (cloudTexture) {
                const float swirl = 0.5f + 0.5f * std::sin(angle * 3.0f + radius * 10.0f);
                const float plume = (std::max)(0.0f, 1.0f - radius);
                alpha = plume * plume * (0.45f + 0.55f * swirl);
            } else {
                const float core = (std::max)(0.0f, 1.0f - radius);
                const float ring = (std::max)(0.0f, 1.0f - std::fabs(radius - 0.58f) * 3.0f);
                alpha = (std::max)(core * core * core, ring * 0.85f);
            }

            const unsigned int alphaByte = static_cast<unsigned int>((std::min)(255.0f, alpha * 255.0f));
            pixels[static_cast<size_t>(y) * static_cast<size_t>(kTextureSize) + static_cast<size_t>(x)]
                = (alphaByte << 24)
                | (alphaByte << 16)
                | (alphaByte << 8)
                | alphaByte;
        }
    }

    texture = g_texMgr.CreateTexture(kTextureSize, kTextureSize, pixels.data(), PF_A8R8G8B8, false);
    return texture;
}

CTexture* GetPortalAuraTexture()
{
    static CTexture* auraTexture = nullptr;
    if (auraTexture) {
        return auraTexture;
    }

    constexpr int kTextureSize = 64;
    std::vector<unsigned long> pixels(static_cast<size_t>(kTextureSize) * static_cast<size_t>(kTextureSize), 0u);
    for (int y = 0; y < kTextureSize; ++y) {
        for (int x = 0; x < kTextureSize; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kTextureSize) * 2.0f - 1.0f;
            const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(kTextureSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(fx * fx + fy * fy);
            const float angle = std::atan2(fy, fx);
            const float streak = 0.5f + 0.5f * std::sin(angle * 6.0f + radius * 14.0f);
            const float core = (std::max)(0.0f, 1.0f - radius);
            const float alpha = core * core * (0.35f + 0.65f * streak);
            const unsigned int alphaByte = static_cast<unsigned int>((std::min)(255.0f, alpha * 255.0f));
            pixels[static_cast<size_t>(y) * static_cast<size_t>(kTextureSize) + static_cast<size_t>(x)]
                = (alphaByte << 24)
                | (alphaByte << 16)
                | (alphaByte << 8)
                | alphaByte;
        }
    }

    auraTexture = g_texMgr.CreateTexture(kTextureSize, kTextureSize, pixels.data(), PF_A8R8G8B8, false);
    return auraTexture;
}

CTexture* GetPortalRingTexture()
{
    static CTexture* ringTexture = nullptr;
    if (ringTexture) {
        return ringTexture;
    }

    constexpr int kTextureSize = 128;
    std::vector<unsigned long> pixels(static_cast<size_t>(kTextureSize) * static_cast<size_t>(kTextureSize), 0u);
    for (int y = 0; y < kTextureSize; ++y) {
        for (int x = 0; x < kTextureSize; ++x) {
            const float fx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kTextureSize) * 2.0f - 1.0f;
            const float fy = (static_cast<float>(y) + 0.5f) / static_cast<float>(kTextureSize) * 2.0f - 1.0f;
            const float radius = std::sqrt(fx * fx + fy * fy);
            const float angle = std::atan2(fy, fx);
            const float ring = (std::max)(0.0f, 1.0f - std::fabs(radius - 0.68f) * 10.0f);
            const float innerGlow = (std::max)(0.0f, 1.0f - std::fabs(radius - 0.48f) * 8.0f) * 0.35f;
            const float streak = 0.55f + 0.45f * std::sin(angle * 8.0f + radius * 18.0f);
            const float alpha = (ring + innerGlow) * streak;
            const unsigned int alphaByte = static_cast<unsigned int>((std::min)(255.0f, alpha * 255.0f));
            pixels[static_cast<size_t>(y) * static_cast<size_t>(kTextureSize) + static_cast<size_t>(x)]
                = (alphaByte << 24)
                | (alphaByte << 16)
                | (alphaByte << 8)
                | alphaByte;
        }
    }

    ringTexture = g_texMgr.CreateTexture(kTextureSize, kTextureSize, pixels.data(), PF_A8R8G8B8, false);
    return ringTexture;
}

CTexture* GetEffectTexture(const char* path)
{
    if (!path || !*path) {
        return nullptr;
    }

    static std::map<std::string, CTexture*> cache;
    static std::map<std::string, bool> loggedMissing;
    const std::string key(path);
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }

    CTexture* texture = g_texMgr.GetTexture(path, false);
    if (!texture && !loggedMissing[key]) {
        DbgLog("[World] missing effect texture '%s'\n", path);
        loggedMissing[key] = true;
    }
    cache.emplace(key, texture);
    return texture;
}

CTexture* GetAngelWingTexture()
{
    return GetEffectTexture("data\\texture\\effect\\wing003.bmp");
}

CTexture* GetJobLevelTexture()
{
    return GetEffectTexture("data\\texture\\effect\\explosive_1_128.bmp");
}

void SubmitTexturedBillboard(const vector3d& center,
    const matrix& viewMatrix,
    CTexture* texture,
    float width,
    float height,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    float depthBias,
    int renderFlags = 1)
{
    if (!texture || width <= 0.0f || height <= 0.0f) {
        return;
    }

    vector3d up = NormalizeVec3(ScaleVec3(g_renderer.m_eyeUp, -1.0f));
    vector3d right = NormalizeVec3(g_renderer.m_eyeRight);
    if (std::fabs(up.x) < 0.0001f && std::fabs(up.y) < 0.0001f && std::fabs(up.z) < 0.0001f) {
        up = vector3d{ 0.0f, -1.0f, 0.0f };
    }
    if (std::fabs(right.x) < 0.0001f && std::fabs(right.y) < 0.0001f && std::fabs(right.z) < 0.0001f) {
        right = vector3d{ 1.0f, 0.0f, 0.0f };
    }

    tlvertex3d baseVert{};
    if (!ProjectPoint(g_renderer, viewMatrix, center, &baseVert)) {
        return;
    }

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const vector3d worldVerts[4] = {
        AddVec3(AddVec3(center, ScaleVec3(up, halfHeight)), ScaleVec3(right, -halfWidth)),
        AddVec3(AddVec3(center, ScaleVec3(up, halfHeight)), ScaleVec3(right, halfWidth)),
        AddVec3(AddVec3(center, ScaleVec3(up, -halfHeight)), ScaleVec3(right, -halfWidth)),
        AddVec3(AddVec3(center, ScaleVec3(up, -halfHeight)), ScaleVec3(right, halfWidth)),
    };

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
        if (!ProjectPoint(g_renderer, viewMatrix, worldVerts[index], &face->m_verts[index])) {
            return;
        }
        if (depthBias > 0.0f) {
            face->m_verts[index].z = (std::max)(0.0f, baseVert.z - depthBias);
        } else {
            face->m_verts[index].oow = baseVert.oow;
        }
        face->m_verts[index].color = color;
        face->m_verts[index].specular = 0xFF000000u;
    }

    face->m_verts[0].tu = 0.0f;
    face->m_verts[0].tv = 0.0f;
    face->m_verts[1].tu = 1.0f;
    face->m_verts[1].tv = 0.0f;
    face->m_verts[2].tu = 0.0f;
    face->m_verts[2].tv = 1.0f;
    face->m_verts[3].tu = 1.0f;
    face->m_verts[3].tv = 1.0f;

    g_renderer.AddRP(face, renderFlags);
}

void SubmitScreenSpaceBillboard(const tlvertex3d& anchor,
    CTexture* texture,
    float centerOffsetX,
    float centerOffsetY,
    float width,
    float height,
    unsigned int color,
    D3DBLEND destBlend,
    float alphaSortKey,
    int renderFlags = 1)
{
    if (!texture || width <= 0.0f || height <= 0.0f) {
        return;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    const float centerX = anchor.x + centerOffsetX;
    const float centerY = anchor.y + centerOffsetY;
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;

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

    face->m_verts[0] = { centerX - halfWidth, centerY - halfHeight, anchor.z, anchor.oow, color, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { centerX + halfWidth, centerY - halfHeight, anchor.z, anchor.oow, color, 0xFF000000u, 1.0f, 0.0f };
    face->m_verts[2] = { centerX - halfWidth, centerY + halfHeight, anchor.z, anchor.oow, color, 0xFF000000u, 0.0f, 1.0f };
    face->m_verts[3] = { centerX + halfWidth, centerY + halfHeight, anchor.z, anchor.oow, color, 0xFF000000u, 1.0f, 1.0f };

    g_renderer.AddRP(face, renderFlags);
}

void RenderPortalGroundDisc(const vector3d& center,
    const matrix& viewMatrix,
    CTexture* texture,
    COLORREF tintColor,
    float radius,
    unsigned int alpha,
    float alphaSortKey,
    float depthBias,
    int renderFlags = 1)
{
    if (!texture || radius <= 0.0f) {
        return;
    }

    const vector3d quad[4] = {
        vector3d{ center.x - radius, center.y, center.z - radius },
        vector3d{ center.x + radius, center.y, center.z - radius },
        vector3d{ center.x - radius, center.y, center.z + radius },
        vector3d{ center.x + radius, center.y, center.z + radius },
    };

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
    face->destAlphaMode = D3DBLEND_ONE;
    face->alphaSortKey = alphaSortKey;

    for (int index = 0; index < 4; ++index) {
        if (!ProjectPoint(g_renderer, viewMatrix, quad[index], &face->m_verts[index])) {
            return;
        }
        if (depthBias > 0.0f) {
            face->m_verts[index].z = (std::max)(0.0f, face->m_verts[index].z - depthBias);
        }
        face->m_verts[index].color = PackPortalColor(alpha, tintColor);
        face->m_verts[index].specular = 0xFF000000u;
    }

    face->m_verts[0].tu = 0.0f;
    face->m_verts[0].tv = 0.0f;
    face->m_verts[1].tu = 1.0f;
    face->m_verts[1].tv = 0.0f;
    face->m_verts[2].tu = 0.0f;
    face->m_verts[2].tv = 1.0f;
    face->m_verts[3].tu = 1.0f;
    face->m_verts[3].tv = 1.0f;

    g_renderer.AddRP(face, renderFlags);
}

void RenderPortalParticleRibbon(const vector3d& center,
    const matrix& viewMatrix,
    CTexture* texture,
    COLORREF tintColor,
    unsigned int maxAlpha,
    int particleCount,
    float orbitRadius,
    float maxHeight,
    float baseAngleDegrees,
    float sweepDegrees,
    float angularSpeedDegrees,
    float cycleSeconds,
    float timeSeconds,
    float phaseOffset,
    float sizeStart,
    float sizeEnd,
    float alphaSortBase,
    float depthBias,
    bool easeInOut,
    float radialJitter,
    int renderFlags = 1)
{
    if (!texture || particleCount <= 0 || cycleSeconds <= 0.0f) {
        return;
    }

    for (int particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
        const float seed = (static_cast<float>(particleIndex) + 0.5f) / static_cast<float>(particleCount);
        const float life = WrapUnit(timeSeconds / cycleSeconds + seed + phaseOffset * 0.071f);
        float fade = std::sin(life * 3.14159265f);
        if (easeInOut) {
            fade *= fade;
        }
        if (fade <= 0.02f) {
            continue;
        }

        const float angleDegrees = baseAngleDegrees
            + sweepDegrees * life
            + angularSpeedDegrees * timeSeconds
            + static_cast<float>((particleIndex * 37) % 360) * 0.15f;
        const float angleRadians = angleDegrees * (3.14159265f / 180.0f);
        const float radiusPulse = orbitRadius
            * (0.92f + 0.08f * std::sin(timeSeconds * 2.6f + static_cast<float>(particleIndex) * 0.61f))
            + radialJitter * std::sin(timeSeconds * 3.1f + static_cast<float>(particleIndex) * 1.17f);
        const float height = maxHeight * life;
        const vector3d particleCenter = {
            center.x + std::cos(angleRadians) * radiusPulse,
            center.y - height,
            center.z + std::sin(angleRadians) * radiusPulse
        };
        const float size = sizeStart + (sizeEnd - sizeStart) * life;
        const unsigned int alpha = static_cast<unsigned int>((std::min)(255.0f, static_cast<float>(maxAlpha) * fade));
        SubmitTexturedBillboard(particleCenter,
            viewMatrix,
            texture,
            size,
            size,
            PackPortalColor(alpha, tintColor),
            D3DBLEND_ONE,
            alphaSortBase + 0.00001f * static_cast<float>(particleIndex),
            depthBias,
            renderFlags);
    }
}

void RenderPortalRingPulse(const vector3d& center,
    const matrix& viewMatrix,
    COLORREF tintColor,
    float baseRadius,
    float maxExpansion,
    float cycleSeconds,
    float timeSeconds,
    float cycleOffset,
    unsigned int peakAlpha,
    float alphaSortKey,
    float depthBias,
    int renderFlags = 1)
{
    const float life = WrapUnit(timeSeconds / cycleSeconds + cycleOffset);
    float fade = 0.0f;
    if (life < 0.15f) {
        fade = life / 0.15f;
    } else {
        fade = 1.0f - (life - 0.15f) / 0.85f;
    }
    fade = (std::max)(0.0f, fade);
    if (fade <= 0.01f) {
        return;
    }

    vector3d pulseCenter = center;
    pulseCenter.y -= life * 0.08f;
    RenderPortalGroundDisc(pulseCenter,
        viewMatrix,
        GetPortalRingTexture(),
        tintColor,
        baseRadius + maxExpansion * life,
        static_cast<unsigned int>((std::min)(255.0f, static_cast<float>(peakAlpha) * fade)),
        alphaSortKey,
        depthBias,
        renderFlags);
}

void RenderPortalEffectAtPosition(const vector3d& sourcePos,
    const matrix& viewMatrix,
    COLORREF ringColor,
    float radius,
    float bandHeight,
    float timeSeconds,
    float phaseOffset,
    bool snapToGround,
    PortalVisualStyle style)
{
    auto submitQuad = [&](const vector3d (&quad)[4],
                          unsigned int color0,
                          unsigned int color1,
                          D3DBLEND destBlend,
                          float alphaSortKey,
                          float depthBias,
                          int renderFlags) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return;
        }

        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts;
        face->numVerts = 4;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = nullptr;
        face->mtPreset = 0;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = destBlend;
        face->alphaSortKey = alphaSortKey;

        for (int index = 0; index < 4; ++index) {
            if (!ProjectPoint(g_renderer, viewMatrix, quad[index], &face->m_verts[index])) {
                return;
            }
            if (depthBias > 0.0f) {
                face->m_verts[index].z = (std::max)(0.0f, face->m_verts[index].z - depthBias);
            }
            face->m_verts[index].color = index >= 2 ? color1 : color0;
            face->m_verts[index].specular = 0xFF000000u;
            face->m_verts[index].tu = (index == 0 || index == 2) ? 0.0f : 1.0f;
            face->m_verts[index].tv = (index <= 1) ? 0.0f : 1.0f;
        }

        g_renderer.AddRP(face, renderFlags);
    };

    vector3d center = sourcePos;
    if (snapToGround && g_world.m_attr) {
        center.y = g_world.m_attr->GetHeight(center.x, center.z) - kPortalBaseHeightOffset;
    } else {
        center.y -= kPortalBaseHeightOffset;
    }

    tlvertex3d centerVert{};
    if (!ProjectPoint(g_renderer, viewMatrix, center, &centerVert)) {
        return;
    }

    const float pulse = radius * 0.08f * std::sin(timeSeconds * 3.4f + phaseOffset);
    const float outerRadius = radius + pulse;
    const float innerRadius = outerRadius * 0.7f;
    const float rotation = timeSeconds * 2.55f + phaseOffset * 0.35f;
    const unsigned int ringColorBottom = PackPortalColor(208u, ringColor);
    const unsigned int ringColorTop = PackPortalColor(104u, ringColor);

    const float outerHaloRadius = outerRadius * 1.28f;
    const float outerHaloInnerRadius = outerHaloRadius * 0.83f;
    const float portalScale = radius / 5.0f;

    int auraGroupCount = 3;
    int auraParticleCount = kPortalAuraParticleCount;
    int windGroupCount = 4;
    int windParticleCount = kPortalWindParticleCount;
    float ringAlphaScale = 1.0f;
    float glowAlphaScale = 1.0f;
    float portalRadiusScale = 1.0f;
    float windRadiusScale = 1.0f;
    bool renderWarpDisc = false;
    unsigned int pulseAlpha = 220u;
    const int emissiveRenderFlags = 1 | 2;

    switch (style) {
    case PortalVisualStyle::Ready:
        auraParticleCount = 0;
        windGroupCount = 0;
        windParticleCount = 0;
        ringAlphaScale = 0.6f;
        glowAlphaScale = 0.98f;
        portalRadiusScale = 0.92f;
        pulseAlpha = 110u;
        break;
    case PortalVisualStyle::WarpZone:
        auraGroupCount = 0;
        auraParticleCount = 0;
        windGroupCount = 4;
        windParticleCount = 22;
        ringAlphaScale = 1.0f;
        glowAlphaScale = 1.28f;
        portalRadiusScale = 1.08f;
        windRadiusScale = 1.18f;
        renderWarpDisc = true;
        pulseAlpha = 180u;
        break;
    case PortalVisualStyle::Portal:
    default:
        auraParticleCount = 36;
        windParticleCount = 24;
        ringAlphaScale = 1.25f;
        glowAlphaScale = 1.45f;
        pulseAlpha = 255u;
        break;
    }

    if (renderWarpDisc) {
        vector3d discCenter = center;
        discCenter.y -= 0.08f;
        RenderPortalGroundDisc(discCenter,
            viewMatrix,
            GetPortalParticleTexture(false),
            ringColor,
            radius * 3.0f,
            76u,
            centerVert.oow - 0.0002f,
            kPortalGroundDepthBias,
            emissiveRenderFlags);
    }

    SubmitTexturedBillboard(center,
        viewMatrix,
        GetPortalParticleTexture(false),
        outerRadius * 3.4f,
        outerRadius * 2.7f,
        PackPortalColor(static_cast<unsigned int>((std::min)(255.0f, 182.0f * glowAlphaScale)), ringColor),
        D3DBLEND_ONE,
        centerVert.oow - 0.00015f,
        kPortalGroundDepthBias,
        emissiveRenderFlags);

    if (style != PortalVisualStyle::WarpZone) {
        RenderPortalRingPulse(center,
            viewMatrix,
            ringColor,
            outerRadius * 0.55f * portalRadiusScale,
            outerRadius * 0.52f * portalRadiusScale,
            0.78f,
            timeSeconds,
            0.00f + phaseOffset * 0.03f,
            pulseAlpha,
            centerVert.oow - 0.00012f,
            kPortalGroundDepthBias,
            emissiveRenderFlags);
        RenderPortalRingPulse(center,
            viewMatrix,
            ringColor,
            outerRadius * 0.55f * portalRadiusScale,
            outerRadius * 0.52f * portalRadiusScale,
            0.78f,
            timeSeconds,
            0.33f + phaseOffset * 0.03f,
            pulseAlpha,
            centerVert.oow - 0.00010f,
            kPortalGroundDepthBias,
            emissiveRenderFlags);
        RenderPortalRingPulse(center,
            viewMatrix,
            ringColor,
            outerRadius * 0.55f * portalRadiusScale,
            outerRadius * 0.52f * portalRadiusScale,
            0.78f,
            timeSeconds,
            0.66f + phaseOffset * 0.03f,
            pulseAlpha,
            centerVert.oow - 0.00008f,
                kPortalGroundDepthBias,
                emissiveRenderFlags);
    }

    const float auraDistances[3] = { 4.0f, 3.0f, 2.0f };
    const float auraHeights[3] = { 7.8f, 6.0f, 4.6f };
    for (int group = 0; group < auraGroupCount; ++group) {
        RenderPortalParticleRibbon(center,
            viewMatrix,
            GetPortalAuraTexture(),
            ringColor,
            static_cast<unsigned int>((std::min)(255.0f, 176.0f * ringAlphaScale)),
            auraParticleCount,
            auraDistances[group] * portalScale * portalRadiusScale,
            auraHeights[group] * portalScale,
            360.0f * static_cast<float>(group) / 3.0f,
            360.0f,
            30.0f + static_cast<float>(group) * 6.0f,
            2.7f - static_cast<float>(group) * 0.18f,
            timeSeconds,
            phaseOffset + static_cast<float>(group) * 0.19f,
                outerRadius * 0.34f,
                outerRadius * 0.12f,
            centerVert.oow + 0.00012f * static_cast<float>(group),
            0.0f,
            true,
                outerRadius * 0.14f,
            emissiveRenderFlags);
    }

    const float windDistances[4] = { 9.0f, 7.5f, 6.0f, 4.5f };
    const float windHeights[4] = { 1.5f, 3.5f, 5.5f, 7.5f };
    const float windRotStarts[4] = { 0.0f, 90.0f, 180.0f, 270.0f };
    for (int group = 0; group < windGroupCount; ++group) {
        RenderPortalParticleRibbon(center,
            viewMatrix,
            GetPortalParticleTexture(true),
            RGB(210, 240, 255),
            style == PortalVisualStyle::WarpZone ? 144u : 132u,
            windParticleCount,
            windDistances[group] * portalScale * windRadiusScale,
            windHeights[group] * portalScale,
            windRotStarts[group],
            30.0f,
            style == PortalVisualStyle::WarpZone ? (58.0f - static_cast<float>(group) * 3.0f) : (46.0f - static_cast<float>(group) * 3.0f),
            style == PortalVisualStyle::WarpZone ? (1.65f + static_cast<float>(group) * 0.08f) : (2.2f + static_cast<float>(group) * 0.12f),
            timeSeconds,
            phaseOffset + static_cast<float>(group) * 0.13f,
                outerRadius * (style == PortalVisualStyle::WarpZone ? 0.36f : 0.44f),
                outerRadius * (style == PortalVisualStyle::WarpZone ? 0.14f : 0.18f),
            centerVert.oow + 0.00025f + 0.00012f * static_cast<float>(group),
            0.0f,
            false,
                outerRadius * 0.22f,
            emissiveRenderFlags);
    }

    for (int segment = 0; segment < kPortalRingSegments; ++segment) {
        const float angle0 = -rotation * 0.65f + (2.0f * 3.14159265f * static_cast<float>(segment) / static_cast<float>(kPortalRingSegments));
        const float angle1 = -rotation * 0.65f + (2.0f * 3.14159265f * static_cast<float>(segment + 1) / static_cast<float>(kPortalRingSegments));
        const vector3d haloQuad[4] = {
            vector3d{ center.x + std::cos(angle0) * outerHaloInnerRadius, center.y - 0.02f, center.z + std::sin(angle0) * outerHaloInnerRadius },
            vector3d{ center.x + std::cos(angle1) * outerHaloInnerRadius, center.y - 0.02f, center.z + std::sin(angle1) * outerHaloInnerRadius },
            vector3d{ center.x + std::cos(angle0) * outerHaloRadius, center.y - 0.02f, center.z + std::sin(angle0) * outerHaloRadius },
            vector3d{ center.x + std::cos(angle1) * outerHaloRadius, center.y - 0.02f, center.z + std::sin(angle1) * outerHaloRadius },
        };
        submitQuad(haloQuad,
            PackPortalColor(static_cast<unsigned int>(92.0f * ringAlphaScale), ringColor),
            PackPortalColor(static_cast<unsigned int>(16.0f * ringAlphaScale), ringColor),
            D3DBLEND_ONE,
            centerVert.oow - 0.0001f,
            kPortalGroundDepthBias,
            emissiveRenderFlags);
    }

    for (int segment = 0; segment < kPortalRingSegments; ++segment) {
        const float angle0 = rotation + (2.0f * 3.14159265f * static_cast<float>(segment) / static_cast<float>(kPortalRingSegments));
        const float angle1 = rotation + (2.0f * 3.14159265f * static_cast<float>(segment + 1) / static_cast<float>(kPortalRingSegments));
        const vector3d ringQuad[4] = {
            vector3d{ center.x + std::cos(angle0) * innerRadius, center.y, center.z + std::sin(angle0) * innerRadius },
            vector3d{ center.x + std::cos(angle1) * innerRadius, center.y, center.z + std::sin(angle1) * innerRadius },
            vector3d{ center.x + std::cos(angle0) * outerRadius, center.y, center.z + std::sin(angle0) * outerRadius },
            vector3d{ center.x + std::cos(angle1) * outerRadius, center.y, center.z + std::sin(angle1) * outerRadius },
        };
        submitQuad(ringQuad,
            PackPortalColor(static_cast<unsigned int>(208.0f * ringAlphaScale), ringColor),
            PackPortalColor(static_cast<unsigned int>(104.0f * ringAlphaScale), ringColor),
            D3DBLEND_ONE,
            centerVert.oow,
            kPortalGroundDepthBias,
            emissiveRenderFlags);
    }
}

class CFixedWorldEffect : public CGameObject {
public:
    explicit CFixedWorldEffect(const C3dWorldRes::effectSrcInfo& source)
        : m_source(source)
        , m_spawnTick(timeGetTime())
    {
    }

    u8 OnProcess() override
    {
        return 1;
    }

    void Render(matrix* viewMatrix) override
    {
        if (!viewMatrix) {
            return;
        }

        auto packColor = [](unsigned int alpha, COLORREF color) -> unsigned int {
            return (alpha << 24)
                | (static_cast<unsigned int>(GetRValue(color)) << 16)
                | (static_cast<unsigned int>(GetGValue(color)) << 8)
                | static_cast<unsigned int>(GetBValue(color));
        };

        auto submitQuad = [&](const vector3d (&quad)[4],
                              unsigned int color0,
                              unsigned int color1,
                              D3DBLEND destBlend,
                              float alphaSortKey) {
            RPFace* face = g_renderer.BorrowNullRP();
            if (!face) {
                return;
            }

            face->primType = D3DPT_TRIANGLESTRIP;
            face->verts = face->m_verts;
            face->numVerts = 4;
            face->indices = nullptr;
            face->numIndices = 0;
            face->tex = nullptr;
            face->mtPreset = 0;
            face->cullMode = D3DCULL_NONE;
            face->srcAlphaMode = D3DBLEND_SRCALPHA;
            face->destAlphaMode = destBlend;
            face->alphaSortKey = alphaSortKey;

            for (int index = 0; index < 4; ++index) {
                if (!ProjectPoint(g_renderer, *viewMatrix, quad[index], &face->m_verts[index])) {
                    return;
                }
                face->m_verts[index].color = index >= 2 ? color1 : color0;
                face->m_verts[index].specular = 0xFF000000u;
                face->m_verts[index].tu = (index == 0 || index == 2) ? 0.0f : 1.0f;
                face->m_verts[index].tv = (index <= 1) ? 0.0f : 1.0f;
            }

            g_renderer.AddRP(face, 1);
        };

        vector3d center = m_source.pos;
        const DWORD elapsed = timeGetTime() - m_spawnTick;
        const float timeSeconds = static_cast<float>(elapsed) * 0.001f;
        const bool portalLike = IsPortalLikeEffect(m_source);
        const float baseRadius = ResolveFixedEffectRadius(m_source);
        const COLORREF ringColor = ResolveFixedEffectColor(m_source);
        if (!portalLike) {
            vector3d center = m_source.pos;
            if (g_world.m_attr) {
                center.y = g_world.m_attr->GetHeight(center.x, center.z) + kFixedEffectBaseHeightOffset;
            }

            tlvertex3d centerVert{};
            if (!ProjectPoint(g_renderer, *viewMatrix, center, &centerVert)) {
                return;
            }

            const float bandHeight = ResolveFixedEffectHeight(m_source);
            const float pulse = baseRadius * 0.05f * std::sin(timeSeconds * 2.1f);
            const float outerRadius = baseRadius + pulse;
            const float innerRadius = outerRadius * 0.6f;
            const float rotation = timeSeconds * 0.9f;
            const unsigned int ringColorBottom = packColor(144u, ringColor);
            const unsigned int ringColorTop = packColor(48u, ringColor);

            for (int segment = 0; segment < kPortalRingSegments; ++segment) {
                const float angle0 = rotation + (2.0f * 3.14159265f * static_cast<float>(segment) / static_cast<float>(kPortalRingSegments));
                const float angle1 = rotation + (2.0f * 3.14159265f * static_cast<float>(segment + 1) / static_cast<float>(kPortalRingSegments));
                const vector3d ringQuad[4] = {
                    vector3d{ center.x + std::cos(angle0) * innerRadius, center.y, center.z + std::sin(angle0) * innerRadius },
                    vector3d{ center.x + std::cos(angle1) * innerRadius, center.y, center.z + std::sin(angle1) * innerRadius },
                    vector3d{ center.x + std::cos(angle0) * outerRadius, center.y, center.z + std::sin(angle0) * outerRadius },
                    vector3d{ center.x + std::cos(angle1) * outerRadius, center.y, center.z + std::sin(angle1) * outerRadius },
                };
                submitQuad(ringQuad, ringColorBottom, ringColorTop, D3DBLEND_INVSRCALPHA, centerVert.oow);
            }
            return;
        }

        RenderPortalEffectAtPosition(m_source.pos,
            *viewMatrix,
            ringColor,
            baseRadius,
            ResolveFixedEffectHeight(m_source),
            timeSeconds,
            0.0f,
            true,
            PortalVisualStyle::Portal);
    }

private:
    C3dWorldRes::effectSrcInfo m_source;
    DWORD m_spawnTick;
};

class CLevelUpEffect : public CGameObject {
public:
    CLevelUpEffect(CGameActor* actor, u32 effectId)
        : m_actor(actor)
        , m_origin(actor ? actor->m_pos : vector3d{})
        , m_spawnTick(timeGetTime())
        , m_effectId(effectId)
    {
    }

    u8 OnProcess() override
    {
        return (timeGetTime() - m_spawnTick) < ResolveLifeMs() ? 1 : 0;
    }

    void Render(matrix* viewMatrix) override
    {
        if (!viewMatrix) {
            return;
        }

        const DWORD elapsed = timeGetTime() - m_spawnTick;
        const float timeSeconds = static_cast<float>(elapsed) * 0.001f;
        if (timeSeconds <= 0.0f) {
            return;
        }

        const float totalLife = static_cast<float>(ResolveLifeMs()) * 0.001f;
        const float normalized = (std::min)(1.0f, timeSeconds / totalLife);
        const float fadeOut = normalized < 0.72f ? 1.0f : (1.0f - (normalized - 0.72f) / 0.28f);
        if (fadeOut <= 0.0f) {
            return;
        }

        tlvertex3d anchor{};
        if (!ResolveProjectedAnchor(*viewMatrix, &anchor)) {
            return;
        }

        switch (m_effectId) {
        case 158:
            RenderJobLevel(anchor, normalized, fadeOut, false);
            break;
        case 337:
            RenderJobLevel(anchor, normalized, fadeOut, true);
            break;
        case 338:
            RenderSuperAngel(anchor, normalized, fadeOut, false);
            break;
        case 582:
            RenderSuperAngel(anchor, normalized, fadeOut, true);
            break;
        case 371:
        default:
            RenderAngel(anchor, normalized, fadeOut);
            break;
        }
    }

private:
    DWORD ResolveLifeMs() const
    {
        switch (m_effectId) {
        case 158:
            return 1500u;
        case 337:
            return 1650u;
        case 338:
        case 582:
            return 1800u;
        case 371:
        default:
            return 1700u;
        }
    }

    bool ResolveProjectedAnchor(const matrix& viewMatrix, tlvertex3d* outAnchor) const
    {
        if (!outAnchor) {
            return false;
        }

        const vector3d base = m_actor ? m_actor->m_pos : m_origin;
        if (!ProjectPoint(g_renderer, viewMatrix, base, outAnchor)) {
            return false;
        }

        // Ref CRagEffect::ProcessEZ2STR uses the actor's projected position with a default y offset of -80.
        outAnchor->y -= 80.0f;
        return true;
    }

    float ResolveFadeAlpha(float fadeOut, float maxAlpha) const
    {
        return static_cast<float>(static_cast<unsigned int>(maxAlpha * fadeOut));
    }

    void RenderAngel(const tlvertex3d& anchor, float normalized, float fadeOut) const
    {
        const COLORREF glowColor = RGB(255, 236, 168);
        const COLORREF wingColor = RGB(255, 250, 236);
        const unsigned int alpha = static_cast<unsigned int>(ResolveFadeAlpha(fadeOut, 210.0f));
        const int renderFlags = 1 | 4 | 8;
        const float risePx = normalized * 10.0f;

        CTexture* wingTexture = GetAngelWingTexture();
        if (!wingTexture) {
            wingTexture = GetPortalAuraTexture();
        }

        if (wingTexture) {
            for (int side = -1; side <= 1; side += 2) {
                SubmitScreenSpaceBillboard(anchor,
                    wingTexture,
                    static_cast<float>(side) * (54.0f + normalized * 8.0f),
                    risePx - 6.0f,
                    wingTexture == GetPortalAuraTexture() ? 150.0f : 118.0f,
                    wingTexture == GetPortalAuraTexture() ? 206.0f : 166.0f,
                    PackPortalColor(static_cast<unsigned int>(alpha * 0.95f), wingColor),
                    D3DBLEND_ONE,
                    0.0f,
                    renderFlags);
            }
        }

        SubmitScreenSpaceBillboard(anchor,
            GetPortalParticleTexture(false),
            0.0f,
            risePx - 2.0f,
            118.0f + normalized * 22.0f,
            228.0f + normalized * 34.0f,
            PackPortalColor(static_cast<unsigned int>(alpha * 0.8f), wingColor),
            D3DBLEND_ONE,
            0.0f,
            renderFlags);

        SubmitScreenSpaceBillboard(anchor,
            GetPortalRingTexture(),
            0.0f,
            risePx + 38.0f,
            136.0f + normalized * 20.0f,
            34.0f + normalized * 6.0f,
            PackPortalColor(static_cast<unsigned int>(alpha * 0.7f), glowColor),
            D3DBLEND_ONE,
            0.0f,
            renderFlags);

        for (int sparkIndex = 0; sparkIndex < 8; ++sparkIndex) {
            const float seed = (static_cast<float>(sparkIndex) + 0.5f) / 8.0f;
            const float angle = seed * 6.2831853f;
            SubmitScreenSpaceBillboard(anchor,
                GetPortalParticleTexture((sparkIndex & 1) != 0),
                std::cos(angle) * (16.0f + normalized * 30.0f),
                risePx - (18.0f + normalized * 44.0f + seed * 12.0f),
                18.0f,
                34.0f,
                PackPortalColor(static_cast<unsigned int>(alpha * (0.3f + 0.5f * (1.0f - seed))), glowColor),
                D3DBLEND_ONE,
                0.0f,
                renderFlags);
        }
    }

    void RenderEntry(const tlvertex3d& anchor, float normalized, float fadeOut) const
    {
        const COLORREF glowColor = RGB(255, 214, 144);
        const unsigned int alpha = static_cast<unsigned int>(ResolveFadeAlpha(fadeOut, 185.0f));
        const int renderFlags = 1 | 4 | 8;
        const float risePx = normalized * 10.0f;

        if (CTexture* burstTexture = GetJobLevelTexture()) {
            SubmitScreenSpaceBillboard(anchor,
                burstTexture,
                0.0f,
                risePx,
                112.0f + normalized * 40.0f,
                112.0f + normalized * 40.0f,
                PackPortalColor(static_cast<unsigned int>(alpha * 0.95f), RGB(255, 248, 220)),
                D3DBLEND_ONE,
                0.0f,
                renderFlags);
        }

        for (int index = 0; index < 10; ++index) {
            const float seed = (static_cast<float>(index) + 0.5f) / 10.0f;
            const float angle = seed * 6.2831853f + normalized * 5.0f;
            SubmitScreenSpaceBillboard(anchor,
                GetPortalParticleTexture((index & 1) != 0),
                std::cos(angle) * (18.0f + normalized * 38.0f),
                risePx - (14.0f + normalized * 26.0f),
                22.0f,
                30.0f,
                PackPortalColor(static_cast<unsigned int>(alpha * (0.4f + 0.5f * (1.0f - seed))), glowColor),
                D3DBLEND_ONE,
                0.0f,
                renderFlags);
        }
    }

    void RenderJobLevel(const tlvertex3d& anchor, float normalized, float fadeOut, bool advanced) const
    {
        const COLORREF glowColor = advanced ? RGB(255, 188, 112) : RGB(255, 214, 144);
        const COLORREF coreColor = advanced ? RGB(255, 242, 220) : RGB(255, 248, 232);
        const unsigned int alpha = static_cast<unsigned int>(ResolveFadeAlpha(fadeOut, advanced ? 210.0f : 190.0f));
        const int renderFlags = 1 | 4 | 8;
        const float risePx = normalized * 10.0f;

        SubmitScreenSpaceBillboard(anchor,
            GetPortalRingTexture(),
            0.0f,
            risePx + 12.0f,
            (advanced ? 154.0f : 140.0f) + normalized * 22.0f,
            38.0f,
            PackPortalColor(static_cast<unsigned int>(alpha * 0.46f), glowColor),
            D3DBLEND_ONE,
            0.0f,
            renderFlags);

        if (CTexture* burstTexture = GetJobLevelTexture()) {
            const float size = (advanced ? 156.0f : 136.0f) + normalized * 30.0f;
            SubmitScreenSpaceBillboard(anchor,
                burstTexture,
                0.0f,
                risePx - 4.0f,
                size,
                size,
                PackPortalColor(static_cast<unsigned int>(alpha * 0.98f), coreColor),
                D3DBLEND_ONE,
                0.0f,
                renderFlags);
        }

        for (int index = 0; index < 6; ++index) {
            const float seed = (static_cast<float>(index) + 0.5f) / 6.0f;
            const float angle = seed * 6.2831853f + normalized * 2.8f;
            SubmitScreenSpaceBillboard(anchor,
                GetPortalParticleTexture((index & 1) != 0),
                std::cos(angle) * (22.0f + normalized * 26.0f),
                risePx - (12.0f + normalized * 24.0f + seed * 10.0f),
                20.0f,
                28.0f,
                PackPortalColor(static_cast<unsigned int>(alpha * (0.38f + 0.4f * (1.0f - seed))), glowColor),
                D3DBLEND_ONE,
                0.0f,
                renderFlags);
        }
    }

    void RenderSuperAngel(const tlvertex3d& anchor, float normalized, float fadeOut, bool taekwonVariant) const
    {
        const COLORREF glowColor = taekwonVariant ? RGB(255, 206, 116) : RGB(255, 236, 180);
        const COLORREF accentColor = taekwonVariant ? RGB(255, 164, 92) : RGB(196, 236, 255);
        const unsigned int alpha = static_cast<unsigned int>(ResolveFadeAlpha(fadeOut, taekwonVariant ? 220.0f : 210.0f));
        const int renderFlags = 1 | 4 | 8;
        const float risePx = normalized * 12.0f;

        CTexture* wingTexture = GetAngelWingTexture();
        if (!wingTexture) {
            wingTexture = GetPortalAuraTexture();
        }

        if (wingTexture) {
            for (int wingIndex = 0; wingIndex < 4; ++wingIndex) {
                const float side = (wingIndex < 2) ? -1.0f : 1.0f;
                const float row = (wingIndex % 2 == 0) ? 0.0f : 1.0f;
                SubmitScreenSpaceBillboard(anchor,
                    wingTexture,
                    side * (46.0f + row * 24.0f + normalized * 8.0f),
                    risePx - (8.0f + row * 32.0f),
                    wingTexture == GetPortalAuraTexture() ? (122.0f + row * 20.0f) : (94.0f + row * 14.0f),
                    wingTexture == GetPortalAuraTexture() ? (168.0f + row * 30.0f) : (136.0f + row * 24.0f),
                    PackPortalColor(static_cast<unsigned int>(alpha * 0.85f), taekwonVariant ? accentColor : glowColor),
                    D3DBLEND_ONE,
                    0.0f,
                    renderFlags);
            }
        }

        SubmitScreenSpaceBillboard(anchor,
            GetPortalParticleTexture(false),
            0.0f,
            risePx - 10.0f,
            112.0f + normalized * 20.0f,
            192.0f + normalized * 24.0f,
            PackPortalColor(static_cast<unsigned int>(alpha * 0.78f), glowColor),
            D3DBLEND_ONE,
            0.0f,
            renderFlags);

        SubmitScreenSpaceBillboard(anchor,
            GetPortalRingTexture(),
            0.0f,
            risePx + 44.0f,
            150.0f + normalized * 18.0f,
            36.0f,
            PackPortalColor(static_cast<unsigned int>(alpha * 0.72f), taekwonVariant ? accentColor : glowColor),
            D3DBLEND_ONE,
            0.0f,
            renderFlags);

        for (int orbIndex = 0; orbIndex < 12; ++orbIndex) {
            const float seed = (static_cast<float>(orbIndex) + 0.5f) / 12.0f;
            const float angle = seed * 6.2831853f;
            SubmitScreenSpaceBillboard(anchor,
                GetPortalParticleTexture((orbIndex & 1) != 0),
                std::cos(angle) * (18.0f + seed * 26.0f + normalized * 24.0f),
                risePx - (18.0f + normalized * 40.0f + seed * 14.0f),
                18.0f,
                26.0f,
                PackPortalColor(static_cast<unsigned int>(alpha * (0.4f + 0.45f * (1.0f - seed))), (orbIndex % 2) == 0 ? glowColor : accentColor),
                D3DBLEND_ONE,
                0.0f,
                renderFlags);
        }
    }

    CGameActor* m_actor;
    vector3d m_origin;
    DWORD m_spawnTick;
    u32 m_effectId;
};

vector3d NormalizeVec3(const vector3d& value);
vector3d ScaleVec3(const vector3d& value, float scale);
vector3d AddVec3(const vector3d& a, const vector3d& b);
bool ProjectPoint(const CRenderer& renderer, const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex);

float MaxBackgroundActorScale(const C3dActor& actor)
{
    return (std::max)(
        std::fabs(actor.m_scale.x),
        (std::max)(std::fabs(actor.m_scale.y), std::fabs(actor.m_scale.z)));
}

bool ShouldRenderBackgroundActor(const C3dActor& actor, const matrix& viewMatrix)
{
    const float clipX = actor.m_pos.x * viewMatrix.m[0][0]
        + actor.m_pos.y * viewMatrix.m[1][0]
        + actor.m_pos.z * viewMatrix.m[2][0]
        + viewMatrix.m[3][0];
    const float clipY = actor.m_pos.x * viewMatrix.m[0][1]
        + actor.m_pos.y * viewMatrix.m[1][1]
        + actor.m_pos.z * viewMatrix.m[2][1]
        + viewMatrix.m[3][1];
    const float clipZ = actor.m_pos.x * viewMatrix.m[0][2]
        + actor.m_pos.y * viewMatrix.m[1][2]
        + actor.m_pos.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];

    const float worldRadius = (std::max)(1.0f, actor.m_boundRadius * (std::max)(1.0f, MaxBackgroundActorScale(actor)));
    if (!std::isfinite(clipZ) || clipZ < (kBackgroundObjectCullNearZ - worldRadius) || clipZ > (kBackgroundObjectCullFarZ + worldRadius)) {
        return false;
    }

    const float projectedX = g_renderer.m_xoffset + clipX * g_renderer.m_hpc / clipZ;
    const float projectedY = g_renderer.m_yoffset + clipY * g_renderer.m_vpc / clipZ;
    const float projectedRadius = (std::max)(
        kBackgroundObjectCullPadding,
        worldRadius * ((std::max)(std::fabs(g_renderer.m_hpc), std::fabs(g_renderer.m_vpc)) / clipZ));

    return projectedX >= -projectedRadius
        && projectedX <= static_cast<float>(g_renderer.m_width) + projectedRadius
        && projectedY >= -projectedRadius
        && projectedY <= static_cast<float>(g_renderer.m_height) + projectedRadius;
}

u32 ResolveBackgroundActorUpdateInterval(const C3dActor& actor,
    const vector3d* focusPos,
    const matrix* viewMatrix)
{
    if (viewMatrix && ShouldRenderBackgroundActor(actor, *viewMatrix)) {
        return 1;
    }

    if (!focusPos) {
        return kBackgroundAnimVeryFarInterval;
    }

    const float dx = actor.m_pos.x - focusPos->x;
    const float dz = actor.m_pos.z - focusPos->z;
    const float distanceSq = dx * dx + dz * dz;
    if (distanceSq <= kBackgroundAnimNearDistance * kBackgroundAnimNearDistance) {
        return kBackgroundAnimNearInterval;
    }
    if (distanceSq <= kBackgroundAnimMidDistance * kBackgroundAnimMidDistance) {
        return kBackgroundAnimMidInterval;
    }
    if (distanceSq <= kBackgroundAnimFarDistance * kBackgroundAnimFarDistance) {
        return kBackgroundAnimFarInterval;
    }
    return kBackgroundAnimVeryFarInterval;
}

bool RenderCachedBillboard(const CWorld::BillboardScreenEntry& entry)
{
    constexpr float kBillboardDepthBias = 0.0005f;

    CPc* actor = entry.actor;
    if (!actor || !actor->m_isVisible || !actor->m_billboardTexture) {
        return false;
    }

    u32 billboardColor = 0xFFFFFFFFu;
    if (actor->m_vanishTime != 0) {
        const u32 now = timeGetTime();
        if (now > actor->m_vanishTime) {
            return false;
        }

        const int fadeAlpha = static_cast<int>((actor->m_vanishTime - now) >> 1);
        const u32 clampedAlpha = static_cast<u32>((std::max)(0, (std::min)(255, fadeAlpha)));
        billboardColor = (clampedAlpha << 24) | 0x00FFFFFFu;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = actor->m_billboardTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0.0f;

    tlvertex3d* verts = face->m_verts;
    for (int index = 0; index < 4; ++index) {
        verts[index].x = entry.renderX[index];
        verts[index].y = entry.renderY[index];
    }
    for (int index = 0; index < 4; ++index) {
        verts[index].z = (std::max)(0.0f, entry.baseZ - kBillboardDepthBias);
        verts[index].oow = entry.baseOow;
        verts[index].color = billboardColor;
        verts[index].specular = 0xFF000000u;
    }

    verts[0].tu = 0.0f;
    verts[0].tv = 0.0f;
    verts[1].tu = 1.0f;
    verts[1].tv = 0.0f;
    verts[2].tu = 0.0f;
    verts[2].tv = 1.0f;
    verts[3].tu = 1.0f;
    verts[3].tv = 1.0f;

    face->alphaSortKey = entry.baseOow;

    g_renderer.AddRP(face, 1);
    return true;
}

bool BuildBillboardRenderEntry(CPc* actor,
    const matrix& viewMatrix,
    float cameraLongitude,
    float zoom,
    CWorld::BillboardScreenEntry* outEntry)
{
    if (!actor || !outEntry) {
        return false;
    }

    if (!actor->m_isVisible) {
        return false;
    }
    if (!actor->EnsureBillboardTexture(cameraLongitude) || !actor->m_billboardTexture) {
        return false;
    }

    const float textureWidth = static_cast<float>((std::max)(1, actor->m_billboardTextureWidth));
    const float textureHeight = static_cast<float>((std::max)(1, actor->m_billboardTextureHeight));
    const float anchorX = static_cast<float>(actor->m_billboardAnchorX);
    const float anchorY = static_cast<float>((std::max)(1, actor->m_billboardAnchorY));
    if (textureWidth <= 0.0f || textureHeight <= 0.0f) {
        return false;
    }

    const int opaqueLeftPx = actor->m_billboardOpaqueBounds.left;
    const int opaqueTopPx = actor->m_billboardOpaqueBounds.top;
    const int opaqueRightPx = actor->m_billboardOpaqueBounds.right;
    const int opaqueBottomPx = actor->m_billboardOpaqueBounds.bottom;

    float leftPixels = 0.0f;
    float topPixels = 0.0f;
    float rightPixels = textureWidth;
    float bottomPixels = textureHeight;
    if (opaqueRightPx > opaqueLeftPx && opaqueBottomPx > opaqueTopPx) {
        leftPixels = static_cast<float>(opaqueLeftPx);
        topPixels = static_cast<float>(opaqueTopPx);
        rightPixels = static_cast<float>(opaqueRightPx);
        bottomPixels = static_cast<float>(opaqueBottomPx);
    }

    const float worldHeight = zoom * kPlayerBillboardWorldHeightScale;
    const float unitsPerPixel = worldHeight / anchorY;
    const float leftUnits = (anchorX - leftPixels) * unitsPerPixel;
    const float rightUnits = (rightPixels - anchorX) * unitsPerPixel;
    const float topUnits = (anchorY - topPixels) * unitsPerPixel;
    const float bottomUnits = (bottomPixels - anchorY) * unitsPerPixel;

    vector3d up = NormalizeVec3(ScaleVec3(g_renderer.m_eyeUp, -1.0f));
    vector3d right = NormalizeVec3(g_renderer.m_eyeRight);
    if (std::fabs(up.x) < 0.0001f && std::fabs(up.y) < 0.0001f && std::fabs(up.z) < 0.0001f) {
        up = vector3d{ 0.0f, -1.0f, 0.0f };
    }
    if (std::fabs(right.x) < 0.0001f && std::fabs(right.y) < 0.0001f && std::fabs(right.z) < 0.0001f) {
        right = vector3d{ 1.0f, 0.0f, 0.0f };
    }

    tlvertex3d projectedBase{};
    if (!ProjectPoint(g_renderer, viewMatrix, actor->m_pos, &projectedBase)) {
        return false;
    }

    const vector3d worldVerts[4] = {
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, topUnits)), ScaleVec3(right, -leftUnits)),
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, topUnits)), ScaleVec3(right, rightUnits)),
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, -bottomUnits)), ScaleVec3(right, -leftUnits)),
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, -bottomUnits)), ScaleVec3(right, rightUnits)),
    };

    float renderMinX = projectedBase.x;
    float renderMinY = projectedBase.y;
    float renderMaxX = projectedBase.x;
    float renderMaxY = projectedBase.y;
    for (const vector3d& worldVert : worldVerts) {
        tlvertex3d projected{};
        if (!ProjectPoint(g_renderer, viewMatrix, worldVert, &projected)) {
            return false;
        }
        renderMinX = (std::min)(renderMinX, projected.x);
        renderMinY = (std::min)(renderMinY, projected.y);
        renderMaxX = (std::max)(renderMaxX, projected.x);
        renderMaxY = (std::max)(renderMaxY, projected.y);
    }

    const float fullLeftUnits = anchorX * unitsPerPixel;
    const float fullRightUnits = (textureWidth - anchorX) * unitsPerPixel;
    const float fullTopUnits = anchorY * unitsPerPixel;
    const float fullBottomUnits = (textureHeight - anchorY) * unitsPerPixel;
    const vector3d fullWorldVerts[4] = {
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, fullTopUnits)), ScaleVec3(right, -fullLeftUnits)),
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, fullTopUnits)), ScaleVec3(right, fullRightUnits)),
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, -fullBottomUnits)), ScaleVec3(right, -fullLeftUnits)),
        AddVec3(AddVec3(actor->m_pos, ScaleVec3(up, -fullBottomUnits)), ScaleVec3(right, fullRightUnits)),
    };

    for (int index = 0; index < 4; ++index) {
        const vector3d& worldVert = fullWorldVerts[index];
        tlvertex3d projected{};
        if (!ProjectPoint(g_renderer, viewMatrix, worldVert, &projected)) {
            return false;
        }
        outEntry->renderX[index] = projected.x;
        outEntry->renderY[index] = projected.y;
    }

    outEntry->actor = actor;
    outEntry->screenY = projectedBase.y;
    outEntry->depthKey = projectedBase.oow;
    outEntry->left = renderMinX;
    outEntry->top = renderMinY;
    outEntry->right = renderMaxX;
    outEntry->bottom = renderMaxY;
    outEntry->labelX = (renderMinX + renderMaxX) * 0.5f;
    outEntry->labelY = renderMaxY;
    outEntry->baseX = projectedBase.x;
    outEntry->baseY = projectedBase.y;
    outEntry->baseZ = projectedBase.z;
    outEntry->baseOow = projectedBase.oow;
    return true;
}

CPc* FindLiveBillboardActor(const CWorld& world, u32 gid)
{
    if (gid == 0) {
        return nullptr;
    }

    if (world.m_player && world.m_player->m_gid == gid) {
        return world.m_player;
    }

    for (CGameActor* actor : world.m_actorList) {
        if (!actor || actor->m_gid != gid) {
            continue;
        }

        return dynamic_cast<CPc*>(actor);
    }

    return nullptr;
}

bool CompareBillboardRenderEntry(const CWorld::BillboardScreenEntry& lhs, const CWorld::BillboardScreenEntry& rhs)
{
    if (lhs.screenY != rhs.screenY) {
        return lhs.screenY < rhs.screenY;
    }
    return lhs.depthKey > rhs.depthKey;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeSlashPath(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

std::string BaseNameOfPath(const std::string& path)
{
    const std::string normalized = NormalizeSlashPath(path);
    const size_t slash = normalized.find_last_of('\\');
    if (slash == std::string::npos) {
        return normalized;
    }
    return normalized.substr(slash + 1);
}

const std::map<std::string, std::string>& GetResNameTableAliasesWorld()
{
    static bool s_loaded = false;
    static std::map<std::string, std::string> s_aliases;
    if (s_loaded) {
        return s_aliases;
    }

    s_loaded = true;

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData("data\\resnametable.txt", &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return s_aliases;
    }

    std::string text(reinterpret_cast<const char*>(bytes), static_cast<size_t>(size));
    delete[] bytes;

    size_t lineStart = 0;
    while (lineStart < text.size()) {
        size_t lineEnd = text.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = text.size();
        }

        std::string line = text.substr(lineStart, lineEnd - lineStart);
        lineStart = text.find_first_not_of("\r\n", lineEnd);
        if (lineStart == std::string::npos) {
            lineStart = text.size();
        }

        if (line.empty() || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
            continue;
        }

        const size_t firstHash = line.find('#');
        const size_t secondHash = firstHash == std::string::npos ? std::string::npos : line.find('#', firstHash + 1);
        if (firstHash == std::string::npos || secondHash == std::string::npos || firstHash == 0 || secondHash <= firstHash + 1) {
            continue;
        }

        std::string key = NormalizeSlashPath(line.substr(0, firstHash));
        std::string value = NormalizeSlashPath(line.substr(firstHash + 1, secondHash - firstHash - 1));
        if (!key.empty() && !value.empty()) {
            s_aliases[ToLowerAscii(key)] = value;
        }
    }

    return s_aliases;
}

std::string ResolveAliasNameWorld(const std::string& candidate)
{
    if (candidate.empty()) {
        return std::string();
    }

    const auto& aliases = GetResNameTableAliasesWorld();
    const std::string normalized = ToLowerAscii(NormalizeSlashPath(candidate));
    const auto it = aliases.find(normalized);
    if (it != aliases.end()) {
        return it->second;
    }

    const std::string candidateBase = ToLowerAscii(BaseNameOfPath(normalized));
    for (const auto& entry : aliases) {
        if (ToLowerAscii(BaseNameOfPath(entry.first)) == candidateBase) {
            return entry.second;
        }
    }

    return std::string();
}

std::string ResolveExistingPathWorld(const std::string& candidate, const std::vector<std::string>& directPrefixes)
{
    if (candidate.empty()) {
        return std::string();
    }

    const std::string normalized = NormalizeSlashPath(candidate);
    if (g_fileMgr.IsDataExist(normalized.c_str())) {
        return normalized;
    }

    for (const std::string& prefix : directPrefixes) {
        const std::string prefixed = NormalizeSlashPath(prefix + normalized);
        if (g_fileMgr.IsDataExist(prefixed.c_str())) {
            return prefixed;
        }
    }

    return std::string();
}

const std::vector<std::string>& GetDataNamesByExtensionWorld(const char* ext)
{
    static std::map<std::string, std::vector<std::string>> s_byExt;
    const std::string key = ToLowerAscii(ext ? ext : "");
    auto it = s_byExt.find(key);
    if (it != s_byExt.end()) {
        return it->second;
    }

    std::vector<std::string> names;
    g_fileMgr.CollectDataNamesByExtension(key.c_str(), names);
    auto inserted = s_byExt.emplace(key, std::move(names));
    return inserted.first->second;
}

std::string ResolveDataPathWorld(const std::string& fileName, const char* ext, const std::vector<std::string>& directPrefixes)
{
    if (fileName.empty()) {
        return std::string();
    }

    const std::string normalizedName = NormalizeSlashPath(fileName);
    for (const std::string& prefix : directPrefixes) {
        const std::string candidate = NormalizeSlashPath(prefix + normalizedName);
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }

        const std::string alias = ResolveAliasNameWorld(candidate);
        if (!alias.empty()) {
            const std::string resolvedAlias = ResolveExistingPathWorld(alias, directPrefixes);
            if (!resolvedAlias.empty()) {
                return resolvedAlias;
            }
        }
    }

    const std::string directAlias = ResolveAliasNameWorld(normalizedName);
    if (!directAlias.empty()) {
        const std::string resolvedDirectAlias = ResolveExistingPathWorld(directAlias, directPrefixes);
        if (!resolvedDirectAlias.empty()) {
            return resolvedDirectAlias;
        }
    }

    const std::string wantedBase = ToLowerAscii(BaseNameOfPath(normalizedName));
    const std::string wantedStem = wantedBase.rfind('.') != std::string::npos
        ? wantedBase.substr(0, wantedBase.rfind('.'))
        : wantedBase;
    const auto& knownNames = GetDataNamesByExtensionWorld(ext);
    for (const std::string& known : knownNames) {
        if (ToLowerAscii(BaseNameOfPath(known)) == wantedBase) {
            return known;
        }
    }

    for (const std::string& known : knownNames) {
        const std::string knownLower = ToLowerAscii(known);
        if (knownLower.find(wantedStem) != std::string::npos) {
            return known;
        }
    }

    for (const std::string& prefix : directPrefixes) {
        const std::string alias = ResolveAliasNameWorld(prefix + normalizedName);
        if (!alias.empty()) {
            const std::string aliasBase = ToLowerAscii(BaseNameOfPath(alias));
            if (aliasBase == wantedBase || aliasBase.find(wantedStem) != std::string::npos) {
                const std::string resolvedAlias = ResolveExistingPathWorld(alias, directPrefixes);
                if (!resolvedAlias.empty()) {
                    return resolvedAlias;
                }
            }
        }
    }

    return std::string();
}

std::string ResolveWaterTexturePath(int waterSet, int frameIndex)
{
    char fileName[32] = {};
    std::snprintf(fileName, sizeof(fileName), "water%d%02d.jpg", waterSet, frameIndex);

    static const char* const kWaterFolderKor =
        "data\\texture\\"
        "\xBF\xF6\xC5\xCD"
        "\\";
    const std::vector<std::string> prefixes = {
        kWaterFolderKor,
        "data\\texture\\water\\",
        "texture\\water\\",
        ""
    };
    return ResolveDataPathWorld(fileName, ".jpg", prefixes);
}

u32 ResolveWaterColor(const C3dGround& ground)
{
    if (ground.m_waterSet == 4) {
        const u8 r = static_cast<u8>((std::max)(0.0f, (std::min)(255.0f, ground.m_ambientCol.x * 255.0f)));
        const u8 g = static_cast<u8>((std::max)(0.0f, (std::min)(255.0f, ground.m_ambientCol.y * 255.0f)));
        const u8 b = static_cast<u8>((std::max)(0.0f, (std::min)(255.0f, ground.m_ambientCol.z * 255.0f)));
        return 0x90000000u | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(b);
    }

    return kWaterDefaultColor;
}

CTexture* UpdateGroundWater(C3dGround& ground)
{
    const u32 now = timeGetTime();
    if (ground.m_lastWaterAnimTick == 0) {
        ground.m_lastWaterAnimTick = now;
    }

    const u32 elapsedMs = now - ground.m_lastWaterAnimTick;
    ground.m_lastWaterAnimTick = now;

    const unsigned long long scaledElapsed = static_cast<unsigned long long>(elapsedMs) * kWaterAnimationTicksPerSecond;
    ground.m_waterAnimAccumulator += static_cast<u32>(scaledElapsed);
    const u32 waterSteps = ground.m_waterAnimAccumulator / 1000u;
    ground.m_waterAnimAccumulator %= 1000u;

    const int texAnimCycle = (std::max)(1, ground.m_texAnimCycle);
    const int totalFrames = 32 * texAnimCycle;
    if (waterSteps > 0) {
        ground.m_waterCnt = (ground.m_waterCnt + static_cast<int>(waterSteps % static_cast<u32>(totalFrames))) % totalFrames;

        long long waterOffset = static_cast<long long>(ground.m_waterOffset)
            + static_cast<long long>(ground.m_waveSpeed) * static_cast<long long>(waterSteps);
        waterOffset %= 360;
        if (waterOffset > 180) {
            waterOffset -= 360;
        }
        if (waterOffset < -180) {
            waterOffset += 360;
        }
        ground.m_waterOffset = static_cast<int>(waterOffset);
    }

    const int frameIndex = (ground.m_waterCnt / texAnimCycle) % 32;
    std::string texturePath = ResolveWaterTexturePath(ground.m_waterSet, frameIndex);
    CTexture* texture = texturePath.empty() ? nullptr : g_texMgr.GetTexture(texturePath.c_str(), false);
    if ((!texture || texture == &CTexMgr::s_dummy_texture) && ground.m_waterSet >= 0) {
        texturePath = ResolveWaterTexturePath(ground.m_waterSet % 6, frameIndex);
        texture = texturePath.empty() ? nullptr : g_texMgr.GetTexture(texturePath.c_str(), false);
    }
    ground.m_waterTex = (texture == &CTexMgr::s_dummy_texture) ? nullptr : texture;
    return ground.m_waterTex;
}

bool BuildWaterQuadHeights(const C3dGround& ground,
    int tileX,
    int tileY,
    float* outH0,
    float* outH1,
    float* outH2,
    float* outH3)
{
    if (!outH0 || !outH1 || !outH2 || !outH3) {
        return false;
    }

    if (ground.m_waveHeight == 0.0f) {
        *outH0 = ground.m_waterLevel;
        *outH1 = ground.m_waterLevel;
        *outH2 = ground.m_waterLevel;
        *outH3 = ground.m_waterLevel;
        return true;
    }

    const int diagonal = tileX + tileY;
    const int waterOffset = ground.m_waterOffset;
    const int pitch = ground.m_wavePitch;
    *outH1 = GetSin(waterOffset + pitch * diagonal) * ground.m_waveHeight + ground.m_waterLevel;
    *outH0 = GetSin(waterOffset + pitch * (tileX + tileY - 1)) * ground.m_waveHeight + ground.m_waterLevel;
    *outH3 = GetSin(waterOffset + pitch * (tileX + tileY + 1)) * ground.m_waveHeight + ground.m_waterLevel;
    *outH2 = *outH1;
    return true;
}

void SubmitWaterQuad(C3dGround& ground,
    const matrix& viewMatrix,
    int tileX,
    int tileY,
    const CGroundCell& cell)
{
    if (!ground.m_waterTex) {
        return;
    }

    float h0 = ground.m_waterLevel;
    float h1 = ground.m_waterLevel;
    float h2 = ground.m_waterLevel;
    float h3 = ground.m_waterLevel;
    if (!BuildWaterQuadHeights(ground, tileX, tileY, &h0, &h1, &h2, &h3)) {
        return;
    }

    if (!(cell.h[0] > h1 || cell.h[1] > h1 || cell.h[2] > h1 || cell.h[3] > h1)) {
        return;
    }

    const float x0 = GroundCoordX(tileX, ground.m_width, ground.m_zoom);
    const float x1 = GroundCoordX(tileX + 1, ground.m_width, ground.m_zoom);
    const float z0 = GroundCoordZ(tileY, ground.m_height, ground.m_zoom);
    const float z1 = GroundCoordZ(tileY + 1, ground.m_height, ground.m_zoom);
    const vector3d waterVerts[4] = {
        vector3d{ x0, h0, z0 },
        vector3d{ x1, h1, z0 },
        vector3d{ x0, h2, z1 },
        vector3d{ x1, h3, z1 },
    };

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = ground.m_waterTex;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0.0f;

    const u32 color = ResolveWaterColor(ground);
    for (int i = 0; i < 4; ++i) {
        if (!ProjectPoint(g_renderer, viewMatrix, waterVerts[i], &face->m_verts[i])) {
            return;
        }
        face->m_verts[i].color = color;
        face->m_verts[i].specular = 0xFF000000u;
    }

    const float baseU = static_cast<float>(tileX & 3);
    const float baseV = static_cast<float>(tileY & 3);
    face->m_verts[0].tu = baseU * kWaterTexUvScale;
    face->m_verts[0].tv = baseV * kWaterTexUvScale;
    face->m_verts[1].tu = (baseU + 1.0f) * kWaterTexUvScale;
    face->m_verts[1].tv = baseV * kWaterTexUvScale;
    face->m_verts[2].tu = baseU * kWaterTexUvScale;
    face->m_verts[2].tv = (baseV + 1.0f) * kWaterTexUvScale;
    face->m_verts[3].tu = (baseU + 1.0f) * kWaterTexUvScale;
    face->m_verts[3].tv = (baseV + 1.0f) * kWaterTexUvScale;

    face->alphaSortKey = face->m_verts[0].oow;
    g_renderer.AddRP(face, 1);
}

u32 DebugGroundColor(int textureId)
{
    static const u32 kColors[] = {
        0xFFFF4040u,
        0xFF40FF40u,
        0xFF4080FFu,
        0xFFFFFF40u,
        0xFFFF8040u,
        0xFF40FFFFu,
        0xFFFF40FFu,
        0xFFFFC040u,
        0xFF80FF40u,
        0xFF40C0FFu,
    };

    if (textureId < 0) {
        return 0xFFFF00FFu;
    }
    return kColors[static_cast<size_t>(textureId) % (sizeof(kColors) / sizeof(kColors[0]))];
}

float GroundCoordX(int x, int width, float zoom)
{
    return (static_cast<float>(x) - static_cast<float>(width) * 0.5f) * zoom;
}

float GroundCoordZ(int y, int height, float zoom)
{
    return (static_cast<float>(y) - static_cast<float>(height) * 0.5f) * zoom;
}

vector3d AttrCoord(int x, int y, int width, int height, float zoom, float cellHeight)
{
    return vector3d{
        GroundCoordX(x, width, zoom),
        cellHeight,
        GroundCoordZ(y, height, zoom)
    };
}

int GroundTileFromWorldX(float worldX, int width, float zoom)
{
    if (zoom <= 0.0f) {
        return 0;
    }
    return static_cast<int>(std::floor(worldX / zoom + static_cast<float>(width) * 0.5f));
}

int GroundTileFromWorldZ(float worldZ, int height, float zoom)
{
    if (zoom <= 0.0f) {
        return 0;
    }
    return static_cast<int>(std::floor(worldZ / zoom + static_cast<float>(height) * 0.5f));
}

int ClampInt(int value, int minValue, int maxValue)
{
    return (std::max)(minValue, (std::min)(maxValue, value));
}

vector2d ComputeGroundUvScale(const CTexture* texture)
{
    if (!texture) {
        return vector2d{ 1.0f, 1.0f };
    }

    unsigned int contentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : texture->m_w;
    unsigned int contentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : texture->m_h;
    if (contentWidth == 0 || contentHeight == 0) {
        return vector2d{ 1.0f, 1.0f };
    }

    const unsigned int surfaceWidth = texture->m_w;
    const unsigned int surfaceHeight = texture->m_h;
    if (surfaceWidth == 0 || surfaceHeight == 0) {
        return vector2d{ 1.0f, 1.0f };
    }

    return vector2d{
        static_cast<float>(contentWidth) / static_cast<float>(surfaceWidth),
        static_cast<float>(contentHeight) / static_cast<float>(surfaceHeight)
    };
}

void ResetNodeState(SceneGraphNode& node)
{
    node.m_parent = nullptr;
    for (SceneGraphNode*& child : node.m_child) {
        child = nullptr;
    }
    node.m_aabb.min = vector3d{ 0.0f, 0.0f, 0.0f };
    node.m_aabb.max = vector3d{ 0.0f, 0.0f, 0.0f };
    node.m_center = vector3d{ 0.0f, 0.0f, 0.0f };
    node.m_halfSize = vector3d{ 0.0f, 0.0f, 0.0f };
    node.m_needUpdate = 1;
    node.m_actorList.clear();
    node.m_ground = nullptr;
    SetRect(&node.m_groundArea, 0, 0, 0, 0);
    node.m_attr = nullptr;
    SetRect(&node.m_attrArea, 0, 0, 0, 0);
}

RECT ComputeGroundArea(const SceneGraphNode& node, const C3dGround& ground)
{
    RECT area{};
    const int left = GroundTileFromWorldX(node.m_aabb.min.x, ground.m_width, ground.m_zoom);
    const int right = GroundTileFromWorldX(node.m_aabb.max.x, ground.m_width, ground.m_zoom);
    const int top = GroundTileFromWorldZ(node.m_aabb.min.z, ground.m_height, ground.m_zoom);
    const int bottom = GroundTileFromWorldZ(node.m_aabb.max.z, ground.m_height, ground.m_zoom);
    const int areaLeft = ClampInt(left, 0, ground.m_width);
    const int areaTop = ClampInt(top, 0, ground.m_height);
    area.left = areaLeft;
    area.right = ClampInt((std::max)(areaLeft + 1, right + 1), 0, ground.m_width);
    area.top = areaTop;
    area.bottom = ClampInt((std::max)(areaTop + 1, bottom + 1), 0, ground.m_height);
    return area;
}

RECT ComputeAttrArea(const SceneGraphNode& node, const C3dAttr& attr)
{
    RECT area{};
    const float zoom = static_cast<float>(attr.m_zoom);
    const int left = GroundTileFromWorldX(node.m_aabb.min.x, attr.m_width, zoom);
    const int right = GroundTileFromWorldX(node.m_aabb.max.x, attr.m_width, zoom);
    const int top = GroundTileFromWorldZ(node.m_aabb.min.z, attr.m_height, zoom);
    const int bottom = GroundTileFromWorldZ(node.m_aabb.max.z, attr.m_height, zoom);
    const int areaLeft = ClampInt(left, 0, attr.m_width);
    const int areaTop = ClampInt(top, 0, attr.m_height);
    area.left = areaLeft;
    area.right = ClampInt((std::max)(areaLeft + 1, right + 1), 0, attr.m_width);
    area.top = areaTop;
    area.bottom = ClampInt((std::max)(areaTop + 1, bottom + 1), 0, attr.m_height);
    return area;
}

bool ContainsPointXZ(const SceneGraphNode& node, float x, float z)
{
    return x >= node.m_aabb.min.x && x <= node.m_aabb.max.x && z >= node.m_aabb.min.z && z <= node.m_aabb.max.z;
}

void ComputeHeightRangeFromAttr(const C3dAttr& attr, float* outMinY, float* outMaxY)
{
    if (!outMinY || !outMaxY || attr.m_cells.empty()) {
        return;
    }

    bool firstHeight = true;
    for (const CAttrCell& cell : attr.m_cells) {
        const float heights[4] = { cell.h1, cell.h2, cell.h3, cell.h4 };
        for (float height : heights) {
            if (firstHeight) {
                *outMinY = *outMaxY = height;
                firstHeight = false;
            } else {
                *outMinY = (std::min)(*outMinY, height);
                *outMaxY = (std::max)(*outMaxY, height);
            }
        }
    }
}

bool ReadAttrI32(const unsigned char* buffer, size_t size, size_t* offset, int* outValue)
{
    if (!offset || !outValue || *offset + sizeof(int) > size) {
        return false;
    }
    std::memcpy(outValue, buffer + *offset, sizeof(int));
    *offset += sizeof(int);
    return true;
}

bool ReadAttrF32(const unsigned char* buffer, size_t size, size_t* offset, float* outValue)
{
    if (!offset || !outValue || *offset + sizeof(float) > size) {
        return false;
    }
    std::memcpy(outValue, buffer + *offset, sizeof(float));
    *offset += sizeof(float);
    return true;
}

vector3d SubtractVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x - b.x, a.y - b.y, a.z - b.z };
}

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

vector3d NormalizeVec3(const vector3d& value)
{
    const float lengthSq = DotVec3(value, value);
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

vector3d ScaleVec3(const vector3d& value, float scale)
{
    return vector3d{ value.x * scale, value.y * scale, value.z * scale };
}

vector3d AddVec3(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x + b.x, a.y + b.y, a.z + b.z };
}

vector3d TransformPoint(const matrix& m, const vector3d& point)
{
    return vector3d{
        point.x * m.m[0][0] + point.y * m.m[1][0] + point.z * m.m[2][0] + m.m[3][0],
        point.x * m.m[0][1] + point.y * m.m[1][1] + point.z * m.m[2][1] + m.m[3][1],
        point.x * m.m[0][2] + point.y * m.m[1][2] + point.z * m.m[2][2] + m.m[3][2]
    };
}

bool ProjectPoint(const CRenderer& renderer, const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
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

u32 ModulateColor(u32 color, const vector3d& light)
{
    const unsigned int r = (color >> 16) & 0xFFu;
    const unsigned int g = (color >> 8) & 0xFFu;
    const unsigned int b = color & 0xFFu;

    const unsigned int outR = static_cast<unsigned int>((std::min)(255.0f, r * light.x));
    const unsigned int outG = static_cast<unsigned int>((std::min)(255.0f, g * light.y));
    const unsigned int outB = static_cast<unsigned int>((std::min)(255.0f, b * light.z));
    return 0xFF000000u | (outR << 16) | (outG << 8) | outB;
}

u32 ResolveSurfaceColor(const CGroundSurface& surface)
{
    const u32 color = surface.color & 0x00FFFFFFu;
    if (color == 0) {
        return 0xFFC8C8C8u;
    }
    return 0xFF000000u | color;
}

u8 Expand5To8(u8 value)
{
    return static_cast<u8>((value << 3) | (value >> 2));
}

void Unpack5BitChannel(const ColorChannel& channel, std::array<u8, 64>* outValues)
{
    if (!outValues) {
        return;
    }

    for (int i = 0; i < 64; ++i) {
        const int bitOffset = i * 5;
        const int byteOffset = bitOffset / 8;
        const int bitShift = bitOffset % 8;

        unsigned int packed = channel.m_buffer[byteOffset];
        if (byteOffset + 1 < static_cast<int>(sizeof(channel.m_buffer))) {
            packed |= static_cast<unsigned int>(channel.m_buffer[byteOffset + 1]) << 8;
        }
        if (byteOffset + 2 < static_cast<int>(sizeof(channel.m_buffer))) {
            packed |= static_cast<unsigned int>(channel.m_buffer[byteOffset + 2]) << 16;
        }

        (*outValues)[static_cast<size_t>(i)] = Expand5To8(static_cast<u8>((packed >> bitShift) & 0x1Fu));
    }
}

bool BuildLmInfo(const CGndRes& gnd, int index, CLMInfo* outInfo)
{
    if (!outInfo || index < 0 || index >= gnd.m_numLightmap) {
        return false;
    }

    std::memset(outInfo, 0, sizeof(*outInfo));
    if (gnd.m_verMinor == 7) {
        const size_t offset = static_cast<size_t>(index) * sizeof(CLMInfo);
        if (offset + sizeof(CLMInfo) > gnd.m_lminfoRaw.size()) {
            return false;
        }
        std::memcpy(outInfo, gnd.m_lminfoRaw.data() + offset, sizeof(CLMInfo));
        return true;
    }

    if (index >= static_cast<int>(gnd.m_lmindex.size())) {
        return false;
    }

    const LMIndex& lmIndex = gnd.m_lmindex[static_cast<size_t>(index)];
    const int channelIds[4] = { lmIndex.a, lmIndex.r, lmIndex.g, lmIndex.b };
    for (int channelId : channelIds) {
        if (channelId < 0 || channelId >= static_cast<int>(gnd.m_colorchannel.size())) {
            return false;
        }
    }

    std::array<u8, 64> channels[4];
    Unpack5BitChannel(gnd.m_colorchannel[static_cast<size_t>(lmIndex.a)], &channels[0]);
    Unpack5BitChannel(gnd.m_colorchannel[static_cast<size_t>(lmIndex.r)], &channels[1]);
    Unpack5BitChannel(gnd.m_colorchannel[static_cast<size_t>(lmIndex.g)], &channels[2]);
    Unpack5BitChannel(gnd.m_colorchannel[static_cast<size_t>(lmIndex.b)], &channels[3]);

    u8* intensity = &outInfo->idata[0][0];
    u8* surface = &outInfo->sdata[0][0][0];
    for (int i = 0; i < 64; ++i) {
        const int dst = (i / 8) + 8 * (i % 8);
        intensity[dst] = channels[0][static_cast<size_t>(i)];
        surface[dst * 3 + 0] = channels[1][static_cast<size_t>(i)];
        surface[dst * 3 + 1] = channels[2][static_cast<size_t>(i)];
        surface[dst * 3 + 2] = channels[3][static_cast<size_t>(i)];
    }
    return true;
}

u32 PackColor(u8 a, u8 r, u8 g, u8 b)
{
    return (static_cast<u32>(a) << 24)
        | (static_cast<u32>(r) << 16)
        | (static_cast<u32>(g) << 8)
        | static_cast<u32>(b);
}

u32 SampleLmIntensity(const CLMInfo& lminfo, int y, int x)
{
    return PackColor(
        lminfo.idata[y][x],
        lminfo.sdata[y][x][0],
        lminfo.sdata[y][x][1],
        lminfo.sdata[y][x][2]);
}

CTexture* GetDebugGroundTestTexture()
{
    static CTexture* s_debugTexture = nullptr;
    if (s_debugTexture) {
        return s_debugTexture;
    }

    constexpr int kSize = 64;
    std::vector<unsigned long> pixels(static_cast<size_t>(kSize) * static_cast<size_t>(kSize));
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const bool major = ((x / 16) + (y / 16)) % 2 == 0;
            const bool minor = ((x / 4) + (y / 4)) % 2 == 0;
            unsigned long color = major ? 0xFFFFF0F0u : 0xFF101010u;
            if (minor) {
                color = major ? 0xFF20C020u : 0xFFC02020u;
            }
            pixels[static_cast<size_t>(y) * static_cast<size_t>(kSize) + static_cast<size_t>(x)] = color;
        }
    }

    s_debugTexture = g_texMgr.CreateTexture(kSize, kSize, pixels.data(), PF_A8R8G8B8, false);
    if (s_debugTexture) {
        std::strncpy(s_debugTexture->m_texName, "__debug_ground_checker__", sizeof(s_debugTexture->m_texName) - 1);
        s_debugTexture->m_texName[sizeof(s_debugTexture->m_texName) - 1] = '\0';
    }
    return s_debugTexture;
}
}

CLightmapMgr::CLightmapMgr()
    : m_numLightmaps(0), m_numLmSurfaces(0)
{
}

CLightmapMgr::~CLightmapMgr()
{
    Reset();
}

void CLightmapMgr::Reset()
{
    for (CTexture* surface : m_lmSurfaces) {
        delete surface;
    }
    m_lmSurfaces.clear();
    m_lightmaps.clear();
    m_numLightmaps = 0;
    m_numLmSurfaces = 0;
}

bool CLightmapMgr::Create(const CGndRes& gnd)
{
    Reset();
    if (gnd.m_numLightmap <= 0) {
        return true;
    }

    m_lightmaps.resize(static_cast<size_t>(gnd.m_numLightmap));
    m_numLightmaps = gnd.m_numLightmap;
    const int atlasCount = (gnd.m_numLightmap + kLightmapsPerAtlas - 1) / kLightmapsPerAtlas;
    m_lmSurfaces.reserve(static_cast<size_t>(atlasCount));

    for (int atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex) {
        std::vector<unsigned long> atlasPixels(static_cast<size_t>(kLightmapAtlasEdge) * static_cast<size_t>(kLightmapAtlasEdge), 0xFFFFFFFFu);
        const int atlasStart = atlasIndex * kLightmapsPerAtlas;
        const int atlasEnd = (std::min)(atlasStart + kLightmapsPerAtlas, gnd.m_numLightmap);

        for (int lightmapIndex = atlasStart; lightmapIndex < atlasEnd; ++lightmapIndex) {
            CLMInfo lminfo{};
            if (!BuildLmInfo(gnd, lightmapIndex, &lminfo)) {
                continue;
            }

            const int atlasLocalIndex = lightmapIndex - atlasStart;
            const int atlasU = atlasLocalIndex % kLightmapsPerAtlasRow;
            const int atlasV = atlasLocalIndex / kLightmapsPerAtlasRow;
            const int baseX = atlasU * kLightmapEdge;
            const int baseY = atlasV * kLightmapEdge;

            for (int y = 0; y < kLightmapEdge; ++y) {
                for (int x = 0; x < kLightmapEdge; ++x) {
                    const size_t pixelIndex = static_cast<size_t>(baseY + y) * static_cast<size_t>(kLightmapAtlasEdge)
                        + static_cast<size_t>(baseX + x);
                    const u8 intensity = lminfo.idata[y][x];
                    atlasPixels[pixelIndex] = PackColor(intensity, intensity, intensity, intensity);
                }
            }

            CLightmap& lightmap = m_lightmaps[static_cast<size_t>(lightmapIndex)];
            lightmap.surface = nullptr;
            lightmap.brightObj[0] = lminfo.sdata[4][4][0];
            lightmap.brightObj[1] = lminfo.sdata[4][4][1];
            lightmap.brightObj[2] = lminfo.sdata[4][4][2];
            lightmap.intensity[0] = SampleLmIntensity(lminfo, 1, 1);
            lightmap.intensity[1] = SampleLmIntensity(lminfo, 1, 5);
            lightmap.intensity[2] = SampleLmIntensity(lminfo, 5, 1);
            lightmap.intensity[3] = SampleLmIntensity(lminfo, 5, 5);

            const float u0 = static_cast<float>(baseX + 1) / static_cast<float>(kLightmapAtlasEdge);
            const float u1 = static_cast<float>(baseX + 7) / static_cast<float>(kLightmapAtlasEdge);
            const float v0 = static_cast<float>(baseY + 1) / static_cast<float>(kLightmapAtlasEdge);
            const float v1 = static_cast<float>(baseY + 7) / static_cast<float>(kLightmapAtlasEdge);
            lightmap.coor[0] = vector2d{ u0, v0 };
            lightmap.coor[1] = vector2d{ u1, v0 };
            lightmap.coor[2] = vector2d{ u0, v1 };
            lightmap.coor[3] = vector2d{ u1, v1 };
        }

        CTexture* atlasTexture = g_texMgr.CreateTexture(
            kLightmapAtlasEdge,
            kLightmapAtlasEdge,
            atlasPixels.data(),
            PF_A8R8G8B8,
            false);
        if (!atlasTexture) {
            Reset();
            return false;
        }
        m_lmSurfaces.push_back(atlasTexture);
    }

    m_numLmSurfaces = static_cast<int>(m_lmSurfaces.size());
    for (int lightmapIndex = 0; lightmapIndex < m_numLightmaps; ++lightmapIndex) {
        const int atlasIndex = lightmapIndex / kLightmapsPerAtlas;
        if (atlasIndex >= 0 && atlasIndex < m_numLmSurfaces) {
            m_lightmaps[static_cast<size_t>(lightmapIndex)].surface = m_lmSurfaces[static_cast<size_t>(atlasIndex)];
        }
    }

    return true;
}

const CLightmapMgr::CLightmap* CLightmapMgr::GetLightmap(int index) const
{
    if (index < 0 || index >= m_numLightmaps || m_lightmaps.empty()) {
        return nullptr;
    }
    return &m_lightmaps[static_cast<size_t>(index)];
}

SceneGraphNode::SceneGraphNode()
{
    ResetNodeState(*this);
}

SceneGraphNode::~SceneGraphNode()
{
    ClearChildren();
}

void SceneGraphNode::ClearChildren()
{
    for (SceneGraphNode*& child : m_child) {
        delete child;
        child = nullptr;
    }
}

void SceneGraphNode::Build(int level, int maxLevel)
{
    m_center.x = (m_aabb.min.x + m_aabb.max.x) * 0.5f;
    m_center.z = (m_aabb.min.z + m_aabb.max.z) * 0.5f;
    m_center.y = 0.0f;
    m_halfSize = vector3d{
        (m_aabb.max.x - m_aabb.min.x) * 0.5f,
        (m_aabb.max.y - m_aabb.min.y) * 0.5f,
        (m_aabb.max.z - m_aabb.min.z) * 0.5f
    };
    m_needUpdate = 0;

    if (level >= maxLevel) {
        return;
    }

    const struct {
        float minX;
        float maxX;
        float minZ;
        float maxZ;
    } quadrants[4] = {
        { m_aabb.min.x, m_center.x, m_aabb.min.z, m_center.z },
        { m_center.x, m_aabb.max.x, m_aabb.min.z, m_center.z },
        { m_aabb.min.x, m_center.x, m_center.z, m_aabb.max.z },
        { m_center.x, m_aabb.max.x, m_center.z, m_aabb.max.z },
    };

    for (int i = 0; i < 4; ++i) {
        if (!m_child[i]) {
            m_child[i] = new SceneGraphNode();
        }
        SceneGraphNode* child = m_child[i];
        child->m_parent = this;
        child->m_aabb.min = vector3d{ quadrants[i].minX, m_aabb.min.y, quadrants[i].minZ };
        child->m_aabb.max = vector3d{ quadrants[i].maxX, m_aabb.max.y, quadrants[i].maxZ };
        child->Build(level + 1, maxLevel);
    }
}

void SceneGraphNode::InsertGround(C3dGround* ground, int level, int maxLevel)
{
    m_ground = ground;
    if (ground) {
        m_groundArea = ComputeGroundArea(*this, *ground);
    } else {
        SetRect(&m_groundArea, 0, 0, 0, 0);
    }

    if (level >= maxLevel) {
        return;
    }

    for (SceneGraphNode* child : m_child) {
        if (child) {
            child->InsertGround(ground, level + 1, maxLevel);
        }
    }
}

void SceneGraphNode::InsertAttr(C3dAttr* attr, int level, int maxLevel)
{
    m_attr = attr;
    if (attr) {
        m_attrArea = ComputeAttrArea(*this, *attr);
    } else {
        SetRect(&m_attrArea, 0, 0, 0, 0);
    }

    if (level >= maxLevel) {
        return;
    }

    for (SceneGraphNode* child : m_child) {
        if (child) {
            child->InsertAttr(attr, level + 1, maxLevel);
        }
    }
}

void SceneGraphNode::InsertActor(CGameActor* actor, float x, float z)
{
    if (!actor) {
        return;
    }

    for (SceneGraphNode* child : m_child) {
        if (child && ContainsPointXZ(*child, x, z)) {
            child->InsertActor(actor, x, z);
            return;
        }
    }

    if (std::find(m_actorList.begin(), m_actorList.end(), actor) == m_actorList.end()) {
        m_actorList.push_back(actor);
    }
}

bool SceneGraphNode::RemoveActor(CGameActor* actor)
{
    if (!actor) {
        return false;
    }

    const auto it = std::find(m_actorList.begin(), m_actorList.end(), actor);
    if (it != m_actorList.end()) {
        m_actorList.erase(it);
        return true;
    }

    for (SceneGraphNode* child : m_child) {
        if (child && child->RemoveActor(actor)) {
            return true;
        }
    }

    return false;
}

std::vector<CGameActor*>* SceneGraphNode::GetActorList(float x, float z)
{
    return &FindLeaf(x, z)->m_actorList;
}

const std::vector<CGameActor*>* SceneGraphNode::GetActorList(float x, float z) const
{
    return &FindLeaf(x, z)->m_actorList;
}

SceneGraphNode* SceneGraphNode::FindLeaf(float x, float z)
{
    for (SceneGraphNode* child : m_child) {
        if (child && ContainsPointXZ(*child, x, z)) {
            return child->FindLeaf(x, z);
        }
    }
    return this;
}

const SceneGraphNode* SceneGraphNode::FindLeaf(float x, float z) const
{
    for (const SceneGraphNode* child : m_child) {
        if (child && ContainsPointXZ(*child, x, z)) {
            return child->FindLeaf(x, z);
        }
    }
    return this;
}

C3dAttr::C3dAttr()
{
    Reset();
}

C3dAttr::~C3dAttr() = default;

bool C3dAttr::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    (void)fName;
    Reset();
    if (!buffer || size < 14 || std::memcmp(buffer, "GRAT", 4) != 0) {
        return false;
    }

    if (buffer[4] != 1 || buffer[5] > 2) {
        return false;
    }

    size_t offset = 6;
    if (!ReadAttrI32(buffer, static_cast<size_t>(size), &offset, &m_width)
        || !ReadAttrI32(buffer, static_cast<size_t>(size), &offset, &m_height)
        || m_width <= 0 || m_height <= 0) {
        return false;
    }

    const size_t cellCount = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    const size_t requiredBytes = offset + cellCount * 20u;
    if (requiredBytes > static_cast<size_t>(size)) {
        return false;
    }

    m_cells.resize(cellCount);
    for (size_t i = 0; i < cellCount; ++i) {
        CAttrCell cell{};
        if (!ReadAttrF32(buffer, static_cast<size_t>(size), &offset, &cell.h1)
            || !ReadAttrF32(buffer, static_cast<size_t>(size), &offset, &cell.h2)
            || !ReadAttrF32(buffer, static_cast<size_t>(size), &offset, &cell.h3)
            || !ReadAttrF32(buffer, static_cast<size_t>(size), &offset, &cell.h4)
            || !ReadAttrI32(buffer, static_cast<size_t>(size), &offset, &cell.flag)) {
            return false;
        }
        m_cells[i] = cell;
    }

    return true;
}

CRes* C3dAttr::Clone()
{
    return new C3dAttr();
}

void C3dAttr::Reset()
{
    m_width = 0;
    m_height = 0;
    m_zoom = 5;
    m_cells.clear();
}

float C3dAttr::GetHeight(float x, float z) const
{
    if (m_width <= 0 || m_height <= 0 || m_zoom <= 0 || m_cells.empty()) {
        return 0.0f;
    }

    const float zoom = static_cast<float>(m_zoom);
    const float localX = static_cast<float>(m_width) * zoom * 0.5f + x;
    const float localZ = static_cast<float>(m_height) * zoom * 0.5f + z;
    const int cellX = static_cast<int>(localX / zoom);
    const int cellZ = static_cast<int>(localZ / zoom);
    if (cellX < 0 || cellX >= m_width || cellZ < 0 || cellZ >= m_height) {
        return 0.0f;
    }

    const CAttrCell& cell = m_cells[static_cast<size_t>(cellZ) * static_cast<size_t>(m_width) + static_cast<size_t>(cellX)];
    const float fx = localX - static_cast<float>(cellX) * zoom;
    const float fz = localZ - static_cast<float>(cellZ) * zoom;
    const float left = (cell.h3 - cell.h1) * fz / zoom + cell.h1;
    const float right = (cell.h4 - cell.h2) * fz / zoom + cell.h2;
    return fx * (right - left) / zoom + left;
}

C3dGround::~C3dGround()
{
    m_lightmapMgr.Reset();
}

bool C3dGround::AssignGnd(const CGndRes& gnd, const vector3d& lightDir, const vector3d& diffuseCol, const vector3d& ambientCol)
{
    m_attr = nullptr;
    m_width = gnd.m_width;
    m_height = gnd.m_height;
    m_zoom = gnd.m_zoom;
    m_numSurfaces = gnd.m_numSurface;
    m_isNewVer = gnd.m_newVer ? 1 : 0;
    m_waterLevel = 0.0f;
    m_texAnimCycle = 3;
    m_wavePitch = 50;
    m_waveSpeed = 2;
    m_waterSet = 0;
    m_waveHeight = 1.0f;
    m_waterTex = nullptr;
    m_pBumpMap = nullptr;
    m_waterCnt = 0;
    m_waterOffset = 0;
    m_lastWaterAnimTick = 0;
    m_waterAnimAccumulator = 0;
    m_waterType = 0;
    m_waterAnimSpeed = 3;
    m_lightDir = lightDir;
    m_diffuseCol = diffuseCol;
    m_ambientCol = ambientCol;
    m_textureNames = gnd.m_texNameTable;
    m_surfaces.clear();
    m_cells.clear();
    m_lightmapMgr.Reset();

    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    const size_t cellCount = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    m_cells.resize(cellCount);
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const GndCellFmt17* srcCell = gnd.GetCell(x, y);
            if (!srcCell) {
                continue;
            }

            CGroundCell& dstCell = m_cells[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
            for (int i = 0; i < 4; ++i) {
                dstCell.h[i] = srcCell->height[i];
            }
            dstCell.topSurfaceId = srcCell->topSurfaceId;
            dstCell.frontSurfaceId = srcCell->frontSurfaceId;
            dstCell.rightSurfaceId = srcCell->rightSurfaceId;

            const float x0 = GroundCoordX(x, m_width, m_zoom);
            const float x1 = GroundCoordX(x + 1, m_width, m_zoom);
            const float z0 = GroundCoordZ(y, m_height, m_zoom);
            const float z1 = GroundCoordZ(y + 1, m_height, m_zoom);

            dstCell.watervert[0] = vector3d{ x0, 0.0f, z0 };
            dstCell.watervert[1] = vector3d{ x1, 0.0f, z0 };
            dstCell.watervert[2] = vector3d{ x0, 0.0f, z1 };
            dstCell.watervert[3] = vector3d{ x1, 0.0f, z1 };
        }
    }

    m_surfaces.resize(static_cast<size_t>(m_numSurfaces));
    for (int surfaceIndex = 0; surfaceIndex < m_numSurfaces; ++surfaceIndex) {
        const GndSurfaceFmt* srcSurface = gnd.GetSurface(surfaceIndex);
        if (!srcSurface) {
            continue;
        }

        CGroundSurface& dstSurface = m_surfaces[static_cast<size_t>(surfaceIndex)];
        dstSurface.textureId = srcSurface->textureId;
        dstSurface.lightmapId = srcSurface->lightmapId;
        dstSurface.color = srcSurface->color;
        dstSurface.tex = nullptr;
        dstSurface.lmtex = nullptr;
        if (srcSurface->textureId >= 0 && srcSurface->textureId < static_cast<int>(m_textureNames.size())) {
            dstSurface.tex = g_texMgr.GetTexture(m_textureNames[static_cast<size_t>(srcSurface->textureId)].c_str(), false);
        }

        const vector2d uvScale = ComputeGroundUvScale(dstSurface.tex);

        for (int i = 0; i < 4; ++i) {
            dstSurface.lmuv[i] = vector2d{ 0.0f, 0.0f };
        }

        for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex) {
            dstSurface.vertex[vertexIndex].uv.x = srcSurface->u[vertexIndex] * uvScale.x;
            dstSurface.vertex[vertexIndex].uv.y = srcSurface->v[vertexIndex] * uvScale.y;
        }
    }

    if (!m_lightmapMgr.Create(gnd)) {
        return false;
    }

    for (CGroundSurface& surface : m_surfaces) {
        const CLightmapMgr::CLightmap* lightmap = m_lightmapMgr.GetLightmap(surface.lightmapId);
        if (!lightmap) {
            continue;
        }
        surface.lmtex = lightmap->surface;
        for (int vertexIndex = 0; vertexIndex < 4; ++vertexIndex) {
            surface.lmuv[vertexIndex] = lightmap->coor[vertexIndex];
        }
    }

    static bool loggedGroundTexturePointers = false;
    if (!loggedGroundTexturePointers) {
        loggedGroundTexturePointers = true;
        int validTextureCount = 0;
        int dummyTextureCount = 0;
        int nullTextureCount = 0;
        int lightmapTextureCount = 0;
        for (const CGroundSurface& surface : m_surfaces) {
            if (!surface.tex) {
                ++nullTextureCount;
            } else if (surface.tex == &CTexMgr::s_dummy_texture) {
                ++dummyTextureCount;
            } else {
                ++validTextureCount;
            }

            if (surface.lmtex) {
                ++lightmapTextureCount;
            }
        }

        if constexpr (kLogGnd) {
            DbgLog("[GND] surface-textures valid=%d dummy=%d null=%d lm=%d dummyPtr=%p\n",
                validTextureCount,
                dummyTextureCount,
                nullTextureCount,
                lightmapTextureCount,
                static_cast<void*>(&CTexMgr::s_dummy_texture));
        }

        const int sampleCount = (std::min)(m_numSurfaces, 12);
        for (int i = 0; i < sampleCount; ++i) {
            const CGroundSurface& surface = m_surfaces[static_cast<size_t>(i)];
            if constexpr (kLogGnd) {
                DbgLog("[GND] surface[%d] textureId=%d tex=%p pdds=%p lmId=%d lmTex=%p\n",
                    i,
                    surface.textureId,
                    static_cast<void*>(surface.tex),
                    surface.tex ? static_cast<void*>(surface.tex->m_pddsSurface) : nullptr,
                    surface.lightmapId,
                    static_cast<void*>(surface.lmtex));
            }
        }
    }

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const CGroundCell& cell = m_cells[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
            if (cell.topSurfaceId < 0 || cell.topSurfaceId >= m_numSurfaces) {
                continue;
            }

            CGroundSurface& surface = m_surfaces[static_cast<size_t>(cell.topSurfaceId)];
            const float x0 = GroundCoordX(x, m_width, m_zoom);
            const float x1 = GroundCoordX(x + 1, m_width, m_zoom);
            const float z0 = GroundCoordZ(y, m_height, m_zoom);
            const float z1 = GroundCoordZ(y + 1, m_height, m_zoom);

            surface.vertex[0].wvert = vector3d{ x0, cell.h[0], z0 };
            surface.vertex[1].wvert = vector3d{ x1, cell.h[1], z0 };
            surface.vertex[2].wvert = vector3d{ x0, cell.h[2], z1 };
            surface.vertex[3].wvert = vector3d{ x1, cell.h[3], z1 };
        }
    }

    static bool loggedTextureUsage = false;
    if (!loggedTextureUsage) {
        loggedTextureUsage = true;

        if constexpr (kLogGnd) {
            DbgLog("[GND] texture-count=%d surface-count=%d cell-count=%d\n",
                static_cast<int>(m_textureNames.size()),
                m_numSurfaces,
                m_width * m_height);
        }

        const int textureNameLogCount = (std::min)(static_cast<int>(m_textureNames.size()), 12);
        for (int i = 0; i < textureNameLogCount; ++i) {
            if constexpr (kLogGnd) {
                DbgLog("[GND] texture[%d]='%s'\n", i, m_textureNames[static_cast<size_t>(i)].c_str());
            }
        }

        std::vector<int> usedTextureIds;
        std::vector<unsigned char> usedFlags(m_textureNames.size(), 0);
        for (const CGroundCell& cell : m_cells) {
            if (cell.topSurfaceId < 0 || cell.topSurfaceId >= m_numSurfaces) {
                continue;
            }

            const CGroundSurface& surface = m_surfaces[static_cast<size_t>(cell.topSurfaceId)];
            if (surface.textureId < 0 || surface.textureId >= static_cast<int>(usedFlags.size())) {
                continue;
            }

            if (!usedFlags[static_cast<size_t>(surface.textureId)]) {
                usedFlags[static_cast<size_t>(surface.textureId)] = 1;
                usedTextureIds.push_back(surface.textureId);
            }
        }

        if constexpr (kLogGnd) {
            DbgLog("[GND] unique-top-texture-ids=%d\n", static_cast<int>(usedTextureIds.size()));
        }
        const int usedTextureLogCount = (std::min)(static_cast<int>(usedTextureIds.size()), 12);
        for (int i = 0; i < usedTextureLogCount; ++i) {
            const int textureId = usedTextureIds[static_cast<size_t>(i)];
            const char* textureName = (textureId >= 0 && textureId < static_cast<int>(m_textureNames.size()))
                ? m_textureNames[static_cast<size_t>(textureId)].c_str()
                : "(out-of-range)";
            if constexpr (kLogGnd) {
                DbgLog("[GND] used-top-texture[%d]=id:%d name='%s'\n", i, textureId, textureName);
            }
        }
    }

    return true;
}

void C3dGround::SetWaterInfo(float waterLevel, int waterType, int waterAnimSpeed, int wavePitch, int waveSpeed, float waveHeight)
{
    m_waterLevel = waterLevel;
    m_texAnimCycle = (std::max)(1, waterAnimSpeed);
    m_wavePitch = wavePitch;
    m_waveHeight = waveHeight;
    m_waveSpeed = waveSpeed % 360;
    m_waterSet = waterType;
    m_waterType = waterType;
    m_waterAnimSpeed = waterAnimSpeed;
    m_waterTex = nullptr;
    m_waterCnt = 0;
    m_waterOffset = 0;
    m_lastWaterAnimTick = 0;
    m_waterAnimAccumulator = 0;
}

const CGroundCell* C3dGround::GetCell(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height || m_cells.empty()) {
        return nullptr;
    }
    return &m_cells[static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x)];
}

const CGroundSurface* C3dGround::GetSurface(int index) const
{
    if (index < 0 || index >= m_numSurfaces || m_surfaces.empty()) {
        return nullptr;
    }
    return &m_surfaces[static_cast<size_t>(index)];
}

void C3dGround::RenderAttrTile(const matrix& viewMatrix, int attrX, int attrY, u32 color) const
{
    if (!m_attr) {
        return;
    }

    if (attrX < 0 || attrY < 0 || attrX >= m_attr->m_width || attrY >= m_attr->m_height) {
        return;
    }

    const CGroundCell* groundCell = GetCell(attrX / 2, attrY / 2);
    if (!groundCell || groundCell->topSurfaceId < 0) {
        return;
    }

    const CGroundSurface* topSurface = GetSurface(groundCell->topSurfaceId);
    if (!topSurface) {
        return;
    }

    const CAttrCell& attrCell = m_attr->m_cells[static_cast<size_t>(attrY) * static_cast<size_t>(m_attr->m_width) + static_cast<size_t>(attrX)];
    vector3d worldVerts[4] = {
        topSurface->vertex[0].wvert,
        topSurface->vertex[1].wvert,
        topSurface->vertex[2].wvert,
        topSurface->vertex[3].wvert,
    };

    worldVerts[0].y = attrCell.h1;
    worldVerts[1].y = attrCell.h2;
    worldVerts[2].y = attrCell.h3;
    worldVerts[3].y = attrCell.h4;

    const float leftX = worldVerts[0].x;
    const float rightX = worldVerts[1].x;
    const float topZ = worldVerts[0].z;
    const float bottomZ = worldVerts[2].z;
    for (int i = 0; i < 4; ++i) {
        if (attrX & 1) {
            if (worldVerts[i].x < rightX) {
                worldVerts[i].x = (rightX - worldVerts[i].x) * 0.5f + worldVerts[i].x;
            }
        } else if (worldVerts[i].x > leftX) {
            worldVerts[i].x = worldVerts[i].x - (worldVerts[i].x - leftX) * 0.5f;
        }

        if (attrY & 1) {
            if (worldVerts[i].z < bottomZ) {
                worldVerts[i].z = (bottomZ - worldVerts[i].z) * 0.5f + worldVerts[i].z;
            }
        } else if (worldVerts[i].z > topZ) {
            worldVerts[i].z = worldVerts[i].z - (worldVerts[i].z - topZ) * 0.5f;
        }
    }

    RPFace* face = g_renderer.BorrowNullRP();
    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = nullptr;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0.0f;

    tlvertex3d* verts = face->m_verts;
    for (int i = 0; i < 4; ++i) {
        if (!ProjectPoint(g_renderer, viewMatrix, worldVerts[i], &verts[i])) {
            return;
        }
        verts[i].z = (std::max)(0.0f, verts[i].z - kAttrTileDepthBias);
        verts[i].color = color;
        verts[i].specular = 0xFF000000u;
    }

    static const char* kGridTextureCandidates[] = {
        "grid.tga",
        "texture\\grid.tga",
        "data\\texture\\grid.tga",
        "effect\\grid.tga",
        nullptr
    };

    CTexture* gridTexture = nullptr;
    for (int i = 0; kGridTextureCandidates[i] != nullptr; ++i) {
        CTexture* candidate = g_texMgr.GetTexture(kGridTextureCandidates[i], false);
        if (candidate && candidate != &CTexMgr::s_dummy_texture) {
            gridTexture = candidate;
            break;
        }
    }
    if (!gridTexture) {
        return;
    }

    const float uAdjust = gridTexture->m_w > 0 ? static_cast<float>(gridTexture->m_surfaceUpdateWidth > 0 ? gridTexture->m_surfaceUpdateWidth : gridTexture->m_w) / static_cast<float>(gridTexture->m_w) : 1.0f;
    const float vAdjust = gridTexture->m_h > 0 ? static_cast<float>(gridTexture->m_surfaceUpdateHeight > 0 ? gridTexture->m_surfaceUpdateHeight : gridTexture->m_h) / static_cast<float>(gridTexture->m_h) : 1.0f;
    static const vector2d kUvAll[4] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 0.0f, 1.0f },
        { 1.0f, 1.0f },
    };
    for (int i = 0; i < 4; ++i) {
        verts[i].tu = uAdjust * kUvAll[i].x;
        verts[i].tv = vAdjust * kUvAll[i].y;
    }

    face->tex = gridTexture;
    g_renderer.AddRP(face, 1);
}

void C3dGround::FlushGround(const matrix& viewMatrix)
{
    if (m_width <= 0 || m_height <= 0 || m_cells.empty()) {
        return;
    }

    UpdateGroundWater(*this);

    int minTileX = 0;
    int maxTileX = m_width - 1;
    int minTileY = 0;
    int maxTileY = m_height - 1;

    int focusTileX = -1;
    int focusTileY = -1;
    if (g_world.m_player) {
        focusTileX = GroundTileFromWorldX(g_world.m_player->m_pos.x, m_width, m_zoom);
        focusTileY = GroundTileFromWorldZ(g_world.m_player->m_pos.z, m_height, m_zoom);
    } else if (g_session.m_playerPosX >= 0 && g_session.m_playerPosY >= 0) {
        focusTileX = g_session.m_playerPosX;
        focusTileY = g_session.m_playerPosY;
    }

    if (focusTileX >= 0 && focusTileY >= 0) {
        const float zoom = (std::max)(1.0f, m_zoom);
        const int halfSpanX = (std::max)(24,
            static_cast<int>(std::ceil(static_cast<float>(g_renderer.m_width) / (zoom * kGroundCullWidthFactor))) + kGroundCullMarginTiles);
        const int halfSpanY = (std::max)(24,
            static_cast<int>(std::ceil(static_cast<float>(g_renderer.m_height) / (zoom * kGroundCullHeightFactor))) + kGroundCullMarginTiles);
        minTileX = (std::max)(0, focusTileX - halfSpanX);
        maxTileX = (std::min)(m_width - 1, focusTileX + halfSpanX);
        minTileY = (std::max)(0, focusTileY - halfSpanY);
        maxTileY = (std::min)(m_height - 1, focusTileY + halfSpanY);
    }

    const int scanWidth = (std::max)(0, maxTileX - minTileX + 1);
    const int scanHeight = (std::max)(0, maxTileY - minTileY + 1);
    g_renderer.m_vertBuffer.reserve(g_renderer.m_vertBuffer.size() + static_cast<size_t>(scanWidth) * static_cast<size_t>(scanHeight) * 4u);

    int totalCells = 0;
    int missingSurface = 0;
    int submittedFaces = 0;
    int texturedFaces = 0;
    int clippedFaces = 0;
    int offscreenFaces = 0;
    int viewportFaces = 0;
    int oversizedFaces = 0;
    int submittedTopFaces = 0;
    int submittedFrontFaces = 0;
    int submittedRightFaces = 0;
    std::vector<unsigned char> viewportTextureFlags(m_textureNames.size(), 0);
    std::vector<int> viewportTextureIds;
    bool capturedSample = false;
    tlvertex3d sampleVerts[4]{};
    const CGroundSurface* sampleSurfaceInfo = nullptr;
    static bool loggedSubmittedGroundTexture = false;

    auto submitSurface = [&](const CGroundSurface* surface, const vector3d (&worldVerts)[4], int* submittedCounter) {
        if (!surface) {
            ++missingSurface;
            return;
        }

        CTexture* diffuseTexture = surface->tex;
        if (kDebugGroundTestTexture) {
            diffuseTexture = GetDebugGroundTestTexture();
        }

        const bool useLightmap = kUseGroundLightmaps && !kDebugGroundFlatColors && surface->lmtex != nullptr;
        RPLmQuadFace* lmFace = useLightmap ? g_renderer.BorrowLmQuadRP() : nullptr;
        RPFace* face = useLightmap ? nullptr : g_renderer.BorrowNullRP();
        tlvertex3d* verts = nullptr;
        lmtlvertex3d* lmVerts = nullptr;
        if (useLightmap) {
            lmVerts = lmFace->m_lmverts;
            lmFace->lmverts = lmFace->m_lmverts;
        } else {
            verts = g_renderer.BorrowVerts(4);
        }
        bool visible = true;
        for (int i = 0; i < 4; ++i) {
            tlvertex3d projected{};
            if (!ProjectPoint(g_renderer, viewMatrix, worldVerts[i], &projected)) {
                visible = false;
                break;
            }

            tlvertex3d* dstVert = useLightmap ? &lmVerts[i].vert : &verts[i];
            *dstVert = projected;

            if (kDebugGroundFlatColors) {
                dstVert->color = DebugGroundColor(surface->textureId);
            } else {
                dstVert->color = 0xFFFFFFFFu;
            }
            if (kDebugGroundCanonicalUvs || kDebugGroundTestTexture) {
                static const float kCanonicalU[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
                static const float kCanonicalV[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
                dstVert->tu = kCanonicalU[i];
                dstVert->tv = kCanonicalV[i];
            } else {
                dstVert->tu = surface->vertex[i].uv.x;
                dstVert->tv = surface->vertex[i].uv.y;
            }

            if (useLightmap) {
                lmVerts[i].tu2 = surface->lmuv[i].x;
                lmVerts[i].tv2 = surface->lmuv[i].y;
            }
        }

        if (!visible) {
            ++clippedFaces;
            return;
        }

        ++submittedFaces;
        if (submittedCounter) {
            ++(*submittedCounter);
        }

        const tlvertex3d* boundsVerts = useLightmap ? &lmVerts[0].vert : verts;
        float minX = boundsVerts[0].x;
        float maxX = boundsVerts[0].x;
        float minY = boundsVerts[0].y;
        float maxY = boundsVerts[0].y;
        for (int i = 1; i < 4; ++i) {
            minX = (std::min)(minX, boundsVerts[i].x);
            maxX = (std::max)(maxX, boundsVerts[i].x);
            minY = (std::min)(minY, boundsVerts[i].y);
            maxY = (std::max)(maxY, boundsVerts[i].y);
        }

        const float faceWidth = maxX - minX;
        const float faceHeight = maxY - minY;
        if (faceWidth > static_cast<float>(g_renderer.m_width) * 2.0f
            || faceHeight > static_cast<float>(g_renderer.m_height) * 2.0f) {
            ++oversizedFaces;
            if (kRejectOversizedGroundFaces) {
                return;
            }
        }

        const bool intersectsViewport = maxX >= 0.0f
            && minX <= static_cast<float>(g_renderer.m_width)
            && maxY >= 0.0f
            && minY <= static_cast<float>(g_renderer.m_height);
        if (!intersectsViewport) {
            ++offscreenFaces;
            return;
        }

        ++viewportFaces;
        if (surface->textureId >= 0 && surface->textureId < static_cast<int>(viewportTextureFlags.size())
            && !viewportTextureFlags[static_cast<size_t>(surface->textureId)]) {
            viewportTextureFlags[static_cast<size_t>(surface->textureId)] = 1;
            viewportTextureIds.push_back(surface->textureId);
        }

        if (!capturedSample) {
            capturedSample = true;
            sampleSurfaceInfo = surface;
            for (int i = 0; i < 4; ++i) {
                sampleVerts[i] = boundsVerts[i];
            }
            if constexpr (kLogGround) if (!loggedSubmittedGroundTexture) {
                loggedSubmittedGroundTexture = true;
                DbgLog("[Ground] sample-submit textureId=%d tex=%p pdds=%p dummy=%d lm=%d lmTex=%p useLightmap=%d color=%08X\n",
                    surface->textureId,
                    static_cast<void*>(diffuseTexture),
                    diffuseTexture ? static_cast<void*>(diffuseTexture->m_pddsSurface) : nullptr,
                    diffuseTexture == &CTexMgr::s_dummy_texture ? 1 : 0,
                    surface->lightmapId,
                    static_cast<void*>(surface->lmtex),
                    useLightmap ? 1 : 0,
                    surface->color);
            }
        }

        if (useLightmap) {
            lmFace->primType = D3DPT_TRIANGLESTRIP;
            lmFace->numVerts = 4;
            lmFace->indices = nullptr;
            lmFace->numIndices = 0;
            lmFace->tex = diffuseTexture == &CTexMgr::s_dummy_texture ? nullptr : diffuseTexture;
            lmFace->tex2 = surface->lmtex;
            if (lmFace->tex) {
                ++texturedFaces;
            }
            g_renderer.AddLmRP(reinterpret_cast<RPLmFace*>(lmFace), 0x800);
        } else {
            face->primType = D3DPT_TRIANGLESTRIP;
            face->verts = verts;
            face->numVerts = 4;
            face->indices = nullptr;
            face->numIndices = 0;
            face->tex = kDebugGroundFlatColors ? nullptr : (diffuseTexture == &CTexMgr::s_dummy_texture ? nullptr : diffuseTexture);
            if (face->tex) {
                ++texturedFaces;
            }
            face->mtPreset = kDebugGroundFlatColors ? 2 : 0;
            face->srcAlphaMode = D3DBLEND_ONE;
            face->destAlphaMode = D3DBLEND_ZERO;
            g_renderer.AddRP(face, 0x800);
        }
    };

    for (int y = minTileY; y <= maxTileY; ++y) {
        for (int x = minTileX; x <= maxTileX; ++x) {
            ++totalCells;
            const CGroundCell* cell = GetCell(x, y);
            if (!cell || cell->topSurfaceId < 0) {
                ++missingSurface;
                continue;
            }

            const CGroundSurface* surface = GetSurface(cell->topSurfaceId);
            if (!surface) {
                ++missingSurface;
                continue;
            }

            const float x0 = GroundCoordX(x, m_width, m_zoom);
            const float x1 = GroundCoordX(x + 1, m_width, m_zoom);
            const float z0 = GroundCoordZ(y, m_height, m_zoom);
            const float z1 = GroundCoordZ(y + 1, m_height, m_zoom);

            const vector3d topVerts[4] = {
                vector3d{ x0, cell->h[0], z0 },
                vector3d{ x1, cell->h[1], z0 },
                vector3d{ x0, cell->h[2], z1 },
                vector3d{ x1, cell->h[3], z1 },
            };
            SubmitWaterQuad(*this, viewMatrix, x, y, *cell);
            submitSurface(surface, topVerts, &submittedTopFaces);

            if (kSubmitGroundSideFaces && cell->frontSurfaceId >= 0 && y < m_height - 1) {
                const CGroundCell* frontNeighbor = GetCell(x, y + 1);
                const float frontH0 = frontNeighbor ? frontNeighbor->h[0] : cell->h[2];
                const float frontH1 = frontNeighbor ? frontNeighbor->h[1] : cell->h[3];
                const vector3d frontVerts[4] = {
                    vector3d{ x0, cell->h[2], z1 },
                    vector3d{ x1, cell->h[3], z1 },
                    vector3d{ x0, frontH0, z1 },
                    vector3d{ x1, frontH1, z1 },
                };
                submitSurface(GetSurface(cell->frontSurfaceId), frontVerts, &submittedFrontFaces);
            }

            if (kSubmitGroundSideFaces && cell->rightSurfaceId >= 0 && x < m_width - 1) {
                const CGroundCell* rightNeighbor = GetCell(x + 1, y);
                const float rightH0 = rightNeighbor ? rightNeighbor->h[0] : cell->h[1];
                const float rightH2 = rightNeighbor ? rightNeighbor->h[2] : cell->h[3];
                const vector3d rightVerts[4] = {
                    vector3d{ x1, cell->h[3], z1 },
                    vector3d{ x1, cell->h[1], z0 },
                    vector3d{ x1, rightH2, z1 },
                    vector3d{ x1, rightH0, z0 },
                };
                submitSurface(GetSurface(cell->rightSurfaceId), rightVerts, &submittedRightFaces);
            }
        }
    }

    static bool loggedGroundStats = false;
    if constexpr (kLogGround) if (!loggedGroundStats) {
        loggedGroundStats = true;
        DbgLog("[Ground] tiles=%d missing=%d clipped=%d offscreen=%d oversized=%d submitted=%d textured=%d top=%d front=%d right=%d size=%dx%d zoom=%.2f screen=%dx%d hpc=%.2f vpc=%.2f\n",
            totalCells,
            missingSurface,
            clippedFaces,
            offscreenFaces,
            oversizedFaces,
            submittedFaces,
            texturedFaces,
            submittedTopFaces,
            submittedFrontFaces,
            submittedRightFaces,
            m_width,
            m_height,
            m_zoom,
            g_renderer.m_width,
            g_renderer.m_height,
            g_renderer.m_hpc,
            g_renderer.m_vpc);
        DbgLog("[Ground] viewport-intersecting faces=%d\n", viewportFaces);
        DbgLog("[Ground] viewport-texture-ids=%d\n", static_cast<int>(viewportTextureIds.size()));
        const int viewportTextureLogCount = (std::min)(static_cast<int>(viewportTextureIds.size()), 12);
        for (int i = 0; i < viewportTextureLogCount; ++i) {
            const int textureId = viewportTextureIds[static_cast<size_t>(i)];
            const char* textureName = (textureId >= 0 && textureId < static_cast<int>(m_textureNames.size()))
                ? m_textureNames[static_cast<size_t>(textureId)].c_str()
                : "(out-of-range)";
            DbgLog("[Ground] viewport-texture[%d]=id:%d name='%s'\n", i, textureId, textureName);
        }

        if (g_world.m_player) {
            const int playerGroundX = GroundTileFromWorldX(g_world.m_player->m_pos.x, m_width, m_zoom);
            const int playerGroundY = GroundTileFromWorldZ(g_world.m_player->m_pos.z, m_height, m_zoom);
            DbgLog("[Ground] player-ground-cell=(%d,%d) player-world=(%.2f,%.2f,%.2f)\n",
                playerGroundX,
                playerGroundY,
                g_world.m_player->m_pos.x,
                g_world.m_player->m_pos.y,
                g_world.m_player->m_pos.z);

            tlvertex3d projectedPlayer{};
            if (ProjectPoint(g_renderer, viewMatrix, g_world.m_player->m_pos, &projectedPlayer)) {
                DbgLog("[Ground] player-projected=(%.1f,%.1f,z=%.3f,oow=%.6f)\n",
                    projectedPlayer.x,
                    projectedPlayer.y,
                    projectedPlayer.z,
                    projectedPlayer.oow);
            } else {
                DbgLog("[Ground] player-projected=clipped\n");
            }

            for (int sampleY = playerGroundY - 2; sampleY <= playerGroundY + 2; ++sampleY) {
                for (int sampleX = playerGroundX - 2; sampleX <= playerGroundX + 2; ++sampleX) {
                    const CGroundCell* sampleCell = GetCell(sampleX, sampleY);
                    if (!sampleCell || sampleCell->topSurfaceId < 0 || sampleCell->topSurfaceId >= m_numSurfaces) {
                        continue;
                    }

                    const CGroundSurface* sampleSurface = GetSurface(sampleCell->topSurfaceId);
                    if (!sampleSurface) {
                        continue;
                    }

                    const int textureId = sampleSurface->textureId;
                    const char* textureName = (textureId >= 0 && textureId < static_cast<int>(m_textureNames.size()))
                        ? m_textureNames[static_cast<size_t>(textureId)].c_str()
                        : "(out-of-range)";
                    DbgLog("[Ground] nearby-cell=(%d,%d) topSurface=%d textureId=%d name='%s'\n",
                        sampleX,
                        sampleY,
                        sampleCell->topSurfaceId,
                        textureId,
                        textureName);

                    const CTexture* sampleTexture = sampleSurface->tex;
                    const unsigned int texWidth = sampleTexture ? sampleTexture->m_w : 0;
                    const unsigned int texHeight = sampleTexture ? sampleTexture->m_h : 0;
                    const unsigned int updateWidth = sampleTexture ? sampleTexture->m_updateWidth : 0;
                    const unsigned int updateHeight = sampleTexture ? sampleTexture->m_updateHeight : 0;
                    DbgLog("[Ground] nearby-cell-uv=(%d,%d) texSize=%ux%u update=%ux%u uv=((%.6f,%.6f),(%.6f,%.6f),(%.6f,%.6f),(%.6f,%.6f))\n",
                        sampleX,
                        sampleY,
                        texWidth,
                        texHeight,
                        updateWidth,
                        updateHeight,
                        sampleSurface->vertex[0].uv.x, sampleSurface->vertex[0].uv.y,
                        sampleSurface->vertex[1].uv.x, sampleSurface->vertex[1].uv.y,
                        sampleSurface->vertex[2].uv.x, sampleSurface->vertex[2].uv.y,
                        sampleSurface->vertex[3].uv.x, sampleSurface->vertex[3].uv.y);
                }
            }
        } else {
            DbgLog("[Ground] player-ground-cell unavailable session-cell=(%d,%d)\n", g_session.m_playerPosX, g_session.m_playerPosY);
        }

        if (capturedSample) {
            DbgLog("[Ground] sample quad screen verts: (%.1f,%.1f,z=%.3f,oow=%.6f) (%.1f,%.1f,z=%.3f,oow=%.6f) (%.1f,%.1f,z=%.3f,oow=%.6f) (%.1f,%.1f,z=%.3f,oow=%.6f)\n",
                sampleVerts[0].x, sampleVerts[0].y, sampleVerts[0].z, sampleVerts[0].oow,
                sampleVerts[1].x, sampleVerts[1].y, sampleVerts[1].z, sampleVerts[1].oow,
                sampleVerts[2].x, sampleVerts[2].y, sampleVerts[2].z, sampleVerts[2].oow,
                sampleVerts[3].x, sampleVerts[3].y, sampleVerts[3].z, sampleVerts[3].oow);
            if (sampleSurfaceInfo) {
                const CTexture* sampleTexture = sampleSurfaceInfo->tex;
                const unsigned int texWidth = sampleTexture ? sampleTexture->m_w : 0;
                const unsigned int texHeight = sampleTexture ? sampleTexture->m_h : 0;
                const unsigned int updateWidth = sampleTexture ? sampleTexture->m_updateWidth : 0;
                const unsigned int updateHeight = sampleTexture ? sampleTexture->m_updateHeight : 0;
                DbgLog("[Ground] sample quad uv texSize=%ux%u update=%ux%u uv=((%.6f,%.6f),(%.6f,%.6f),(%.6f,%.6f),(%.6f,%.6f))\n",
                    texWidth,
                    texHeight,
                    updateWidth,
                    updateHeight,
                    sampleSurfaceInfo->vertex[0].uv.x, sampleSurfaceInfo->vertex[0].uv.y,
                    sampleSurfaceInfo->vertex[1].uv.x, sampleSurfaceInfo->vertex[1].uv.y,
                    sampleSurfaceInfo->vertex[2].uv.x, sampleSurfaceInfo->vertex[2].uv.y,
                    sampleSurfaceInfo->vertex[3].uv.x, sampleSurfaceInfo->vertex[3].uv.y);
            }
        }
    }
}

CWorld::CWorld() 
    : m_curMode(nullptr), m_ground(nullptr), m_player(nullptr), m_attr(nullptr)
    , m_bgObjCount(0), m_bgObjThread(0), m_isPKZone(0), m_isSiegeMode(0)
    , m_isBattleFieldMode(0), m_isEventPVPMode(0)
    , m_bgLightDir{ 0.0f, 1.0f, 0.0f }, m_bgDiffuseCol{ 1.0f, 1.0f, 1.0f }, m_bgAmbientCol{ 0.3f, 0.3f, 0.3f }
    , m_billboardFrameCameraLongitude(0.0f), m_billboardFrameZoom(0.0f)
    , m_billboardFrameCacheValid(false), m_billboardFrameCacheDirty(true)
    , m_Calculated(nullptr)
{
}

CWorld::~CWorld()
{
    ClearFixedObjects();
    ClearBackgroundObjects();
    ClearGround();
}

void CWorld::ClearGround()
{
    delete m_ground;
    m_ground = nullptr;
    InvalidateBillboardFrameCache();
    ResetSceneGraph();
}

void CWorld::ClearBackgroundObjects()
{
    for (C3dActor* actor : m_bgObjList) {
        delete actor;
    }
    m_bgObjList.clear();
    m_bgObjCount = 0;
}

void CWorld::ClearFixedObjects()
{
    for (CGameObject* object : m_gameObjectList) {
        delete object;
    }
    m_gameObjectList.clear();

    for (CItem* item : m_itemList) {
        delete item;
    }
    m_itemList.clear();
}

void CWorld::ResetSceneGraph()
{
    m_rootNode.ClearChildren();
    ResetNodeState(m_rootNode);
    m_rootNode.m_attr = m_attr;
    if (m_attr) {
        SetRect(&m_rootNode.m_attrArea, 0, 0, m_attr->m_width, m_attr->m_height);
    }
    m_Calculated = nullptr;
}

void CWorld::InvalidateBillboardFrameCache()
{
    m_billboardFrameEntries.clear();
    m_billboardFrameEntryByGid.clear();
    m_billboardFrameCacheValid = false;
    m_billboardFrameCacheDirty = true;
}

void CWorld::EnsureBillboardFrameCache(const matrix& viewMatrix, float cameraLongitude) const
{
    const float zoom = m_ground ? m_ground->m_zoom : static_cast<float>(m_attr ? m_attr->m_zoom : 5);
    if (!m_billboardFrameCacheDirty
        && m_billboardFrameCacheValid
        && std::memcmp(&m_billboardFrameViewMatrix, &viewMatrix, sizeof(matrix)) == 0
        && m_billboardFrameCameraLongitude == cameraLongitude
        && m_billboardFrameZoom == zoom) {
        return;
    }

    m_billboardFrameEntries.clear();
    m_billboardFrameEntryByGid.clear();
    m_billboardFrameEntries.reserve(m_actorList.size() + (m_player ? 1u : 0u));

    auto enqueueActor = [&](CGameActor* actor) {
        if (!actor || !actor->m_isVisible) {
            return;
        }

        CPc* pc = dynamic_cast<CPc*>(actor);
        if (!pc || IsPortalActorJob(pc->m_job)) {
            return;
        }

        BillboardScreenEntry entry{};
        if (!BuildBillboardRenderEntry(pc, viewMatrix, cameraLongitude, zoom, &entry)) {
            return;
        }

        m_billboardFrameEntries.push_back(entry);
    };

    enqueueActor(m_player);
    for (CGameActor* actor : m_actorList) {
        if (!actor || actor == m_player) {
            continue;
        }
        enqueueActor(actor);
    }

    std::stable_sort(m_billboardFrameEntries.begin(), m_billboardFrameEntries.end(), CompareBillboardRenderEntry);
    for (size_t index = 0; index < m_billboardFrameEntries.size(); ++index) {
        const BillboardScreenEntry& entry = m_billboardFrameEntries[index];
        if (entry.actor) {
            m_billboardFrameEntryByGid[entry.actor->m_gid] = index;
        }
    }
    m_billboardFrameViewMatrix = viewMatrix;
    m_billboardFrameCameraLongitude = cameraLongitude;
    m_billboardFrameZoom = zoom;
    m_billboardFrameCacheValid = true;
    m_billboardFrameCacheDirty = false;
}

const CWorld::BillboardScreenEntry* CWorld::FindBillboardFrameEntryByGid(u32 gid) const
{
    const auto it = m_billboardFrameEntryByGid.find(gid);
    if (it == m_billboardFrameEntryByGid.end()) {
        return nullptr;
    }
    const size_t index = it->second;
    if (index >= m_billboardFrameEntries.size()) {
        return nullptr;
    }
    return &m_billboardFrameEntries[index];
}

void CWorld::RebuildSceneGraph()
{
    ResetSceneGraph();

    if (!m_ground && !m_attr) {
        return;
    }

    float minY = 0.0f;
    float maxY = 0.0f;
    bool hasHeightRange = false;
    float zoom = 5.0f;
    int width = 0;
    int height = 0;

    if (m_ground) {
        zoom = m_ground->m_zoom;
        width = m_ground->m_width;
        height = m_ground->m_height;
        for (const CGroundCell& cell : m_ground->m_cells) {
            for (float cellHeight : cell.h) {
                if (!hasHeightRange) {
                    minY = maxY = cellHeight;
                    hasHeightRange = true;
                } else {
                    minY = (std::min)(minY, cellHeight);
                    maxY = (std::max)(maxY, cellHeight);
                }
            }
        }
    } else if (m_attr) {
        zoom = static_cast<float>(m_attr->m_zoom);
        width = m_attr->m_width;
        height = m_attr->m_height;
        ComputeHeightRangeFromAttr(*m_attr, &minY, &maxY);
        hasHeightRange = !m_attr->m_cells.empty();
    }

    if (!hasHeightRange) {
        minY = -10.0f;
        maxY = 10.0f;
    }

    const float halfWidth = width * zoom * 0.5f;
    const float halfDepth = height * zoom * 0.5f;
    m_rootNode.m_aabb.min = vector3d{ -halfWidth, minY, -halfDepth };
    m_rootNode.m_aabb.max = vector3d{ halfWidth, maxY, halfDepth };
    m_rootNode.Build(0, kSceneGraphMaxLevel);
    m_rootNode.InsertGround(m_ground, 0, kSceneGraphMaxLevel);
    m_rootNode.InsertAttr(m_attr, 0, kSceneGraphMaxLevel);
    for (CGameActor* actor : m_actorList) {
        if (actor) {
            m_rootNode.InsertActor(actor, actor->m_pos.x, actor->m_pos.z);
        }
    }
    m_Calculated = &m_rootNode;
}

void CWorld::UpdateCalculatedNodeForTile(int tileX, int tileY)
{
    if (!m_ground && !m_attr) {
        m_Calculated = nullptr;
        return;
    }

    const float zoom = m_ground ? m_ground->m_zoom : static_cast<float>(m_attr ? m_attr->m_zoom : 5);
    const int width = m_ground ? m_ground->m_width : (m_attr ? m_attr->m_width : 0);
    const int height = m_ground ? m_ground->m_height : (m_attr ? m_attr->m_height : 0);
    const float worldX = GroundCoordX(tileX, width, zoom) + zoom * 0.5f;
    const float worldZ = GroundCoordZ(tileY, height, zoom) + zoom * 0.5f;
    m_Calculated = m_rootNode.FindLeaf(worldX, worldZ);
}

void CWorld::RegisterActor(CGameActor* actor)
{
    if (!actor) {
        return;
    }

    if (std::find(m_actorList.begin(), m_actorList.end(), actor) == m_actorList.end()) {
        m_actorList.push_back(actor);
    }

    if (m_rootNode.m_child[0] || m_rootNode.m_child[1] || m_rootNode.m_child[2] || m_rootNode.m_child[3]) {
        m_rootNode.InsertActor(actor, actor->m_pos.x, actor->m_pos.z);
    }
    InvalidateBillboardFrameCache();
}

void CWorld::UnregisterActor(CGameActor* actor)
{
    if (!actor) {
        return;
    }

    m_rootNode.RemoveActor(actor);
    m_actorList.remove(actor);
    InvalidateBillboardFrameCache();
}

std::vector<CGameActor*>* CWorld::GetActorsAtWorldPos(float x, float z)
{
    if (!(m_rootNode.m_child[0] || m_rootNode.m_child[1] || m_rootNode.m_child[2] || m_rootNode.m_child[3])) {
        return &m_rootNode.m_actorList;
    }
    return m_rootNode.GetActorList(x, z);
}

const std::vector<CGameActor*>* CWorld::GetActorsAtWorldPos(float x, float z) const
{
    if (!(m_rootNode.m_child[0] || m_rootNode.m_child[1] || m_rootNode.m_child[2] || m_rootNode.m_child[3])) {
        return &m_rootNode.m_actorList;
    }
    return m_rootNode.GetActorList(x, z);
}

bool CWorld::BuildGroundFromGnd(const CGndRes& gnd,
    const vector3d& lightDir,
    const vector3d& diffuseCol,
    const vector3d& ambientCol,
    float waterLevel,
    int waterType,
    int waterAnimSpeed,
    int wavePitch,
    int waveSpeed,
    float waveHeight)
{
    ClearGround();

    C3dGround* ground = new C3dGround();
    if (!ground) {
        return false;
    }

    if (!ground->AssignGnd(gnd, lightDir, diffuseCol, ambientCol)) {
        delete ground;
        return false;
    }

    ground->m_attr = m_attr;
    ground->SetWaterInfo(waterLevel, waterType, waterAnimSpeed, wavePitch, waveSpeed, waveHeight);
    m_ground = ground;

    RebuildSceneGraph();
    return true;
}

bool CWorld::BuildBackgroundObjects(const C3dWorldRes& worldRes,
    const vector3d& lightDir,
    const vector3d& diffuseCol,
    const vector3d& ambientCol)
{
    return AppendBackgroundObjects(
        worldRes,
        0,
        static_cast<size_t>(-1),
        lightDir,
        diffuseCol,
        ambientCol,
        nullptr,
        true);
}

bool CWorld::AppendBackgroundObjects(const C3dWorldRes& worldRes,
    size_t startIndex,
    size_t maxActors,
    const vector3d& lightDir,
    const vector3d& diffuseCol,
    const vector3d& ambientCol,
    size_t* outNextIndex,
    bool clearExisting)
{
    if (clearExisting) {
        ClearBackgroundObjects();
    }

    m_bgLightDir = lightDir;
    m_bgDiffuseCol = diffuseCol;
    m_bgAmbientCol = ambientCol;

    size_t actorIndex = 0;
    size_t processed = 0;
    for (const C3dWorldRes::actorInfo* actorInfo : worldRes.m_3dActors) {
        if (actorIndex < startIndex) {
            ++actorIndex;
            continue;
        }
        if (processed >= maxActors) {
            break;
        }

        ++processed;
        ++actorIndex;

        if (!actorInfo || !actorInfo->modelName[0]) {
            continue;
        }

        const std::string modelPath = ResolveDataPathWorld(actorInfo->modelName, "rsm", {
            "",
            "data\\",
            "model\\",
            "data\\model\\"
        });
        if (modelPath.empty()) {
            DbgLog("[World] background object model unresolved raw='%s' name='%s'\n", actorInfo->modelName, actorInfo->name);
            continue;
        }

        C3dModelRes* modelRes = g_resMgr.GetAs<C3dModelRes>(modelPath.c_str());
        if (!modelRes) {
            DbgLog("[World] background object model missing raw='%s' resolved='%s'\n", actorInfo->modelName, modelPath.c_str());
            continue;
        }
        const C3dNodeRes* rootNode = modelRes->FindNode(actorInfo->nodeName);
        if (!rootNode && modelRes->m_objectList.empty()) {
            DbgLog("[World] background object root missing model='%s' node='%s'\n", modelPath.c_str(), actorInfo->nodeName);
            continue;
        }
        C3dActor* actor = new C3dActor();
        if (!actor) {
            return false;
        }

        actor->m_animSpeed = actorInfo->animSpeed != 0.0f ? actorInfo->animSpeed * (1.0f / 3.0f) : 0.0f;
        actor->m_animType = actorInfo->animType;
        actor->m_pos = actorInfo->pos;
        actor->m_rot = actorInfo->rot;
        actor->m_scale = actorInfo->scale;
        actor->m_blockType = actorInfo->blockType;
        actor->m_isHideCheck = 0;
        actor->m_isMatrixNeedUpdate = 1;
        actor->m_debugModelPath = modelPath;
        actor->m_debugNodeName = actorInfo->nodeName;

        if (!actor->AssignModel(*modelRes)) {
            delete actor;
            continue;
        }

        std::strncpy(actor->m_name, actorInfo->name, sizeof(actor->m_name) - 1);
        actor->m_name[sizeof(actor->m_name) - 1] = '\0';
        actor->UpdateVertexColor(m_bgLightDir, m_bgDiffuseCol, m_bgAmbientCol);
        m_bgObjList.push_back(actor);
    }

    m_bgObjCount = static_cast<int>(m_bgObjList.size());
    if (outNextIndex) {
        *outNextIndex = actorIndex;
    }
    return true;
}

bool CWorld::BuildFixedEffects(const C3dWorldRes& worldRes)
{
    return AppendFixedEffects(worldRes, 0, static_cast<size_t>(-1), nullptr, true);
}

bool CWorld::AppendFixedEffects(const C3dWorldRes& worldRes,
    size_t startIndex,
    size_t maxEffects,
    size_t* outNextIndex,
    bool clearExisting)
{
    if (clearExisting) {
        ClearFixedObjects();
    }

    size_t effectIndex = 0;
    size_t processed = 0;
    for (const C3dWorldRes::effectSrcInfo* effectInfo : worldRes.m_particles) {
        if (effectIndex < startIndex) {
            ++effectIndex;
            continue;
        }
        if (processed >= maxEffects) {
            break;
        }

        ++processed;
        ++effectIndex;

        if (!effectInfo) {
            continue;
        }

        CFixedWorldEffect* effect = new CFixedWorldEffect(*effectInfo);
        if (!effect) {
            return false;
        }

        m_gameObjectList.push_back(effect);
        DbgLog("[WorldEffect] spawn name='%s' type=%d emit=%.3f pos=(%.2f,%.2f,%.2f) param=(%.2f,%.2f,%.2f,%.2f)\n",
            effectInfo->name,
            effectInfo->type,
            effectInfo->emitSpeed,
            effectInfo->pos.x,
            effectInfo->pos.y,
            effectInfo->pos.z,
            effectInfo->param[0],
            effectInfo->param[1],
            effectInfo->param[2],
            effectInfo->param[3]);
    }

    if (outNextIndex) {
        *outNextIndex = effectIndex;
    }
    return true;
}

void CWorld::UpdateGameObjects()
{
    for (auto it = m_gameObjectList.begin(); it != m_gameObjectList.end(); ) {
        CGameObject* object = *it;
        if (!object || object->OnProcess() == 0) {
            delete object;
            it = m_gameObjectList.erase(it);
            continue;
        }
        ++it;
    }
}

void CWorld::RenderGameObjects(const matrix& viewMatrix) const
{
    for (CGameObject* object : m_gameObjectList) {
        if (!object) {
            continue;
        }
        object->Render(const_cast<matrix*>(&viewMatrix));
    }
}

void CWorld::UpdateBackgroundObjects(const matrix* viewMatrix)
{
    static u32 s_backgroundUpdateFrame = 0;
    static u64 s_profileFrames = 0;
    static u64 s_animatedActors = 0;
    static u64 s_animatedUpdated = 0;
    static u64 s_animatedSkipped = 0;
    static u64 s_staticRelit = 0;
    ++s_backgroundUpdateFrame;

    const vector3d* focusPos = m_player ? &m_player->m_pos : nullptr;
    for (size_t index = 0; index < m_bgObjList.size(); ++index) {
        C3dActor* actor = m_bgObjList[index];
        if (!actor) {
            continue;
        }

        if (actor->m_animType != 0) {
            s_animatedActors += 1;
            const u32 updateInterval = ResolveBackgroundActorUpdateInterval(*actor, focusPos, viewMatrix);
            if (updateInterval > 1 && ((s_backgroundUpdateFrame + static_cast<u32>(index)) % updateInterval) != 0) {
                s_animatedSkipped += 1;
                continue;
            }
            actor->AdvanceFrame();
            actor->UpdateVertexColor(m_bgLightDir, m_bgDiffuseCol, m_bgAmbientCol);
            s_animatedUpdated += 1;
        } else if (actor->m_isMatrixNeedUpdate) {
            actor->UpdateVertexColor(m_bgLightDir, m_bgDiffuseCol, m_bgAmbientCol);
            s_staticRelit += 1;
        }
    }

    s_profileFrames += 1;
    if ((s_profileFrames % 120u) == 0) {
        s_animatedActors = 0;
        s_animatedUpdated = 0;
        s_animatedSkipped = 0;
        s_staticRelit = 0;
    }
}

void CWorld::UpdateActors()
{
    InvalidateBillboardFrameCache();
    UpdateGameObjects();

    for (CItem* item : m_itemList) {
        if (!item) {
            continue;
        }
        item->OnProcess();
    }

    if (m_player) {
        m_player->ProcessState();
    }

    for (CGameActor* actor : m_actorList) {
        if (!actor) {
            continue;
        }
        actor->ProcessState();
    }
}

void CWorld::RenderActors(const matrix& viewMatrix, float cameraLongitude)
{
    RenderGameObjects(viewMatrix);

    for (CItem* item : m_itemList) {
        if (!item) {
            continue;
        }
        item->Render(const_cast<matrix*>(&viewMatrix));
    }

    for (CGameActor* actor : m_actorList) {
        if (!actor || !actor->m_isVisible) {
            continue;
        }
        if (actor == m_player) {
            continue;
        }

        CPc* pc = dynamic_cast<CPc*>(actor);
        if (!pc || !IsPortalActorJob(pc->m_job)) {
            continue;
        }

        const DWORD tick = timeGetTime();
        const float timeSeconds = static_cast<float>(tick) * 0.001f;
        const COLORREF portalColor = pc->m_job == kJobPreWarpPortal
            ? RGB(255, 172, 92)
            : RGB(86, 196, 255);
        const PortalVisualStyle portalStyle = pc->m_job == kJobPreWarpPortal
            ? PortalVisualStyle::Ready
            : (pc->m_job == kJobWarpPortal ? PortalVisualStyle::WarpZone : PortalVisualStyle::Portal);
        RenderPortalEffectAtPosition(pc->m_pos,
            viewMatrix,
            portalColor,
            pc->m_job == kJobWarpNpc ? 6.5f : 5.0f,
            pc->m_job == kJobWarpNpc ? 6.0f : 5.0f,
            timeSeconds,
            static_cast<float>(pc->m_gid & 0xFFu) * 0.19f,
            false,
            portalStyle);
    }

    if (m_player) {
        CPc* pc = dynamic_cast<CPc*>(m_player);
        if (pc && pc->m_isVisible && IsPortalActorJob(pc->m_job)) {
            const DWORD tick = timeGetTime();
            const float timeSeconds = static_cast<float>(tick) * 0.001f;
            const COLORREF portalColor = pc->m_job == kJobPreWarpPortal
                ? RGB(255, 172, 92)
                : RGB(86, 196, 255);
            const PortalVisualStyle portalStyle = pc->m_job == kJobPreWarpPortal
                ? PortalVisualStyle::Ready
                : (pc->m_job == kJobWarpPortal ? PortalVisualStyle::WarpZone : PortalVisualStyle::Portal);
            RenderPortalEffectAtPosition(pc->m_pos,
                viewMatrix,
                portalColor,
                pc->m_job == kJobWarpNpc ? 6.5f : 5.0f,
                pc->m_job == kJobWarpNpc ? 6.0f : 5.0f,
                timeSeconds,
                static_cast<float>(pc->m_gid & 0xFFu) * 0.19f,
                false,
                portalStyle);
        }
    }

    EnsureBillboardFrameCache(viewMatrix, cameraLongitude);
    for (const BillboardScreenEntry& entry : m_billboardFrameEntries) {
        RenderCachedBillboard(entry);
    }
}

bool CWorld::GetPlayerScreenLabel(const matrix& viewMatrix,
    float cameraLongitude,
    int* outLabelX,
    int* outLabelY) const
{
    (void)cameraLongitude;

    if (outLabelX) {
        *outLabelX = 0;
    }
    if (outLabelY) {
        *outLabelY = 0;
    }

    CPc* pc = dynamic_cast<CPc*>(m_player);
    if (!pc) {
        return false;
    }

    EnsureBillboardFrameCache(viewMatrix, cameraLongitude);
    if (const BillboardScreenEntry* entry = FindBillboardFrameEntryByGid(pc->m_gid)) {
        if (outLabelX) {
            *outLabelX = static_cast<int>(std::lround(entry->baseX));
        }
        if (outLabelY) {
            *outLabelY = static_cast<int>(std::lround(entry->baseY)) + 18;
        }
        return true;
    }

    tlvertex3d projectedBase{};
    if (!ProjectPoint(g_renderer, viewMatrix, pc->m_pos, &projectedBase)) {
        return false;
    }

    if (outLabelX) {
        *outLabelX = static_cast<int>(std::lround(projectedBase.x));
    }
    if (outLabelY) {
        *outLabelY = static_cast<int>(std::lround(projectedBase.y)) + 18;
    }
    return true;
}

bool CWorld::GetActorScreenMarker(const matrix& viewMatrix,
    float cameraLongitude,
    u32 gid,
    int* outCenterX,
    int* outTopY,
    int* outLabelY) const
{
    if (outCenterX) {
        *outCenterX = 0;
    }
    if (outTopY) {
        *outTopY = 0;
    }
    if (outLabelY) {
        *outLabelY = 0;
    }

    CGameActor* actor = nullptr;
    if (m_player && m_player->m_gid == gid) {
        actor = m_player;
    } else {
        for (CGameActor* entry : m_actorList) {
            if (entry && entry->m_gid == gid) {
                actor = entry;
                break;
            }
        }
    }

    if (!actor || !actor->m_isVisible) {
        return false;
    }

    EnsureBillboardFrameCache(viewMatrix, cameraLongitude);
    if (const BillboardScreenEntry* entry = FindBillboardFrameEntryByGid(gid)) {
        if (outCenterX) {
            *outCenterX = static_cast<int>(std::lround((entry->left + entry->right) * 0.5f));
        }
        if (outTopY) {
            *outTopY = static_cast<int>(std::lround(entry->top));
        }
        if (outLabelY) {
            *outLabelY = static_cast<int>(std::lround(entry->labelY));
        }
        return true;
    }

    tlvertex3d projectedBase{};
    if (!ProjectPoint(g_renderer, viewMatrix, actor->m_pos, &projectedBase)) {
        return false;
    }

    if (outCenterX) {
        *outCenterX = static_cast<int>(std::lround(projectedBase.x));
    }
    if (outTopY) {
        *outTopY = static_cast<int>(std::lround(projectedBase.y)) - 48;
    }
    if (outLabelY) {
        *outLabelY = static_cast<int>(std::lround(projectedBase.y));
    }
    return true;
}

bool CWorld::FindHoveredActorScreen(const matrix& viewMatrix,
    float cameraLongitude,
    int screenX,
    int screenY,
    CGameActor** outActor,
    int* outLabelX,
    int* outLabelY) const
{
    if (outActor) {
        *outActor = nullptr;
    }
    if (outLabelX) {
        *outLabelX = 0;
    }
    if (outLabelY) {
        *outLabelY = 0;
    }

    EnsureBillboardFrameCache(viewMatrix, cameraLongitude);

    const float mouseX = static_cast<float>(screenX);
    const float mouseY = static_cast<float>(screenY);
    for (auto it = m_billboardFrameEntries.rbegin(); it != m_billboardFrameEntries.rend(); ++it) {
        if (mouseX < it->left || mouseX > it->right || mouseY < it->top || mouseY > it->bottom) {
            continue;
        }

        CPc* liveActor = FindLiveBillboardActor(*this, it->actor ? it->actor->m_gid : 0);
        if (!liveActor || !liveActor->m_isVisible) {
            continue;
        }

        if (outActor) {
            *outActor = liveActor;
        }
        if (outLabelX) {
            *outLabelX = static_cast<int>(std::lround(it->baseX));
        }
        if (outLabelY) {
            *outLabelY = static_cast<int>(std::lround(it->baseY));
        }
        return true;
    }

    return false;
}

bool CWorld::GetGroundItemScreenMarker(const matrix& viewMatrix,
    u32 aid,
    int* outCenterX,
    int* outTopY,
    int* outLabelY) const
{
    if (outCenterX) {
        *outCenterX = 0;
    }
    if (outTopY) {
        *outTopY = 0;
    }
    if (outLabelY) {
        *outLabelY = 0;
    }

    for (CItem* item : m_itemList) {
        if (!item || item->m_aid != aid) {
            continue;
        }

        return GetGroundItemScreenRect(viewMatrix, *item, nullptr, outCenterX, outTopY, outLabelY, nullptr);
    }

    return false;
}

bool CWorld::FindHoveredGroundItemScreen(const matrix& viewMatrix,
    int screenX,
    int screenY,
    CItem** outItem,
    int* outLabelX,
    int* outLabelY) const
{
    if (outItem) {
        *outItem = nullptr;
    }
    if (outLabelX) {
        *outLabelX = 0;
    }
    if (outLabelY) {
        *outLabelY = 0;
    }

    const POINT mousePoint{ screenX, screenY };
    float bestDepth = 1.0f;
    CItem* bestItem = nullptr;
    int bestLabelX = 0;
    int bestLabelY = 0;
    for (CItem* item : m_itemList) {
        if (!item) {
            continue;
        }

        RECT rect{};
        int centerX = 0;
        int topY = 0;
        float depth = 1.0f;
        if (!GetGroundItemScreenRect(viewMatrix, *item, &rect, &centerX, &topY, &topY, &depth)) {
            continue;
        }
        if (!PtInRect(&rect, mousePoint)) {
            continue;
        }

        if (!bestItem || depth < bestDepth) {
            bestItem = item;
            bestDepth = depth;
            bestLabelX = centerX;
            bestLabelY = topY;
        }
    }

    if (!bestItem) {
        return false;
    }

    if (outItem) {
        *outItem = bestItem;
    }
    if (outLabelX) {
        *outLabelX = bestLabelX;
    }
    if (outLabelY) {
        *outLabelY = bestLabelY;
    }
    return true;
}

void CWorld::RenderBackgroundObjects(const matrix& viewMatrix) const
{
    for (const C3dActor* actor : m_bgObjList) {
        if (!actor) {
            continue;
        }
        if (!ShouldRenderBackgroundActor(*actor, viewMatrix)) {
            continue;
        }
        actor->Render(viewMatrix);
    }
}

void LaunchLevelUpEffect(CGameActor* actor, u32 effectId)
{
    if (!actor) {
        return;
    }

    g_world.m_gameObjectList.push_back(new CLevelUpEffect(actor, effectId));
}
