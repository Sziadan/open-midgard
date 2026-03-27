#include "GameActor.h"
#include "MsgEffect.h"
#include "World.h"
#include "audio/Audio.h"
#include "core/ClientInfoLocale.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "DebugLog.h"
#include "render/DC.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "item/Item.h"
#include "session/Session.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>

namespace {

float g_runtimeActorCameraLongitude = 0.0f;

constexpr int kPlayerBillboardComposeWidth = 128;
constexpr int kPlayerBillboardComposeHeight = 128;
constexpr int kPlayerBillboardAnchorX = 64;
constexpr int kPlayerBillboardAnchorY = 110;
constexpr int kItemBillboardComposeWidth = 96;
constexpr int kItemBillboardComposeHeight = 96;
constexpr int kItemBillboardAnchorX = 48;
constexpr int kItemBillboardAnchorY = 72;
constexpr float kGroundItemScreenScale = 1.6f;
constexpr bool kLogActLoad = false;
constexpr int kJobWarpNpc = 0x2D;
constexpr int kJobWarpPortal = 0x80;
constexpr int kJobPreWarpPortal = 0x81;
constexpr int kJobHiddenWarpNpc = 0x8B;
constexpr const char* kLegacyMonsterSpriteRoot = "data\\sprite\\\xB8\xF3\xBD\xBA\xC5\xCD\\";
constexpr float kDefaultMotionDelay = 4.0f;
constexpr float kDefaultMotionSpeedFactor = 1.0f;
constexpr float kAttackMotionFactor = 0.0023148148f;
constexpr int kAttackStateId = kGameActorAttackStateId;
constexpr int kDeathStateId = kGameActorDeathStateId;
constexpr int kHitReactionStateId = 4;
constexpr int kDeathAction = 64;
constexpr int kDeathMotion = 4;
constexpr int kHitReactionAction = 48;
constexpr int kHitReactionMotion = 5;
constexpr u32 kHitReactionDurationMs = 200;
constexpr float kHitReactionKnockbackDist = 6.0f;
constexpr int kMonsterJobMin = 1000;
constexpr int kMercenaryJobMin = 6001;
constexpr int kMercenaryJobMax = 6047;
constexpr float kItemBillboardDepthBias = 0.00015f;
constexpr float kItemProjectNearPlane = 10.0f;
constexpr float kItemProjectSubmitNearPlane = 80.0f;
constexpr const char* kLegacyItemSpriteRoot = "data\\sprite\\\xBE\xC6\xC0\xCC\xC5\xDB\\";

#define LOG_ACT_LOAD(...) do { if constexpr (kLogActLoad) { DbgLog(__VA_ARGS__); } } while (0)

float NormalizeAngle360(float angle)
{
    while (angle < 0.0f) {
        angle += 360.0f;
    }
    while (angle >= 360.0f) {
        angle -= 360.0f;
    }
    return angle;
}

int ResolveEightDirectionFromLongitude(float longitude, bool useRoundedDirs)
{
    longitude = NormalizeAngle360(longitude);
    if (useRoundedDirs) {
        if (longitude < 22.5f || longitude > 337.5f) {
            return 0;
        }
        return static_cast<int>((longitude + 22.5f) / 45.0f) & 7;
    }

    return static_cast<int>(longitude / 45.0f) & 7;
}

bool TryResolveNonPcSpritePaths(const char* spriteRoot, const char* jobName, char* actPath, char* sprPath)
{
    if (!spriteRoot || !jobName || !*jobName || !actPath || !sprPath) {
        return false;
    }

    std::sprintf(actPath, "%s%s.act", spriteRoot, jobName);
    std::sprintf(sprPath, "%s%s.spr", spriteRoot, jobName);
    return g_resMgr.IsExist(actPath) && g_resMgr.IsExist(sprPath);
}

float ActorRotationDegreesFromDir(int dir)
{
    return NormalizeAngle360(45.0f * static_cast<float>(dir & 7));
}

bool IsMonsterLikeWaveActor(const CGameActor& actor)
{
    return actor.m_job >= kMonsterJobMin
        && (actor.m_job < kMercenaryJobMin || actor.m_job > kMercenaryJobMax);
}

bool ShouldPlayMotionWaveForActor(const CGameActor& actor)
{
    return actor.m_isPc != 0 || IsMonsterLikeWaveActor(actor);
}

u32 ResolveClientInfoActorAid(const CGameActor& actor)
{
    if (actor.m_gid != 0 && (actor.m_gid == g_session.m_gid || actor.m_gid == g_session.m_aid)) {
        return g_session.m_aid != 0 ? g_session.m_aid : actor.m_gid;
    }

    if (const CPc* pc = dynamic_cast<const CPc*>(&actor)) {
        return pc->m_gid;
    }
    return actor.m_gid;
}

bool ShouldUseGameMasterAppearance(const CGameActor& actor)
{
    return actor.m_isPc != 0 && IsGravityAid(ResolveClientInfoActorAid(actor));
}

int ResolveDisplayJob(const CGameActor& actor)
{
    return ShouldUseGameMasterAppearance(actor) ? JT_G_MASTER : actor.m_job;
}

std::string NormalizeWaveEventPath(const char* eventName);

bool QueueActorHitWave(CGameActor& actor, const char* eventName)
{
    if (!eventName || !*eventName) {
        return false;
    }

    const std::string normalized = NormalizeWaveEventPath(eventName);
    if (normalized.empty()) {
        return false;
    }

    std::snprintf(actor.m_hitWaveName, sizeof(actor.m_hitWaveName), "%s", normalized.c_str());
    actor.m_isPlayHitWave = 1;
    return true;
}

bool TryPlayQueuedWaveAtActor(const CGameActor& actor, const char* waveName)
{
    if (!waveName || !*waveName) {
        return false;
    }

    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    const std::string normalized = NormalizeWaveEventPath(waveName);
    if (normalized.empty()) {
        return false;
    }

    std::array<std::string, 3> candidates = {
        normalized,
        std::string("wav\\") + normalized,
        std::string("data\\wav\\") + normalized,
    };

    for (const std::string& candidate : candidates) {
        if (audio->PlaySound3D(candidate.c_str(), actor.m_pos, listenerPos)) {
            return true;
        }
    }

    return false;
}

bool ContainsAsciiCaseInsensitive(const char* text, const char* token)
{
    if (!text || !token || !*text || !*token) {
        return false;
    }

    std::string loweredText(text);
    std::string loweredToken(token);
    std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(loweredToken.begin(), loweredToken.end(), loweredToken.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return loweredText.find(loweredToken) != std::string::npos;
}

std::string NormalizeWaveEventPath(const char* eventName)
{
    std::string normalized = eventName ? eventName : "";
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

bool TryPlayActorMotionWave(CGameActor& actor, const char* eventName)
{
    if (!ShouldPlayMotionWaveForActor(actor) || !ContainsAsciiCaseInsensitive(eventName, ".wav")) {
        return false;
    }

    if (actor.m_isPc != 0) {
        return QueueActorHitWave(actor, eventName);
    }

    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    const std::string normalized = NormalizeWaveEventPath(eventName);
    if (normalized.empty()) {
        return false;
    }

    std::array<std::string, 3> candidates = {
        normalized,
        std::string("wav\\") + normalized,
        std::string("data\\wav\\") + normalized,
    };

    for (const std::string& candidate : candidates) {
        if (audio->PlaySound3D(candidate.c_str(), actor.m_pos, listenerPos)) {
            return true;
        }
    }

    return false;
}

void PlayMotionWaveEvents(CRenderObject* object, CActRes* actRes, int action, int oldMotion, int newMotion)
{
    if (!object || !actRes || newMotion == oldMotion) {
        return;
    }

    CGameActor* actor = dynamic_cast<CGameActor*>(object);
    if (!actor) {
        return;
    }

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        return;
    }

    auto playRange = [&](int first, int last) {
        for (int motionIndex = first; motionIndex <= last; ++motionIndex) {
            const CMotion* motion = actRes->GetMotion(action, motionIndex);
            if (!motion || motion->eventId < 0) {
                continue;
            }

            const char* eventName = actRes->GetEventName(motion->eventId);
            if (!eventName || !*eventName) {
                continue;
            }

            TryPlayActorMotionWave(*actor, eventName);
        }
    };

    if (newMotion > oldMotion) {
        playRange(oldMotion + 1, newMotion);
        return;
    }

    if (oldMotion + 1 < motionCount) {
        playRange(oldMotion + 1, motionCount - 1);
    }
    playRange(0, newMotion);
}

void ProcessRenderMotionWaveEvents(CRenderObject* object, CActRes* actRes, int action, int motion)
{
    if (!object || !actRes) {
        return;
    }

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        object->m_oldBaseAction = action;
        object->m_oldMotion = 0;
        return;
    }

    const int clampedMotion = (std::max)(0, (std::min)(motion, motionCount - 1));
    if (object->m_oldBaseAction == action && object->m_oldMotion == clampedMotion) {
        return;
    }

    int previousMotion = object->m_oldMotion;
    if (object->m_oldBaseAction != action) {
        previousMotion = 0;
    }
    previousMotion = (std::max)(0, (std::min)(previousMotion, motionCount - 1));

    PlayMotionWaveEvents(object, actRes, action, previousMotion, clampedMotion);
    object->m_oldBaseAction = action;
    object->m_oldMotion = clampedMotion;
}

void ProcessQueuedHitWave(CGameActor& actor)
{
    if (actor.m_isPlayHitWave == 0 || actor.m_hitWaveName[0] == '\0') {
        return;
    }

    actor.m_isPlayHitWave = 0;
    TryPlayQueuedWaveAtActor(actor, actor.m_hitWaveName);

    actor.m_hitWaveName[0] = '\0';
}

void ApplyQueuedHitReaction(CGameActor& actor, const WBA& hitInfo)
{
    actor.m_damageDestX = hitInfo.damageDestX;
    actor.m_damageDestZ = hitInfo.damageDestZ;

    if (hitInfo.waveName[0] != '\0') {
        TryPlayQueuedWaveAtActor(actor, hitInfo.waveName);
    } else if (const char* genericWave = g_session.GetWeaponHitWaveName(-1)) {
        TryPlayQueuedWaveAtActor(actor, genericWave);
    }

    if (actor.m_stateId == kDeathStateId) {
        return;
    }

    if (hitInfo.attackedMotionTime > 0) {
        actor.m_motionSpeed = (std::max)(kDefaultMotionSpeedFactor,
            static_cast<float>(hitInfo.attackedMotionTime) * kAttackMotionFactor);
    }
    actor.SetState(kHitReactionStateId);
}

bool IsPortalFallbackJob(int job)
{
    return job == kJobWarpNpc
        || job == kJobWarpPortal
        || job == kJobPreWarpPortal;
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

void AlphaBlendArgb(unsigned int& dst, unsigned int src)
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

void DrawFilledCircle(unsigned int* pixels, int width, int height, float centerX, float centerY, float radius, unsigned int color)
{
    if (!pixels || width <= 0 || height <= 0 || radius <= 0.0f) {
        return;
    }

    const int minX = (std::max)(0, static_cast<int>(std::floor(centerX - radius - 1.0f)));
    const int maxX = (std::min)(width - 1, static_cast<int>(std::ceil(centerX + radius + 1.0f)));
    const int minY = (std::max)(0, static_cast<int>(std::floor(centerY - radius - 1.0f)));
    const int maxY = (std::min)(height - 1, static_cast<int>(std::ceil(centerY + radius + 1.0f)));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - centerX;
            const float dy = (static_cast<float>(y) + 0.5f) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) {
                continue;
            }

            const float coverage = 1.0f - dist / radius;
            const unsigned int scaledAlpha = static_cast<unsigned int>(((color >> 24) & 0xFFu) * coverage);
            if (scaledAlpha == 0u) {
                continue;
            }

            const unsigned int scaledColor = PremultiplyArgb(
                (scaledAlpha << 24) | (color & 0x00FFFFFFu));
            AlphaBlendArgb(pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)], scaledColor);
        }
    }
}

void DrawRing(unsigned int* pixels, int width, int height, float centerX, float centerY, float radius, float thickness, unsigned int color)
{
    if (!pixels || width <= 0 || height <= 0 || radius <= 0.0f || thickness <= 0.0f) {
        return;
    }

    const float innerRadius = (std::max)(0.0f, radius - thickness * 0.5f);
    const float outerRadius = radius + thickness * 0.5f;
    const int minX = (std::max)(0, static_cast<int>(std::floor(centerX - outerRadius - 1.0f)));
    const int maxX = (std::min)(width - 1, static_cast<int>(std::ceil(centerX + outerRadius + 1.0f)));
    const int minY = (std::max)(0, static_cast<int>(std::floor(centerY - outerRadius - 1.0f)));
    const int maxY = (std::min)(height - 1, static_cast<int>(std::ceil(centerY + outerRadius + 1.0f)));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - centerX;
            const float dy = (static_cast<float>(y) + 0.5f) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < innerRadius || dist > outerRadius) {
                continue;
            }

            const float distanceToBand = std::fabs(dist - radius);
            const float coverage = 1.0f - distanceToBand / ((std::max)(0.001f, thickness * 0.5f));
            const unsigned int scaledAlpha = static_cast<unsigned int>(((color >> 24) & 0xFFu) * coverage);
            if (scaledAlpha == 0u) {
                continue;
            }

            const unsigned int scaledColor = PremultiplyArgb(
                (scaledAlpha << 24) | (color & 0x00FFFFFFu));
            AlphaBlendArgb(pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)], scaledColor);
        }
    }
}

bool DrawWarpPortalFallback(CDCBitmap& bitmap, const CPc& actor, int* outJob)
{
    if (actor.m_job == kJobHiddenWarpNpc || !IsPortalFallbackJob(actor.m_job)) {
        return false;
    }

    unsigned int* pixels = bitmap.GetImageData();
    const int width = static_cast<int>(bitmap.GetWidth());
    const int height = static_cast<int>(bitmap.GetHeight());
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    const bool preWarp = actor.m_job == kJobPreWarpPortal;
    const unsigned int ringColor = preWarp ? 0xD0FF9A52u : 0xD05AC8FFu;
    const unsigned int glowColor = preWarp ? 0x60FFB874u : 0x605AC8FFu;
    const unsigned int coreColor = preWarp ? 0x409C5A20u : 0x403088D0u;
    const float centerX = static_cast<float>(width) * 0.5f;
    const float centerY = static_cast<float>(height) * 0.76f;

    DrawFilledCircle(pixels, width, height, centerX, centerY - 2.0f, 20.0f, coreColor);
    DrawRing(pixels, width, height, centerX, centerY, 28.0f, 5.0f, glowColor);
    DrawRing(pixels, width, height, centerX, centerY, 24.0f, 3.0f, ringColor);
    DrawRing(pixels, width, height, centerX, centerY, 16.0f, 2.0f, 0x90FFFFFFu);

    for (int band = 0; band < 4; ++band) {
        const float offset = -12.0f + 8.0f * static_cast<float>(band);
        DrawFilledCircle(pixels, width, height, centerX + offset * 0.35f, centerY - 18.0f - std::fabs(offset) * 0.4f, 5.5f, glowColor);
        DrawFilledCircle(pixels, width, height, centerX + offset * 0.25f, centerY - 32.0f - std::fabs(offset) * 0.8f, 3.5f, 0x50FFFFFFu);
    }

    if (outJob) {
        *outJob = actor.m_job;
    }
    return true;
}

int ResolveDirFromRotationDegrees(float rotationDegrees)
{
    return static_cast<int>((NormalizeAngle360(rotationDegrees) + 22.5f) / 45.0f) & 7;
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

int ResolveDirFromStepDelta(int dx, int dy)
{
    if (dx == 0 && dy < 0) {
        return 0;
    }
    if (dx > 0 && dy < 0) {
        return 1;
    }
    if (dx > 0 && dy == 0) {
        return 2;
    }
    if (dx > 0 && dy > 0) {
        return 3;
    }
    if (dx == 0 && dy > 0) {
        return 4;
    }
    if (dx < 0 && dy > 0) {
        return 5;
    }
    if (dx < 0 && dy == 0) {
        return 6;
    }
    if (dx < 0 && dy < 0) {
        return 7;
    }
    return g_session.m_playerDir & 7;
}

int ClampDamageNumber(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 999999) {
        return 999999;
    }
    return value;
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

void SpawnDamageNumberEffect(CGameActor& actor, CGameActor* sourceActor, int value, u32 colorArgb, int kind)
{
    CMsgEffect* effect = new CMsgEffect();
    if (!effect) {
        return;
    }

    effect->m_colorArgb = colorArgb;
    effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), kind, 0);
    effect->SendMsg(nullptr,
        64,
        static_cast<int>(reinterpret_cast<intptr_t>(&actor.m_pos)),
        value,
        0);
    effect->SendMsg(&actor, 50, 0, 0, 0);

    if (kind == 22) {
        effect->m_zoom = 0.5f;
        effect->m_orgZoom = 0.5f;
    }

    if (sourceActor) {
        const float dx = actor.m_pos.x - sourceActor->m_pos.x;
        const float dz = actor.m_pos.z - sourceActor->m_pos.z;
        const float lengthSq = dx * dx + dz * dz;
        if (lengthSq > 0.0001f) {
            const float invLength = 0.8f / std::sqrt(lengthSq);
            effect->m_destPos.x = dx * invLength;
            effect->m_destPos.z = dz * invLength;
        }
    }

    g_world.m_gameObjectList.push_back(effect);
    actor.m_msgEffectList.push_back(effect);
}

void MakeDamageNumber(CGameActor& actor, CGameActor* sourceActor, int value, u32 colorArgb, int kind)
{
    const int clampedValue = ClampDamageNumber(value);
    if (kind == 22) {
        actor.DeleteTotalNumber(kind);
    }

    SpawnDamageNumberEffect(actor, sourceActor, clampedValue, colorArgb, kind);
}

bool FindActivePathSegment(const CPathInfo& path, u32 now, size_t* outStartIndex)
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

bool InterpolatePathPosition(const CGameActor& actor, u32 now, vector3d* outPos)
{
    if (!outPos || actor.m_path.m_cells.empty()) {
        return false;
    }

    const CWorld* world = &g_world;
    if (actor.m_path.m_cells.size() == 1) {
        const PathCell& cell = actor.m_path.m_cells.front();
        outPos->x = TileToWorldCoordX(world, cell.x);
        outPos->z = TileToWorldCoordZ(world, cell.y);
        outPos->y = world->m_attr ? world->m_attr->GetHeight(outPos->x, outPos->z) : 0.0f;
        return true;
    }

    if (now >= actor.m_path.m_cells.back().arrivalTime) {
        const PathCell& cell = actor.m_path.m_cells.back();
        outPos->x = TileToWorldCoordX(world, cell.x);
        outPos->z = TileToWorldCoordZ(world, cell.y);
        outPos->y = world->m_attr ? world->m_attr->GetHeight(outPos->x, outPos->z) : 0.0f;
        return true;
    }

    size_t startIndex = 0;
    if (!FindActivePathSegment(actor.m_path, now, &startIndex)) {
        return false;
    }

    const PathCell& startCell = actor.m_path.m_cells[startIndex];
    const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
    const u32 startTime = startCell.arrivalTime;
    const u32 endTime = endCell.arrivalTime;
    const float duration = static_cast<float>((std::max)(1u, endTime - startTime));
    const float ratio = static_cast<float>(now - startTime) / duration;
    const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));

    const float startX = startIndex == 0 ? actor.m_moveStartPos.x : TileToWorldCoordX(world, startCell.x);
    const float startZ = startIndex == 0 ? actor.m_moveStartPos.z : TileToWorldCoordZ(world, startCell.y);
    const float endX = TileToWorldCoordX(world, endCell.x);
    const float endZ = TileToWorldCoordZ(world, endCell.y);

    outPos->x = startX + (endX - startX) * clamped;
    outPos->z = startZ + (endZ - startZ) * clamped;
    outPos->y = world->m_attr ? world->m_attr->GetHeight(outPos->x, outPos->z) : 0.0f;
    return true;
}

int ResolvePcFacingDir(const CPc* actor)
{
    if (!actor) {
        return g_session.m_playerDir & 7;
    }

    if (actor->m_isMoving) {
        return ResolveDirFromRotationDegrees(actor->m_roty);
    }

    if (std::isfinite(actor->m_roty)) {
        return ResolveDirFromRotationDegrees(actor->m_roty);
    }

    return g_session.m_playerDir & 7;
}

struct SharedNonPcBillboardKey {
    int job = 0;
    int action = 0;
    int motion = 0;

    bool operator<(const SharedNonPcBillboardKey& other) const
    {
        if (job != other.job) {
            return job < other.job;
        }
        if (action != other.action) {
            return action < other.action;
        }
        return motion < other.motion;
    }
};

struct SharedNonPcBillboardValue {
    CTexture* texture = nullptr;
    tagRECT opaqueBounds{};
    int width = 0;
    int height = 0;
    int anchorX = kPlayerBillboardAnchorX;
    int anchorY = kPlayerBillboardAnchorY;
};

std::map<SharedNonPcBillboardKey, SharedNonPcBillboardValue>& GetSharedNonPcBillboardCache()
{
    static std::map<SharedNonPcBillboardKey, SharedNonPcBillboardValue> cache;
    return cache;
}

struct SharedItemBillboardKey {
    u32 itemId = 0;
    u8 identified = 0;
    int motion = 0;

    bool operator<(const SharedItemBillboardKey& other) const
    {
        if (itemId != other.itemId) {
            return itemId < other.itemId;
        }
        if (identified != other.identified) {
            return identified < other.identified;
        }
        return motion < other.motion;
    }
};

struct SharedItemBillboardValue {
    CTexture* texture = nullptr;
    tagRECT opaqueBounds{};
    int width = 0;
    int height = 0;
    int anchorX = kItemBillboardAnchorX;
    int anchorY = kItemBillboardAnchorY;
};

std::map<SharedItemBillboardKey, SharedItemBillboardValue>& GetSharedItemBillboardCache()
{
    static std::map<SharedItemBillboardKey, SharedItemBillboardValue> cache;
    return cache;
}

void ReleaseActorBillboardTexture(CPc& actor)
{
    if (actor.m_billboardTextureOwned && actor.m_billboardTexture) {
        delete actor.m_billboardTexture;
    }
    actor.m_billboardTexture = nullptr;
    actor.m_billboardTextureOwned = 0;
}

} // namespace

void SetRuntimeActorCameraLongitude(float longitude)
{
    g_runtimeActorCameraLongitude = longitude;
}

int CGameObject::Get8Dir(float rot)
{
    return ResolveEightDirectionFromLongitude(-g_runtimeActorCameraLongitude - rot, false);
}

int CGameActor::Get8Dir(float rot)
{
    const bool useRoundedDirs = (m_isPc != 0 && (m_baseAction == 0 || m_baseAction == 8 || m_baseAction == 16))
        || (m_job > 45 && m_job < 1000 && m_baseAction == 0);
    return ResolveEightDirectionFromLongitude(-g_runtimeActorCameraLongitude - rot, useRoundedDirs);
}

namespace {

int ResolvePcMotionIndex(const CPc* actor, int action, const std::string& bodyActName)
{
    if (!actor || !actor->m_isMoving) {
        return 0;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return 0;
    }

    const int motionCount = bodyActRes->GetMotionCount(action);
    if (motionCount <= 1) {
        return 0;
    }

    const float motionDelay = (std::max)(0.0001f, bodyActRes->GetDelay(action));
    const int motion = static_cast<int>(actor->m_dist * 1.48f / motionDelay);
    if (motion < 0) {
        return 0;
    }
    return motion % motionCount;
}

int ResolveAvailableActionIndex(CActRes* actRes, int action)
{
    if (!actRes) {
        return -1;
    }

    for (int candidate = action; candidate >= 0; candidate -= 8) {
        if (actRes->GetMotionCount(candidate) > 0) {
            return candidate;
        }
    }

    return -1;
}

int ResolveTimedMotionIndex(const CGameActor& actor, CActRes* actRes, int action)
{
    if (!actRes) {
        return 0;
    }

    const int resolvedAction = ResolveAvailableActionIndex(actRes, action);
    if (resolvedAction < 0) {
        return 0;
    }

    const int motionCount = actRes->GetMotionCount(resolvedAction);
    if (motionCount <= 1) {
        return 0;
    }

    const u32 startTick = actor.m_stateStartTick != 0 ? actor.m_stateStartTick : timeGetTime();
    const u32 elapsedMs = timeGetTime() - startTick;
    const float stateTicks = static_cast<float>(elapsedMs) * 0.041666668f;
    const float actionDelay = actRes->GetDelay(resolvedAction);
    const bool useScaledAttackTiming = actor.m_stateId == kAttackStateId
        && actor.m_attackMotion >= 0.0f
        && actor.m_motionSpeed > 0.0f;
    const bool useDeathTiming = actor.m_stateId == kDeathStateId;
    const float motionDelay = useScaledAttackTiming
        ? actor.m_motionSpeed
        : (useDeathTiming
            ? (std::max)(kDefaultMotionDelay, actionDelay)
            : (std::max)(0.0001f, actionDelay));
    const int motion = static_cast<int>(stateTicks / motionDelay);
    if (motion < 0) {
        return 0;
    }

    if (useDeathTiming) {
        return (std::min)(motion, motionCount - 1);
    }

    return motion % motionCount;
}

bool IsTransientActionActive(const CGameActor& actor, CActRes* actRes, int action)
{
    if (!actRes || action <= 0) {
        return false;
    }

    const bool allowWhileMoving = actor.m_stateId == kAttackStateId || actor.m_stateId == kDeathStateId;
    if (actor.m_isMoving && !allowWhileMoving) {
        return false;
    }

    const int resolvedAction = ResolveAvailableActionIndex(actRes, action);
    if (resolvedAction < 0) {
        return false;
    }

    const int motionCount = (std::max)(1, actRes->GetMotionCount(resolvedAction));
    const float actionDelay = actRes->GetDelay(resolvedAction);
    const bool useScaledAttackTiming = actor.m_stateId == kAttackStateId
        && actor.m_attackMotion >= 0.0f
        && actor.m_motionSpeed > 0.0f;
    const bool useDeathTiming = actor.m_stateId == kDeathStateId;
    const float motionDelay = useScaledAttackTiming
        ? actor.m_motionSpeed
        : (useDeathTiming
            ? (std::max)(kDefaultMotionDelay, actionDelay)
            : (std::max)(0.0001f, actionDelay));
    const float totalDurationMs = static_cast<float>(motionCount) * motionDelay * 24.0f;
    const u32 startTick = actor.m_stateStartTick != 0 ? actor.m_stateStartTick : timeGetTime();
    const u32 elapsedMs = timeGetTime() - startTick;

    if (useDeathTiming) {
        return actor.m_vanishTime == 0 || timeGetTime() < actor.m_vanishTime;
    }

    return static_cast<float>(elapsedMs) <= totalDurationMs + 50.0f;
}

int ResolveSpriteMotionIndex(const CGameActor& actor, CActRes* actRes, int action)
{
    if (!actRes) {
        return 0;
    }

    int resolvedAction = action;
    int motionCount = actRes->GetMotionCount(resolvedAction);
    if (motionCount <= 1 && resolvedAction >= 8) {
        resolvedAction -= 8;
        motionCount = actRes->GetMotionCount(resolvedAction);
    }
    if (motionCount <= 1) {
        return 0;
    }

    if (!actor.m_isMoving) {
        return ResolveTimedMotionIndex(actor, actRes, resolvedAction);
    }

    const float motionDelay = (std::max)(0.0001f, actRes->GetDelay(resolvedAction));
    const int motion = static_cast<int>(actor.m_dist * 1.48f / motionDelay);
    if (motion < 0) {
        return 0;
    }
    return motion % motionCount;
}

int ResolvePcBodyActionFromView(float cameraLongitude, float actorRotationDegrees)
{
    const float longitude = NormalizeAngle360(-cameraLongitude - actorRotationDegrees);
    if (longitude < 22.5f || longitude > 337.5f) {
        return 0;
    }
    return static_cast<int>((longitude + 22.5f) / 45.0f) & 7;
}

int ResolveActionDirFromView(float cameraLongitude, float actorRotationDegrees, bool useRoundedPlayerDirs)
{
    const float longitude = NormalizeAngle360(-cameraLongitude - actorRotationDegrees);
    if (useRoundedPlayerDirs) {
        if (longitude < 22.5f || longitude > 337.5f) {
            return 0;
        }
        return static_cast<int>((longitude + 22.5f) / 45.0f) & 7;
    }

    return static_cast<int>(longitude / 45.0f) & 7;
}

int ResolveDirectionalActionFromView(CActRes* actRes,
    const CGameActor& actor,
    float cameraLongitude,
    bool isPlayerStyleActor)
{
    const bool useRoundedPlayerDirs = isPlayerStyleActor
        && (actor.m_baseAction == 0 || actor.m_baseAction == 8 || actor.m_baseAction == 16);
    const int directionalAction = actor.m_baseAction
        + ResolveActionDirFromView(cameraLongitude, actor.m_roty, useRoundedPlayerDirs);

    const int resolvedDirectionalAction = ResolveAvailableActionIndex(actRes, directionalAction);
    if (resolvedDirectionalAction >= 0) {
        return resolvedDirectionalAction;
    }

    const int resolvedBaseAction = ResolveAvailableActionIndex(actRes, actor.m_baseAction);
    if (resolvedBaseAction >= 0) {
        return resolvedBaseAction;
    }

    return directionalAction;
}

int ResolveTransientBillboardAction(CActRes* actRes,
    const CGameActor& actor,
    float cameraLongitude,
    bool isPlayerStyleActor)
{
    if (!actRes) {
        return actor.m_baseAction;
    }

    return ResolveDirectionalActionFromView(actRes, actor, cameraLongitude, isPlayerStyleActor);
}

void ResolveTransientSpriteState(const CGameActor& actor,
    CActRes* actRes,
    float cameraLongitude,
    bool isPlayerStyleActor,
    int* outAction,
    int* outMotion)
{
    if (!outAction || !outMotion) {
        return;
    }

    if (!actRes) {
        *outAction = actor.m_baseAction;
        *outMotion = 0;
        return;
    }

    const int resolvedAction = ResolveTransientBillboardAction(actRes, actor, cameraLongitude, isPlayerStyleActor);
    *outAction = resolvedAction;

    const int motionCount = actRes->GetMotionCount(resolvedAction);
    if (motionCount <= 0) {
        *outMotion = 0;
        return;
    }

    if (actor.m_isMotionFinished || actor.m_isMotionFreezed) {
        *outMotion = (std::min)(actor.m_curMotion, motionCount - 1);
        return;
    }

    const u32 startTick = actor.m_stateStartTick != 0 ? actor.m_stateStartTick : timeGetTime();
    const float stateTicks = static_cast<float>(timeGetTime() - startTick) * 0.041666668f;
    const float motionSpeed = (std::max)(kDefaultMotionSpeedFactor, actor.m_motionSpeed);

    if (actor.m_motionType == 1) {
        const float motionProgress = stateTicks / motionSpeed;
        int resolvedMotion = static_cast<int>(motionProgress) % motionCount;

        const float loopProgress = motionProgress / static_cast<float>(motionCount);
        if (loopProgress >= 1.0f) {
            resolvedMotion = motionCount - 1;
        }
        *outMotion = resolvedMotion;
        return;
    }

    *outMotion = static_cast<int>(stateTicks / motionSpeed) % motionCount;
}

void LogDeathBillboardSelectionOnce(const CGameActor& actor,
    CActRes* actRes,
    int resolvedAction,
    int resolvedMotion)
{
    if (actor.m_stateId != kDeathStateId || !actRes || actor.m_gid == 0) {
        return;
    }

    static std::map<u32, u32> loggedDeathStateTicks;
    const auto it = loggedDeathStateTicks.find(actor.m_gid);
    if (it != loggedDeathStateTicks.end() && it->second == actor.m_stateStartTick) {
        return;
    }

    loggedDeathStateTicks[actor.m_gid] = actor.m_stateStartTick;
    DbgLog("[Actor] death billboard gid=%u job=%d name='%s' baseAction=%d curAction=%d resolvedAction=%d resolvedMotion=%d motionCount=%d vanish=%u roty=%.2f dir=%d\n",
        actor.m_gid,
        actor.m_job,
        g_session.GetJobName(actor.m_job),
        actor.m_baseAction,
        actor.m_curAction,
        resolvedAction,
        resolvedMotion,
        actRes->GetMotionCount(resolvedAction),
        actor.m_vanishTime,
        actor.m_roty,
        const_cast<CGameActor&>(actor).Get8Dir(actor.m_roty));

    if (actor.m_job == 1002) {
        for (int action = 64; action <= 71; ++action) {
            const int motionCount = actRes->GetMotionCount(action);
            int firstSpr = -1;
            int secondSpr = -1;
            if (motionCount > 0) {
                if (const CMotion* motion0 = actRes->GetMotion(action, 0)) {
                    if (!motion0->sprClips.empty()) {
                        firstSpr = motion0->sprClips[0].sprIndex;
                    }
                }
            }
            if (motionCount > 1) {
                if (const CMotion* motion1 = actRes->GetMotion(action, 1)) {
                    if (!motion1->sprClips.empty()) {
                        secondSpr = motion1->sprClips[0].sprIndex;
                    }
                }
            }
            DbgLog("[Actor] death family job=%d action=%d motions=%d spr0=%d spr1=%d\n",
                actor.m_job,
                action,
                motionCount,
                firstSpr,
                secondSpr);
        }
    }
}

void LogDeathMotionProgress(const CGameActor& actor)
{
    if (actor.m_stateId != kDeathStateId || actor.m_gid == 0) {
        return;
    }

    struct DeathProgressState {
        u32 stateTick = 0;
        int lastAction = -1;
        int lastMotion = -1;
    };

    static std::map<u32, DeathProgressState> progressByGid;
    DeathProgressState& state = progressByGid[actor.m_gid];
    if (state.stateTick != actor.m_stateStartTick) {
        state.stateTick = actor.m_stateStartTick;
        state.lastAction = -1;
        state.lastMotion = -1;
    }

    if (state.lastAction == actor.m_curAction && state.lastMotion == actor.m_curMotion) {
        return;
    }

    state.lastAction = actor.m_curAction;
    state.lastMotion = actor.m_curMotion;
    DbgLog("[Actor] death progress gid=%u action=%d motion=%d roty=%.2f vanish=%u elapsed=%u\n",
        actor.m_gid,
        actor.m_curAction,
        actor.m_curMotion,
        actor.m_roty,
        actor.m_vanishTime,
        actor.m_stateStartTick != 0 ? (timeGetTime() - actor.m_stateStartTick) : 0);
}

int ResolveHeadMotionFromBodyAction(int bodyAction)
{
    (void)bodyAction;
    return 0;
}

bool BuildPaletteOverride(const std::string& paletteName, std::array<unsigned int, 256>& outPalette)
{
    if (paletteName.empty()) {
        return false;
    }

    CPaletteRes* palRes = g_resMgr.GetAs<CPaletteRes>(paletteName.c_str());
    if (!palRes) {
        return false;
    }

    g_3dDevice.ConvertPalette(outPalette.data(), palRes->m_pal, 256);
    return true;
}

bool ApplyAttachPointDelta(const CMotion* baseMotion, const CMotion* attachedMotion, POINT* inOutPoint)
{
    if (!baseMotion || !attachedMotion || !inOutPoint || baseMotion->attachInfo.empty() || attachedMotion->attachInfo.empty()) {
        return false;
    }

    const CAttachPointInfo& attached = attachedMotion->attachInfo.front();
    const CAttachPointInfo& base = baseMotion->attachInfo.front();
    if (attached.attr != base.attr) {
        return false;
    }

    inOutPoint->x += base.x - attached.x;
    inOutPoint->y += base.y - attached.y;
    return true;
}

POINT GetPlayerLayerPoint(int layerPriority,
    int resolvedLayer,
    CImfRes* imfRes,
    const CMotion* motion,
    const std::string& bodyActName,
    const std::string& headActName,
    int curAction,
    int bodyMotionIndex,
    int curMotion,
    int headMotionIndex)
{
    POINT point = imfRes->GetPoint(resolvedLayer, curAction, curMotion);
    if (layerPriority != 1 || !motion || motion->attachInfo.empty()) {
        if (layerPriority < 2 || !motion || motion->attachInfo.empty()) {
            return point;
        }
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return point;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
    if (!bodyMotion || bodyMotion->attachInfo.empty()) {
        return point;
    }

    if (layerPriority == 1) {
        ApplyAttachPointDelta(bodyMotion, motion, &point);
        return point;
    }

    CActRes* headActRes = g_resMgr.GetAs<CActRes>(headActName.c_str());
    if (!headActRes) {
        return point;
    }

    const CMotion* headMotion = headActRes->GetMotion(curAction, headMotionIndex);
    if (!headMotion) {
        return point;
    }

    POINT headOffset{};
    ApplyAttachPointDelta(bodyMotion, headMotion, &headOffset);
    point.x += headOffset.x;
    point.y += headOffset.y;
    ApplyAttachPointDelta(headMotion, motion, &point);
    return point;
}

bool DrawAttachedAccessoryMotion(HDC hdc,
    int drawX,
    int drawY,
    int curAction,
    int bodyMotionIndex,
    int headMotionIndex,
    const std::string& bodyActName,
    const std::string& headActName,
    const std::string& accessoryActName,
    const std::string& accessorySprName)
{
    if (accessoryActName.empty() || accessorySprName.empty()) {
        return false;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    CActRes* headActRes = g_resMgr.GetAs<CActRes>(headActName.c_str());
    CActRes* accessoryActRes = g_resMgr.GetAs<CActRes>(accessoryActName.c_str());
    CSprRes* accessorySprRes = g_resMgr.GetAs<CSprRes>(accessorySprName.c_str());
    if (!bodyActRes || !headActRes || !accessoryActRes || !accessorySprRes) {
        return false;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
    const CMotion* headMotion = headActRes->GetMotion(curAction, headMotionIndex);
    const CMotion* accessoryMotion = accessoryActRes->GetMotion(curAction, headMotionIndex);
    if (!accessoryMotion) {
        accessoryMotion = accessoryActRes->GetMotion(curAction, 0);
    }
    if (!bodyMotion || !headMotion || !accessoryMotion) {
        return false;
    }

    POINT point{};
    ApplyAttachPointDelta(bodyMotion, headMotion, &point);
    ApplyAttachPointDelta(headMotion, accessoryMotion, &point);
    return DrawActMotionToHdc(hdc, drawX + point.x, drawY + point.y, accessorySprRes, accessoryMotion, accessorySprRes->m_pal);
}

bool DrawPlayerLayer(HDC hdc,
    int drawX,
    int drawY,
    int layerIndex,
    int curAction,
    int curMotion,
    const std::string& actName,
    const std::string& sprName,
    const std::string& imfName,
    const std::string& bodyActName,
    const std::string& headActName,
    int bodyMotionIndex,
    int headMotionIndex,
    const std::string& paletteName)
{
    CActRes* actRes = g_resMgr.GetAs<CActRes>(actName.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprName.c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        static std::map<std::string, bool> loggedMissingResources;
        const std::string key = actName + "|" + sprName + "|" + imfName;
        if (loggedMissingResources.insert(std::make_pair(key, true)).second) {
            DbgLog("[Actor] player layer resources missing act='%s' actExists=%d actRes=%p spr='%s' sprExists=%d sprRes=%p imf='%s' imfExists=%d imfRes=%p\n",
                actName.c_str(),
                g_resMgr.IsExist(actName.c_str()) ? 1 : 0,
                actRes,
                sprName.c_str(),
                g_resMgr.IsExist(sprName.c_str()) ? 1 : 0,
                sprRes,
                imfName.c_str(),
                g_resMgr.IsExist(imfName.c_str()) ? 1 : 0,
                imfRes);
        }
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, curAction, curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(curAction, curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        static std::map<std::string, bool> loggedMotionFailures;
        char key[512] = {};
        std::sprintf(key, "%s|layer=%d|action=%d|motion=%d|resolved=%d", actName.c_str(), layerIndex, curAction, curMotion, resolvedLayer);
        if (loggedMotionFailures.insert(std::make_pair(key, true)).second) {
            DbgLog("[Actor] player layer motion missing act='%s' layer=%d action=%d motion=%d resolvedLayer=%d clipCount=%d\n",
                actName.c_str(),
                layerIndex,
                curAction,
                curMotion,
                resolvedLayer,
                motion ? static_cast<int>(motion->sprClips.size()) : -1);
        }
        return false;
    }

    const POINT point = GetPlayerLayerPoint(layerIndex,
        resolvedLayer,
        imfRes,
        motion,
        bodyActName,
        headActName,
        curAction,
        bodyMotionIndex,
        curMotion,
        headMotionIndex);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (BuildPaletteOverride(paletteName, paletteOverride)) {
        palette = paletteOverride.data();
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    return DrawActMotionToHdc(hdc, drawX + point.x, drawY + point.y, sprRes, &singleLayerMotion, palette);
}

bool DrawPcBillboard(CDCBitmap& bitmap,
    const CPc& actor,
    int drawX,
    int drawY,
    int bodyAction,
    int headMotion,
    int* outJob,
    int* outHead,
    int* outSex,
    int* outBodyPalette,
    int* outHeadPalette)
{
    char bodyAct[260] = {};
    char bodySpr[260] = {};
    char headAct[260] = {};
    char headSpr[260] = {};
    char accessoryBottomAct[260] = {};
    char accessoryBottomSpr[260] = {};
    char accessoryMidAct[260] = {};
    char accessoryMidSpr[260] = {};
    char accessoryTopAct[260] = {};
    char accessoryTopSpr[260] = {};
    char imfName[260] = {};
    char bodyPalette[260] = {};
    char headPalette[260] = {};

    const int displayJob = ResolveDisplayJob(actor);
    const int sex = actor.m_sex != 0 ? 1 : 0;
    int head = actor.m_head;
    const int curAction = bodyAction;
    const int curMotion = headMotion;

    const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyAct);
    const std::string bodySprName = g_session.GetJobSprName(displayJob, sex, bodySpr);
    const std::string headActName = g_session.GetHeadActName(displayJob, &head, sex, headAct);
    const std::string headSprName = g_session.GetHeadSprName(displayJob, &head, sex, headSpr);
    const std::string accessoryBottomActName = g_session.GetAccessoryActName(displayJob, &head, sex, actor.m_accessory, accessoryBottomAct);
    const std::string accessoryBottomSprName = g_session.GetAccessorySprName(displayJob, &head, sex, actor.m_accessory, accessoryBottomSpr);
    const std::string accessoryMidActName = g_session.GetAccessoryActName(displayJob, &head, sex, actor.m_accessory3, accessoryMidAct);
    const std::string accessoryMidSprName = g_session.GetAccessorySprName(displayJob, &head, sex, actor.m_accessory3, accessoryMidSpr);
    const std::string accessoryTopActName = g_session.GetAccessoryActName(displayJob, &head, sex, actor.m_accessory2, accessoryTopAct);
    const std::string accessoryTopSprName = g_session.GetAccessorySprName(displayJob, &head, sex, actor.m_accessory2, accessoryTopSpr);
    const std::string imfPath = g_session.GetImfName(displayJob, head, sex, imfName);
    const std::string bodyPaletteName = actor.m_bodyPalette > 0
        ? g_session.GetBodyPaletteName(displayJob, sex, actor.m_bodyPalette, bodyPalette)
        : std::string();
    const std::string headPaletteName = actor.m_headPalette > 0
        ? g_session.GetHeadPaletteName(head, displayJob, sex, actor.m_headPalette, headPalette)
        : std::string();

    HDC hdc = nullptr;
    if (!bitmap.GetDC(&hdc) || !hdc) {
        return false;
    }

    const bool bodyOk = DrawPlayerLayer(hdc, drawX, drawY, 0, curAction, curMotion, bodyActName, bodySprName, imfPath, bodyActName, headActName, curMotion, headMotion, bodyPaletteName);
    const bool headOk = DrawPlayerLayer(hdc, drawX, drawY, 1, curAction, headMotion, headActName, headSprName, imfPath, bodyActName, headActName, curMotion, headMotion, headPaletteName);
    const bool accessoryBottomOk = !accessoryBottomActName.empty() && !accessoryBottomSprName.empty()
        ? DrawAttachedAccessoryMotion(hdc, drawX, drawY, curAction, curMotion, headMotion, bodyActName, headActName, accessoryBottomActName, accessoryBottomSprName)
        : false;
    const bool accessoryMidOk = !accessoryMidActName.empty() && !accessoryMidSprName.empty()
        ? DrawAttachedAccessoryMotion(hdc, drawX, drawY, curAction, curMotion, headMotion, bodyActName, headActName, accessoryMidActName, accessoryMidSprName)
        : false;
    const bool accessoryTopOk = !accessoryTopActName.empty() && !accessoryTopSprName.empty()
        ? DrawAttachedAccessoryMotion(hdc, drawX, drawY, curAction, curMotion, headMotion, bodyActName, headActName, accessoryTopActName, accessoryTopSprName)
        : false;
    bitmap.ReleaseDC(hdc);

    if (!bodyOk && !headOk && !accessoryBottomOk && !accessoryMidOk && !accessoryTopOk) {
        static std::map<int, bool> loggedBillboardFailures;
        if (loggedBillboardFailures.insert(std::make_pair(displayJob, true)).second) {
            DbgLog("[Actor] player billboard draw failed job=%d bodyAct='%s' bodySpr='%s' headAct='%s' headSpr='%s' imf='%s' bodyPal='%s' headPal='%s' action=%d motion=%d head=%d sex=%d\n",
                displayJob,
                bodyActName.c_str(),
                bodySprName.c_str(),
                headActName.c_str(),
                headSprName.c_str(),
                imfPath.c_str(),
                bodyPaletteName.c_str(),
                headPaletteName.c_str(),
                curAction,
                curMotion,
                head,
                sex);
        }
    }

    if (outJob) {
        *outJob = displayJob;
    }
    if (outHead) {
        *outHead = head;
    }
    if (outSex) {
        *outSex = sex;
    }
    if (outBodyPalette) {
        *outBodyPalette = actor.m_bodyPalette;
    }
    if (outHeadPalette) {
        *outHeadPalette = actor.m_headPalette;
    }

    return bodyOk || headOk || accessoryBottomOk || accessoryMidOk || accessoryTopOk;
}

bool ResolveNonPcSpritePaths(int job, char* actPath, char* sprPath)
{
    const char* const jobName = g_session.GetJobName(job);
    if (!jobName || !*jobName || !actPath || !sprPath) {
        return false;
    }

    if (job >= 1000) {
        if (job >= 6001 && job <= 6047) {
            const char* const spriteRoot = (job >= 6017 && job <= 6046)
                ? "data\\sprite\\mercenary\\"
                : "data\\sprite\\homun\\";
            std::sprintf(actPath, "%s%s.act", spriteRoot, jobName);
            std::sprintf(sprPath, "%s%s.spr", spriteRoot, jobName);
            return true;
        }

        if (TryResolveNonPcSpritePaths("data\\sprite\\monster\\", jobName, actPath, sprPath)) {
            return true;
        }
        if (TryResolveNonPcSpritePaths(kLegacyMonsterSpriteRoot, jobName, actPath, sprPath)) {
            return true;
        }
        if (TryResolveNonPcSpritePaths("data\\sprite\\", jobName, actPath, sprPath)) {
            return true;
        }

        std::sprintf(actPath, "%s%s.act", "data\\sprite\\monster\\", jobName);
        std::sprintf(sprPath, "%s%s.spr", "data\\sprite\\monster\\", jobName);
        return true;
    }

    std::sprintf(actPath, "%s%s.act", "data\\sprite\\NPC\\", jobName);
    std::sprintf(sprPath, "%s%s.spr", "data\\sprite\\NPC\\", jobName);
    return true;
}

void LogNonPcResourceResolvedOnce(int job, const char* jobName, const char* actPath, const char* sprPath)
{
    static std::map<int, bool> loggedJobs;
    if (!loggedJobs.insert(std::make_pair(job, true)).second) {
        return;
    }

    DbgLog("[Actor] nonpc resources resolved job=%d name='%s' act='%s' spr='%s'\n",
        job,
        jobName ? jobName : "",
        actPath ? actPath : "",
        sprPath ? sprPath : "");
}

void LogNonPcResourceMissingOnce(int job, const char* jobName, const char* actPath, const char* sprPath, const void* actRes, const void* sprRes)
{
    static std::map<int, bool> loggedJobs;
    if (!loggedJobs.insert(std::make_pair(job, true)).second) {
        return;
    }

    DbgLog("[Actor] nonpc resources missing job=%d name='%s' act='%s' actRes=%p spr='%s' sprRes=%p\n",
        job,
        jobName ? jobName : "",
        actPath ? actPath : "",
        actRes,
        sprPath ? sprPath : "",
        sprRes);
}

void LogNonPcBillboardFailureOnce(int job, const char* jobName, int bodyAction, int motion)
{
    static std::map<int, bool> loggedJobs;
    if (!loggedJobs.insert(std::make_pair(job, true)).second) {
        return;
    }

    DbgLog("[Actor] nonpc billboard draw failed job=%d name='%s' bodyAction=%d motion=%d\n",
        job,
        jobName ? jobName : "",
        bodyAction,
        motion);
}

bool ResolveCachedNonPcResourcesForActor(CPc& actor, CActRes** outActRes, CSprRes** outSprRes);
CActRes* ResolveRuntimeActorActRes(CRenderObject* object);

bool DrawNonPcBillboard(CDCBitmap& bitmap,
    const CPc& actor,
    int drawX,
    int drawY,
    int bodyAction,
    int motion,
    int* outJob)
{
    CActRes* actRes = nullptr;
    CSprRes* sprRes = nullptr;
    if (!ResolveCachedNonPcResourcesForActor(const_cast<CPc&>(actor), &actRes, &sprRes)) {
        return DrawWarpPortalFallback(bitmap, actor, outJob);
    }

    if (!actRes || !sprRes) {
        return false;
    }

    int resolvedAction = bodyAction;
    if (actRes->GetMotionCount(resolvedAction) <= 0 && resolvedAction >= 8) {
        resolvedAction -= 8;
    }
    if (actRes->GetMotionCount(resolvedAction) <= 0) {
        resolvedAction = 0;
    }
    if (actRes->GetMotionCount(resolvedAction) <= 0) {
        return false;
    }

    const CMotion* spriteMotion = actRes->GetMotion(resolvedAction, motion);
    if (!spriteMotion) {
        spriteMotion = actRes->GetMotion(resolvedAction, 0);
    }
    if (!spriteMotion) {
        return false;
    }

    HDC hdc = nullptr;
    if (!bitmap.GetDC(&hdc) || !hdc) {
        return false;
    }

    const bool drawOk = DrawActMotionToHdc(hdc, drawX, drawY, sprRes, spriteMotion, sprRes->m_pal);
    bitmap.ReleaseDC(hdc);

    if (outJob) {
        *outJob = ResolveDisplayJob(actor);
    }
    return drawOk;
}

bool FindOpaqueBounds(const unsigned int* pixels, int width, int height, tagRECT* outBounds)
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
            if ((pixel >> 24) == 0) {
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

template <typename T>
bool ReadValue(const unsigned char*& cursor, const unsigned char* end, T& out)
{
    if (cursor + sizeof(T) > end) {
        return false;
    }
    std::memcpy(&out, cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}

bool ReadBytes(const unsigned char*& cursor, const unsigned char* end, void* out, size_t size)
{
    if (cursor + size > end) {
        return false;
    }
    std::memcpy(out, cursor, size);
    cursor += size;
    return true;
}

struct ActHeader {
    unsigned short id;
    unsigned short ver;
    unsigned short count;
    unsigned char reserved[10];
};

struct ActClipV1 {
    int x;
    int y;
    int sprIndex;
    int flags;
};

struct ActClipV2 {
    int x;
    int y;
    int sprIndex;
    int flags;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    float zoom;
    int angle;
    int clipType;
};

struct ActClipV4 {
    int x;
    int y;
    int sprIndex;
    int flags;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    float zoomX;
    float zoomY;
    int angle;
    int clipType;
};

struct ActClipV5Extra {
    int width;
    int height;
};

struct ActAttachPoint {
    int reserved;
    int x;
    int y;
    int attr;
};

} // namespace

CActRes::CActRes() : numMaxClipPerMotion(0) {}
CActRes::~CActRes() { Reset(); }

bool CActRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    LOG_ACT_LOAD("[ActLoad] LoadFromBuffer: '%s' size=%d\n", fName ? fName : "(null)", size);
    (void)fName;

    LOG_ACT_LOAD("[ActLoad] Calling Reset...\n");
    Reset();
    LOG_ACT_LOAD("[ActLoad] Reset done\n");
    if (!buffer || size < static_cast<int>(sizeof(ActHeader))) {
        LOG_ACT_LOAD("[ActLoad] FAIL: null/short buffer\n");
        return false;
    }

    const unsigned char* cursor = buffer;
    const unsigned char* end = buffer + size;

    ActHeader header{};
    if (!ReadValue(cursor, end, header)) {
        LOG_ACT_LOAD("[ActLoad] FAIL: could not read header\n");
        return false;
    }
    LOG_ACT_LOAD("[ActLoad] id=0x%04X ver=0x%04X count=%u\n", header.id, header.ver, header.count);
    if (header.id != 17217 || header.ver > 0x205u) {
        LOG_ACT_LOAD("[ActLoad] FAIL: bad id or ver\n");
        return false;
    }

    LOG_ACT_LOAD("[ActLoad] actions.resize(%u)...\n", header.count);
    actions.resize(header.count);
    LOG_ACT_LOAD("[ActLoad] m_delay.assign(%u)...\n", header.count);
    m_delay.assign(header.count, 4.0f);
    LOG_ACT_LOAD("[ActLoad] Starting action parse loop\n");

    for (unsigned int actionIndex = 0; actionIndex < header.count; ++actionIndex) {
        unsigned int motionCount = 0;
        if (!ReadValue(cursor, end, motionCount)) {
            LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] could not read motionCount\n", actionIndex);
            Reset();
            return false;
        }
        LOG_ACT_LOAD("[ActLoad] action[%u] motionCount=%u\n", actionIndex, motionCount);

        CAction& action = actions[actionIndex];
        action.motions.resize(motionCount);
        for (unsigned int motionIndex = 0; motionIndex < motionCount; ++motionIndex) {
            CMotion& motion = action.motions[motionIndex];
            if (!ReadBytes(cursor, end, &motion.range1, sizeof(RECT)) ||
                !ReadBytes(cursor, end, &motion.range2, sizeof(RECT))) {
                LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] range read failed\n", actionIndex, motionIndex);
                Reset();
                return false;
            }

            unsigned int spriteCount = 0;
            if (!ReadValue(cursor, end, spriteCount)) {
                LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] spriteCount read failed\n", actionIndex, motionIndex);
                Reset();
                return false;
            }
            LOG_ACT_LOAD("[ActLoad] action[%u] motion[%u] spriteCount=%u\n", actionIndex, motionIndex, spriteCount);

            motion.sprClips.resize(spriteCount);
            numMaxClipPerMotion = (std::max)(numMaxClipPerMotion, static_cast<int>(spriteCount));

            for (unsigned int clipIndex = 0; clipIndex < spriteCount; ++clipIndex) {
                CSprClip& clip = motion.sprClips[clipIndex];
                clip.r = 0xFF;
                clip.g = 0xFF;
                clip.b = 0xFF;
                clip.a = 0xFF;
                clip.zoomX = 1.0f;
                clip.zoomY = 1.0f;
                clip.angle = 0;
                clip.clipType = 0;

                if (header.ver >= 0x204u) {
                    ActClipV4 src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V4 read failed\n", clipIndex);
                        Reset();
                        return false;
                    }
                    clip.x = src.x;
                    clip.y = src.y;
                    clip.sprIndex = src.sprIndex;
                    clip.flags = src.flags;
                    clip.r = src.r;
                    clip.g = src.g;
                    clip.b = src.b;
                    clip.a = src.a;
                    clip.zoomX = src.zoomX;
                    clip.zoomY = src.zoomY;
                    clip.angle = src.angle;
                    clip.clipType = src.clipType;

                    if (header.ver >= 0x205u) {
                        ActClipV5Extra extra{};
                        if (!ReadValue(cursor, end, extra)) {
                            LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V5 extra read failed\n", clipIndex);
                            Reset();
                            return false;
                        }
                    }
                } else if (header.ver >= 0x200u) {
                    ActClipV2 src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V2 read failed\n", clipIndex);
                        Reset();
                        return false;
                    }
                    clip.x = src.x;
                    clip.y = src.y;
                    clip.sprIndex = src.sprIndex;
                    clip.flags = src.flags;
                    clip.r = src.r;
                    clip.g = src.g;
                    clip.b = src.b;
                    clip.a = src.a;
                    clip.zoomX = src.zoom;
                    clip.zoomY = src.zoom;
                    clip.angle = src.angle;
                    clip.clipType = src.clipType;
                } else {
                    ActClipV1 src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V1 read failed\n", clipIndex);
                        Reset();
                        return false;
                    }
                    clip.x = src.x;
                    clip.y = src.y;
                    clip.sprIndex = src.sprIndex;
                    clip.flags = src.flags;
                }
            }

            motion.eventId = -1;
            if (header.ver >= 0x200u) {
                if (!ReadValue(cursor, end, motion.eventId)) {
                    LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] eventId read failed\n", actionIndex, motionIndex);
                    Reset();
                    return false;
                }
                if (header.ver == 0x200u) {
                    motion.eventId = -1;
                }
            }

            motion.attachCount = 0;
            motion.attachInfo.clear();
            if (header.ver >= 0x203u) {
                if (!ReadValue(cursor, end, motion.attachCount)) {
                    LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] attachCount read failed\n", actionIndex, motionIndex);
                    Reset();
                    return false;
                }
                if (motion.attachCount < 0) {
                    LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] negative attachCount=%d\n", actionIndex, motionIndex, motion.attachCount);
                    Reset();
                    return false;
                }

                motion.attachInfo.resize(static_cast<size_t>(motion.attachCount));
                for (int attachIndex = 0; attachIndex < motion.attachCount; ++attachIndex) {
                    ActAttachPoint src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] attach[%d] read failed\n", actionIndex, motionIndex, attachIndex);
                        Reset();
                        return false;
                    }

                    CAttachPointInfo& dst = motion.attachInfo[static_cast<size_t>(attachIndex)];
                    dst.x = src.x;
                    dst.y = src.y;
                    dst.attr = src.attr;
                }
            }
        }
    }

    if (header.ver >= 0x201u) {
        unsigned int eventCount = 0;
        if (!ReadValue(cursor, end, eventCount)) {
            LOG_ACT_LOAD("[ActLoad] FAIL: eventCount read failed\n");
            Reset();
            return false;
        }
        LOG_ACT_LOAD("[ActLoad] eventCount=%u\n", eventCount);
        m_events.resize(eventCount);
        for (unsigned int i = 0; i < eventCount; ++i) {
            char eventName[40] = {};
            if (!ReadBytes(cursor, end, eventName, sizeof(eventName))) {
                LOG_ACT_LOAD("[ActLoad] FAIL: event[%u] name read failed\n", i);
                Reset();
                return false;
            }
            m_events[i] = eventName;
        }
    }

    if (header.ver >= 0x202u) {
        for (unsigned int i = 0; i < header.count; ++i) {
            if (!ReadValue(cursor, end, m_delay[i])) {
                LOG_ACT_LOAD("[ActLoad] FAIL: delay[%u] read failed\n", i);
                Reset();
                return false;
            }
        }
    }

    LOG_ACT_LOAD("[ActLoad] SUCCESS: '%s' actions=%u\n", fName ? fName : "(null)", header.count);
    return true;
}

bool CActRes::Load(const char* fName) { return CRes::Load(fName); }
CRes* CActRes::Clone() { return new CActRes(); }
void CActRes::Reset() { actions.clear(); m_events.clear(); m_delay.clear(); numMaxClipPerMotion = 0; }

float CActRes::GetDelay(int actionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(m_delay.size())) {
        return kDefaultMotionDelay;
    }
    return m_delay[actionIndex];
}

int CActRes::GetMotionCount(int actionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(actions.size())) {
        return 0;
    }
    return static_cast<int>(actions[actionIndex].motions.size());
}

const CMotion* CActRes::GetMotion(int actionIndex, int motionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(actions.size())) {
        return nullptr;
    }
    const CAction& action = actions[actionIndex];
    if (motionIndex < 0 || motionIndex >= static_cast<int>(action.motions.size())) {
        return nullptr;
    }
    return &action.motions[motionIndex];
}

const char* CActRes::GetEventName(int eventIndex) const {
    if (eventIndex < 0 || eventIndex >= static_cast<int>(m_events.size())) {
        return "";
    }
    return m_events[static_cast<size_t>(eventIndex)].c_str();
}

// CGameObject stubs
// CGameObject::~CGameObject() {} // defined in header as virtual

// CRenderObject stubs
void CRenderObject::SetAction(int act, int mot, int type) {
    CActRes* actRes = ResolveRuntimeActorActRes(this);
    const float baseDelay = actRes ? actRes->GetDelay(act) : kDefaultMotionDelay;
    const float clampedDelay = (std::max)(kDefaultMotionSpeedFactor, baseDelay);

    m_baseAction = act;
    m_curAction = act;
    m_curMotion = 0;
    m_oldBaseAction = act;
    m_oldMotion = 0;
    m_motionType = type;
    m_motionSpeed = clampedDelay * (m_modifyFactorOfmotionSpeed > 0.0f
        ? m_modifyFactorOfmotionSpeed
        : kDefaultMotionSpeedFactor);
    m_isMotionFinished = 0;
    m_loopCountOfmotionFinish = 1.0f;
    m_modifyFactorOfmotionSpeed = kDefaultMotionSpeedFactor;
    m_isForceState = 0;
    m_isForceState2 = 0;
    m_isForceState3 = 0;
}

void CRenderObject::ProcessMotion() {
    CActRes* actRes = ResolveRuntimeActorActRes(this);
    if (!actRes) {
        return;
    }

    if (m_oldBaseAction != m_baseAction) {
        m_oldMotion = 0;
    }

    const int resolvedAction = m_baseAction + Get8Dir(m_roty);

    const int availableAction = ResolveAvailableActionIndex(actRes, resolvedAction);
    m_curAction = availableAction >= 0 ? availableAction : resolvedAction;

    const int motionCount = actRes->GetMotionCount(m_curAction);
    if (motionCount <= 0) {
        m_curMotion = 0;
        m_oldBaseAction = m_baseAction;
        m_oldMotion = 0;
        return;
    }

    if (m_isMotionFinished || m_isMotionFreezed) {
        m_curMotion = (std::min)(m_curMotion, motionCount - 1);
        m_oldBaseAction = m_baseAction;
        m_oldMotion = m_curMotion;
        return;
    }

    const int previousMotion = (std::max)(0, (std::min)(m_oldMotion, motionCount - 1));

    const float stateTicks = static_cast<float>(timeGetTime() - m_stateStartTick) * 0.041666668f;
    const float motionSpeed = (std::max)(kDefaultMotionSpeedFactor, m_motionSpeed);

    if (m_motionType == 1) {
        const float motionProgress = stateTicks / motionSpeed;
        m_curMotion = static_cast<int>(motionProgress) % motionCount;

        const float loopProgress = motionProgress / static_cast<float>(motionCount);
        if (loopProgress >= 1.0f) {
            m_curMotion = motionCount - 1;
            if (loopProgress >= m_loopCountOfmotionFinish) {
                m_isMotionFinished = 1;
            }
        }
        PlayMotionWaveEvents(this, actRes, m_curAction, previousMotion, m_curMotion);
        m_oldBaseAction = m_baseAction;
        m_oldMotion = m_curMotion;
        return;
    }

    m_curMotion = static_cast<int>(stateTicks / motionSpeed) % motionCount;
    PlayMotionWaveEvents(this, actRes, m_curAction, previousMotion, m_curMotion);
    m_oldBaseAction = m_baseAction;
    m_oldMotion = m_curMotion;
}

void CRenderObject::SetRenderInfo(RENDER_INFO_RECT* rect, float f1, float f2) {
}

void CRenderObject::SetTlvert(float x, float y) {
}

CGameActor::~CGameActor()
{
    for (CMsgEffect* effect : m_msgEffectList) {
        if (!effect) {
            continue;
        }
        effect->SendMsg(this, 49, 0, 0, 0);
        effect->SendMsg(this, 53, 0, 0, 0);
    }
    m_msgEffectList.clear();
    m_birdEffect = nullptr;
}

u8 CGameActor::ProcessState() {
    ProcessWillBeAttacked();

    if (m_isMoving) {
        const u32 now = g_session.GetServerTime();
        const vector3d prevPos = m_pos;
        if (m_path.m_cells.size() >= 2 && InterpolatePathPosition(*this, now, &m_pos)) {
            if (now >= m_moveEndTime) {
                m_pos = m_moveEndPos;
                m_isMoving = 0;
            }
        } else if (m_moveEndTime <= m_moveStartTime || now >= m_moveEndTime) {
            m_pos = m_moveEndPos;
            m_isMoving = 0;
        } else {
            const float duration = static_cast<float>(m_moveEndTime - m_moveStartTime);
            const float ratio = static_cast<float>(now - m_moveStartTime) / duration;
            const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));
            m_pos.x = m_moveStartPos.x + (m_moveEndPos.x - m_moveStartPos.x) * clamped;
            m_pos.y = m_moveStartPos.y + (m_moveEndPos.y - m_moveStartPos.y) * clamped;
            m_pos.z = m_moveStartPos.z + (m_moveEndPos.z - m_moveStartPos.z) * clamped;
        }

        if (prevPos.x != m_pos.x || prevPos.z != m_pos.z) {
            const float dx = m_pos.x - prevPos.x;
            const float dz = m_pos.z - prevPos.z;
            m_dist += std::sqrt(dx * dx + dz * dz);
            const float rotation = std::atan2(m_pos.x - prevPos.x, -(m_pos.z - prevPos.z)) * (180.0f / 3.14159265f);
            m_roty = NormalizeAngle360(rotation);
        }
    }

    switch (m_stateId) {
    case kAttackStateId:
        m_isMotionFreezed = 0;
        ProcessMotion();
        if (m_isMotionFinished) {
            m_stateId = 0;
            m_attackMotion = -1.0f;
        }
        break;
    case kDeathStateId:
        m_isMotionFreezed = 0;
        m_isForceState = 0;
        m_isForceState2 = 0;
        m_isForceState3 = 0;
        ProcessMotion();
        LogDeathMotionProgress(*this);
        break;
    case kHitReactionStateId: {
        m_isMotionFreezed = 0;
        ProcessMotion();

        const u32 elapsedMs = timeGetTime() - m_stateStartTick;
        const float ratio = (std::min)(1.0f, static_cast<float>(elapsedMs) / static_cast<float>(kHitReactionDurationMs));
        m_pos.x = m_moveStartPos.x + (m_moveEndPos.x - m_moveStartPos.x) * ratio;
        m_pos.z = m_moveStartPos.z + (m_moveEndPos.z - m_moveStartPos.z) * ratio;

        if (m_isMotionFinished || elapsedMs >= kHitReactionDurationMs) {
            m_pos.x = m_moveEndPos.x;
            m_pos.z = m_moveEndPos.z;
            m_stateId = 0;
            m_targetGid = 0;
            m_attackMotion = -1.0f;
        }
        break;
    }
    default:
        break;
    }

    ProcessQueuedHitWave(*this);

    return 1;
}

void CGameActor::SendMsg(CGameObject* src, int msg, int par1, int par2, int par3)
{
    switch (msg) {
    case 28:
        SetState(kDeathStateId);
        return;
    case 98:
        m_willBeDead = 0;
        m_vanishTime = 0;
        m_isLieOnGround = 0;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        m_attackMotion = -1.0f;
        m_shouldAddPickInfo = 1;
        m_stateId = 0;
        SetState(0);
        if (CPc* pcActor = dynamic_cast<CPc*>(this)) {
            pcActor->InvalidateBillboard();
        }
        return;
    case 88: {
        CGameActor* sourceActor = dynamic_cast<CGameActor*>(src);
        MakeDamageNumber(*this, sourceActor, par1, static_cast<u32>(par2), par3);
        return;
    }
    default:
        return;
    }
}

void CGameActor::SetState(int state) {
    if (m_stateId == kHitReactionStateId) {
        m_pos.x = m_damageDestX;
        m_pos.z = m_damageDestZ;
    }

    m_stateId = state;
    m_stateStartTick = timeGetTime();

    switch (state) {
    case 0:
        m_attackMotion = -1.0f;
        m_isMotionFreezed = 0;
        return;
    case kHitReactionStateId:
        if (m_isForceState || m_isForceState2 || m_isForceState3) {
            return;
        }
        m_isMoving = 0;
        m_path.Reset();
        m_moveStartPos = m_pos;
        m_moveEndPos = m_pos;
        m_moveEndPos.x = m_damageDestX;
        m_moveEndPos.z = m_damageDestZ;
        m_targetGid = 0;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        SetAction(kHitReactionAction, kHitReactionMotion, 1);
        return;
    case kDeathStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_moveStartPos = m_pos;
        m_moveEndPos = m_pos;
        m_moveStartTime = 0;
        m_moveEndTime = 0;
        m_targetGid = 0;
        m_attackMotion = -1.0f;
        m_isLieOnGround = 1;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        SetAction(kDeathAction, kDeathMotion, 1);
        return;
    default:
        return;
    }
}

void CGameActor::ProcessWillBeAttacked()
{
    const u32 now = timeGetTime();
    for (auto it = m_willBeAttackedList.begin(); it != m_willBeAttackedList.end(); ) {
        if (now < it->time) {
            ++it;
            continue;
        }

        ApplyQueuedHitReaction(*this, *it);
        it = m_willBeAttackedList.erase(it);
    }
}

void CGameActor::QueueWillBeAttacked(const WBA& hitInfo)
{
    m_willBeAttackedList.push_back(hitInfo);
}

void CGameActor::SetModifyFactorOfmotionSpeed(int attackMT)
{
    if (attackMT <= 0) {
        attackMT = 1440;
    }

    const float factor = static_cast<float>(attackMT) * kAttackMotionFactor;
    m_modifyFactorOfmotionSpeed = factor;
    m_modifyFactorOfmotionSpeed2 = factor;
}

int CGameActor::GetAttackMotion()
{
    CActRes* actRes = ResolveRuntimeActorActRes(this);
    if (!actRes) {
        return 0;
    }

    const int motionCount = actRes->GetMotionCount(m_curAction);
    for (int motionIndex = 0; motionIndex < motionCount; ++motionIndex) {
        const CMotion* motion = actRes->GetMotion(m_curAction, motionIndex);
        if (!motion || motion->eventId < 0) {
            continue;
        }

        const char* eventName = actRes->GetEventName(motion->eventId);
        if (eventName && std::strcmp(eventName, "atk") == 0) {
            return motionIndex;
        }
    }

    return motionCount > 1 ? motionCount - 2 : 0;
}

void CGameActor::RegisterPos() {
    g_world.RegisterActor(this);
}

void CGameActor::UnRegisterPos() {
    g_world.UnregisterActor(this);
}

void CGameActor::DeleteMatchingEffect(CMsgEffect* effect)
{
    if (!effect) {
        return;
    }

    if (m_birdEffect == effect) {
        m_birdEffect = nullptr;
    }

    const auto it = std::find(m_msgEffectList.begin(), m_msgEffectList.end(), effect);
    if (it != m_msgEffectList.end()) {
        m_msgEffectList.erase(it);
    }
}

void CGameActor::DeleteTotalNumber(int kind)
{
    for (CMsgEffect* effect : m_msgEffectList) {
        if (!effect || effect->m_msgEffectType != kind) {
            continue;
        }
        effect->SendMsg(this, 49, 0, 0, 0);
        effect->SendMsg(this, 53, 0, 0, 0);
    }
}

namespace {

void ReleaseItemBillboardTexture(CItem& item)
{
    if (item.m_billboardTextureOwned && item.m_billboardTexture) {
        delete item.m_billboardTexture;
    }
    item.m_billboardTexture = nullptr;
    item.m_billboardTextureOwned = 0;
}

float DotVec3Item(const vector3d& a, const vector3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector3d NormalizeVec3Item(const vector3d& value)
{
    const float lengthSq = DotVec3Item(value, value);
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

vector3d ScaleVec3Item(const vector3d& value, float scale)
{
    return vector3d{ value.x * scale, value.y * scale, value.z * scale };
}

vector3d AddVec3Item(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x + b.x, a.y + b.y, a.z + b.z };
}

bool ProjectItemPoint(const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
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
    if (!std::isfinite(clipZ) || clipZ <= kItemProjectSubmitNearPlane) {
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

    outVertex->x = g_renderer.m_xoffset + projectedX * g_renderer.m_hpc * oow;
    outVertex->y = g_renderer.m_yoffset + projectedY * g_renderer.m_vpc * oow;
    const float depth = (1500.0f / (1500.0f - kItemProjectNearPlane)) * ((1.0f / oow) - kItemProjectNearPlane) * oow;
    if (!std::isfinite(outVertex->x) || !std::isfinite(outVertex->y) || !std::isfinite(depth)) {
        return false;
    }

    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, depth));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return true;
}

bool ResolveItemSpritePaths(const std::string& resourceName, std::string* outActName, std::string* outSprName)
{
    if (outActName) {
        outActName->clear();
    }
    if (outSprName) {
        outSprName->clear();
    }
    if (resourceName.empty()) {
        return false;
    }

    const char* const roots[] = {
        kLegacyItemSpriteRoot,
        "sprite\\\xBE\xC6\xC0\xCC\xC5\xDB\\",
        "data\\sprite\\item\\",
        "sprite\\item\\",
        "data\\sprite\\items\\",
        "sprite\\items\\",
        nullptr
    };

    for (int index = 0; roots[index]; ++index) {
        const std::string actPath = std::string(roots[index]) + resourceName + ".act";
        const std::string sprPath = std::string(roots[index]) + resourceName + ".spr";
        if (g_resMgr.IsExist(actPath.c_str()) && g_resMgr.IsExist(sprPath.c_str())) {
            if (outActName) {
                *outActName = actPath;
            }
            if (outSprName) {
                *outSprName = sprPath;
            }
            return true;
        }
    }

    return false;
}

bool ResolveItemResources(CItem& item)
{
    if (!item.m_resourceName.empty()) {
        std::string actPath;
        std::string sprPath;
        if (ResolveItemSpritePaths(item.m_resourceName, &actPath, &sprPath)) {
            item.m_actRes = g_resMgr.GetAs<CActRes>(actPath.c_str());
            item.m_sprRes = g_resMgr.GetAs<CSprRes>(sprPath.c_str());
        } else {
            item.m_actRes = nullptr;
            item.m_sprRes = nullptr;
        }
    } else {
        item.m_actRes = nullptr;
        item.m_sprRes = nullptr;
    }

    if (item.m_actRes && item.m_sprRes) {
        return true;
    }

    static std::map<std::string, bool> loggedMissingItems;
    const std::string key = item.m_resourceName.empty()
        ? std::string("item:") + std::to_string(item.m_itemId)
        : item.m_resourceName;
    if (loggedMissingItems.insert(std::make_pair(key, true)).second) {
        DbgLog("[Item] missing floor item resources itemId=%u resource='%s'\n",
            static_cast<unsigned int>(item.m_itemId),
            item.m_resourceName.c_str());
    }
    return false;
}

bool EnsureItemBillboardTexture(CItem& item)
{
    if (!ResolveItemResources(item)) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    const bool isVulkanBackend = GetRenderDevice().GetBackendType() == RenderBackendType::Vulkan;

    const int motionCount = item.m_actRes->GetMotionCount(0);
    if (motionCount <= 0) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    int motion = item.m_curMotion;
    if (motion < 0 || motion >= motionCount) {
        motion = 0;
    }

    if (item.m_billboardTexture && item.m_cachedBillboardMotion == motion) {
        return true;
    }

    if (isVulkanBackend) {
        const SharedItemBillboardKey sharedKey{ item.m_itemId, item.m_identified, motion };
        const auto& sharedCache = GetSharedItemBillboardCache();
        const auto sharedIt = sharedCache.find(sharedKey);
        if (sharedIt != sharedCache.end() && sharedIt->second.texture) {
            if (item.m_billboardTexture != sharedIt->second.texture) {
                ReleaseItemBillboardTexture(item);
            }
            item.m_billboardTexture = sharedIt->second.texture;
            item.m_billboardTextureOwned = 0;
            item.m_billboardTextureWidth = sharedIt->second.width;
            item.m_billboardTextureHeight = sharedIt->second.height;
            item.m_billboardAnchorX = sharedIt->second.anchorX;
            item.m_billboardAnchorY = sharedIt->second.anchorY;
            item.m_billboardOpaqueBounds = sharedIt->second.opaqueBounds;
            item.m_cachedBillboardMotion = motion;
            return true;
        }
    }

    const CMotion* spriteMotion = item.m_actRes->GetMotion(0, motion);
    if (!spriteMotion) {
        spriteMotion = item.m_actRes->GetMotion(0, 0);
        motion = 0;
    }
    if (!spriteMotion) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    CDCBitmap composeSurface(kItemBillboardComposeWidth, kItemBillboardComposeHeight);
    RECT clearRect{ 0, 0, kItemBillboardComposeWidth, kItemBillboardComposeHeight };
    composeSurface.ClearSurface(&clearRect, 0x00000000u);
    composeSurface.BltSprite(kItemBillboardAnchorX,
        kItemBillboardAnchorY,
        item.m_sprRes,
        const_cast<CMotion*>(spriteMotion),
        item.m_sprRes->m_pal);

    tagRECT opaqueBounds{};
    if (!FindOpaqueBounds(composeSurface.GetImageData(),
            static_cast<int>(composeSurface.GetWidth()),
            static_cast<int>(composeSurface.GetHeight()),
            &opaqueBounds)) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    std::vector<unsigned int> pixels(composeSurface.GetImageData(),
        composeSurface.GetImageData() + static_cast<size_t>(composeSurface.GetWidth()) * static_cast<size_t>(composeSurface.GetHeight()));
    UnpremultiplyPixels(pixels);

    if (!item.m_billboardTexture
        || !item.m_billboardTextureOwned
        || item.m_billboardTextureWidth != kItemBillboardComposeWidth
        || item.m_billboardTextureHeight != kItemBillboardComposeHeight) {
        ReleaseItemBillboardTexture(item);
        item.m_billboardTexture = new CTexture();
        if (!item.m_billboardTexture) {
            return false;
        }
        if (!item.m_billboardTexture->Create(kItemBillboardComposeWidth, kItemBillboardComposeHeight, PF_A8R8G8B8, false)) {
            ReleaseItemBillboardTexture(item);
            return false;
        }
        item.m_billboardTextureOwned = 1;
        item.m_billboardTextureWidth = kItemBillboardComposeWidth;
        item.m_billboardTextureHeight = kItemBillboardComposeHeight;
    }

    item.m_billboardTexture->Update(0,
        0,
        kItemBillboardComposeWidth,
        kItemBillboardComposeHeight,
        pixels.data(),
        true,
        kItemBillboardComposeWidth * static_cast<int>(sizeof(unsigned int)));
    item.m_billboardAnchorX = kItemBillboardAnchorX;
    item.m_billboardAnchorY = kItemBillboardAnchorY;
    item.m_billboardOpaqueBounds = opaqueBounds;
    item.m_cachedBillboardMotion = motion;

    if (isVulkanBackend && item.m_billboardTexture) {
        SharedItemBillboardValue value{};
        value.texture = item.m_billboardTexture;
        value.opaqueBounds = opaqueBounds;
        value.width = item.m_billboardTextureWidth;
        value.height = item.m_billboardTextureHeight;
        value.anchorX = item.m_billboardAnchorX;
        value.anchorY = item.m_billboardAnchorY;
        GetSharedItemBillboardCache()[SharedItemBillboardKey{ item.m_itemId, item.m_identified, motion }] = value;
        item.m_billboardTextureOwned = 0;
    }

    return true;
}

} // namespace

bool CItem::EnsureBillboardTexture()
{
    return EnsureItemBillboardTexture(*this);
}

CItem::CItem()
    : m_aid(0)
    , m_itemId(0)
    , m_amount(0)
    , m_tileX(0)
    , m_tileY(0)
    , m_identified(0)
    , m_subX(0)
    , m_subY(0)
    , m_isJumping(0)
    , m_sfallingSpeed(0.0f)
    , m_sPosY(0.0f)
    , m_billboardTexture(nullptr)
    , m_billboardTextureOwned(0)
    , m_billboardTextureWidth(0)
    , m_billboardTextureHeight(0)
    , m_billboardAnchorX(kItemBillboardAnchorX)
    , m_billboardAnchorY(kItemBillboardAnchorY)
    , m_cachedBillboardMotion(-1)
{
    m_billboardOpaqueBounds.left = 0;
    m_billboardOpaqueBounds.top = 0;
    m_billboardOpaqueBounds.right = 0;
    m_billboardOpaqueBounds.bottom = 0;
    m_isVisible = 1;
    m_isVisibleBody = 1;
    m_shouldAddPickInfo = 1;
    m_motionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed2 = 1.0f;
    m_baseAction = 0;
    m_curAction = 0;
    m_curMotion = 0;
    m_oldBaseAction = 0;
    m_oldMotion = 0;
    m_stateStartTick = timeGetTime();
    m_pos = vector3d{ 0.0f, 0.0f, 0.0f };
}

CItem::~CItem()
{
    ReleaseItemBillboardTexture(*this);
}

u8 CItem::OnProcess()
{
    if (m_isJumping) {
        m_sfallingSpeed += 0.18f;
        m_sPosY -= m_sfallingSpeed;
        if (m_sPosY <= 0.0f) {
            if (m_sfallingSpeed > 0.45f) {
                m_sPosY = -m_sPosY * 0.35f;
                m_sfallingSpeed = -m_sfallingSpeed * 0.45f;
            } else {
                m_sPosY = 0.0f;
                m_sfallingSpeed = 0.0f;
                m_isJumping = 0;
            }
        }
    }

    if (ResolveItemResources(*this)) {
        ProcessMotion();
    } else {
        m_curMotion = 0;
    }

    return 1;
}

void CItem::Render(matrix* viewMatrix)
{
    if (!viewMatrix || !m_isVisible || !EnsureBillboardTexture()) {
        return;
    }

    vector3d base = m_pos;
    if (g_world.m_attr) {
        base.y = g_world.m_attr->GetHeight(base.x, base.z);
    }
    base.y += m_sPosY;

    tlvertex3d projectedBase{};
    if (!ProjectItemPoint(*viewMatrix, base, &projectedBase)) {
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
    face->tex = m_billboardTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = projectedBase.oow;

    const float scaledAnchorX = static_cast<float>(m_billboardAnchorX) * kGroundItemScreenScale;
    const float scaledAnchorY = static_cast<float>(m_billboardAnchorY) * kGroundItemScreenScale;
    const float scaledWidth = static_cast<float>(m_billboardTextureWidth) * kGroundItemScreenScale;
    const float scaledHeight = static_cast<float>(m_billboardTextureHeight) * kGroundItemScreenScale;
    const float left = projectedBase.x - scaledAnchorX;
    const float top = projectedBase.y - scaledAnchorY;
    const float right = left + scaledWidth;
    const float bottom = top + scaledHeight;

    face->m_verts[0].x = left;
    face->m_verts[0].y = top;
    face->m_verts[1].x = right;
    face->m_verts[1].y = top;
    face->m_verts[2].x = left;
    face->m_verts[2].y = bottom;
    face->m_verts[3].x = right;
    face->m_verts[3].y = bottom;

    for (int index = 0; index < 4; ++index) {
        face->m_verts[index].z = (std::max)(0.0f, projectedBase.z - kItemBillboardDepthBias);
        face->m_verts[index].oow = projectedBase.oow;
        face->m_verts[index].color = 0xFFFFFFFFu;
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
    g_renderer.AddRP(face, 1);
}

int CItem::Get8Dir(float rot)
{
    (void)rot;
    return 0;
}

void CItem::InvalidateBillboard()
{
    m_cachedBillboardMotion = -1;
}

void CItem::TriggerDropAnimation()
{
    m_isJumping = 1;
    m_sPosY = 2.2f;
    m_sfallingSpeed = -0.28f;
}

CPc::CPc()
    : m_billboardTexture(nullptr)
    , m_billboardTextureOwned(0)
    , m_billboardTextureWidth(0)
    , m_billboardTextureHeight(0)
    , m_billboardAnchorX(kPlayerBillboardAnchorX)
    , m_billboardAnchorY(kPlayerBillboardAnchorY)
    , m_cachedBillboardBodyAction(-1)
    , m_cachedBillboardHeadMotion(-1)
    , m_cachedBillboardJob(-1)
    , m_cachedBillboardHead(-1)
    , m_cachedBillboardSex(-1)
    , m_cachedBillboardBodyPalette(-1)
    , m_cachedBillboardHeadPalette(-1)
    , m_cachedNonPcResourceJob(-1)
    , m_cachedNonPcActRes(nullptr)
    , m_cachedNonPcSprRes(nullptr)
{
    m_billboardOpaqueBounds.left = 0;
    m_billboardOpaqueBounds.top = 0;
    m_billboardOpaqueBounds.right = 0;
    m_billboardOpaqueBounds.bottom = 0;
    m_sprRes = nullptr;
    m_actRes = nullptr;
    m_motionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed2 = 1.0f;
    m_attackMotion = -1.0f;
    m_curAction = 0;
    m_baseAction = 0;
    m_curMotion = 0;
    m_oldBaseAction = 0;
    m_oldMotion = 0;
    m_isPlayHitWave = 0;
    m_hitWaveName[0] = '\0';
}

CPc::~CPc()
{
    ReleaseActorBillboardTexture(*this);
}

void CPc::InvalidateBillboard()
{
    m_cachedBillboardBodyAction = -1;
    m_cachedBillboardHeadMotion = -1;
    m_cachedBillboardJob = -1;
    m_cachedBillboardHead = -1;
    m_cachedBillboardSex = -1;
    m_cachedBillboardBodyPalette = -1;
    m_cachedBillboardHeadPalette = -1;
    const int displayJob = ResolveDisplayJob(*this);
    if ((!m_isPc || displayJob != m_job) && m_cachedNonPcResourceJob != displayJob) {
        m_cachedNonPcResourceJob = -1;
        m_cachedNonPcActRes = nullptr;
        m_cachedNonPcSprRes = nullptr;
    }
}

namespace {

bool ResolveCachedNonPcResourcesForActor(CPc& actor, CActRes** outActRes, CSprRes** outSprRes)
{
    if (!outActRes || !outSprRes) {
        return false;
    }

    const int displayJob = ResolveDisplayJob(actor);

    if (actor.m_cachedNonPcResourceJob != displayJob
        || !actor.m_cachedNonPcActRes
        || !actor.m_cachedNonPcSprRes) {
        char actPath[260] = {};
        char sprPath[260] = {};
        const char* const jobName = g_session.GetJobName(displayJob);
        if (!ResolveNonPcSpritePaths(displayJob, actPath, sprPath)) {
            LogNonPcResourceMissingOnce(displayJob, jobName, actPath, sprPath, nullptr, nullptr);
            actor.m_cachedNonPcResourceJob = -1;
            actor.m_cachedNonPcActRes = nullptr;
            actor.m_cachedNonPcSprRes = nullptr;
            return false;
        }

        actor.m_cachedNonPcActRes = g_resMgr.GetAs<CActRes>(actPath);
        actor.m_cachedNonPcSprRes = g_resMgr.GetAs<CSprRes>(sprPath);
        if (!actor.m_cachedNonPcActRes || !actor.m_cachedNonPcSprRes) {
            LogNonPcResourceMissingOnce(displayJob,
                jobName,
                actPath,
                sprPath,
                actor.m_cachedNonPcActRes,
                actor.m_cachedNonPcSprRes);
        } else {
            LogNonPcResourceResolvedOnce(displayJob, jobName, actPath, sprPath);
        }
        actor.m_cachedNonPcResourceJob = (actor.m_cachedNonPcActRes && actor.m_cachedNonPcSprRes)
            ? displayJob
            : -1;
    }

    *outActRes = actor.m_cachedNonPcActRes;
    *outSprRes = actor.m_cachedNonPcSprRes;
    return *outActRes != nullptr && *outSprRes != nullptr;
}

CActRes* ResolveRuntimeActorActRes(CRenderObject* object)
{
    if (!object) {
        return nullptr;
    }

    CGameActor* actor = dynamic_cast<CGameActor*>(object);
    if (!actor) {
        return object->m_actRes;
    }

    if (actor->m_isPc) {
        const int displayJob = ResolveDisplayJob(*actor);
        const int sex = actor->m_sex != 0 ? 1 : 0;
        char bodyActPath[260] = {};
        const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyActPath);
        actor->m_actRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
        return actor->m_actRes;
    }

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        CActRes* actRes = nullptr;
        CSprRes* sprRes = nullptr;
        if (ResolveCachedNonPcResourcesForActor(*pcActor, &actRes, &sprRes)) {
            actor->m_actRes = actRes;
            actor->m_sprRes = sprRes;
            return actRes;
        }
    }

    return actor->m_actRes;
}

} // namespace

bool CPc::EnsureBillboardTexture(float cameraLongitude)
{
    const bool isVulkanBackend = GetRenderDevice().GetBackendType() == RenderBackendType::Vulkan;

    const int actorDir = ResolvePcFacingDir(this);
    const float actorRotationDegrees = ActorRotationDegreesFromDir(actorDir);
    const bool isPlayerStyleActor = m_isPc != 0;
    const bool usePlayerStyleBillboard = isPlayerStyleActor;
    const int displayJob = ResolveDisplayJob(*this);
    const int sex = m_sex != 0 ? 1 : 0;
    int bodyAction = (m_isMoving ? 8 : 0) + ResolvePcBodyActionFromView(cameraLongitude, actorRotationDegrees);
    int headMotion = 0;
    if (usePlayerStyleBillboard) {
        char bodyAct[260] = {};
        const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyAct);
        CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
        if (IsTransientActionActive(*this, bodyActRes, m_curAction)) {
            bodyAction = m_curAction;
            headMotion = m_curMotion;
        } else {
            headMotion = ResolvePcMotionIndex(this, bodyAction, bodyActName);
        }
    } else {
        CActRes* actRes = nullptr;
        CSprRes* sprRes = nullptr;
        if (ResolveCachedNonPcResourcesForActor(*this, &actRes, &sprRes) && actRes) {
            const bool transientActionActive = IsTransientActionActive(*this, actRes, m_curAction);
            if (transientActionActive) {
                bodyAction = m_curAction;
                headMotion = m_curMotion;
                LogDeathBillboardSelectionOnce(*this, actRes, bodyAction, headMotion);
            } else {
                headMotion = ResolveSpriteMotionIndex(*this, actRes, bodyAction);
                ProcessRenderMotionWaveEvents(this, actRes, bodyAction, headMotion);
            }
        }
    }

    if (m_billboardTexture
        && m_cachedBillboardBodyAction == bodyAction
        && m_cachedBillboardHeadMotion == headMotion
        && m_cachedBillboardJob == displayJob
        && m_cachedBillboardHead == (usePlayerStyleBillboard ? m_head : 0)
        && m_cachedBillboardSex == sex
        && m_cachedBillboardBodyPalette == (usePlayerStyleBillboard ? m_bodyPalette : 0)
        && m_cachedBillboardHeadPalette == (usePlayerStyleBillboard ? m_headPalette : 0)) {
        return true;
    }

    if (isVulkanBackend && !usePlayerStyleBillboard) {
        const SharedNonPcBillboardKey sharedKey{ displayJob, bodyAction, headMotion };
        const auto& sharedCache = GetSharedNonPcBillboardCache();
        const auto sharedIt = sharedCache.find(sharedKey);
        if (sharedIt != sharedCache.end() && sharedIt->second.texture) {
            if (m_billboardTexture != sharedIt->second.texture) {
                ReleaseActorBillboardTexture(*this);
            }
            m_billboardTexture = sharedIt->second.texture;
            m_billboardTextureOwned = 0;
            m_billboardTextureWidth = sharedIt->second.width;
            m_billboardTextureHeight = sharedIt->second.height;
            m_billboardAnchorX = sharedIt->second.anchorX;
            m_billboardAnchorY = sharedIt->second.anchorY;
            m_billboardOpaqueBounds = sharedIt->second.opaqueBounds;
            m_cachedBillboardBodyAction = bodyAction;
            m_cachedBillboardHeadMotion = headMotion;
            m_cachedBillboardJob = displayJob;
            m_cachedBillboardHead = 0;
            m_cachedBillboardSex = sex;
            m_cachedBillboardBodyPalette = 0;
            m_cachedBillboardHeadPalette = 0;
            return true;
        }
    }

    CDCBitmap composeSurface(kPlayerBillboardComposeWidth, kPlayerBillboardComposeHeight);
    RECT clearRect{ 0, 0, kPlayerBillboardComposeWidth, kPlayerBillboardComposeHeight };
    composeSurface.ClearSurface(&clearRect, 0x00000000u);

    int resolvedJob = -1;
    int resolvedHead = -1;
    int resolvedSex = -1;
    int resolvedBodyPalette = -1;
    int resolvedHeadPalette = -1;
    const bool drawOk = usePlayerStyleBillboard
        ? DrawPcBillboard(composeSurface,
            *this,
            kPlayerBillboardAnchorX,
            kPlayerBillboardAnchorY,
            bodyAction,
            headMotion,
            &resolvedJob,
            &resolvedHead,
            &resolvedSex,
            &resolvedBodyPalette,
            &resolvedHeadPalette)
        : DrawNonPcBillboard(composeSurface,
            *this,
            kPlayerBillboardAnchorX,
            kPlayerBillboardAnchorY,
            bodyAction,
            headMotion,
            &resolvedJob);
    if (!drawOk) {
        if (!usePlayerStyleBillboard) {
            LogNonPcBillboardFailureOnce(displayJob, g_session.GetJobName(displayJob), bodyAction, headMotion);
        }
        return false;
    }

    if (!usePlayerStyleBillboard) {
        resolvedHead = 0;
        resolvedSex = sex;
        resolvedBodyPalette = 0;
        resolvedHeadPalette = 0;
    }

    tagRECT opaqueBounds{};
    if (!FindOpaqueBounds(composeSurface.GetImageData(),
            static_cast<int>(composeSurface.GetWidth()),
            static_cast<int>(composeSurface.GetHeight()),
            &opaqueBounds)) {
        return false;
    }

    std::vector<unsigned int> pixels(composeSurface.GetImageData(),
        composeSurface.GetImageData() + static_cast<size_t>(composeSurface.GetWidth()) * static_cast<size_t>(composeSurface.GetHeight()));
    UnpremultiplyPixels(pixels);

    if (isVulkanBackend) {
        static DWORD s_vulkanBillboardUploadTick = 0;
        static unsigned int s_vulkanBillboardUploadsThisTick = 0;
        const DWORD tick = timeGetTime();
        if (tick != s_vulkanBillboardUploadTick) {
            s_vulkanBillboardUploadTick = tick;
            s_vulkanBillboardUploadsThisTick = 0;
        }
        if (s_vulkanBillboardUploadsThisTick >= 4u) {
            return m_billboardTexture != nullptr;
        }
        ++s_vulkanBillboardUploadsThisTick;
    }

    if (!m_billboardTexture
        || !m_billboardTextureOwned
        || m_billboardTextureWidth != kPlayerBillboardComposeWidth
        || m_billboardTextureHeight != kPlayerBillboardComposeHeight) {
        ReleaseActorBillboardTexture(*this);
        m_billboardTexture = new CTexture();
        if (!m_billboardTexture) {
            return false;
        }
        if (!m_billboardTexture->Create(kPlayerBillboardComposeWidth, kPlayerBillboardComposeHeight, PF_A8R8G8B8, false)) {
            ReleaseActorBillboardTexture(*this);
            return false;
        }
        m_billboardTextureOwned = 1;
        m_billboardTextureWidth = kPlayerBillboardComposeWidth;
        m_billboardTextureHeight = kPlayerBillboardComposeHeight;
    }

    m_billboardTexture->Update(0,
        0,
        kPlayerBillboardComposeWidth,
        kPlayerBillboardComposeHeight,
        pixels.data(),
        true,
        kPlayerBillboardComposeWidth * static_cast<int>(sizeof(unsigned int)));

    m_billboardAnchorX = kPlayerBillboardAnchorX;
    m_billboardAnchorY = kPlayerBillboardAnchorY;
    m_billboardOpaqueBounds = opaqueBounds;
    m_cachedBillboardBodyAction = bodyAction;
    m_cachedBillboardHeadMotion = headMotion;
    m_cachedBillboardJob = resolvedJob;
    m_cachedBillboardHead = resolvedHead;
    m_cachedBillboardSex = resolvedSex;
    m_cachedBillboardBodyPalette = resolvedBodyPalette;
    m_cachedBillboardHeadPalette = resolvedHeadPalette;

    if (isVulkanBackend && !usePlayerStyleBillboard && m_billboardTexture) {
        SharedNonPcBillboardValue value{};
        value.texture = m_billboardTexture;
        value.opaqueBounds = opaqueBounds;
        value.width = m_billboardTextureWidth;
        value.height = m_billboardTextureHeight;
        value.anchorX = m_billboardAnchorX;
        value.anchorY = m_billboardAnchorY;

        GetSharedNonPcBillboardCache()[SharedNonPcBillboardKey{ displayJob, bodyAction, headMotion }] = value;
        m_billboardTextureOwned = 0;
    }

    return true;
}

CPlayer::CPlayer()
    : m_destCellX(-1)
    , m_destCellZ(-1)
    , m_attackReqTime(0)
    , m_preMoveStartTick(0)
    , m_preMoveOn(0)
    , m_attackMode(0)
    , m_isAttackRequest(0)
    , m_isWaitingMoveAck(0)
    , m_isPreengageStateOfMove(0)
    , m_proceedTargetGid(0)
    , m_totalAttackReqCnt(0)
    , m_tickOfMoveForAttack(0)
    , m_moveReqTick(0)
    , m_standTick(0)
    , m_skillId(0)
    , m_skillAttackRange(0)
    , m_skillUseLevel(0)
    , m_gSkillDx(0)
    , m_gSkillDy(0)
    , m_preengageXOfMove(-1)
    , m_preengageYOfMove(-1)
    , m_statusEffect(nullptr)
{
}

CPlayer::~CPlayer()
{
}

void CPlayer::ProcessPreMove()
{
}

// CGrannyPc implementation moved to Granny.cpp


// CGameActor stubs
// CPc stubs
// CPlayer stubs
// CSkill stubs
// CItem stubs
