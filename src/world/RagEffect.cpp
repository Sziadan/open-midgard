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
#include <vector>

namespace {

constexpr float kNearPlane = 10.0f;
constexpr float kSubmitNearPlane = 80.0f;
constexpr float kPi = 3.14159265f;
constexpr float kEffectTickMs = 24.0f;
constexpr float kEffectPixelRatioScale = 0.14285715f;
constexpr float kEffectSpriteDepthBias = 0.0002f;
constexpr float kWeatherCloudDurationFrames = 960.0f;
constexpr float kMapPillarDurationFrames = 960.0f;
constexpr float kWaterfallSegmentHeight = 40.0f;
constexpr int kWaterfallVisibleSegments = 5;
constexpr int kWaterfallTextureCycle = 3;
// Ref/Wave.cpp PlayWave defaults used for skill / EzStr SFX.
constexpr int kRefEffectWaveMaxDist = 250;
constexpr int kRefEffectWaveMinDist = 40;
constexpr int kSightEffectStateMask = 0x0001;

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

bool MasterHasEffectState(const CRenderObject* master, int mask)
{
    const CGameActor* actor = dynamic_cast<const CGameActor*>(master);
    return actor && (actor->m_effectState & mask) != 0;
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

const char* RefBlessingSpriteStem()
{
    return "\xC3\xE0\xBA\xB9";
}

const char* RefParticle2SpriteStem()
{
    return "particle2";
}

const char* RefParticle3SpriteStem()
{
    return "particle3";
}

const char* RefParticle6SpriteStem()
{
    return "particle6";
}

const char* RefChimneySmokeSpriteStem()
{
    return "\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2";
}

const char* RefTorchSpriteStem()
{
    return "torch_01";
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

void ConfigureEnchantPoisonParticle(CEffectPrim* prim, int speedBase, int speedSpan, int sizeBase, int sizeSpan)
{
    if (!prim) {
        return;
    }

    prim->m_duration = 40;

    const float angle = static_cast<float>(rand() % 360);
    MatrixIdentity(prim->m_matrix);
    MatrixAppendYRotation(prim->m_matrix, angle * (kPi / 180.0f));

    const float radius = static_cast<float>(rand() % 6 + 2);
    prim->m_deltaPos2 = {
        radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
        radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
        radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
    };

    prim->m_latitude = 90.0f;
    prim->m_longitude = 0.0f;
    prim->m_speed = static_cast<float>(rand() % speedSpan + speedBase) * 0.01f;
    prim->m_accel = prim->m_speed / static_cast<float>(prim->m_duration) * -0.5f;
    prim->m_size = static_cast<float>(rand() % sizeSpan + sizeBase) * 0.01f;
    prim->m_rollSpeed = 3.0f;
    prim->m_alpha = 0.0f;
    prim->m_alphaSpeed = prim->m_maxAlpha * 0.1f;
    prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5;
    prim->m_tintColor = RGB(255, 255, 255);
    ConfigureEffectSpritePrim(prim, { RefParticle3SpriteStem(), RefParticle2SpriteStem() }, 0, 1.0f, true, 0.0f, 0);
}

void AppendTorchFallbackTextures(CEffectPrim* prim, const std::string& effectName)
{
    if (!prim) {
        return;
    }

    auto appendSequence = [&](const char* prefix) {
        for (int frameIndex = 1; frameIndex <= 13; ++frameIndex) {
            std::string path = std::string("effect\\") + prefix;
            if (frameIndex < 10) {
                path += '0';
            }
            path += std::to_string(frameIndex);
            path += ".bmp";
            if (CTexture* texture = TryResolveEffectTextureCandidates({ path.c_str() })) {
                prim->m_texture.push_back(texture);
            }
        }
    };

    if (ContainsIgnoreCaseAscii(effectName, "red")) {
        appendSequence("torch_red");
    } else if (ContainsIgnoreCaseAscii(effectName, "violet") || ContainsIgnoreCaseAscii(effectName, "purple")) {
        appendSequence("torch_violet");
    } else if (ContainsIgnoreCaseAscii(effectName, "green")) {
        appendSequence("torch_green");
    }

    if (prim->m_texture.empty()) {
        appendSequence("torch_red");
        appendSequence("torch_green");
        appendSequence("torch_violet");
    }

    if (prim->m_texture.empty()) {
        prim->m_texture.push_back(ResolveEffectTextureCandidates({
            "effect\\torch_red01.bmp",
            "effect\\torch_green01.bmp",
            "effect\\torch_violet01.bmp",
            "effect\\magic_red.tga",
        }, false));
    }
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

void SetBandMode(EffectBandState* band, u8 mode)
{
    if (!band) {
        return;
    }
    band->modes.fill(mode);
}

constexpr u8 kBandModeBeginCasting = 4;
constexpr u8 kBandModeSaintCasting = 5;

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

void ConfigureBeginCastingBand(CEffectPrim* prim,
    int bandIndex,
    int process,
    float maxHeight,
    float rotStart,
    float distance,
    float riseAngle,
    float alphaCeiling,
    u8 mode)
{
    ConfigureBand(prim, bandIndex, process, maxHeight, rotStart, distance, riseAngle, alphaCeiling, mode);
    if (!prim || bandIndex < 0 || bandIndex >= static_cast<int>(prim->m_bands.size())) {
        return;
    }

    EffectBandState& band = prim->m_bands[static_cast<size_t>(bandIndex)];
    band.alpha = 0.0f;
    band.radius = distance;
}

void ConfigureStandardBeginCastingBands(CEffectPrim* prim, float scale, float rotOff)
{
    if (!prim) {
        return;
    }

    ConfigureBeginCastingBand(prim, 0, 0, 25.0f * scale, WrapAngle360(0.0f + rotOff), 4.5f * scale, 70.0f, 180.0f, kBandModeBeginCasting);
    ConfigureBeginCastingBand(prim, 1, 0, 22.0f * scale, WrapAngle360(90.0f + rotOff), 5.0f * scale, 57.0f, 180.0f, kBandModeBeginCasting);
    ConfigureBeginCastingBand(prim, 2, 0, 19.0f * scale, WrapAngle360(180.0f + rotOff), 5.5f * scale, 45.0f, 180.0f, kBandModeBeginCasting);
    ConfigureBeginCastingBand(prim, 3, 0, 96.0f * scale, WrapAngle360(0.0f + rotOff), 4.0f * scale, 89.0f, 70.0f, kBandModeBeginCasting);
}

void ConfigureSaintCastingBands(CEffectPrim* prim, float scale, float rotOff, int startDelay)
{
    if (!prim) {
        return;
    }

    ConfigureBeginCastingBand(prim, 0, startDelay + 0, 20.0f * scale, WrapAngle360(180.0f + rotOff), 4.1f * scale, 80.0f, 180.0f, kBandModeSaintCasting);
    ConfigureBeginCastingBand(prim, 1, startDelay - 5, 19.0f * scale, WrapAngle360(270.0f + rotOff), 4.1f * scale, 80.0f, 180.0f, kBandModeSaintCasting);
    ConfigureBeginCastingBand(prim, 2, startDelay - 10, 18.0f * scale, WrapAngle360(0.0f + rotOff), 4.1f * scale, 80.0f, 180.0f, kBandModeSaintCasting);
    ConfigureBeginCastingBand(prim, 3, startDelay - 15, 17.0f * scale, WrapAngle360(90.0f + rotOff), 4.1f * scale, 80.0f, 180.0f, kBandModeSaintCasting);
}

// variant 0/1 match SpawnWarpZone2 m_stateCnt 1/2 band parameters; rotOff rotates all bands together.
void ConfigureWarpZoneCastingBands(CEffectPrim* prim, int variant, float rotOff)
{
    if (!prim) {
        return;
    }
    const bool a = variant == 0;
    ConfigureBand(prim, 0, 0, a ? 2.5f : 2.7f, WrapAngle360((a ? 270.0f : 271.0f) + rotOff), a ? 2.5f : 2.7f, a ? 53.0f : 52.0f, -1.0f, 0);
    ConfigureBand(prim, 1, 0, a ? 5.0f : 5.2f, WrapAngle360((a ? 0.0f : 1.0f) + rotOff), a ? 5.0f : 5.2f, a ? 60.0f : 59.0f, -1.0f, 0);
    ConfigureBand(prim, 2, 0, a ? 7.5f : 7.7f, WrapAngle360((a ? 90.0f : 91.0f) + rotOff), a ? 7.5f : 7.7f, a ? 55.0f : 54.0f, -1.0f, 0);
    ConfigureBand(prim, 3, 0, 10.0f, WrapAngle360((a ? 180.0f : 181.0f) + rotOff), 10.0f, a ? 50.0f : 49.0f, -1.0f, 0);
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

void RenderCastingBandLowPolygon(const CEffectPrim& prim,
    const vector3d& base,
    const EffectBandState& band,
    const matrix& viewMatrix,
    CTexture* texture)
{
    RenderBandLowPolygonTeiRect(prim, base, band, viewMatrix, texture, (std::max)(0.0f, band.distance));
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
        const u8 mode = band.modes[0];
        if ((mode == kBandModeBeginCasting || mode == kBandModeSaintCasting) && band.process < 0) {
            continue;
        }

        if (mode == kBandModeBeginCasting) {
            const float alphaCeiling = band.fadeThreshold >= 0.0f ? band.fadeThreshold : 180.0f;
            const float initialDistance = band.radius > 0.0f ? band.radius : (std::max)(band.distance, 1.0f);
            band.distance = (std::max)(0.0f, band.distance - 0.05f);
            band.riseAngle = 90.0f - band.distance * 9.0f;
            const float heightRatio = initialDistance > 0.0f ? band.distance / initialDistance : 0.0f;
            band.heights.fill((std::max)(0.0f, band.maxHeight * heightRatio));
            if (prim->m_stateCnt < prim->m_duration - 40) {
                band.alpha = (std::min)(alphaCeiling, band.alpha + (band.maxHeight > 30.0f ? 4.0f : 8.0f));
            } else {
                band.alpha = (std::max)(0.0f, band.alpha - (band.maxHeight > 30.0f ? 4.0f : 6.0f));
                if (band.alpha <= 0.0f) {
                    band.active = false;
                }
            }
            continue;
        }

        if (mode == kBandModeSaintCasting) {
            const float alphaCeiling = band.fadeThreshold >= 0.0f ? band.fadeThreshold : 180.0f;
            band.distance = band.radius > 0.0f ? band.radius : band.distance;
            band.riseAngle = 80.0f;
            const float growth = (std::min)(1.0f, static_cast<float>(band.process) / 16.0f);
            band.heights.fill((std::max)(0.0f, band.maxHeight * growth));
            if (prim->m_stateCnt < prim->m_duration - 20) {
                band.alpha = (std::min)(alphaCeiling, band.alpha + 10.0f);
            } else {
                band.alpha = (std::max)(0.0f, band.alpha - 8.0f);
                if (band.alpha <= 0.0f) {
                    band.active = false;
                }
            }
            continue;
        }

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
    constexpr int kTextureCount = 3;
    const int startIndex = rand() % kTextureCount;
    for (int offset = 0; offset < kTextureCount; ++offset) {
        if (CTexture* texture = ResolveEffectTextureCandidates({
                textureNames[(startIndex + offset) % kTextureCount],
            }, true)) {
            return texture;
        }
    }

    return GetSoftGlowTexture(true);
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

float ResolveWeatherCloudAlphaCeiling(int mapVariant)
{
    switch (mapVariant) {
    case 3:
        return 148.0f;
    case 5:
        return 176.0f;
    case 7:
        return 188.0f;
    case 8:
        return 208.0f;
    default:
        return 168.0f;
    }
}

float ResolveWeatherCloudWidthScale(int mapVariant)
{
    return mapVariant == 3 ? 5.8f : 7.8f;
}

float ResolveWeatherCloudHeightScale(int mapVariant)
{
    return mapVariant == 3 ? 3.2f : 4.4f;
}

COLORREF ResolveWeatherCloudTint(int mapVariant)
{
    switch (mapVariant) {
    case 3:
        return RGB(252, 171, 143);
    case 5:
        return RGB(94, 0, 0);
    case 7:
        return RGB(0, 0, 0);
    case 8:
        return RGB(255, 180, 180);
    default:
        return RGB(255, 255, 255);
    }
}

void InitWeatherCloudSegment(CEffectPrim* prim, int index, int mapVariant, const vector3d& center)
{
    if (!prim || index < 0 || index >= static_cast<int>(prim->m_segments.size())) {
        return;
    }

    EffectSegmentState& segment = prim->m_segments[static_cast<size_t>(index)];
    MatrixIdentity(segment.transform);

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
    switch (mapVariant) {
    case 0:
        offsetY = -125.0f + static_cast<float>(rand() % 10);
        offsetX = static_cast<float>(rand() % 300 - 150);
        offsetZ = static_cast<float>(rand() % 300 - 150);
        break;
    case 2:
        offsetY = static_cast<float>(rand() % 10);
        offsetX = static_cast<float>(rand() % 300 - 150);
        offsetZ = static_cast<float>(rand() % 300 - 150);
        break;
    case 3: {
        offsetX = static_cast<float>(rand() % 300 - 150);
        offsetZ = static_cast<float>(rand() % 300 - 150);
        const float sampleX = center.x + offsetX;
        const float sampleZ = center.z + offsetZ;
        const float groundY = g_world.m_attr ? g_world.m_attr->GetHeight(sampleX, sampleZ) : center.y;
        offsetY = groundY - center.y - 20.0f - static_cast<float>(rand() % 5);
        break;
    }
    case 5:
        offsetY = 20.0f + static_cast<float>(rand() % 10);
        offsetX = static_cast<float>(rand() % 300 - 150);
        offsetZ = static_cast<float>(rand() % 300 - 150);
        break;
    default:
        offsetY = 40.0f + static_cast<float>(rand() % 10);
        offsetX = static_cast<float>(rand() % 200 + 25);
        if ((rand() & 1) == 0) {
            offsetX = -offsetX;
        }
        offsetZ = static_cast<float>(rand() % 200 + 25);
        if ((rand() & 1) == 0) {
            offsetZ = -offsetZ;
        }
        break;
    }

    const vector3d origin = {
        center.x + offsetX,
        center.y + offsetY,
        center.z + offsetZ,
    };
    segment.pos = origin;
    segment.alpha = 0.0f;
    segment.process = 0;
    segment.displayAngle = static_cast<float>(rand() % 360);
    segment.phaseDegrees = static_cast<float>(rand() % 360);
    segment.radius = (mapVariant == 3 ? 35.0f : 30.0f) + static_cast<float>(rand() % (mapVariant == 3 ? 10 : 20));
    segment.size = 0.85f + static_cast<float>(rand() % 40) * 0.01f;

    const float basePhase = static_cast<float>(rand() % 360) * (kPi / 180.0f);
    const float speedScale = 0.08f + static_cast<float>(rand() % 30) * 0.004f;
    segment.transform.m[0][0] = ((rand() & 1) == 0 ? -1.0f : 1.0f) * speedScale;
    segment.transform.m[0][1] = mapVariant == 3 ? 0.0f : (0.01f + static_cast<float>(rand() % 10) * 0.0015f);
    segment.transform.m[0][2] = ((rand() & 1) == 0 ? -1.0f : 1.0f) * (speedScale * 0.6f);
    segment.transform.m[1][0] = static_cast<float>(rand() % 60);
    segment.transform.m[1][1] = basePhase;
    segment.transform.m[1][2] = ResolveWeatherCloudAlphaCeiling(mapVariant);
    segment.transform.m[2][0] = mapVariant == 3 ? 0.8f : 2.4f + static_cast<float>(rand() % 20) * 0.1f;
    segment.transform.m[2][1] = 0.01f + static_cast<float>(rand() % 8) * 0.0025f;
    segment.transform.m[2][2] = mapVariant == 3 ? 0.35f : 1.2f;
    segment.transform.m[3][0] = origin.x;
    segment.transform.m[3][1] = origin.y;
    segment.transform.m[3][2] = origin.z;
}

bool UpdateWeatherCloudPrimitive(CEffectPrim* prim)
{
    if (!prim) {
        return false;
    }

    ++prim->m_stateCnt;
    const int mapVariant = static_cast<int>(prim->m_param[0]);
    const float fadeInFrames = mapVariant == 3 ? 24.0f : 50.0f;
    const float fadeOutFrames = 96.0f;
    bool anyVisible = false;
    const int count = (std::min)(prim->m_numSegments, static_cast<int>(prim->m_segments.size()));
    for (int index = 0; index < count; ++index) {
        EffectSegmentState& segment = prim->m_segments[static_cast<size_t>(index)];
        const float delay = segment.transform.m[1][0];
        const float localTick = static_cast<float>(prim->m_stateCnt) - delay;
        if (localTick < 0.0f) {
            segment.alpha = 0.0f;
            continue;
        }

        const float remaining = static_cast<float>(prim->m_duration - prim->m_stateCnt);
        const float alphaCeiling = segment.transform.m[1][2];
        float alpha = (std::min)(alphaCeiling, localTick * (alphaCeiling / fadeInFrames));
        if (remaining < fadeOutFrames) {
            alpha = (std::min)(alpha, (std::max)(0.0f, remaining) * (alphaCeiling / fadeOutFrames));
        }
        segment.alpha = alpha;
        if (segment.alpha <= 0.0f) {
            continue;
        }

        const float phase = segment.transform.m[1][1] + localTick * segment.transform.m[2][1];
        const float sway = segment.transform.m[2][0];
        segment.pos.x = segment.transform.m[3][0] + segment.transform.m[0][0] * localTick + std::sin(phase) * sway;
        segment.pos.y = segment.transform.m[3][1] + segment.transform.m[0][1] * localTick + std::sin(phase * 0.47f) * segment.transform.m[2][2];
        segment.pos.z = segment.transform.m[3][2] + segment.transform.m[0][2] * localTick + std::cos(phase * 0.83f) * sway;
        segment.process = static_cast<int>(localTick);
        segment.displayAngle = std::fmod(segment.displayAngle + (mapVariant == 3 ? 0.18f : 0.32f), 360.0f);
        segment.phaseDegrees = std::fmod(segment.phaseDegrees + (mapVariant == 3 ? 0.9f : 1.4f), 360.0f);
        anyVisible = true;
    }

    return prim->m_stateCnt <= prim->m_duration || anyVisible;
}

bool UpdateWaterfallPrimitive(CEffectPrim* prim)
{
    if (!prim) {
        return false;
    }

    ++prim->m_stateCnt;
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

    ++prim->m_stateCnt;
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

static std::string StrBasenameLowerNoExt(const char* strPath)
{
    if (!strPath || !*strPath) {
        return std::string();
    }

    std::string path = CollapseUtf8Latin1ToBytes(strPath);
    std::replace(path.begin(), path.end(), '/', '\\');
    const size_t slash = path.find_last_of("\\/");
    std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    if (file.size() >= 4) {
        std::string lower = ToLowerAscii(file);
        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".str") == 0) {
            file = file.substr(0, file.size() - 4);
        }
    }
    return ToLowerAscii(file);
}

// GRF: data\wav\effect\wizard_<compact>.wav where compact is STR display name with only [a-z0-9] (e.g. storm gust -> stormgust).
static std::string CompactAlnumLowerAscii(std::string value)
{
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return out;
}

static std::string UnderscoreFromSpacesLower(std::string value)
{
    value = ToLowerAscii(std::move(value));
    for (char& ch : value) {
        if (ch == ' ') {
            ch = '_';
        }
    }
    return value;
}

static bool TryPlaySingleEffectWave(const vector3d& soundPos, const std::string& relativePath)
{
    if (relativePath.empty() || !g_soundMode || !g_isSoundOn) {
        return false;
    }

    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    const std::array<std::string, 3> candidates = {
        relativePath,
        std::string("wav\\") + relativePath,
        std::string("data\\wav\\") + relativePath,
    };

    for (const std::string& candidate : candidates) {
        if (audio->PlaySound3D(candidate.c_str(),
                soundPos,
                listenerPos,
                kRefEffectWaveMaxDist,
                kRefEffectWaveMinDist,
                1.0f)) {
            return true;
        }
    }
    return false;
}

// Ref RagEffect.cpp: most skill visuals use dedicated PlayWave paths; generic EzStr effects do not read
// audio from STR keyframes. Match common data layout: effect\ef_<strstem>.wav (and a few fallbacks).
static void PlayEzStrAssociatedStartupSound(const char* strResourcePath, int effectId, const vector3d& soundPos)
{
    if (!strResourcePath || !*strResourcePath || !g_soundMode || !g_isSoundOn) {
        return;
    }

    std::vector<std::string> tryPaths;
    if (effectId == 337) {
        tryPaths.push_back("effect\\levelup.wav");
        tryPaths.push_back("wav\\levelup.wav");
    }

    const std::string base = StrBasenameLowerNoExt(strResourcePath);
    if (!base.empty()) {
        const std::string compact = CompactAlnumLowerAscii(base);
        if (!compact.empty()) {
            tryPaths.push_back(std::string("effect\\wizard_") + compact + ".wav");
            if (compact.size() > 1) {
                std::string noTrailDigits = compact;
                while (!noTrailDigits.empty() && std::isdigit(static_cast<unsigned char>(noTrailDigits.back()))) {
                    noTrailDigits.pop_back();
                }
                if (!noTrailDigits.empty() && noTrailDigits != compact) {
                    tryPaths.push_back(std::string("effect\\wizard_") + noTrailDigits + ".wav");
                }
            }
            tryPaths.push_back(std::string("effect\\ef_") + compact + ".wav");
        }
        const std::string under = UnderscoreFromSpacesLower(base);
        if (!under.empty() && under != base) {
            tryPaths.push_back(std::string("effect\\ef_") + under + ".wav");
        }
        if (base.find(' ') == std::string::npos) {
            tryPaths.push_back(std::string("effect\\ef_") + base + ".wav");
        }
        tryPaths.push_back(std::string("effect\\") + under + ".wav");
    }

    for (const std::string& rel : tryPaths) {
        if (TryPlaySingleEffectWave(soundPos, rel)) {
            return;
        }
    }
}

bool TryPlayEffectWaveAt(const vector3d& soundPos, std::initializer_list<const char*> paths)
{
    for (const char* path : paths) {
        if (!path || !*path) {
            continue;
        }
        if (TryPlaySingleEffectWave(soundPos, std::string(path))) {
            return true;
        }
    }
    return false;
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
    const int motionIndex = prim.m_spriteRepeat
        ? (clampedBase + motionAdvance) % motionCount
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
    target->srcalpha = delta.srcalpha;
    target->destalpha = delta.destalpha;
    target->mtpreset = delta.mtpreset;
}

std::string ResolveStrName(int effectId)
{
    if (effectId == 49) {
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "FireHit%d.str", 1 + (rand() % 3));
        return ResolveWorldStrName(buffer);
    }

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

    ++prim->m_stateCnt;
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

    ++prim->m_stateCnt;
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

    ++prim->m_stateCnt;
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
    , m_spriteRepeat(false)
    , m_repeatAnim(true)
    , m_isDisappear(false)
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
    m_roll = 0.0f;
    m_rollSpeed = 0.0f;
    m_rollAccel = 0.0f;
    m_isDisappear = false;
    m_alphaDelta = 1.0f;
    m_minAlpha = 0.0f;
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
    if (m_type == PP_3DCROSSTEXTURE) {
        return UpdateCrossTexturePrimitive(this);
    }
    if (m_type == PP_2DTEXTURE || m_type == PP_2DFLASH || m_type == PP_2DCIRCLE) {
        return UpdateScreenTexturePrimitive(this);
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
    if (m_type == PP_WATERFALL) {
        return UpdateWaterfallPrimitive(this);
    }
    if (m_type == PP_CLOUD) {
        return UpdateWeatherCloudPrimitive(this);
    }
    if (m_type == PP_MAPPILLAR) {
        return UpdateMapPillarPrimitive(this);
    }
    if (m_type == PP_CASTINGRING4 && std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active; })) {
        UpdateCastingBands(this);
        ++m_stateCnt;
        return m_stateCnt <= m_duration || std::any_of(m_bands.begin(), m_bands.end(), [](const EffectBandState& band) { return band.active && band.alpha > 0.0f; });
    }

    ++m_stateCnt;
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

    const vector3d base = AddVec3(m_master->ResolveBasePosition(), m_deltaPos);
    const float normalizedAlpha = (std::max)(0.0f, (std::min)(255.0f, m_alpha));
    if (normalizedAlpha <= 0.0f
        && m_type != PP_MAPMAGICZONE
        && m_type != PP_MAPPARTICLE
        && m_type != PP_PORTALSTACK
        && m_type != PP_CASTINGRING4
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
    case PP_CLOUD: {
        CTexture* texture = !m_texture.empty() ? m_texture[0] : GetSoftGlowTexture(true);
        const int mapVariant = static_cast<int>(m_param[0]);
        const int renderFlags = ResolveEffectRenderFlags(m_renderFlag, 1 | 2);
        const D3DBLEND destBlend = ResolveEffectDestBlend(m_renderFlag);
        const COLORREF tintColor = ResolveWeatherCloudTint(mapVariant);
        const int count = (std::min)(m_numSegments, static_cast<int>(m_segments.size()));
        for (int index = 0; index < count; ++index) {
            const EffectSegmentState& segment = m_segments[static_cast<size_t>(index)];
            if (segment.alpha <= 0.0f) {
                continue;
            }

            const float sizePulse = 1.0f + std::sin(segment.phaseDegrees * (kPi / 180.0f)) * 0.05f;
            const float width = (std::max)(56.0f, segment.radius * ResolveWeatherCloudWidthScale(mapVariant) * segment.size * sizePulse);
            const float height = (std::max)(28.0f, segment.radius * ResolveWeatherCloudHeightScale(mapVariant) * segment.size * sizePulse);
            SubmitBillboard(segment.pos,
                *viewMatrix,
                texture,
                width,
                height,
                PackColor(static_cast<unsigned int>((std::max)(0.0f, (std::min)(255.0f, segment.alpha))), tintColor),
                destBlend,
                segment.displayAngle * (kPi / 180.0f),
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
            m_roll,
            color,
            destBlend,
            0.0f,
            renderFlags,
            kEffectSpriteDepthBias);
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
    , m_targetPos{}
    , m_grimToothStep{}
    , m_grimToothRemaining{}
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

    if (CAbleToMakeEffect* owner = ResolveCurrentEffectOwner()) {
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
    m_longitude = master ? master->m_roty : 0.0f;
    m_handler = Handler::None;
    m_loop = false;
    m_duration = 100;
    m_stateCnt = 0;
    m_lastProcessTick = timeGetTime();
    m_tickCarryMs = kEffectTickMs;
    m_targetPos = vector3d{};
    m_hasTargetPos = false;
    m_grimToothStep = vector3d{};
    m_grimToothRemaining = vector3d{};

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
        if (m_ezEffectRes) {
            PlayEzStrAssociatedStartupSound("joblvup.str", effectId, ResolveBasePosition());
        }
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
        // Base level-up: GameModePacket::PlayBaseLevelUpPresentation already plays levelup.wav. angel.str maps to
        // wizard_angel / ef_angel SFX which reads as Angelus — skip duplicate startup audio here.
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
    case 331:
        m_handler = Handler::RecoveryHp;
        m_duration = 30;
        break;
    case 332:
        m_handler = Handler::RecoverySp;
        m_duration = 30;
        break;
    case 12:
        m_handler = Handler::BeginCasting;
        m_duration = 56;
        break;
    case 312:
        m_handler = Handler::HealLight;
        m_duration = 26;
        break;
    case 313:
        m_handler = Handler::HealMedium;
        m_duration = 34;
        break;
    case 325:
        m_handler = Handler::HealLarge;
        m_duration = 40;
        break;
    case 159:
        m_handler = Handler::HideStart;
        m_duration = 14;
        break;
    case 123:
        m_handler = Handler::GrimTooth;
        m_duration = 24;
        break;
    case 132:
        m_handler = Handler::GrimToothAtk;
        m_duration = 10;
        break;
    case 54:
    case 55:
    case 56:
    case 57:
    case 59:
        m_handler = Handler::BeginCasting;
        m_duration = 70;
        break;
    case 58:
        m_handler = Handler::BeginCasting;
        m_duration = 56;
        break;
    case 22:
        m_handler = Handler::Sight;
        m_duration = 20;
        break;
    case 24:
        m_handler = Handler::FireBall;
        m_duration = 40;
        break;
    case 229:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 0.0f;
        m_param[1] = 40.0f;
        break;
    case 230:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 1.0f;
        m_param[1] = 40.0f;
        break;
    case 231:
        m_handler = Handler::MapPillar;
        m_duration = static_cast<int>(kMapPillarDurationFrames);
        break;
    case 233:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 2.0f;
        m_param[1] = 40.0f;
        break;
    case 515:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 3.0f;
        m_param[1] = 40.0f;
        break;
    case 516:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 4.0f;
        m_param[1] = 40.0f;
        break;
    case 592:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 5.0f;
        m_param[1] = 40.0f;
        break;
    case 697:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 7.0f;
        m_param[1] = 80.0f;
        break;
    case 698:
        m_handler = Handler::WeatherCloud;
        m_duration = static_cast<int>(kWeatherCloudDurationFrames);
        m_param[0] = 8.0f;
        m_param[1] = 80.0f;
        break;
    case 33:
        m_handler = Handler::Ruwach;
        m_duration = 25;
        break;
    case 247:
        m_handler = Handler::MapPillar;
        m_duration = static_cast<int>(kMapPillarDurationFrames);
        break;
    case 37:
    case 43:
        m_handler = Handler::IncAgility;
            m_duration = 100;
        break;
    case 20:
    case 125:
        m_handler = Handler::EnchantPoison;
        m_duration = 40;
        break;
    case 493:
        m_handler = Handler::EnchantPoison2;
        m_duration = 80;
        break;
    case 42:
        m_handler = Handler::Blessing;
        m_duration = 60;
        break;
    case 44:
        m_handler = Handler::Smoke;
        m_duration = 100;
        break;
    case 601:
        m_handler = Handler::SightState;
        m_duration = 20;
        m_loop = true;
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
            if (m_ezEffectRes) {
                PlayEzStrAssociatedStartupSound(strName.c_str(), effectId, ResolveBasePosition());
            }
            if (m_aniClips && m_aniClips->cFrame > 0) {
                m_duration = (std::max)(m_duration, m_aniClips->cFrame + 8);
            }
        }
        break;
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
    m_master = new CWorldAnchor(source.pos);
    m_ownsMaster = true;
    m_masterIsActor = false;
    m_masterActorGid = 0;
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
        if (m_ezEffectRes) {
            PlayEzStrAssociatedStartupSound(worldStrName.c_str(), m_type, ResolveBasePosition());
        }
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
        m_handler = Handler::Smoke;
    } else if (source.type == 47) {
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
    return prim;
}

void CRagEffect::DetachFromMaster(CRenderObject* knownMaster)
{
    CRenderObject* ownerMaster = knownMaster;
    if (!ownerMaster && !m_masterIsActor) {
        ownerMaster = ResolveCurrentMaster();
    }

    const vector3d masterPos = ownerMaster ? ownerMaster->m_pos : m_cachedPos;
    const float masterRot = ownerMaster ? ownerMaster->m_roty : m_longitude;
    const vector3d base = AddVec3(masterPos, m_deltaPos);

    if (CAbleToMakeEffect* owner = dynamic_cast<CAbleToMakeEffect*>(ownerMaster)) {
        if (owner->m_beginSpellEffect == this) {
            owner->m_beginSpellEffect = nullptr;
        }
        if (owner->m_magicTargetEffect == this) {
            owner->m_magicTargetEffect = nullptr;
        }
    }

    m_cachedPos = base;
    m_longitude = masterRot;

    if (m_ownsMaster) {
        m_master = nullptr;
        return;
    }

    m_master = new CWorldAnchor(base);
    m_ownsMaster = true;
    m_masterIsActor = false;
    m_masterActorGid = 0;
    m_deltaPos = vector3d{};
}

void CRagEffect::OnActorDeleted(const CGameActor* actor)
{
    if (!actor) {
        return;
    }

    if (m_masterIsActor && actor->m_gid == m_masterActorGid) {
        DetachFromMaster(const_cast<CGameActor*>(actor));
        return;
    }

    if (static_cast<const CRenderObject*>(actor) == m_master) {
        DetachFromMaster(const_cast<CGameActor*>(actor));
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
    if (CRenderObject* master = ResolveCurrentMaster()) {
        return AddVec3(master->m_pos, m_deltaPos);
    }
    return AddVec3(m_cachedPos, m_deltaPos);
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
    case Handler::Portal2:
    case Handler::ReadyPortal2:
    case Handler::WarpZone2:
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
    case Handler::Entry2:
    case Handler::JobLevelUp50:
    case Handler::SuperAngel:
    case Handler::RecoveryHp:
    case Handler::RecoverySp:
    case Handler::HealLight:
    case Handler::HealMedium:
    case Handler::HealLarge:
    case Handler::HideStart:
    case Handler::GrimTooth:
    case Handler::GrimToothAtk:
    case Handler::Sight:
    case Handler::SightState:
    case Handler::FireBall:
    case Handler::Ruwach:
    case Handler::SightAura:
    case Handler::FireBoltRain:
        radius = 140.0f;
        break;
    case Handler::None:
    default:
        radius = 96.0f;
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

void CRagEffect::SpawnPortal2()
{
    const int variant = static_cast<int>(m_param[0]);
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);
    CTexture* ringRed = ResolveEffectTextureCandidates({
        "effect\\ring_red.tga",
        "effect\\ring_red.bmp",
    }, false);
    CTexture* magicViolet = ResolveEffectTextureCandidates({
        "effect\\magic_violet.tga",
        "effect\\magic_violet.bmp",
    }, true);
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
        if (CEffectPrim* prim = LaunchEffectPrim(PP_CASTINGRING4, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(variant == 3 ? ringRed : magicViolet);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = variant == 3 ? 10.0f : 4.0f;
            ConfigureWarpZoneCastingBands(prim, 0, static_cast<float>(rand() % 360));
        }
    }

    if (m_stateCnt == 1) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_PORTAL, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(variant == 3 ? ringRed : ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = variant == 3 ? 10.0f : 4.0f;
            prim->m_param[0] = 0.0f;
            ConfigureBand(prim, 0, 0, 6.0009999f, 0.0f, 0.0f, 2.0f, 3111.0f, 0);
            ConfigureBand(prim, 1, variant == 3 ? -20 : -10, 6.0009999f, 25.0f, 0.0f, 3.0f, 3111.0f, 0);
            ConfigureBand(prim, 2, variant == 3 ? -40 : -20, 6.0009999f, 50.0f, 0.0f, 4.0f, 3111.0f, 0);
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
            ConfigureWarpZoneCastingBands(prim, m_stateCnt == 1 ? 0 : 1, 0.0f);
        }
    }

    if ((m_stateCnt % 10) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEORBIT, vector3d{})) {
            prim->m_renderFlag = 5u;
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

        if (CEffectPrim* prim = LaunchEffectPrim(PP_CASTINGRING4, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(255, 255, 255);
            prim->m_size = 4.0f;
            ConfigureWarpZoneCastingBands(prim, 0, static_cast<float>(rand() % 360));
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
    CTexture* magicBlue = ResolveEffectTextureCandidates({
        "effect\\magic_blue.tga",
        "effect\\magic_blue.bmp",
    }, false);

    auto spawnBurst = [&](CTexture* texture, COLORREF tintColor, float startAngle, float radiusSpeed, float size) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_RADIALSLASH, vector3d{})) {
            prim->m_duration = 42;
            prim->m_spawnCount = 4;
            prim->m_radius = 0.8f;
            prim->m_radiusSpeed = radiusSpeed;
            prim->m_heightSize = 2.0f;
            prim->m_alpha = 0.0f;
            prim->m_alphaSpeed = 22.0f;
            prim->m_maxAlpha = 220.0f;
            prim->m_fadeOutCnt = 26;
            prim->m_size = size;
            prim->m_texture.push_back(texture);
            prim->m_tintColor = tintColor;
            prim->m_param[0] = startAngle;
            prim->m_param[1] = 7.0f;
            prim->m_param[2] = 12.0f;
        }
    };

    spawnBurst(magicGreen, RGB(172, 255, 188), 0.0f, 0.22f, 1.0f);
    spawnBurst(magicBlue, RGB(156, 216, 255), 45.0f, 0.16f, 0.8f);
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
    if (m_stateCnt != 0 || !m_master) {
        return;
    }

    const int mapVariant = static_cast<int>(m_param[0]);
    const int cloudCount = (std::max)(1, static_cast<int>(m_param[1]));
    const vector3d center = ResolveBasePosition();

    for (int cloudIndex = 0; cloudIndex < cloudCount; ++cloudIndex) {
        CEffectPrim* prim = LaunchEffectPrim(PP_CLOUD, vector3d{});
        if (!prim) {
            continue;
        }

        prim->m_renderFlag = 5u;
        prim->m_duration = m_duration;
        prim->m_alpha = 255.0f;
        prim->m_maxAlpha = 255.0f;
        prim->m_numSegments = 4;
        prim->m_param[0] = static_cast<float>(mapVariant);
        prim->m_tintColor = ResolveWeatherCloudTint(mapVariant);
        prim->m_texture.push_back(ResolveWeatherCloudTexture(mapVariant));
        for (int segmentIndex = 0; segmentIndex < prim->m_numSegments; ++segmentIndex) {
            InitWeatherCloudSegment(prim, segmentIndex, mapVariant, center);
        }
    }
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
        // Ref RagEffect OnProcess case 0x152 (effect 338): PlayWave(levelup) only at frame 0, not ef_angel.
        TryPlayEffectWaveAt(ResolveBasePosition(), {
            "effect\\levelup.wav",
            "wav\\levelup.wav",
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

void CRagEffect::SpawnRecoveryHp()
{
    const vector3d base = ResolveBasePosition();
    CTexture* alphaDown = ResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    }, false);
    CTexture* ringWhite = ResolveEffectTextureCandidates({
        "effect\\ring_white.tga",
        "effect\\ring_white.bmp",
        "effect\\ring_blue.tga",
    }, false);
    CTexture* sparkTexture = ResolveEffectTextureCandidates({
        "effect\\alpha_center.tga",
        "effect\\torch_green11.bmp",
        "effect\\torch_green10.bmp",
        "effect\\ring_white.tga",
    }, false);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            "effect\\priest_recovery.wav",
        });

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 9.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.1f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.45f;
            prim->m_alpha = 170.0f;
            prim->m_maxAlpha = 170.0f;
            prim->m_fadeOutCnt = prim->m_duration - 12;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(120, 255, 150);
        }

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -4.0f;
            prim->m_arcAngle = 36.0f;
            prim->m_innerSize = 1.8f;
            prim->m_outerSize = 4.4f;
            prim->m_innerSpeed = 0.03f;
            prim->m_outerSpeed = 0.09f;
            prim->m_heightSize = 28.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 120.0f;
            prim->m_alphaSpeed = 18.0f;
            prim->m_fadeOutCnt = prim->m_duration - 10;
            prim->m_texture.push_back(ringWhite);
            prim->m_tintColor = RGB(228, 255, 228);
        }
    }

    if ((m_stateCnt % 2) != 0 || m_stateCnt > 14) {
        return;
    }

    for (int index = 0; index < 3; ++index) {
        const float angleDegrees = static_cast<float>(rand() % 360);
        const float angleRadians = angleDegrees * (kPi / 180.0f);
        const float radius = static_cast<float>(rand() % 6 + 2);
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = 18 + rand() % 6;
            prim->m_deltaPos2 = {
                std::sin(angleRadians) * radius,
                -16.0f + static_cast<float>(rand() % 6),
                -std::cos(angleRadians) * radius
            };
            prim->m_latitude = -90.0f;
            prim->m_longitude = angleDegrees;
            prim->m_speed = 0.26f + static_cast<float>(rand() % 8) * 0.02f;
            prim->m_accel = -prim->m_speed / static_cast<float>(prim->m_duration) * 0.3f;
            prim->m_size = 1.2f + static_cast<float>(rand() % 5) * 0.15f;
            prim->m_sizeSpeed = -0.02f;
            prim->m_alpha = 220.0f;
            prim->m_maxAlpha = 220.0f;
            prim->m_fadeOutCnt = prim->m_duration - 8;
            prim->m_texture.push_back(sparkTexture);
            prim->m_tintColor = RGB(220, 255, 220);
        }
    }
}

void CRagEffect::SpawnRecoverySp()
{
    const vector3d base = ResolveBasePosition();
    CTexture* alphaDown = ResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    }, false);
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);
    CTexture* sparkTexture = ResolveEffectTextureCandidates({
        "effect\\alpha_center.tga",
        "effect\\magic_blue.tga",
        "effect\\ring_blue.tga",
    }, false);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            "effect\\priest_recovery.wav",
        });

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 9.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.0f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.4f;
            prim->m_alpha = 150.0f;
            prim->m_maxAlpha = 150.0f;
            prim->m_fadeOutCnt = prim->m_duration - 12;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(96, 180, 255);
        }

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -4.0f;
            prim->m_arcAngle = 36.0f;
            prim->m_innerSize = 2.0f;
            prim->m_outerSize = 4.8f;
            prim->m_innerSpeed = 0.04f;
            prim->m_outerSpeed = 0.1f;
            prim->m_heightSize = 30.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 112.0f;
            prim->m_alphaSpeed = 16.0f;
            prim->m_fadeOutCnt = prim->m_duration - 10;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(156, 220, 255);
        }
    }

    if ((m_stateCnt % 2) != 0 || m_stateCnt > 14) {
        return;
    }

    for (int index = 0; index < 4; ++index) {
        const float angleDegrees = static_cast<float>(rand() % 360);
        const float angleRadians = angleDegrees * (kPi / 180.0f);
        const float radius = static_cast<float>(rand() % 7 + 2);
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = 20 + rand() % 6;
            prim->m_deltaPos2 = {
                std::sin(angleRadians) * radius,
                -18.0f + static_cast<float>(rand() % 8),
                -std::cos(angleRadians) * radius
            };
            prim->m_latitude = -90.0f;
            prim->m_longitude = angleDegrees;
            prim->m_speed = 0.24f + static_cast<float>(rand() % 8) * 0.02f;
            prim->m_accel = -prim->m_speed / static_cast<float>(prim->m_duration) * 0.25f;
            prim->m_size = 1.1f + static_cast<float>(rand() % 5) * 0.12f;
            prim->m_sizeSpeed = -0.015f;
            prim->m_alpha = 210.0f;
            prim->m_maxAlpha = 210.0f;
            prim->m_fadeOutCnt = prim->m_duration - 8;
            prim->m_texture.push_back(sparkTexture);
            prim->m_tintColor = RGB(180, 228, 255);
        }
    }
}

void CRagEffect::SpawnHealLight()
{
    CTexture* alphaDown = ResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    }, false);
    CTexture* sparkTexture = ResolveEffectTextureCandidates({
        "effect\\alpha_center.tga",
        "effect\\ring_white.tga",
    }, false);

    if (m_stateCnt == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 8.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 0.95f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.4f;
            prim->m_alpha = 150.0f;
            prim->m_maxAlpha = 150.0f;
            prim->m_fadeOutCnt = prim->m_duration - 10;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(176, 255, 176);
        }
    }

    if ((m_stateCnt % 2) != 0 || m_stateCnt > 8) {
        return;
    }

    for (int index = 0; index < 2; ++index) {
        const float angleDegrees = static_cast<float>(rand() % 360);
        const float angleRadians = angleDegrees * (kPi / 180.0f);
        const float radius = static_cast<float>(rand() % 4 + 1);
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = 16;
            prim->m_deltaPos2 = {
                std::sin(angleRadians) * radius,
                -16.0f + static_cast<float>(rand() % 6),
                -std::cos(angleRadians) * radius
            };
            prim->m_latitude = -90.0f;
            prim->m_longitude = angleDegrees;
            prim->m_speed = 0.22f;
            prim->m_accel = -prim->m_speed / static_cast<float>(prim->m_duration) * 0.25f;
            prim->m_size = 1.0f;
            prim->m_sizeSpeed = -0.02f;
            prim->m_alpha = 180.0f;
            prim->m_maxAlpha = 180.0f;
            prim->m_fadeOutCnt = prim->m_duration - 7;
            prim->m_texture.push_back(sparkTexture);
            prim->m_tintColor = RGB(228, 255, 228);
        }
    }
}

void CRagEffect::SpawnHealMedium()
{
    CTexture* alphaDown = ResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    }, false);
    CTexture* ringWhite = ResolveEffectTextureCandidates({
        "effect\\ring_white.tga",
        "effect\\ring_white.bmp",
        "effect\\ring_blue.tga",
    }, false);
    CTexture* sparkTexture = ResolveEffectTextureCandidates({
        "effect\\alpha_center.tga",
        "effect\\ring_white.tga",
        "effect\\torch_green11.bmp",
    }, false);

    if (m_stateCnt == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 9.5f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.1f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.42f;
            prim->m_alpha = 165.0f;
            prim->m_maxAlpha = 165.0f;
            prim->m_fadeOutCnt = prim->m_duration - 12;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(156, 255, 176);
        }

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -4.0f;
            prim->m_arcAngle = 36.0f;
            prim->m_innerSize = 1.8f;
            prim->m_outerSize = 4.5f;
            prim->m_innerSpeed = 0.03f;
            prim->m_outerSpeed = 0.08f;
            prim->m_heightSize = 28.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 112.0f;
            prim->m_alphaSpeed = 16.0f;
            prim->m_fadeOutCnt = prim->m_duration - 10;
            prim->m_texture.push_back(ringWhite);
            prim->m_tintColor = RGB(236, 255, 236);
        }
    }

    if ((m_stateCnt % 2) != 0 || m_stateCnt > 12) {
        return;
    }

    for (int index = 0; index < 3; ++index) {
        const float angleDegrees = static_cast<float>(rand() % 360);
        const float angleRadians = angleDegrees * (kPi / 180.0f);
        const float radius = static_cast<float>(rand() % 6 + 2);
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = 18 + rand() % 4;
            prim->m_deltaPos2 = {
                std::sin(angleRadians) * radius,
                -17.0f + static_cast<float>(rand() % 8),
                -std::cos(angleRadians) * radius
            };
            prim->m_latitude = -90.0f;
            prim->m_longitude = angleDegrees;
            prim->m_speed = 0.25f + static_cast<float>(rand() % 5) * 0.02f;
            prim->m_accel = -prim->m_speed / static_cast<float>(prim->m_duration) * 0.25f;
            prim->m_size = 1.15f + static_cast<float>(rand() % 4) * 0.12f;
            prim->m_sizeSpeed = -0.015f;
            prim->m_alpha = 210.0f;
            prim->m_maxAlpha = 210.0f;
            prim->m_fadeOutCnt = prim->m_duration - 7;
            prim->m_texture.push_back(sparkTexture);
            prim->m_tintColor = RGB(232, 255, 232);
        }
    }
}

void CRagEffect::SpawnHealLarge()
{
    CTexture* alphaDown = ResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    }, false);
    CTexture* ringWhite = ResolveEffectTextureCandidates({
        "effect\\ring_white.tga",
        "effect\\ring_white.bmp",
        "effect\\ring_blue.tga",
    }, false);
    CTexture* sparkTexture = ResolveEffectTextureCandidates({
        "effect\\alpha_center.tga",
        "effect\\ring_white.tga",
        "effect\\torch_green12.bmp",
    }, false);

    if (m_stateCnt == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 11.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.2f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.35f;
            prim->m_alpha = 175.0f;
            prim->m_maxAlpha = 175.0f;
            prim->m_fadeOutCnt = prim->m_duration - 14;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(180, 255, 196);
        }

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 14.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 1.45f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.35f;
            prim->m_alpha = 120.0f;
            prim->m_maxAlpha = 120.0f;
            prim->m_fadeOutCnt = prim->m_duration - 12;
            prim->m_texture.push_back(ringWhite);
            prim->m_tintColor = RGB(255, 255, 255);
        }

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCYLINDER, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = m_duration;
            prim->m_deltaPos2.y = -5.0f;
            prim->m_arcAngle = 36.0f;
            prim->m_innerSize = 2.2f;
            prim->m_outerSize = 5.2f;
            prim->m_innerSpeed = 0.04f;
            prim->m_outerSpeed = 0.11f;
            prim->m_heightSize = 34.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 124.0f;
            prim->m_alphaSpeed = 18.0f;
            prim->m_fadeOutCnt = prim->m_duration - 12;
            prim->m_texture.push_back(ringWhite);
            prim->m_tintColor = RGB(240, 255, 240);
        }
    }

    if ((m_stateCnt % 2) != 0 || m_stateCnt > 16) {
        return;
    }

    for (int index = 0; index < 4; ++index) {
        const float angleDegrees = static_cast<float>(rand() % 360);
        const float angleRadians = angleDegrees * (kPi / 180.0f);
        const float radius = static_cast<float>(rand() % 8 + 2);
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = 20 + rand() % 6;
            prim->m_deltaPos2 = {
                std::sin(angleRadians) * radius,
                -18.0f + static_cast<float>(rand() % 10),
                -std::cos(angleRadians) * radius
            };
            prim->m_latitude = -90.0f;
            prim->m_longitude = angleDegrees;
            prim->m_speed = 0.28f + static_cast<float>(rand() % 6) * 0.025f;
            prim->m_accel = -prim->m_speed / static_cast<float>(prim->m_duration) * 0.22f;
            prim->m_size = 1.2f + static_cast<float>(rand() % 5) * 0.15f;
            prim->m_sizeSpeed = -0.012f;
            prim->m_alpha = 220.0f;
            prim->m_maxAlpha = 220.0f;
            prim->m_fadeOutCnt = prim->m_duration - 8;
            prim->m_texture.push_back(sparkTexture);
            prim->m_tintColor = RGB(244, 255, 244);
        }
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

void CRagEffect::SpawnGrimTooth()
{
    if (!m_hasTargetPos) {
        return;
    }

    const vector3d base = ResolveBasePosition();
    const vector3d totalDelta = {
        m_targetPos.x - base.x,
        m_targetPos.y - base.y,
        m_targetPos.z - base.z,
    };
    const float totalDistance = std::sqrt(totalDelta.x * totalDelta.x + totalDelta.z * totalDelta.z);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            "effect\\ef_frostdiver.wav",
            "effect\\EF_FrostDiver.wav",
        });
    }

    if (m_flag != 0) {
        return;
    }

    if ((m_stateCnt % 3) == 0) {
        const float traveledDistance = std::sqrt(
            m_grimToothRemaining.x * m_grimToothRemaining.x
            + m_grimToothRemaining.z * m_grimToothRemaining.z);
        if (traveledDistance >= (std::max)(0.0f, totalDistance - 2.5f)) {
            m_flag = 1;
        }

        CTexture* stoneTexture = ResolveEffectTextureCandidates({
            "effect\\stone.bmp",
            "effect\\stone.tga",
        }, false);
        if (stoneTexture) {
            if (CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN, vector3d{})) {
                prim->m_renderFlag = 0u;
                prim->m_pattern |= 1;
                prim->m_duration = 40;
                prim->m_deltaPos2 = {
                    m_grimToothRemaining.x,
                    20.0f + totalDelta.y * (totalDistance > 0.0f ? (traveledDistance / totalDistance) : 0.0f),
                    m_grimToothRemaining.z,
                };
                prim->m_size = static_cast<float>(rand() % 40 + 60) * 0.01f;
                prim->m_heightSize = 10.0f;
                prim->m_longitude = static_cast<float>(rand() % 360);
                prim->m_latitude = static_cast<float>(rand() % 30 + 75);
                MatrixIdentity(prim->m_matrix);
                MatrixAppendXRotation(prim->m_matrix, prim->m_latitude * (kPi / 180.0f));
                MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));
                prim->m_speed = 3.0f;
                prim->m_accel = prim->m_speed / static_cast<float>(prim->m_duration) * -2.0f;
                prim->m_alpha = 255.0f;
                prim->m_maxAlpha = 255.0f;
                prim->m_fadeOutCnt = prim->m_duration - 10;
                prim->m_texture.push_back(stoneTexture);
                prim->m_tintColor = RGB(255, 255, 255);
                if (m_stateCnt <= 6) {
                    DbgLog("[Projectile] grimtooth effect123 step=%d base=(%.2f,%.2f,%.2f) travel=(%.2f,%.2f,%.2f) total=(%.2f,%.2f,%.2f)\n",
                        m_stateCnt,
                        base.x,
                        base.y,
                        base.z,
                        prim->m_deltaPos2.x,
                        prim->m_deltaPos2.y,
                        prim->m_deltaPos2.z,
                        totalDelta.x,
                        totalDelta.y,
                        totalDelta.z);
                }
            }
        }
    }

    m_grimToothRemaining = {
        m_grimToothRemaining.x + m_grimToothStep.x,
        m_grimToothRemaining.y + m_grimToothStep.y,
        m_grimToothRemaining.z + m_grimToothStep.z,
    };
}

void CRagEffect::SpawnGrimToothAtk()
{
    if (m_stateCnt != 0) {
        return;
    }

    CTexture* stoneTexture = ResolveEffectTextureCandidates({
        "effect\\stone.bmp",
        "effect\\stone.tga",
    }, false);
    if (!stoneTexture) {
        return;
    }

    const vector3d offsets[3] = {
        { 0.0f, 40.0f, -12.0f },
        { 12.0f, 40.0f, 6.0f },
        { -12.0f, 40.0f, 6.0f },
    };

    for (int index = 0; index < 3; ++index) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DQUADHORN, vector3d{})) {
            prim->m_renderFlag = 0u;
            prim->m_duration = 24;
            prim->m_deltaPos2 = offsets[index];
            prim->m_size = 0.9f;
            prim->m_heightSize = 25.0f;
            prim->m_longitude = static_cast<float>(120 * (6 - index));
            prim->m_latitude = 75.0f;
            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix, prim->m_latitude * (kPi / 180.0f));
            MatrixAppendYRotation(prim->m_matrix, prim->m_longitude * (kPi / 180.0f));
            prim->m_alpha = 255.0f;
            prim->m_fadeOutCnt = prim->m_duration - 8;
            prim->m_texture.push_back(stoneTexture);
            prim->m_tintColor = RGB(255, 255, 255);
        }
    }
}

void CRagEffect::SpawnBeginCasting()
{
    if (m_stateCnt != 0) {
        return;
    }

    const vector3d base = ResolveBasePosition();
    TryPlayEffectWaveAt(base, {
        "effect\\EF_BeginSpell.wav",
        "effect\\ef_beginspell.wav",
    });

    const auto launchCastingPrim = [&](CTexture* texture, COLORREF tintColor, float scale, bool saintCasting, int startDelay, float rotOffset) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_CASTINGRING4, vector3d{})) {
            prim->m_renderFlag = 5u;
            prim->m_duration = saintCasting ? (std::max)(m_duration, 56) : (std::max)(m_duration, 70);
            prim->m_texture.push_back(texture);
            prim->m_tintColor = tintColor;
            if (saintCasting) {
                ConfigureSaintCastingBands(prim, scale, rotOffset, startDelay);
            } else {
                ConfigureStandardBeginCastingBands(prim, scale, rotOffset);
            }
        }
    };

    switch (m_type) {
    case 12: {
        CTexture* ringYellow = ResolveEffectTextureCandidates({
            "effect\\ring_yellow.tga",
            "effect\\ring_yellow.bmp",
            "effect\\ring_white.tga",
        }, false);
        launchCastingPrim(ringYellow, RGB(255, 236, 160), 1.15f, true, 0, 0.0f);
        launchCastingPrim(ringYellow, RGB(255, 224, 140), 1.0f, true, 0, 22.5f);
        break;
    }
    case 54: {
        CTexture* ringBlue = ResolveEffectTextureCandidates({
            "effect\\ring_blue.tga",
            "effect\\ring_blue.bmp",
        }, false);
        launchCastingPrim(ringBlue, RGB(224, 240, 255), 1.08f, false, 0, 0.0f);
        break;
    }
    case 55: {
        CTexture* ringYellow = ResolveEffectTextureCandidates({
            "effect\\ring_yellow.tga",
            "effect\\ring_yellow.bmp",
            "effect\\ring_white.tga",
        }, false);
        launchCastingPrim(ringYellow, RGB(255, 236, 172), 1.0f, false, 0, 0.0f);
        break;
    }
    case 56: {
        CTexture* magicGreen = ResolveEffectTextureCandidates({
            "effect\\magic_green.tga",
            "effect\\magic_green.bmp",
            "effect\\Magic_Green.tga",
        }, false);
        launchCastingPrim(magicGreen, RGB(180, 255, 196), 1.0f, false, 0, 0.0f);
        break;
    }
    case 57: {
        CTexture* ringYellow = ResolveEffectTextureCandidates({
            "effect\\ring_yellow.tga",
            "effect\\ring_yellow.bmp",
            "effect\\ring_white.tga",
        }, false);
        launchCastingPrim(ringYellow, RGB(255, 220, 132), 0.92f, false, 0, 0.0f);
        break;
    }
    case 58: {
        CTexture* ringWhite = ResolveEffectTextureCandidates({
            "effect\\ring_white.tga",
            "effect\\ring_white.bmp",
            "effect\\ring_blue.tga",
        }, false);
        launchCastingPrim(ringWhite, RGB(255, 255, 255), 1.0f, true, 0, 0.0f);
        launchCastingPrim(ringWhite, RGB(236, 244, 255), 0.92f, true, 0, 22.5f);
        break;
    }
    case 59: {
        CTexture* magicBlue = ResolveEffectTextureCandidates({
            "effect\\magic_blue.tga",
            "effect\\magic_blue.bmp",
            "effect\\ring_white.tga",
            "effect\\ring_blue.tga",
        }, false);
        launchCastingPrim(magicBlue, RGB(224, 240, 255), 1.0f, false, 0, 0.0f);
        break;
    }
    default:
        break;
    }
}

void CRagEffect::SpawnIncAgility()
{
    const bool dexVariant = m_type == 43;
    const vector3d base = ResolveBasePosition();
    CTexture* overlayTexture = ResolveEffectTextureCandidates({
        dexVariant ? "effect\\dex_agi_up.bmp" : "effect\\agi_up.bmp",
        dexVariant ? "effect\\agi_up.bmp" : "effect\\dex_agi_up.bmp",
        "effect\\alpha_center.tga",
    }, false);
    CTexture* trailTexture = ResolveEffectTextureCandidates({
        "effect\\ac_center2.tga",
        "effect\\alpha_center.tga",
        "effect\\magic_blue.tga",
        "effect\\magic_blue.bmp",
    }, false);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            dexVariant ? "effect\\EF_IncAgiDex.wav" : "effect\\EF_IncAgility.wav",
            "effect\\ef_incagi.wav",
        });

        if (CEffectPrim* prim = LaunchEffectPrim(PP_2DTEXTURE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_longitude = 0.0f;
            prim->m_speed = 1.5f;
            prim->m_accel = prim->m_speed / static_cast<float>(prim->m_duration) * -1.2f;
            prim->m_outerSize = 40.0f / 12.0f;
            prim->m_heightSize = 20.0f / 12.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 200.0f;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.06666667f;
            prim->m_fadeOutCnt = prim->m_duration - 15;
            prim->m_texture.push_back(overlayTexture);
            prim->m_tintColor = RGB(255, 255, 255);
        }
    }

    if ((m_stateCnt % 2) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCROSSTEXTURE, vector3d{})) {
            prim->m_duration = 50;
            const float angle = static_cast<float>(rand() % 360);
            MatrixIdentity(prim->m_matrix);
            MatrixAppendYRotation(prim->m_matrix, angle * (kPi / 180.0f));

            const float radius = static_cast<float>(rand() % 7 + 2);
            prim->m_deltaPos2 = {
                radius * prim->m_matrix.m[2][0] + prim->m_matrix.m[3][0],
                radius * prim->m_matrix.m[2][1] + prim->m_matrix.m[3][1],
                radius * prim->m_matrix.m[2][2] + prim->m_matrix.m[3][2]
            };

            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix, 90.0f * (kPi / 180.0f));
            prim->m_speed = static_cast<float>(rand() % 50 + 20) * 0.01f;
            prim->m_size = static_cast<float>(rand() % 60 + 30) * (0.1f / 34.0f);
            prim->m_heightSize = 0.18000001f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 200.0f;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.050000001f;
            prim->m_fadeOutCnt = prim->m_duration - 20;
            prim->m_texture.push_back(trailTexture);
            prim->m_tintColor = RGB(255, 255, 255);
        }
    }
}

void CRagEffect::SpawnBlessing()
{
    const vector3d base = ResolveBasePosition();
    CTexture* alphaDown = ResolveEffectTextureCandidates({
        "effect\\alpha_down.tga",
        "effect\\alpha_down.bmp",
        "effect\\alpha_dow.bmp",
    }, false);
    CTexture* sparkleTexture = ResolveEffectTextureCandidates({
        "effect\\torch_green10.bmp",
        "effect\\torch_green11.bmp",
        "effect\\torch_green12.bmp",
        "effect\\alpha_center.tga",
    }, false);
    CTexture* blessingBurstTexture = ResolveEffectTextureCandidates({
        "effect\\torch_green11.bmp",
        "effect\\torch_green12.bmp",
        "effect\\torch_green10.bmp",
        "effect\\alpha_center.tga",
    }, false);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            "effect\\EF_Blessing.wav",
            "effect\\ef_blessing.wav",
        });

        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_pattern |= 1;
            prim->m_renderFlag |= 0x200;
            prim->m_radius = 10.0f;
            prim->m_latitude = 90.0f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 100.0f;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.033333335f;
            prim->m_fadeOutCnt = prim->m_duration - 30;
            prim->m_texture.push_back(alphaDown);
            prim->m_tintColor = RGB(32, 176, 232);
        }
    }

    if ((m_stateCnt % 3) == 0 && m_stateCnt < 10) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_duration = m_duration;
            prim->m_animSpeed = 2;
            prim->m_alpha = static_cast<float>(200 - 6 * m_stateCnt);
            prim->m_param[2] = -25.0f;
            prim->m_tintColor = RGB(255, 255, 255);
            ConfigureEffectSpritePrim(prim, { RefBlessingSpriteStem() }, 0, 1.0f, false, 2.0f, 0);
        }
    }

    if ((m_stateCnt % 4) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_duration = 80;
            prim->m_pattern |= 0x80;
            const float angle = static_cast<float>(rand() % 360);
            const float radius = static_cast<float>(rand() % 7 + 2);
            prim->m_deltaPos2 = {
                std::sin(angle * (kPi / 180.0f)) * radius,
                -25.0f,
                -std::cos(angle * (kPi / 180.0f)) * radius
            };
            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix, -90.0f * (kPi / 180.0f));
            prim->m_speed = static_cast<float>(rand() % 20 + 20) * 0.01f;
            prim->m_accel = prim->m_speed / static_cast<float>(prim->m_duration) * -0.5f;
            prim->m_alpha = 0.0f;
            prim->m_size = 0.5f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.1f;
            prim->m_tintColor = RGB(255, 255, 255);
            if (!ConfigureEffectSpritePrim(prim, { RefParticle6SpriteStem() }, 0, 1.0f, false, 0.0f, 0)) {
                prim->m_texture.push_back(sparkleTexture ? sparkleTexture : blessingBurstTexture);
            }
        }
    }
}

void CRagEffect::SpawnSmoke()
{
    if (m_stateCnt != 0) {
        return;
    }

    const int smokeCount = rand() % 4 + 1;
    for (int smokeIndex = 0; smokeIndex < smokeCount; ++smokeIndex) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            prim->m_duration = m_duration;
            MatrixIdentity(prim->m_matrix);
            MatrixAppendXRotation(prim->m_matrix, static_cast<float>(rand() % 30 + 75) * (kPi / 180.0f));
            MatrixAppendYRotation(prim->m_matrix, static_cast<float>(rand() % 360) * (kPi / 180.0f));
            prim->m_deltaPos2.y = -9.0f;
            prim->m_rollSpeed = 0.75f;
            prim->m_speed = static_cast<float>(rand() % 3 + 3) * 0.1f;
            prim->m_alpha = 0.0f;
            prim->m_maxAlpha = 100.0f;
            prim->m_alphaSpeed = prim->m_maxAlpha * 0.033333335f;
            prim->m_size = 1.5f;
            prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3;
            prim->m_tintColor = RGB(255, 255, 255);

            if (!ConfigureEffectSpritePrim(prim, { RefChimneySmokeSpriteStem() }, 0, 1.0f, true, 0.0f, 0)) {
                prim->m_texture.push_back(ResolveEffectTextureCandidates({
                    "effect\\smoke.tga",
                    "effect\\smoke.bmp",
                    "effect\\smoke2.bmp",
                }, true));
            }
        }
    }
}

void CRagEffect::SpawnTorch()
{
    if (m_stateCnt != 0) {
        return;
    }

    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
        prim->m_pattern |= 0x200;
        prim->m_duration = m_duration;
        prim->m_deltaPos2.y = -10.0f;
        prim->m_alpha = 0.0f;
        prim->m_maxAlpha = 180.0f;
        prim->m_alphaSpeed = 12.0f;
        prim->m_size = m_param[0] > 0.0f ? m_param[0] : 1.0f;
        prim->m_animSpeed = (std::max)(1, static_cast<int>(m_param[1]));
        prim->m_tintColor = RGB(255, 255, 255);

        if (!ConfigureEffectSpritePrim(prim, { RefTorchSpriteStem() }, 0, 1.0f, true, 0.0f, 0)) {
            AppendTorchFallbackTextures(prim, m_effectName);
        }
    }
}

void CRagEffect::SpawnEnchantPoison()
{
    const vector3d base = ResolveBasePosition();
    if (m_stateCnt <= 2) {
        DbgLog("[EnchantPoisonDbg] effect20 state=%d base=(%.2f,%.2f,%.2f) master=%p\n",
            m_stateCnt,
            base.x,
            base.y,
            base.z,
            static_cast<void*>(m_master));
    }

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            "effect\\assasin_enchantpoison.wav",
        });
        TryPlayEffectWaveAt(base, {
            "effect\\EF_PoisonAttack.wav",
            "effect\\ef_poisonattack.wav",
        });
    }

    if ((m_stateCnt % 5) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            ConfigureEnchantPoisonParticle(prim, 30, 30, 40, 50);
            DbgLog("[EnchantPoisonDbg] spawned effect20 prim=%p delta2=(%.2f,%.2f,%.2f) speed=%.3f size=%.3f rollSpeed=%.2f\n",
                static_cast<void*>(prim),
                prim->m_deltaPos2.x,
                prim->m_deltaPos2.y,
                prim->m_deltaPos2.z,
                prim->m_speed,
                prim->m_size,
                prim->m_rollSpeed);
        }
    }
}

void CRagEffect::SpawnEnchantPoison2()
{
    const vector3d base = ResolveBasePosition();
    if (m_stateCnt <= 2) {
        DbgLog("[EnchantPoisonDbg] effect493 state=%d base=(%.2f,%.2f,%.2f) master=%p\n",
            m_stateCnt,
            base.x,
            base.y,
            base.z,
            static_cast<void*>(m_master));
    }

    if (m_stateCnt == 0 || m_stateCnt == 5 || m_stateCnt == 11 || m_stateCnt == 18 || m_stateCnt == 26 || m_stateCnt == 40) {
        TryPlayEffectWaveAt(base, {
            "effect\\assasin_enchantpoison.wav",
        });
    }

    if ((m_stateCnt % 3) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
            ConfigureEnchantPoisonParticle(prim, 30, 50, 20, 20);
            DbgLog("[EnchantPoisonDbg] spawned effect493 prim=%p delta2=(%.2f,%.2f,%.2f) speed=%.3f size=%.3f rollSpeed=%.2f\n",
                static_cast<void*>(prim),
                prim->m_deltaPos2.x,
                prim->m_deltaPos2.y,
                prim->m_deltaPos2.z,
                prim->m_speed,
                prim->m_size,
                prim->m_rollSpeed);
        }
    }

    if (CRenderObject* master = ResolveCurrentMaster(); master && (m_stateCnt % 2) == 0 && (master->m_BodyLight & 1) == 0) {
        ++master->m_BodyLight;
    }
}

void CRagEffect::SpawnSightAura()
{
    const vector3d base = ResolveBasePosition();
    CTexture* ringBlue = ResolveEffectTextureCandidates({
        "effect\\ring_blue.tga",
        "effect\\ring_blue.bmp",
        "effect\\ring_b.bmp",
    }, false);
    CTexture* fireGlow = ResolveEffectTextureCandidates({
        "effect\\magic_red.tga",
        "effect\\magic_red.bmp",
        "effect\\magic_violet.tga",
        "effect\\magic_violet.bmp",
    }, true);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(base, {
            "effect\\ef_ruwach.wav",
            "effect\\ef_sight.wav",
            "effect\\ef_incagi.wav",
        });
    }

    if ((m_stateCnt % 8) == 0) {
        if (CEffectPrim* prim = LaunchEffectPrim(PP_3DCIRCLE, vector3d{})) {
            prim->m_duration = 20;
            prim->m_deltaPos2.y = -1.0f;
            prim->m_innerSize = 7.0f;
            prim->m_latitude = 90.0f;
            prim->m_numSegments = 3;
            prim->m_pattern |= 1;
            prim->m_radius = 0.0f;
            prim->m_radiusSpeed = 0.85f;
            prim->m_radiusAccel = prim->m_radiusSpeed / static_cast<float>(prim->m_duration) * -0.5f;
            prim->m_alpha = 150.0f;
            prim->m_maxAlpha = 150.0f;
            prim->m_fadeOutCnt = prim->m_duration - 10;
            prim->m_texture.push_back(ringBlue);
            prim->m_tintColor = RGB(255, 210, 96);
        }
    }

    if ((m_stateCnt % 3) == 0) {
        const float angle = static_cast<float>(m_stateCnt) * 24.0f * (kPi / 180.0f);
        const float offsets[2] = { 0.0f, kPi };
        for (float phase : offsets) {
            if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
                const float orbit = angle + phase;
                prim->m_duration = 18;
                prim->m_deltaPos2.x = std::sin(orbit) * 15.0f;
                prim->m_deltaPos2.y = -10.0f + std::cos(orbit * 0.5f) * 2.0f;
                prim->m_deltaPos2.z = -std::cos(orbit) * 15.0f;
                prim->m_size = 2.6f;
                prim->m_sizeSpeed = -0.08f;
                prim->m_alpha = 220.0f;
                prim->m_alphaSpeed = -8.0f;
                prim->m_fadeOutCnt = 10;
                if (!ConfigureEffectSpritePrim(prim, { "fireball", "sight" }, 0, 1.0f, true, 2.0f, 0)) {
                    prim->m_texture.push_back(fireGlow);
                }
                prim->m_tintColor = RGB(255, 220, 120);
            }
        }
    }
}

void CRagEffect::SpawnSight()
{
    if ((m_stateCnt % 2) != 0) {
        return;
    }

    const float angle = static_cast<float>(-5 * m_stateCnt) * (kPi / 180.0f);
    const float offsetX = std::sin(angle) * 15.0f;
    const float offsetZ = -std::cos(angle) * 15.0f;
    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEGRAVITY, vector3d{})) {
        prim->m_duration = 20;
        prim->m_deltaPos2 = { offsetX, -20.0f, offsetZ };
        prim->m_gravAccel = 0.1f;
        prim->m_size = 2.5f;
        prim->m_sizeSpeed = -0.1f;
        prim->m_alpha = 150.0f;
        prim->m_alphaSpeed = -3.0f;
        if (!ConfigureEffectSpritePrim(prim, { "fireball", "sight" }, 0, 1.0f, true, 3.0f, 0)) {
            prim->m_texture.push_back(ResolveEffectTextureCandidates({
                "effect\\pikapika.bmp",
                "texture\\effect\\pikapika.bmp",
                "data\\texture\\effect\\pikapika.bmp",
            }, false));
        }
        prim->m_tintColor = RGB(255, 176, 96);
    }

    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
        prim->m_duration = 20;
        prim->m_deltaPos2 = { offsetX, 0.0f, offsetZ };
        prim->m_size = 1.5f;
        prim->m_sizeSpeed = -0.06f;
        prim->m_alpha = 120.0f;
        prim->m_alphaSpeed = -1.0f;
        ConfigureEffectSpritePrim(prim, { "Shadow", "shadow" }, 0, 1.0f, true, 2.0f, 0);
    }
}

void CRagEffect::SpawnSightState()
{
    CRenderObject* master = ResolveCurrentMaster();
    if (!master || !MasterHasEffectState(master, kSightEffectStateMask)) {
        m_loop = false;
        m_duration = m_stateCnt;
        return;
    }

    if (m_stateCnt >= 9998) {
        m_stateCnt = 0;
    }
    if ((m_stateCnt % 2) != 0) {
        return;
    }

    const float angle = static_cast<float>(-5 * m_stateCnt) * (kPi / 180.0f);
    const float offsetX = std::sin(angle) * 15.0f;
    const float offsetZ = -std::cos(angle) * 15.0f;
    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEGRAVITY, vector3d{})) {
        prim->m_duration = 20;
        prim->m_deltaPos2 = { offsetX, -20.0f, offsetZ };
        prim->m_gravAccel = 0.1f;
        prim->m_size = 2.5f;
        prim->m_sizeSpeed = -0.1f;
        prim->m_alpha = 50.0f;
        prim->m_alphaSpeed = -3.0f;
        if (!ConfigureEffectSpritePrim(prim, { "fireball", "sight" }, 0, 1.0f, true, 3.0f, 0)) {
            prim->m_texture.push_back(ResolveEffectTextureCandidates({
                "effect\\pikapika.bmp",
                "texture\\effect\\pikapika.bmp",
                "data\\texture\\effect\\pikapika.bmp",
            }, false));
        }
        prim->m_tintColor = RGB(255, 168, 88);
    }
}

void CRagEffect::SpawnFireBall()
{
    if (m_stateCnt != 20 && m_stateCnt != 24 && m_stateCnt != 28 && m_stateCnt != 32) {
        return;
    }

    const vector3d base = ResolveBasePosition();
    TryPlayEffectWaveAt(base, {
        "effect\\EF_FireBall.wav",
        "effect\\ef_fireball.wav",
    });

    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
        prim->m_duration = 20;
        prim->m_pattern |= 0x120;
        prim->m_deltaPos2 = { 0.0f, -15.0f, 0.0f };

        const float dx = m_deltaPos.x;
        const float dz = m_deltaPos.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        const float facingDegrees = std::atan2(dx, -dz) * (180.0f / kPi);
        prim->m_longitude = -facingDegrees;
        prim->m_speed = distance > 0.0f ? -(distance / static_cast<float>(prim->m_duration)) : 0.0f;
        prim->m_size = 1.3f;
        prim->m_animSpeed = 1;
        prim->m_alpha = 255.0f;

        if (m_stateCnt != 20) {
            prim->m_tintColor = RGB(246, 199, 76);
            if (m_stateCnt == 24) {
                prim->m_alpha = 180.0f;
            } else if (m_stateCnt == 28) {
                prim->m_alpha = 130.0f;
            } else {
                prim->m_alpha = 80.0f;
            }
        }

        ConfigureEffectSpritePrim(prim, { "fireball" }, 0, 1.0f, true, 0.0f, 0);
    }
}

void CRagEffect::SpawnRuwach()
{
    if ((m_stateCnt % 3) != 0) {
        return;
    }

    constexpr float kRuwachStepDegrees = -50.0f / 13.0f;
    const float angle = static_cast<float>(m_stateCnt) * kRuwachStepDegrees * (kPi / 180.0f);
    const float offsetX = std::sin(angle) * 15.0f;
    const float offsetZ = -std::cos(angle) * 15.0f;

    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
        prim->m_duration = 25;
        prim->m_deltaPos2 = { offsetX, -10.0f, offsetZ };
        prim->m_size = 3.0f;
        prim->m_sizeSpeed = -0.1f;
        prim->m_alpha = 250.0f;
        prim->m_alphaSpeed = -3.0f;
        prim->m_fadeOutCnt = prim->m_duration - 6;
        ConfigureEffectSpritePrim(prim, { RefParticle2SpriteStem() }, 0, 1.0f, false, 0.0f, 0);
    }

    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLE, vector3d{})) {
        prim->m_duration = 25;
        prim->m_deltaPos2 = { offsetX, 0.0f, offsetZ };
        prim->m_animSpeed = 2;
        prim->m_size = 1.5f;
        prim->m_sizeSpeed = -0.06f;
        prim->m_alpha = 150.0f;
        prim->m_alphaSpeed = -2.5f;
        prim->m_fadeOutCnt = prim->m_duration - 6;
        ConfigureEffectSpritePrim(prim, { "Shadow", "shadow" }, 0, 1.0f, false, 0.0f, 0);
    }
}

void CRagEffect::SpawnFireBoltRain()
{
    const vector3d target = m_hasTargetPos ? m_targetPos : ResolveBasePosition();
    CTexture* glow = ResolveEffectTextureCandidates({
        "effect\\magic_red.tga",
        "effect\\magic_red.bmp",
        "effect\\magic_violet.tga",
        "effect\\magic_violet.bmp",
    }, true);

    if (m_stateCnt == 0) {
        TryPlayEffectWaveAt(target, {
            "effect\\ef_firebolt.wav",
            "effect\\ef_firehit.wav",
            "effect\\ef_fireball.wav",
            "effect\\ef_firehit1.wav",
        });
    }

    const int hitCount = (std::max)(1, m_level > 0 ? m_level : 1);
    const int spawnInterval = (std::max)(1, 18 / hitCount);
    if ((m_stateCnt % spawnInterval) != 0 || (m_stateCnt / spawnInterval) >= hitCount) {
        return;
    }

    if (CEffectPrim* prim = LaunchEffectPrim(PP_3DPARTICLEGRAVITY, vector3d{})) {
        const float lateralX = static_cast<float>((rand() % 9) - 4);
        const float lateralZ = static_cast<float>((rand() % 9) - 4);
        prim->m_duration = 10;
        prim->m_deltaPos2.x = target.x - ResolveBasePosition().x + lateralX;
        prim->m_deltaPos2.y = 42.0f + static_cast<float>(rand() % 10);
        prim->m_deltaPos2.z = target.z - ResolveBasePosition().z + lateralZ;
        prim->m_latitude = 180.0f;
        prim->m_speed = 4.5f + static_cast<float>(rand() % 3);
        prim->m_gravSpeed = -1.4f;
        prim->m_gravAccel = -0.85f;
        prim->m_size = 5.0f;
        prim->m_sizeSpeed = -0.18f;
        prim->m_alpha = 245.0f;
        prim->m_alphaSpeed = -18.0f;
        prim->m_fadeOutCnt = 6;
        if (!ConfigureEffectSpritePrim(prim, { "fireball" }, 0, 1.0f, true, 1.0f, 0)) {
            prim->m_texture.push_back(glow);
        }
        prim->m_tintColor = RGB(255, 168, 80);
    }
}

u8 CRagEffect::OnProcess()
{
    if (CRenderObject* master = ResolveCurrentMaster()) {
        m_cachedPos = master->m_pos;
        m_longitude = master->m_roty;
    }

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
        || m_handler == Handler::MapPillar
        || m_handler == Handler::MapMagicZone
        || m_handler == Handler::MapParticle
        || m_handler == Handler::SuperAngel
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
        const bool dispatchHandler = m_loop || frameDrivenStrHandler || m_stateCnt < m_duration;
        if (dispatchHandler) {
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
            case Handler::MapPillar:
                SpawnMapPillar();
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
            case Handler::RecoveryHp:
                SpawnRecoveryHp();
                break;
            case Handler::RecoverySp:
                SpawnRecoverySp();
                break;
            case Handler::BeginCasting:
                SpawnBeginCasting();
                break;
            case Handler::HealLight:
                SpawnHealLight();
                break;
            case Handler::HealMedium:
                SpawnHealMedium();
                break;
            case Handler::HealLarge:
                SpawnHealLarge();
                break;
            case Handler::HideStart:
                SpawnHideStart();
                break;
            case Handler::GrimTooth:
                SpawnGrimTooth();
                break;
            case Handler::GrimToothAtk:
                SpawnGrimToothAtk();
                break;
            case Handler::IncAgility:
                SpawnIncAgility();
                break;
            case Handler::EnchantPoison:
                SpawnEnchantPoison();
                break;
            case Handler::EnchantPoison2:
                SpawnEnchantPoison2();
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
            case Handler::SightState:
                SpawnSightState();
                break;
            case Handler::FireBall:
                SpawnFireBall();
                break;
            case Handler::Ruwach:
                SpawnRuwach();
                break;
            case Handler::SightAura:
                SpawnSightAura();
                break;
            case Handler::FireBoltRain:
                SpawnFireBoltRain();
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

void CRagEffect::SendMsg(CGameObject*, int msg, msgparam_t par1, msgparam_t, msgparam_t)
{
    switch (msg) {
    case 14:
        if ((m_type == 24 || m_type == 123) && par1 != 0) {
            const vector3d* targetPos = reinterpret_cast<const vector3d*>(par1);
            m_targetPos = *targetPos;
            m_hasTargetPos = true;
            if (m_type == 24 && m_handler != Handler::FireBoltRain) {
                ClearPrims();
                m_handler = Handler::FireBoltRain;
                m_duration = 18;
                m_stateCnt = 0;
            } else if (m_type == 123) {
                const vector3d base = ResolveBasePosition();
                const float dx = m_targetPos.x - base.x;
                const float dy = m_targetPos.y - base.y;
                const float dz = m_targetPos.z - base.z;
                const float distance = std::sqrt(dx * dx + dz * dz);
                ClearPrims();
                m_handler = Handler::GrimTooth;
                CRenderObject* ownerMaster = nullptr;
                if (CAbleToMakeEffect* owner = ResolveCurrentEffectOwner()) {
                    ownerMaster = dynamic_cast<CRenderObject*>(owner);
                    owner->m_effectList.remove(this);
                    if (owner->m_beginSpellEffect == this) {
                        owner->m_beginSpellEffect = nullptr;
                    }
                    if (owner->m_magicTargetEffect == this) {
                        owner->m_magicTargetEffect = nullptr;
                    }
                }
                DetachFromMaster(ownerMaster);
                m_cachedPos = base;
                m_deltaPos = vector3d{};
                if (distance > 0.0001f) {
                    const float invDistance = 2.0f / distance;
                    m_grimToothStep = { dx * invDistance, 0.0f, dz * invDistance };
                    m_grimToothRemaining = vector3d{};
                    m_flag = 0;
                } else {
                    m_grimToothStep = vector3d{};
                    m_grimToothRemaining = vector3d{};
                    m_flag = 1;
                }
                DbgLog("[Projectile] grimtooth effect123 init base=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) step=(%.2f,%.2f,%.2f)\n",
                    base.x,
                    base.y,
                    base.z,
                    m_targetPos.x,
                    m_targetPos.y,
                    m_targetPos.z,
                    m_grimToothStep.x,
                    m_grimToothStep.y,
                    m_grimToothStep.z);
                m_duration = (std::max)(6, static_cast<int>(std::ceil(distance / 2.0f)) + 2);
                m_stateCnt = 0;
            }
        }
        return;
    case 44:
        m_level = static_cast<int>(par1);
        return;
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
    case 109:
        ClearPrims();
        m_loop = false;
        m_duration = 0;
        m_stateCnt = 1;
        return;
    default:
        return;
    }
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
