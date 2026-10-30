#include "MsgEffect.h"

#include "World.h"
#include "audio/Audio.h"
#include "DebugLog.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "res/ActRes.h"
#include "res/Sprite.h"
#include "res/Res.h"
#include "session/Session.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr float kMsgEffectNearPlane = 10.0f;
constexpr float kMsgEffectSubmitNearPlane = 80.0f;
constexpr const char* kDamageNumberSpritePath = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xFD\xC0\xDA.spr";
constexpr const char* kDamageNumberActPath = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\\xBC\xFD\xC0\xDA.act";
constexpr const char* kMissEffectSpritePath = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\msg.spr";
constexpr const char* kMissEffectActPath = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\msg.act";
constexpr int kAttachedActorSpriteMsgEffectType = 203;
constexpr int kMissMsgEffectType = 13;
constexpr int kCriticalMsgEffectType = 15;
constexpr int kLuckyDodgeMsgEffectType = 17;
constexpr float kAttachedActorSpriteDepthBias = 0.00015f;
constexpr float kAttachedActorSpriteWorldWidthCells = 2.0f;
constexpr float kAttachedActorDepthWorldBiasScale = 0.35f;
constexpr float kAttachedActorDepthWorldBiasMin = 0.75f;
constexpr float kAttachedActorDepthWorldBiasMax = 3.0f;
constexpr float kAttachedActorDepthHeightBiasFactor = 0.45f;
constexpr float kAttachedActorTopDepthForwardBiasScale = 0.85f;
constexpr float kFallbackWorldTileSize = 5.0f;
constexpr float kAttachedActorRenderLeashScale = 1.65f;
constexpr float kAttachedActorRenderLagMs = 110.0f;
constexpr float kAttachedActorRenderLagMinMs = 70.0f;
constexpr float kAttachedActorRenderLagMaxMs = 220.0f;
constexpr float kAttachedActorTrailSampleMs = 45.0f;

bool ShouldTraceAttachedCartEffect(const CMsgEffect& effect)
{
    return effect.m_msgEffectType == kAttachedActorSpriteMsgEffectType
        && effect.m_masterActor != nullptr
        && effect.m_masterActor->m_gid != 0
        && effect.m_masterActor->m_gid != g_session.m_gid;
}

struct QueuedMsgEffectDraw {
    int screenX = 0;
    int screenY = 0;
    int digit = 0;
    u32 colorArgb = 0xFFFFFFFFu;
    int alpha = 255;
    float zoom = 1.0f;
    int sprShift = 0;
};

struct QueuedMsgSpriteEffectDraw {
    int screenX = 0;
    int screenY = 0;
    int actionIndex = 0;
    int motionIndex = 0;
    std::string spritePath;
    std::string actPath;
    u32 colorArgb = 0xFFFFFFFFu;
    int alpha = 255;
    float zoom = 1.0f;
    float alphaSortBase = 1.7f;
    float projectedZ = 0.0f;
    float projectedOow = 1.0f;
    float pixelsPerTile = 0.0f;
    float desiredWorldWidthCells = 0.0f;
    bool useProjectedDepth = false;
    bool usePerspectiveScale = false;
    bool bypassZoomClamp = false;
};

std::vector<QueuedMsgEffectDraw> g_queuedMsgEffects;
std::vector<QueuedMsgSpriteEffectDraw> g_queuedMsgSpriteEffects;

CSprRes* GetMissEffectSprite();
CActRes* GetMissEffectAct();

bool ResolveMsgSpriteResources(const std::string& spritePath,
    const std::string& actPath,
    CSprRes** outSprite,
    CActRes** outAct)
{
    if (!outSprite || !outAct) {
        return false;
    }

    if (!spritePath.empty() && !actPath.empty()) {
        *outSprite = g_resMgr.GetAs<CSprRes>(spritePath.c_str());
        *outAct = g_resMgr.GetAs<CActRes>(actPath.c_str());
    } else {
        *outSprite = GetMissEffectSprite();
        *outAct = GetMissEffectAct();
    }

    return *outSprite != nullptr && *outAct != nullptr;
}

bool ContainsAsciiCaseSensitive(const char* text, const char* token)
{
    if (!text || !token || !*text || !*token) {
        return false;
    }

    return std::string(text).find(token) != std::string::npos;
}

std::string NormalizeWaveEventPath(const char* eventName)
{
    std::string normalized = eventName ? eventName : "";
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

bool IsSpriteBackedMsgEffectType(int msgEffectType)
{
    return msgEffectType == kMissMsgEffectType
        || msgEffectType == kCriticalMsgEffectType
        || msgEffectType == kLuckyDodgeMsgEffectType;
}

bool IsAnimatedSpriteBackedMsgEffectType(int msgEffectType)
{
    return msgEffectType == kCriticalMsgEffectType
        || msgEffectType == kLuckyDodgeMsgEffectType;
}

bool TryPlayMsgSpriteMotionEvent(const CMsgEffect& effect, int motionIndex)
{
    CActRes* act = GetMissEffectAct();
    if (!act) {
        return false;
    }
    const CMotion* motion = act->GetMotion(effect.m_spriteActionIndex, motionIndex);
    if (!motion || motion->eventId < 0) {
        return false;
    }
    const char* eventName = act->GetEventName(motion->eventId);
    if (!ContainsAsciiCaseSensitive(eventName, ".wav")) {
        return false;
    }
    CGameMode* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || !mode->m_world || !mode->m_world->m_player) {
        return false;
    }
    const vector3d& origin = effect.m_pos;
    const vector3d& listener = mode->m_world->m_player->m_pos;
    return PlayWave(eventName, origin.x - listener.x, 0.0f, origin.z - listener.z);
}

bool ProjectMsgEffectPoint(const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= kMsgEffectSubmitNearPlane) {
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
    outVertex->z = (1500.0f / (1500.0f - kMsgEffectNearPlane)) * ((1.0f / oow) - kMsgEffectNearPlane) * oow;
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return std::isfinite(outVertex->x) && std::isfinite(outVertex->y) && std::isfinite(outVertex->z);
}

CSprRes* GetDamageCountSprite()
{
    static CSprRes* sprite = nullptr;
    static bool attemptedLoad = false;
    if (!attemptedLoad) {
        attemptedLoad = true;
        sprite = g_resMgr.GetAs<CSprRes>(kDamageNumberSpritePath);
        if (!sprite) {
            DbgLog("[MsgEffect] Failed to load damage number sprite '%s'\n", kDamageNumberSpritePath);
        }
    }
    return sprite;
}

CActRes* GetDamageCountAct()
{
    static CActRes* act = nullptr;
    static bool attemptedLoad = false;
    if (!attemptedLoad) {
        attemptedLoad = true;
        act = g_resMgr.GetAs<CActRes>(kDamageNumberActPath);
        if (!act) {
            DbgLog("[MsgEffect] Failed to load damage number act '%s'\n", kDamageNumberActPath);
        }
    }
    return act;
}

CSprRes* GetMissEffectSprite()
{
    static CSprRes* sprite = nullptr;
    static bool attemptedLoad = false;
    if (!attemptedLoad) {
        attemptedLoad = true;
        sprite = g_resMgr.GetAs<CSprRes>(kMissEffectSpritePath);
        if (!sprite) {
            DbgLog("[MsgEffect] Failed to load miss effect sprite '%s'\n", kMissEffectSpritePath);
        }
    }
    return sprite;
}

CActRes* GetMissEffectAct()
{
    static CActRes* act = nullptr;
    static bool attemptedLoad = false;
    if (!attemptedLoad) {
        attemptedLoad = true;
        act = g_resMgr.GetAs<CActRes>(kMissEffectActPath);
        if (!act) {
            DbgLog("[MsgEffect] Failed to load miss effect act '%s'\n", kMissEffectActPath);
        }
    }
    return act;
}

float ResolveDigitScale(float zoom)
{
    const float scale = 1.8f + zoom * 0.6f;
    return (std::max)(1.8f, (std::min)(4.8f, scale));
}

float ResolveMsgSpriteScale(float zoom)
{
    return (std::max)(0.75f, (std::min)(2.0f, zoom));
}

float ResolveWorldTileSize(const CWorld* world)
{
    if (!world) {
        return kFallbackWorldTileSize;
    }

    if (world->m_attr) {
        return static_cast<float>(world->m_attr->m_zoom);
    }
    if (world->m_ground) {
        return static_cast<float>(world->m_ground->m_zoom);
    }
    return kFallbackWorldTileSize;
}

float ResolveWorldGroundHeight(const CWorld* world, float x, float z)
{
    return (world && world->m_attr) ? world->m_attr->GetHeight(x, z) : 0.0f;
}

float TileToWorldCoordXForMsgEffect(const CWorld* world, int tileX)
{
    const int width = world && world->m_attr ? world->m_attr->m_width : (world && world->m_ground ? world->m_ground->m_width : 0);
    const float zoom = ResolveWorldTileSize(world);
    return (static_cast<float>(tileX) - static_cast<float>(width) * 0.5f) * zoom + zoom * 0.5f;
}

float TileToWorldCoordZForMsgEffect(const CWorld* world, int tileY)
{
    const int height = world && world->m_attr ? world->m_attr->m_height : (world && world->m_ground ? world->m_ground->m_height : 0);
    const float zoom = ResolveWorldTileSize(world);
    return (static_cast<float>(tileY) - static_cast<float>(height) * 0.5f) * zoom + zoom * 0.5f;
}

bool FindActivePathSegmentForMsgEffect(const CPathInfo& path, u32 now, size_t* outStartIndex)
{
    if (path.m_cells.size() < 2 || !outStartIndex) {
        return false;
    }

    for (size_t index = 1; index < path.m_cells.size(); ++index) {
        if (now < path.m_cells[index].arrivalTime) {
            *outStartIndex = index - 1;
            return true;
        }
    }

    *outStartIndex = path.m_cells.size() - 2;
    return true;
}

bool ResolveActorInterpolatedPositionForMsgEffect(const CGameActor& actor, u32 now, vector3d* outPos);
float ResolveActorFacingRotationForMsgEffect(const CGameActor& actor, u32 now);

bool ResolveAttachedActorTrailPosition(const CGameActor& actor, u32 now, vector3d* outPos)
{
    if (!outPos) {
        return false;
    }

    const CWorld* world = &g_world;
    const float followDistance = ResolveWorldTileSize(world);
    if (followDistance <= 0.0f) {
        return false;
    }

    float dirX = 0.0f;
    float dirZ = 0.0f;
    bool hasDirection = false;
    size_t startIndex = 0;
    if (actor.m_path.m_cells.size() >= 2
        && FindActivePathSegmentForMsgEffect(actor.m_path, now, &startIndex)) {
        const PathCell& startCell = actor.m_path.m_cells[startIndex];
        const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
        const float startX = startIndex == 0
            ? actor.m_moveStartPos.x
            : TileToWorldCoordXForMsgEffect(world, startCell.x);
        const float startZ = startIndex == 0
            ? actor.m_moveStartPos.z
            : TileToWorldCoordZForMsgEffect(world, startCell.y);
        const float endX = TileToWorldCoordXForMsgEffect(world, endCell.x);
        const float endZ = TileToWorldCoordZForMsgEffect(world, endCell.y);
        const float deltaX = endX - startX;
        const float deltaZ = endZ - startZ;
        const float length = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        if (length > 0.001f) {
            dirX = deltaX / length;
            dirZ = deltaZ / length;
            hasDirection = true;
        }
    }

    *outPos = actor.m_pos;
    if (hasDirection) {
        outPos->x -= dirX * followDistance;
        outPos->z -= dirZ * followDistance;
    } else {
        const float radians = actor.m_roty * (3.14159265f / 180.0f);
        outPos->x -= std::sin(radians) * followDistance;
        outPos->z += std::cos(radians) * followDistance;
    }
    outPos->y = ResolveWorldGroundHeight(world, outPos->x, outPos->z);
    return true;
}

void ResolveAttachedActorIdlePosition(const CGameActor& actor, u32 now, vector3d* outPos)
{
    if (!outPos) {
        return;
    }

    (void)now;
    *outPos = actor.m_pos;
    const float radians = actor.m_roty * (3.14159265f / 180.0f);
    const float followDistance = ResolveWorldTileSize(&g_world);
    outPos->x -= std::sin(radians) * followDistance;
    outPos->z += std::cos(radians) * followDistance;
    outPos->y = ResolveWorldGroundHeight(&g_world, outPos->x, outPos->z);
}

const vector3d& ResolveAttachedActorDisplayPosition(const CMsgEffect& effect)
{
    return effect.m_followRenderInitialized ? effect.m_followRenderPos : effect.m_pos;
}

float ResolveProjectedTilePixels(const matrix& viewMatrix, const vector3d& origin)
{
    const float tileSize = ResolveWorldTileSize(&g_world);
    if (tileSize <= 0.0f) {
        return 0.0f;
    }

    tlvertex3d base{};
    if (!ProjectMsgEffectPoint(viewMatrix, origin, &base)) {
        return 0.0f;
    }

    float maxDistance = 0.0f;
    const std::array<vector3d, 2> offsets = {
        vector3d{ origin.x + tileSize, origin.y, origin.z },
        vector3d{ origin.x, origin.y, origin.z + tileSize },
    };
    for (const vector3d& offset : offsets) {
        tlvertex3d projected{};
        if (!ProjectMsgEffectPoint(viewMatrix, offset, &projected)) {
            continue;
        }

        const float dx = projected.x - base.x;
        const float dy = projected.y - base.y;
        maxDistance = (std::max)(maxDistance, std::hypot(dx, dy));
    }

    return maxDistance;
}

float ResolveQueuedMsgSpriteScale(const QueuedMsgSpriteEffectDraw& draw, int referenceWidth)
{
    if (draw.usePerspectiveScale && draw.pixelsPerTile > 0.0f && referenceWidth > 0) {
        const float desiredWidthPixels = draw.pixelsPerTile * (std::max)(0.5f, draw.desiredWorldWidthCells);
        return (std::max)(0.2f, desiredWidthPixels / static_cast<float>(referenceWidth));
    }

    if (draw.bypassZoomClamp) {
        return (std::max)(0.2f, draw.zoom);
    }

    return ResolveMsgSpriteScale(draw.zoom);
}

int ResolveDigitShiftPixels(const QueuedMsgEffectDraw& draw)
{
    return draw.sprShift;
}

int CountDamageDigits(int value)
{
    if (value >= 100000) {
        return 6;
    }
    if (value >= 10000) {
        return 5;
    }
    if (value >= 1000) {
        return 4;
    }
    if (value >= 100) {
        return 3;
    }
    if (value >= 10) {
        return 2;
    }
    return 1;
}

int ResolveDigitAtIndex(int value, int index)
{
    for (int i = 0; i < index; ++i) {
        value /= 10;
    }
    return value % 10;
}

bool ResolveDigitClipBox(int digit, RECT* outClipBox)
{
    if (!outClipBox) {
        return false;
    }

    CSprRes* sprite = GetDamageCountSprite();
    CActRes* act = GetDamageCountAct();
    if (!sprite || !act) {
        return false;
    }

    const int digitIndex = (std::max)(0, (std::min)(9, digit));
    const CMotion* motion = act->GetMotion(0, digitIndex);
    if (!motion) {
        motion = act->GetMotion(0, 0);
    }
    if (!motion) {
        return false;
    }

    RECT clipBox{};
    bool hasClip = false;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const int drawX = clip.x;
        const int drawY = clip.y;
        RECT current = { drawX, drawY, drawX + image->width, drawY + image->height };
        if (!hasClip) {
            clipBox = current;
            hasClip = true;
        } else {
            clipBox.left = (std::min)(clipBox.left, current.left);
            clipBox.top = (std::min)(clipBox.top, current.top);
            clipBox.right = (std::max)(clipBox.right, current.right);
            clipBox.bottom = (std::max)(clipBox.bottom, current.bottom);
        }
    }

    if (!hasClip) {
        return false;
    }

    *outClipBox = clipBox;
    return true;
}

int ResolveDigitOutputWidth(int digit, float zoom)
{
    RECT clipBox{};
    if (!ResolveDigitClipBox(digit, &clipBox)) {
        return (std::max)(1, static_cast<int>(std::lround(8.0f * ResolveDigitScale(zoom))));
    }

    const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    return (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * ResolveDigitScale(zoom))));
}

unsigned int PremultiplyArgb(unsigned int color)
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

unsigned int ModulateDigitColor(unsigned int srcRgb, unsigned int alpha, unsigned int tintRed, unsigned int tintGreen, unsigned int tintBlue)
{
    const unsigned int srcRed = (srcRgb >> 16) & 0xFFu;
    const unsigned int srcGreen = (srcRgb >> 8) & 0xFFu;
    const unsigned int srcBlue = srcRgb & 0xFFu;
    const unsigned int maxChannel = (std::max)(srcRed, (std::max)(srcGreen, srcBlue));

    if (maxChannel <= 24u) {
        return PremultiplyArgb(alpha << 24);
    }

    // Treat the digit sprite as a brightness mask so the requested damage tint
    // stays stable through the fade instead of inheriting the source art hue.
    const unsigned int modulatedRed = maxChannel * tintRed / 255u;
    const unsigned int modulatedGreen = maxChannel * tintGreen / 255u;
    const unsigned int modulatedBlue = maxChannel * tintBlue / 255u;
    return PremultiplyArgb((alpha << 24) | (modulatedRed << 16) | (modulatedGreen << 8) | modulatedBlue);
}

unsigned int BuildDigitMaskPixel(unsigned int rgb, unsigned int alpha)
{
    const unsigned int srcRed = (rgb >> 16) & 0xFFu;
    const unsigned int srcGreen = (rgb >> 8) & 0xFFu;
    const unsigned int srcBlue = rgb & 0xFFu;
    const unsigned int maxChannel = (std::max)(srcRed, (std::max)(srcGreen, srcBlue));
    const unsigned int luminance = maxChannel <= 24u ? 0u : maxChannel;
    return (alpha << 24)
        | (luminance << 16)
        | (luminance << 8)
        | luminance;
}

CTexture* GetOrCreateDigitClipTexture(CSprRes* sprite, const SprImg* image)
{
    if (!sprite || !image || image->width <= 0 || image->height <= 0) {
        return nullptr;
    }

    SprImg* mutableImage = const_cast<SprImg*>(image);
    if (mutableImage->tex && mutableImage->tex != &CTexMgr::s_dummy_texture) {
        return mutableImage->tex;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(image->width) * static_cast<size_t>(image->height), 0u);
    if (!image->rgba.empty()) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                const unsigned int rgba = image->rgba[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)];
                const unsigned int alpha = (rgba >> 24) & 0xFFu;
                if (alpha == 0u) {
                    continue;
                }
                pixels[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)] =
                    BuildDigitMaskPixel(rgba & 0x00FFFFFFu, alpha);
            }
        }
    } else if (!image->indices.empty()) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                const unsigned char paletteIndex = image->indices[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)];
                if (paletteIndex == 0) {
                    continue;
                }
                const unsigned int paletteColor = sprite->m_pal[paletteIndex] & 0x00FFFFFFu;
                pixels[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)] =
                    BuildDigitMaskPixel(paletteColor, 0xFFu);
            }
        }
    } else {
        return nullptr;
    }

    CTexture* texture = new CTexture();
    if (!texture || !texture->Create(static_cast<unsigned int>(image->width), static_cast<unsigned int>(image->height), PF_A8R8G8B8)) {
        delete texture;
        return nullptr;
    }
    texture->Update(0,
        0,
        image->width,
        image->height,
        pixels.data(),
        true,
        image->width * static_cast<int>(sizeof(unsigned int)));
    mutableImage->tex = texture;
    return texture;
}

CTexture* GetOrCreateSpriteClipTexture(CSprRes* sprite, const SprImg* image)
{
    if (!sprite || !image || image->width <= 0 || image->height <= 0) {
        return nullptr;
    }

    SprImg* mutableImage = const_cast<SprImg*>(image);
    if (mutableImage->tex && mutableImage->tex != &CTexMgr::s_dummy_texture) {
        return mutableImage->tex;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(image->width) * static_cast<size_t>(image->height), 0u);
    if (!image->rgba.empty()) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                const unsigned int rgba = image->rgba[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)];
                const unsigned int alpha = (rgba >> 24) & 0xFFu;
                if (alpha == 0u) {
                    continue;
                }
                pixels[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)] =
                    (alpha << 24) | (rgba & 0x00FFFFFFu);
            }
        }
    } else if (!image->indices.empty()) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                const unsigned char paletteIndex = image->indices[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)];
                if (paletteIndex == 0) {
                    continue;
                }
                pixels[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)] =
                    0xFF000000u | (sprite->m_pal[paletteIndex] & 0x00FFFFFFu);
            }
        }
    } else {
        return nullptr;
    }

    CTexture* texture = new CTexture();
    if (!texture || !texture->Create(static_cast<unsigned int>(image->width), static_cast<unsigned int>(image->height), PF_A8R8G8B8)) {
        delete texture;
        return nullptr;
    }
    texture->Update(0,
        0,
        image->width,
        image->height,
        pixels.data(),
        true,
        image->width * static_cast<int>(sizeof(unsigned int)));
    mutableImage->tex = texture;
    return texture;
}

u32 BuildDigitClipColor(const QueuedMsgEffectDraw& draw, const CSprClip& clip)
{
    const unsigned int tintAlpha = (draw.colorArgb >> 24) & 0xFFu;
    const unsigned int totalAlpha = static_cast<unsigned int>((std::max)(0, (std::min)(255, draw.alpha))) * tintAlpha / 255u;
    const unsigned int tintRed = (draw.colorArgb >> 16) & 0xFFu;
    const unsigned int tintGreen = (draw.colorArgb >> 8) & 0xFFu;
    const unsigned int tintBlue = draw.colorArgb & 0xFFu;
    const unsigned int clipAlpha = static_cast<unsigned int>(clip.a) * totalAlpha / 255u;
    const unsigned int clipRed = tintRed * clip.r / 255u;
    const unsigned int clipGreen = tintGreen * clip.g / 255u;
    const unsigned int clipBlue = tintBlue * clip.b / 255u;
    return (clipAlpha << 24)
        | (clipRed << 16)
        | (clipGreen << 8)
        | clipBlue;
}

bool QueueDigitSpriteQuad(const QueuedMsgEffectDraw& draw)
{
    CSprRes* sprite = GetDamageCountSprite();
    CActRes* act = GetDamageCountAct();
    if (!sprite || !act) {
        return false;
    }

    const int digitIndex = (std::max)(0, (std::min)(9, draw.digit));
    const CMotion* motion = act->GetMotion(0, digitIndex);
    if (!motion) {
        motion = act->GetMotion(0, 0);
    }
    if (!motion) {
        return false;
    }

    RECT clipBox{};
    if (!ResolveDigitClipBox(draw.digit, &clipBox)) {
        return false;
    }

    const float scale = ResolveDigitScale(draw.zoom);
    const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    const int nativeHeight = (std::max)(1, static_cast<int>(clipBox.bottom - clipBox.top));
    const int outWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * scale)));
    const int outHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeHeight) * scale)));
    const float originX = OverlayScreenEdge(draw.screenX + ResolveDigitShiftPixels(draw) - outWidth / 2);
    const float originY = OverlayScreenEdge(draw.screenY - outHeight / 2);

    bool queuedAny = false;
    float clipSortOffset = 0.0f;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        CTexture* texture = GetOrCreateDigitClipTexture(sprite, image);
        if (!texture || texture == &CTexMgr::s_dummy_texture) {
            continue;
        }

        const float drawLeft = originX + static_cast<float>(clip.x - clipBox.left) * scale;
        const float drawTop = originY + static_cast<float>(clip.y - clipBox.top) * scale;
        const float drawRight = drawLeft + static_cast<float>(image->width) * scale;
        const float drawBottom = drawTop + static_cast<float>(image->height) * scale;

        const float maxU = texture->m_w != 0
            ? static_cast<float>(texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(image->width))
                / static_cast<float>(texture->m_w)
            : 1.0f;
        const float maxV = texture->m_h != 0
            ? static_cast<float>(texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(image->height))
                / static_cast<float>(texture->m_h)
            : 1.0f;

        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            continue;
        }

        const bool flipX = (clip.flags & 1) != 0;
        const u32 color = BuildDigitClipColor(draw, clip);
        if (((color >> 24) & 0xFFu) == 0u) {
            continue;
        }

        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts;
        face->numVerts = 4;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = kSpriteTextureMode;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        face->alphaSortKey = 1.7f + clipSortOffset;

        face->m_verts[0] = { drawLeft,  drawTop,    0.0f, 1.0f, color, 0xFF000000u, flipX ? maxU : 0.0f, 0.0f };
        face->m_verts[1] = { drawRight, drawTop,    0.0f, 1.0f, color, 0xFF000000u, flipX ? 0.0f : maxU, 0.0f };
        face->m_verts[2] = { drawLeft,  drawBottom, 0.0f, 1.0f, color, 0xFF000000u, flipX ? maxU : 0.0f, maxV };
        face->m_verts[3] = { drawRight, drawBottom, 0.0f, 1.0f, color, 0xFF000000u, flipX ? 0.0f : maxU, maxV };
        g_renderer.AddRP(face, 1 | 8);
        clipSortOffset += 0.0001f;
        queuedAny = true;
    }

    return queuedAny;
}

bool GetDigitDrawBounds(const QueuedMsgEffectDraw& draw, RECT* outRect)
{
    if (!outRect) {
        return false;
    }

    RECT clipBox{};
    if (ResolveDigitClipBox(draw.digit, &clipBox)) {
        const float scale = ResolveDigitScale(draw.zoom);
        const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
        const int nativeHeight = (std::max)(1, static_cast<int>(clipBox.bottom - clipBox.top));
        const int outWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * scale)));
        const int outHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeHeight) * scale)));
        const int originX = draw.screenX + ResolveDigitShiftPixels(draw) - outWidth / 2;
        const int originY = draw.screenY - outHeight / 2;
        outRect->left = originX - 1;
        outRect->top = originY - 1;
        outRect->right = originX + outWidth + 1;
        outRect->bottom = originY + outHeight + 1;
        return true;
    }

    const int fontHeight = (std::max)(14, (std::min)(32, static_cast<int>(std::lround(8.0f + draw.zoom * 4.0f))));
    const int drawX = draw.screenX + ResolveDigitShiftPixels(draw) - fontHeight / 4;
    const int drawY = draw.screenY - fontHeight / 2;
    outRect->left = drawX - 2;
    outRect->top = drawY - 2;
    outRect->right = drawX + fontHeight + 2;
    outRect->bottom = drawY + fontHeight + 2;
    return true;
}

bool ResolveMsgSpriteClipBox(CSprRes* sprite, CActRes* act, int actionIndex, int motionIndex, RECT* outClipBox)
{
    if (!sprite || !act || !outClipBox) {
        return false;
    }

    const CMotion* motion = act->GetMotion(actionIndex, motionIndex);
    if (!motion) {
        motion = act->GetMotion(actionIndex, 0);
    }
    if (!motion) {
        return false;
    }

    RECT clipBox{};
    bool hasClip = false;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        const int drawX = clip.x;
        const int drawY = clip.y;
        RECT current = { drawX, drawY, drawX + image->width, drawY + image->height };
        if (!hasClip) {
            clipBox = current;
            hasClip = true;
        } else {
            clipBox.left = (std::min)(clipBox.left, current.left);
            clipBox.top = (std::min)(clipBox.top, current.top);
            clipBox.right = (std::max)(clipBox.right, current.right);
            clipBox.bottom = (std::max)(clipBox.bottom, current.bottom);
        }
    }

    if (!hasClip) {
        return false;
    }

    *outClipBox = clipBox;
    return true;
}

bool ResolveMsgSpriteReferenceClipBox(CSprRes* sprite, CActRes* act, RECT* outClipBox)
{
    if (!sprite || !act || !outClipBox) {
        return false;
    }

    RECT referenceClipBox{};
    bool hasReference = false;
    for (int actionIndex = 0; actionIndex < 8; ++actionIndex) {
        RECT currentClipBox{};
        if (!ResolveMsgSpriteClipBox(sprite, act, actionIndex, 0, &currentClipBox)) {
            continue;
        }

        if (!hasReference) {
            referenceClipBox = currentClipBox;
            hasReference = true;
        } else {
            referenceClipBox.left = (std::min)(referenceClipBox.left, currentClipBox.left);
            referenceClipBox.top = (std::min)(referenceClipBox.top, currentClipBox.top);
            referenceClipBox.right = (std::max)(referenceClipBox.right, currentClipBox.right);
            referenceClipBox.bottom = (std::max)(referenceClipBox.bottom, currentClipBox.bottom);
        }
    }

    if (!hasReference) {
        return false;
    }

    *outClipBox = referenceClipBox;
    return true;
}

u32 BuildMsgSpriteClipColor(const QueuedMsgSpriteEffectDraw& draw, const CSprClip& clip)
{
    const unsigned int tintAlpha = (draw.colorArgb >> 24) & 0xFFu;
    const unsigned int totalAlpha = static_cast<unsigned int>((std::max)(0, (std::min)(255, draw.alpha))) * tintAlpha / 255u;
    const unsigned int tintRed = (draw.colorArgb >> 16) & 0xFFu;
    const unsigned int tintGreen = (draw.colorArgb >> 8) & 0xFFu;
    const unsigned int tintBlue = draw.colorArgb & 0xFFu;
    const unsigned int clipAlpha = static_cast<unsigned int>(clip.a) * totalAlpha / 255u;
    const unsigned int clipRed = tintRed * clip.r / 255u;
    const unsigned int clipGreen = tintGreen * clip.g / 255u;
    const unsigned int clipBlue = tintBlue * clip.b / 255u;
    return (clipAlpha << 24)
        | (clipRed << 16)
        | (clipGreen << 8)
        | clipBlue;
}

bool QueueMsgSpriteQuad(const QueuedMsgSpriteEffectDraw& draw)
{
    CSprRes* sprite = nullptr;
    CActRes* act = nullptr;
    if (!ResolveMsgSpriteResources(draw.spritePath, draw.actPath, &sprite, &act)) {
        return false;
    }

    const CMotion* motion = act->GetMotion(draw.actionIndex, draw.motionIndex);
    if (!motion) {
        motion = act->GetMotion(draw.actionIndex, 0);
    }
    if (!motion) {
        return false;
    }

    RECT clipBox{};
    if (!ResolveMsgSpriteClipBox(sprite, act, draw.actionIndex, draw.motionIndex, &clipBox)) {
        return false;
    }

    RECT referenceClipBox = clipBox;
    if (draw.usePerspectiveScale) {
        ResolveMsgSpriteReferenceClipBox(sprite, act, &referenceClipBox);
    }

    const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    const int nativeHeight = (std::max)(1, static_cast<int>(clipBox.bottom - clipBox.top));
    const int referenceWidth = (std::max)(1, static_cast<int>(referenceClipBox.right - referenceClipBox.left));
    const float scale = ResolveQueuedMsgSpriteScale(draw, referenceWidth);
    const int outWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * scale)));
    const int outHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeHeight) * scale)));
    const float originX = OverlayScreenEdge(draw.screenX - outWidth / 2);
    const float originY = OverlayScreenEdge(draw.screenY - outHeight / 2);

    bool queuedAny = false;
    float clipSortOffset = 0.0f;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        CTexture* texture = GetOrCreateSpriteClipTexture(sprite, image);
        if (!texture || texture == &CTexMgr::s_dummy_texture) {
            continue;
        }

        const float drawLeft = originX + static_cast<float>(clip.x - clipBox.left) * scale;
        const float drawTop = originY + static_cast<float>(clip.y - clipBox.top) * scale;
        const float drawRight = drawLeft + static_cast<float>(image->width) * scale;
        const float drawBottom = drawTop + static_cast<float>(image->height) * scale;

        const float maxU = texture->m_w != 0
            ? static_cast<float>(texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(image->width))
                / static_cast<float>(texture->m_w)
            : 1.0f;
        const float maxV = texture->m_h != 0
            ? static_cast<float>(texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(image->height))
                / static_cast<float>(texture->m_h)
            : 1.0f;

        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            continue;
        }

        const bool flipX = (clip.flags & 1) != 0;
        const u32 color = BuildMsgSpriteClipColor(draw, clip);
        if (((color >> 24) & 0xFFu) == 0u) {
            continue;
        }

        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts;
        face->numVerts = 4;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = kSpriteTextureMode;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        const float depth = draw.useProjectedDepth ? draw.projectedZ - kAttachedActorSpriteDepthBias : 0.0f;
        const float oow = draw.useProjectedDepth ? draw.projectedOow : 1.0f;
        face->alphaSortKey = draw.alphaSortBase + clipSortOffset;

        face->m_verts[0] = { drawLeft,  drawTop,    depth, oow, color, 0xFF000000u, flipX ? maxU : 0.0f, 0.0f };
        face->m_verts[1] = { drawRight, drawTop,    depth, oow, color, 0xFF000000u, flipX ? 0.0f : maxU, 0.0f };
        face->m_verts[2] = { drawLeft,  drawBottom, depth, oow, color, 0xFF000000u, flipX ? maxU : 0.0f, maxV };
        face->m_verts[3] = { drawRight, drawBottom, depth, oow, color, 0xFF000000u, flipX ? 0.0f : maxU, maxV };
        g_renderer.AddRP(face, draw.useProjectedDepth ? 1 : (1 | 8));
        clipSortOffset += 0.0001f;
        queuedAny = true;
    }

    return queuedAny;
}

bool GetMsgSpriteDrawBounds(const QueuedMsgSpriteEffectDraw& draw, RECT* outRect)
{
    if (!outRect) {
        return false;
    }

    CSprRes* sprite = nullptr;
    CActRes* act = nullptr;
    if (!ResolveMsgSpriteResources(draw.spritePath, draw.actPath, &sprite, &act)) {
        return false;
    }
    RECT clipBox{};
    if (!ResolveMsgSpriteClipBox(sprite, act, draw.actionIndex, draw.motionIndex, &clipBox)) {
        return false;
    }

    RECT referenceClipBox = clipBox;
    if (draw.usePerspectiveScale) {
        ResolveMsgSpriteReferenceClipBox(sprite, act, &referenceClipBox);
    }

    const int nativeWidth = (std::max)(1, static_cast<int>(clipBox.right - clipBox.left));
    const int nativeHeight = (std::max)(1, static_cast<int>(clipBox.bottom - clipBox.top));
    const int referenceWidth = (std::max)(1, static_cast<int>(referenceClipBox.right - referenceClipBox.left));
    const float scale = ResolveQueuedMsgSpriteScale(draw, referenceWidth);
    const int outWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeWidth) * scale)));
    const int outHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(nativeHeight) * scale)));
    const int originX = draw.screenX - outWidth / 2;
    const int originY = draw.screenY - outHeight / 2;
    outRect->left = originX - 1;
    outRect->top = originY - 1;
    outRect->right = originX + outWidth + 1;
    outRect->bottom = originY + outHeight + 1;
    return true;
}

int ResolveMsgSpriteMotionCount(int actionIndex)
{
    CActRes* act = GetMissEffectAct();
    if (!act) {
        return 0;
    }

    int motionCount = 0;
    while (act->GetMotion(actionIndex, motionCount)) {
        ++motionCount;
    }
    return motionCount;
}

void ResolveLateralOffset(float rotationDegrees, float* outX, float* outZ)
{
    if (!outX || !outZ) {
        return;
    }

    *outX = 0.0f;
    *outZ = 0.0f;

    int dir = 0;
    int normalized = static_cast<int>(rotationDegrees);
    while (normalized < 0) {
        normalized += 360;
    }
    while (normalized >= 360) {
        normalized -= 360;
    }

    if (normalized < 90) {
        dir = 0;
    } else if (normalized < 180) {
        dir = 2;
    } else if (normalized < 270) {
        dir = 4;
    } else {
        dir = 6;
    }

    switch (dir) {
    case 0:
        *outX = -0.8f;
        *outZ = 0.8f;
        break;
    case 2:
        *outX = -0.8f;
        *outZ = -0.8f;
        break;
    case 4:
        *outX = 0.8f;
        *outZ = -0.8f;
        break;
    default:
        *outX = 0.8f;
        *outZ = 0.8f;
        break;
    }
}

int ResolveMsgSpriteMotionCountForEffect(const CMsgEffect& effect, int actionIndex)
{
    CSprRes* sprite = nullptr;
    CActRes* act = nullptr;
    if (!ResolveMsgSpriteResources(effect.m_customSpritePath, effect.m_customActPath, &sprite, &act)) {
        return 0;
    }

    int motionCount = 0;
    while (act->GetMotion(actionIndex, motionCount)) {
        ++motionCount;
    }
    return motionCount;
}

float LengthXZ(const vector3d& value)
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

vector3d NormalizeXZ(const vector3d& value)
{
    const float length = LengthXZ(value);
    if (length <= 0.0001f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return { value.x / length, 0.0f, value.z / length };
}

vector3d Normalize3D(const vector3d& value)
{
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return { value.x / length, value.y / length, value.z / length };
}

vector3d ScaleVector(const vector3d& value, float scale)
{
    return { value.x * scale, value.y * scale, value.z * scale };
}

vector3d AddVector(const vector3d& lhs, const vector3d& rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

bool ResolveActorInterpolatedPositionForMsgEffect(const CGameActor& actor, u32 now, vector3d* outPos)
{
    if (!outPos) {
        return false;
    }

    if (!actor.m_isMoving) {
        *outPos = actor.m_pos;
        return true;
    }

    const CWorld* world = &g_world;
    if (actor.m_path.m_cells.size() >= 2) {
        size_t startIndex = 0;
        if (FindActivePathSegmentForMsgEffect(actor.m_path, now, &startIndex)) {
            const PathCell& startCell = actor.m_path.m_cells[startIndex];
            const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
            const u32 startTime = startCell.arrivalTime;
            const u32 endTime = endCell.arrivalTime;
            const float duration = static_cast<float>((std::max)(1u, endTime - startTime));
            const float ratio = static_cast<float>(now - startTime) / duration;
            const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));
            const float startX = startIndex == 0
                ? actor.m_moveStartPos.x
                : TileToWorldCoordXForMsgEffect(world, startCell.x);
            const float startZ = startIndex == 0
                ? actor.m_moveStartPos.z
                : TileToWorldCoordZForMsgEffect(world, startCell.y);
            const float endX = TileToWorldCoordXForMsgEffect(world, endCell.x);
            const float endZ = TileToWorldCoordZForMsgEffect(world, endCell.y);
            outPos->x = startX + (endX - startX) * clamped;
            outPos->z = startZ + (endZ - startZ) * clamped;
            outPos->y = ResolveWorldGroundHeight(world, outPos->x, outPos->z);
            return true;
        }
    }

    if (actor.m_moveEndTime > actor.m_moveStartTime && now < actor.m_moveEndTime) {
        const float duration = static_cast<float>(actor.m_moveEndTime - actor.m_moveStartTime);
        const float ratio = static_cast<float>(now - actor.m_moveStartTime) / duration;
        const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));
        outPos->x = actor.m_moveStartPos.x + (actor.m_moveEndPos.x - actor.m_moveStartPos.x) * clamped;
        outPos->y = actor.m_moveStartPos.y + (actor.m_moveEndPos.y - actor.m_moveStartPos.y) * clamped;
        outPos->z = actor.m_moveStartPos.z + (actor.m_moveEndPos.z - actor.m_moveStartPos.z) * clamped;
        return true;
    }

    *outPos = actor.m_moveEndPos;
    if (!std::isfinite(outPos->x) || !std::isfinite(outPos->z)) {
        *outPos = actor.m_pos;
    }
    outPos->y = ResolveWorldGroundHeight(world, outPos->x, outPos->z);
    return true;
}

float ResolveActorPlanarSpeedForMsgEffect(const CGameActor& actor, u32 now)
{
    const CWorld* world = &g_world;
    size_t startIndex = 0;
    if (actor.m_path.m_cells.size() >= 2
        && FindActivePathSegmentForMsgEffect(actor.m_path, now, &startIndex)) {
        const PathCell& startCell = actor.m_path.m_cells[startIndex];
        const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
        const float startX = startIndex == 0
            ? actor.m_moveStartPos.x
            : TileToWorldCoordXForMsgEffect(world, startCell.x);
        const float startZ = startIndex == 0
            ? actor.m_moveStartPos.z
            : TileToWorldCoordZForMsgEffect(world, startCell.y);
        const float endX = TileToWorldCoordXForMsgEffect(world, endCell.x);
        const float endZ = TileToWorldCoordZForMsgEffect(world, endCell.y);
        const float distance = std::hypot(endX - startX, endZ - startZ);
        const float durationMs = static_cast<float>((std::max)(1u, endCell.arrivalTime - startCell.arrivalTime));
        return durationMs > 0.0f ? distance * 1000.0f / durationMs : 0.0f;
    }

    if (actor.m_moveEndTime > actor.m_moveStartTime && now < actor.m_moveEndTime) {
        const float distance = std::hypot(actor.m_moveEndPos.x - actor.m_moveStartPos.x,
            actor.m_moveEndPos.z - actor.m_moveStartPos.z);
        const float durationMs = static_cast<float>(actor.m_moveEndTime - actor.m_moveStartTime);
        return durationMs > 0.0f ? distance * 1000.0f / durationMs : 0.0f;
    }

    return 0.0f;
}

bool ResolveActorTrailDirectionForMsgEffect(const CGameActor& actor, u32 now, vector3d* outDir)
{
    if (!outDir) {
        return false;
    }

    vector3d currentPos{};
    if (!ResolveActorInterpolatedPositionForMsgEffect(actor, now, &currentPos)) {
        return false;
    }

    const u32 sampleWindowMs = static_cast<u32>(kAttachedActorTrailSampleMs);
    const u32 previousNow = now > sampleWindowMs ? now - sampleWindowMs : 0u;
    vector3d previousPos = currentPos;
    if (!ResolveActorInterpolatedPositionForMsgEffect(actor, previousNow, &previousPos)) {
        previousPos = currentPos;
    }

    const vector3d delta = {
        currentPos.x - previousPos.x,
        0.0f,
        currentPos.z - previousPos.z,
    };
    const float length = LengthXZ(delta);
    if (length > 0.001f) {
        *outDir = { delta.x / length, 0.0f, delta.z / length };
        return true;
    }

    const float radians = ResolveActorFacingRotationForMsgEffect(actor, now) * (3.14159265f / 180.0f);
    *outDir = { std::sin(radians), 0.0f, -std::cos(radians) };
    return true;
}

float ResolveActorFacingRotationForMsgEffect(const CGameActor& actor, u32 now)
{
    const CWorld* world = &g_world;
    size_t startIndex = 0;
    if (actor.m_path.m_cells.size() >= 2
        && FindActivePathSegmentForMsgEffect(actor.m_path, now, &startIndex)) {
        const PathCell& startCell = actor.m_path.m_cells[startIndex];
        const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
        const float startX = startIndex == 0
            ? actor.m_moveStartPos.x
            : TileToWorldCoordXForMsgEffect(world, startCell.x);
        const float startZ = startIndex == 0
            ? actor.m_moveStartPos.z
            : TileToWorldCoordZForMsgEffect(world, startCell.y);
        const float endX = TileToWorldCoordXForMsgEffect(world, endCell.x);
        const float endZ = TileToWorldCoordZForMsgEffect(world, endCell.y);
        const float deltaX = endX - startX;
        const float deltaZ = endZ - startZ;
        if (std::fabs(deltaX) > 0.0001f || std::fabs(deltaZ) > 0.0001f) {
            return std::atan2(deltaX, -deltaZ) * (180.0f / 3.14159265f);
        }
    }

    if (actor.m_moveEndTime > actor.m_moveStartTime && now < actor.m_moveEndTime) {
        const float deltaX = actor.m_moveEndPos.x - actor.m_moveStartPos.x;
        const float deltaZ = actor.m_moveEndPos.z - actor.m_moveStartPos.z;
        if (std::fabs(deltaX) > 0.0001f || std::fabs(deltaZ) > 0.0001f) {
            return std::atan2(deltaX, -deltaZ) * (180.0f / 3.14159265f);
        }
    }

    return actor.m_roty;
}

int ResolveAttachedActorFacingAction(const CMsgEffect& effect,
    const vector3d& effectPos,
    const vector3d& targetPos,
    float ownerRotationDegrees)
{
    if (!effect.m_masterActor) {
        return 0;
    }

    const float deltaX = targetPos.x - effectPos.x;
    const float deltaZ = targetPos.z - effectPos.z;
    if (std::fabs(deltaX) <= 0.0001f && std::fabs(deltaZ) <= 0.0001f) {
        return effect.m_masterActor->Get8Dir(ownerRotationDegrees);
    }

    const float radians = std::atan2(deltaX, -deltaZ);
    const float degrees = radians * (180.0f / 3.14159265f);
    return effect.m_masterActor->Get8Dir(degrees);
}

void InitializeAttachedActorTrailPosition(CMsgEffect& effect)
{
    if (!effect.m_masterActor) {
        return;
    }

    const u32 now = timeGetTime();
    if (!ResolveAttachedActorTrailPosition(*effect.m_masterActor, now, &effect.m_pos)) {
        ResolveAttachedActorIdlePosition(*effect.m_masterActor, now, &effect.m_pos);
    }
    effect.m_orgPos = effect.m_pos;
    effect.m_followRenderPos = effect.m_pos;
    effect.m_followTravelDistance = 0.0f;
    effect.m_followInitialized = 1;
    effect.m_followRenderInitialized = 1;
    effect.m_followRenderTick = now;
}

bool BuildMsgSpriteBillboardBasis(vector3d* outUp, vector3d* outRight)
{
    if (!outUp || !outRight) {
        return false;
    }

    vector3d up = { -g_renderer.m_eyeUp.x, -g_renderer.m_eyeUp.y, -g_renderer.m_eyeUp.z };
    vector3d right = { g_renderer.m_eyeRight.x, g_renderer.m_eyeRight.y, g_renderer.m_eyeRight.z };
    const float upLength = std::sqrt(up.x * up.x + up.y * up.y + up.z * up.z);
    const float rightLength = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (upLength <= 0.0001f || rightLength <= 0.0001f) {
        return false;
    }

    *outUp = { up.x / upLength, up.y / upLength, up.z / upLength };
    *outRight = { right.x / rightLength, right.y / rightLength, right.z / rightLength };
    return true;
}

bool ProcessAttachedActorSpriteState(CMsgEffect& effect)
{
    if (!effect.m_masterActor) {
        return false;
    }

    const CGameActor& actor = *effect.m_masterActor;
    const u32 now = timeGetTime();
    if (!effect.m_followInitialized) {
        InitializeAttachedActorTrailPosition(effect);
    }

    const float followDistance = ResolveWorldTileSize(&g_world);
    if (followDistance <= 0.0f) {
        return false;
    }

    const vector3d previousPos = effect.m_pos;
    vector3d offsetFromActor = {
        effect.m_pos.x - actor.m_pos.x,
        0.0f,
        effect.m_pos.z - actor.m_pos.z,
    };
    const float currentDistance = LengthXZ(offsetFromActor);
    bool moved = false;
    if (currentDistance > followDistance) {
        const vector3d direction = NormalizeXZ(offsetFromActor);
        effect.m_pos.x = actor.m_pos.x + direction.x * followDistance;
        effect.m_pos.z = actor.m_pos.z + direction.z * followDistance;
        moved = true;
    }
    effect.m_pos.y = ResolveWorldGroundHeight(&g_world, effect.m_pos.x, effect.m_pos.z);

    const float ownerRotationDegrees = ResolveActorFacingRotationForMsgEffect(actor, now);
    int actionIndex = ResolveAttachedActorFacingAction(effect, effect.m_pos, actor.m_pos, ownerRotationDegrees);
    int motionCount = ResolveMsgSpriteMotionCountForEffect(effect, actionIndex);
    if (motionCount <= 0) {
        for (int fallbackAction = 0; fallbackAction < 8; ++fallbackAction) {
            motionCount = ResolveMsgSpriteMotionCountForEffect(effect, fallbackAction);
            if (motionCount > 0) {
                actionIndex = fallbackAction;
                break;
            }
        }
    }
    if (motionCount <= 0) {
        return false;
    }

    effect.m_spriteActionIndex = actionIndex;
    if (moved) {
        const float traveledX = effect.m_pos.x - previousPos.x;
        const float traveledZ = effect.m_pos.z - previousPos.z;
        effect.m_followTravelDistance += std::sqrt(traveledX * traveledX + traveledZ * traveledZ);
        effect.m_spriteMotionIndex = static_cast<int>(effect.m_followTravelDistance * 0.92f) % motionCount;
    } else if (effect.m_spriteMotionIndex >= motionCount) {
        effect.m_spriteMotionIndex = 0;
    }

    if (!effect.m_followRenderInitialized) {
        effect.m_followRenderPos = effect.m_pos;
        effect.m_followRenderInitialized = 1;
        effect.m_followRenderTick = now;
    }
    return true;
}

bool UpdateAttachedActorSpritePose(CMsgEffect& effect);

bool RenderAttachedActorSpriteWorldBillboard(const CMsgEffect& effect,
    const matrix& viewMatrix,
    float alphaSortBase)
{
    CSprRes* sprite = nullptr;
    CActRes* act = nullptr;
    if (!ResolveMsgSpriteResources(effect.m_customSpritePath, effect.m_customActPath, &sprite, &act)) {
        return false;
    }

    const CMotion* motion = act->GetMotion(effect.m_spriteActionIndex, effect.m_spriteMotionIndex);
    if (!motion) {
        motion = act->GetMotion(effect.m_spriteActionIndex, 0);
    }
    if (!motion) {
        return false;
    }

    RECT referenceClipBox{};
    if (!ResolveMsgSpriteReferenceClipBox(sprite, act, &referenceClipBox)) {
        if (!ResolveMsgSpriteClipBox(sprite, act, effect.m_spriteActionIndex, effect.m_spriteMotionIndex, &referenceClipBox)) {
            return false;
        }
    }

    const int referenceWidth = (std::max)(1, static_cast<int>(referenceClipBox.right - referenceClipBox.left));
    const int referenceHeight = (std::max)(1, static_cast<int>(referenceClipBox.bottom - referenceClipBox.top));
    const float worldTileSize = ResolveWorldTileSize(&g_world);
    const float actorZoom = effect.m_masterActor && effect.m_masterActor->m_zoom > 0.0f ? effect.m_masterActor->m_zoom : 1.0f;
    const float desiredWorldWidth = worldTileSize * (std::max)(0.5f, kAttachedActorSpriteWorldWidthCells * actorZoom);
    const float unitsPerPixel = desiredWorldWidth / static_cast<float>(referenceWidth);
    if (unitsPerPixel <= 0.0f) {
        return false;
    }

    const vector3d& renderPos = ResolveAttachedActorDisplayPosition(effect);

    tlvertex3d projectedBase{};
    if (!ProjectMsgEffectPoint(viewMatrix, renderPos, &projectedBase)) {
        return false;
    }

    vector3d renderUp{};
    vector3d renderRight{};
    if (!BuildMsgSpriteBillboardBasis(&renderUp, &renderRight)) {
        return false;
    }

    vector3d eyeForward = Normalize3D(g_renderer.m_eyeForward);
    if (std::fabs(eyeForward.x) < 0.0001f && std::fabs(eyeForward.y) < 0.0001f && std::fabs(eyeForward.z) < 0.0001f) {
        eyeForward = { 0.0f, 0.0f, 1.0f };
    }
    vector3d flatForward = NormalizeXZ({ eyeForward.x, 0.0f, eyeForward.z });
    if (std::fabs(flatForward.x) < 0.0001f && std::fabs(flatForward.z) < 0.0001f) {
        flatForward = { 0.0f, 0.0f, 1.0f };
    }

    const vector3d depthUp = { 0.0f, -1.0f, 0.0f };
    const float worldHeight = static_cast<float>(referenceHeight) * unitsPerPixel;
    const float fullTopUnits = static_cast<float>((std::max)(0L, -static_cast<long>(referenceClipBox.top))) * unitsPerPixel;
    const float fullBottomUnits = static_cast<float>((std::max)(0L, static_cast<long>(referenceClipBox.bottom))) * unitsPerPixel;
    const float depthBiasWorld = (std::max)(kAttachedActorDepthWorldBiasMin,
        (std::min)(kAttachedActorDepthWorldBiasMax, worldHeight * kAttachedActorDepthWorldBiasScale));
    const vector3d depthAnchorBase = AddVector(renderPos, ScaleVector(depthUp, fullTopUnits * kAttachedActorDepthHeightBiasFactor));
    const float baseTopForwardBiasWorld = depthBiasWorld
        * kAttachedActorTopDepthForwardBiasScale
        * kAttachedActorDepthHeightBiasFactor;

    tlvertex3d projectedDepthBase = projectedBase;
    {
        const vector3d depthBiasedBase = AddVector(
            AddVector(depthAnchorBase, ScaleVector(flatForward, -depthBiasWorld)),
            ScaleVector(eyeForward, -baseTopForwardBiasWorld));
        ProjectMsgEffectPoint(viewMatrix, depthBiasedBase, &projectedDepthBase);
    }

    tlvertex3d projectedDepthTop = projectedDepthBase;
    {
        const vector3d fullDepthTopCenter = AddVector(renderPos, ScaleVector(depthUp, fullTopUnits));
        const vector3d depthBiasedTop = AddVector(
            AddVector(fullDepthTopCenter, ScaleVector(flatForward, -depthBiasWorld)),
            ScaleVector(eyeForward, -(depthBiasWorld * kAttachedActorTopDepthForwardBiasScale)));
        ProjectMsgEffectPoint(viewMatrix, depthBiasedTop, &projectedDepthTop);
    }

    tlvertex3d projectedDepthBottom = projectedBase;
    {
        const vector3d fullDepthBottomCenter = AddVector(renderPos, ScaleVector(depthUp, -fullBottomUnits));
        const vector3d depthBiasedBottom = AddVector(fullDepthBottomCenter, ScaleVector(flatForward, -depthBiasWorld));
        ProjectMsgEffectPoint(viewMatrix, depthBiasedBottom, &projectedDepthBottom);
    }

    bool renderedAny = false;
    float clipSortOffset = 0.0f;
    QueuedMsgSpriteEffectDraw colorSource{};
    colorSource.colorArgb = effect.m_colorArgb;
    colorSource.alpha = effect.m_alpha;
    for (const CSprClip& clip : motion->sprClips) {
        const SprImg* image = sprite->GetSprite(clip.clipType, clip.sprIndex);
        if (!image || image->width <= 0 || image->height <= 0) {
            continue;
        }

        CTexture* texture = GetOrCreateSpriteClipTexture(sprite, image);
        if (!texture || texture == &CTexMgr::s_dummy_texture) {
            continue;
        }

        const u32 color = BuildMsgSpriteClipColor(colorSource, clip);
        if (((color >> 24) & 0xFFu) == 0u) {
            continue;
        }

        const float localLeft = static_cast<float>(clip.x);
        const float localTop = static_cast<float>(clip.y);
        const float localRight = localLeft + static_cast<float>(image->width);
        const float localBottom = localTop + static_cast<float>(image->height);

        const float leftUnits = localLeft * unitsPerPixel;
        const float rightUnits = localRight * unitsPerPixel;
        const float topUnits = -localTop * unitsPerPixel;
        const float bottomUnits = -localBottom * unitsPerPixel;

        const vector3d topLeft = AddVector(AddVector(renderPos, ScaleVector(renderUp, topUnits)), ScaleVector(renderRight, leftUnits));
        const vector3d topRight = AddVector(AddVector(renderPos, ScaleVector(renderUp, topUnits)), ScaleVector(renderRight, rightUnits));
        const vector3d bottomLeft = AddVector(AddVector(renderPos, ScaleVector(renderUp, bottomUnits)), ScaleVector(renderRight, leftUnits));
        const vector3d bottomRight = AddVector(AddVector(renderPos, ScaleVector(renderUp, bottomUnits)), ScaleVector(renderRight, rightUnits));

        tlvertex3d projectedVerts[4] = {};
        if (!ProjectMsgEffectPoint(viewMatrix, topLeft, &projectedVerts[0])
            || !ProjectMsgEffectPoint(viewMatrix, topRight, &projectedVerts[1])
            || !ProjectMsgEffectPoint(viewMatrix, bottomLeft, &projectedVerts[2])
            || !ProjectMsgEffectPoint(viewMatrix, bottomRight, &projectedVerts[3])) {
            continue;
        }

        const float maxU = texture->m_w != 0
            ? static_cast<float>(texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(image->width))
                / static_cast<float>(texture->m_w)
            : 1.0f;
        const float maxV = texture->m_h != 0
            ? static_cast<float>(texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(image->height))
                / static_cast<float>(texture->m_h)
            : 1.0f;

        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            continue;
        }

        const bool flipX = (clip.flags & 1) != 0;
        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts;
        face->numVerts = 4;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = kSpriteTextureMode;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_INVSRCALPHA;

        projectedVerts[0].z = (std::max)(0.0f, projectedDepthTop.z - kAttachedActorSpriteDepthBias);
        projectedVerts[0].oow = projectedDepthTop.oow;
        projectedVerts[1].z = (std::max)(0.0f, projectedDepthTop.z - kAttachedActorSpriteDepthBias);
        projectedVerts[1].oow = projectedDepthTop.oow;
        projectedVerts[2].z = (std::max)(0.0f, projectedDepthBottom.z - kAttachedActorSpriteDepthBias);
        projectedVerts[2].oow = projectedDepthBottom.oow;
        projectedVerts[3].z = (std::max)(0.0f, projectedDepthBottom.z - kAttachedActorSpriteDepthBias);
        projectedVerts[3].oow = projectedDepthBottom.oow;
        for (int index = 0; index < 4; ++index) {
            projectedVerts[index].color = color;
            projectedVerts[index].specular = 0xFF000000u;
        }

        projectedVerts[0].tu = flipX ? maxU : 0.0f;
        projectedVerts[0].tv = 0.0f;
        projectedVerts[1].tu = flipX ? 0.0f : maxU;
        projectedVerts[1].tv = 0.0f;
        projectedVerts[2].tu = flipX ? maxU : 0.0f;
        projectedVerts[2].tv = maxV;
        projectedVerts[3].tu = flipX ? 0.0f : maxU;
        projectedVerts[3].tv = maxV;

        face->m_verts[0] = projectedVerts[0];
        face->m_verts[1] = projectedVerts[1];
        face->m_verts[2] = projectedVerts[2];
        face->m_verts[3] = projectedVerts[3];
        face->alphaSortKey = alphaSortBase + clipSortOffset;
        g_renderer.AddRP(face, 1);
        clipSortOffset += 0.0001f;
        renderedAny = true;
    }

    return renderedAny;
}

bool QueueAttachedActorSpriteProjectedFallback(const CMsgEffect& effect,
    const matrix& viewMatrix,
    float alphaSortBase)
{
    CSprRes* sprite = nullptr;
    CActRes* act = nullptr;
    if (!ResolveMsgSpriteResources(effect.m_customSpritePath, effect.m_customActPath, &sprite, &act)) {
        return false;
    }

    const CMotion* motion = act->GetMotion(effect.m_spriteActionIndex, effect.m_spriteMotionIndex);
    if (!motion) {
        motion = act->GetMotion(effect.m_spriteActionIndex, 0);
    }
    if (!motion || motion->sprClips.empty()) {
        return false;
    }

    const vector3d& renderPos = ResolveAttachedActorDisplayPosition(effect);
    tlvertex3d projectedBase{};
    if (!ProjectMsgEffectPoint(viewMatrix, renderPos, &projectedBase)) {
        return false;
    }

    QueuedMsgSpriteEffectDraw draw{};
    draw.screenX = static_cast<int>(std::lround(projectedBase.x));
    draw.screenY = static_cast<int>(std::lround(projectedBase.y));
    draw.actionIndex = effect.m_spriteActionIndex;
    draw.motionIndex = effect.m_spriteMotionIndex;
    draw.spritePath = effect.m_customSpritePath;
    draw.actPath = effect.m_customActPath;
    draw.colorArgb = effect.m_colorArgb;
    draw.alpha = effect.m_alpha;
    draw.zoom = effect.m_zoom;
    draw.alphaSortBase = alphaSortBase;
    draw.projectedZ = projectedBase.z;
    draw.projectedOow = projectedBase.oow;
    draw.useProjectedDepth = true;
    draw.usePerspectiveScale = true;
    draw.bypassZoomClamp = true;
    draw.pixelsPerTile = ResolveProjectedTilePixels(viewMatrix, renderPos);
    const float actorZoom = effect.m_masterActor && effect.m_masterActor->m_zoom > 0.0f
        ? effect.m_masterActor->m_zoom
        : 1.0f;
    draw.desiredWorldWidthCells = kAttachedActorSpriteWorldWidthCells * actorZoom;
    g_queuedMsgSpriteEffects.push_back(draw);
    return true;
}

bool RenderAttachedActorEffectInternal(CMsgEffect& effect,
    const matrix& viewMatrix,
    float ownerDepthKey,
    float ownerScreenY,
    bool afterOwner,
    u32 frameId)
{
    if (!effect.m_masterActor || !effect.m_isVisible || effect.m_isDisappear) {
        if (ShouldTraceAttachedCartEffect(effect)) {
            DbgLog("[CartEffect] skip render gid=%u effect=%p visible=%d disappear=%d owner=%p\n",
                static_cast<unsigned int>(effect.m_masterActor ? effect.m_masterActor->m_gid : 0),
                static_cast<void*>(&effect),
                effect.m_isVisible,
                effect.m_isDisappear,
                static_cast<void*>(effect.m_masterActor));
        }
        return false;
    }

    if (effect.m_followLastRenderFrame != frameId) {
        if (!UpdateAttachedActorSpritePose(effect)) {
            return false;
        }

        const vector3d& renderPos = ResolveAttachedActorDisplayPosition(effect);
        tlvertex3d projectedBase{};
        if (!ProjectMsgEffectPoint(viewMatrix, renderPos, &projectedBase)) {
            return false;
        }

        effect.m_followRenderAfterOwner = projectedBase.y >= ownerScreenY ? 1 : 0;
        effect.m_followLastRenderFrame = frameId;
    }

    if ((afterOwner ? 1u : 0u) != effect.m_followRenderAfterOwner) {
        return false;
    }

    const float relativeDepthBias = afterOwner ? 0.0002f : -0.0002f;
    const float alphaSortBase = ownerDepthKey + relativeDepthBias;
    if (RenderAttachedActorSpriteWorldBillboard(effect, viewMatrix, alphaSortBase)) {
        static std::map<u32, int> s_cartRenderSuccessLogs;
        if (ShouldTraceAttachedCartEffect(effect) && s_cartRenderSuccessLogs[effect.m_masterActor->m_gid] < 8) {
            ++s_cartRenderSuccessLogs[effect.m_masterActor->m_gid];
            const vector3d& renderPos = ResolveAttachedActorDisplayPosition(effect);
            DbgLog("[CartEffect] render world gid=%u effect=%p after=%d action=%d motion=%d pos=(%.2f,%.2f,%.2f)\n",
                static_cast<unsigned int>(effect.m_masterActor->m_gid),
                static_cast<void*>(&effect),
                afterOwner ? 1 : 0,
                effect.m_spriteActionIndex,
                effect.m_spriteMotionIndex,
                renderPos.x,
                renderPos.y,
                renderPos.z);
        }
        return true;
    }

    const bool queuedFallback = QueueAttachedActorSpriteProjectedFallback(effect, viewMatrix, alphaSortBase);
    static std::map<u32, int> s_cartRenderFallbackLogs;
    if (ShouldTraceAttachedCartEffect(effect) && s_cartRenderFallbackLogs[effect.m_masterActor->m_gid] < 8) {
        ++s_cartRenderFallbackLogs[effect.m_masterActor->m_gid];
        const vector3d& renderPos = ResolveAttachedActorDisplayPosition(effect);
        DbgLog("[CartEffect] render fallback gid=%u effect=%p after=%d queued=%d action=%d motion=%d pos=(%.2f,%.2f,%.2f)\n",
            static_cast<unsigned int>(effect.m_masterActor->m_gid),
            static_cast<void*>(&effect),
            afterOwner ? 1 : 0,
            queuedFallback ? 1 : 0,
            effect.m_spriteActionIndex,
            effect.m_spriteMotionIndex,
            renderPos.x,
            renderPos.y,
            renderPos.z);
    }
    return queuedFallback;
}

bool UpdateAttachedActorSpritePose(CMsgEffect& effect)
{
    if (!effect.m_masterActor) {
        return false;
    }

    const u32 now = timeGetTime();
    if (!effect.m_followInitialized) {
        InitializeAttachedActorTrailPosition(effect);
    }

    if (!effect.m_followRenderInitialized) {
        effect.m_followRenderPos = effect.m_pos;
        effect.m_followRenderInitialized = 1;
        effect.m_followRenderTick = now;
        return true;
    }

    const u32 elapsedMs = now > effect.m_followRenderTick ? now - effect.m_followRenderTick : 0u;
    const float clampedElapsedMs = static_cast<float>((std::min)(elapsedMs, 100u));
    const float alpha = 1.0f - std::exp(-clampedElapsedMs / 24.0f);
    effect.m_followRenderPos.x += (effect.m_pos.x - effect.m_followRenderPos.x) * alpha;
    effect.m_followRenderPos.z += (effect.m_pos.z - effect.m_followRenderPos.z) * alpha;
    effect.m_followRenderPos.y = ResolveWorldGroundHeight(&g_world, effect.m_followRenderPos.x, effect.m_followRenderPos.z);
    effect.m_followRenderTick = now;
    return true;
}

} // namespace

CMsgEffect::CMsgEffect()
    : m_msgEffectType(0)
    , m_digit(0)
    , m_numberValue(0)
    , m_sprShift(0)
    , m_spriteActionIndex(0)
    , m_spriteMotionIndex(0)
    , m_lastProcessedSpriteMotionIndex(-1)
    , m_alpha(255)
    , m_masterGid(0)
    , m_colorArgb(0xFFFFFFFFu)
    , m_stateStartTick(timeGetTime())
    , m_pos{}
    , m_orgPos{}
    , m_destPos{}
    , m_destPos2{}
    , m_followRenderPos{}
    , m_zoom(1.0f)
    , m_orgZoom(1.0f)
    , m_followTravelDistance(0.0f)
    , m_followLastRenderFrame(0)
    , m_followRenderTick(0)
    , m_masterActor(nullptr)
    , m_isVisible(0)
    , m_isDisappear(0)
    , m_removedFromOwner(0)
    , m_followInitialized(0)
    , m_followRenderInitialized(0)
    , m_followRenderAfterOwner(0)
{
}

CMsgEffect::~CMsgEffect()
{
    if (!m_removedFromOwner && m_masterActor) {
        m_masterActor->DeleteMatchingEffect(this);
    }
}

void CMsgEffect::OnActorDeleted(const CGameActor* actor)
{
    if (!actor || m_masterActor != actor) {
        return;
    }

    m_masterActor = nullptr;
    m_removedFromOwner = 1;
}

u8 CMsgEffect::OnProcess()
{
    if (m_isDisappear) {
        if (!m_removedFromOwner && m_masterActor) {
            m_masterActor->DeleteMatchingEffect(this);
            m_removedFromOwner = 1;
        }
        return 0;
    }

    const float stateCount = static_cast<float>(timeGetTime() - m_stateStartTick) * 0.041666668f;
    switch (m_msgEffectType) {
    case 12:
        if (m_masterActor) {
            m_pos = m_masterActor->m_pos;
            m_pos.y -= 20.0f;
            if (static_cast<int>(stateCount) % 34 == 0) {
                m_masterActor->SendMsg(nullptr, 77, 0, 0, 0);
            }
        }
        break;
    case 4: {
        if (!m_masterActor) {
            m_isDisappear = 1;
            break;
        }
        m_pos = m_masterActor->m_pos;
        m_pos.y -= 15.0f;
        const int motionCount = ResolveMsgSpriteMotionCountForEffect(*this, m_spriteActionIndex);
        if (motionCount <= 0) {
            return 1;
        }
        m_spriteMotionIndex = static_cast<int>(stateCount) % motionCount;
        break;
    }
    case 1: {
        if (m_masterActor && m_masterActor->m_chatProhibitEffect == this) {
            m_spriteMotionIndex = 15;
            m_colorArgb = 0xFFFA5050u;
            break;
        }
        const int motionCount = ResolveMsgSpriteMotionCountForEffect(*this, m_spriteActionIndex);
        if (motionCount <= 0) {
            return 1;
        }
        m_spriteMotionIndex = static_cast<int>(stateCount) % motionCount;
        if (stateCount / static_cast<float>(motionCount) >= 1.0f) {
            m_isDisappear = 1;
        }
        break;
    }
    case kAttachedActorSpriteMsgEffectType:
        if (!m_masterActor) {
            m_isDisappear = 1;
            break;
        }
        m_alpha = 255;
        m_zoom = m_masterActor->m_zoom > 0.0f ? m_masterActor->m_zoom : 1.0f;
        ProcessAttachedActorSpriteState(*this);
        break;
    case kCriticalMsgEffectType: {
        if (m_destPos.x == 0.0f && m_masterActor) {
            ResolveLateralOffset(m_masterActor->m_roty, &m_destPos.x, &m_destPos.z);
            m_destPos.x *= 0.625f;
            m_destPos.z *= 0.625f;
        }

        m_pos.x = m_orgPos.x + (stateCount * 0.33333334f + 3.0f) * m_destPos.x + m_destPos2.x;
        m_pos.z = m_orgPos.z + (stateCount * 0.33333334f + 3.0f) * m_destPos.z + m_destPos2.z;
        m_pos.y = m_orgPos.y + 8.0f - (2.0f - stateCount * 0.033333335f) * stateCount + m_destPos.y;
        m_destPos.y = m_pos.y >= 0.0f ? 1.3333334f : -1.3333334f;

        m_zoom = stateCount >= 0.0f ? (std::max)(1.0f, m_orgZoom - stateCount * 0.14400001f) : m_orgZoom;
        m_alpha = (std::max)(0, static_cast<int>(255.0f - stateCount * 3.45f));

        const int motionCount = ResolveMsgSpriteMotionCount(m_spriteActionIndex);
        if (motionCount <= 0) {
            m_isDisappear = 1;
            break;
        }

        const int nextMotionIndex = (std::min)(static_cast<int>(stateCount * 0.5f), motionCount - 1);
        if (m_lastProcessedSpriteMotionIndex >= 0 && nextMotionIndex > m_lastProcessedSpriteMotionIndex) {
            for (int motionIndex = m_lastProcessedSpriteMotionIndex + 1; motionIndex <= nextMotionIndex; ++motionIndex) {
                TryPlayMsgSpriteMotionEvent(*this, motionIndex);
            }
        }

        m_spriteMotionIndex = nextMotionIndex;
        m_lastProcessedSpriteMotionIndex = nextMotionIndex;
        if (stateCount >= 70.0f) {
            m_isDisappear = 1;
        }
        break;
    }
    case kLuckyDodgeMsgEffectType: {
        if (m_masterActor) {
            m_pos.x = m_masterActor->m_pos.x;
            m_pos.z = m_masterActor->m_pos.z;
        }

        const int motionCount = ResolveMsgSpriteMotionCount(m_spriteActionIndex);
        if (motionCount <= 0) {
            m_isDisappear = 1;
            break;
        }

        const int nextMotionIndex = (std::min)(static_cast<int>(stateCount), motionCount - 1);
        if (m_lastProcessedSpriteMotionIndex >= 0 && nextMotionIndex > m_lastProcessedSpriteMotionIndex) {
            for (int motionIndex = m_lastProcessedSpriteMotionIndex + 1; motionIndex <= nextMotionIndex; ++motionIndex) {
                TryPlayMsgSpriteMotionEvent(*this, motionIndex);
            }
        }

        m_spriteMotionIndex = nextMotionIndex;
        m_lastProcessedSpriteMotionIndex = nextMotionIndex;
        m_alpha = 255;
        m_zoom = m_orgZoom;
        if (stateCount >= static_cast<float>(motionCount)) {
            m_isDisappear = 1;
        }
        break;
    }
    case 14:
    case 21: {
        if (m_destPos.x == 0.0f && m_masterActor) {
            ResolveLateralOffset(m_masterActor->m_roty, &m_destPos.x, &m_destPos.z);
        }

        m_pos.x = m_orgPos.x + (stateCount * 0.33333334f + 3.0f) * m_destPos.x + m_destPos2.x;
        m_pos.z = m_orgPos.z + (stateCount * 0.33333334f + 3.0f) * m_destPos.z + m_destPos2.z;
        m_pos.y = m_orgPos.y + 8.0f - (2.0f - stateCount * 0.033333335f) * stateCount + m_destPos.y;
        m_zoom = stateCount >= 0.0f ? (std::max)(1.2f, m_orgZoom - stateCount * 0.23999999f) : m_orgZoom;
        m_alpha = (std::max)(0, static_cast<int>(250.0f - stateCount * 3.4f));
        if (stateCount >= 70.0f) {
            m_isDisappear = 1;
        }
        break;
    }
    case 16: {
        if (m_destPos.x == 0.0f && m_masterActor) {
            ResolveLateralOffset(m_masterActor->m_roty, &m_destPos.x, &m_destPos.z);
            m_destPos.x *= 0.625f;
            m_destPos.z *= 0.625f;
        }

        m_pos.x = m_orgPos.x + (stateCount * 0.33333334f + 3.0f) * m_destPos.x + m_destPos2.x;
        m_pos.z = m_orgPos.z + (stateCount * 0.33333334f + 3.0f) * m_destPos.z + m_destPos2.z;
        m_pos.y = m_orgPos.y + 8.0f - (2.0f - stateCount * 0.033333335f) * stateCount + m_destPos.y;
        m_zoom = stateCount >= 0.0f ? (std::max)(1.3f, m_orgZoom - stateCount * 0.23999999f) : m_orgZoom;
        m_alpha = (std::max)(0, static_cast<int>(250.0f - stateCount * 3.4f));
        if (stateCount >= 70.0f) {
            m_isDisappear = 1;
        }
        break;
    }
    case 5:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 18:
        if (m_masterActor) {
            m_pos = m_masterActor->m_pos;
            m_orgPos = m_pos;
        }
        {
            const int motionCount = ResolveMsgSpriteMotionCountForEffect(*this, m_spriteActionIndex);
            if (motionCount > 0) {
                m_spriteMotionIndex = static_cast<int>(stateCount) % motionCount;
            }
        }
        m_alpha = 255;
        m_zoom = m_msgEffectType == 18 ? 6.0f : (m_orgZoom > 0.0f ? m_orgZoom : 1.0f);
        break;
    case 6:
    case 25:
        if (m_masterActor) {
            m_pos.x = m_masterActor->m_pos.x;
            m_pos.z = m_masterActor->m_pos.z;
        }
        {
            const int motionCount = ResolveMsgSpriteMotionCountForEffect(*this, m_spriteActionIndex);
            if (motionCount <= 0 || stateCount >= static_cast<float>(motionCount)) {
                m_isDisappear = 1;
                break;
            }
            m_spriteMotionIndex = static_cast<int>(stateCount);
        }
        m_alpha = 255;
        m_zoom = m_orgZoom > 0.0f ? m_orgZoom : 1.0f;
        break;
    case 22:
        m_pos.y = m_orgPos.y - stateCount * 0.1f;
        if (m_zoom < 3.0f) {
            m_zoom += stateCount * 0.18f;
            if (m_zoom > 3.0f) {
                m_zoom = 3.0f;
            }
        }
        m_alpha = stateCount > 90.0f
            ? (std::max)(0, 250 - static_cast<int>((stateCount - 90.0f) * 8.0f))
            : 250;
        if (stateCount >= 120.0f) {
            m_isDisappear = 1;
        }
        break;
    case 114:
    default:
        if (m_masterActor) {
            m_pos.x = m_masterActor->m_pos.x;
            m_pos.z = m_masterActor->m_pos.z;
        }
        m_pos.y = m_orgPos.y - stateCount * 0.54f;
        m_zoom = 1.0f;
        m_alpha = (std::max)(0, static_cast<int>(250.0f - stateCount * 3.0f));
        if (stateCount >= 80.0f) {
            m_isDisappear = 1;
        }
        break;
    }

    if (m_isDisappear && !m_removedFromOwner && m_masterActor) {
        m_masterActor->DeleteMatchingEffect(this);
        m_removedFromOwner = 1;
    }
    return m_isDisappear ? 0 : 1;
}

bool CMsgEffect::ProcessOwnerMessage(CGameObject* sender, int msg)
{
    if (msg == 50) {
        m_masterActor = dynamic_cast<CGameActor*>(sender);
        m_followInitialized = 0;
        m_followRenderInitialized = 0;
        m_followRenderTick = timeGetTime();
        m_followTravelDistance = 0.0f;
        if (m_masterActor && m_msgEffectType == 114) {
            m_pos.x = m_masterActor->m_pos.x;
            m_pos.z = m_masterActor->m_pos.z;
        }
        if (m_masterActor && m_msgEffectType == kAttachedActorSpriteMsgEffectType) {
            InitializeAttachedActorTrailPosition(*this);
            ProcessAttachedActorSpriteState(*this);
            if (ShouldTraceAttachedCartEffect(*this)) {
                DbgLog("[CartEffect] init gid=%u effect=%p pos=(%.2f,%.2f,%.2f) action=%d motion=%d\n",
                    static_cast<unsigned int>(m_masterActor->m_gid),
                    static_cast<void*>(this),
                    m_pos.x,
                    m_pos.y,
                    m_pos.z,
                    m_spriteActionIndex,
                    m_spriteMotionIndex);
            }
        }
        if (m_masterActor && IsAnimatedSpriteBackedMsgEffectType(m_msgEffectType)) {
            m_lastProcessedSpriteMotionIndex = m_spriteMotionIndex;
            TryPlayMsgSpriteMotionEvent(*this, m_spriteMotionIndex);
        }
        return true;
    }
    if (msg == 53) {
        m_isDisappear = 1;
        return true;
    }
    return false;
}

void CMsgEffect::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)
{
    if (ProcessOwnerMessage(sender, msg)) {
        return;
    }
    switch (msg) {
    case 22:
        m_masterGid = static_cast<u32>(par1);
        m_msgEffectType = static_cast<int>(par2);
        return;
    case 49:
        m_masterActor = nullptr;
        return;
    case 139: {
        m_masterActor = static_cast<CGameActor*>(sender);
        if (sender) {
            m_pos.x = static_cast<float>(static_cast<double>(rand() % 11 - 5) * 0.1 + m_masterActor->m_pos.x);
            m_pos.z = static_cast<float>(static_cast<double>(rand() % 11 - 5) * 0.1 + m_masterActor->m_pos.z);
        }
        return;
    }
    case 64: {
        const vector3d* pos = reinterpret_cast<const vector3d*>(par1);
        if (!pos) {
            m_isDisappear = 1;
            return;
        }

        m_isVisible = 1;
        m_stateStartTick = timeGetTime();
        m_numberValue = (std::max)(0, static_cast<int>(par2));
        m_digit = (std::max)(0, (std::min)(9, m_numberValue % 10));
        m_sprShift = static_cast<int>(par3);
        m_pos = *pos;
        m_orgPos = *pos;

        if (m_msgEffectType == 114) {
            m_pos.y -= 12.0f;
            m_orgPos.y = m_pos.y;
            m_zoom = 5.0f;
            m_orgZoom = 5.0f;
        } else {
            m_pos.y -= 15.0f;
            m_orgPos.y = m_pos.y;
            m_zoom = 5.0f;
            m_orgZoom = 5.0f;
        }
        return;
    }
    default:
        return;
    }
}

void CMsgEffect::Render(matrix* viewMatrix)
{
    if (!m_isVisible || !viewMatrix) {
        return;
    }

    if (m_msgEffectType == kAttachedActorSpriteMsgEffectType) {
        return;
    }

    tlvertex3d projected{};
    if (!ProjectMsgEffectPoint(*viewMatrix, m_pos, &projected)) {
        return;
    }

    if (IsSpriteBackedMsgEffectType(m_msgEffectType)
        || (!m_customSpritePath.empty() && !m_customActPath.empty())) {
        QueuedMsgSpriteEffectDraw draw{};
        draw.screenX = static_cast<int>(std::lround(projected.x));
        draw.screenY = static_cast<int>(std::lround(projected.y));
        draw.actionIndex = m_spriteActionIndex;
        draw.motionIndex = m_spriteMotionIndex;
        draw.spritePath = m_customSpritePath;
        draw.actPath = m_customActPath;
        draw.colorArgb = m_colorArgb;
        draw.alpha = m_alpha;
        draw.zoom = m_zoom;
        draw.alphaSortBase = m_msgEffectType == kAttachedActorSpriteMsgEffectType
            ? projected.oow
            : (m_msgEffectType == kCriticalMsgEffectType ? 1.6f : 1.7f);
        if (m_msgEffectType == kAttachedActorSpriteMsgEffectType) {
            draw.projectedZ = projected.z;
            draw.projectedOow = projected.oow;
            draw.useProjectedDepth = true;
            draw.usePerspectiveScale = true;
            draw.bypassZoomClamp = true;
            draw.pixelsPerTile = ResolveProjectedTilePixels(*viewMatrix, m_pos);
            const float actorZoom = m_masterActor && m_masterActor->m_zoom > 0.0f ? m_masterActor->m_zoom : 1.0f;
            draw.desiredWorldWidthCells = kAttachedActorSpriteWorldWidthCells * actorZoom;
        }
        g_queuedMsgSpriteEffects.push_back(draw);
        return;
    }

    const int digitCount = CountDamageDigits(m_numberValue);
    int digits[6] = {};
    int digitWidths[6] = {};
    int totalWidth = 0;
    const int digitGap = -2;

    for (int index = 0; index < digitCount; ++index) {
        digits[index] = ResolveDigitAtIndex(m_numberValue, digitCount - 1 - index);
        digitWidths[index] = ResolveDigitOutputWidth(digits[index], m_zoom);
        totalWidth += digitWidths[index];
    }
    if (digitCount > 1) {
        totalWidth += digitGap * (digitCount - 1);
    }

    int currentLeft = -totalWidth / 2;
    for (int index = 0; index < digitCount; ++index) {
        QueuedMsgEffectDraw draw{};
        draw.screenX = static_cast<int>(std::lround(projected.x));
        draw.screenY = static_cast<int>(std::lround(projected.y));
        draw.digit = digits[index];
        draw.colorArgb = m_colorArgb;
        draw.alpha = m_alpha;
        draw.zoom = m_zoom;
        draw.sprShift = currentLeft + digitWidths[index] / 2;
        g_queuedMsgEffects.push_back(draw);
        currentLeft += digitWidths[index] + digitGap;
    }
}

bool QueueQueuedMsgEffectsQuads()
{
    bool queuedAny = false;
    for (const QueuedMsgEffectDraw& draw : g_queuedMsgEffects) {
        queuedAny = QueueDigitSpriteQuad(draw) || queuedAny;
    }
    g_queuedMsgEffects.clear();
    for (const QueuedMsgSpriteEffectDraw& draw : g_queuedMsgSpriteEffects) {
        queuedAny = QueueMsgSpriteQuad(draw) || queuedAny;
    }
    g_queuedMsgSpriteEffects.clear();
    return queuedAny;
}

bool GetQueuedMsgEffectsBounds(RECT* outRect)
{
    if (!outRect) {
        return false;
    }

    SetRectEmpty(outRect);
    bool hasBounds = false;
    for (const QueuedMsgEffectDraw& draw : g_queuedMsgEffects) {
        RECT digitRect{};
        if (!GetDigitDrawBounds(draw, &digitRect)) {
            continue;
        }
        if (!hasBounds) {
            *outRect = digitRect;
            hasBounds = true;
        } else {
            RECT combined{};
            UnionRect(&combined, outRect, &digitRect);
            *outRect = combined;
        }
    }

    for (const QueuedMsgSpriteEffectDraw& draw : g_queuedMsgSpriteEffects) {
        RECT spriteRect{};
        if (!GetMsgSpriteDrawBounds(draw, &spriteRect)) {
            continue;
        }
        if (!hasBounds) {
            *outRect = spriteRect;
            hasBounds = true;
        } else {
            RECT combined{};
            UnionRect(&combined, outRect, &spriteRect);
            *outRect = combined;
        }
    }

    if (hasBounds) {
        InflateRect(outRect, 4, 4);
    }
    return hasBounds;
}

void ClearQueuedMsgEffects()
{
    g_queuedMsgEffects.clear();
    g_queuedMsgSpriteEffects.clear();
}

bool HasQueuedMsgEffects()
{
    return !g_queuedMsgEffects.empty() || !g_queuedMsgSpriteEffects.empty();
}

bool HasActiveMsgEffects()
{
    for (CGameObject* object : g_world.m_gameObjectList) {
        CMsgEffect* effect = dynamic_cast<CMsgEffect*>(object);
        if (!effect) {
            continue;
        }
        if (effect->m_isVisible && !effect->m_isDisappear) {
            return true;
        }
    }
    return false;
}

bool RenderAttachedActorEffectPass(CMsgEffect& effect,
    matrix* viewMatrix,
    float ownerDepthKey,
    float ownerScreenY,
    bool afterOwner,
    u32 frameId)
{
    if (!viewMatrix) {
        return false;
    }

    return RenderAttachedActorEffectInternal(effect, *viewMatrix, ownerDepthKey, ownerScreenY, afterOwner, frameId);
}
