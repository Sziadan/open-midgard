#include "core/UiMessageStrings.h"
#include "core/SourceWeight.h"
#include "core/SettingsIni.h"
#include "core/SourceChatCommand.h"
#include "core/SourceChatParser.h"
#include "core/SourceChatAliases.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <climits>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

#include "network/Connection.h"
#include "network/Packet.h"
#include "network/GronPacket.h"
#include "network/MapSendProfile.h"
#include "CursorRenderer.h"
#include "GameMode.h"
#include "ItemSnapshot.h"
#include "ui/UIItemCompositionWnd.h"
#include "ui/UIItemIdentifyWnd.h"
#include "core/MannerFilter.h"
#include "core/SourceTextCodec.h"
#include "ui/UIChatRoomWnd.h"
#include "ui/UIChatRoomSettingsWnd.h"
#include "ui/UINpcInputWnd.h"
#include "ui/UIItemPurchaseWnd.h"
#include "ui/UICompanionStatusWnd.h"
#include "ui/UIWhisperWnd.h"
#include "ui/UIPetInfoWnd.h"
#include "ui/UIPetTamingDeceiveWnd.h"
#include "core/ChatRoomRequests.h"
#include "core/MannerFilter.h"
#include "ActionRequestSender.h"
#include "Commerce.h"
#include "PacketPadding.h"
#include "View.h"
#include "qtui/QtUiRuntime.h"
#include "audio/Audio.h"
#include "GameModePacket.h"
#include "GuildCreation.h"
#include "pathfinder/PathFinder.h"
#include "core/ClientInfoLocale.h"
#include "core/File.h"
#include "core/Globals.h"
#include "core/Timer.h"
#include "DebugLog.h"
#include "render/DC.h"
#include "res/ActRes.h"
#include "res/Bitmap.h"
#include "res/GndRes.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "res/WorldRes.h"
#include "session/Session.h"
#include "companion_ai/CompanionAiRuntime.h"
#include "skill/Skill.h"
#include "ui/UIMinimapWnd.h"
#include "ui/UiScale.h"
#include "ui/UINpcMenuWnd.h"
#include "ui/UIPlayerContextMenuWnd.h"
#include "ui/UIRechargeGage.h"
#include "ui/UISayDialogWnd.h"
#include "ui/UIWindow.h"
#include "ui/UIOptionWnd.h"
#include "ui/UIWindowMgr.h"
#include "ui/UISkillSelectWnd.h"
#include "ui/UITransBalloonText.h"
#include "ui/UIChooseSellBuyWnd.h"
#include "render/DrawUtil.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "world/GameActor.h"
#include "world/MsgEffect.h"
#include "world/RagEffect.h"
#include "world/3dActor.h"
#include "world/World.h"
#include "world/Weather.h"
#include "world/Emotion.h"
#include "main/WinMain.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <cstdio>
#include <cstring>
#include <vector>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

void NoteLocalHideSkillRequest(u16 skillId, u32 targetGid);

constexpr int kPlayerVitalsBarWidth = 56;
constexpr int kPlayerVitalsBarHeight = 4;
constexpr int kPlayerVitalsBorderThickness = 1;
constexpr int kPlayerVitalsNameTopPadding = 3;
constexpr int kPlayerVitalsFeetGap = 22;
constexpr float kPlayerVitalsMinZoomScale = 0.2f;
constexpr float kPlayerVitalsMaxZoomScale = 5.0f;
constexpr float kRefAverageCameraDistance = 500.0f;

int ScaleVitalsPixels(int pixels, float scale)
{
    return (std::max)(1, static_cast<int>(std::lround(static_cast<float>(pixels) * scale)));
}

PlayerVitalsScreenMetrics ResolvePlayerVitalsScreenMetrics(const CView* view)
{
    PlayerVitalsScreenMetrics metrics;
    metrics.barWidth = kPlayerVitalsBarWidth;
    metrics.barHeight = kPlayerVitalsBarHeight;
    metrics.border = kPlayerVitalsBorderThickness;
    metrics.namePadding = kPlayerVitalsNameTopPadding;
    const int baseTotalHeight = kPlayerVitalsBarHeight * 2 + kPlayerVitalsBorderThickness * 3;
    metrics.totalHeight = baseTotalHeight;
    metrics.hoverNameExtra = baseTotalHeight + kPlayerVitalsNameTopPadding - 10;

    float distance = kRefAverageCameraDistance;
    if (view) {
        distance = view->GetCameraDistance();
    }
    if (!(distance > 1.0f)) {
        return metrics;
    }

    float scale = kRefAverageCameraDistance / distance;
    if (scale < kPlayerVitalsMinZoomScale) {
        scale = kPlayerVitalsMinZoomScale;
    } else if (scale > kPlayerVitalsMaxZoomScale) {
        scale = kPlayerVitalsMaxZoomScale;
    }

    metrics.barWidth = ScaleVitalsPixels(kPlayerVitalsBarWidth, scale);
    metrics.barHeight = ScaleVitalsPixels(kPlayerVitalsBarHeight, scale);
    metrics.border = ScaleVitalsPixels(kPlayerVitalsBorderThickness, scale);
    metrics.namePadding = (std::max)(0, static_cast<int>(std::lround(static_cast<float>(kPlayerVitalsNameTopPadding) * scale)));
    metrics.totalHeight = metrics.barHeight * 2 + metrics.border * 3;
    // GetPlayerScreenLabel already includes the unscaled gap under the feet.
    metrics.topDelta = static_cast<int>(std::lround(static_cast<float>(kPlayerVitalsFeetGap) * scale)) - kPlayerVitalsFeetGap;
    metrics.hoverNameExtra = metrics.totalHeight + metrics.namePadding + metrics.topDelta - 10;
    metrics.qtNameExtra = metrics.topDelta + (metrics.totalHeight - baseTotalHeight);
    return metrics;
}

bool ActorHasPlayerStyleVitalsOverlay(const CGameActor* actor)
{
    if (!actor || actor->m_gid == 0) {
        return false;
    }
    if (actor->m_gid == g_session.m_gid || actor->m_gid == g_session.m_aid) {
        return true;
    }
    if (g_session.m_homunStatus.alive && actor->m_gid == g_session.m_homunGID) {
        return true;
    }
    if (g_session.m_merStatus.alive && actor->m_gid == g_session.m_merGID) {
        return true;
    }
    return false;
}

int ResolvePlayerStyleVitalsNameOffsetY(const CView* view)
{
    const PlayerVitalsScreenMetrics metrics = ResolvePlayerVitalsScreenMetrics(view);
    return metrics.topDelta + metrics.totalHeight + metrics.namePadding;
}

namespace {
CGameModePacketRouter g_gameModePacketRouter;

void SetTextureDebugName(CTexture* texture, const char* name)
{
    if (!texture || !name || name[0] == '\0') {
        return;
    }

    std::strncpy(texture->m_texName, name, sizeof(texture->m_texName) - 1);
    texture->m_texName[sizeof(texture->m_texName) - 1] = '\0';
}

constexpr char kGameModeBuildMarker[] = "2026-03-23 20:14 actor-trace-f";
constexpr u32 kFramePerfLogIntervalFrames = 120;
constexpr u32 kLoadingScreenMinShowMs = 700;
constexpr u32 kLoadingActorQuietMs = 250;
constexpr u32 kLoadingPostAckWaitMs = 450;
constexpr u32 kLoadingAckRetryDelayMs = 900;
constexpr u32 kLoadingActorFallbackMs = 3000;
constexpr u32 kPacketTraceWindowMs = 8000;
constexpr u32 kBootstrapLoadBudgetMs = 6;
constexpr size_t kBootstrapTextureBatch = 2;
constexpr size_t kBootstrapBackgroundActorBatch = 6;
constexpr size_t kBootstrapFixedEffectBatch = 12;
constexpr float kWorldProcessTickMs = 24.0f;
constexpr u32 kMovingCameraHoverRefreshMs = 100;
constexpr int kMaxWorldProcessStepsPerUpdate = 8;
constexpr int kHoverNameFontHeight = 20;
constexpr unsigned char kHoverNameFontBold = 1;
constexpr int kHoverNameTextPadding = 4;
constexpr int kHoverNameVerticalOffset = 26;
constexpr u32 kPickupRetryIntervalMs = 250;
constexpr int kPlayerVitalsVerticalOffset = 10;
constexpr float kPlayerVitalsOverlaySortKey = 0.95f;
constexpr char kLockedTargetArrowBitmapName[] = "scroll0down.bmp";
constexpr int kLockedTargetArrowBaseLift = 56;
constexpr float kLockedTargetArrowScale = 1.25f;
constexpr char kLockedTargetTexturePath[] = "data\\texture\\effect\\lockon128.tga";
constexpr float kLockedTargetMarkerBaseSize = 30.0f;
constexpr float kLockedTargetMarkerGroundYOffset = -2.0f;
constexpr float kLockedTargetMarkerDepthBias = 0.0003f;
constexpr float kLockedTargetMarkerNearPlane = 80.0f;
constexpr float kLockedTargetMarkerRotationPerMs = 0.27f;
constexpr int kLockedTargetArrowYOffset = 6;
constexpr unsigned int kOverlayTransparentKey = 0x00FF00FFu;
constexpr int kRoMapCornerEllipseSize = 10;
constexpr float kLockedTargetArrowBouncePerMs = 0.0045f;
constexpr float kLockedTargetArrowBouncePixels = 4.0f;
constexpr u32 kHoverNameRequestCooldownMs = 1000;
constexpr u32 kMovingOverlayRefreshMs = 200;
constexpr size_t kNearbyBackgroundLogCount = 12;
constexpr float kRefCloseCameraDistance = 150.0f;
constexpr float kRefIndoorCameraDistance = 420.0f;
constexpr float kRefFarCameraDistance = 700.0f;
constexpr float kRefIndoorLatitudeMin = -55.0f;
constexpr float kRefIndoorLatitudeMax = -35.0f;
constexpr float kRefOutdoorLatitudeMin = -65.0f;
constexpr float kRefOutdoorLatitudeMax = -25.0f;
constexpr float kAmbientSoundBoundarySlack = 75.0f;
constexpr float kAmbientSoundCullSlack = 130.0f;

struct AmbientSoundTrigger {
    float relativeX = 0.0f;
    float relativeZ = 0.0f;
};

bool ResolveAmbientSoundTrigger(float playerX, float playerZ, float soundX, float soundZ,
    int soundWidth, int soundHeight, float soundRange, float soundDiagonal,
    AmbientSoundTrigger* outTrigger)
{
    if (!outTrigger) {
        return false;
    }

    const float playerDx = playerX - soundX;
    const float playerDz = playerZ - soundZ;
    const float centerDistance = std::sqrt(playerDx * playerDx + playerDz * playerDz);
    if (centerDistance - (soundDiagonal + soundRange) > kAmbientSoundCullSlack) {
        return false;
    }

    const bool verticalAxis = playerDx == 0.0f || soundWidth == 0;
    const float slope = verticalAxis ? 0.0f : playerDz / playerDx;
    const int integerAspect = soundWidth != 0 ? soundHeight / soundWidth : 0;
    const std::int64_t integerSlope = verticalAxis ? 0 : static_cast<std::int64_t>(slope);
    const std::int64_t absoluteSlope = integerSlope < 0 ? -integerSlope : integerSlope;
    const bool useHorizontalEdge = !verticalAxis && absoluteSlope <= integerAspect;
    const float xDirection = playerDx >= 0.0f ? 1.0f : -1.0f;
    const float zDirection = playerDz >= 0.0f ? 1.0f : -1.0f;

    float edgeOffsetX = 0.0f;
    float edgeOffsetZ = 0.0f;
    if (useHorizontalEdge) {
        edgeOffsetX = static_cast<float>(soundWidth) * xDirection * 0.5f;
        edgeOffsetZ = edgeOffsetX * slope;
    } else {
        edgeOffsetZ = static_cast<float>(soundHeight) * zDirection * 0.5f;
        edgeOffsetX = verticalAxis ? 0.0f : edgeOffsetZ / slope;
    }

    const float edgeX = soundX + edgeOffsetX;
    const float edgeZ = soundZ + edgeOffsetZ;
    const float edgeDx = edgeX - playerX;
    const float edgeDz = edgeZ - playerZ;
    const float edgeDistance = std::sqrt(edgeDx * edgeDx + edgeDz * edgeDz);
    if (edgeDistance - soundRange > kAmbientSoundBoundarySlack) {
        return false;
    }

    outTrigger->relativeX = edgeDx;
    outTrigger->relativeZ = edgeDz;
    return true;
}

float g_savedOutdoorCameraLatitude = -45.0f;
float g_savedOutdoorCameraDistance = kRefAverageCameraDistance;
float g_savedIndoorCameraLatitude = -45.0f;
float g_savedIndoorCameraDistance = kRefIndoorCameraDistance;

u32 g_packetTraceStartTick = 0;
std::map<u16, bool> g_packetTraceLoggedIds;

struct OverlayMovePerfStats {
    u64 frames = 0;
    u64 modernRefreshes = 0;
    double queueModernMs = 0.0;
    double queueRoMapMs = 0.0;
    double queueLockedTargetMs = 0.0;
    double queueMsgMs = 0.0;
    double queueCursorMs = 0.0;
    double modernOverlayDrawMs = 0.0;
    double modernUiDrawMs = 0.0;
    double modernConvertMs = 0.0;
    double modernTextureUpdateMs = 0.0;
    double fallbackOverlayDrawMs = 0.0;
    double fallbackMsgMs = 0.0;
    double fallbackUiDrawMs = 0.0;
    double fallbackCursorHdcMs = 0.0;
    double fallbackCursorMs = 0.0;
    double flipMs = 0.0;
};

OverlayMovePerfStats g_overlayMovePerfStats;

std::string ToLowerAsciiMapName(const char* rswName)
{
    if (!rswName) {
        return std::string();
    }

    std::string value = rswName;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool MatchMapName(const std::string& mapName, std::initializer_list<const char*> candidates)
{
    for (const char* candidate : candidates) {
        if (candidate && mapName == candidate) {
            return true;
        }
    }
    return false;
}

int ResolveRefWeatherCloudEffectId(const char* rswName)
{
    const std::string mapName = ToLowerAsciiMapName(rswName);
    if (MatchMapName(mapName, {
            "gef_fild07.rsw",
            "mjolnir_01.rsw",
        })) {
        return 229;
    }
    if (MatchMapName(mapName, {
            "yuno.rsw",
            "gonryun.rsw",
            "gon_dun02.rsw",
            "ra_temsky.rsw",
            "que_temsky.rsw",
            "sch_gld.rsw",
            "bat_fild02.rsw",
            "bat_b01.rsw",
            "bat_b02.rsw",
        })) {
        return 230;
    }
    if (MatchMapName(mapName, {
            "valkyrie.rsw",
            "rwc01.rsw",
            "himinn.rsw",
            "que_qsch01.rsw",
            "que_qsch02.rsw",
            "que_qsch03.rsw",
            "que_qsch04.rsw",
            "que_qsch05.rsw",
            "que_qaru01.rsw",
            "que_qaru02.rsw",
            "que_qaru03.rsw",
            "que_qaru04.rsw",
            "que_qaru05.rsw",
        })) {
        return 233;
    }
    if (mapName == "einbroch.rsw") {
        return 515;
    }
    if (mapName == "thana_boss.rsw" || mapName == "moc_fild22.rsw") {
        return 592;
    }
    if (mapName == "airplane.rsw" || mapName == "airplane_01.rsw") {
        return 516;
    }
    if (mapName == "6@tower.rsw") {
        return 697;
    }
    if (mapName == "5@tower.rsw") {
        return 698;
    }
    return 0;
}

int ResolveRefMapClearColor(const char* rswName)
{
    const std::string mapName = ToLowerAsciiMapName(rswName);

    if (MatchMapName(mapName, {
            "yuno.rsw",
            "valkyrie.rsw",
            "rwc01.rsw",
            "himinn.rsw",
            "airplane.rsw",
            "airplane01.rsw",
            "schgld.rsw",
            "bat_fild02.rsw",
            "que_qsch01.rsw",
            "que_qsch02.rsw",
            "que_qsch03.rsw",
            "que_qsch04.rsw",
            "que_qsch05.rsw",
            "que_qaru01.rsw",
            "que_qaru02.rsw",
            "que_qaru03.rsw",
            "que_qaru04.rsw",
            "que_qaru05.rsw",
            "bat_b01.rsw",
            "bat_b02.rsw",
        })) {
        return static_cast<int>(0xFF99CCFFu);
    }

    if (MatchMapName(mapName, {
            "gonryun.rsw",
            "gon_dun02.rsw",
            "ra_temsky.rsw",
            "que_temsky.rsw",
        })) {
        return static_cast<int>(0xFF6699CCu);
    }

    if (mapName == "thana_boss.rsw") {
        return static_cast<int>(0xFFE0D5C2u);
    }

    if (mapName == "5@tower.rsw" || mapName == "5tower.rsw") {
        return static_cast<int>(0xFF330033u);
    }

    return 0;
}

void ApplyRefFogForMap(const char* rswName, int fogOn)
{
    if (!fogOn) {
        g_renderer.FogSwitch(0);
        return;
    }

    SessionFogParameter parameter{};
    if (!g_session.GetFogParameter(rswName, &parameter)) {
        g_renderer.FogSwitch(0);
        return;
    }

    g_renderer.SetFogParameters(parameter.start, parameter.end, parameter.color, parameter.density);
    g_renderer.FogSwitch(1);
}

void LaunchMapWeatherEffect(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    CAbleToMakeEffect* player = dynamic_cast<CAbleToMakeEffect*>(mode.m_world->m_player);
    if (!player) {
        return;
    }

    if (!g_session.m_isEffectOn) {
        return;
    }

    if (CWeather::IsSnowMap(mode.m_rswName)) {
        g_Weather.LaunchSnow();
        return;
    }
    const int effectId = ResolveRefWeatherCloudEffectId(mode.m_rswName);
    if (effectId == 0) {
        return;
    }

    player->LaunchEffect(effectId, vector3d{}, 0.0f);
}

void UpdateMapAudio(CGameMode& mode)
{
    CAudio* audio = CAudio::GetInstance();
    if (!audio) {
        return;
    }

    if (mode.m_rswName[0] != '\0') {
        const std::string bgmPath = audio->ResolveMapBgmPath(mode.m_rswName);
        if (!bgmPath.empty() && bgmPath != mode.m_streamFileName) {
            mode.m_streamFileName = bgmPath;
            audio->PlayBGM(mode.m_streamFileName.c_str());
        }
    }

    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    C3dWorldRes* worldRes = g_resMgr.GetAs<C3dWorldRes>(mode.m_rswName);
    if (!worldRes) {
        return;
    }

    const vector3d listenerPos = mode.m_world->m_player->m_pos;
    const u32 now = GetTickCount();

    size_t soundIndex = 0;
    for (C3dWorldRes::soundSrcInfo* sound : worldRes->m_sounds) {
        if (!sound || sound->waveName[0] == '\0') {
            ++soundIndex;
            continue;
        }

        const std::string wavName = sound->waveName;
        const u32 cycleMs = static_cast<u32>(static_cast<s64>(sound->cycle * 1000.0f));
        PLAY_WAVE_INFO* playingState = nullptr;
        for (PLAY_WAVE_INFO& playing : mode.m_ambientWaveList) {
            if (playing.wavName == wavName && playing.nAID == static_cast<u32>(soundIndex)) {
                playingState = &playing;
                break;
            }
        }

        if (!playingState) {
            PLAY_WAVE_INFO info{};
            info.wavName = wavName;
            info.nAID = static_cast<u32>(soundIndex);
            info.term = cycleMs;
            info.endTick = now - cycleMs;
            info.pos = sound->pos;
            info.volumeFactor = sound->vol;
            info.volumeMaxDist = static_cast<int>(sound->range);
            info.volumeMinDist = static_cast<int>(sound->range * 0.16666667f);
            mode.m_ambientWaveList.push_back(info);
            playingState = &mode.m_ambientWaveList.back();
        }

        playingState->term = cycleMs;
        const int diagonalSquared = sound->width * sound->width + sound->height * sound->height;
        const float diagonal = std::sqrt(static_cast<float>(diagonalSquared));
        AmbientSoundTrigger trigger;
        if (!ResolveAmbientSoundTrigger(listenerPos.x, listenerPos.z,
                sound->pos.x, sound->pos.z, sound->width, sound->height,
                sound->range, diagonal, &trigger)) {
            ++soundIndex;
            continue;
        }

        if (now > playingState->endTick + playingState->term) {
            // Ref CSoundMaker::OnProcess stores the tick before attempting playback.
            playingState->endTick = GetTickCount();
            // Ref runtime-wave: 0x5D6A53. Name comes from this map RSW sound source.
            PlayWave(wavName.c_str(), trigger.relativeX, 0.0f, trigger.relativeZ,
                static_cast<int>(sound->range),
                static_cast<int>(sound->range * 0.16666667f),
                sound->vol);
        }

        ++soundIndex;
    }
}

void ResetGamePacketTrace(u32 startTick)
{
    g_packetTraceStartTick = startTick;
    g_packetTraceLoggedIds.clear();
}

void DrawHoveredActorName(CGameMode& mode, HDC hdc);
void DrawLockedTargetName(CGameMode& mode, HDC hdc);
void DrawLockedTargetArrow(CGameMode& mode, HDC hdc);
bool QueueLockedTargetOverlayQuad(CGameMode& mode);
bool QueueHoverLabelsOverlayQuad(CGameMode& mode);
bool QueueMsgEffectsOverlayQuad();
bool QueuePlayerVitalsOverlayQuad(CGameMode& mode);
bool QueueOwnedCompanionVitalsOverlayQuads(CGameMode& mode);
bool IsMapLoadingActive(const CGameMode& mode);
bool IsMonsterLikeHoverActor(const CGameActor* actor);
bool IsWalkableAttrCell(const CGameMode& mode, int tileX, int tileY);
bool IsAttackTargetWithinRange(CGameMode& mode, const CGameActor& target, int attackRange);
bool IsServerCellWithinSkillRangeOfActor(const CGameMode& mode, const CGameActor& target, int skillRange);
bool SendUseSkillToIdPacket(u16 skillId, u16 skillLevel, u32 targetGid);
void ClearAttackChaseHint(CGameMode& mode);
void ClearSkillChase(CGameMode& mode);
bool HasPendingSkillUse(const CGameMode& mode)
{
    return mode.m_skillUseInfo.useType!=0;
}
void ClearPendingSkillUse(CGameMode& mode);
void BeginSkillChase(CGameMode& mode, u32 targetGid, int skillId, int skillLevel, int skillRange);
bool TryRequestSourceSkillOnActor(CGameMode& mode,CGameActor* target,bool shift,bool alt,bool control);
void DrawPlayerVitalsOverlay(CGameMode& mode, HDC hdc);
void FillSolidRectArgb(unsigned int* pixels, int width, int height, const RECT& rect, COLORREF color);
void DrawGameplayOverlayToHdc(CGameMode& mode, HDC targetDc);
std::string ResolveHoveredActorName(CGameMode& mode, CGameActor* actor);
const char* UiKorPrefix();
std::string ResolveDataPath(const std::string& fileName, const char* ext, const std::vector<std::string>& directPrefixes);
double QpcNowMs();
bool BlitArgbBitsToWindow(HWND hwnd, const void* bits, int width, int height);

bool EnsureOverlayComposeSurface(int width, int height, ArgbDibSurface* composeSurface)
{
    return composeSurface && composeSurface->EnsureSize(width, height);
}

void ClearOverlayComposeBits(void* composeBits, int width, int height)
{
    if (!composeBits || width <= 0 || height <= 0) {
        return;
    }

    unsigned int* pixels = static_cast<unsigned int*>(composeBits);
    std::fill_n(pixels, static_cast<size_t>(width) * static_cast<size_t>(height), kOverlayTransparentKey);
}

void ConvertOverlayComposeBitsToAlpha(void* composeBits, int width, int height)
{
    if (!composeBits || width <= 0 || height <= 0) {
        return;
    }

    unsigned int* pixels = static_cast<unsigned int*>(composeBits);
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t index = 0; index < pixelCount; ++index) {
        const unsigned int rgb = pixels[index] & 0x00FFFFFFu;
        pixels[index] = (rgb == (kOverlayTransparentKey & 0x00FFFFFFu)) ? 0u : (0xFF000000u | rgb);
    }
}

struct BootstrapWorldCache;
COLORREF ResolveHoverNameColor(const CGameActor* actor);
float ClampUnit(float value);
const CAttrCell* GetAttrCellSafe(const SceneGraphNode& scene, int x, int y);
float GetTerrainCornerHeight(const SceneGraphNode& scene, int x, int y, int corner);
float GetTerrainAverageHeightAt(const SceneGraphNode& scene, int x, int y);
COLORREF GetGroundTextureColor(const SceneGraphNode& scene, const BootstrapWorldCache& cache, int x, int y);
COLORREF ColorFromVector(const vector3d& value, float scale);
COLORREF BlendColor(COLORREF a, COLORREF b, float amount);
COLORREF MultiplyColor(COLORREF color, float brightness);
COLORREF ModulateColor(COLORREF color, const vector3d& lightColor);
vector3d ComputeLightingColor(const vector3d& lightDir, const vector3d& diffuseCol, const vector3d& ambientCol, const vector3d& normal);
vector3d ComputeCellNormal(float h1, float h2, float h3, float h4);
POINT ProjectWorldPoint(const RECT& viewRect,
    float localX,
    float localY,
    float localHeight,
    float tileWidth,
    float tileHeight,
    float heightScale);

#if RO_ENABLE_QT6_UI
QFont BuildOverlayLabelFont()
{
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(kHoverNameFontHeight);
    font.setBold(kHoverNameFontBold != 0);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

QColor ToQColor(COLORREF color)
{
    return QColor(GetRValue(color), GetGValue(color), GetBValue(color));
}

bool MeasureOverlayTextQt(const std::string& text, SIZE* outSize)
{
    if (!outSize || text.empty()) {
        return false;
    }

    const QString label = QString::fromLocal8Bit(text.c_str(), static_cast<int>(text.size()));
    if (label.isEmpty()) {
        return false;
    }

    const QFontMetrics metrics(BuildOverlayLabelFont());
    outSize->cx = (std::max)(1, metrics.horizontalAdvance(label));
    outSize->cy = (std::max)(1, metrics.height());
    return true;
}

void DrawOutlinedTextQtToOverlay(
    unsigned int* pixels,
    int width,
    int height,
    int pitch,
    int drawX,
    int drawY,
    const std::string& text,
    COLORREF color)
{
    if (!pixels || width <= 0 || height <= 0 || pitch < width * static_cast<int>(sizeof(unsigned int)) || text.empty()) {
        return;
    }

    QImage image(reinterpret_cast<uchar*>(pixels), width, height, pitch, QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text.c_str(), static_cast<int>(text.size()));
    if (label.isEmpty()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildOverlayLabelFont());

    const QFontMetrics metrics(painter.font());
    const int baselineY = drawY + metrics.ascent();
    painter.setPen(Qt::black);
    painter.drawText(drawX - 1, baselineY, label);
    painter.drawText(drawX + 1, baselineY, label);
    painter.drawText(drawX, baselineY - 1, label);
    painter.drawText(drawX, baselineY + 1, label);
    painter.setPen(ToQColor(color));
    painter.drawText(drawX, baselineY, label);
}

QFont BuildBootstrapHeaderFont()
{
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(15);
    font.setBold(true);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

QFont BuildBootstrapBodyFont()
{
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(12);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawBootstrapSceneTextQt(unsigned int* pixels,
    int width,
    int height,
    const CGameMode& mode,
    const BootstrapWorldCache& cache,
    int attrWidth,
    int attrHeight,
    int groundWidth,
    int groundHeight,
    int activeWidth,
    int activeHeight);
#else
void DrawOutlinedTextQtToOverlay(
    unsigned int*,
    int,
    int,
    int,
    int,
    int,
    const std::string&,
    COLORREF)
{
}
#endif

inline unsigned int PackOverlayRgb(COLORREF color)
{
    return (static_cast<unsigned int>(GetRValue(color)) << 16)
        | (static_cast<unsigned int>(GetGValue(color)) << 8)
        | static_cast<unsigned int>(GetBValue(color));
}

void PutOverlayPixel(unsigned int* pixels, int width, int height, int x, int y, COLORREF color)
{
    if (!pixels || x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }

    pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = PackOverlayRgb(color);
}

bool PointInTriangle(const POINT& p, const POINT& a, const POINT& b, const POINT& c)
{
    const auto sign = [](const POINT& p0, const POINT& p1, const POINT& p2) -> long {
        return static_cast<long>(p0.x - p2.x) * static_cast<long>(p1.y - p2.y)
            - static_cast<long>(p1.x - p2.x) * static_cast<long>(p0.y - p2.y);
    };

    const long d1 = sign(p, a, b);
    const long d2 = sign(p, b, c);
    const long d3 = sign(p, c, a);
    const bool hasNegative = d1 < 0 || d2 < 0 || d3 < 0;
    const bool hasPositive = d1 > 0 || d2 > 0 || d3 > 0;
    return !(hasNegative && hasPositive);
}

void FillOverlayTriangle(unsigned int* pixels, int width, int height, const POINT& a, const POINT& b, const POINT& c, COLORREF color)
{
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    const int minX = (std::max)(0, static_cast<int>((std::min)({ a.x, b.x, c.x })));
    const int maxX = (std::min)(width - 1, static_cast<int>((std::max)({ a.x, b.x, c.x })));
    const int minY = (std::max)(0, static_cast<int>((std::min)({ a.y, b.y, c.y })));
    const int maxY = (std::min)(height - 1, static_cast<int>((std::max)({ a.y, b.y, c.y })));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const POINT p{ x, y };
            if (PointInTriangle(p, a, b, c)) {
                PutOverlayPixel(pixels, width, height, x, y, color);
            }
        }
    }
}

void DrawFallbackLockedTargetArrowToPixels(unsigned int* pixels, int width, int height, int centerX, int tipY)
{
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    const POINT outerA{ centerX, tipY };
    const POINT outerB{ centerX - 7, tipY - 11 };
    const POINT outerC{ centerX + 7, tipY - 11 };
    const POINT innerA{ centerX, tipY - 1 };
    const POINT innerB{ centerX - 4, tipY - 9 };
    const POINT innerC{ centerX + 4, tipY - 9 };

    FillOverlayTriangle(pixels, width, height, outerA, outerB, outerC, RGB(255, 226, 120));
    FillOverlayTriangle(pixels, width, height, innerA, innerB, innerC, RGB(255, 92, 92));

    const COLORREF outline = RGB(32, 16, 16);
    const POINT outlinePoints[] = {
        outerA, outerB, outerC
    };
    for (const POINT& point : outlinePoints) {
        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                PutOverlayPixel(pixels, width, height, point.x + offsetX, point.y + offsetY, outline);
            }
        }
    }
}

void ApplyRoundedOverlayMask(void* composeBits, int width, int height, int ellipseWidth, int ellipseHeight)
{
    if (!composeBits || width <= 0 || height <= 0 || ellipseWidth <= 0 || ellipseHeight <= 0) {
        return;
    }

    HRGN clipRgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, ellipseWidth, ellipseHeight);
    if (!clipRgn) {
        return;
    }

    unsigned int* pixels = static_cast<unsigned int*>(composeBits);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!PtInRegion(clipRgn, x, y)) {
                pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = kOverlayTransparentKey;
            }
        }
    }

    DeleteObject(clipRgn);
}

void HashTokenValue(std::uint64_t* hash, std::uint64_t value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(std::uint64_t* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<std::uint64_t>(ch));
    }
    HashTokenValue(hash, 0xFFull);
}

std::string ResolveGroundItemHoverLabel(const CItem* item);
void DrawHoveredGroundItemName(CGameMode& mode, HDC hdc);

bool IsWorldHoverBlockedByUi(int screenX, int screenY)
{
    const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(screenX) : screenX;
    const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(screenY) : screenY;
    return g_windowMgr.HasActiveNpcDialog() || g_windowMgr.HasWindowAtPoint(uiX, uiY);
}

std::uint64_t ComputeGameplayOverlayStateToken(
    CGameMode& mode,
    int cursorActNum,
    u32 mouseAnimStartTick,
    int clientWidth,
    int clientHeight,
    bool includeWorldHover)
{
    std::uint64_t hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<std::uint64_t>(clientWidth));
    HashTokenValue(&hash, static_cast<std::uint64_t>(clientHeight));
    (void)cursorActNum;
    (void)mouseAnimStartTick;

    if (mode.m_world && mode.m_view && mode.m_world->m_player) {
        const CPlayer* player = mode.m_world->m_player;
        HashTokenValue(&hash, static_cast<std::uint64_t>(player->m_gid));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_Hp)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_MaxHp)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_Sp)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_MaxSp)));

        const u32 statusNow = g_session.GetServerTime();
        const std::vector<ACTIVE_STATUS_ICON>& activeStatusIcons = g_session.GetActiveStatusIcons();
        HashTokenValue(&hash, static_cast<std::uint64_t>(activeStatusIcons.size()));
        for (const ACTIVE_STATUS_ICON& statusIcon : activeStatusIcons) {
            HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(statusIcon.statusType)));
            HashTokenValue(&hash, statusIcon.hasTimer ? 1ull : 0ull);
            if (statusIcon.hasTimer) {
                const u32 remainingMs = statusIcon.expireServerTime > statusNow
                    ? (statusIcon.expireServerTime - statusNow)
                    : 0u;
                HashTokenValue(&hash, static_cast<std::uint64_t>(remainingMs / 1000u));
            }
        }

        if (includeWorldHover && !IsWorldHoverBlockedByUi(mode.m_oldMouseX, mode.m_oldMouseY)) {
            CItem* hoveredGroundItem = nullptr;
            int hoveredItemLabelX = 0;
            int hoveredItemLabelY = 0;
            if (mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
                mode.m_oldMouseX,
                mode.m_oldMouseY,
                &hoveredGroundItem,
                &hoveredItemLabelX,
                &hoveredItemLabelY)
                && hoveredGroundItem) {
                HashTokenValue(&hash, static_cast<std::uint64_t>(hoveredGroundItem->m_aid));
                HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(hoveredItemLabelX)));
                HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(hoveredItemLabelY)));
                HashTokenString(&hash, ResolveGroundItemHoverLabel(hoveredGroundItem));
            }

            CGameActor* hoveredActor = nullptr;
            int hoveredLabelX = 0;
            int hoveredLabelY = 0;
            if (!hoveredGroundItem
                && mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
                mode.m_view->GetCameraLongitude(),
                mode.m_oldMouseX,
                mode.m_oldMouseY,
                &hoveredActor,
                &hoveredLabelX,
                &hoveredLabelY)
                && hoveredActor
                && hoveredActor->m_gid != mode.m_lastLockOnMonGid) {
                HashTokenValue(&hash, static_cast<std::uint64_t>(hoveredActor->m_gid));
                HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(hoveredLabelX)));
                HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(hoveredLabelY)));
                HashTokenString(&hash, ResolveHoveredActorName(mode, hoveredActor));
            }
        }
    }

    HashTokenValue(&hash, static_cast<std::uint64_t>(mode.m_lastLockOnMonGid));
    return hash;
}

void SyncRendererToWindowSize()
{
    GetRenderDevice().RefreshRenderSize();
    const int renderWidth = GetRenderDevice().GetRenderWidth();
    const int renderHeight = GetRenderDevice().GetRenderHeight();
    if (renderWidth <= 0 || renderHeight <= 0) {
        return;
    }

    if (g_renderer.m_width != renderWidth || g_renderer.m_height != renderHeight) {
        g_renderer.SetSize(renderWidth, renderHeight);
    }
}

#if RO_PLATFORM_WINDOWS
RECT GroundPromptLogicalViewport(RECT client, bool scaled)
{
    if (!scaled) return client;
    return {0,0,UiScaleRawToLogicalCoordinate(client.right),UiScaleRawToLogicalCoordinate(client.bottom)};
}

RECT GroundPromptScreenBounds(RECT bounds, bool scaled)
{
    if (!scaled) return bounds;
    return {UiScaleLogicalToRawCoordinate(bounds.left),UiScaleLogicalToRawCoordinate(bounds.top),
        UiScaleLogicalToRawCoordinate(bounds.right),UiScaleLogicalToRawCoordinate(bounds.bottom)};
}

bool QueueGroundSkillPromptOverlayQuad(RECT client, bool scaled)
{
    static ArgbDibSurface surface;
    RECT bounds{};
    if (!g_windowMgr.BuildGroundSkillPromptOverlay(surface, GroundPromptLogicalViewport(client, scaled),
            timeGetTime(), bounds)) return false;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return true;

    static CTexture* texture = nullptr;
    static int textureWidth = 0;
    static int textureHeight = 0;
    if (!texture || textureWidth != width || textureHeight != height) {
        delete texture;
        texture = new CTexture();
        if (!texture->Create(width, height, PF_A8R8G8B8, false)) {
            delete texture;
            texture = nullptr;
            return false;
        }
        textureWidth = width;
        textureHeight = height;
    }
    if (!GetRenderDevice().UpdateTextureResource(texture, 0, 0, width, height, surface.GetPixels(),
            true, width * static_cast<int>(sizeof(unsigned)))) return false;

    auto* face = g_renderer.BorrowNullRP();
    if (!face) return false;
    const auto screen = GroundPromptScreenBounds(bounds, scaled);
    const float left = OverlayScreenEdge(screen.left);
    const float top = OverlayScreenEdge(screen.top);
    const float right = OverlayScreenEdge(screen.right);
    const float bottom = OverlayScreenEdge(screen.bottom);
    const float maxU = static_cast<float>(width) / texture->m_w;
    const float maxV = static_cast<float>(height) / texture->m_h;
    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_ONE;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = g_windowMgr.HasGameplayInputModal() ? 0.99f : 1.75f;
    face->m_verts[0] = {left,top,0.0f,1.0f,0xFFFFFFFFu,0xFF000000u,0.0f,0.0f};
    face->m_verts[1] = {right,top,0.0f,1.0f,0xFFFFFFFFu,0xFF000000u,maxU,0.0f};
    face->m_verts[2] = {left,bottom,0.0f,1.0f,0xFFFFFFFFu,0xFF000000u,0.0f,maxV};
    face->m_verts[3] = {right,bottom,0.0f,1.0f,0xFFFFFFFFu,0xFF000000u,maxU,maxV};
    g_renderer.AddRP(face, 1 | 8);
    return true;
}
#endif

bool QueueModernOverlayQuad(CGameMode& mode, int cursorActNum, u32 mouseAnimStartTick)
{
    const bool trackMovePerf = mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_isMoving;
    const bool mapLoadingActive = IsMapLoadingActive(mode);

    if (!g_hMainWnd) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return false;
    }

    static ArgbDibSurface s_overlayComposeSurface;
    static std::vector<unsigned int> s_qtOverlayComposePixels;
    static std::uint64_t s_overlayStateToken = 0ull;
    static bool s_overlayTextureValid = false;
    static u32 s_lastMovingOverlayRefreshTick = 0;
    const bool qtRuntimeEnabled = IsQtUiRuntimeEnabled();

    static CTexture* s_overlayTexture = nullptr;
    static int s_overlayTextureWidth = 0;
    static int s_overlayTextureHeight = 0;
    static CTexture* s_qtOverlayTexture = nullptr;
    static int s_qtOverlayTextureWidth = 0;
    static int s_qtOverlayTextureHeight = 0;
    static bool s_qtOverlayTextureValid = false;
    if (!s_overlayTexture || s_overlayTextureWidth != clientWidth || s_overlayTextureHeight != clientHeight) {
        delete s_overlayTexture;
        s_overlayTexture = new CTexture();
        if (!s_overlayTexture || !s_overlayTexture->Create(clientWidth, clientHeight, PF_A8R8G8B8, false)) {
            delete s_overlayTexture;
            s_overlayTexture = nullptr;
            s_overlayTextureWidth = 0;
            s_overlayTextureHeight = 0;
            return false;
        }
        s_overlayTextureWidth = clientWidth;
        s_overlayTextureHeight = clientHeight;
        s_overlayTextureValid = false;
        s_overlayStateToken = 0ull;
    }
    if (!s_qtOverlayTexture || s_qtOverlayTextureWidth != clientWidth || s_qtOverlayTextureHeight != clientHeight) {
        delete s_qtOverlayTexture;
        s_qtOverlayTexture = new CTexture();
        if (!s_qtOverlayTexture || !s_qtOverlayTexture->Create(clientWidth, clientHeight, PF_A8R8G8B8, false)) {
            delete s_qtOverlayTexture;
            s_qtOverlayTexture = nullptr;
            s_qtOverlayTextureWidth = 0;
            s_qtOverlayTextureHeight = 0;
            s_qtOverlayTextureValid = false;
        } else {
            s_qtOverlayTextureWidth = clientWidth;
            s_qtOverlayTextureHeight = clientHeight;
            s_qtOverlayTextureValid = false;
        }
    }

    const bool overlayIsAnimated = qtRuntimeEnabled && QtUiRuntimeNeedsRender();
    const bool uiDirty = qtRuntimeEnabled
        ? g_windowMgr.HasDirtyVisualState()
        : g_windowMgr.HasDirtyVisualStateExcludingRoMap();
    std::uint64_t overlayStateToken = 0ull;
    if (!mapLoadingActive) {
        const bool includeWorldHoverInOverlayToken = !qtRuntimeEnabled;
        overlayStateToken = ComputeGameplayOverlayStateToken(
            mode,
            cursorActNum,
            mouseAnimStartTick,
            clientWidth,
            clientHeight,
            includeWorldHoverInOverlayToken);
    }
    const bool overlayStateChanged = overlayStateToken != s_overlayStateToken;
    const u32 now = GetTickCount();
    const bool allowMovingOverlayRefresh = !trackMovePerf
        || s_lastMovingOverlayRefreshTick == 0
        || now - s_lastMovingOverlayRefreshTick >= kMovingOverlayRefreshMs;
    const bool needOverlayRefresh = !s_overlayTextureValid
        || overlayIsAnimated
        || uiDirty
        || (overlayStateChanged && allowMovingOverlayRefresh);

    if (needOverlayRefresh) {
        const double refreshStartMs = trackMovePerf ? QpcNowMs() : 0.0;
        const bool allowQtCpuBridge = qtRuntimeEnabled
            && GetRenderDevice().GetBackendType() != RenderBackendType::Vulkan;
        s_qtOverlayTextureValid = qtRuntimeEnabled
            && s_qtOverlayTexture
            && (mapLoadingActive
                ? RenderQtUiMenuOverlayTexture(s_qtOverlayTexture, clientWidth, clientHeight)
                : RenderQtUiGameplayOverlayTexture(mode, s_qtOverlayTexture, clientWidth, clientHeight));

        {
            static int s_lastLoggedGameplayPath = -1;
            const int gameplayPath = mapLoadingActive
                ? (s_qtOverlayTextureValid ? 12 : (allowQtCpuBridge ? 11 : 10))
                : (s_qtOverlayTextureValid ? 2 : (allowQtCpuBridge ? 1 : 0));
            if (gameplayPath != s_lastLoggedGameplayPath) {
                DbgLog("[GameMode] %s overlay path=%s qtEnabled=%d texture=%p size=%dx%d\n",
                    mapLoadingActive ? "loading" : "gameplay",
                    s_qtOverlayTextureValid ? "native_texture" : (allowQtCpuBridge ? "cpu_bridge" : "legacy_gdi"),
                    qtRuntimeEnabled ? 1 : 0,
                    s_qtOverlayTexture,
                    clientWidth,
                    clientHeight);
                s_lastLoggedGameplayPath = gameplayPath;
            }
        }

        if (s_qtOverlayTextureValid) {
            g_windowMgr.ClearDirtyVisualState();
            s_overlayTextureValid = false;
        } else {
            if (!allowQtCpuBridge) {
                const bool composeReady = EnsureOverlayComposeSurface(clientWidth, clientHeight, &s_overlayComposeSurface);
                if (!composeReady) {
                    return false;
                }

                ClearOverlayComposeBits(s_overlayComposeSurface.GetBits(), clientWidth, clientHeight);
                const double overlayDrawStartMs = trackMovePerf ? QpcNowMs() : 0.0;
#if !RO_ENABLE_QT6_UI
                DrawGameplayOverlayToHdc(mode, s_overlayComposeSurface.GetDC());
#endif
                if (trackMovePerf) {
                    g_overlayMovePerfStats.modernOverlayDrawMs += QpcNowMs() - overlayDrawStartMs;
                }
            }

            const double uiDrawStartMs = trackMovePerf ? QpcNowMs() : 0.0;
            if (!allowQtCpuBridge) {
                g_windowMgr.OnDrawExcludingRoMapToHdc(s_overlayComposeSurface.GetDC(), !RO_PLATFORM_WINDOWS);
            } else {
                g_windowMgr.ClearDirtyVisualState();
            }
            if (trackMovePerf) {
                g_overlayMovePerfStats.modernUiDrawMs += QpcNowMs() - uiDrawStartMs;
            }

            unsigned int* overlayPixels = nullptr;
            if (allowQtCpuBridge) {
                const size_t pixelCount = static_cast<size_t>(clientWidth) * static_cast<size_t>(clientHeight);
                if (s_qtOverlayComposePixels.size() != pixelCount) {
                    s_qtOverlayComposePixels.assign(pixelCount, 0u);
                } else {
                    std::fill(s_qtOverlayComposePixels.begin(), s_qtOverlayComposePixels.end(), 0u);
                }
                overlayPixels = s_qtOverlayComposePixels.data();
            } else {
                const double convertStartMs = trackMovePerf ? QpcNowMs() : 0.0;
                ConvertOverlayComposeBitsToAlpha(s_overlayComposeSurface.GetBits(), clientWidth, clientHeight);
                if (trackMovePerf) {
                    g_overlayMovePerfStats.modernConvertMs += QpcNowMs() - convertStartMs;
                }
                overlayPixels = s_overlayComposeSurface.GetPixels();
            }
            if (allowQtCpuBridge) {
                if (mapLoadingActive) {
                    CompositeQtUiMenuOverlay(
                        overlayPixels,
                        clientWidth,
                        clientHeight,
                        clientWidth * static_cast<int>(sizeof(unsigned int)));
                } else {
                    CompositeQtUiGameplayOverlay(mode,
                        overlayPixels,
                        clientWidth,
                        clientHeight,
                        clientWidth * static_cast<int>(sizeof(unsigned int)));
                }
            }

            const double textureUpdateStartMs = trackMovePerf ? QpcNowMs() : 0.0;
            s_overlayTexture->Update(0,
                0,
                clientWidth,
                clientHeight,
                overlayPixels,
                true,
                clientWidth * static_cast<int>(sizeof(unsigned int)));
            if (trackMovePerf) {
                g_overlayMovePerfStats.modernTextureUpdateMs += QpcNowMs() - textureUpdateStartMs;
            }
            s_overlayTextureValid = true;
        }
        if (trackMovePerf) {
            g_overlayMovePerfStats.modernRefreshes += 1;
            (void)refreshStartMs;
        }
        s_overlayStateToken = overlayStateToken;
        if (overlayStateChanged) {
            s_lastMovingOverlayRefreshTick = now;
        }
    }

    if (!s_overlayTextureValid && !s_qtOverlayTextureValid) {
        return false;
    }

    const float left = OverlayScreenEdge(0);
    const float top = OverlayScreenEdge(0);
    const float right = OverlayScreenEdge(clientWidth);
    const float bottom = OverlayScreenEdge(clientHeight);
    if (s_overlayTextureValid) {
        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return false;
        }

        const unsigned int overlayContentWidth = s_overlayTexture->m_surfaceUpdateWidth > 0 ? s_overlayTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(clientWidth);
        const unsigned int overlayContentHeight = s_overlayTexture->m_surfaceUpdateHeight > 0 ? s_overlayTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(clientHeight);
        const float maxU = s_overlayTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_overlayTexture->m_w) : 1.0f;
        const float maxV = s_overlayTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_overlayTexture->m_h) : 1.0f;

        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts;
        face->numVerts = 4;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = s_overlayTexture;
        face->mtPreset = 3;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        face->alphaSortKey = 1.0f;

        face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
        face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
        face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
        face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
        g_renderer.AddRP(face, 1 | 8);
    }

    if (s_qtOverlayTextureValid) {
        RPFace* qtFace = g_renderer.BorrowNullRP();
        if (!qtFace) {
            return true;
        }

        const unsigned int qtOverlayContentWidth = s_qtOverlayTexture->m_surfaceUpdateWidth > 0 ? s_qtOverlayTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(clientWidth);
        const unsigned int qtOverlayContentHeight = s_qtOverlayTexture->m_surfaceUpdateHeight > 0 ? s_qtOverlayTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(clientHeight);
        const float qtMaxU = s_qtOverlayTexture->m_w != 0 ? static_cast<float>(qtOverlayContentWidth) / static_cast<float>(s_qtOverlayTexture->m_w) : 1.0f;
        const float qtMaxV = s_qtOverlayTexture->m_h != 0 ? static_cast<float>(qtOverlayContentHeight) / static_cast<float>(s_qtOverlayTexture->m_h) : 1.0f;

        qtFace->primType = D3DPT_TRIANGLESTRIP;
        qtFace->verts = qtFace->m_verts;
        qtFace->numVerts = 4;
        qtFace->indices = nullptr;
        qtFace->numIndices = 0;
        qtFace->tex = s_qtOverlayTexture;
        qtFace->mtPreset = 3;
        qtFace->cullMode = D3DCULL_NONE;
        qtFace->srcAlphaMode = D3DBLEND_SRCALPHA;
        qtFace->destAlphaMode = D3DBLEND_INVSRCALPHA;
        qtFace->alphaSortKey = 1.0f;
        qtFace->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
        qtFace->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, qtMaxU, 0.0f };
        qtFace->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, qtMaxV };
        qtFace->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, qtMaxU, qtMaxV };
        g_renderer.AddRP(qtFace, 1 | 8);
    }
#if RO_PLATFORM_WINDOWS
    if (!mapLoadingActive) return QueueGroundSkillPromptOverlayQuad(clientRect, qtRuntimeEnabled);
#endif
    return true;
}

bool QueueRoMapOverlayQuad()
{
    if (!g_hMainWnd) {
        return false;
    }

    RECT mapRect{};
    if (!g_windowMgr.GetRoMapRect(&mapRect)) {
        return false;
    }

    const int width = mapRect.right - mapRect.left;
    const int height = mapRect.bottom - mapRect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    static ArgbDibSurface s_roMapComposeSurface;
    static std::vector<unsigned int> s_roMapComposePixels;
    static bool s_roMapTextureValid = false;

    static CTexture* s_roMapTexture = nullptr;
    static int s_roMapTextureWidth = 0;
    static int s_roMapTextureHeight = 0;
    if (!s_roMapTexture || s_roMapTextureWidth != width || s_roMapTextureHeight != height) {
        delete s_roMapTexture;
        s_roMapTexture = new CTexture();
        if (!s_roMapTexture || !s_roMapTexture->Create(width, height, PF_A8R8G8B8, false)) {
            delete s_roMapTexture;
            s_roMapTexture = nullptr;
            s_roMapTextureWidth = 0;
            s_roMapTextureHeight = 0;
            return false;
        }
        s_roMapTextureWidth = width;
        s_roMapTextureHeight = height;
        s_roMapTextureValid = false;
    }

    const bool needRoMapRefresh = !s_roMapTextureValid || g_windowMgr.HasRoMapDirtyVisualState();
    if (needRoMapRefresh) {
        unsigned int* uploadPixels = nullptr;
#if RO_ENABLE_QT6_UI
        bool builtWithQtImage = false;
        if (g_windowMgr.m_roMapWnd) {
            QImage overlayImage;
            if (g_windowMgr.m_roMapWnd->BuildOverlayImageForRenderer(&overlayImage) && !overlayImage.isNull()) {
                const QImage straightImage = overlayImage.convertToFormat(QImage::Format_ARGB32);
                const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
                if (s_roMapComposePixels.size() != pixelCount) {
                    s_roMapComposePixels.resize(pixelCount);
                }

                for (int y = 0; y < height; ++y) {
                    const unsigned int* srcRow = reinterpret_cast<const unsigned int*>(straightImage.constScanLine(y));
                    unsigned int* dstRow = s_roMapComposePixels.data() + static_cast<size_t>(y) * static_cast<size_t>(width);
                    std::memcpy(dstRow, srcRow, static_cast<size_t>(width) * sizeof(unsigned int));
                }

                uploadPixels = s_roMapComposePixels.data();
                builtWithQtImage = true;
            }
        }
        if (!builtWithQtImage)
#endif
        {
            const bool composeReady = EnsureOverlayComposeSurface(width, height, &s_roMapComposeSurface);
            if (!composeReady) {
                return false;
            }

            ClearOverlayComposeBits(s_roMapComposeSurface.GetBits(), width, height);
            if (!g_windowMgr.DrawRoMapToHdc(s_roMapComposeSurface.GetDC(), 0, 0)) {
                return false;
            }
            ApplyRoundedOverlayMask(s_roMapComposeSurface.GetBits(), width, height, kRoMapCornerEllipseSize, kRoMapCornerEllipseSize);
            ConvertOverlayComposeBitsToAlpha(s_roMapComposeSurface.GetBits(), width, height);
            uploadPixels = s_roMapComposeSurface.GetPixels();
        }

        s_roMapTexture->Update(0,
            0,
            width,
            height,
            uploadPixels,
            true,
            width * static_cast<int>(sizeof(unsigned int)));
        s_roMapTextureValid = true;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(mapRect.left);
    const float top = OverlayScreenEdge(mapRect.top);
    const float right = OverlayScreenEdge(mapRect.right);
    const float bottom = OverlayScreenEdge(mapRect.bottom);
    const unsigned int overlayContentWidth = s_roMapTexture->m_surfaceUpdateWidth > 0 ? s_roMapTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = s_roMapTexture->m_surfaceUpdateHeight > 0 ? s_roMapTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = s_roMapTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_roMapTexture->m_w) : 1.0f;
    const float maxV = s_roMapTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_roMapTexture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_roMapTexture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0.5f;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueCursorOverlayQuad(int cursorActNum, u32 mouseAnimStartTick)
{
    if (!g_hMainWnd) {
        return false;
    }

    POINT cursorPos{};
    if (!GetModeCursorClientPos(&cursorPos)) {
        return false;
    }

    RECT cursorBounds{};
    const bool hasCustomBounds = GetModeCursorDrawBounds(cursorActNum, mouseAnimStartTick, &cursorBounds);
    const int textureWidth = hasCustomBounds ? (std::max)(1, static_cast<int>(cursorBounds.right - cursorBounds.left)) : 32;
    const int textureHeight = hasCustomBounds ? (std::max)(1, static_cast<int>(cursorBounds.bottom - cursorBounds.top)) : 32;
    const int drawOriginX = hasCustomBounds ? -(std::min)(0, static_cast<int>(cursorBounds.left)) : 0;
    const int drawOriginY = hasCustomBounds ? -(std::min)(0, static_cast<int>(cursorBounds.top)) : 0;
    const int left = hasCustomBounds ? cursorPos.x + (std::min)(0, static_cast<int>(cursorBounds.left)) : cursorPos.x;
    const int top = hasCustomBounds ? cursorPos.y + (std::min)(0, static_cast<int>(cursorBounds.top)) : cursorPos.y;
    static std::vector<unsigned int> s_cursorComposePixels;
    static bool s_cursorTextureValid = false;
    static CTexture* s_cursorTexture = nullptr;
    static int s_cursorTextureWidth = 0;
    static int s_cursorTextureHeight = 0;
    static std::uint64_t s_cursorStateToken = 0ull;
    if (!s_cursorTexture || s_cursorTextureWidth != textureWidth || s_cursorTextureHeight != textureHeight) {
        delete s_cursorTexture;
        s_cursorTexture = new CTexture();
        if (!s_cursorTexture || !s_cursorTexture->Create(textureWidth, textureHeight, PF_A8R8G8B8, false)) {
            delete s_cursorTexture;
            s_cursorTexture = nullptr;
            s_cursorTextureWidth = 0;
            s_cursorTextureHeight = 0;
            return false;
        }
        SetTextureDebugName(s_cursorTexture, "__overlay_cursor__");
        s_cursorTextureWidth = textureWidth;
        s_cursorTextureHeight = textureHeight;
        s_cursorComposePixels.assign(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight), 0u);
        s_cursorTextureValid = false;
        s_cursorStateToken = 0ull;
    } else if (s_cursorComposePixels.size() != static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight)) {
        s_cursorComposePixels.assign(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight), 0u);
    }

    std::uint64_t cursorStateToken = 1469598103934665603ull;
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(cursorActNum));
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(GetModeCursorVisualFrame(cursorActNum, mouseAnimStartTick)));
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(static_cast<unsigned int>(textureWidth)));
    HashTokenValue(&cursorStateToken, static_cast<std::uint64_t>(static_cast<unsigned int>(textureHeight)));

    if (!s_cursorTextureValid || cursorStateToken != s_cursorStateToken) {
        std::fill(s_cursorComposePixels.begin(), s_cursorComposePixels.end(), 0u);
        if (!DrawModeCursorAtToArgb(
            s_cursorComposePixels.data(),
            textureWidth,
            textureHeight,
            drawOriginX,
            drawOriginY,
            cursorActNum,
            mouseAnimStartTick)) {
            return false;
        }
        s_cursorTexture->Update(0,
            0,
            textureWidth,
            textureHeight,
            s_cursorComposePixels.data(),
            true,
            textureWidth * static_cast<int>(sizeof(unsigned int)));
        s_cursorTextureValid = true;
        s_cursorStateToken = cursorStateToken;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float quadLeft = static_cast<float>(left);
    const float quadTop = static_cast<float>(top);
    const float right = static_cast<float>(left + textureWidth);
    const float bottom = static_cast<float>(top + textureHeight);
    const unsigned int overlayContentWidth = s_cursorTexture->m_surfaceUpdateWidth > 0 ? s_cursorTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(textureWidth);
    const unsigned int overlayContentHeight = s_cursorTexture->m_surfaceUpdateHeight > 0 ? s_cursorTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(textureHeight);
    const float maxU = s_cursorTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_cursorTexture->m_w) : 1.0f;
    const float maxV = s_cursorTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_cursorTexture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_cursorTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 2.0f;

    face->m_verts[0] = { quadLeft, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, quadTop, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { quadLeft, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueLockedTargetOverlayQuad(CGameMode& mode)
{
    if (IsQtUiRuntimeEnabled()) {
        return false;
    }
    if (!g_hMainWnd || !mode.m_world || !mode.m_view || mode.m_lastLockOnMonGid == 0) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);

    auto actorIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
    if (actorIt == mode.m_runtimeActors.end() || !actorIt->second || !actorIt->second->m_isVisible) {
        return false;
    }

    int centerX = 0;
    int labelY = 0;
    if (!mode.m_world->GetActorScreenMarker(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        mode.m_lastLockOnMonGid,
        &centerX,
        nullptr,
        &labelY)) {
        return false;
    }

    RECT overlayRect{};
    SetRectEmpty(&overlayRect);
    RECT textRect{};
    SetRectEmpty(&textRect);

    const std::string label = ResolveHoveredActorName(mode, actorIt->second);
    const bool drawLockedTargetText = !label.empty();
    const COLORREF lockedTargetTextColor = ResolveHoverNameColor(actorIt->second);
    SIZE textSize{};
    int textX = 0;
    int textY = 0;
    if (drawLockedTargetText) {
#if RO_ENABLE_QT6_UI
        if (!MeasureOverlayTextQt(label, &textSize)) {
            return false;
        }
#else
        static ArgbDibSurface s_measureSurface;
        if (!s_measureSurface.EnsureSize(1, 1)) {
            return false;
        }
        DrawDC drawDc(s_measureSurface.GetDC());
        drawDc.SetFont(FONT_DEFAULT, kHoverNameFontHeight, kHoverNameFontBold);
        drawDc.GetTextExtentPoint32A(label.c_str(), static_cast<int>(label.size()), &textSize);
#endif
        textX = centerX - (textSize.cx / 2);
        textY = labelY + kHoverNameTextPadding + kHoverNameVerticalOffset;
        textRect = { textX - 2, textY - 2, textX + textSize.cx + 2, textY + textSize.cy + 2 };
        UnionRect(&overlayRect, &overlayRect, &textRect);
    }

    static bool s_arrowPixelsLoaded = false;
    static std::vector<unsigned int> s_arrowPixels;
    static int s_arrowWidth = 0;
    static int s_arrowHeight = 0;
    static CTexture* s_targetArrowTexture = nullptr;
    if (!s_arrowPixelsLoaded) {
        s_arrowPixelsLoaded = true;
        std::string bitmapPath = ResolveDataPath(kLockedTargetArrowBitmapName, "bmp", {
            "",
            std::string(UiKorPrefix()),
            "texture\\",
            std::string(UiKorPrefix()) + "basic_interface\\",
            "data\\",
            "data\\texture\\",
            std::string("data\\") + UiKorPrefix(),
            std::string("data\\") + UiKorPrefix() + "basic_interface\\"
        });
        if (!bitmapPath.empty()) {
            u32* pixels = nullptr;
            if (LoadBgraPixelsFromGameData(bitmapPath.c_str(), &pixels, &s_arrowWidth, &s_arrowHeight)
                && pixels
                && s_arrowWidth > 0
                && s_arrowHeight > 0) {
                s_arrowPixels.assign(
                    pixels,
                    pixels + static_cast<size_t>(s_arrowWidth) * static_cast<size_t>(s_arrowHeight));
            }
            delete[] pixels;
        }
    }

    const u32 now = GetTickCount();
    const int bounce = static_cast<int>(std::lround((0.5f + 0.5f * std::sin(static_cast<float>(now) * kLockedTargetArrowBouncePerMs))
        * kLockedTargetArrowBouncePixels));
    RECT arrowRect{};
    const bool hasArrowBitmap = !s_arrowPixels.empty() && s_arrowWidth > 0 && s_arrowHeight > 0;
    int arrowDrawX = 0;
    int arrowDrawY = 0;
    int arrowScaledWidth = 0;
    int arrowScaledHeight = 0;
    if (hasArrowBitmap) {
        arrowScaledWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(s_arrowWidth) * kLockedTargetArrowScale)));
        arrowScaledHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(s_arrowHeight) * kLockedTargetArrowScale)));
        arrowDrawX = centerX - (arrowScaledWidth / 2);
        arrowDrawY = labelY - kLockedTargetArrowBaseLift - arrowScaledHeight - kLockedTargetArrowYOffset - bounce;
        const int drawX = arrowDrawX;
        const int drawY = arrowDrawY;
        arrowRect = { drawX - 2, drawY - 2, drawX + arrowScaledWidth + 2, drawY + arrowScaledHeight + 2 };
    } else {
        const int tipY = labelY - kLockedTargetArrowBaseLift - bounce;
        arrowRect = { centerX - 9, tipY - 13, centerX + 9, tipY + 2 };
    }

    if (hasArrowBitmap && !s_targetArrowTexture) {
        s_targetArrowTexture = new CTexture();
        if (!s_targetArrowTexture || !s_targetArrowTexture->Create(s_arrowWidth, s_arrowHeight, PF_A8R8G8B8, false)) {
            delete s_targetArrowTexture;
            s_targetArrowTexture = nullptr;
        } else {
            SetTextureDebugName(s_targetArrowTexture, "__overlay_target_arrow__");
            s_targetArrowTexture->Update(
                0,
                0,
                s_arrowWidth,
                s_arrowHeight,
                s_arrowPixels.data(),
                false,
                s_arrowWidth * static_cast<int>(sizeof(unsigned int)));
        }
    }

    auto queueOverlayTextureQuad = [&](CTexture* texture, float left, float top, float right, float bottom) -> bool {
        if (!texture) {
            return false;
        }

        RPFace* face = g_renderer.BorrowNullRP();
        if (!face) {
            return false;
        }

        const unsigned int overlayContentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : texture->m_w;
        const unsigned int overlayContentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : texture->m_h;
        const float maxU = texture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(texture->m_w) : 1.0f;
        const float maxV = texture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(texture->m_h) : 1.0f;

        face->primType = D3DPT_TRIANGLESTRIP;
        face->verts = face->m_verts;
        face->numVerts = 4;
        face->indices = nullptr;
        face->numIndices = 0;
        face->tex = texture;
        face->mtPreset = 3;
        face->cullMode = D3DCULL_NONE;
        face->srcAlphaMode = D3DBLEND_SRCALPHA;
        face->destAlphaMode = D3DBLEND_INVSRCALPHA;
        face->alphaSortKey = 1.5f;

        face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
        face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
        face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
        face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
        g_renderer.AddRP(face, 1 | 8);
        return true;
    };

    if (hasArrowBitmap && s_targetArrowTexture) {
        bool queuedAny = false;

        if (drawLockedTargetText) {
            static CTexture* s_targetLabelTexture = nullptr;
            static int s_targetLabelTextureWidth = 0;
            static int s_targetLabelTextureHeight = 0;
            static std::vector<unsigned int> s_targetLabelPixels;
            static bool s_targetLabelTextureValid = false;
            static std::uint64_t s_targetLabelStateToken = 0ull;
            const int labelWidth = (std::max)(1, static_cast<int>(textRect.right - textRect.left));
            const int labelHeight = (std::max)(1, static_cast<int>(textRect.bottom - textRect.top));

            if (!s_targetLabelTexture || s_targetLabelTextureWidth != labelWidth || s_targetLabelTextureHeight != labelHeight) {
                delete s_targetLabelTexture;
                s_targetLabelTexture = new CTexture();
                if (!s_targetLabelTexture || !s_targetLabelTexture->Create(labelWidth, labelHeight, PF_A8R8G8B8, false)) {
                    delete s_targetLabelTexture;
                    s_targetLabelTexture = nullptr;
                    s_targetLabelTextureWidth = 0;
                    s_targetLabelTextureHeight = 0;
                } else {
                    SetTextureDebugName(s_targetLabelTexture, "__overlay_target_label__");
                    s_targetLabelTextureWidth = labelWidth;
                    s_targetLabelTextureHeight = labelHeight;
                    s_targetLabelPixels.assign(static_cast<size_t>(labelWidth) * static_cast<size_t>(labelHeight), kOverlayTransparentKey);
                    s_targetLabelTextureValid = false;
                    s_targetLabelStateToken = 0ull;
                }
            } else if (s_targetLabelPixels.size() != static_cast<size_t>(labelWidth) * static_cast<size_t>(labelHeight)) {
                s_targetLabelPixels.assign(static_cast<size_t>(labelWidth) * static_cast<size_t>(labelHeight), kOverlayTransparentKey);
                s_targetLabelTextureValid = false;
            }

            if (s_targetLabelTexture) {
                std::uint64_t labelStateToken = 1469598103934665603ull;
                HashTokenString(&labelStateToken, label);
                HashTokenValue(&labelStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(lockedTargetTextColor)));
                HashTokenValue(&labelStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(labelWidth)));
                HashTokenValue(&labelStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(labelHeight)));

                if (!s_targetLabelTextureValid || labelStateToken != s_targetLabelStateToken) {
                    std::fill(s_targetLabelPixels.begin(), s_targetLabelPixels.end(), kOverlayTransparentKey);
                    DrawOutlinedTextQtToOverlay(
                        s_targetLabelPixels.data(),
                        labelWidth,
                        labelHeight,
                        labelWidth * static_cast<int>(sizeof(unsigned int)),
                        2,
                        2,
                        label,
                        lockedTargetTextColor);
                    ConvertOverlayComposeBitsToAlpha(s_targetLabelPixels.data(), labelWidth, labelHeight);
                    s_targetLabelTexture->Update(
                        0,
                        0,
                        labelWidth,
                        labelHeight,
                        s_targetLabelPixels.data(),
                        true,
                        labelWidth * static_cast<int>(sizeof(unsigned int)));
                    s_targetLabelTextureValid = true;
                    s_targetLabelStateToken = labelStateToken;
                }

                queuedAny = queueOverlayTextureQuad(
                    s_targetLabelTexture,
                    OverlayScreenEdge(textRect.left),
                    OverlayScreenEdge(textRect.top),
                    OverlayScreenEdge(textRect.right),
                    OverlayScreenEdge(textRect.bottom)) || queuedAny;
            }
        }

        queuedAny = queueOverlayTextureQuad(
            s_targetArrowTexture,
            OverlayScreenEdge(arrowDrawX),
            OverlayScreenEdge(arrowDrawY),
            OverlayScreenEdge(arrowDrawX + arrowScaledWidth),
            OverlayScreenEdge(arrowDrawY + arrowScaledHeight)) || queuedAny;

        if (queuedAny) {
            return true;
        }
    }

    UnionRect(&overlayRect, &overlayRect, &arrowRect);

    InflateRect(&overlayRect, 4, 4);
    RECT clippedRect{};
    IntersectRect(&clippedRect, &overlayRect, &clientRect);

    const int width = clippedRect.right - clippedRect.left;
    const int height = clippedRect.bottom - clippedRect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    static CTexture* s_targetTexture = nullptr;
    static int s_targetTextureWidth = 0;
    static int s_targetTextureHeight = 0;
    static std::vector<unsigned int> s_targetComposePixels;
#if !RO_ENABLE_QT6_UI
    static ArgbDibSurface s_targetComposeSurface;
#endif
    if (!s_targetTexture || s_targetTextureWidth != width || s_targetTextureHeight != height) {
        delete s_targetTexture;
        s_targetTexture = new CTexture();
        if (!s_targetTexture || !s_targetTexture->Create(width, height, PF_A8R8G8B8, false)) {
            delete s_targetTexture;
            s_targetTexture = nullptr;
            s_targetTextureWidth = 0;
            s_targetTextureHeight = 0;
            return false;
        }
        SetTextureDebugName(s_targetTexture, "__overlay_target_composite__");
        s_targetTextureWidth = width;
        s_targetTextureHeight = height;
        s_targetComposePixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), kOverlayTransparentKey);
    } else if (s_targetComposePixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        s_targetComposePixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), kOverlayTransparentKey);
    }

    unsigned int* targetPixels = nullptr;
#if RO_ENABLE_QT6_UI
    std::fill(s_targetComposePixels.begin(), s_targetComposePixels.end(), kOverlayTransparentKey);
    targetPixels = s_targetComposePixels.data();
    if (drawLockedTargetText) {
        DrawOutlinedTextQtToOverlay(targetPixels,
            width,
            height,
            width * static_cast<int>(sizeof(unsigned int)),
            textX - clippedRect.left,
            textY - clippedRect.top,
            label,
            lockedTargetTextColor);
    }
    if (!hasArrowBitmap) {
        const int tipY = labelY - kLockedTargetArrowBaseLift - bounce;
        DrawFallbackLockedTargetArrowToPixels(
            targetPixels,
            width,
            height,
            centerX - clippedRect.left,
            tipY - clippedRect.top);
    }
#else
    const bool needComposeSurface = drawLockedTargetText || !hasArrowBitmap;
    if (needComposeSurface) {
        const bool composeReady = EnsureOverlayComposeSurface(width, height, &s_targetComposeSurface);
        if (!composeReady) {
            return false;
        }

        ClearOverlayComposeBits(s_targetComposeSurface.GetBits(), width, height);
        if (drawLockedTargetText) {
            const int savedDc = SaveDC(s_targetComposeSurface.GetDC());
            SetViewportOrgEx(s_targetComposeSurface.GetDC(), -clippedRect.left, -clippedRect.top, nullptr);
            DrawLockedTargetName(mode, s_targetComposeSurface.GetDC());
            RestoreDC(s_targetComposeSurface.GetDC(), savedDc);
        }
        if (!hasArrowBitmap) {
            const int savedArrowDc = SaveDC(s_targetComposeSurface.GetDC());
            SetViewportOrgEx(s_targetComposeSurface.GetDC(), -clippedRect.left, -clippedRect.top, nullptr);
            DrawLockedTargetArrow(mode, s_targetComposeSurface.GetDC());
            RestoreDC(s_targetComposeSurface.GetDC(), savedArrowDc);
        }
        targetPixels = s_targetComposeSurface.GetPixels();
    } else {
        std::fill(s_targetComposePixels.begin(), s_targetComposePixels.end(), kOverlayTransparentKey);
        targetPixels = s_targetComposePixels.data();
    }
#endif

    if (hasArrowBitmap) {
        for (int destY = 0; destY < arrowScaledHeight; ++destY) {
            const int localY = (arrowDrawY - clippedRect.top) + destY;
            if (localY < 0 || localY >= height) {
                continue;
            }

            const int srcY = destY * s_arrowHeight / arrowScaledHeight;
            for (int destX = 0; destX < arrowScaledWidth; ++destX) {
                const int localX = (arrowDrawX - clippedRect.left) + destX;
                if (localX < 0 || localX >= width) {
                    continue;
                }

                const int srcX = destX * s_arrowWidth / arrowScaledWidth;
                const unsigned int srcPixel = s_arrowPixels[static_cast<size_t>(srcY) * static_cast<size_t>(s_arrowWidth) + static_cast<size_t>(srcX)];
                if ((srcPixel & 0x00FFFFFFu) == (kOverlayTransparentKey & 0x00FFFFFFu)) {
                    continue;
                }

                const unsigned int srcAlpha = (srcPixel >> 24) & 0xFFu;
                unsigned int& dstPixel = targetPixels[static_cast<size_t>(localY) * static_cast<size_t>(width) + static_cast<size_t>(localX)];
                if (srcAlpha >= 0xFFu || (dstPixel & 0x00FFFFFFu) == (kOverlayTransparentKey & 0x00FFFFFFu)) {
                    dstPixel = srcPixel & 0x00FFFFFFu;
                } else if (srcAlpha != 0u) {
                    const unsigned int dstBlue = dstPixel & 0xFFu;
                    const unsigned int dstGreen = (dstPixel >> 8) & 0xFFu;
                    const unsigned int dstRed = (dstPixel >> 16) & 0xFFu;
                    const unsigned int srcBlue = srcPixel & 0xFFu;
                    const unsigned int srcGreen = (srcPixel >> 8) & 0xFFu;
                    const unsigned int srcRed = (srcPixel >> 16) & 0xFFu;
                    const unsigned int invAlpha = 255u - srcAlpha;
                    const unsigned int outBlue = (srcBlue * srcAlpha + dstBlue * invAlpha) / 255u;
                    const unsigned int outGreen = (srcGreen * srcAlpha + dstGreen * invAlpha) / 255u;
                    const unsigned int outRed = (srcRed * srcAlpha + dstRed * invAlpha) / 255u;
                    dstPixel = (outRed << 16) | (outGreen << 8) | outBlue;
                }
            }
        }
    }

    ConvertOverlayComposeBitsToAlpha(targetPixels, width, height);
    s_targetTexture->Update(0,
        0,
        width,
        height,
        targetPixels,
        true,
        width * static_cast<int>(sizeof(unsigned int)));

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(clippedRect.left);
    const float top = OverlayScreenEdge(clippedRect.top);
    const float right = OverlayScreenEdge(clippedRect.right);
    const float bottom = OverlayScreenEdge(clippedRect.bottom);
    const unsigned int overlayContentWidth = s_targetTexture->m_surfaceUpdateWidth > 0 ? s_targetTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = s_targetTexture->m_surfaceUpdateHeight > 0 ? s_targetTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = s_targetTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_targetTexture->m_w) : 1.0f;
    const float maxV = s_targetTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_targetTexture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_targetTexture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 1.5f;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueMsgEffectsOverlayQuad()
{
    return QueueQueuedMsgEffectsQuads();
}

#if RO_ENABLE_QT6_UI
bool QueueHoverLabelsOverlayQuad(CGameMode& mode)
{
    if (!g_hMainWnd || !mode.m_world || !mode.m_view) {
        return false;
    }

    if (IsWorldHoverBlockedByUi(mode.m_oldMouseX, mode.m_oldMouseY)) {
        return false;
    }

    std::string label;
    COLORREF textColor = RGB(255, 255, 255);
    int drawX = 0;
    int drawY = 0;

    CItem* hoveredItem = nullptr;
    int labelX = 0;
    int labelY = 0;
    if (mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
            mode.m_oldMouseX,
            mode.m_oldMouseY,
            &hoveredItem,
            &labelX,
            &labelY)
        && hoveredItem) {
        label = ResolveGroundItemHoverLabel(hoveredItem);
        if (label.empty()) {
            return false;
        }

        SIZE textSize{};
        if (!MeasureOverlayTextQt(label, &textSize)) {
            return false;
        }
        drawX = labelX - (textSize.cx / 2);
        drawY = labelY - textSize.cy - kHoverNameTextPadding;
    } else {
        CGameActor* hoveredActor = nullptr;
        if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
                mode.m_view->GetCameraLongitude(),
                mode.m_oldMouseX,
                mode.m_oldMouseY,
                &hoveredActor,
                &labelX,
                &labelY)
            || !hoveredActor
            || hoveredActor->m_gid == mode.m_lastLockOnMonGid) {
            return false;
        }

        label = ResolveHoveredActorName(mode, hoveredActor);
        if (label.empty()) {
            return false;
        }

        SIZE textSize{};
        if (!MeasureOverlayTextQt(label, &textSize)) {
            return false;
        }
        drawX = labelX - (textSize.cx / 2);
        drawY = labelY + kHoverNameTextPadding + kHoverNameVerticalOffset;
        if (ActorHasPlayerStyleVitalsOverlay(hoveredActor)
            || hoveredActor == mode.m_world->m_player) {
            drawY += ResolvePlayerStyleVitalsNameOffsetY(mode.m_view);
        }
        textColor = ResolveHoverNameColor(hoveredActor);
    }

    SIZE finalTextSize{};
    if (!MeasureOverlayTextQt(label, &finalTextSize)) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    RECT overlayRect{ drawX - 2, drawY - 2, drawX + finalTextSize.cx + 2, drawY + finalTextSize.cy + 2 };
    RECT clippedRect{};
    IntersectRect(&clippedRect, &overlayRect, &clientRect);
    const int width = clippedRect.right - clippedRect.left;
    const int height = clippedRect.bottom - clippedRect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    static CTexture* s_hoverTextTexture = nullptr;
    static int s_hoverTextTextureWidth = 0;
    static int s_hoverTextTextureHeight = 0;
    static std::vector<unsigned int> s_hoverTextPixels;
    if (!s_hoverTextTexture || s_hoverTextTextureWidth != width || s_hoverTextTextureHeight != height) {
        delete s_hoverTextTexture;
        s_hoverTextTexture = new CTexture();
        if (!s_hoverTextTexture || !s_hoverTextTexture->Create(width, height, PF_A8R8G8B8, false)) {
            delete s_hoverTextTexture;
            s_hoverTextTexture = nullptr;
            s_hoverTextTextureWidth = 0;
            s_hoverTextTextureHeight = 0;
            return false;
        }
        SetTextureDebugName(s_hoverTextTexture, "__overlay_hover_label__");
        s_hoverTextTextureWidth = width;
        s_hoverTextTextureHeight = height;
        s_hoverTextPixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), kOverlayTransparentKey);
    } else if (s_hoverTextPixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        s_hoverTextPixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), kOverlayTransparentKey);
    }

    std::fill(s_hoverTextPixels.begin(), s_hoverTextPixels.end(), kOverlayTransparentKey);
    DrawOutlinedTextQtToOverlay(s_hoverTextPixels.data(),
        width,
        height,
        width * static_cast<int>(sizeof(unsigned int)),
        drawX - clippedRect.left,
        drawY - clippedRect.top,
        label,
        textColor);
    ConvertOverlayComposeBitsToAlpha(s_hoverTextPixels.data(), width, height);
    s_hoverTextTexture->Update(0,
        0,
        width,
        height,
        s_hoverTextPixels.data(),
        true,
        width * static_cast<int>(sizeof(unsigned int)));

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(clippedRect.left);
    const float top = OverlayScreenEdge(clippedRect.top);
    const float right = OverlayScreenEdge(clippedRect.right);
    const float bottom = OverlayScreenEdge(clippedRect.bottom);
    const unsigned int overlayContentWidth = s_hoverTextTexture->m_surfaceUpdateWidth > 0 ? s_hoverTextTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = s_hoverTextTexture->m_surfaceUpdateHeight > 0 ? s_hoverTextTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = s_hoverTextTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_hoverTextTexture->m_w) : 1.0f;
    const float maxV = s_hoverTextTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_hoverTextTexture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_hoverTextTexture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 1.55f;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}
#else
bool QueueHoverLabelsOverlayQuad(CGameMode& mode)
{
    (void)mode;
    return false;
}
#endif

bool QueuePlayerVitalsOverlayQuad(CGameMode& mode)
{
    if (!g_hMainWnd || !mode.m_world || !mode.m_view || !mode.m_world->m_player) {
        return false;
    }

    int labelX = 0;
    int labelY = 0;
    if (!mode.m_world->GetPlayerScreenLabel(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        &labelX,
        &labelY)) {
        return false;
    }

    CPlayer* player = mode.m_world->m_player;
    const s64 maxHp = player->m_MaxHp;
    const s64 maxSp = player->m_MaxSp;
    if (maxHp <= 0 && maxSp <= 0) {
        return false;
    }

    const PlayerVitalsScreenMetrics metrics = ResolvePlayerVitalsScreenMetrics(mode.m_view);
    const int totalHeight = metrics.totalHeight;
    const int outerWidth = metrics.barWidth + metrics.border * 2;
    const int outerLeft = labelX - (outerWidth / 2);
    const int barTop = labelY + metrics.topDelta;
    RECT vitalsRect{
        outerLeft,
        barTop,
        outerLeft + outerWidth,
        barTop + totalHeight,
    };

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    RECT clippedRect{};
    IntersectRect(&clippedRect, &vitalsRect, &clientRect);
    const int width = clippedRect.right - clippedRect.left;
    const int height = clippedRect.bottom - clippedRect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    static CTexture* s_vitalsTexture = nullptr;
    static int s_vitalsTextureWidth = 0;
    static int s_vitalsTextureHeight = 0;
    static std::vector<unsigned int> s_vitalsComposePixels;
    static bool s_vitalsTextureValid = false;
    static std::uint64_t s_vitalsStateToken = 0ull;
    if (!s_vitalsTexture || s_vitalsTextureWidth != width || s_vitalsTextureHeight != height) {
        delete s_vitalsTexture;
        s_vitalsTexture = new CTexture();
        if (!s_vitalsTexture || !s_vitalsTexture->Create(width, height, PF_A8R8G8B8, false)) {
            delete s_vitalsTexture;
            s_vitalsTexture = nullptr;
            s_vitalsTextureWidth = 0;
            s_vitalsTextureHeight = 0;
            return false;
        }
        SetTextureDebugName(s_vitalsTexture, "__overlay_player_vitals__");
        s_vitalsTextureWidth = width;
        s_vitalsTextureHeight = height;
        s_vitalsComposePixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
        s_vitalsTextureValid = false;
        s_vitalsStateToken = 0ull;
    } else if (s_vitalsComposePixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        s_vitalsComposePixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
        s_vitalsTextureValid = false;
    }

    std::uint64_t vitalsStateToken = 1469598103934665603ull;
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(player->m_gid));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_Hp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_MaxHp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_Sp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_MaxSp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(labelX)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(barTop)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(metrics.barWidth)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(metrics.barHeight)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(width)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(height)));

    if (!s_vitalsTextureValid || vitalsStateToken != s_vitalsStateToken) {
        std::fill(s_vitalsComposePixels.begin(), s_vitalsComposePixels.end(), 0u);
    const int localOuterLeft = outerLeft - clippedRect.left;
    const int localOuterTop = barTop - clippedRect.top;
    RECT outerRect{
        localOuterLeft,
        localOuterTop,
        localOuterLeft + outerWidth,
        localOuterTop + totalHeight,
    };
    FillSolidRectArgb(s_vitalsComposePixels.data(), width, height, outerRect, RGB(0, 0, 0));

    const int innerLeft = outerRect.left + metrics.border;
    const int innerTop = outerRect.top + metrics.border;
    RECT hpRect{ innerLeft, innerTop, innerLeft + metrics.barWidth, innerTop + metrics.barHeight };
    RECT spRect{
        innerLeft,
        hpRect.bottom + metrics.border,
        innerLeft + metrics.barWidth,
        hpRect.bottom + metrics.border + metrics.barHeight,
    };
    FillSolidRectArgb(s_vitalsComposePixels.data(), width, height, hpRect, RGB(255, 255, 255));
    FillSolidRectArgb(s_vitalsComposePixels.data(), width, height, spRect, RGB(255, 255, 255));

    if (maxHp > 0) {
        const s64 hp = (std::min)(static_cast<s64>(player->m_Hp), maxHp);
        const int hpFillWidth = VitalBarFill(hp, maxHp, metrics.barWidth);
        if (hpFillWidth > 0) {
            RECT hpFillRect{ hpRect.left, hpRect.top, hpRect.left + hpFillWidth, hpRect.bottom };
            const bool lowHp = hp * 4 < maxHp;
            FillSolidRectArgb(
                s_vitalsComposePixels.data(),
                width,
                height,
                hpFillRect,
                lowHp ? RGB(220, 32, 32) : RGB(48, 192, 48));
        }
    }

    if (maxSp > 0) {
        const s64 sp = (std::min)(static_cast<s64>(player->m_Sp), maxSp);
        const int spFillWidth = VitalBarFill(sp, maxSp, metrics.barWidth);
        if (spFillWidth > 0) {
            RECT spFillRect{ spRect.left, spRect.top, spRect.left + spFillWidth, spRect.bottom };
            FillSolidRectArgb(s_vitalsComposePixels.data(), width, height, spFillRect, RGB(48, 96, 220));
        }
    }

        s_vitalsTexture->Update(0,
            0,
            width,
            height,
            s_vitalsComposePixels.data(),
            true,
            width * static_cast<int>(sizeof(unsigned int)));
        s_vitalsTextureValid = true;
        s_vitalsStateToken = vitalsStateToken;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(clippedRect.left);
    const float top = OverlayScreenEdge(clippedRect.top);
    const float right = OverlayScreenEdge(clippedRect.right);
    const float bottom = OverlayScreenEdge(clippedRect.bottom);
    const unsigned int overlayContentWidth = s_vitalsTexture->m_surfaceUpdateWidth > 0 ? s_vitalsTexture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = s_vitalsTexture->m_surfaceUpdateHeight > 0 ? s_vitalsTexture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = s_vitalsTexture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(s_vitalsTexture->m_w) : 1.0f;
    const float maxV = s_vitalsTexture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(s_vitalsTexture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = s_vitalsTexture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = kPlayerVitalsOverlaySortKey;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueVitalsOverlayQuadAt(CGameMode& mode,
    u32 ownerGid,
    int labelX,
    int labelY,
    s64 hp,
    s64 maxHp,
    s64 sp,
    s64 maxSp,
    float sortKey,
    const char* textureDebugName)
{
    if (!g_hMainWnd || !mode.m_view || ownerGid == 0) {
        return false;
    }
    if (maxHp <= 0 && maxSp <= 0) {
        return false;
    }

    const PlayerVitalsScreenMetrics metrics = ResolvePlayerVitalsScreenMetrics(mode.m_view);
    const int totalHeight = metrics.totalHeight;
    const int outerWidth = metrics.barWidth + metrics.border * 2;
    const int outerLeft = labelX - (outerWidth / 2);
    const int barTop = labelY + metrics.topDelta;
    RECT vitalsRect{
        outerLeft,
        barTop,
        outerLeft + outerWidth,
        barTop + totalHeight,
    };

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    RECT clippedRect{};
    IntersectRect(&clippedRect, &vitalsRect, &clientRect);
    const int width = clippedRect.right - clippedRect.left;
    const int height = clippedRect.bottom - clippedRect.top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    struct CompanionVitalsSlot {
        CTexture* texture = nullptr;
        int textureWidth = 0;
        int textureHeight = 0;
        std::vector<unsigned int> composePixels;
        bool textureValid = false;
        std::uint64_t stateToken = 0ull;
        u32 boundGid = 0;
    };
    static CompanionVitalsSlot s_slots[2];
    CompanionVitalsSlot* slot = nullptr;
    for (CompanionVitalsSlot& candidate : s_slots) {
        if (candidate.boundGid == ownerGid) {
            slot = &candidate;
            break;
        }
    }
    if (!slot) {
        for (CompanionVitalsSlot& candidate : s_slots) {
            if (candidate.boundGid == 0) {
                slot = &candidate;
                break;
            }
        }
    }
    if (!slot) {
        slot = &s_slots[0];
    }
    slot->boundGid = ownerGid;

    if (!slot->texture || slot->textureWidth != width || slot->textureHeight != height) {
        delete slot->texture;
        slot->texture = new CTexture();
        if (!slot->texture || !slot->texture->Create(width, height, PF_A8R8G8B8, false)) {
            delete slot->texture;
            slot->texture = nullptr;
            slot->textureWidth = 0;
            slot->textureHeight = 0;
            slot->boundGid = 0;
            return false;
        }
        SetTextureDebugName(slot->texture, textureDebugName ? textureDebugName : "__overlay_companion_vitals__");
        slot->textureWidth = width;
        slot->textureHeight = height;
        slot->composePixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
        slot->textureValid = false;
        slot->stateToken = 0ull;
    } else if (slot->composePixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
        slot->composePixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
        slot->textureValid = false;
    }

    std::uint64_t vitalsStateToken = 1469598103934665603ull;
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(ownerGid));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(hp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(maxHp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(sp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(maxSp)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(labelX)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(barTop)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(metrics.barWidth)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(metrics.barHeight)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(width)));
    HashTokenValue(&vitalsStateToken, static_cast<std::uint64_t>(static_cast<std::uint32_t>(height)));

    if (!slot->textureValid || vitalsStateToken != slot->stateToken) {
        std::fill(slot->composePixels.begin(), slot->composePixels.end(), 0u);
        const int localOuterLeft = outerLeft - clippedRect.left;
        const int localOuterTop = barTop - clippedRect.top;
        RECT outerRect{
            localOuterLeft,
            localOuterTop,
            localOuterLeft + outerWidth,
            localOuterTop + totalHeight,
        };
        FillSolidRectArgb(slot->composePixels.data(), width, height, outerRect, RGB(0, 0, 0));

        const int innerLeft = outerRect.left + metrics.border;
        const int innerTop = outerRect.top + metrics.border;
        RECT hpRect{ innerLeft, innerTop, innerLeft + metrics.barWidth, innerTop + metrics.barHeight };
        RECT spRect{
            innerLeft,
            hpRect.bottom + metrics.border,
            innerLeft + metrics.barWidth,
            hpRect.bottom + metrics.border + metrics.barHeight,
        };
        FillSolidRectArgb(slot->composePixels.data(), width, height, hpRect, RGB(255, 255, 255));
        FillSolidRectArgb(slot->composePixels.data(), width, height, spRect, RGB(255, 255, 255));

        if (maxHp > 0) {
            const s64 clampedHp = (std::min)(hp, maxHp);
            const int hpFillWidth = VitalBarFill(clampedHp, maxHp, metrics.barWidth);
            if (hpFillWidth > 0) {
                RECT hpFillRect{ hpRect.left, hpRect.top, hpRect.left + hpFillWidth, hpRect.bottom };
                const bool lowHp = clampedHp * 4 < maxHp;
                FillSolidRectArgb(
                    slot->composePixels.data(),
                    width,
                    height,
                    hpFillRect,
                    lowHp ? RGB(220, 32, 32) : RGB(48, 192, 48));
            }
        }

        if (maxSp > 0) {
            const s64 clampedSp = (std::min)(sp, maxSp);
            const int spFillWidth = VitalBarFill(clampedSp, maxSp, metrics.barWidth);
            if (spFillWidth > 0) {
                RECT spFillRect{ spRect.left, spRect.top, spRect.left + spFillWidth, spRect.bottom };
                FillSolidRectArgb(slot->composePixels.data(), width, height, spFillRect, RGB(48, 96, 220));
            }
        }

        slot->texture->Update(0,
            0,
            width,
            height,
            slot->composePixels.data(),
            true,
            width * static_cast<int>(sizeof(unsigned int)));
        slot->textureValid = true;
        slot->stateToken = vitalsStateToken;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(clippedRect.left);
    const float top = OverlayScreenEdge(clippedRect.top);
    const float right = OverlayScreenEdge(clippedRect.right);
    const float bottom = OverlayScreenEdge(clippedRect.bottom);
    const unsigned int overlayContentWidth = slot->texture->m_surfaceUpdateWidth > 0
        ? slot->texture->m_surfaceUpdateWidth
        : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = slot->texture->m_surfaceUpdateHeight > 0
        ? slot->texture->m_surfaceUpdateHeight
        : static_cast<unsigned int>(height);
    const float maxU = slot->texture->m_w != 0
        ? static_cast<float>(overlayContentWidth) / static_cast<float>(slot->texture->m_w)
        : 1.0f;
    const float maxV = slot->texture->m_h != 0
        ? static_cast<float>(overlayContentHeight) / static_cast<float>(slot->texture->m_h)
        : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = slot->texture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = sortKey;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueOwnedCompanionVitalsOverlayQuads(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_view) {
        return false;
    }

    bool queued = false;
    const matrix& viewMatrix = mode.m_view->GetViewMatrix();
    const float cameraLongitude = mode.m_view->GetCameraLongitude();

    auto queueOwned = [&](u32 gid, bool alive, const CompanionCombatStatus& combat, const char* debugName, float sortKey) {
        if (!alive || gid == 0) {
            return;
        }
        if (!mode.m_world->GetGameActorByAID(gid)) {
            return;
        }
        int centerX = 0;
        int labelY = 0;
        if (!mode.m_world->GetActorScreenMarker(viewMatrix, cameraLongitude, gid, &centerX, nullptr, &labelY)) {
            return;
        }
        if (QueueVitalsOverlayQuadAt(mode,
                gid,
                centerX,
                labelY,
                combat.hp,
                combat.maxHp,
                combat.sp,
                combat.maxSp,
                sortKey,
                debugName)) {
            queued = true;
        }
    };

    queueOwned(g_session.m_homunGID,
        g_session.m_homunStatus.alive,
        g_session.m_homunStatus.combat,
        "__overlay_homun_vitals__",
        kPlayerVitalsOverlaySortKey - 0.001f);
    queueOwned(g_session.m_merGID,
        g_session.m_merStatus.alive,
        g_session.m_merStatus.combat,
        "__overlay_mer_vitals__",
        kPlayerVitalsOverlaySortKey - 0.002f);
    return queued;
}

void DrawGameplayOverlayToHdc(CGameMode& mode, HDC targetDc)
{
    if (!targetDc) {
        return;
    }
    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    DrawHoveredGroundItemName(mode, targetDc);
    DrawHoveredActorName(mode, targetDc);
}

void DrawGameplayFallbackToWindow(
    CGameMode& mode,
    int cursorActNum,
    u32 mouseAnimStartTick,
    bool trackMovePerfFrame,
    double uiDrawStartMs,
    bool allowCursorWithoutWindowDc,
    bool cursorAlreadyQueued)
{
    const bool qtGameplayRuntimeEnabled = IsQtUiRuntimeEnabled()
        && GetRenderDevice().GetLegacyDevice() == nullptr;
    if (qtGameplayRuntimeEnabled) {
        HDC windowDc = GetDC(g_hMainWnd);
        if (windowDc) {
#if RO_PLATFORM_WINDOWS
            RECT client{};
            if (GetClientRect(g_hMainWnd, &client)) {
                static ArgbDibSurface surface;
                RECT bounds{};
                if (g_windowMgr.BuildGroundSkillPromptOverlay(surface, GroundPromptLogicalViewport(client, true),
                        timeGetTime(), bounds) && bounds.right > bounds.left && bounds.bottom > bounds.top) {
                    const auto screen = GroundPromptScreenBounds(bounds, true);
                    AlphaBlendArgbToHdc(windowDc, screen.left, screen.top, screen.right-screen.left,
                        screen.bottom-screen.top, surface.GetPixels(), surface.GetWidth(), surface.GetHeight());
                }
            }
#endif
            if (!cursorAlreadyQueued) {
                const double cursorHdcStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
                DrawModeCursorToHdc(windowDc, cursorActNum, mouseAnimStartTick);
                if (trackMovePerfFrame) {
                    g_overlayMovePerfStats.fallbackCursorHdcMs += QpcNowMs() - cursorHdcStartMs;
                }
            }
            ReleaseDC(g_hMainWnd, windowDc);
        } else if (!cursorAlreadyQueued && allowCursorWithoutWindowDc) {
            const double cursorStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
            DrawModeCursor(cursorActNum, mouseAnimStartTick);
            if (trackMovePerfFrame) {
                g_overlayMovePerfStats.fallbackCursorMs += QpcNowMs() - cursorStartMs;
            }
        }
        return;
    }

    HDC windowDc = GetDC(g_hMainWnd);
    if (windowDc) {
        const double overlayDrawStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        DrawGameplayOverlayToHdc(mode, windowDc);
        DrawPlayerVitalsOverlay(mode, windowDc);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.fallbackOverlayDrawMs += QpcNowMs() - overlayDrawStartMs;
        }
        g_windowMgr.DrawVisibleWindowsToHdc(windowDc, true);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.fallbackUiDrawMs += QpcNowMs() - uiDrawStartMs;
        }
        const double cursorHdcStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        DrawModeCursorToHdc(windowDc, cursorActNum, mouseAnimStartTick);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.fallbackCursorHdcMs += QpcNowMs() - cursorHdcStartMs;
        }
        ReleaseDC(g_hMainWnd, windowDc);
    } else if (allowCursorWithoutWindowDc) {
        const double cursorStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        DrawModeCursor(cursorActNum, mouseAnimStartTick);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.fallbackCursorMs += QpcNowMs() - cursorStartMs;
        }
    }
}

bool RequestReturnToCharSelect()
{
    PACKET_CZ_RESTART packet{};
    packet.PacketType = PACKETID_CZ_RESTART;
    packet.Type = 1;

    if (!BeginPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect)) {
        g_windowMgr.PushChatEvent(
            "A disconnect request is already pending.",
            0x00FF4040u,
            6);
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (!sent) {
        ClearPendingDisconnectAction();
        g_windowMgr.PushChatEvent(
            "Unable to send the character-select request.",
            0x00FF4040u,
            6);
        DbgLog("[GameMode] failed to send return-to-character-select request\n");
        return false;
    }

    MarkPendingDisconnectActionSent(GetTickCount());
    return true;
}

bool RequestReturnToSavePoint()
{
    PACKET_CZ_RESTART packet{};
    packet.PacketType = PACKETID_CZ_RESTART;
    packet.Type = 0;

    return CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
}

bool RequestExitToWindows()
{
    PACKET_CZ_QUITGAME packet{};
    packet.PacketType = PACKETID_CZ_QUITGAME;
    packet.Type = 0;

    if (!BeginPendingDisconnectAction(PendingDisconnectAction::ExitToWindows)) {
        g_windowMgr.PushChatEvent(
            "A disconnect request is already pending.",
            0x00FF4040u,
            6);
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (!sent) {
        ClearPendingDisconnectAction();
        g_windowMgr.PushChatEvent(
            "Unable to send the exit request.",
            0x00FF4040u,
            6);
        DbgLog("[GameMode] failed to send exit-to-Windows request\n");
        return false;
    }

    MarkPendingDisconnectActionSent(GetTickCount());
    return true;
}

void TraceGamePacketFirstSeen(const PacketView& packet)
{
    if (!packet.data || g_packetTraceStartTick == 0) {
        return;
    }

    const u32 now = GetTickCount();
    if (now - g_packetTraceStartTick > kPacketTraceWindowMs) {
        return;
    }

    if (!g_packetTraceLoggedIds.insert(std::make_pair(packet.packetId, true)).second) {
        return;
    }

    DbgLog("[GameMode] trace recv id=0x%04X len=%u dt=%ums\n",
        packet.packetId,
        packet.packetLength,
        now - g_packetTraceStartTick);
}

struct ViewPointConstraintData {
    int minDistance = 0;
    int distanceScope = 0;
    int initialDistance = 0;
    int minLongitude = 0;
    int maxLongitude = 0;
    int initialLongitude = 0;
    int maxLatitude = 0;
    int minLatitude = 0;
    int initialLatitude = 0;
};

std::string TrimAscii(const std::string& value)
{
    size_t start = 0;
    while (start < value.size() && static_cast<unsigned char>(value[start]) <= ' ') {
        ++start;
    }

    size_t end = value.size();
    while (end > start && static_cast<unsigned char>(value[end - 1]) <= ' ') {
        --end;
    }

    return value.substr(start, end - start);
}

std::string LowercaseAscii(std::string value)
{
    for (char& ch : value) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte < 0x80) {
            ch = static_cast<char>(std::tolower(byte));
        }
    }
    return value;
}

ULONG_PTR EnsureGdiplusStarted();
const char* UiKorPrefix();
std::string ResolveDataPath(const std::string& fileName, const char* ext, const std::vector<std::string>& directPrefixes);

std::string NormalizeRswNameForCameraTables(const char* rswName)
{
    if (!rswName || !*rswName) {
        return std::string();
    }

    std::string normalized = LowercaseAscii(TrimAscii(rswName));
    const size_t slashPos = normalized.find_last_of("\\/");
    if (slashPos != std::string::npos) {
        normalized = normalized.substr(slashPos + 1);
    }

    while (!normalized.empty() && normalized.back() == '#') {
        normalized.pop_back();
    }

    if (normalized.size() >= 4 && normalized.compare(normalized.size() - 4, 4, ".gat") == 0) {
        normalized.replace(normalized.size() - 4, 4, ".rsw");
    } else if (normalized.find('.') == std::string::npos) {
        normalized += ".rsw";
    }

    return normalized;
}

std::string LoadTextFileContents(const char* path)
{
    CFile file;
    if (!file.Open(path, CFile::modeRead)) {
        return std::string();
    }

    const int size = file.Size();
    if (size <= 0) {
        return std::string();
    }

    std::string contents(static_cast<size_t>(size), '\0');
    if (!file.Read(&contents[0], static_cast<u32>(size))) {
        return std::string();
    }

    return contents;
}

const std::vector<std::string>& GetIndoorRswTable()
{
    static bool loaded = false;
    static std::vector<std::string> table;
    if (loaded) {
        return table;
    }

    loaded = true;
    const std::string contents = LoadTextFileContents("data\\indoorrswtable.txt");
    if (contents.empty()) {
        return table;
    }

    size_t lineStart = 0;
    while (lineStart < contents.size()) {
        size_t lineEnd = contents.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = contents.size();
        }

        std::string line = TrimAscii(contents.substr(lineStart, lineEnd - lineStart));
        if (!line.empty() && !(line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
            const size_t hashPos = line.find('#');
            if (hashPos != std::string::npos) {
                line.resize(hashPos);
            }

            line = NormalizeRswNameForCameraTables(line.c_str());
            if (!line.empty()) {
                table.push_back(line);
            }
        }

        lineStart = contents.find_first_not_of("\r\n", lineEnd);
        if (lineStart == std::string::npos) {
            break;
        }
    }

    std::sort(table.begin(), table.end());
    table.erase(std::unique(table.begin(), table.end()), table.end());
    return table;
}

const std::map<std::string, ViewPointConstraintData>& GetViewPointTable()
{
    static bool loaded = false;
    static std::map<std::string, ViewPointConstraintData> table;
    if (loaded) {
        return table;
    }

    loaded = true;
    const std::string contents = LoadTextFileContents("data\\viewpointtable.txt");
    if (contents.empty()) {
        return table;
    }

    size_t lineStart = 0;
    while (lineStart < contents.size()) {
        size_t lineEnd = contents.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = contents.size();
        }

        const std::string rawLine = TrimAscii(contents.substr(lineStart, lineEnd - lineStart));
        if (!rawLine.empty() && !(rawLine.size() >= 2 && rawLine[0] == '/' && rawLine[1] == '/')) {
            std::vector<std::string> fields;
            size_t fieldStart = 0;
            while (fieldStart <= rawLine.size()) {
                size_t fieldEnd = rawLine.find('#', fieldStart);
                if (fieldEnd == std::string::npos) {
                    fieldEnd = rawLine.size();
                }
                fields.push_back(TrimAscii(rawLine.substr(fieldStart, fieldEnd - fieldStart)));
                if (fieldEnd == rawLine.size()) {
                    break;
                }
                fieldStart = fieldEnd + 1;
            }

            if (fields.size() >= 10) {
                const std::string key = NormalizeRswNameForCameraTables(fields[0].c_str());
                if (!key.empty()) {
                    ViewPointConstraintData data{};
                    data.minDistance = std::atoi(fields[1].c_str());
                    data.distanceScope = std::atoi(fields[2].c_str());
                    data.initialDistance = std::atoi(fields[3].c_str());
                    data.minLongitude = std::atoi(fields[4].c_str());
                    data.maxLongitude = std::atoi(fields[5].c_str());
                    data.initialLongitude = std::atoi(fields[6].c_str());
                    data.maxLatitude = std::atoi(fields[7].c_str());
                    data.minLatitude = std::atoi(fields[8].c_str());
                    data.initialLatitude = std::atoi(fields[9].c_str());
                    table[key] = data;
                }
            }
        }

        lineStart = contents.find_first_not_of("\r\n", lineEnd);
        if (lineStart == std::string::npos) {
            break;
        }
    }

    return table;
}

bool IsIndoorRswName(const char* rswName)
{
    const std::string key = NormalizeRswNameForCameraTables(rswName);
    if (key.empty()) {
        return false;
    }

    const std::vector<std::string>& table = GetIndoorRswTable();
    return std::binary_search(table.begin(), table.end(), key);
}

const ViewPointConstraintData* FindViewPointConstraintData(const char* rswName)
{
    const std::string key = NormalizeRswNameForCameraTables(rswName);
    if (key.empty()) {
        return nullptr;
    }

    const std::map<std::string, ViewPointConstraintData>& table = GetViewPointTable();
    const auto found = table.find(key);
    return found != table.end() ? &found->second : nullptr;
}

void SaveCameraStateForMap(const CGameMode& mode)
{
    if (!mode.m_view) {
        return;
    }

    if (IsIndoorRswName(mode.m_rswName)) {
        g_savedIndoorCameraLatitude = mode.m_view->GetTargetCameraLatitude();
        g_savedIndoorCameraDistance = mode.m_view->GetTargetCameraDistance();
    } else {
        g_savedOutdoorCameraLatitude = mode.m_view->GetTargetCameraLatitude();
        g_savedOutdoorCameraDistance = mode.m_view->GetTargetCameraDistance();
    }
}

void ConfigureCameraForMap(CGameMode& mode)
{
    if (!mode.m_view) {
        return;
    }

    const bool isIndoor = IsIndoorRswName(mode.m_rswName);
    const ViewPointConstraintData* viewPoint = FindViewPointConstraintData(mode.m_rswName);

    mode.m_hasViewPoint = viewPoint ? 1 : 0;
    std::memset(mode.ViewPointData, 0, sizeof(mode.ViewPointData));

    CView::CameraConstraints constraints{};
    constraints.minDistance = kRefCloseCameraDistance;
    constraints.maxDistance = isIndoor ? kRefIndoorCameraDistance : kRefFarCameraDistance;
    constraints.defaultDistance = isIndoor ? g_savedIndoorCameraDistance : g_savedOutdoorCameraDistance;
    constraints.minLatitude = isIndoor ? kRefIndoorLatitudeMin : kRefOutdoorLatitudeMin;
    constraints.maxLatitude = isIndoor ? kRefIndoorLatitudeMax : kRefOutdoorLatitudeMax;
    constraints.defaultLatitude = isIndoor ? g_savedIndoorCameraLatitude : g_savedOutdoorCameraLatitude;
    constraints.defaultLongitude = isIndoor ? 45.0f : 0.0f;
    constraints.lockLongitude = isIndoor;
    constraints.constrainLongitude = false;
    constraints.minLongitude = -360.0f;
    constraints.maxLongitude = 360.0f;

    if (viewPoint) {
        mode.ViewPointData[0] = static_cast<s16>(viewPoint->minDistance);
        mode.ViewPointData[1] = static_cast<s16>(viewPoint->distanceScope);
        mode.ViewPointData[2] = static_cast<s16>(viewPoint->initialDistance);
        mode.ViewPointData[3] = static_cast<s16>(viewPoint->minLongitude);
        mode.ViewPointData[4] = static_cast<s16>(viewPoint->maxLongitude);
        mode.ViewPointData[5] = static_cast<s16>(viewPoint->initialLongitude);
        mode.ViewPointData[6] = static_cast<s16>(viewPoint->maxLatitude);
        mode.ViewPointData[7] = static_cast<s16>(viewPoint->minLatitude);
        mode.ViewPointData[8] = static_cast<s16>(viewPoint->initialLatitude);

        constraints.minDistance = static_cast<float>(viewPoint->minDistance);
        constraints.maxDistance = static_cast<float>(viewPoint->minDistance + viewPoint->distanceScope);
        constraints.defaultDistance = static_cast<float>(viewPoint->initialDistance);
        constraints.minLatitude = static_cast<float>((std::min)(viewPoint->minLatitude, viewPoint->maxLatitude));
        constraints.maxLatitude = static_cast<float>((std::max)(viewPoint->minLatitude, viewPoint->maxLatitude));
        constraints.defaultLatitude = static_cast<float>(viewPoint->initialLatitude);
        constraints.defaultLongitude = static_cast<float>(viewPoint->initialLongitude);
        constraints.lockLongitude = viewPoint->minLongitude == viewPoint->maxLongitude;
        constraints.constrainLongitude = !(viewPoint->minLongitude <= -360 && viewPoint->maxLongitude >= 360);
        constraints.minLongitude = static_cast<float>((std::min)(viewPoint->minLongitude, viewPoint->maxLongitude));
        constraints.maxLongitude = static_cast<float>((std::max)(viewPoint->minLongitude, viewPoint->maxLongitude));
    }

    constraints.defaultDistance = (std::max)(constraints.minDistance, (std::min)(constraints.maxDistance, constraints.defaultDistance));
    constraints.defaultLatitude = (std::max)(constraints.minLatitude, (std::min)(constraints.maxLatitude, constraints.defaultLatitude));

    mode.m_fixedLongitude = constraints.defaultLongitude;
    mode.m_view->SetCameraConstraints(constraints);
    mode.m_view->SetInitialCamera(constraints.defaultLongitude, constraints.defaultLatitude, constraints.defaultDistance);
}

struct FramePerfStats {
    u64 frames = 0;
    u64 updateMs = 0;
    u64 processUiMs = 0;
    u64 renderPrepMs = 0;
    u64 drawSceneMs = 0;
    u64 uiDrawMs = 0;
    u64 flipMs = 0;
};

void LogNearbyBackgroundActorsOnce(const CGameMode& mode)
{
    static std::string loggedWorld;
    if (!mode.m_world || !mode.m_world->m_player || mode.m_rswName[0] == '\0') {
        return;
    }
    if (loggedWorld == mode.m_rswName) {
        return;
    }

    const vector3d playerPos = mode.m_world->m_player->m_pos;
    struct NearbyActorInfo {
        float distSq;
        const C3dActor* actor;
    };

    std::vector<NearbyActorInfo> nearestActors;
    nearestActors.reserve(mode.m_world->m_bgObjList.size());
    for (const C3dActor* actor : mode.m_world->m_bgObjList) {
        if (!actor) {
            continue;
        }

        const float dx = actor->m_pos.x - playerPos.x;
        const float dz = actor->m_pos.z - playerPos.z;
        nearestActors.push_back(NearbyActorInfo{ dx * dx + dz * dz, actor });
    }

    std::sort(nearestActors.begin(), nearestActors.end(), [](const NearbyActorInfo& a, const NearbyActorInfo& b) {
        return a.distSq < b.distSq;
    });

    DbgLog("[GameMode] nearby-bg playerWorld=(%.2f,%.2f,%.2f) map='%s' total=%u\n",
        playerPos.x,
        playerPos.y,
        playerPos.z,
        mode.m_rswName,
        static_cast<unsigned>(mode.m_world->m_bgObjList.size()));

    const size_t count = (std::min)(nearestActors.size(), kNearbyBackgroundLogCount);
    for (size_t index = 0; index < count; ++index) {
        const C3dActor* actor = nearestActors[index].actor;
        DbgLog("[GameMode]   nearby-bg[%u] dist=%.2f name='%s' model='%s' node='%s' pos=(%.2f,%.2f,%.2f) scale=(%.3f,%.3f,%.3f) radius=%.2f\n",
            static_cast<unsigned>(index),
            std::sqrt(nearestActors[index].distSq),
            actor->m_name,
            actor->m_debugModelPath.c_str(),
            actor->m_debugNodeName.c_str(),
            actor->m_pos.x,
            actor->m_pos.y,
            actor->m_pos.z,
            actor->m_scale.x,
            actor->m_scale.y,
            actor->m_scale.z,
            actor->m_boundRadius);
    }

    loggedWorld = mode.m_rswName;
}

FramePerfStats g_framePerfStats;

struct UpdatePerfStats {
    u64 frames = 0;
    u64 packetMs = 0;
    u64 actorSummaryMs = 0;
    u64 bootstrapMs = 0;
    u64 actorUpdateMs = 0;
    u64 backgroundUpdateMs = 0;
    u64 ensureViewMs = 0;
    u64 hoverUpdateMs = 0;
    u64 heldMoveMs = 0;
    u64 viewCalcMs = 0;
    u64 packetsProcessed = 0;
};

UpdatePerfStats g_updatePerfStats;

struct GameModeMovePerfStats {
    u64 frames = 0;
    u64 skillRechargeCalls = 0;
    u64 cursorCalls = 0;
    double skillRechargeMs = 0.0;
    double cursorMs = 0.0;
};

GameModeMovePerfStats g_gameModeMovePerfStats;

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

bool BlitArgbBitsToWindow(HWND hwnd, const void* bits, int width, int height)
{
    if (!hwnd || !bits || width <= 0 || height <= 0) {
        return false;
    }

    HDC targetDc = GetDC(hwnd);
    if (!targetDc) {
        return GetRenderDevice().UpdateBackBufferFromMemory(bits, width, height, width * static_cast<int>(sizeof(unsigned int)));
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const bool success = StretchDIBits(targetDc,
                                       0,
                                       0,
                                       width,
                                       height,
                                       0,
                                       0,
                                       width,
                                       height,
                                       bits,
                                       &bmi,
                                       DIB_RGB_COLORS,
                                       SRCCOPY) != GDI_ERROR;
    ReleaseDC(hwnd, targetDc);
    return success;
}

bool IsMovePerfActive(const CGameMode& mode)
{
    return mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_isMoving;
}

void LogGameModeMovePerfIfNeeded()
{
    if (g_gameModeMovePerfStats.frames == 0 || (g_gameModeMovePerfStats.frames % 30u) != 0) {
        return;
    }

    const double frameCount = static_cast<double>(g_gameModeMovePerfStats.frames);
    const double skillCalls = static_cast<double>((std::max)(u64{1}, g_gameModeMovePerfStats.skillRechargeCalls));
    const double cursorCalls = static_cast<double>((std::max)(u64{1}, g_gameModeMovePerfStats.cursorCalls));
    DbgLog("[GameModePerfHiRes] moveFrames=%llu skillRecharge=%.3fms skillCalls=%llu skillCall=%.3fms cursor=%.3fms cursorCalls=%llu cursorCall=%.3fms\n",
        static_cast<unsigned long long>(g_gameModeMovePerfStats.frames),
        g_gameModeMovePerfStats.skillRechargeMs / frameCount,
        static_cast<unsigned long long>(g_gameModeMovePerfStats.skillRechargeCalls),
        g_gameModeMovePerfStats.skillRechargeMs / skillCalls,
        g_gameModeMovePerfStats.cursorMs / frameCount,
        static_cast<unsigned long long>(g_gameModeMovePerfStats.cursorCalls),
        g_gameModeMovePerfStats.cursorMs / cursorCalls);
    g_gameModeMovePerfStats = GameModeMovePerfStats{};
}

void SendActorNameRequest(CGameMode& mode, u32 gid)
{
    if (gid == 0 || gid == g_session.m_gid) {
        return;
    }

    const u32 now = GetTickCount();
    const auto timerIt = mode.m_actorNameByGIDReqTimer.find(gid);
    if (timerIt != mode.m_actorNameByGIDReqTimer.end() && now - timerIt->second < kHoverNameRequestCooldownMs) {
        return;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    bool sent = false;
    if (ro::net::IsLegacyMapGameplaySendProfile()) {
        PACKET_CZ_REQNAME_LEGACY packet{};
        packet.PacketType = profile.getCharNameRequest;
        packet.GID = gid;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)));
    } else {
        PACKET_CZ_REQNAME2 packet{};
        packet.PacketType = profile.getCharNameRequest;
        FillPacketPadding(packet.padding, static_cast<int>(sizeof(packet.padding)));
        packet.GID = gid;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)));
    }
    DbgLog("[GameMode] hover actor name request opcode=0x%04X gid=%u sent=%d\n",
        profile.getCharNameRequest,
        gid,
        sent ? 1 : 0);
    if (sent) {
        mode.m_actorNameByGIDReqTimer[gid] = now;
    }
}

bool IsMonsterLikeHoverActor(const CGameActor* actor)
{
    if (!actor || actor->m_isPc != 0) {
        return false;
    }

    const int job = actor->m_job;
    return job >= 1000 && (job < 6001 || job > 6047);
}

bool IsNpcLikeHoverActor(const CGameActor* actor)
{
    if (!actor || actor->m_isPc) {
        return false;
    }

    const int job = actor->m_job;
    if (job >= 45 && job < 1000) {
        return true;
    }

    return actor->m_objectType == 6;
}

bool IsPcLikeHoverActor(const CGameActor* actor)
{
    return actor && actor->m_isPc;
}

enum class PetCaptureActorKind { None, Npc, Monster, Pc, Homun, Companion };
PetCaptureActorKind ClassifyPetCaptureActor(const CGameActor* actor)
{
    if(!actor)return PetCaptureActorKind::None;
    const int job=actor->m_job;
    // Source class4 overrides the marked-pet class3 assignment.
    if(actor->m_objectType==9 || (job>6000 && job<6048))
        return job>6000 && job<=6016?PetCaptureActorKind::Homun:PetCaptureActorKind::Companion;
    if(job>1000 && job<4000 && actor->m_headType==100)return PetCaptureActorKind::None;
    if(job==45)return PetCaptureActorKind::None;
    if(actor->m_isPc)return PetCaptureActorKind::Pc;
    if(job>45 && job<1000)return PetCaptureActorKind::Npc;
    if(job>1000 && job<4000)return PetCaptureActorKind::Monster;
    return PetCaptureActorKind::None;
}

void SetModeCursorAction(CGameMode& mode, CursorAction cursorActNum)
{
    mode.SetCursorAction(cursorActNum);
}

constexpr int kSkillInfGroundSkill = 0x02;
constexpr int kSkillInfSelfSkill = 0x04;
constexpr int kSkillInfTargetSkill = 0x20; // Ref SKILLTYPE_TOSKILL / SUT_TO_SKILL
constexpr u16 kSkillMerchantChangeCart = 154;
constexpr int kEffectStatePushCartMask = 0x0008 | 0x0080 | 0x0100 | 0x0200 | 0x0400;

enum class ShortcutSkillUseMode {
    ActorTarget,
    GroundTarget,
    SkillTarget,
    ImmediateSelf,
    CartSelect,
};

ShortcutSkillUseMode ResolveShortcutSkillUseMode(u16 skillId)
{
    if(skillId==10000)return ShortcutSkillUseMode::ActorTarget;
    if (skillId == kSkillMerchantChangeCart) {
        return ShortcutSkillUseMode::CartSelect;
    }

    const PLAYER_SKILL_INFO* skillInfo = g_session.GetSkillItemBySkillId(static_cast<int>(skillId));
    if (!skillInfo) {
        return ShortcutSkillUseMode::ActorTarget;
    }

    const int skillInf = skillInfo->type;
    if ((skillInf & kSkillInfGroundSkill) != 0) {
        return ShortcutSkillUseMode::GroundTarget;
    }
    if (skillInf == kSkillInfTargetSkill) {
        return ShortcutSkillUseMode::SkillTarget;
    }
    if ((skillInf & kSkillInfSelfSkill) != 0) {
        return ShortcutSkillUseMode::ImmediateSelf;
    }
    return ShortcutSkillUseMode::ActorTarget;
}

ShortcutSkillUseMode ResolvePendingSkillUseMode(const CGameMode& mode)
{
    if(mode.m_skillUseInfo.useType==1)return ShortcutSkillUseMode::GroundTarget;
    if(mode.m_skillUseInfo.useType==2 || mode.m_skillUseInfo.useType==4)return ShortcutSkillUseMode::ActorTarget;
    if(mode.m_skillUseInfo.useType==5)return ShortcutSkillUseMode::SkillTarget;
    return ResolveShortcutSkillUseMode(static_cast<u16>(mode.m_skillUseInfo.id));
}

bool IsPushCartEffectState(int effectState)
{
    return (effectState & kEffectStatePushCartMask) != 0;
}

const CGameActor* ResolveLocalPlayerActor()
{
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || !gameMode->m_world) {
        return nullptr;
    }

    return gameMode->m_world->m_player;
}

bool LocalPlayerHasPushCart()
{
    const CGameActor* actor = ResolveLocalPlayerActor();
    return actor && IsPushCartEffectState(actor->m_effectState);
}

bool OpenSelectCartWindow()
{
    return g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTCARTWND) != nullptr;
}

bool HandleChangeCartSkillUiRequest(CGameMode& mode)
{
    ClearPendingSkillUse(mode);
    ClearSkillChase(mode);
    mode.m_lastMonGid = 0;
    mode.m_lastLockOnMonGid = 0;
    ClearAttackChaseHint(mode);

    if (!LocalPlayerHasPushCart()) {
        g_windowMgr.PushChatEvent("You must have a Pushcart.", 0x00FF4040u, 6);
        return false;
    }

    return OpenSelectCartWindow();
}

int ResolveSkillUseRange(u16 skillId)
{
    const PLAYER_SKILL_INFO* skillInfo = g_session.GetSkillItemBySkillId(static_cast<int>(skillId));
    if (!skillInfo) {
        return 1;
    }

    return (std::max)(1, skillInfo->attackRange);
}

u32 ResolveSelfSkillTargetId()
{
    if (g_session.m_aid != 0) {
        return g_session.m_aid;
    }
    if (g_session.m_gid != 0) {
        return g_session.m_gid;
    }
    return 0;
}

u32 ResolveSelectedPcSkillTargetId(const CGameMode* gameMode)
{
    if (!gameMode) {
        return 0;
    }

    const u32 targetGid = gameMode->m_selectedPcTargetGid;
    if (targetGid == 0) {
        return 0;
    }

    if (targetGid == ResolveSelfSkillTargetId()) {
        return targetGid;
    }

    const auto actorIt = gameMode->m_runtimeActors.find(targetGid);
    if (actorIt == gameMode->m_runtimeActors.end() || !actorIt->second) {
        return 0;
    }

    const CGameActor* actor = actorIt->second;
    return (actor->m_isPc && actor->m_isVisible) ? targetGid : 0;
}

CGameActor* ResolvePcSkillTargetActor(CGameMode& mode, u32 targetGid)
{
    if (targetGid == 0) {
        return nullptr;
    }

    if (mode.m_world && mode.m_world->m_player && targetGid == ResolveSelfSkillTargetId()) {
        return mode.m_world->m_player;
    }

    const auto actorIt = mode.m_runtimeActors.find(targetGid);
    if (actorIt == mode.m_runtimeActors.end()) {
        return nullptr;
    }

    CGameActor* actor = actorIt->second;
    return (actor && actor->m_isPc) ? actor : nullptr;
}

bool TryUseSelectedPcTargetSkill(CGameMode& mode, u16 skillId, u16 skillLevel)
{
    const u32 targetGid = ResolveSelectedPcSkillTargetId(&mode);
    if (targetGid == 0) {
        return false;
    }

    CGameActor* targetActor = ResolvePcSkillTargetActor(mode, targetGid);
    if (!targetActor) {
        return false;
    }

    const int skillRange = ResolveSkillUseRange(skillId);
    mode.m_lastMonGid = 0;
    mode.m_lastLockOnMonGid = 0;

    if (IsAttackTargetWithinRange(mode, *targetActor, skillRange)
        && IsServerCellWithinSkillRangeOfActor(mode, *targetActor, skillRange)) {
        if (SendUseSkillToIdPacket(skillId, skillLevel, targetGid)) {
            ClearSkillChase(mode);
            return true;
        }
        return false;
    }

    BeginSkillChase(mode, targetGid, skillId, skillLevel, skillRange);
    ClearAttackChaseHint(mode);
    DbgLog("[GameMode] selected pc skill chase armed skillId=%u level=%u target=%u range=%d\n",
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(skillLevel),
        static_cast<unsigned int>(targetGid),
        skillRange);
    return true;
}

bool TrySelectPcTargetFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_world || !mode.m_view || mode.m_canRotateView) {
        return false;
    }

    CGameActor* hoveredActor = nullptr;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        screenX,
        screenY,
        &hoveredActor,
        nullptr,
        nullptr)) {
        return false;
    }

    const int logicalX=IsQtUiRuntimeEnabled()?UiScaleRawToLogicalCoordinate(screenX):screenX;
    const int logicalY=IsQtUiRuntimeEnabled()?UiScaleRawToLogicalCoordinate(screenY):screenY;
    if(mode.OpenPetContextMenu(hoveredActor,logicalX,logicalY))return true;
    if (!IsPcLikeHoverActor(hoveredActor) || IsMonsterLikeHoverActor(hoveredActor) || IsNpcLikeHoverActor(hoveredActor)) {
        return false;
    }

    const bool isSelfTarget = hoveredActor == mode.m_world->m_player
        || hoveredActor->m_gid == g_session.m_gid
        || hoveredActor->m_gid == g_session.m_aid;
    mode.m_selectedPcTargetGid = isSelfTarget
        ? ResolveSelfSkillTargetId()
        : hoveredActor->m_gid;
    mode.m_lastMonGid = 0;
    mode.m_lastLockOnMonGid = 0;
    ClearAttackChaseHint(mode);
    DbgLog("[GameMode] selected pc target gid=%u\n", static_cast<unsigned int>(mode.m_selectedPcTargetGid));
    return true;
}

void UpdateGameplayCursor(CGameMode& mode)
{
    if (mode.m_canRotateView) {
        SetModeCursorAction(mode, CursorAction::CameraPan);
        return;
    }

    if (!mode.m_world || !mode.m_view) {
        SetModeCursorAction(mode, CursorAction::Arrow);
        return;
    }

    if (IsWorldHoverBlockedByUi(mode.m_oldMouseX, mode.m_oldMouseY)) {
        SetModeCursorAction(mode, CursorAction::Arrow);
        return;
    }

    int hoverAttrX = -1;
    int hoverAttrY = -1;
    const bool hasHoverAttrCell = mode.m_view->ScreenToHoveredAttrCell(mode.m_oldMouseX, mode.m_oldMouseY, &hoverAttrX, &hoverAttrY);

    CGameActor* hoveredActor = nullptr;
    const bool hasHoveredActor = mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        mode.m_oldMouseX,
        mode.m_oldMouseY,
        &hoveredActor,
        nullptr,
        nullptr,
        HasPendingSkillUse(mode)
            && ResolvePendingSkillUseMode(mode) == ShortcutSkillUseMode::ActorTarget);

    if (HasPendingSkillUse(mode)) {
        const ShortcutSkillUseMode skillUseMode = ResolvePendingSkillUseMode(mode);
        switch (skillUseMode) {
        case ShortcutSkillUseMode::GroundTarget:
            SetModeCursorAction(mode, hasHoverAttrCell ? CursorAction::SkillTarget : CursorAction::Forbidden);
            return;
        case ShortcutSkillUseMode::ActorTarget:
            if(mode.m_skillUseInfo.useType==2 || mode.m_skillUseInfo.useType==4){
                const auto kind=ClassifyPetCaptureActor(hoveredActor);
                const bool shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;
                const bool good=SKILL_INFO::IsGoodSkillForActor(mode.m_skillUseInfo.id);
                const bool pcTarget=(good?!shift:shift) || g_session.m_isNoShift==1
                    || (hoveredActor && hoveredActor->m_pkState) || (mode.m_world && mode.m_world->m_isPKZone);
                const bool monsterTarget=(good?shift:!shift) || g_session.m_isNoShift==1;
                SetModeCursorAction(mode,kind==PetCaptureActorKind::Monster?(monsterTarget?CursorAction::SkillTargetAlt:CursorAction::SkillTarget)
                    :kind==PetCaptureActorKind::Npc?CursorAction::Talk
                    :kind==PetCaptureActorKind::Pc?(pcTarget?CursorAction::SkillTargetAlt:CursorAction::SkillTarget)
                    :hoveredActor && hoveredActor->m_job==45?CursorAction::Portal:CursorAction::Arrow);
            }else if (hasHoveredActor && hoveredActor && !IsNpcLikeHoverActor(hoveredActor)) {
                SetModeCursorAction(mode, CursorAction::SkillTargetAlt);
            } else {
                SetModeCursorAction(mode, CursorAction::Forbidden);
            }
            return;
        case ShortcutSkillUseMode::SkillTarget: {
            CSkill* hoveredSkill = nullptr;
            if (mode.m_world->FindHoveredSkillScreen(mode.m_view->GetViewMatrix(),
                mode.m_oldMouseX,
                mode.m_oldMouseY,
                &hoveredSkill)
                && hoveredSkill) {
                SetModeCursorAction(mode, CursorAction::SkillTargetAlt);
            } else {
                SetModeCursorAction(mode, CursorAction::Forbidden);
            }
            return;
        }
        case ShortcutSkillUseMode::ImmediateSelf:
        case ShortcutSkillUseMode::CartSelect:
            break;
        }
    }

    if (!hasHoveredActor) {
        CItem* hoveredItem = nullptr;
        if (mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
            mode.m_oldMouseX,
            mode.m_oldMouseY,
            &hoveredItem,
            nullptr,
            nullptr)
            && hoveredItem) {
            SetModeCursorAction(mode, CursorAction::Item);
            return;
        }
        if (hasHoverAttrCell && mode.m_world->HasWarpAtAttrCell(hoverAttrX, hoverAttrY)) {
            SetModeCursorAction(mode, CursorAction::Portal);
            return;
        }
        if (hasHoverAttrCell && !IsWalkableAttrCell(mode, hoverAttrX, hoverAttrY)) {
            SetModeCursorAction(mode, CursorAction::Forbidden);
            return;
        }
        SetModeCursorAction(mode, CursorAction::Arrow);
        return;
    }

    if (IsMonsterLikeHoverActor(hoveredActor)) {
        SetModeCursorAction(mode, CursorAction::Attack);
        return;
    }

    if (IsNpcLikeHoverActor(hoveredActor)) {
        SetModeCursorAction(mode, CursorAction::Talk);
        return;
    }

    CItem* hoveredItem = nullptr;
    if (mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
        mode.m_oldMouseX,
        mode.m_oldMouseY,
        &hoveredItem,
        nullptr,
        nullptr)
        && hoveredItem) {
        SetModeCursorAction(mode, CursorAction::Item);
        return;
    }

    if (hasHoverAttrCell && mode.m_world->HasWarpAtAttrCell(hoverAttrX, hoverAttrY)) {
        SetModeCursorAction(mode, CursorAction::Portal);
        return;
    }

    SetModeCursorAction(mode, CursorAction::Arrow);
}

bool ShouldUseServerNameForHoverActor(const CGameActor* actor)
{
    if (!actor) {
        return false;
    }

    if (actor->m_isPc) {
        return true;
    }

    if (IsMonsterLikeHoverActor(actor)) {
        return true;
    }

    return actor->m_objectType == 6;
}

std::string ResolveOwnedCompanionHoverName(const CGameActor* actor)
{
    if (!actor || actor->m_gid == 0) {
        return std::string();
    }

    // Companion windows already show m_homunStatus/m_merStatus.name from 0x022E / 0x029B.
    // Hover must use those custom names too — not JobNameTable stems (ARCHER06, AMISTR, ...).
    if (g_session.m_homunStatus.alive
        && actor->m_gid == g_session.m_homunGID
        && !g_session.m_homunStatus.name.empty()) {
        return g_session.m_homunStatus.name;
    }
    if (g_session.m_merStatus.alive
        && actor->m_gid == g_session.m_merGID
        && !g_session.m_merStatus.name.empty()) {
        return g_session.m_merStatus.name;
    }
    return std::string();
}

std::string ResolveHoveredActorName(CGameMode& mode, CGameActor* actor)
{
    if (!actor) {
        return std::string();
    }

    if (actor->m_gid == g_session.m_gid) {
        const char* playerName = g_session.GetPlayerName();
        if (playerName && *playerName) {
            return playerName;
        }
    }

    if (const std::string companionName = ResolveOwnedCompanionHoverName(actor); !companionName.empty()) {
        return companionName;
    }

    const auto cachedNameIt = mode.m_actorNameListByGID.find(actor->m_gid);
    if (cachedNameIt != mode.m_actorNameListByGID.end() && !cachedNameIt->second.name.empty()) {
        return cachedNameIt->second.name;
    }

    if (ShouldUseServerNameForHoverActor(actor)) {
        SendActorNameRequest(mode, actor->m_gid);
        if (actor->m_isPc) {
            return "Player";
        }
        if (IsMonsterLikeHoverActor(actor)) {
            return "Monster";
        }
        return "NPC";
    }

    const char* jobName = g_session.GetJobName(actor->m_job);
    if (jobName && *jobName) {
        return jobName;
    }

    return "Entity";
}

std::string FormatPlayerContextLabel(const char* format, const std::string& playerName)
{
    if (!format) {
        return std::string();
    }

    char buffer[256] = {};
    std::snprintf(buffer, sizeof(buffer), format, playerName.c_str());
    return buffer;
}

std::vector<PlayerContextMenuEntry> BuildPlayerContextMenuEntries(const std::string& playerName)
{
    return {
        { CGameMode::PlayerContext_CheckEquipmentInfo, FormatPlayerContextLabel("Check %s's Equipment Info", playerName) },
        { CGameMode::PlayerContext_RequestDeal, FormatPlayerContextLabel("Request a deal with (%s)", playerName) },
        { CGameMode::PlayerContext_AskJoinParty, FormatPlayerContextLabel("Ask (%s) to join your party", playerName) },
        { CGameMode::PlayerContext_InviteJoinGuild, FormatPlayerContextLabel("Invite (%s) to join your guild", playerName) },
        { CGameMode::PlayerContext_SendWhisper, "Send whisper to player" },
        { CGameMode::PlayerContext_RegisterFriend, "Register as a Friend" },
        { CGameMode::PlayerContext_SendMail, "Send a mail" },
        { g_session.IsExName(playerName.c_str())?CGameMode::PlayerContext_AllowWhispering:CGameMode::PlayerContext_RejectWhispering,
          g_session.IsExName(playerName.c_str())?"Allow Whispering":"Reject Whispering" },
    };
}

bool CanRequestGuildRelation(const CGameMode& mode, u32 targetAid, bool hostile)
{
    const auto& guild = g_session.m_localGuild;
    if (!guild.guildId || !guild.isMaster || !mode.m_world
        || !targetAid || targetAid == g_session.m_aid || targetAid == g_session.m_gid) return false;
    const auto* actor = mode.m_world->GetGameActorByAID(targetAid);
    if (!actor || !IsPcLikeHoverActor(actor) || actor == mode.m_world->m_player
        || actor->m_gdid <= 0 || static_cast<u32>(actor->m_gdid) == guild.guildId) return false;
    const auto& relations = g_session.m_guildWindow.relations;
    return std::none_of(relations.begin(), relations.end(), [&](const auto& relation) {
        return relation.id == static_cast<u32>(actor->m_gdid) && relation.hostile == hostile;
    });
}

bool TryOpenPlayerContextMenu(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_world || !mode.m_view) {
        return false;
    }
    if(GetAsyncKeyState(VK_SHIFT)&0x8000)return false;

    CGameActor* hoveredActor = nullptr;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        screenX,
        screenY,
        &hoveredActor,
        nullptr,
        nullptr)) {
        return false;
    }

    if (!IsPcLikeHoverActor(hoveredActor) || IsMonsterLikeHoverActor(hoveredActor) || IsNpcLikeHoverActor(hoveredActor)) {
        return false;
    }

    const bool isSelfTarget = hoveredActor == mode.m_world->m_player
        || hoveredActor->m_gid == g_session.m_gid
        || hoveredActor->m_gid == g_session.m_aid;
    if (isSelfTarget) {
        return false;
    }

    const int menuX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(screenX) : screenX;
    const int menuY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(screenY) : screenY;
    return mode.OpenPlayerContextMenu(hoveredActor->m_gid, ResolveHoveredActorName(mode, hoveredActor), menuX, menuY);
}

const char* GetPlayerContextActionName(CGameMode::PlayerContextAction action)
{
    switch (action) {
    case CGameMode::PlayerContext_CheckEquipmentInfo:
        return "check-equipment-info";
    case CGameMode::PlayerContext_RequestDeal:
        return "request-deal";
    case CGameMode::PlayerContext_AskJoinParty:
        return "ask-join-party";
    case CGameMode::PlayerContext_InviteJoinGuild:
        return "invite-join-guild";
    case CGameMode::PlayerContext_SendWhisper:
        return "send-whisper";
    case CGameMode::PlayerContext_RegisterFriend:
        return "register-friend";
    case CGameMode::PlayerContext_SendMail:
        return "send-mail";
    case CGameMode::PlayerContext_RejectWhispering:
        return "reject-whispering";
    case CGameMode::PlayerContext_AllowWhispering:
        return "allow-whispering";
    default:
        return "unknown";
    }
}

COLORREF ResolveHoverNameColor(const CGameActor* actor)
{
    if (!actor) {
        return RGB(255, 255, 255);
    }

    u32 actorId = actor->m_gid;
    if (actorId != 0 && (actorId == g_session.m_gid || actorId == g_session.m_aid)) {
        actorId = g_session.m_aid != 0 ? g_session.m_aid : actorId;
    }
    return IsNameYellow(actorId) ? RGB(255, 255, 0) : RGB(255, 255, 255);
}

std::string ResolveGroundItemHoverLabel(const CItem* item)
{
    if (!item) {
        return std::string();
    }

    std::string itemName = item->m_itemName;
    if (itemName.empty() && item->m_itemId != 0) {
        itemName = g_ttemmgr.GetDisplayName(item->m_itemId, item->m_identified != 0);
    }
    if (itemName.empty()) {
        itemName = "Item";
    }

    char amountText[64]{};
    std::snprintf(amountText, sizeof(amountText), "%s: %u ea", itemName.c_str(), static_cast<unsigned int>(item->m_amount));
    return amountText;
}

void DrawOutlinedScreenText(HDC hdc, int x, int y, const char* text, COLORREF color = RGB(255, 255, 255))
{
    if (!hdc || !text || !*text) {
        return;
    }

    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, kHoverNameFontHeight, kHoverNameFontBold);
    SetBkMode(hdc, TRANSPARENT);

    const int textLen = static_cast<int>(std::strlen(text));
    drawDc.SetTextColor(RGB(0, 0, 0));
    drawDc.TextOutA(x - 1, y, text, textLen);
    drawDc.TextOutA(x + 1, y, text, textLen);
    drawDc.TextOutA(x, y - 1, text, textLen);
    drawDc.TextOutA(x, y + 1, text, textLen);

    drawDc.SetTextColor(color);
    drawDc.TextOutA(x, y, text, textLen);
}

void DrawHoveredGroundItemName(CGameMode& mode, HDC hdc)
{
    if (!hdc || !mode.m_world || !mode.m_view) {
        return;
    }

    if (IsWorldHoverBlockedByUi(mode.m_oldMouseX, mode.m_oldMouseY)) {
        return;
    }

    CItem* hoveredItem = nullptr;
    int labelX = 0;
    int labelY = 0;
    if (!mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
        mode.m_oldMouseX,
        mode.m_oldMouseY,
        &hoveredItem,
        &labelX,
        &labelY)) {
        return;
    }

    const std::string label = ResolveGroundItemHoverLabel(hoveredItem);
    if (label.empty()) {
        return;
    }

    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, kHoverNameFontHeight, kHoverNameFontBold);
    SIZE textSize{};
    drawDc.GetTextExtentPoint32A(label.c_str(), static_cast<int>(label.size()), &textSize);
    const int drawX = labelX - (textSize.cx / 2);
    const int drawY = labelY - textSize.cy - kHoverNameTextPadding;
    DrawOutlinedScreenText(hdc, drawX, drawY, label.c_str(), RGB(255, 255, 255));
}

void DrawHoveredActorName(CGameMode& mode, HDC hdc)
{
    if (!hdc || !mode.m_world || !mode.m_view) {
        return;
    }

    if (IsWorldHoverBlockedByUi(mode.m_oldMouseX, mode.m_oldMouseY)) {
        return;
    }

    CItem* hoveredItem = nullptr;
    if (mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
        mode.m_oldMouseX,
        mode.m_oldMouseY,
        &hoveredItem,
        nullptr,
        nullptr)
        && hoveredItem) {
        return;
    }

    CGameActor* hoveredActor = nullptr;
    int labelX = 0;
    int labelY = 0;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        mode.m_oldMouseX,
        mode.m_oldMouseY,
        &hoveredActor,
        &labelX,
        &labelY)) {
        return;
    }

    if (hoveredActor && hoveredActor->m_gid == mode.m_lastLockOnMonGid) {
        return;
    }

    const std::string label = ResolveHoveredActorName(mode, hoveredActor);
    if (label.empty()) {
        return;
    }

    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, kHoverNameFontHeight, kHoverNameFontBold);
    SIZE textSize{};
    drawDc.GetTextExtentPoint32A(label.c_str(), static_cast<int>(label.size()), &textSize);
    const int textWidth = textSize.cx;
    const int drawX = labelX - (textWidth / 2);
    int drawY = labelY + kHoverNameTextPadding + kHoverNameVerticalOffset;
    if (ActorHasPlayerStyleVitalsOverlay(hoveredActor)
        || hoveredActor == mode.m_world->m_player) {
        drawY += ResolvePlayerStyleVitalsNameOffsetY(mode.m_view);
    }
    DrawOutlinedScreenText(hdc, drawX, drawY, label.c_str(), ResolveHoverNameColor(hoveredActor));
}

void DrawLockedTargetName(CGameMode& mode, HDC hdc)
{
    if (!hdc || !mode.m_world || !mode.m_view || mode.m_lastLockOnMonGid == 0) {
        return;
    }

    const auto actorIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
    if (actorIt == mode.m_runtimeActors.end() || !actorIt->second || !actorIt->second->m_isVisible) {
        return;
    }

    int labelX = 0;
    int topY = 0;
    int labelY = 0;
    if (!mode.m_world->GetActorScreenMarker(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        mode.m_lastLockOnMonGid,
        &labelX,
        &topY,
        &labelY)) {
        return;
    }

    const std::string label = ResolveHoveredActorName(mode, actorIt->second);
    if (label.empty()) {
        return;
    }

    DrawDC drawDc(hdc);
    drawDc.SetFont(FONT_DEFAULT, kHoverNameFontHeight, kHoverNameFontBold);
    SIZE textSize{};
    drawDc.GetTextExtentPoint32A(label.c_str(), static_cast<int>(label.size()), &textSize);
    const int drawX = labelX - (textSize.cx / 2);
    const int drawY = labelY + kHoverNameTextPadding + kHoverNameVerticalOffset;
    DrawOutlinedScreenText(hdc, drawX, drawY, label.c_str(), ResolveHoverNameColor(actorIt->second));
}

void DrawFallbackLockedTargetArrow(HDC hdc, int centerX, int tipY)
{
    if (!hdc) {
        return;
    }

    const POINT outer[] = {
        { centerX, tipY },
        { centerX - 7, tipY - 11 },
        { centerX + 7, tipY - 11 },
    };
    const POINT inner[] = {
        { centerX, tipY - 1 },
        { centerX - 4, tipY - 9 },
        { centerX + 4, tipY - 9 },
    };

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(32, 16, 16));
    HBRUSH outerBrush = CreateSolidBrush(RGB(255, 226, 120));
    HBRUSH innerBrush = CreateSolidBrush(RGB(255, 92, 92));
    if (!pen || !outerBrush || !innerBrush) {
        if (pen) {
            DeleteObject(pen);
        }
        if (outerBrush) {
            DeleteObject(outerBrush);
        }
        if (innerBrush) {
            DeleteObject(innerBrush);
        }
        return;
    }

    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, outerBrush);
    Polygon(hdc, outer, 3);
    SelectObject(hdc, innerBrush);
    Polygon(hdc, inner, 3);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(innerBrush);
    DeleteObject(outerBrush);
    DeleteObject(pen);
}

void DrawLockedTargetArrow(CGameMode& mode, HDC hdc)
{
    if (!hdc || !mode.m_world || !mode.m_view || mode.m_lastLockOnMonGid == 0) {
        return;
    }

    const auto actorIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
    if (actorIt == mode.m_runtimeActors.end() || !actorIt->second || !actorIt->second->m_isVisible) {
        return;
    }

    int centerX = 0;
    int labelY = 0;
    if (!mode.m_world->GetActorScreenMarker(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        mode.m_lastLockOnMonGid,
        &centerX,
        nullptr,
        &labelY)) {
        return;
    }

    static bool s_bitmapLoaded = false;
    static std::string s_bitmapPath;
    static std::vector<unsigned int> s_bitmapPixels;
    static int s_width = 0;
    static int s_height = 0;
    if (!s_bitmapLoaded) {
        s_bitmapLoaded = true;
        s_bitmapPath = ResolveDataPath(kLockedTargetArrowBitmapName, "bmp", {
            "",
            std::string(UiKorPrefix()),
            "texture\\",
            std::string(UiKorPrefix()) + "basic_interface\\",
            "data\\",
            "data\\texture\\",
            std::string("data\\") + UiKorPrefix(),
            std::string("data\\") + UiKorPrefix() + "basic_interface\\"
        });
        if (!s_bitmapPath.empty()) {
            u32* pixels = nullptr;
            if (LoadBgraPixelsFromGameData(s_bitmapPath.c_str(), &pixels, &s_width, &s_height)
                && pixels
                && s_width > 0
                && s_height > 0) {
                s_bitmapPixels.assign(
                    pixels,
                    pixels + static_cast<size_t>(s_width) * static_cast<size_t>(s_height));
                delete[] pixels;
                DbgLog("[GameMode] locked target arrow loaded path='%s' size=%dx%d\n",
                    s_bitmapPath.c_str(),
                    s_width,
                    s_height);
            } else {
                delete[] pixels;
                DbgLog("[GameMode] locked target arrow bitmap decode failed path='%s'\n",
                    s_bitmapPath.c_str());
            }
        } else {
            DbgLog("[GameMode] locked target arrow asset not found name='%s'\n",
                kLockedTargetArrowBitmapName);
        }
    }

    if (s_bitmapPixels.empty() || s_width <= 0 || s_height <= 0) {
        const u32 now = GetTickCount();
        const int bounce = static_cast<int>(std::lround((0.5f + 0.5f * std::sin(static_cast<float>(now) * kLockedTargetArrowBouncePerMs))
            * kLockedTargetArrowBouncePixels));
        const int tipY = labelY - kLockedTargetArrowBaseLift - bounce;
        DrawFallbackLockedTargetArrow(hdc, centerX, tipY);
        return;
    }

    const u32 now = GetTickCount();
    const int bounce = static_cast<int>(std::lround((0.5f + 0.5f * std::sin(static_cast<float>(now) * kLockedTargetArrowBouncePerMs))
        * kLockedTargetArrowBouncePixels));
    const int scaledWidth = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(s_width) * kLockedTargetArrowScale)));
    const int scaledHeight = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(s_height) * kLockedTargetArrowScale)));
    const int drawX = centerX - (scaledWidth / 2);
    const int drawY = labelY - kLockedTargetArrowBaseLift - scaledHeight - kLockedTargetArrowYOffset - bounce;

    AlphaBlendArgbToHdc(hdc, drawX, drawY, scaledWidth, scaledHeight, s_bitmapPixels.data(), s_width, s_height);
}

u32 PackLockedTargetColor(u8 alpha, u8 red, u8 green, u8 blue)
{
    return (static_cast<u32>(alpha) << 24)
        | (static_cast<u32>(red) << 16)
        | (static_cast<u32>(green) << 8)
        | static_cast<u32>(blue);
}

bool ProjectLockedTargetPoint(const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= kLockedTargetMarkerNearPlane) {
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
    const float depth = (1500.0f / (1500.0f - 10.0f)) * ((1.0f / oow) - 10.0f) * oow;
    if (!std::isfinite(outVertex->x) || !std::isfinite(outVertex->y) || !std::isfinite(depth)) {
        return false;
    }

    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, depth));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return true;
}

CTexture* GetLockedTargetMarkerTexture()
{
    static bool s_loggedFailure = false;
    CTexture* texture = g_texMgr.GetTexture(kLockedTargetTexturePath, false);
    if ((!texture || texture == &CTexMgr::s_dummy_texture) && !s_loggedFailure) {
        s_loggedFailure = true;
        DbgLog("[GameMode] failed to load locked target texture '%s'\n", kLockedTargetTexturePath);
    }
    return (!texture || texture == &CTexMgr::s_dummy_texture) ? nullptr : texture;
}

void SubmitLockedTargetMarkerQuad(const matrix& viewMatrix,
    CTexture* texture,
    const vector3d& center,
    float halfExtent,
    float rotationDegrees,
    u32 color)
{
    if (!texture || halfExtent <= 0.0f) {
        return;
    }

    const float radians = rotationDegrees * (3.14159265f / 180.0f);
    const float cosAngle = std::cos(radians);
    const float sinAngle = std::sin(radians);
    const vector3d axisA{ cosAngle * halfExtent, 0.0f, sinAngle * halfExtent };
    const vector3d axisB{ -sinAngle * halfExtent, 0.0f, cosAngle * halfExtent };
    const vector3d quad[4] = {
        vector3d{ center.x - axisA.x - axisB.x, center.y, center.z - axisA.z - axisB.z },
        vector3d{ center.x + axisA.x - axisB.x, center.y, center.z + axisA.z - axisB.z },
        vector3d{ center.x - axisA.x + axisB.x, center.y, center.z - axisA.z + axisB.z },
        vector3d{ center.x + axisA.x + axisB.x, center.y, center.z + axisA.z + axisB.z },
    };

    tlvertex3d centerVert{};
    if (!ProjectLockedTargetPoint(viewMatrix, center, &centerVert)) {
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
    face->destAlphaMode = D3DBLEND_ONE;
    face->alphaSortKey = centerVert.z;

    for (int index = 0; index < 4; ++index) {
        if (!ProjectLockedTargetPoint(viewMatrix, quad[index], &face->m_verts[index])) {
            return;
        }
        face->m_verts[index].z = (std::max)(0.0f, centerVert.z - kLockedTargetMarkerDepthBias);
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

    g_renderer.AddRP(face, 1);
}

void SubmitLockedTargetMarkerEffect(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_view || mode.m_lastLockOnMonGid == 0) {
        return;
    }

    const auto actorIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
    if (actorIt == mode.m_runtimeActors.end() || !actorIt->second || !actorIt->second->m_isVisible) {
        return;
    }

    CTexture* texture = GetLockedTargetMarkerTexture();
    if (!texture) {
        return;
    }

    vector3d markerCenter = actorIt->second->m_pos;
    if (mode.m_world->m_attr) {
        markerCenter.y = mode.m_world->m_attr->GetHeight(markerCenter.x, markerCenter.z);
    }
    markerCenter.y += kLockedTargetMarkerGroundYOffset;

    const u32 now = GetTickCount();
    const float spin = std::fmod(static_cast<float>(now) * kLockedTargetMarkerRotationPerMs, 360.0f);
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now) * 0.008f);
    const float halfExtent = (kLockedTargetMarkerBaseSize - 2.0f + pulse * 4.0f) * 0.5f;
    const u8 greenBlue = static_cast<u8>(140 + pulse * 30.0f);
    const u32 color = PackLockedTargetColor(220, 250, greenBlue, greenBlue);

    const matrix& viewMatrix = mode.m_view->GetViewMatrix();
    SubmitLockedTargetMarkerQuad(viewMatrix, texture, markerCenter, halfExtent, spin, color);
    SubmitLockedTargetMarkerQuad(viewMatrix, texture, markerCenter, halfExtent, spin + 45.0f, color);
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) {
        return;
    }
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

unsigned int PackColorRefArgb(COLORREF color)
{
    const unsigned int red = static_cast<unsigned int>(GetRValue(color));
    const unsigned int green = static_cast<unsigned int>(GetGValue(color));
    const unsigned int blue = static_cast<unsigned int>(GetBValue(color));
    return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

void FillSolidRectArgb(unsigned int* pixels, int width, int height, const RECT& rect, COLORREF color)
{
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    RECT clipped{
        (std::max)(0, static_cast<int>(rect.left)),
        (std::max)(0, static_cast<int>(rect.top)),
        (std::min)(static_cast<LONG>(width), rect.right),
        (std::min)(static_cast<LONG>(height), rect.bottom)
    };
    if (clipped.right <= clipped.left || clipped.bottom <= clipped.top) {
        return;
    }

    const unsigned int argb = PackColorRefArgb(color);
    for (LONG y = clipped.top; y < clipped.bottom; ++y) {
        unsigned int* row = pixels + static_cast<size_t>(y) * static_cast<size_t>(width);
        for (LONG x = clipped.left; x < clipped.right; ++x) {
            row[x] = argb;
        }
    }
}

void DrawPlayerVitalsOverlay(CGameMode& mode, HDC hdc)
{
    if (!hdc || !mode.m_world || !mode.m_view || !mode.m_world->m_player) {
        return;
    }

    int labelX = 0;
    int labelY = 0;
    if (!mode.m_world->GetPlayerScreenLabel(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        &labelX,
        &labelY)) {
        return;
    }

    CPlayer* player = mode.m_world->m_player;
    const s64 maxHp = player->m_MaxHp;
    const s64 maxSp = player->m_MaxSp;
    if (maxHp <= 0 && maxSp <= 0) {
        return;
    }

    const PlayerVitalsScreenMetrics metrics = ResolvePlayerVitalsScreenMetrics(mode.m_view);
    const int totalHeight = metrics.totalHeight;
    const int outerWidth = metrics.barWidth + metrics.border * 2;
    const int outerLeft = labelX - (outerWidth / 2);
    const int outerTop = labelY + metrics.topDelta;
    RECT outerRect{
        outerLeft,
        outerTop,
        outerLeft + outerWidth,
        outerTop + totalHeight,
    };
    FillSolidRect(hdc, outerRect, RGB(0, 0, 0));

    const int innerLeft = outerRect.left + metrics.border;
    const int innerTop = outerRect.top + metrics.border;
    RECT hpRect{ innerLeft, innerTop, innerLeft + metrics.barWidth, innerTop + metrics.barHeight };
    RECT spRect{
        innerLeft,
        hpRect.bottom + metrics.border,
        innerLeft + metrics.barWidth,
        hpRect.bottom + metrics.border + metrics.barHeight,
    };

    FillSolidRect(hdc, hpRect, RGB(255, 255, 255));
    FillSolidRect(hdc, spRect, RGB(255, 255, 255));

    if (maxHp > 0) {
        const s64 hp = (std::min)(static_cast<s64>(player->m_Hp), maxHp);
        const int hpFillWidth = VitalBarFill(hp, maxHp, metrics.barWidth);
        if (hpFillWidth > 0) {
            RECT hpFillRect{ hpRect.left, hpRect.top, hpRect.left + hpFillWidth, hpRect.bottom };
            const bool lowHp = hp * 4 < maxHp;
            FillSolidRect(hdc, hpFillRect, lowHp ? RGB(220, 32, 32) : RGB(48, 192, 48));
        }
    }

    if (maxSp > 0) {
        const s64 sp = (std::min)(static_cast<s64>(player->m_Sp), maxSp);
        const int spFillWidth = VitalBarFill(sp, maxSp, metrics.barWidth);
        if (spFillWidth > 0) {
            RECT spFillRect{ spRect.left, spRect.top, spRect.left + spFillWidth, spRect.bottom };
            FillSolidRect(hdc, spFillRect, RGB(48, 96, 220));
        }
    }

}

bool SendLoadEndAckPacket();
ULONG_PTR EnsureGdiplusStarted();
void SyncBootstrapSelfActorWorldPos(CGameMode& mode);
void SyncBootstrapRuntimeActorWorldPos(CGameMode& mode);
void EnsureRealView(CGameMode& mode);
void EnsureBootstrapWorldAssets(const CGameMode& mode);
struct BootstrapWorldCache;
BootstrapWorldCache& GetBootstrapWorldCache();
bool IsBootstrapWorldReady(const BootstrapWorldCache& cache);
float GetBootstrapWorldLoadProgress(const BootstrapWorldCache& cache);

enum class BootstrapLoadStage {
    ResolveWorld = 0,
    LoadAttr,
    ResolveGround,
    SampleGroundTextures,
    BuildGround,
    BuildBackgroundObjects,
    BuildFixedEffects,
    LoadMinimap,
    Complete,
};

bool IsMapLoadingActive(const CGameMode& mode)
{
    return mode.m_mapLoadingStage != CGameMode::MapLoading_None;
}

bool EndsWithMapExtension(const char* value, const char* extension)
{
    if (!value || !extension) {
        return false;
    }

    const size_t valueLen = std::strlen(value);
    const size_t extensionLen = std::strlen(extension);
    if (valueLen < extensionLen) {
        return false;
    }

    const size_t offset = valueLen - extensionLen;
    for (size_t i = 0; i < extensionLen; ++i) {
        const char lhs = static_cast<char>(std::tolower(static_cast<unsigned char>(value[offset + i])));
        const char rhs = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));
        if (lhs != rhs) {
            return false;
        }
    }

    return true;
}

void NormalizeSessionMapName(char* mapName)
{
    if (!mapName) {
        return;
    }

    while (mapName[0]) {
        if (EndsWithMapExtension(mapName, ".rsw") ||
            EndsWithMapExtension(mapName, ".gat") ||
            EndsWithMapExtension(mapName, ".gnd")) {
            mapName[std::strlen(mapName) - 4] = '\0';
            continue;
        }
        break;
    }
}

std::string ChooseRandomLoadingWallpaper()
{
    RefreshDefaultLoadingScreenList();
    const std::vector<std::string>& loadingScreens = GetLoadingScreenList();
    if (loadingScreens.empty()) {
        return {};
    }

    const DWORD tick = GetTickCount();
    const size_t index = static_cast<size_t>(tick % static_cast<DWORD>(loadingScreens.size()));
    return loadingScreens[index];
}

void UpdateMapLoadingUi(CGameMode& mode, const char* message, float progress)
{
    g_windowMgr.UpdateLoadingScreen(message ? message : "Loading...", progress);
}

void ReplayPersistedStatusVisuals(CGameMode& mode)
{
    // Map load deletes every world effect, including auras already restarted
    // on the new player. Status icons stay in the session and the server does
    // not send them again, so they have to be launched once the map is ready.
    CGameActor* actor = mode.m_world ? static_cast<CGameActor*>(mode.m_world->m_player) : nullptr;
    if (!actor) {
        const u32 selfActorId = g_session.m_aid != 0 ? g_session.m_aid : g_session.m_gid;
        if (selfActorId != 0) {
            auto it = mode.m_runtimeActors.find(selfActorId);
            if (it == mode.m_runtimeActors.end() && g_session.m_gid != 0 && g_session.m_gid != selfActorId) {
                it = mode.m_runtimeActors.find(g_session.m_gid);
            }
            if (it != mode.m_runtimeActors.end()) {
                actor = it->second;
            }
        }
    }
    if (!actor) {
        return;
    }

    g_session.PruneExpiredStatusIcons(g_session.GetServerTime());
    const std::vector<ACTIVE_STATUS_ICON>& icons = g_session.GetActiveStatusIcons();
    DbgLog("[GameMode] replay status visuals gid=%u count=%zu\n",
        actor->m_gid,
        icons.size());
    for (const ACTIVE_STATUS_ICON& icon : icons) {
        actor->SendMsg(actor, 110, icon.statusType, 1, 0);
    }

    // Ref GameModeShowEFST starts effect375 from OPT3_ASSUMPTIO after the
    // status-message pass. Message110 removes the previous effect instance,
    // so replay the persistent login status after those icon messages.
    const bool assumptioIconActive = std::any_of(icons.begin(), icons.end(),
        [](const ACTIVE_STATUS_ICON& icon) { return icon.statusType == 110; });
    const bool assumptioStateActive = (actor->m_effectState & 0x00000800) != 0;
    const bool actorHidden = (actor->m_effectState & (0x00000002 | 0x00000004)) != 0;
    const bool hasAssumptioEffect = std::any_of(actor->m_effectList.begin(), actor->m_effectList.end(),
        [](const CRagEffect* effect) { return effect != nullptr && effect->GetEffectType() == 375; });
    if (actor->m_isPc && !actorHidden && (assumptioStateActive || assumptioIconActive) && !hasAssumptioEffect) {
        actor->LaunchEffect(375, vector3d{}, 0.0f);
    }

    // The same cleanup deletes the level 99 rag effects. Message 126 refuses
    // to launch again while m_99 is already 99, so clear that and start over.
    // Ref CPlayer::ReLaunchBlurEffects does this when the session level is 99.
    auto restartLevel99Aura = [](CGameActor* candidate) {
        if (!candidate || candidate->m_clevel != 99) {
            return;
        }
        candidate->m_99 = 0;
        candidate->SendMsg(candidate, 126, 0, 0, 0);
    };
    restartLevel99Aura(actor);
    for (const auto& entry : mode.m_runtimeActors) {
        if (entry.second != actor) {
            restartLevel99Aura(entry.second);
        }
    }
}

void FinishMapLoading(CGameMode& mode)
{
    SyncBootstrapRuntimeActorWorldPos(mode);
    LaunchMapWeatherEffect(mode);
    ReplayPersistedStatusVisuals(mode);
    g_windowMgr.HideLoadingScreen();
    g_windowMgr.EnsureChatWindowVisible();
    g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_SHORTCUTWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_ROMAPWND);
    g_windowMgr.SetInstanceWindowVisible(g_session.m_instanceStatus.showWindow);
    DbgLog("[GameMode] map loading finished world='%s' actors=%zu runtime=%zu\n",
        mode.m_rswName,
        mode.m_actorPosList.size(),
        mode.m_runtimeActors.size());
    mode.m_mapLoadingStage = CGameMode::MapLoading_None;
}

void AdvanceMapLoading(CGameMode& mode, u32 now)
{
    switch (mode.m_mapLoadingStage) {
    case CGameMode::MapLoading_PresentScreen:
        UpdateMapLoadingUi(mode, "Preparing loading screen...", 0.08f);
        mode.m_mapLoadingStage = CGameMode::MapLoading_BootstrapAssets;
        return;

    case CGameMode::MapLoading_BootstrapAssets:
    {
        EnsureBootstrapWorldAssets(mode);
        SyncBootstrapSelfActorWorldPos(mode);
        BootstrapWorldCache& cache = GetBootstrapWorldCache();
        const float bootstrapProgress = GetBootstrapWorldLoadProgress(cache);
        if (!mode.m_world || !mode.m_world->m_ground || !IsBootstrapWorldReady(cache)) {
            UpdateMapLoadingUi(mode, "Loading ground and map objects...", 0.12f + bootstrapProgress * 0.58f);
            return;
        }
        UpdateMapLoadingUi(mode, "Loading ground and map objects...", 0.62f);
        mode.m_mapLoadingStage = CGameMode::MapLoading_CreateView;
        return;
    }

    case CGameMode::MapLoading_CreateView:
        EnsureRealView(mode);
        if (!mode.m_view) {
            UpdateMapLoadingUi(mode, "Preparing camera and scene...", 0.7f);
            return;
        }
        UpdateMapLoadingUi(mode, "Preparing camera and scene...", 0.78f);
        mode.m_mapLoadingStage = CGameMode::MapLoading_SendLoadAck;
        return;

    case CGameMode::MapLoading_SendLoadAck:
        if (!mode.m_sentLoadEndAck) {
            mode.m_sentLoadEndAck = SendLoadEndAckPacket() ? 1 : 0;
        }
        mode.m_mapLoadingAckTick = now;
        mode.m_lastActorBootstrapPacketTick = now;
        UpdateMapLoadingUi(mode, "Waiting for map actors...", 0.86f);
        mode.m_mapLoadingStage = CGameMode::MapLoading_AwaitActors;
        return;

    case CGameMode::MapLoading_AwaitActors: {
        const u32 visibleFor = now - mode.m_mapLoadingStartTick;
        const u32 sinceAck = now - mode.m_mapLoadingAckTick;
        const u32 sinceActor = now - mode.m_lastActorBootstrapPacketTick;

        const bool needsRetry = !mode.m_sentLoadEndAck
            || (mode.m_sentLoadEndAck == 1 && mode.m_receiveSyneRequestTime == 0);
        if (needsRetry && sinceAck >= kLoadingAckRetryDelayMs) {
            const bool resent = SendLoadEndAckPacket();
            DbgLog("[GameMode] retry load end ack sent=%d runtime=%zu pos=%zu waitingForSync=%d\n",
                resent ? 1 : 0,
                mode.m_runtimeActors.size(),
                mode.m_actorPosList.size(),
                mode.m_receiveSyneRequestTime == 0 ? 1 : 0);
            if (resent) {
                mode.m_sentLoadEndAck = 2;
                mode.m_mapLoadingAckTick = now;
                mode.m_lastActorBootstrapPacketTick = now;
                return;
            }
            if (!mode.m_sentLoadEndAck) {
                ReturnToLoginModeWithStatus("Login: could not send the map load acknowledgment. Sign in again to retry.");
                return;
            }
            // The initial acknowledgment was queued. This optional resend is one-shot.
            mode.m_sentLoadEndAck = 2;
        }
        if (!mode.m_sentLoadEndAck) {
            UpdateMapLoadingUi(mode, "Sending map load acknowledgment...", 0.86f);
            return;
        }

        const float settleProgress = (std::min)(1.0f, static_cast<float>(sinceAck) / static_cast<float>(kLoadingPostAckWaitMs));
        UpdateMapLoadingUi(mode, "Waiting for map actors...", 0.86f + settleProgress * 0.14f);
        const bool quietWindowSatisfied = visibleFor >= kLoadingScreenMinShowMs
            && sinceAck >= kLoadingPostAckWaitMs
            && sinceActor >= kLoadingActorQuietMs;
        const bool fallbackWaitSatisfied = sinceAck >= kLoadingActorFallbackMs;
        if (quietWindowSatisfied || fallbackWaitSatisfied) {
            if (!quietWindowSatisfied && fallbackWaitSatisfied) {
                DbgLog("[GameMode] finishing map loading after actor wait timeout world='%s' sinceAck=%u sinceActor=%u runtime=%zu pos=%zu\n",
                    mode.m_rswName,
                    static_cast<unsigned int>(sinceAck),
                    static_cast<unsigned int>(sinceActor),
                    mode.m_runtimeActors.size(),
                    mode.m_actorPosList.size());
            }
            FinishMapLoading(mode);
        }
        return;
    }

    default:
        return;
    }
}

void LogRuntimeActorSummary(const CGameMode& mode)
{
    static size_t s_lastActorPosCount = static_cast<size_t>(-1);
    static size_t s_lastRuntimeActorCount = static_cast<size_t>(-1);
    static size_t s_lastPcCount = static_cast<size_t>(-1);

    size_t pcCount = 0;
    for (const auto& entry : mode.m_runtimeActors) {
        const CGameActor* actor = entry.second;
        if (actor && actor->m_isPc) {
            ++pcCount;
        }
    }

    if (s_lastActorPosCount == mode.m_actorPosList.size()
        && s_lastRuntimeActorCount == mode.m_runtimeActors.size()
        && s_lastPcCount == pcCount) {
        return;
    }

    s_lastActorPosCount = mode.m_actorPosList.size();
    s_lastRuntimeActorCount = mode.m_runtimeActors.size();
    s_lastPcCount = pcCount;

    DbgLog("[GameMode] actor summary pos=%zu runtime=%zu pc=%zu\n",
        mode.m_actorPosList.size(),
        mode.m_runtimeActors.size(),
        pcCount);
}

void LogFirstSeenUnhandledGamePacket(u16 packetId, int packetLength)
{
    static std::map<u16, bool> loggedPacketIds;
    if (loggedPacketIds.insert(std::make_pair(packetId, true)).second) {
        DbgLog("[GameMode] unhandled packet id=0x%04X len=%d\n", packetId, packetLength);
    }
}

constexpr u32 kHeldMoveRequestIntervalMs = 75;
constexpr int kHeldMoveRetargetThresholdCells = 2;
constexpr int kHeldMoveDirectionalExtensionCells = 4;
constexpr u32 kHeldMoveDirectionalRetargetNearEndMs = 80;
constexpr u32 kMoveAckTimeoutMs = 1000;
constexpr u32 kAttackChaseRequestIntervalMs = 1200;
constexpr u32 kAttackRetryIntervalMs = 1200;
constexpr u32 kTimeSyncIntervalMs = 12000;
constexpr u32 kTimeSyncReplyRetryMs = 3000;

bool SendLoadEndAckPacket()
{
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    u8 packet[2]{};
    packet[0] = static_cast<u8>(profile.notifyActorInit & 0xFF);
    packet[1] = static_cast<u8>((profile.notifyActorInit >> 8) & 0xFF);

    const bool sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet));
    DbgLog("[GameMode] load end ack opcode=0x%04X sent=%d\n",
        profile.notifyActorInit,
        sent ? 1 : 0);
    return sent;
}

bool SendTimeSyncRequest(CGameMode& mode, bool syncNow)
{
    // Before load-end the selected map server can silently drop time requests.
    if (mode.m_mapLoadingStage != CGameMode::MapLoading_None && !mode.m_sentLoadEndAck) return false;
    const u32 now = timeGetTime();
    const bool waitingForReply = mode.m_syncRequestTime != 0 && mode.m_receiveSyneRequestTime == 0;
    const u32 intervalMs = waitingForReply ? kTimeSyncReplyRetryMs : kTimeSyncIntervalMs;
    const bool needsCompositionSync = mode.m_pendingItemComposition.cardIndex
        && !mode.m_pendingItemComposition.completionSync;
    if (!syncNow && !needsCompositionSync && mode.m_syncRequestTime != 0 && now <= mode.m_syncRequestTime + intervalMs) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    bool sent = false;
    if (ro::net::IsLegacyMapGameplaySendProfile()) {
        PACKET_CZ_TICKSEND_LEGACY packet{};
        packet.PacketType = profile.tickSend;
        packet.ClientTick = now;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
    } else {
        PACKET_CZ_TICKSEND2 packet{};
        packet.PacketType = profile.tickSend;
        FillPacketPadding(&packet.padding, 2);
        packet.ClientTick = now;
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
    }

    if (sent) {
        mode.m_receiveSyneRequestTime = 0;
        mode.m_syncRequestTime = now;
        const auto request = CRagConnection::instance()->RecordTimeSyncRequest();
        if (needsCompositionSync) mode.m_pendingItemComposition.completionSync = request;
    }
    DbgLog("[GameMode] time sync request opcode=0x%04X tick=%u sent=%d\n",
        profile.tickSend,
        now,
        sent ? 1 : 0);
    return sent;
}

bool EncodeMoveDestination(int dstX, int dstY, u8 outDest[3])
{
    if (!outDest || dstX < 0 || dstX > 1023 || dstY < 0 || dstY > 1023) {
        return false;
    }

    outDest[0] = static_cast<u8>((dstX >> 2) & 0xFF);
    outDest[1] = static_cast<u8>(((dstX & 0x03) << 6) | ((dstY >> 4) & 0x3F));
    outDest[2] = static_cast<u8>((dstY & 0x0F) << 4);
    return true;
}

int ResolveDirFromCellDelta(int dx, int dy)
{
    if (dx == 0 && dy < 0) {
        return 4;
    }
    if (dx > 0 && dy < 0) {
        return 5;
    }
    if (dx > 0 && dy == 0) {
        return 6;
    }
    if (dx > 0 && dy > 0) {
        return 7;
    }
    if (dx == 0 && dy > 0) {
        return 0;
    }
    if (dx < 0 && dy > 0) {
        return 1;
    }
    if (dx < 0 && dy == 0) {
        return 2;
    }
    if (dx < 0 && dy < 0) {
        return 3;
    }
    return -1;
}

int NormalizePacketDir(int dir)
{
    int normalized = dir % 8;
    if (normalized < 0) {
        normalized += 8;
    }
    return normalized;
}

u8 NormalizeHeadDir(int headDir);

int ResolveCurrentPacketBodyDir(const CGameMode& mode)
{
    int bodyDir = g_session.m_playerDir & 7;
    if (mode.m_world && mode.m_world->m_player) {
        const float rotationDegrees = mode.m_world->m_player->m_roty;
        if (std::isfinite(rotationDegrees)) {
            const int renderDir = static_cast<int>((std::floor((rotationDegrees + 22.5f) / 45.0f))) & 7;
            bodyDir = (renderDir + 4) & 7;
        }
    }
    return bodyDir;
}

bool ResolveRefTurnOnlyDirectionRequest(const CGameMode& mode, int targetDir, u8* outHeadDir, u8* outBodyDir)
{
    if (!outHeadDir || !outBodyDir || !mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    const int bodyDir = NormalizePacketDir(ResolveCurrentPacketBodyDir(mode));
    int headDir = 0;
    if (const CPc* playerPc = dynamic_cast<const CPc*>(mode.m_world->m_player)) {
        headDir = NormalizeHeadDir(playerPc->m_headDir);
    }

    const int normalizedTargetDir = NormalizePacketDir(targetDir);
    // Ref computes the octant delta from current body rotation minus target
    // rotation. In packet-space that maps to currentDir - targetDir.
    const int delta = NormalizePacketDir(bodyDir - normalizedTargetDir);

    int resolvedBodyDir = bodyDir;
    int resolvedHeadDir = 0;

    switch (delta) {
    case 0:
    case 4:
        resolvedBodyDir = normalizedTargetDir;
        resolvedHeadDir = 0;
        break;
    case 1:
        if (headDir != 1) {
            resolvedBodyDir = bodyDir;
            resolvedHeadDir = 1;
        } else {
            resolvedBodyDir = normalizedTargetDir;
            resolvedHeadDir = 0;
        }
        break;
    case 2:
    case 3:
        resolvedBodyDir = NormalizePacketDir(normalizedTargetDir + 1);
        resolvedHeadDir = 1;
        break;
    case 7:
        if (headDir != 2) {
            resolvedBodyDir = bodyDir;
            resolvedHeadDir = 2;
        } else {
            resolvedBodyDir = normalizedTargetDir;
            resolvedHeadDir = 0;
        }
        break;
    case 5:
    case 6:
        resolvedBodyDir = NormalizePacketDir(normalizedTargetDir - 1);
        resolvedHeadDir = 2;
        break;
    default:
        return false;
    }

    *outHeadDir = NormalizeHeadDir(resolvedHeadDir);
    *outBodyDir = static_cast<u8>(NormalizePacketDir(resolvedBodyDir));
    return true;
}

bool ShouldUseTurnOnlyGroundClick(const CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    const CGameActor* const player = mode.m_world->m_player;
    const bool isShiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool looksSeated = (player->m_isSitting != 0)
        || (player->m_baseAction == 16)
        || (player->m_curAction >= 16 && player->m_curAction < 24);
    const bool hasChatRoomContext = g_windowMgr.m_chatRoomWnd != nullptr;
    return looksSeated || isShiftHeld || hasChatRoomContext;
}

bool TryHandleCompanionGroundClick(CGameMode& mode, companion_ai::Kind kind, int screenX, int screenY)
{
    if ((!g_session.m_homunStatus.alive && !g_session.m_merStatus.alive)
        || !mode.m_world || !mode.m_view || mode.m_canRotateView || mode.m_isPlayerDead) return false;

    CGameActor* actor = nullptr;
    CItem* item = nullptr;
    const auto& viewMatrix = mode.m_view->GetViewMatrix();
    if ((mode.m_world->FindHoveredActorScreen(viewMatrix, mode.m_view->GetCameraLongitude(),
             screenX, screenY, &actor, nullptr, nullptr) && actor)
        || (mode.m_world->FindHoveredGroundItemScreen(viewMatrix,
             screenX, screenY, &item, nullptr, nullptr) && item)) return false;

    int x = 0, y = 0;
    if (!mode.m_view->ScreenToHoveredAttrCell(screenX, screenY, &x, &y)) return false;
    const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    return g_session.ApplyCompanionGroundClick(kind, x, y, alt, shift);
}

bool IsPcAttackEligibleForMap(const CGameMode& mode, const CGameActor& target, bool baseEligible)
{
    if (!mode.m_world) return false;
    const auto* player = mode.m_world->m_player;
    const bool sameGuild = player && target.m_gdid && target.m_gdid == player->m_gdid;
    const bool pvpMap = mode.m_world->m_isPKZone || mode.m_world->m_isEventPVPMode;
    const auto& party = g_session.GetPartyList();
    const bool sameParty = pvpMap && std::any_of(party.begin(), party.end(),
        [&](const FRIEND_INFO& member) { return member.AID == target.m_gid; });
    bool eligible = baseEligible;
    const int type = g_session.m_curMapType;
    if (g_session.m_mapTypeInfo.Enabled(type, ro::SourceMapTypeTable::AllyParty)) {
        if (!sameParty) eligible = true;
    } else if (g_session.m_mapTypeInfo.Enabled(type, ro::SourceMapTypeTable::AllyGuild) && !sameGuild) {
        eligible = true;
    }
    if (target.m_pkState || pvpMap) {
        if (!sameParty) eligible = true;
    } else if (mode.m_world->m_isSiegeMode && !sameGuild) {
        eligible = true;
    }
    return eligible;
}

bool IsMonsterAttackEligibleForMap(const CGameMode& mode, const CGameActor& target, bool baseEligible)
{
    const auto* player = mode.m_world ? mode.m_world->m_player : nullptr;
    const bool sameGuild = player && target.m_gdid && target.m_gdid == player->m_gdid;
    const bool guildAllies = g_session.m_mapTypeInfo.Enabled(g_session.m_curMapType,
        ro::SourceMapTypeTable::AllyGuild);
    const bool siege = mode.m_world && mode.m_world->m_isSiegeMode;
    return (baseEligible && !(sameGuild && (guildAllies || siege))) || g_session.m_isNoShift == 1;
}

bool TryHandleCompanionActorClick(CGameMode& mode, companion_ai::Kind kind, int screenX, int screenY)
{
    if (!(GetKeyState(VK_MENU) & 0x8000)
        || (!g_session.m_homunStatus.alive && !g_session.m_merStatus.alive)
        || !mode.m_world || !mode.m_view || mode.m_canRotateView || mode.m_isPlayerDead) return false;

    CGameActor* actor = nullptr;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
            mode.m_view->GetCameraLongitude(), screenX, screenY, &actor, nullptr, nullptr)
        || !actor || !actor->m_gid || actor == mode.m_world->m_player || actor->m_stateId == 3
        || actor->m_gid == g_session.m_homunGID || actor->m_gid == g_session.m_merGID) return false;

    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (IsPcLikeHoverActor(actor)) {
        if (!IsPcAttackEligibleForMap(mode, *actor, shift)
            && g_session.m_isNoShift != 1 && !mode.m_world->m_isSiegeMode) return false;
    } else if (!IsMonsterLikeHoverActor(actor) || IsNpcLikeHoverActor(actor)) {
        return false;
    }

    return g_session.ApplyCompanionActorClick(kind, actor->m_gid, shift);
}

bool WorldToAttrCell(const CWorld* world, float worldX, float worldZ, int* outTileX, int* outTileY);
bool ResolveMoveSourceTile(const CGameMode& mode, int* outTileX, int* outTileY);

void ClearAttackChaseHint(CGameMode& mode)
{
    mode.m_lastAttackChaseHintTick = 0;
    mode.m_attackChaseTargetGid = 0;
    mode.m_attackChaseTargetCellX = -1;
    mode.m_attackChaseTargetCellY = -1;
    mode.m_attackChaseSourceCellX = -1;
    mode.m_attackChaseSourceCellY = -1;
    mode.m_attackChaseRange = 1;
    mode.m_hasAttackChaseHint = 0;
}

void ClearSkillChase(CGameMode& mode)
{
    if(mode.m_world && mode.m_world->m_player){
        auto* player=mode.m_world->m_player;
        if(player->m_proceedType.type==3 || player->m_proceedType.type==4 || player->m_proceedType.type==6){
            player->m_proceedType.type=0;player->m_isAttackRequest=0;
        }
    }
    mode.m_skillChaseTargetGid = 0;
    mode.m_lastSkillChaseRequestTick = 0;
    mode.m_skillChaseSkillId = 0;
    mode.m_skillChaseSkillLevel = 0;
    mode.m_skillChaseRange = 1;
    mode.m_hasSkillChase = 0;
    mode.m_groundSkillCellX = 0;
    mode.m_groundSkillCellY = 0;
    mode.m_groundSkillId = 0;
    mode.m_groundSkillLevel = 0;
    mode.m_groundSkillRange = 0;
    mode.m_hasGroundSkillChase = 0;
    mode.m_touchSkillTargetAid = 0;
    mode.m_touchSkillId = 0;
    mode.m_touchSkillLevel = 0;
    mode.m_touchSkillRange = 0;
    mode.m_hasTouchSkillChase = 0;
}

void ClearPickupIntent(CGameMode& mode)
{
    mode.m_pickupReqItemNaidList.clear();
    mode.m_lastPickupRequestTick = 0;
}

bool FindActivePathSegmentForChase(const CPathInfo& path, u32 now, size_t* outStartIndex)
{
    if (!outStartIndex || path.m_cells.size() < 2) {
        return false;
    }

    for (size_t index = 0; index + 1 < path.m_cells.size(); ++index) {
        if (now < path.m_cells[index + 1].arrivalTime) {
            *outStartIndex = index;
            return true;
        }
    }

    return false;
}

bool ResolveAttackChaseSourceTile(const CGameMode& mode, int* outTileX, int* outTileY)
{
    if (!outTileX || !outTileY) {
        return false;
    }

    if (mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_isMoving
        && (g_session.m_aid != 0 || g_session.m_gid != 0)) {
        const u32 selfActorId = g_session.m_aid != 0 ? g_session.m_aid : g_session.m_gid;
        auto runtimeIt = mode.m_runtimeActors.find(selfActorId);
        if (runtimeIt == mode.m_runtimeActors.end() && g_session.m_gid != 0 && g_session.m_gid != selfActorId) {
            runtimeIt = mode.m_runtimeActors.find(g_session.m_gid);
        }
        if (runtimeIt != mode.m_runtimeActors.end() && runtimeIt->second) {
            const CGameActor* actor = runtimeIt->second;
            size_t activeIndex = 0;
            if (FindActivePathSegmentForChase(actor->m_path, g_session.GetServerTime(), &activeIndex)) {
                const PathCell& activeCell = actor->m_path.m_cells[activeIndex];
                *outTileX = activeCell.x;
                *outTileY = activeCell.y;
                return true;
            }
        }
    }

    if (ResolveMoveSourceTile(mode, outTileX, outTileY)) {
        return true;
    }

    if (mode.m_attackChaseSourceCellX >= 0 && mode.m_attackChaseSourceCellY >= 0) {
        *outTileX = mode.m_attackChaseSourceCellX;
        *outTileY = mode.m_attackChaseSourceCellY;
        return true;
    }

    return false;
}

bool CanFindMovePath(const CGameMode& mode, int sx, int sy, int cellX, int cellY, int dx, int dy)
{
    if (!mode.m_world || !mode.m_world->m_attr) {
        return false;
    }

    g_pathFinder.SetMap(mode.m_world);
    CPathInfo pathInfo;
    return g_pathFinder.FindPath(timeGetTime(), sx, sy, cellX, cellY, dx, dy, 150, &pathInfo);
}

bool CanFindMovePath(const CGameMode& mode, int sx, int sy, int dx, int dy)
{
    return CanFindMovePath(mode, sx, sy, dx, dy, dx, dy);
}

bool IsWalkableAttrCell(const CGameMode& mode, int tileX, int tileY)
{
    return mode.m_world && mode.m_world->IsMapCellWalkable(tileX, tileY);
}

bool IsOccupiedActorCell(const CGameMode& mode, int tileX, int tileY, u32 ignoredActorA = 0, u32 ignoredActorB = 0)
{
    for (const auto& entry : mode.m_actorPosList) {
        if (entry.first == ignoredActorA || entry.first == ignoredActorB) {
            continue;
        }

        if (entry.second.x == tileX && entry.second.y == tileY) {
            return true;
        }
    }

    return false;
}

bool IsValidMoveCell(const CGameMode& mode,
    int sourceTileX,
    int sourceTileY,
    int requestedTileX,
    int requestedTileY,
    int candidateTileX,
    int candidateTileY,
    u32 ignoredActorA = 0,
    u32 ignoredActorB = 0)
{
    if (!IsWalkableAttrCell(mode, candidateTileX, candidateTileY)) {
        return false;
    }

    if (IsOccupiedActorCell(mode, candidateTileX, candidateTileY, ignoredActorA, ignoredActorB)) {
        return false;
    }

    return CanFindMovePath(mode, sourceTileX, sourceTileY, requestedTileX, requestedTileY, candidateTileX, candidateTileY);
}

bool SendMoveRequestPacket(int dstX, int dstY)
{
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    bool sent = false;
    if (ro::net::IsLegacyMapGameplaySendProfile()) {
        PACKET_CZ_REQUEST_MOVE_LEGACY packet{};
        packet.PacketType = profile.walkToXY;
        if (!EncodeMoveDestination(dstX, dstY, packet.Dest)) {
            DbgLog("[GameMode] move request encode failed dst=%d,%d\n", dstX, dstY);
            return false;
        }
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
    } else {
        PACKET_CZ_REQUEST_MOVE2 packet{};
        packet.PacketType = profile.walkToXY;
        FillPacketPadding(packet.padding, static_cast<int>(sizeof(packet.padding)));
        if (!EncodeMoveDestination(dstX, dstY, packet.Dest)) {
            DbgLog("[GameMode] move request encode failed dst=%d,%d\n", dstX, dstY);
            return false;
        }
        sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
    }
    DbgLog("[GameMode] move request opcode=0x%04X dst=%d,%d sent=%d\n",
        profile.walkToXY,
        dstX,
        dstY,
        sent ? 1 : 0);
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            if (gameMode->m_world && gameMode->m_world->m_player) {
                gameMode->m_world->m_player->m_isWaitingMoveAck = 1;
                gameMode->m_world->m_player->m_moveReqTick = GetTickCount();
            }
        }
    }
    return sent;
}

bool SendChangeDirRequestPacket(u16 headDir, u8 dir,bool normalizeDir=true)
{
    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    bool sent = false;
    if (ro::net::IsLegacyMapGameplaySendProfile()) {
        PACKET_CZ_CHANGE_DIRECTION_LEGACY packet{};
        packet.PacketType = profile.changeDir;
        packet.HeadDir = static_cast<u16>(headDir);
        packet.Dir = normalizeDir?static_cast<u8>(dir & 7):dir;
        sent = CRagConnection::instance()->SendPacket(
            reinterpret_cast<const char*>(&packet),
            static_cast<int>(sizeof(packet)));
    } else {
        PACKET_CZ_CHANGE_DIRECTION2 packet{};
        packet.PacketType = profile.changeDir;
        FillPacketPadding(packet.padding0, static_cast<int>(sizeof(packet.padding0)));
        packet.HeadDir = static_cast<u16>(headDir);
        FillPacketPadding(&packet.padding1, 1);
        packet.Dir = normalizeDir?static_cast<u8>(dir & 7):dir;
        sent = CRagConnection::instance()->SendPacket(
            reinterpret_cast<const char*>(&packet),
            static_cast<int>(sizeof(packet)));
    }
    DbgLog("[GameMode] change dir request opcode=0x%04X headdir=%u dir=%u sent=%d\n",
        profile.changeDir,
        static_cast<unsigned int>(headDir),
        static_cast<unsigned int>(dir & 7),
        sent ? 1 : 0);
    return sent;
}

u8 NormalizeHeadDir(int headDir)
{
    return static_cast<u8>((std::max)(0, (std::min)(headDir, 2)));
}

void ApplyLocalPlayerDirection(CGameMode& mode, u8 headDir, u8 dir)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    CGameActor* const player = mode.m_world->m_player;
    player->m_roty = static_cast<float>(45 * (((dir & 7) + 4)));

    if (player->m_isSitting != 0) {
        const int baseAction = (player->m_isPc != 0) ? 16 : 0;
        const int resolvedAction = baseAction + ((static_cast<int>(dir) + 4) & 7);
        player->m_baseAction = baseAction;
        player->m_curAction = resolvedAction;
        player->m_oldBaseAction = baseAction;
        player->m_oldMotion = player->m_curMotion;
    }

    if (CPc* const playerPc = dynamic_cast<CPc*>(player)) {
        playerPc->m_headDir = NormalizeHeadDir(headDir);
        if (!playerPc->m_isMoving && (playerPc->m_isSitting != 0 || playerPc->m_baseAction == 0 || playerPc->m_baseAction == 16)) {
            playerPc->m_curMotion = playerPc->m_headDir;
            playerPc->m_oldMotion = playerPc->m_headDir;
        }
        playerPc->InvalidateBillboard();
    }

    g_session.SetPlayerPosDir(g_session.m_playerPosX, g_session.m_playerPosY, dir & 7);
}

bool LocalPlayerHasPendingMoveAck(const CGameMode& mode, u32 now)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    const CPlayer* player = mode.m_world->m_player;
    if (!player->m_isWaitingMoveAck) {
        return false;
    }

    return now - player->m_moveReqTick <= kMoveAckTimeoutMs;
}

bool SendGlobalChatMessage(const char* playerName, const std::string& message)
{
    if (!playerName || *playerName == '\0' || message.empty()) {
        return false;
    }

    const std::string payload = std::string(playerName) + " : " + message;
    const size_t packetSize = 4 + payload.size() + 1;
    if (packetSize > 0xFFFFu) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    std::vector<u8> packet(packetSize, 0);
    packet[0] = static_cast<u8>(profile.globalMessage & 0xFFu);
    packet[1] = static_cast<u8>((profile.globalMessage >> 8) & 0xFFu);
    packet[2] = static_cast<u8>(packetSize & 0xFFu);
    packet[3] = static_cast<u8>((packetSize >> 8) & 0xFFu);
    std::memcpy(packet.data() + 4, payload.c_str(), payload.size());

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()));
    DbgLog("[GameMode] chat send opcode=0x%04X name='%s' text='%s' sent=%d\n",
        profile.globalMessage,
        playerName,
        message.c_str(),
        sent ? 1 : 0);
    return sent;
}

bool SendWhisperChatMessage(const std::string& targetName, const std::string& message)
{
    if (targetName.empty() || targetName.size() > 24 || message.empty()) {
        return false;
    }

    const size_t packetSize = 28 + message.size() + 1;
    if (packetSize > 0xFFFFu) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    std::vector<u8> packet(packetSize, 0);
    packet[0] = static_cast<u8>(profile.whisper & 0xFFu);
    packet[1] = static_cast<u8>((profile.whisper >> 8) & 0xFFu);
    packet[2] = static_cast<u8>(packetSize & 0xFFu);
    packet[3] = static_cast<u8>((packetSize >> 8) & 0xFFu);
    std::memcpy(packet.data() + 4, targetName.data(), targetName.size());
    std::memcpy(packet.data() + 28, message.c_str(), message.size());

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()));
    DbgLog("[GameMode] whisper send opcode=0x%04X target='%s' text='%s' sent=%d\n",
        profile.whisper,
        targetName.c_str(),
        message.c_str(),
        sent ? 1 : 0);
    return sent;
}

bool ResolveMoveSourceTile(const CGameMode& mode, int* outTileX, int* outTileY)
{
    if (!outTileX || !outTileY || !mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    int tileX = g_session.m_playerPosX;
    int tileY = g_session.m_playerPosY;
    if (tileX < 0 || tileY < 0) {
        if (!WorldToAttrCell(mode.m_world, mode.m_world->m_player->m_pos.x, mode.m_world->m_player->m_pos.z, &tileX, &tileY)) {
            return false;
        }
    }

    *outTileX = tileX;
    *outTileY = tileY;
    return true;
}

bool TryRequestMoveToCell(CGameMode& mode, int sourceTileX, int sourceTileY, int attrX, int attrY, bool allowRepeatDestination = false)
{
    if (!mode.m_view || !mode.m_world || !mode.m_world->m_player || mode.m_canRotateView) {
        return false;
    }

    if (!allowRepeatDestination
        && attrX == mode.m_lastMoveRequestCellX
        && attrY == mode.m_lastMoveRequestCellY) {
        return false;
    }

    if (!CanFindMovePath(mode, sourceTileX, sourceTileY, attrX, attrY)) {
        return false;
    }

    if (!SendMoveRequestPacket(attrX, attrY)) {
        return false;
    }

    mode.m_lastMoveRequestCellX = attrX;
    mode.m_lastMoveRequestCellY = attrY;
    return true;
}

bool TryRequestMoveToCell(CGameMode& mode, int attrX, int attrY)
{
    int playerTileX = -1;
    int playerTileY = -1;
    if (!ResolveMoveSourceTile(mode, &playerTileX, &playerTileY)) {
        return false;
    }

    return TryRequestMoveToCell(mode, playerTileX, playerTileY, attrX, attrY);
}

bool IsTargetWithinAttackRange(int sourceTileX, int sourceTileY, int targetTileX, int targetTileY, int attackRange)
{
    const int clampedRange = (std::max)(1, attackRange);
    return (std::max)(std::abs(sourceTileX - targetTileX), std::abs(sourceTileY - targetTileY)) <= clampedRange;
}

// The visible body can be ahead of the cell the server last confirmed.
// Sending a skill from that predicted cell is what produces a swing with no
// hit and no SP cost: the server drops the request and never echoes it.
bool IsServerCellWithinSkillRange(int targetTileX, int targetTileY, int skillRange)
{
    if (g_session.m_playerPosX < 0 || g_session.m_playerPosY < 0) {
        return false;
    }
    return IsTargetWithinAttackRange(g_session.m_playerPosX, g_session.m_playerPosY, targetTileX, targetTileY, skillRange);
}

bool IsServerCellWithinSkillRangeOfActor(const CGameMode& mode, const CGameActor& target, int skillRange)
{
    int targetTileX = -1;
    int targetTileY = -1;
    if (!WorldToAttrCell(mode.m_world, target.m_pos.x, target.m_pos.z, &targetTileX, &targetTileY)) {
        return true;
    }
    return IsServerCellWithinSkillRange(targetTileX, targetTileY, skillRange);
}

int ApproachSign(int delta)
{
    if (delta > 0) {
        return 1;
    }
    if (delta < 0) {
        return -1;
    }
    return 0;
}

int GetHeldMoveDirectionalExtensionCells(const CPlayer& player)
{
    if (player.m_speed <= 60) {
        return kHeldMoveDirectionalExtensionCells + 2;
    }
    if (player.m_speed <= 90) {
        return kHeldMoveDirectionalExtensionCells + 1;
    }
    return kHeldMoveDirectionalExtensionCells;
}

u32 GetHeldMoveDirectionalRetargetNearEndMs(const CPlayer& player)
{
    if (player.m_speed <= 60) {
        return 150;
    }
    if (player.m_speed <= 90) {
        return 120;
    }
    return kHeldMoveDirectionalRetargetNearEndMs;
}

bool ResolveEathenaChaseCell(CGameMode& mode,
    int sourceTileX,
    int sourceTileY,
    int targetTileX,
    int targetTileY,
    u32 targetGid,
    int* outTileX,
    int* outTileY)
{
    if (!outTileX || !outTileY) {
        return false;
    }

    const int stepX = ApproachSign(targetTileX - sourceTileX);
    const int stepY = ApproachSign(targetTileY - sourceTileY);

    const int preferredX = targetTileX - stepX;
    const int preferredY = targetTileY - stepY;

    std::array<std::pair<int, int>, 9> candidates = {
        std::make_pair(preferredX, preferredY),
        std::make_pair(targetTileX - 1, targetTileY),
        std::make_pair(targetTileX - 1, targetTileY - 1),
        std::make_pair(targetTileX, targetTileY - 1),
        std::make_pair(targetTileX + 1, targetTileY - 1),
        std::make_pair(targetTileX + 1, targetTileY),
        std::make_pair(targetTileX + 1, targetTileY + 1),
        std::make_pair(targetTileX, targetTileY + 1),
        std::make_pair(targetTileX - 1, targetTileY + 1),
    };

    for (const auto& candidate : candidates) {
        if (candidate.first == targetTileX && candidate.second == targetTileY) {
            continue;
        }

        if (!IsValidMoveCell(mode,
            sourceTileX,
            sourceTileY,
            candidate.first,
            candidate.second,
            candidate.first,
            candidate.second,
            mode.m_world->m_player->m_gid,
            targetGid)) {
            continue;
        }

        *outTileX = candidate.first;
        *outTileY = candidate.second;
        return true;
    }

    return false;
}

bool ResolveAttackChaseCell(CGameMode& mode,
    int sourceTileX,
    int sourceTileY,
    int targetTileX,
    int targetTileY,
    int attackRange,
    u32 targetGid,
    int* outTileX,
    int* outTileY)
{
    if (!outTileX || !outTileY || !mode.m_world || !mode.m_world->m_attr) {
        return false;
    }

    const int clampedRange = (std::max)(1, attackRange);
    if (IsTargetWithinAttackRange(sourceTileX, sourceTileY, targetTileX, targetTileY, clampedRange)) {
        return false;
    }

    if (clampedRange == 1) {
        return ResolveEathenaChaseCell(mode,
            sourceTileX,
            sourceTileY,
            targetTileX,
            targetTileY,
            targetGid,
            outTileX,
            outTileY);
    }

    const int stepX = ApproachSign(targetTileX - sourceTileX);
    const int stepY = ApproachSign(targetTileY - sourceTileY);
    const int preferredX = targetTileX - stepX * clampedRange;
    const int preferredY = targetTileY - stepY * clampedRange;

    struct ChaseCandidate {
        int x;
        int y;
        int sourceDistance;
        int preferredDistance;
    };

    std::vector<ChaseCandidate> candidates;
    candidates.reserve(static_cast<size_t>((clampedRange * 2 + 1) * (clampedRange * 2 + 1)));
    for (int dy = -clampedRange; dy <= clampedRange; ++dy) {
        for (int dx = -clampedRange; dx <= clampedRange; ++dx) {
            const int ringDistance = (std::max)(std::abs(dx), std::abs(dy));
            if (ringDistance == 0 || ringDistance > clampedRange) {
                continue;
            }

            const int candidateX = targetTileX + dx;
            const int candidateY = targetTileY + dy;
            candidates.push_back({
                candidateX,
                candidateY,
                (std::max)(std::abs(candidateX - sourceTileX), std::abs(candidateY - sourceTileY)),
                (std::max)(std::abs(candidateX - preferredX), std::abs(candidateY - preferredY)),
            });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ChaseCandidate& lhs, const ChaseCandidate& rhs) {
        if (lhs.sourceDistance != rhs.sourceDistance) {
            return lhs.sourceDistance < rhs.sourceDistance;
        }
        if (lhs.preferredDistance != rhs.preferredDistance) {
            return lhs.preferredDistance < rhs.preferredDistance;
        }
        if (lhs.y != rhs.y) {
            return lhs.y < rhs.y;
        }
        return lhs.x < rhs.x;
    });

    for (const ChaseCandidate& candidate : candidates) {
        if (!IsValidMoveCell(mode,
            sourceTileX,
            sourceTileY,
            candidate.x,
            candidate.y,
            candidate.x,
            candidate.y,
            mode.m_world->m_player->m_gid,
            targetGid)) {
            continue;
        }

        *outTileX = candidate.x;
        *outTileY = candidate.y;
        return true;
    }

    return ResolveEathenaChaseCell(mode,
        sourceTileX,
        sourceTileY,
        targetTileX,
        targetTileY,
        targetGid,
        outTileX,
        outTileY);
}

bool ResolveAttackChaseCell(CGameMode& mode, const CGameActor& target, int* outTileX, int* outTileY)
{
    if (!outTileX || !outTileY || !mode.m_world || !mode.m_world->m_attr) {
        return false;
    }

    int playerTileX = -1;
    int playerTileY = -1;
    if (!ResolveMoveSourceTile(mode, &playerTileX, &playerTileY)) {
        return false;
    }

    int targetTileX = -1;
    int targetTileY = -1;
    if (!WorldToAttrCell(mode.m_world, target.m_pos.x, target.m_pos.z, &targetTileX, &targetTileY)) {
        return false;
    }

    return ResolveAttackChaseCell(mode, playerTileX, playerTileY, targetTileX, targetTileY, 1, target.m_gid, outTileX, outTileY);
}

bool ResolveGroundItemPickupCell(CGameMode& mode,
    int sourceTileX,
    int sourceTileY,
    const GroundItemState& item,
    int* outTileX,
    int* outTileY)
{
    if (!outTileX || !outTileY) {
        return false;
    }

    const int targetTileX = static_cast<int>(item.tileX);
    const int targetTileY = static_cast<int>(item.tileY);
    if (IsTargetWithinAttackRange(sourceTileX, sourceTileY, targetTileX, targetTileY, 1)) {
        return false;
    }

    // Match the Ref/eAthena chase behavior we already use for attacks: path to
    // an adjacent reachable cell, not onto the occupied target cell itself.
    return ResolveEathenaChaseCell(mode,
        sourceTileX,
        sourceTileY,
        targetTileX,
        targetTileY,
        0,
        outTileX,
        outTileY);
}

bool IsAttackTargetWithinRange(CGameMode& mode, const CGameActor& target, int attackRange)
{
    int playerTileX = -1;
    int playerTileY = -1;
    if (!ResolveAttackChaseSourceTile(mode, &playerTileX, &playerTileY)) {
        return false;
    }

    int targetTileX = -1;
    int targetTileY = -1;
    if (!WorldToAttrCell(mode.m_world, target.m_pos.x, target.m_pos.z, &targetTileX, &targetTileY)) {
        return false;
    }

    return IsTargetWithinAttackRange(playerTileX, playerTileY, targetTileX, targetTileY, attackRange);
}

bool IsCurrentMoveDestinationWithinRange(CGameMode& mode, const CGameActor& target, int attackRange)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    const CPlayer* player = mode.m_world->m_player;
    if (!player->m_isMoving) {
        return false;
    }

    const int moveDestX = player->m_moveDestX;
    const int moveDestY = player->m_moveDestY;
    if (moveDestX < 0 || moveDestY < 0) {
        return false;
    }

    int targetTileX = -1;
    int targetTileY = -1;
    if (!WorldToAttrCell(mode.m_world, target.m_pos.x, target.m_pos.z, &targetTileX, &targetTileY)) {
        return false;
    }

    if (!IsTargetWithinAttackRange(moveDestX, moveDestY, targetTileX, targetTileY, attackRange)) {
        return false;
    }

    // Only preserve the current move when the latest server chase hint also
    // agrees that the player is effectively already in range. If the server is
    // still reporting an older out-of-range source tile, keep pumping chase
    // movement instead of assuming the queued destination is enough.
    if (mode.m_hasAttackChaseHint
        && mode.m_attackChaseTargetGid == target.m_gid
        && mode.m_attackChaseSourceCellX >= 0
        && mode.m_attackChaseSourceCellY >= 0
        && !IsTargetWithinAttackRange(
            mode.m_attackChaseSourceCellX,
            mode.m_attackChaseSourceCellY,
            targetTileX,
            targetTileY,
            attackRange)) {
        return false;
    }

    return true;
}

bool TryRequestMoveFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_view) {
        return false;
    }

    int attrX = -1;
    int attrY = -1;
    if (!mode.m_view->ScreenToHoveredAttrCell(screenX, screenY, &attrX, &attrY)) {
        return false;
    }

    if (!TryRequestMoveToCell(mode, attrX, attrY)) {
        return false;
    }
    if (mode.m_world) {
        if (CPlayer* const localPlayer = dynamic_cast<CPlayer*>(mode.m_world->m_player)) {
            localPlayer->BeginPreMove(attrX, attrY);
        }
    }
    return true;
}

bool TryRequestChangeDirToCell(CGameMode& mode, int attrX, int attrY)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    int sourceTileX = -1;
    int sourceTileY = -1;
    if (!ResolveMoveSourceTile(mode, &sourceTileX, &sourceTileY)) {
        sourceTileX = g_session.m_playerPosX;
        sourceTileY = g_session.m_playerPosY;
    }

    const int stepX = (attrX > sourceTileX) ? 1 : ((attrX < sourceTileX) ? -1 : 0);
    const int stepY = (attrY > sourceTileY) ? 1 : ((attrY < sourceTileY) ? -1 : 0);
    const int dir = ResolveDirFromCellDelta(stepX, stepY);
    if (dir < 0) {
        return false;
    }

    u8 headDir = 0;
    u8 packetDir = 0;
    if (!ResolveRefTurnOnlyDirectionRequest(mode, dir, &headDir, &packetDir)) {
        return false;
    }

    const bool sent = SendChangeDirRequestPacket(headDir, packetDir);
    if (sent) {
        // eAthena broadcasts changedir without echoing it back to the sender.
        ApplyLocalPlayerDirection(mode, headDir, packetDir);
    }
    return sent;
}

bool TryRequestChangeDirFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_view) {
        return false;
    }

    int attrX = -1;
    int attrY = -1;
    if (!mode.m_view->ScreenToHoveredAttrCell(screenX, screenY, &attrX, &attrY)) {
        return false;
    }

    return TryRequestChangeDirToCell(mode, attrX, attrY);
}

constexpr u8 kActionRequestSitDown = 0x02;
constexpr u8 kActionRequestStandUp = 0x03;
constexpr u8 kActionRequestContinuousAttack = 0x07;

bool SendAttackRequestPacket(u32 targetGid, u8 action)
{
    return SendActionRequestPacket(targetGid, action);
}

bool SendSitStandRequest(CGameMode& mode, bool sitDown)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    CPlayer* player = mode.m_world->m_player;
    player->m_targetGid = 0;
    const u8 action = sitDown ? kActionRequestSitDown : kActionRequestStandUp;
    return SendActionRequestPacket(0, action);
}

bool SendSitStandToggleRequest(CGameMode& mode)
{
    return mode.m_world
        && mode.m_world->m_player
        ? SendSitStandRequest(mode, mode.m_world->m_player->m_isSitting == 0)
        : false;
}

bool SendTakeItemRequestPacket(u32 objectAid)
{
    if (objectAid == 0) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    std::array<u8, sizeof(PACKET_CZ_TAKE_ITEM2)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveTakeItemPacket(
            objectAid,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    DbgLog("[GameMode] take item aid=%u opcode=0x%04X sent=%d\n",
        objectAid,
        profile.takeItem,
        sent ? 1 : 0);
    return sent;
}

bool IsGroundItemWithinPickupRange(CGameMode& mode, const GroundItemState& item)
{
    int playerTileX = -1;
    int playerTileY = -1;
    if (!ResolveAttackChaseSourceTile(mode, &playerTileX, &playerTileY)) {
        return false;
    }

    return IsTargetWithinAttackRange(playerTileX,
        playerTileY,
        static_cast<int>(item.tileX),
        static_cast<int>(item.tileY),
        1);
}

bool IsCurrentMoveDestinationWithinPickupRange(CGameMode& mode, const GroundItemState& item)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    const CPlayer* player = mode.m_world->m_player;
    if (!player->m_isMoving || player->m_moveDestX < 0 || player->m_moveDestY < 0) {
        return false;
    }

    return IsTargetWithinAttackRange(player->m_moveDestX,
        player->m_moveDestY,
        static_cast<int>(item.tileX),
        static_cast<int>(item.tileY),
        1);
}

bool TryBeginGroundItemPickup(CGameMode& mode, u32 objectAid)
{
    const auto groundItemIt = mode.m_groundItemList.find(objectAid);
    if (groundItemIt == mode.m_groundItemList.end()) {
        return false;
    }

    const GroundItemState& groundItem = groundItemIt->second;
    if (!mode.m_pickupReqItemNaidList.empty() && mode.m_pickupReqItemNaidList.front() == objectAid
        && mode.m_lastPickupRequestTick != 0) return true;
    mode.m_pickupReqItemNaidList.clear();
    mode.m_pickupReqItemNaidList.push_back(objectAid);
    mode.m_lastPickupRequestTick = 0;
    mode.m_lastMonGid = 0;
    mode.m_lastLockOnMonGid = 0;
    mode.m_isAutoMoveClickOn = 0;
    mode.m_lastAttackRequestTick = 0;
    ClearAttackChaseHint(mode);

    const u32 now = GetTickCount();
    if (IsGroundItemWithinPickupRange(mode, groundItem)) {
        if (SendTakeItemRequestPacket(objectAid)) {
            mode.m_lastPickupRequestTick = now;
            return true;
        }
        return false;
    }

    if (IsCurrentMoveDestinationWithinPickupRange(mode, groundItem)) {
        return true;
    }

    int sourceTileX = -1;
    int sourceTileY = -1;
    if (!ResolveAttackChaseSourceTile(mode, &sourceTileX, &sourceTileY)) {
        return false;
    }

    int chaseTileX = -1;
    int chaseTileY = -1;
    if (!ResolveGroundItemPickupCell(mode,
        sourceTileX,
        sourceTileY,
        groundItem,
        &chaseTileX,
        &chaseTileY)) {
        return false;
    }

    if (TryRequestMoveToCell(mode,
        sourceTileX,
        sourceTileY,
        chaseTileX,
        chaseTileY,
        true)) {
        mode.m_lastMoveRequestTick = now;
        return true;
    }

    return false;
}

bool TryRequestGroundItemFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_world || !mode.m_view || mode.m_canRotateView) {
        return false;
    }

    CItem* hoveredItem = nullptr;
    if (!mode.m_world->FindHoveredGroundItemScreen(mode.m_view->GetViewMatrix(),
        screenX,
        screenY,
        &hoveredItem,
        nullptr,
        nullptr)
        || !hoveredItem) {
        return false;
    }

    return TryBeginGroundItemPickup(mode, hoveredItem->m_aid);
}

bool RequestEquipInventoryItem(u16 itemIndex, u16 equipLocation, unsigned expectedItemId = 0)
{
    const auto* item = g_session.GetInventoryItemByIndex(itemIndex);
    if (static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots
        || !item || item->m_num <= 0 || !equipLocation || (expectedItemId && item->GetItemId() != expectedItemId)) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    PACKET_CZ_REQ_WEAR_EQUIP packet{};
    packet.PacketType = profile.equipItem;
    packet.ItemIndex = itemIndex;
    packet.WearLocation = equipLocation;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_waitingWearEquipAck = itemIndex;
        }
    }

    DbgLog("[GameMode] equip request index=%u wear=0x%04X sent=%d\n",
        static_cast<unsigned int>(itemIndex),
        static_cast<unsigned int>(equipLocation),
        sent ? 1 : 0);
    return sent;
}

bool RequestUnequipInventoryItem(u16 itemIndex, unsigned expectedItemId = 0)
{
    const auto* item = g_session.GetInventoryItemByIndex(itemIndex);
    if (static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots
        || !item || !item->m_wearLocation || (expectedItemId && item->GetItemId() != expectedItemId)) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    PACKET_CZ_REQ_TAKEOFF_EQUIP packet{};
    packet.PacketType = profile.unequipItem;
    packet.ItemIndex = itemIndex;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_waitingTakeoffEquipAck = itemIndex;
        }
    }

    DbgLog("[GameMode] unequip request index=%u sent=%d\n",
        static_cast<unsigned int>(itemIndex),
        sent ? 1 : 0);
    return sent;
}

bool RequestUpgradeSkillLevel(u16 skillId)
{
    if (skillId == 0) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    PACKET_CZ_SKILLUP packet{};
    packet.PacketType = profile.skillUp;
    packet.SkillId = skillId;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_isReqUpgradeSkillLevel = 1;
        }
    }

    DbgLog("[GameMode] skill upgrade request skid=%u sent=%d\n",
        static_cast<unsigned int>(skillId),
        sent ? 1 : 0);
    return sent;
}

u32 ResolveShortcutSkillTargetId(const CGameMode* gameMode)
{
    if (gameMode && gameMode->m_lastLockOnMonGid != 0) {
        return gameMode->m_lastLockOnMonGid;
    }
    if (g_session.m_gid != 0) {
        return g_session.m_gid;
    }
    return g_session.m_aid;
}

bool SendUseSkillToIdPacket(u16 skillId, u16 skillLevel, u32 targetGid)
{
    if (skillId == 0 || skillLevel == 0 || targetGid == 0) {
        return false;
    }

    std::array<u8, sizeof(PACKET_CZ_USESKILLTOID_PACKETVER22)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveUseSkillToIdPacket(
            skillId,
            skillLevel,
            targetGid,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    if (sent) {
        NoteLocalHideSkillRequest(skillId, targetGid);
    }
    DbgLog("[GameMode] useskilltoid skillId=%u level=%u target=%u sent=%d\n",
        static_cast<unsigned int>(skillId),
        static_cast<unsigned int>(skillLevel),
        static_cast<unsigned int>(targetGid),
        sent ? 1 : 0);
    return sent;
}

bool DispatchSourcePlayerSkill(CGameMode& mode,int skillId,u32 targetGid,int requestedLevel)
{
    const auto special=[&](u16 opcode){
        const u8 packet[]={static_cast<u8>(opcode),static_cast<u8>(opcode>>8),static_cast<u8>(targetGid),
            static_cast<u8>(targetGid>>8),static_cast<u8>(targetGid>>16),static_cast<u8>(targetGid>>24)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet),sizeof(packet));
    };
    if(skillId==20000)return special(0x01e5);
    if(skillId==20001){
        const char* text="[ (val1 == 20001) ]";
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text),0x6e96ff,0,0);
        return special(0x01f9);
    }
    if(ro::commerce::state.vendingActive || !mode.m_world || !mode.m_world->m_player)return false;
    const bool homun=skillId>=8000 && skillId<8017,merc=SKILL_INFO::IsMercenarySkill(skillId);
    int level=requestedLevel;
    if(!level){
        const auto* skill=homun?g_session.GetHomunSkillItemBySkillId(skillId)
            :merc?g_session.GetMercSkillItemBySkillId(skillId):g_session.GetSkillItemBySkillId(skillId);
        if(!skill || !skill->m_isValid)skill=g_session.GetTemporarySkillItemBySkillId(skillId);
        level=skill && skill->m_isValid?skill->level:0;
    }
    if((!homun && !merc && mode.m_world->m_player->m_stateId==6) || g_windowMgr.m_chatRoomWnd)return false;
    if(skillId==154){
        // The port currently keeps the local effect mask on its player actor;
        // Ref reads the session's self-effect mask here (not a target actor).
        const unsigned message=static_cast<s32>(g_session.GetCharacterVitals().sp)<40?202:!IsPushCartEffectState(mode.m_world->m_player->m_effectState)?660:0;
        if(message){
            ro::UiMessageStrings strings;strings.Load();const auto text=strings.Get(message,message==202?"Not enough SP.":"You must equip a Pushcart.");
            g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),255,10,0);return false;
        }
        g_windowMgr.MakeWindow(UIWindowMgr::WID_SELECTCARTWND);
    }
    // The source dispatcher permits zero when both rank lookups miss, and
    // truncates nonzero raw arguments only at the wire field.
    std::array<u8,sizeof(PACKET_CZ_USESKILLTOID_PACKETVER22)> packet{};int length=0;
    if(!ro::net::BuildActiveUseSkillToIdPacket(static_cast<u16>(skillId),static_cast<u16>(level),targetGid,
        packet.data(),static_cast<int>(packet.size()),&length))return false;
    const bool sent=CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),length);
    if(sent)NoteLocalHideSkillRequest(static_cast<u16>(skillId),targetGid);
    return sent;
}

bool DispatchSourceGroundSkill(const std::array<int,4>& values)
{
    int level=values[3];
    if(!level){const auto* row=g_session.GetSkillItemBySkillId(values[0]);level=row?row->level:0;}
    PACKET_CZ_USESKILLTOPOS packet{};
    packet.PacketType=ro::net::GetActiveMapGameplaySendProfile().useSkillToPos;
    packet.SkillId=static_cast<u16>(values[0]);packet.SkillLevel=static_cast<u16>(level);
    packet.X=static_cast<u16>(values[1]);packet.Y=static_cast<u16>(values[2]);
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet),sizeof(packet));
}

bool DispatchSourceGroundSkillText(const GroundSkillTextRequest& request)
{
    const int codePage = ro::SelectedSourceTextCodePage();
    const auto text = ro::EncodeSourceText(request.text ? request.text : "", codePage);
    if (!text) return false;
    auto& filter=ro::GetMannerFilter();filter.EnsureLoaded();
    if(filter.IsBadEncodedSentence(*text, codePage)){
        g_windowMgr.SendMsg(3,0,0,0,0);
        ro::UiMessageStrings table;table.Load();const auto notice=table.Get(50,"Cannot send because of inappropriate language.");
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),255,0,0);return false;
    }
    const u16 skill=static_cast<u16>(request.values[0]);u16 level=static_cast<u16>(request.values[3]);
    if(!level){const auto* row=g_session.GetSkillItemBySkillId(skill);level=static_cast<u16>(row?row->level:0);}
    std::array<u8,sizeof(PACKET_CZ_USESKILLTOPOSINFO)> packet{};int length=0;
    if(!ro::net::BuildActiveUseSkillToPosInfoPacket(skill,level,static_cast<s16>(request.values[1]),static_cast<s16>(request.values[2]),
        *text,packet.data(),static_cast<int>(packet.size()),&length))return false;
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),length);
}

bool SendUseSkillToPosInfoPacket(u16 skillId, u16 skillLevel, int cellX, int cellY, const std::string& contents)
{
    if (skillId == 0 || skillLevel == 0 || cellX < 0 || cellY < 0 || cellX > 0x7FFF || cellY > 0x7FFF) {
        return false;
    }

    std::array<u8, sizeof(PACKET_CZ_USESKILLTOPOSINFO)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveUseSkillToPosInfoPacket(skillId, skillLevel,
            static_cast<s16>(cellX), static_cast<s16>(cellY),
            std::string_view(contents.data(), contents.size()),
            packetBuffer.data(), static_cast<int>(packetBuffer.size()), &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()), packetLength);
    DbgLog("[GameMode] useskilltoposinfo skillId=%u level=%u cell=(%d,%d) textBytes=%u sent=%d\n",
        static_cast<unsigned int>(skillId), static_cast<unsigned int>(skillLevel), cellX, cellY,
        static_cast<unsigned int>(contents.size()), sent ? 1 : 0);
    return sent;
}

bool SendUseSkillMapPacket(u16 skillId, const char* mapName)
{
    if (skillId == 0 || !mapName || *mapName == '\0') {
        return false;
    }

    std::array<u8, sizeof(PACKET_CZ_USESKILLMAP)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveUseSkillMapPacket(
            skillId,
            mapName,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    const PACKET_CZ_USESKILLMAP* packet = reinterpret_cast<const PACKET_CZ_USESKILLMAP*>(packetBuffer.data());
    DbgLog("[GameMode] useskillmap skillId=%u map='%s' sent=%d\n",
        static_cast<unsigned int>(skillId),
        packet->MapName,
        sent ? 1 : 0);
    return sent;
}

void ClearPendingSkillUse(CGameMode& mode)
{
    mode.SendMsg(78,0,0,0);
}

void BeginSkillChase(CGameMode& mode, u32 targetGid, int skillId, int skillLevel, int skillRange)
{
    if(!mode.m_world || !mode.m_world->m_player)return;
    auto* player=mode.m_world->m_player;
    player->SendMsg(nullptr,41,targetGid,skillId,skillRange);
    player->SendMsg(nullptr,90,skillLevel,0,0);
    mode.SynchronizePlayerSkillProceed(player);
}

void BeginTouchSkillChase(CGameMode& mode, u32 targetAid, int skillId, int skillLevel, int skillRange)
{
    if(!mode.m_world || !mode.m_world->m_player)return;
    const SKILL_USE_INFO pending{skillId,skillLevel,skillRange,5};
    mode.m_world->m_player->SendMsg(nullptr,103,targetAid,reinterpret_cast<msgparam_t>(&pending),0);
}

bool RequestPendingGroundSkill(CGameMode& mode, int x, int y, bool shift)
{
    const auto pending = mode.m_skillUseInfo;
    const bool homunculus = pending.id >= 8000 && pending.id < 8017;
    const bool mercenary = SKILL_INFO::IsMercenarySkill(pending.id);
    if (homunculus || mercenary) {
        companion_ai::Msg2Ai command;
        command.command = companion_ai::SKILL_AREA_CMD;
        command.skillLevel = pending.level;
        command.skillId = pending.id;
        command.x = x;
        command.y = y;
        const auto kind = homunculus ? companion_ai::Kind::Homunculus : companion_ai::Kind::Mercenary;
        companion_ai::Runtime::QueueCommand(kind, command, shift);
        return true;
    }

    mode.m_world->m_player->SendMsg(nullptr, 65, pending.id, x, y);
    mode.m_world->m_player->SendMsg(nullptr, 90, pending.level, 0, 0);
    return true;
}

bool TryRequestPendingSkillFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!HasPendingSkillUse(mode) || !mode.m_world || !mode.m_world->m_player || !mode.m_view || mode.m_canRotateView) {
        return false;
    }

    if(mode.m_skillUseInfo.useType!=0 && mode.m_skillUseInfo.useType!=1){
        CSkill* unit=nullptr;
        if(mode.m_world->FindHoveredSkillScreen(mode.m_view->GetViewMatrix(),screenX,screenY,&unit) && unit){
            const bool special=unit->m_job==141 || unit->m_job==143 || unit->m_job==152;
            const int id=mode.m_skillUseInfo.id;
            if((special && !(id>=215 && id<=218)) || (!special && mode.m_skillUseInfo.useType==5)){
                mode.m_world->m_player->SendMsg(nullptr,103,unit->m_aid,reinterpret_cast<msgparam_t>(&mode.m_skillUseInfo),0);
                return true;
            }
            return false;
        }
        if(mode.m_skillUseInfo.useType==2 || mode.m_skillUseInfo.useType==4){
            CGameActor* actor=nullptr;
            if(mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),mode.m_view->GetCameraLongitude(),
                screenX,screenY,&actor,nullptr,nullptr,true))
                return TryRequestSourceSkillOnActor(mode,actor,(GetKeyState(VK_SHIFT)&0x8000)!=0,
                    (GetKeyState(VK_MENU)&0x8000)!=0,(GetKeyState(VK_CONTROL)&0x8000)!=0);
        }
        return false;
    }

    if(mode.m_skillUseInfo.useType==1){
        int x=-1,y=-1;if(!mode.m_view->ScreenToHoveredAttrCell(screenX,screenY,&x,&y))return false;
        return RequestPendingGroundSkill(mode, x, y, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
    }

    return false;
}

bool RequestShortcutSlotUpdate(int absoluteSlotIndex, const SHORTCUT_SLOT* replacement = nullptr)
{
    const SHORTCUT_SLOT* current = g_session.GetShortcutSlotByAbsoluteIndex(absoluteSlotIndex);
    if (!current) {
        return false;
    }
    const SHORTCUT_SLOT& slot = replacement ? *replacement : *current;
    if (replacement && current->id == slot.id && current->isSkill == slot.isSkill && current->count == slot.count) {
        return true;
    }

    PACKET_CZ_SHORTCUT_KEY_CHANGE packet{};
    packet.PacketType = PacketProfile::LegacyShortcutSend::kKeyChange;
    packet.Index = static_cast<u16>(absoluteSlotIndex);
    packet.IsSkill = slot.id != 0 && slot.isSkill != 0 ? 1 : 0;
    packet.Id = slot.id;
    packet.Count = slot.id != 0 ? slot.count : 0;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent && replacement) {
        g_session.SetShortcutSlotByAbsoluteIndex(absoluteSlotIndex, packet.IsSkill, packet.Id, packet.Count);
    }
    DbgLog("[GameMode] shortcut update slot=%d skill=%u id=%u count=%u sent=%d\n",
        absoluteSlotIndex,
        static_cast<unsigned int>(packet.IsSkill),
        static_cast<unsigned int>(packet.Id),
        static_cast<unsigned int>(packet.Count),
        sent ? 1 : 0);
    return sent;
}

bool RequestUseInventoryItem(u16 itemIndex)
{
    if (static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots) {
        return false;
    }

    const ITEM_INFO* item = g_session.GetInventoryItemByIndex(itemIndex);
    if (!item || !item->GetItemId() || item->m_num <= 0) {
        return false;
    }

    // Ref inventory/shortcut equipment commands both send CZ_REQ_WEAR_EQUIP.
    // Ammo is stackable, but uses EQP_AMMO rather than a consumable-use packet.
    if (item->m_location != 0 && (item->m_itemType == 4 || item->m_itemType == 5
        || (item->m_itemType >= 8 && item->m_itemType <= 17))) {
        if (item->m_wearLocation != 0) return true;
        return RequestEquipInventoryItem(itemIndex, static_cast<u16>(item->m_location));
    }

    if (item->IsCard()) {
        if (const auto* mode = g_modeMgr.GetCurrentGameMode(); mode && mode->m_pendingItemComposition.cardIndex) return false;
        const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
        PACKET_CZ_ITEM_COMPOSITION_LIST packet{};
        packet.PacketType = profile.itemCompositionList;
        packet.CardItemIndex = static_cast<u16>(item->m_itemIndex);

        const bool sent = CRagConnection::instance()->SendPacket(
            reinterpret_cast<const char*>(&packet),
            static_cast<int>(sizeof(packet)));
        if (sent) {
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_lastCardItemIndex = static_cast<int>(item->m_itemIndex);
            }
        }

        DbgLog("[GameMode] item composition list request itemId=%u index=%u sent=%d\n",
            static_cast<unsigned int>(item->GetItemId()),
            static_cast<unsigned int>(item->m_itemIndex),
            sent ? 1 : 0);
        return sent;
    }

    std::array<u8, sizeof(PACKET_CZ_USEITEM_PACKETVER22)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveUseItemPacket(
            static_cast<u16>(item->m_itemIndex),
            g_session.m_aid,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_waitingUseItemAck = static_cast<int>(item->m_itemIndex);
        }
    }

    DbgLog("[GameMode] inventory item use itemId=%u index=%u sent=%d\n",
        static_cast<unsigned int>(item->GetItemId()),
        static_cast<unsigned int>(item->m_itemIndex),
        sent ? 1 : 0);
    return sent;
}

bool RequestIdentifyInventoryItem(s16 itemIndex)
{
    auto* mode = g_modeMgr.GetCurrentGameMode();
    if (itemIndex < -1 || (mode && mode->m_pendingIdentifyIndex)) return false;
    unsigned itemId = 0;
    if (itemIndex >= 0) {
        const ITEM_INFO* item = g_session.GetInventoryItemByIndex(static_cast<unsigned int>(itemIndex));
        if (static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots
            || !item || !item->GetItemId() || item->m_num <= 0 || item->m_isIdentified != 0) {
            return false;
        }
        itemId = item->GetItemId();
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    PACKET_CZ_ITEM_IDENTIFY packet{};
    packet.PacketType = profile.itemIdentify;
    packet.ItemIndex = itemIndex;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent && mode && itemId) {
        mode->m_pendingIdentifyIndex = static_cast<unsigned>(itemIndex);
        mode->m_pendingIdentifyItemId = itemId;
    }
    DbgLog("[GameMode] item identify request index=%d sent=%d\n",
        static_cast<int>(itemIndex),
        sent ? 1 : 0);
    return sent;
}

bool RequestComposeCardIntoEquipment(s16 cardItemIndex, s16 equipItemIndex)
{
    auto* mode = g_modeMgr.GetCurrentGameMode();
    if (!mode || mode->m_pendingItemComposition.cardIndex) return false;
    if (cardItemIndex < 0 || equipItemIndex < 0) {
        mode->m_lastCardItemIndex = 0;
        return false;
    }

    const ITEM_INFO* cardItem = g_session.GetInventoryItemByIndex(static_cast<unsigned int>(cardItemIndex));
    const ITEM_INFO* equipItem = g_session.GetInventoryItemByIndex(static_cast<unsigned int>(equipItemIndex));
    if (!cardItem || !equipItem || cardItemIndex != mode->m_lastCardItemIndex || cardItemIndex == equipItemIndex
        || cardItemIndex < 2 || cardItemIndex > 101 || equipItemIndex < 2 || equipItemIndex > 101
        || equipItem->m_wearLocation || !equipItem->m_isIdentified || cardItem->m_num <= 0) {
        return false;
    }
    if (!cardItem->IsCard()) {
        return false;
    }

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    PACKET_CZ_ITEM_COMPOSITION packet{};
    packet.PacketType = profile.itemComposition;
    packet.CardItemIndex = static_cast<u16>(cardItemIndex);
    packet.EquipItemIndex = static_cast<u16>(equipItemIndex);

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        auto& pending = mode->m_pendingItemComposition;
        pending.cardIndex = cardItem->m_itemIndex;
        pending.equipmentIndex = equipItem->m_itemIndex;
        pending.cardId = cardItem->GetItemId();
        pending.equipmentId = equipItem->GetItemId();
        std::copy_n(equipItem->m_slot, pending.equipmentCards.size(), pending.equipmentCards.begin());
    }
    DbgLog("[GameMode] item composition request cardIndex=%d equipIndex=%d sent=%d\n",
        static_cast<int>(cardItemIndex),
        static_cast<int>(equipItemIndex),
        sent ? 1 : 0);
    return sent;
}

bool RequestDropInventoryItem(u16 itemIndex, u16 amount, unsigned expectedItemId)
{
    if (static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots
        || amount == 0 || !expectedItemId) {
        return false;
    }

    const ITEM_INFO* item = g_session.GetInventoryItemByIndex(itemIndex);
    if (!item || item->GetItemId() != expectedItemId || item->m_num <= 0 || item->m_wearLocation != 0) {
        return false;
    }

    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        if (gameMode->m_waitingItemThrowAck != 0) {
            return false;
        }
    }

    const int availableCount = (std::min)(item->m_num, 0xFFFF);
    const int requestedCount = static_cast<int>(amount);
    const int clampedCount = (std::max)(1, (std::min)(requestedCount, availableCount));
    const u16 dropAmount = static_cast<u16>(clampedCount);

    const ro::net::MapGameplaySendProfile& profile = ro::net::GetActiveMapGameplaySendProfile();
    PACKET_CZ_ITEM_THROW packet{};
    packet.PacketType = profile.dropItem;
    packet.padding0[0] = 0;
    packet.padding0[1] = 0;
    packet.padding0[2] = 0;
    packet.ItemIndex = itemIndex;
    packet.padding1 = 0;
    packet.Count = dropAmount;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_waitingItemThrowAck = static_cast<int>(itemIndex);
        }
    }

    DbgLog("[GameMode] inventory item drop itemId=%u index=%u amount=%u opcode=0x%04X sent=%d\n",
        static_cast<unsigned int>(item->GetItemId()),
        static_cast<unsigned int>(itemIndex),
        static_cast<unsigned int>(dropAmount),
        static_cast<unsigned int>(profile.dropItem),
        sent ? 1 : 0);
    return sent;
}

bool RequestShortcutSlotUse(int absoluteSlot)
{
    // Every caller supplies the fixed bank/column index (0..26).
    const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(absoluteSlot);
    if (!slot || slot->id == 0) {
        return false;
    }

    if (slot->isSkill == 0) {
        const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id);
        if (!item || item->m_itemIndex == 0) {
            return false;
        }

        const bool sent = RequestUseInventoryItem(static_cast<u16>(item->m_itemIndex));
        DbgLog("[GameMode] shortcut item use slot=%d itemId=%u index=%u sent=%d\n",
            absoluteSlot,
            static_cast<unsigned int>(slot->id),
            static_cast<unsigned int>(item->m_itemIndex),
            sent ? 1 : 0);
        return sent;
    }

    const int id=static_cast<int>(slot->id);
    const auto* skill=id>=8000 && id<8017?g_session.GetHomunSkillItemBySkillId(id)
        :SKILL_INFO::IsMercenarySkill(id)?g_session.GetMercSkillItemBySkillId(id):g_session.GetSkillItemBySkillId(id);
    if(!skill || !skill->m_isValid || !skill->level)return false;
    auto* mode=g_modeMgr.GetCurrentGameMode();if(!mode)return false;
    const PLAYER_SKILL_INFO snapshot=*skill;
    // Selected UIShortCutWnd4A2E33 sign-extends the packed slot word. The
    // source caller ignores126's result; activating does not cancel a proceed.
    mode->SendMsg(126,reinterpret_cast<msgparam_t>(&snapshot),static_cast<s16>(slot->count),0);
    return true;
}

bool RequestCartOff()
{
    std::array<u8, sizeof(PACKET_CZ_REQ_CARTOFF)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveCartOffPacket(
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    DbgLog("[GameMode] cart off sent=%d\n", sent ? 1 : 0);
    return sent;
}

bool RequestChangeCart(u16 type)
{
    if (type < 1 || type > 5) {
        return false;
    }

    std::array<u8, sizeof(PACKET_CZ_REQ_CHANGECART)> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveChangeCartPacket(
            type,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packetBuffer.data()),
        packetLength);
    DbgLog("[GameMode] change cart type=%u sent=%d\n",
        static_cast<unsigned int>(type),
        sent ? 1 : 0);
    return sent;
}

bool RequestIncreaseStatus(u16 statusId)
{
    if (statusId < 13 || statusId > 18) {
        return false;
    }

    PACKET_CZ_STATUS_CHANGE packet{};
    packet.PacketType = 0x00BB;
    packet.StatusId = statusId;
    packet.Amount = 1;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] status increase request status=%u sent=%d\n",
        static_cast<unsigned int>(statusId),
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcContact(u32 npcId)
{
    if (npcId == 0) {
        return false;
    }

    PACKET_CZ_CONTACTNPC packet{};
    packet.PacketType = PacketProfile::LegacyNpcScriptSend::kContactNpc;
    packet.NpcId = npcId;
    packet.Type = 1;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] npc contact npc=%u sent=%d\n",
        static_cast<unsigned int>(npcId),
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcMenuSelection(u32 npcId, u8 choice)
{
    if (npcId == 0 || choice == 0) {
        return false;
    }

    PACKET_CZ_NPC_SELECTMENU packet{};
    packet.PacketType = PacketProfile::LegacyNpcScriptSend::kSelectMenu;
    packet.NpcId = npcId;
    packet.Choice = choice;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent && choice == 0xFF) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_isOnQuest = 0;
            gameMode->m_lastNaid = 0;
        }
    }
    DbgLog("[GameMode] npc menu npc=%u choice=%u sent=%d\n",
        static_cast<unsigned int>(npcId),
        static_cast<unsigned int>(choice),
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcNext(u32 npcId)
{
    if (npcId == 0) {
        return false;
    }

    PACKET_CZ_NPC_NEXT_CLICK packet{};
    packet.PacketType = PacketProfile::LegacyNpcScriptSend::kNextClick;
    packet.NpcId = npcId;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] npc next npc=%u sent=%d\n",
        static_cast<unsigned int>(npcId),
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcInputNumber(u32 npcId, u32 value)
{
    if (npcId == 0) {
        return false;
    }

    PACKET_CZ_NPC_INPUT_NUMBER packet{};
    packet.PacketType = PacketProfile::LegacyNpcScriptSend::kInputNumber;
    packet.NpcId = npcId;
    packet.Value = value;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] npc input number npc=%u value=%u sent=%d\n",
        static_cast<unsigned int>(npcId),
        static_cast<unsigned int>(value),
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcInputString(u32 npcId, const char* text)
{
    if (npcId == 0 || !text || *text == '\0') {
        return false;
    }

    const int codePage = ro::SelectedSourceTextCodePage();
    const auto encoded = ro::EncodeSourceText(text, codePage);
    if (!encoded || encoded->size() > PacketProfile::LegacyNpcScriptSend::kInputStringMaxBytes) {
        return false;
    }
    auto& filter = ro::GetMannerFilter();
    filter.EnsureLoaded();
    if (filter.IsBadEncodedSentence(*encoded, codePage)) {
        ro::UiMessageStrings table;
        table.Load();
        const auto notice = table.Get(50, "Cannot send because of inappropriate language.");
        g_windowMgr.SendMsg(1, reinterpret_cast<msgparam_t>(notice.c_str()), 255, 0, 0);
        return false;
    }

    const size_t textLen = encoded->size();
    const size_t packetLen = sizeof(PACKET_CZ_NPC_INPUT_STRING) + textLen + 1;
    std::vector<u8> bytes(packetLen, 0);
    auto* packet = reinterpret_cast<PACKET_CZ_NPC_INPUT_STRING*>(bytes.data());
    packet->PacketType = PacketProfile::LegacyNpcScriptSend::kInputString;
    packet->PacketLength = static_cast<u16>(packetLen);
    packet->NpcId = npcId;
    std::memcpy(bytes.data() + sizeof(PACKET_CZ_NPC_INPUT_STRING), encoded->data(), textLen);

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()));
    DbgLog("[GameMode] npc input string npc=%u len=%zu sent=%d\n",
        static_cast<unsigned int>(npcId),
        textLen,
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcCloseDialog(u32 npcId)
{
    if (npcId == 0) {
        return false;
    }

    PACKET_CZ_NPC_CLOSE_DIALOG packet{};
    packet.PacketType = PacketProfile::LegacyNpcScriptSend::kCloseDialog;
    packet.NpcId = npcId;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
            gameMode->m_isOnQuest = 0;
            gameMode->m_lastNaid = 0;
        }
    }
    DbgLog("[GameMode] npc close npc=%u sent=%d\n",
        static_cast<unsigned int>(npcId),
        sent ? 1 : 0);
    return sent;
}

bool RequestNpcShopDealType(u8 dealType)
{
    CGameMode* const gameMode = g_modeMgr.GetCurrentGameMode();
    if (g_session.m_shopNpcId == 0 || dealType > 1 || g_session.m_shopMode != NpcShopMode::None
        || g_session.m_shopChoicePending
        || (gameMode && gameMode->m_isPlayerDead)) {
        return false;
    }

    PACKET_CZ_ACK_SELECT_DEALTYPE packet{};
    packet.PacketType = PacketProfile::LegacyNpcShopSend::kSelectDealType;
    packet.NpcId = g_session.m_shopNpcId;
    packet.Type = dealType;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (sent) {
        g_session.m_shopChoicePending = true;
        if (g_windowMgr.m_chooseSellBuyWnd) g_windowMgr.m_chooseSellBuyWnd->SetShow(0);
    }
    DbgLog("[GameMode] npc shop deal type npc=%u type=%u sent=%d\n",
        static_cast<unsigned int>(packet.NpcId),
        static_cast<unsigned int>(packet.Type),
        sent ? 1 : 0);
    return sent;
}

bool SameNpcShopSellItem(const ITEM_INFO& first, const ITEM_INFO& second)
{
    // Count may change independently, but an indexed slot must still contain
    // the quoted item, including its identification/refinement/card metadata.
    return first.GetItemId() == second.GetItemId() && first.m_itemIndex == second.m_itemIndex
        && first.m_itemType == second.m_itemType && first.m_location == second.m_location
        && first.m_isIdentified == second.m_isIdentified && first.m_isDamaged == second.m_isDamaged
        && first.m_refiningLevel == second.m_refiningLevel && first.m_isYours == second.m_isYours
        && first.m_deleteTime == second.m_deleteTime
        && std::equal(first.m_slot, first.m_slot + 4, second.m_slot);
}

constexpr size_t kNpcShopRequestMaxLength = 0x7fff;

bool ValidateNpcShopDeal(NpcShopMode mode)
{
    CGameMode* const gameMode = g_modeMgr.GetCurrentGameMode();
    const auto& deals = g_session.m_shopDealRows;
    if (g_session.m_shopMode != mode || g_session.m_shopChoicePending
        || deals.empty()
        || deals.size() > (kNpcShopRequestMaxLength - 4u) / 4u || (gameMode && gameMode->m_isPlayerDead)) return false;

    const auto quoteKey = [mode](unsigned int id, unsigned int index) {
        return mode == NpcShopMode::Buy ? static_cast<unsigned long long>(id)
            : (static_cast<unsigned long long>(index) << 32) | id;
    };
    std::unordered_map<unsigned long long, const NPC_SHOP_ROW*> quotes;
    quotes.reserve(g_session.m_shopRows.size());
    for (const auto& source : g_session.m_shopRows)
        quotes.emplace(quoteKey(source.itemInfo.GetItemId(), source.sourceItemIndex), &source);
    std::unordered_set<unsigned int> identities;
    identities.reserve(deals.size());
    for (const auto& deal : deals) {
        const unsigned int id = deal.itemInfo.GetItemId();
        if (id == 0 || id > 0xffffu || deal.quantity <= 0 || deal.quantity > 30000) return false;
        const unsigned int key = mode == NpcShopMode::Buy ? id : deal.sourceItemIndex;
        if (!identities.insert(key).second) return false;
        const auto found = quotes.find(quoteKey(id, deal.sourceItemIndex));
        if (found == quotes.end()) return false;
        const NPC_SHOP_ROW& quoted = *found->second;
        if (deal.unitPrice != g_session.GetNpcShopUnitPrice(quoted)
            || deal.itemInfo.m_itemType != quoted.itemInfo.m_itemType
            || (!NpcShopItemIsStackable(quoted.itemInfo.m_itemType) && deal.quantity != 1)) return false;
        if (mode == NpcShopMode::Sell) {
            const ITEM_INFO* current = g_session.GetInventoryItemByIndex(key);
            if (key < 2 || key > 0xffffu || !current || current->m_wearLocation != 0
                || current->m_num < deal.quantity || quoted.availableCount < deal.quantity
                || !SameNpcShopSellItem(quoted.itemInfo, *current)
                || !SameNpcShopSellItem(deal.itemInfo, *current)) return false;
        }
    }
    return true;
}

void FinishNpcShopInteraction(NpcShopMode pendingResult)
{
    g_windowMgr.CloseNpcDialogWindows();
    g_windowMgr.CloseNpcShopWindows();
    // Replies have no transaction ID. Preserve expected reply order across
    // later NPC contexts instead of letting an old CA consume a new purchase.
    g_session.ClearNpcShopState(false);
    g_session.QueueNpcShopResult(pendingResult);
    if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        gameMode->m_isOnQuest = 0;
        if (gameMode->m_dragType == static_cast<int>(DragType::ShopItem)) {
            gameMode->m_dragType = 0;
            gameMode->m_dragInfo = DRAG_INFO{};
        }
    }
}

bool RequestNpcCashShopPurchase(u32 kafra)
{
    if (!g_session.m_shopUsesCash || g_session.m_cashShopPending || !ValidateNpcShopDeal(NpcShopMode::Buy)
        || g_session.m_shopDealRows.size() != 1) return false;
    const auto& row = g_session.m_shopDealRows.front();
    const auto total = static_cast<u64>(row.unitPrice) * row.quantity;
    if (!total || total > 0x7fffffffu || kafra > total || kafra > g_session.m_kafraShopPoints
        || total - kafra > g_session.m_cashShopPoints) return false;

    const auto item = row.itemInfo.GetItemId();
    const auto quantity = static_cast<u16>(row.quantity);
    const u8 packet[] = {0x88, 2, static_cast<u8>(item), static_cast<u8>(item >> 8),
        static_cast<u8>(quantity), static_cast<u8>(quantity >> 8), static_cast<u8>(kafra),
        static_cast<u8>(kafra >> 8), static_cast<u8>(kafra >> 16), static_cast<u8>(kafra >> 24)};
    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet))) return false;
    g_session.m_cashShopPending = true;
    g_session.m_cashShopPendingQuote = g_session.m_cashShopQuote;
    g_session.m_shopDealRows.clear();
    g_session.m_shopSelectedDealRow = -1;
    g_session.m_shopDealTotal = 0;
    return true;
}

u32 CashShopDealIdentity()
{
    if (!g_session.m_shopUsesCash || g_session.m_cashShopPending || !ValidateNpcShopDeal(NpcShopMode::Buy)
        || g_session.m_shopDealRows.size() != 1) return 0;
    const auto& row = g_session.m_shopDealRows.front();
    return (static_cast<u32>(row.quantity) << 16) | row.itemInfo.GetItemId();
}

bool ConfirmCashShopContribution(u32 quote, u32 requested, u32 identity)
{
    auto* owner = g_windowMgr.m_itemPurchaseWnd;
    if (!identity || identity != CashShopDealIdentity() || quote != g_session.m_cashShopQuote
        || !owner || !owner->m_show || g_windowMgr.m_modalWindow) return false;

    const auto& row = g_session.m_shopDealRows.front();
    const auto total = static_cast<u64>(row.unitPrice) * row.quantity;
    if (!total || total > 0x7fffffffu) return false;
    const auto rounded = (std::min)(requested, g_session.m_kafraShopPoints) / 100 * 100;
    const auto kafra = static_cast<u32>((std::min)(total, static_cast<u64>(rounded)));
    const auto cash = total - kafra;
    if (cash > g_session.m_cashShopPoints) {
        g_windowMgr.PushChatEvent("You do not have enough cash points for that split.", 0x00ff0000u, 6);
        return false;
    }
    const auto text = "Buy " + std::to_string(row.quantity) + " × " + row.itemInfo.GetDisplayName()
        + " for " + std::to_string(cash) + " cash points and " + std::to_string(kafra) + " Kafra points?";
    auto* box = new UIChatRoomMessageBox(text, 0);
    const auto generation = g_modeMgr.GetOperationGeneration();
    box->ConfigureConfirmation(owner, [quote, identity, kafra, generation](bool accepted) {
        if (accepted && g_modeMgr.GetOperationGeneration() == generation
            && quote == g_session.m_cashShopQuote && identity == CashShopDealIdentity()) {
            RequestNpcCashShopPurchase(kafra);
        }
    });
    g_windowMgr.m_roomMessageBox = box;
    g_windowMgr.m_children.push_back(box);
    g_windowMgr.m_modalWindow = box;
    g_windowMgr.m_editWindow = nullptr;
    return true;
}

bool BeginCashShopPurchase()
{
    const auto identity = CashShopDealIdentity();
    if (!identity || g_windowMgr.m_modalWindow) return false;
    const auto& row = g_session.m_shopDealRows.front();
    const auto total = static_cast<u64>(row.unitPrice) * row.quantity;
    if (!total || total > 0x7fffffffu) return false;
    const auto quote = g_session.m_cashShopQuote;
    const auto available = g_session.m_kafraShopPoints;
    if (available < 100) return ConfirmCashShopContribution(quote, 0, identity);
    const auto cash = g_session.m_cashShopPoints;
    const auto minimum = total > cash ? static_cast<u32>((total - cash + 99) / 100 * 100) : 0;
    auto* input = static_cast<UINpcInputWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_NPCINPUTWND));
    if (!input) return false;
    input->OpenGameNumberPrompt("Kafra points to use (multiples of 100)",
        CGameMode::GameMsg_ConfirmCashShopContribution, quote, minimum, available, nullptr, identity, true);
    return true;
}

bool RequestNpcShopPurchaseList()
{
    if (!ValidateNpcShopDeal(NpcShopMode::Buy)) {
        return false;
    }

    if (g_session.m_shopUsesCash) return BeginCashShopPurchase();

    const size_t rowCount = g_session.m_shopDealRows.size();
    const size_t packetLen = sizeof(PACKET_CZ_PC_PURCHASE_ITEMLIST) + rowCount * 4;
    if (packetLen > kNpcShopRequestMaxLength) {
        return false;
    }

    std::vector<u8> bytes(packetLen, 0);
    auto* packet = reinterpret_cast<PACKET_CZ_PC_PURCHASE_ITEMLIST*>(bytes.data());
    packet->PacketType = PacketProfile::LegacyNpcShopSend::kPurchaseItemList;
    packet->PacketLength = static_cast<u16>(packetLen);

    size_t offset = sizeof(PACKET_CZ_PC_PURCHASE_ITEMLIST);
    for (const NPC_SHOP_DEAL_ROW& row : g_session.m_shopDealRows) {
        const u16 amount = static_cast<u16>(row.quantity);
        const u16 itemId = static_cast<u16>(row.itemInfo.GetItemId());
        bytes[offset + 0] = static_cast<u8>(amount & 0xFFu);
        bytes[offset + 1] = static_cast<u8>((amount >> 8) & 0xFFu);
        bytes[offset + 2] = static_cast<u8>(itemId & 0xFFu);
        bytes[offset + 3] = static_cast<u8>((itemId >> 8) & 0xFFu);
        offset += 4;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()));
    DbgLog("[GameMode] npc shop purchase rows=%u sent=%d\n",
        static_cast<unsigned int>(rowCount),
        sent ? 1 : 0);
    if (sent) FinishNpcShopInteraction(NpcShopMode::Buy);
    return sent;
}

bool RequestNpcShopSellList()
{
    if (!ValidateNpcShopDeal(NpcShopMode::Sell)) {
        return false;
    }

    const size_t rowCount = g_session.m_shopDealRows.size();
    const size_t packetLen = sizeof(PACKET_CZ_PC_SELL_ITEMLIST) + rowCount * 4;
    if (packetLen > kNpcShopRequestMaxLength) {
        return false;
    }

    std::vector<u8> bytes(packetLen, 0);
    auto* packet = reinterpret_cast<PACKET_CZ_PC_SELL_ITEMLIST*>(bytes.data());
    packet->PacketType = PacketProfile::LegacyNpcShopSend::kSellItemList;
    packet->PacketLength = static_cast<u16>(packetLen);

    size_t offset = sizeof(PACKET_CZ_PC_SELL_ITEMLIST);
    for (const NPC_SHOP_DEAL_ROW& row : g_session.m_shopDealRows) {
        const u16 itemIndex = static_cast<u16>(row.sourceItemIndex);
        const u16 amount = static_cast<u16>(row.quantity);
        bytes[offset + 0] = static_cast<u8>(itemIndex & 0xFFu);
        bytes[offset + 1] = static_cast<u8>((itemIndex >> 8) & 0xFFu);
        bytes[offset + 2] = static_cast<u8>(amount & 0xFFu);
        bytes[offset + 3] = static_cast<u8>((amount >> 8) & 0xFFu);
        offset += 4;
    }

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()));
    DbgLog("[GameMode] npc shop sell rows=%u sent=%d\n",
        static_cast<unsigned int>(rowCount),
        sent ? 1 : 0);
    if (sent) FinishNpcShopInteraction(NpcShopMode::Sell);
    return sent;
}

bool RequestNpcShopCancel()
{
    if (g_session.m_shopNpcId == 0 && g_session.m_shopMode == NpcShopMode::None) return false;
    // Legacy primary cancel is local. An empty C8/C9 is a real transaction
    // (and produces CA/CB); 0146 belongs to script-dialog continuation.
    FinishNpcShopInteraction(NpcShopMode::None);
    return true;
}

bool RequestMoveInventoryItemToStorage(u16 itemIndex, u32 amount, unsigned itemId)
{
    const auto* item = g_session.GetInventoryItemByIndex(itemIndex);
    if (!g_session.IsStorageOpen() || !amount || !itemId || !item || item->GetItemId() != itemId || item->m_num <= 0
        || static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots) {
        return false;
    }

    PACKET_CZ_MOVE_ITEM_TO_STORE packet{};
    packet.PacketType = PacketProfile::ActiveStorageSend::kMoveToStorage;
    packet.ItemIndex = itemIndex;
    packet.Count = (std::min)(amount, static_cast<u32>(item->m_num));

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] storage store index=%u amount=%u sent=%d\n",
        static_cast<unsigned int>(packet.ItemIndex),
        static_cast<unsigned int>(packet.Count),
        sent ? 1 : 0);
    return sent;
}

bool RequestMoveStorageItemToInventory(u16 itemIndex, u32 amount, unsigned itemId)
{
    const auto* item = g_session.GetStorageItemByIndex(itemIndex);
    if (!g_session.IsStorageOpen() || !amount || !itemId || !item || item->GetItemId() != itemId || item->m_num <= 0
        || static_cast<unsigned>(itemIndex) - ro::item_snapshot::kFirstStorageIndex >= ro::item_snapshot::kStorageSlots) {
        return false;
    }

    PACKET_CZ_MOVE_ITEM_FROM_STORE packet{};
    packet.PacketType = PacketProfile::ActiveStorageSend::kMoveFromStorage;
    packet.ItemIndex = itemIndex;
    packet.Count = (std::min)(amount, static_cast<u32>(item->m_num));

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] storage withdraw index=%u amount=%u sent=%d\n",
        static_cast<unsigned int>(packet.ItemIndex),
        static_cast<unsigned int>(packet.Count),
        sent ? 1 : 0);
    return sent;
}

bool RequestCloseStorage()
{
    if (!g_session.IsStorageOpen()) {
        return false;
    }

    PACKET_CZ_CLOSE_STORE packet{};
    packet.PacketType = PacketProfile::ActiveStorageSend::kCloseStorage;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] storage close sent=%d\n", sent ? 1 : 0);
    return sent;
}

std::string NormalizePartyNameForPacket(const char* rawName)
{
    std::string partyName = rawName ? rawName : "";
    const size_t first = partyName.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const size_t last = partyName.find_last_not_of(" \t\r\n");
    partyName = partyName.substr(first, last - first + 1);
    if (partyName.size() > 23) {
        partyName.resize(23);
    }
    return partyName;
}

bool RequestCreateParty(const std::string& partyName, bool itemShareParty, bool itemSharingTypeShared)
{
    if (g_session.GetNumParty() > 0) {
        return false;
    }

    if (partyName.empty()) {
        return false;
    }

    PACKET_CZ_MAKE_GROUP2 packet{};
    packet.PacketType = PacketProfile::ActivePartySend::kCreateParty;
    std::memcpy(packet.PartyName, partyName.c_str(), partyName.size());
    packet.ItemPickup = itemShareParty ? 1 : 0;
    packet.ItemShare = itemSharingTypeShared ? 1 : 0;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] create party name='%s' sent=%d\n", partyName.c_str(), sent ? 1 : 0);
    return sent;
}

bool RequestPartyInvite(u32 targetAid)
{
    if (targetAid == 0 || g_session.GetNumParty() == 0) {
        return false;
    }

    PACKET_CZ_REQ_JOIN_GROUP packet{};
    packet.PacketType = PacketProfile::ActivePartySend::kInviteParty;
    packet.AccountId = targetAid;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] party invite targetAid=%u sent=%d\n",
        static_cast<unsigned int>(targetAid),
        sent ? 1 : 0);
    return sent;
}

bool RequestPartyInviteByName(const std::string& name)
{
    PACKET_CZ_PARTY_JOIN_REQ packet{};
    if (g_session.GetNumParty() == 0 || name.empty() || name.size() >= sizeof(packet.CharacterName)) {
        return false;
    }

    packet.PacketType = PacketProfile::ActivePartySend::kInvitePartyByName;
    std::memcpy(packet.CharacterName, name.data(), name.size());
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

bool ReplyPartyInvite(u32 partyId, bool accept, bool extended)
{
    if (partyId == 0) {
        return false;
    }

    if (extended) {
        const PACKET_CZ_PARTY_JOIN_REQ_ACK packet{
            PacketProfile::ActivePartySend::kReplyPartyInviteExtended, partyId, static_cast<u8>(accept)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
    }

    PACKET_CZ_JOIN_GROUP packet{};
    packet.PacketType = PacketProfile::ActivePartySend::kReplyPartyInvite;
    packet.PartyId = partyId;
    packet.Flag = accept ? 1u : 0u;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] party invite reply partyId=%u accept=%d sent=%d\n",
        static_cast<unsigned int>(partyId),
        accept ? 1 : 0,
        sent ? 1 : 0);
    return sent;
}

bool RequestChangePartyOptions(bool expShare)
{
    if (g_session.GetNumParty() == 0 || !g_session.m_amIPartyMaster) {
        return false;
    }

    PACKET_CZ_CHANGE_GROUPEXPOPTION packet{};
    packet.PacketType = PacketProfile::ActivePartySend::kChangePartyOption;
    packet.ExpOption = expShare ? 1 : 0;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] change party EXP sharing exp=%u sent=%d\n",
        static_cast<unsigned int>(packet.ExpOption),
        sent ? 1 : 0);
    return sent;
}

bool RequestLeaveParty()
{
    if (g_session.GetNumParty() == 0) {
        return false;
    }

    PACKET_CZ_REQ_LEAVE_GROUP packet{};
    packet.PacketType = PacketProfile::ActivePartySend::kLeaveParty;

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] leave party sent=%d\n", sent ? 1 : 0);
    return sent;
}

bool RequestRemovePartyMember(u32 accountId, const char* characterName)
{
    if (accountId == 0 || !characterName || *characterName == '\0' || !g_session.m_amIPartyMaster) {
        return false;
    }

    PACKET_CZ_REQ_EXPEL_GROUP_MEMBER packet{};
    packet.PacketType = PacketProfile::ActivePartySend::kRemovePartyMember;
    packet.AccountId = accountId;
    const std::string normalizedName = NormalizePartyNameForPacket(characterName);
    std::memcpy(packet.CharacterName, normalizedName.c_str(), normalizedName.size());

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] remove party member aid=%u name='%s' sent=%d\n",
        static_cast<unsigned int>(accountId),
        normalizedName.c_str(),
        sent ? 1 : 0);
    return sent;
}

bool RequestDisbandParty()
{
    if (g_session.GetNumParty() == 0 || !g_session.m_amIPartyMaster) {
        return false;
    }

    bool sentAny = false;
    for (const FRIEND_INFO& member : g_session.GetPartyList()) {
        if (member.AID == 0 || member.AID == g_session.m_aid) {
            continue;
        }
        sentAny = RequestRemovePartyMember(member.AID, member.characterName.c_str()) || sentAny;
    }

    return RequestLeaveParty() || sentAny;
}

bool TryRequestNpcTalkFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_world || !mode.m_view || mode.m_canRotateView) {
        return false;
    }

    CGameActor* hoveredActor = nullptr;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        screenX,
        screenY,
        &hoveredActor,
        nullptr,
        nullptr)) {
        return false;
    }

    if (!IsNpcLikeHoverActor(hoveredActor)) {
        return false;
    }

    if (!RequestNpcContact(hoveredActor->m_gid)) {
        return false;
    }

    mode.m_lastMonGid = 0;
    mode.m_lastLockOnMonGid = 0;
    mode.m_isAutoMoveClickOn = 0;
    mode.m_isLeftButtonHeld = 0;
    mode.m_hasHeldMoveTarget = 0;
    mode.m_lastMoveRequestCellX = -1;
    mode.m_lastMoveRequestCellY = -1;
    mode.m_heldMoveTargetCellX = -1;
    mode.m_heldMoveTargetCellY = -1;
    mode.m_lastMoveRequestTick = 0;
    mode.m_lastAttackRequestTick = 0;
    ClearPickupIntent(mode);
    ClearAttackChaseHint(mode);
    ClearSkillChase(mode);
    return true;
}

void TurnPlayerTowardCaptureNpc(CGameMode& mode,const CGameActor& target)
{
    auto* player=mode.m_world->m_player;
    auto* attr=mode.m_world->m_attr;
    if(!attr || attr->m_zoom<=0)return;
    int x=0,y=0;mode.GetServerCoor(target.m_pos.x,target.m_pos.z,x,y);
    float wx=0,wz=0;mode.GetClientCoor(x,y,wx,wz);
    const float dx=wx-player->m_pos.x,dz=wz-player->m_pos.z;
    double angle=std::atan2(static_cast<double>(dx),-static_cast<double>(dz))/static_cast<double>(3.141592f)*180.0;
    if(angle>=360)angle-=360;if(angle<0)angle+=360;
    if(!std::isfinite(angle) || !std::isfinite(player->m_roty))return;
    int delta=static_cast<int>(player->m_roty-angle);if(delta<0)delta+=360;
    int sector=static_cast<int>((delta+22.5)/45.0);if(sector<0)sector+=8;if(sector>=8)sector-=8;
    switch(sector){
    case 0:case 4:player->m_roty=static_cast<float>(angle);player->m_headDir=0;break;
    case 1:case 7:{
        const int head=sector==1?1:2;
        if(player->m_headDir!=head)player->m_headDir=head;
        else{player->m_roty=static_cast<float>(angle);player->m_headDir=0;}
        break;
    }
    case 2:case 3:{double body=angle+45.0;if(body>=360)body-=360;player->m_headDir=1;player->m_roty=static_cast<float>(body);break;}
    case 5:case 6:{double body=angle-45.0;if(body<0)body+=360;player->m_headDir=2;player->m_roty=static_cast<float>(body);break;}
    default:return;
    }
    player->m_curMotion=player->m_headDir;
    const int direction=(static_cast<int>(player->m_roty)+180)/45%8;
    // Source message16 mutates local facing before the raw152 send attempt.
    mode.SendMsg(152,direction,player->m_headDir,0);
}

bool TryRequestSourceSkillOnActor(CGameMode& mode,CGameActor* target,bool shift,bool alt,bool control)
{
    if(mode.m_skillUseInfo.useType!=2 && mode.m_skillUseInfo.useType!=4)return false;
    if(!mode.m_world || !mode.m_world->m_player || !target)return false;
    auto* player=mode.m_world->m_player;const auto kind=ClassifyPetCaptureActor(target);
    const int skillId = mode.m_skillUseInfo.id;
    const bool homunculusSkill = skillId >= 8000 && skillId < 8017;
    const bool mercenarySkill = SKILL_INFO::IsMercenarySkill(skillId);
    if ((homunculusSkill || mercenarySkill)
        && kind != PetCaptureActorKind::Monster && kind != PetCaptureActorKind::Pc) {
        return true;
    }
    if(kind==PetCaptureActorKind::Npc){
        if(player->m_stateId!=0 && player->m_stateId!=6)return false;
        TurnPlayerTowardCaptureNpc(mode,*target);RequestNpcContact(target->m_gid);return true;
    }
    const bool good=SKILL_INFO::IsGoodSkillForActor(mode.m_skillUseInfo.id);
    const bool all=mode.m_skillUseInfo.useType==4;
    if (kind == PetCaptureActorKind::Monster || kind == PetCaptureActorKind::Pc) {
        const bool attackEligible = kind == PetCaptureActorKind::Monster
            ? IsMonsterAttackEligibleForMap(mode, *target, good ? shift : !shift)
            : IsPcAttackEligibleForMap(mode, *target, good ? !shift : shift)
                || g_session.m_isNoShift == 1 || mode.m_world->m_isSiegeMode;
        if (homunculusSkill || mercenarySkill) {
            if (mercenarySkill && !attackEligible) return true;
            companion_ai::Msg2Ai command;
            command.command = companion_ai::SKILL_OBJECT_CMD;
            command.skillId = skillId;
            command.skillLevel = mode.m_skillUseInfo.level;
            command.targetId = target == player ? ResolveSelfSkillTargetId() : target->m_gid;
            companion_ai::Runtime::QueueCommand(homunculusSkill ? companion_ai::Kind::Homunculus
                : companion_ai::Kind::Mercenary, command, shift);
            return true;
        }
        if (kind == PetCaptureActorKind::Monster && alt) {
            if (g_session.m_homunStatus.alive) g_session.m_homunTarget.SelectActor(target->m_gid, shift);
            if (g_session.m_merStatus.alive) g_session.m_merTarget.SelectActor(target->m_gid, shift);
        }
    }
    bool eligible=false;
    switch(kind){
    case PetCaptureActorKind::Monster:eligible=IsMonsterAttackEligibleForMap(mode,*target,good?shift:!shift) && !alt;break;
    case PetCaptureActorKind::Pc:
        eligible=((target!=player && target->m_gid!=g_session.m_aid) || all)
            && IsPcAttackEligibleForMap(mode,*target,good?!shift:shift);break;
    case PetCaptureActorKind::Homun:
        eligible=mode.m_skillUseInfo.id==231 || ((mode.m_world->m_isPKZone || mode.m_world->m_isSiegeMode)
            && target->m_gid!=g_session.m_homunGID);break;
    case PetCaptureActorKind::Companion:
        eligible=((good?!shift:shift) || target->m_pkState || mode.m_world->m_isPKZone || mode.m_world->m_isSiegeMode)
            && (target->m_gid!=g_session.m_merGID || all);break;
    default:break;
    }
    if(!eligible)return false;
    const bool ordinaryActor=kind==PetCaptureActorKind::Monster || kind==PetCaptureActorKind::Pc;
    if(ordinaryActor)mode.m_isCtrlLock=!mode.m_world->m_isSiegeMode && (control || g_session.m_isNoCtrl==1);
    else if(kind==PetCaptureActorKind::Homun)mode.m_isCtrlLock=0;
    if(ordinaryActor){mode.SendMsg(28,1,0,0);mode.m_lastMonGid=target->m_gid;mode.m_lastLockOnMonGid=target->m_gid;}
    if(mode.m_skillUseInfo.id!=10000 && ro::stats::SourceWeightPercent(g_session.m_weight,g_session.m_maxWeight)>=90){
        ro::UiMessageStrings table;table.Load();const auto text=table.Get(307,"You cannot attack while overweight.");
        g_windowMgr.SendMsg(3,0,0,0);g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(text.c_str()),255,0,0);
        return true;
    }
    // Ref CPlayer keeps the local actor GID as AID; chase/use-skill must send
    // that canonical id even if an older bootstrap still stored character GID.
    // Do not treat another actor as self merely because its id matches char GID.
    const bool selfTarget = target == player || target->m_gid == g_session.m_aid;
    const u32 chaseGid = selfTarget ? ResolveSelfSkillTargetId() : target->m_gid;
    BeginSkillChase(mode,chaseGid,mode.m_skillUseInfo.id,mode.m_skillUseInfo.level,mode.m_skillUseInfo.attackRange);
    if(ordinaryActor){mode.m_lastMonGid=chaseGid;mode.m_lastLockOnMonGid=chaseGid;}
    return true;
}

bool TryRequestPetCaptureOnActor(CGameMode& mode,CGameActor* target,bool shift,bool alt,bool control)
{
    if(mode.m_skillUseInfo.id!=10000 || mode.m_skillUseInfo.level!=0 || mode.m_skillUseInfo.useType!=2)return false;
    return TryRequestSourceSkillOnActor(mode,target,shift,alt,control);
}

bool TryRequestAttackOnActor(CGameMode& mode, CGameActor* actor, bool shift, bool alt, bool control)
{
    if (!mode.m_world || !IsMonsterLikeHoverActor(actor) || !actor->m_gid) {
        return false;
    }
    if (alt || !IsMonsterAttackEligibleForMap(mode, *actor, !shift)) {
        return true;
    }

    const bool locked = !mode.m_world->m_isSiegeMode && (control || g_session.m_isNoCtrl == 1);
    if (locked) {
        const char stopPacket[] = {0x18, 0x01};
        if (!CRagConnection::instance()->SendPacket(stopPacket, sizeof(stopPacket))) {
            return true;
        }
        mode.m_lastMonGid = 0;
        mode.m_lastLockOnMonGid = 0;
        mode.m_lastAttackRequestTick = 0;
        ClearAttackChaseHint(mode);
    }
    if (!SendAttackRequestPacket(actor->m_gid, kActionRequestContinuousAttack)) {
        return true;
    }

    mode.m_lastMonGid = actor->m_gid;
    mode.m_lastLockOnMonGid = actor->m_gid;
    mode.m_isAutoMoveClickOn = 0;
    ClearPickupIntent(mode);
    mode.m_lastMoveRequestCellX = -1;
    mode.m_lastMoveRequestCellY = -1;
    mode.m_heldMoveTargetCellX = -1;
    mode.m_heldMoveTargetCellY = -1;
    mode.m_hasHeldMoveTarget = 0;
    mode.m_lastMoveRequestTick = 0;
    mode.m_lastAttackRequestTick = 0;
    ClearAttackChaseHint(mode);
    ClearSkillChase(mode);
    return true;
}

bool TryRequestAttackFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_world || !mode.m_view || mode.m_canRotateView) {
        return false;
    }

    CGameActor* hoveredActor = nullptr;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        screenX,
        screenY,
        &hoveredActor,
        nullptr,
        nullptr)) {
        return false;
    }

    return TryRequestAttackOnActor(mode, hoveredActor, (GetKeyState(VK_SHIFT) & 0x8000) != 0,
        (GetKeyState(VK_MENU) & 0x8000) != 0, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
}

void UpdateHeldMoveTargetFromScreenPoint(CGameMode& mode, int screenX, int screenY)
{
    if (!mode.m_isLeftButtonHeld || !mode.m_view) {
        return;
    }

    int attrX = -1;
    int attrY = -1;
    if (!mode.m_view->ScreenToHoveredAttrCell(screenX, screenY, &attrX, &attrY)) {
        mode.m_hasHeldMoveTarget = 0;
        return;
    }

    if (mode.m_hasHeldMoveTarget && mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_isMoving) {
        const int deltaFromHeldX = std::abs(attrX - mode.m_heldMoveTargetCellX);
        const int deltaFromHeldY = std::abs(attrY - mode.m_heldMoveTargetCellY);
        const int deltaFromLastReqX = mode.m_lastMoveRequestCellX >= 0 ? std::abs(attrX - mode.m_lastMoveRequestCellX) : 99;
        const int deltaFromLastReqY = mode.m_lastMoveRequestCellY >= 0 ? std::abs(attrY - mode.m_lastMoveRequestCellY) : 99;

        // Ignore one-cell wobble while dragging if the player is already moving.
        // This keeps small cursor jitter from flipping the walk target back and forth.
        if ((std::max)(deltaFromHeldX, deltaFromHeldY) <= 1
            && (std::max)(deltaFromLastReqX, deltaFromLastReqY) <= 1) {
            return;
        }
    }

    mode.m_heldMoveTargetCellX = attrX;
    mode.m_heldMoveTargetCellY = attrY;
    mode.m_hasHeldMoveTarget = 1;
}

bool ShouldPreserveCurrentHeldMove(const CGameMode& mode)
{
    if (!mode.m_hasHeldMoveTarget || !mode.m_world || !mode.m_world->m_player) {
        return false;
    }

    const CPlayer* player = mode.m_world->m_player;
    if (!player->m_isMoving) {
        return false;
    }

    const int moveDestX = player->m_moveDestX;
    const int moveDestY = player->m_moveDestY;
    if (moveDestX < 0 || moveDestY < 0) {
        return false;
    }

    const int heldDeltaX = mode.m_heldMoveTargetCellX - moveDestX;
    const int heldDeltaY = mode.m_heldMoveTargetCellY - moveDestY;
    if ((std::max)(std::abs(heldDeltaX), std::abs(heldDeltaY)) <= kHeldMoveRetargetThresholdCells) {
        return true;
    }

    const int currentDeltaX = moveDestX - player->m_moveSrcX;
    const int currentDeltaY = moveDestY - player->m_moveSrcY;
    if (currentDeltaX == 0 && currentDeltaY == 0) {
        return false;
    }

    const u32 now = g_session.GetServerTime();
    if (player->m_moveEndTime > now
        && player->m_moveEndTime - now <= GetHeldMoveDirectionalRetargetNearEndMs(*player)) {
        return false;
    }

    const int currentSignX = ApproachSign(currentDeltaX);
    const int currentSignY = ApproachSign(currentDeltaY);
    const int heldSignX = ApproachSign(heldDeltaX);
    const int heldSignY = ApproachSign(heldDeltaY);

    if ((std::max)(std::abs(heldDeltaX), std::abs(heldDeltaY)) > GetHeldMoveDirectionalExtensionCells(*player)
        && (heldSignX != currentSignX && heldSignX != 0 && currentSignX != 0)
        && (heldSignY != currentSignY && heldSignY != 0 && currentSignY != 0)) {
        return false;
    }

    const bool compatibleHeadingX = heldSignX == 0 || currentSignX == 0 || heldSignX == currentSignX;
    const bool compatibleHeadingY = heldSignY == 0 || currentSignY == 0 || heldSignY == currentSignY;
    return compatibleHeadingX && compatibleHeadingY;
}

void PumpHeldMoveRequest(CGameMode& mode)
{
    if (!mode.m_isLeftButtonHeld || !mode.m_hasHeldMoveTarget) {
        return;
    }

    const u32 now = GetTickCount();
    if (LocalPlayerHasPendingMoveAck(mode, now)) {
        return;
    }

    if (mode.m_lastMoveRequestTick != 0 && now - mode.m_lastMoveRequestTick < kHeldMoveRequestIntervalMs) {
        return;
    }

    if (ShouldPreserveCurrentHeldMove(mode)) {
        return;
    }

    if (TryRequestMoveToCell(mode, mode.m_heldMoveTargetCellX, mode.m_heldMoveTargetCellY)) {
        mode.m_lastMoveRequestTick = now;
    }
}

void PumpSkillChaseRequest(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    CPlayer* player = mode.m_world->m_player;
    if (player->m_proceedType.type==4) {
        mode.SynchronizePlayerSkillProceed(player);
        const int skillRange = mode.m_groundSkillRange;
        const int rangeDistance=static_cast<s32>(static_cast<u32>(skillRange)*5u);
        C3dAttr* attr = mode.m_world->m_attr;
        if (!attr || attr->m_zoom <= 0
            || attr->m_width <= 0 || attr->m_height <= 0
            || attr->m_cells.size() < static_cast<size_t>(attr->m_width) * static_cast<size_t>(attr->m_height)) {
            return;
        }

        int sourceTileX = -1;
        int sourceTileY = -1;
        mode.GetServerCoor(player->m_pos.x, player->m_pos.z, sourceTileX, sourceTileY);

        float targetWorldX = 0.0f;
        float targetWorldZ = 0.0f;
        mode.GetClientCoor(mode.m_groundSkillCellX, mode.m_groundSkillCellY, targetWorldX, targetWorldZ);

        // Ref ConvertToNumCellCloseAndFindNoRedCell (Ref/GameMode.cpp:8533,
        // EXE VA 0x5709E0), with the selected server cell converted to its
        // client-space center before projecting the range approach.
        const float deltaX = player->m_pos.x - targetWorldX;
        const float deltaZ = player->m_pos.z - targetWorldZ;
        const float lengthSquared = deltaX * deltaX + deltaZ * deltaZ;
        int approachTileX = sourceTileX;
        int approachTileY = sourceTileY;
        if (lengthSquared > 0.0f) {
            const double inverseLength = 1.0 / std::sqrt(static_cast<double>(lengthSquared));
            const float normalizedDirectionX = static_cast<float>(inverseLength * deltaX);
            const float projectedOffsetX = static_cast<float>(static_cast<double>(rangeDistance) * normalizedDirectionX);
            const float projectedOffsetZ = static_cast<float>(static_cast<double>(rangeDistance) * (inverseLength * deltaZ));
            const float approachWorldX = projectedOffsetX + targetWorldX;
            const float approachWorldZ = projectedOffsetZ + targetWorldZ;
            const float zoom = static_cast<float>(attr->m_zoom);
            approachTileX = static_cast<int>(std::floor(static_cast<double>(approachWorldX / zoom)
                + static_cast<double>(attr->m_width / 2)));
            approachTileY = static_cast<int>(std::floor(static_cast<double>(approachWorldZ / zoom)
                + static_cast<double>(attr->m_height / 2)));
        }

        constexpr std::array<std::array<int, 2>, 8> kGroundRefNeighborOrder{{
            {{1, 0}}, {{1, 1}}, {{1, -1}}, {{-1, 0}},
            {{-1, 1}}, {{-1, -1}}, {{0, 1}}, {{0, -1}},
        }};
        if (!mode.m_world->IsMapCellWalkable(approachTileX, approachTileY)) {
            const int projectedX = approachTileX;
            const int projectedY = approachTileY;
            for (const auto& offset : kGroundRefNeighborOrder) {
                approachTileX = projectedX + offset[0];
                approachTileY = projectedY + offset[1];
                if (mode.m_world->IsMapCellWalkable(approachTileX, approachTileY)) {
                    break;
                }
            }
        }

        // Ref allows states below 2, state 4, and states 16 or above.
        if (!(static_cast<u32>(player->m_stateId)<2 || player->m_stateId==4 || player->m_stateId>=16)) {
            return;
        }

        const float dx = player->m_pos.x - targetWorldX;
        const float dz = player->m_pos.z - targetWorldZ;
        const float dy = 0.0f; // Ref calls CalcDist with the player's own Y for the ground target.
        const float horizontalDistanceSquared = dx * dx + dz * dz;
        const double horizontalDistance = std::sqrt(static_cast<double>(horizontalDistanceSquared));
        const float distanceSquared = static_cast<float>(horizontalDistance * horizontalDistance + dy * dy);
        const double distance = std::sqrt(static_cast<double>(distanceSquared));
        const bool destinationIsPlayerCell = player->m_destCellX == sourceTileX
            && player->m_destCellZ == sourceTileY;
        if (distance <= static_cast<double>(rangeDistance) + 5.5 || destinationIsPlayerCell) {
            player->SendMsg(nullptr,66,0,0,0);
            player->SendMsg(nullptr,13,0,0,0);
            return;
        }

        if (player->m_destCellX == approachTileX && player->m_destCellZ == approachTileY) {
            return;
        }
        if (player->m_stateId == 2) {
            return;
        }

        player->m_destCellX = approachTileX;
        player->m_destCellZ = approachTileY;
        if (!player->m_isWaitingMoveAck || timeGetTime() > player->m_moveReqTick + 1000) {
            player->m_isWaitingMoveAck = 1;
            player->m_moveReqTick = timeGetTime();
            SendMoveRequestPacket(approachTileX, approachTileY);
        }
        return;
    }

    if (mode.m_hasTouchSkillChase) {
        const u32 targetAid = mode.m_touchSkillTargetAid;
        CSkill* target = targetAid != 0 ? mode.m_world->GetSkillByAID(targetAid) : nullptr;
        if (!target) {
            // Missing source target resolves to message 13 (clear proceed).
            player->SendMsg(nullptr,13,0,0,0);
            return;
        }

        const int skillId = mode.m_touchSkillId;
        const int skillRange = mode.m_touchSkillRange;
        C3dAttr* attr = mode.m_world->m_attr;
        if (!attr || attr->m_zoom <= 0 || attr->m_width <= 0 || attr->m_height <= 0
            || attr->m_cells.size() < static_cast<size_t>(attr->m_width) * static_cast<size_t>(attr->m_height)) {
            return;
        }

        int sourceTileX = -1;
        int sourceTileY = -1;
        mode.GetServerCoor(player->m_pos.x, player->m_pos.z, sourceTileX, sourceTileY);
        int skillTileX = -1;
        int skillTileY = -1;
        mode.GetServerCoor(target->m_pos.x, target->m_pos.z, skillTileX, skillTileY);
        float targetWorldX = 0.0f;
        float targetWorldZ = 0.0f;
        mode.GetClientCoor(skillTileX, skillTileY, targetWorldX, targetWorldZ);

        // Ref PT_TOUCH_SKILL passes the skill unit's server-cell center to the
        // same closest-safe-cell helper used for targeted actor skills.
        const float deltaX = player->m_pos.x - targetWorldX;
        const float deltaZ = player->m_pos.z - targetWorldZ;
        const float lengthSquared = deltaX * deltaX + deltaZ * deltaZ;
        int approachTileX = sourceTileX;
        int approachTileY = sourceTileY;
        if (lengthSquared > 0.0f) {
            const double inverseLength = 1.0 / std::sqrt(static_cast<double>(lengthSquared));
            const float normalizedDirectionX = static_cast<float>(inverseLength * deltaX);
            const float projectedOffsetX = static_cast<float>(static_cast<double>(5 * skillRange) * normalizedDirectionX);
            const float projectedOffsetZ = static_cast<float>(static_cast<double>(5 * skillRange) * (inverseLength * deltaZ));
            const float approachWorldX = projectedOffsetX + targetWorldX;
            const float approachWorldZ = projectedOffsetZ + targetWorldZ;
            const float zoom = static_cast<float>(attr->m_zoom);
            approachTileX = static_cast<int>(std::floor(static_cast<double>(approachWorldX / zoom)
                + static_cast<double>(attr->m_width / 2)));
            approachTileY = static_cast<int>(std::floor(static_cast<double>(approachWorldZ / zoom)
                + static_cast<double>(attr->m_height / 2)));
        }

        constexpr std::array<std::array<int, 2>, 8> kTouchRefNeighborOrder{{
            {{1, 0}}, {{1, 1}}, {{1, -1}}, {{-1, 0}},
            {{-1, 1}}, {{-1, -1}}, {{0, 1}}, {{0, -1}},
        }};
        if (!mode.m_world->IsMapCellWalkable(approachTileX, approachTileY)) {
            const int projectedX = approachTileX;
            const int projectedY = approachTileY;
            for (const auto& offset : kTouchRefNeighborOrder) {
                approachTileX = projectedX + offset[0];
                approachTileY = projectedY + offset[1];
                if (mode.m_world->IsMapCellWalkable(approachTileX, approachTileY)) {
                    break;
                }
            }
        }

        if (static_cast<u32>(player->m_stateId) >= 2 && player->m_stateId != 4 && player->m_stateId < 16) {
            return;
        }

        const float dx = player->m_pos.x - targetWorldX;
        const float dz = player->m_pos.z - targetWorldZ;
        const float dy = 0.0f;
        const float horizontalDistanceSquared = dx * dx + dz * dz;
        const double horizontalDistance = std::sqrt(static_cast<double>(horizontalDistanceSquared));
        const float distanceSquared = static_cast<float>(horizontalDistance * horizontalDistance + dy * dy);
        const double distance = std::sqrt(static_cast<double>(distanceSquared));
        const bool destinationIsPlayerCell = player->m_destCellX == sourceTileX
            && player->m_destCellZ == sourceTileY;
        if (distance <= static_cast<double>(5 * skillRange) + 5.5 || destinationIsPlayerCell) {
            const bool specialSkillUnitJob = target->m_job == 141 || target->m_job == 143 || target->m_job == 152;
            if (specialSkillUnitJob && skillId == 0) {
                // Source29 invokes virtual mode150 and retains PT_TOUCH_SKILL.
                player->SendMsg(nullptr,29,0,0,0);
                return;
            } else {
                // Ref message 42 maps to mode message 76 and actor-skill use.
                player->SendMsg(nullptr,42,0,0,0);
            }
            player->SendMsg(nullptr,13,0,0,0);
            return;
        }

        if (player->m_destCellX == approachTileX && player->m_destCellZ == approachTileY) {
            return;
        }
        const bool specialSkillUnitJob = target->m_job == 141 || target->m_job == 143 || target->m_job == 152;
        if (player->m_stateId == 2 && !specialSkillUnitJob) {
            return;
        }

        player->m_destCellX = approachTileX;
        player->m_destCellZ = approachTileY;
        if (!player->m_isWaitingMoveAck || timeGetTime() > player->m_moveReqTick + 1000) {
            player->m_isWaitingMoveAck = 1;
            player->m_moveReqTick = timeGetTime();
            SendMoveRequestPacket(approachTileX, approachTileY);
        }
        return;
    }

    if (!mode.m_hasSkillChase || mode.m_skillChaseTargetGid == 0) {
        return;
    }

    CGameActor* target = nullptr;
    if (mode.m_skillChaseTargetGid == g_session.m_gid
        || mode.m_skillChaseTargetGid == g_session.m_aid) {
        target = player;
    } else {
        const auto targetIt = mode.m_runtimeActors.find(mode.m_skillChaseTargetGid);
        if (targetIt != mode.m_runtimeActors.end()) {
            target = targetIt->second;
        }
    }
    if (!target) {
        ClearSkillChase(mode);
        return;
    }

    const int skillRange = mode.m_skillChaseRange;
    C3dAttr* attr = mode.m_world->m_attr;
    if (!attr || attr->m_zoom <= 0 || attr->m_width <= 0 || attr->m_height <= 0
        || attr->m_cells.size() < static_cast<size_t>(attr->m_width) * static_cast<size_t>(attr->m_height)) {
        return;
    }

    int sourceTileX = -1;
    int sourceTileY = -1;
    mode.GetServerCoor(player->m_pos.x, player->m_pos.z, sourceTileX, sourceTileY);

    // Ref ConvertToNumCellCloseAndFindNoRedCell (Ref/GameMode.cpp:8533,
    // EXE VA 0x5709E0): project 5*range world units from the target toward
    // the player, floor to the attr cell, then scan its fixed neighbor order
    // only when the projected cell is red or outside the map.
    const float deltaX = player->m_pos.x - target->m_pos.x;
    const float deltaZ = player->m_pos.z - target->m_pos.z;
    const float lengthSquared = deltaX * deltaX + deltaZ * deltaZ;
    // The source helper divides by sqrt(lengthSquared); a coincident target
    // makes its cell projection undefined. Keep the current player cell as a
    // safe placeholder; the source distance branch below still dispatches an
    // in-range skill for a valid nonnegative range.
    int chaseTileX = sourceTileX;
    int chaseTileY = sourceTileY;
    if (lengthSquared > 0.0f) {
        const double inverseLength = 1.0 / std::sqrt(static_cast<double>(lengthSquared));
        const float normalizedDirectionX = static_cast<float>(inverseLength * deltaX);
        const float projectedOffsetX = static_cast<float>(static_cast<double>(5 * skillRange) * normalizedDirectionX);
        const float projectedOffsetZ = static_cast<float>(static_cast<double>(5 * skillRange) * (inverseLength * deltaZ));
        const float approachWorldX = projectedOffsetX + target->m_pos.x;
        const float approachWorldZ = projectedOffsetZ + target->m_pos.z;
        const float zoom = static_cast<float>(attr->m_zoom);
        chaseTileX = static_cast<int>(std::floor(static_cast<double>(approachWorldX / zoom)
            + static_cast<double>(attr->m_width / 2)));
        chaseTileY = static_cast<int>(std::floor(static_cast<double>(approachWorldZ / zoom)
            + static_cast<double>(attr->m_height / 2)));
    }

    constexpr std::array<std::array<int, 2>, 8> kRefNeighborOrder{{
        {{1, 0}}, {{1, 1}}, {{1, -1}}, {{-1, 0}},
        {{-1, 1}}, {{-1, -1}}, {{0, 1}}, {{0, -1}},
    }};
    if (!mode.m_world->IsMapCellWalkable(chaseTileX, chaseTileY)) {
        const int projectedX = chaseTileX;
        const int projectedY = chaseTileY;
        for (const auto& offset : kRefNeighborOrder) {
            chaseTileX = projectedX + offset[0];
            chaseTileY = projectedY + offset[1];
            if (mode.m_world->IsMapCellWalkable(chaseTileX, chaseTileY)) {
                break;
            }
        }
    }

    // Ref evaluates the state gate after calculating the approach cell.
    if (player->m_stateId >= 2 && player->m_stateId != 4 && player->m_stateId < 16) {
        return;
    }

    const float dx = player->m_pos.x - target->m_pos.x;
    const float dz = player->m_pos.z - target->m_pos.z;
    const float dy = player->m_pos.y - target->m_pos.y;
    const float horizontalDistanceSquared = dx * dx + dz * dz;
    const double horizontalDistance = std::sqrt(static_cast<double>(horizontalDistanceSquared));
    const float distanceSquared = static_cast<float>(horizontalDistance * horizontalDistance + dy * dy);
    double distance = std::sqrt(static_cast<double>(distanceSquared));
    if (target->m_actorType == 1) {
        distance -= 20.0f;
        if (distance < 0.0f) {
            distance = 0.0f;
        }
    }

    const bool destinationIsPlayerCell = player->m_destCellX == sourceTileX
        && player->m_destCellZ == sourceTileY;
    if (distance <= static_cast<double>(5 * skillRange) + 5.5 || destinationIsPlayerCell) {
        // Ref message 42 dispatches mode message 76, then message 13 clears
        // the proceed state even when the packet send cannot be completed.
        player->SendMsg(nullptr,42,0,0,0);
        player->SendMsg(nullptr,13,0,0,0);
        ClearSkillChase(mode);
        return;
    }

    if (player->m_destCellX == chaseTileX && player->m_destCellZ == chaseTileY) {
        return;
    }
    if (player->m_stateId == 2) {
        return;
    }

    // Ref stores the requested approach cell before message 91. CPlayer::SendMsg
    // then applies its strict one-second resend gate before message 151.
    player->m_destCellX = chaseTileX;
    player->m_destCellZ = chaseTileY;

    int moveTileX = chaseTileX;
    int moveTileY = chaseTileY;
    if (target->m_actorType == 1) {
        long double angle = std::atan2(static_cast<long double>(target->m_pos.x - player->m_pos.x),
            -static_cast<long double>(target->m_pos.z - player->m_pos.z)) / 3.141592 * 180.0;
        if (angle >= 360.0) {
            angle -= 360.0;
        }
        if (angle < 0.0) {
            angle += 360.0;
        }

        if (angle < 25.0) {
            moveTileY += 5;
        } else if (angle < 70.0) {
            moveTileX += 5;
            moveTileY += 5;
        } else if (angle < 115.0) {
            moveTileX += 5;
        } else if (angle < 160.0) {
            moveTileX += 5;
            moveTileY -= 5;
        } else if (angle < 205.0) {
            moveTileY -= 5;
        } else if (angle < 250.0) {
            moveTileX -= 5;
            moveTileY += 5;
        } else if (angle < 295.0) {
            moveTileX -= 5;
        } else if (angle < 340.0) {
            moveTileX -= 5;
            moveTileY -= 5;
        } else {
            moveTileY += 5;
        }
    }

    if (!player->m_isWaitingMoveAck || timeGetTime() > player->m_moveReqTick + 1000) {
        player->m_isWaitingMoveAck = 1;
        player->m_moveReqTick = timeGetTime();
        SendMoveRequestPacket(moveTileX, moveTileY);
    }
}

void PumpAttackChaseRequest(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player || mode.m_lastLockOnMonGid == 0) {
        return;
    }

    if (mode.m_hasSkillChase || HasPendingSkillUse(mode)) {
        return;
    }

    if (mode.m_isLeftButtonHeld || mode.m_hasHeldMoveTarget) {
        return;
    }

    const auto targetIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
    if (targetIt == mode.m_runtimeActors.end() || !targetIt->second) {
        mode.m_lastLockOnMonGid = 0;
        mode.m_lastAttackRequestTick = 0;
        ClearAttackChaseHint(mode);
        return;
    }

    CGameActor* target = targetIt->second;
    if (!target->m_isVisible) {
        return;
    }

    const u32 now = GetTickCount();
    const int attackRange = (std::max)(1, mode.m_attackChaseRange);
    if (IsAttackTargetWithinRange(mode, *target, attackRange)) {
        if (mode.m_lastAttackRequestTick != 0 && now - mode.m_lastAttackRequestTick < kAttackRetryIntervalMs) {
            return;
        }

        if (SendAttackRequestPacket(target->m_gid, kActionRequestContinuousAttack)) {
            mode.m_lastAttackRequestTick = now;
        }
        return;
    }

    // If the server-authoritative move destination is already in attack range,
    // keep that path instead of sending a new chase cell that can bounce us back.
    if (IsCurrentMoveDestinationWithinRange(mode, *target, attackRange)) {
        return;
    }

    if (mode.m_lastAttackRequestTick == 0 || now - mode.m_lastAttackRequestTick >= kAttackRetryIntervalMs) {
        if (SendAttackRequestPacket(target->m_gid, kActionRequestContinuousAttack)) {
            mode.m_lastAttackRequestTick = now;
        }
    }

    if (!mode.m_hasAttackChaseHint || mode.m_attackChaseTargetGid != target->m_gid) {
        return;
    }

    int sourceTileX = -1;
    int sourceTileY = -1;
    if (!ResolveAttackChaseSourceTile(mode, &sourceTileX, &sourceTileY)) {
        return;
    }

    int targetTileX = -1;
    int targetTileY = -1;
    if (!WorldToAttrCell(mode.m_world, target->m_pos.x, target->m_pos.z, &targetTileX, &targetTileY)) {
        return;
    }

    int chaseTileX = -1;
    int chaseTileY = -1;
    if (!ResolveAttackChaseCell(mode,
        sourceTileX,
        sourceTileY,
        targetTileX,
        targetTileY,
        attackRange,
        target->m_gid,
        &chaseTileX,
        &chaseTileY)) {
        return;
    }

    if (mode.m_lastMoveRequestTick != 0 && now - mode.m_lastMoveRequestTick < kAttackChaseRequestIntervalMs) {
        return;
    }

    if (TryRequestMoveToCell(mode,
        sourceTileX,
        sourceTileY,
        chaseTileX,
        chaseTileY,
        true)) {
        mode.m_lastMoveRequestTick = now;
    }
}

void PumpPendingPickupRequest(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player || mode.m_pickupReqItemNaidList.empty()) {
        return;
    }

    const u32 objectAid = mode.m_pickupReqItemNaidList.front();
    const auto groundItemIt = mode.m_groundItemList.find(objectAid);
    if (groundItemIt == mode.m_groundItemList.end()) {
        ClearPickupIntent(mode);
        return;
    }

    const GroundItemState& groundItem = groundItemIt->second;
    const u32 now = GetTickCount();
    if (LocalPlayerHasPendingMoveAck(mode, now)) {
        return;
    }

    if (IsGroundItemWithinPickupRange(mode, groundItem)) {
        if (mode.m_lastPickupRequestTick != 0 && now - mode.m_lastPickupRequestTick < kPickupRetryIntervalMs) {
            return;
        }

        if (SendTakeItemRequestPacket(objectAid)) {
            mode.m_lastPickupRequestTick = now;
        }
        return;
    }

    if (IsCurrentMoveDestinationWithinPickupRange(mode, groundItem)) {
        return;
    }

    if (mode.m_lastMoveRequestTick != 0 && now - mode.m_lastMoveRequestTick < kAttackChaseRequestIntervalMs) {
        return;
    }

    int sourceTileX = -1;
    int sourceTileY = -1;
    if (!ResolveAttackChaseSourceTile(mode, &sourceTileX, &sourceTileY)) {
        return;
    }

    int chaseTileX = -1;
    int chaseTileY = -1;
    if (!ResolveGroundItemPickupCell(mode,
        sourceTileX,
        sourceTileY,
        groundItem,
        &chaseTileX,
        &chaseTileY)) {
        return;
    }

    if (TryRequestMoveToCell(mode,
        sourceTileX,
        sourceTileY,
        chaseTileX,
        chaseTileY,
        true)) {
        mode.m_lastMoveRequestTick = now;
    }
}

void DestroyDeferredRuntimeActors(CGameMode& mode);

void ClearRuntimeActors(CGameMode& mode)
{
    mode.m_pvpRankStartTick = 0;
    mode.m_auctionSearch = {};
    mode.m_auctionStaging = {};
    mode.m_auctionTransaction = CGameMode::AuctionTransaction::None;
    mode.m_recallPartnerName.clear();
    mode.ResetBattlegroundMapState();
    mode.ResetCompanionMapState();
    g_Weather.Reset();
    mode.ClearNpcProgress();
    mode.m_compassPosList.clear();
    mode.m_characterNameCache.clear();
    mode.m_characterNameReqTimer.clear();
    CWorld* const world = mode.m_world ? mode.m_world : &g_world;
    if (world) {
        world->m_player = nullptr;
    }

    ClearQueuedMsgEffects();

    for (auto& entry : mode.m_runtimeActors) {
        CGameActor* actor = entry.second;
        if (!actor) {
            continue;
        }
        if (world) {
            world->NotifyActorDeleted(actor);
            world->UnregisterActor(actor);
        }
        actor->UnRegisterPos();
        delete actor;
    }
    mode.m_runtimeActors.clear();
    mode.m_actorPosList.clear();
    DestroyDeferredRuntimeActors(mode);

    if (world) {
        world->m_actorList.clear();
        world->ClearFixedObjects();
        world->RebuildSceneGraph();
        world->InvalidateBillboardFrameCache();
    }
}

void DestroyDeferredRuntimeActors(CGameMode& mode)
{
    for (CGameActor* actor : mode.m_deferredActorDeletes) {
        delete actor;
    }
    mode.m_deferredActorDeletes.clear();
}

void EnsureBootstrapSelfActor(CGameMode& mode)
{
    // Ref CPlayer::OnInit assigns m_gid = GetAid(). Account AID is the map
    // identity used for pick/self-skill targeting; character GID alone is not.
    const u32 selfActorId = g_session.m_aid != 0 ? g_session.m_aid
        : (g_session.m_gid != 0 ? g_session.m_gid : 0u);
    if (selfActorId == 0) {
        return;
    }

    CGameActor* actor = nullptr;
    bool createdActor = false;
    auto it = mode.m_runtimeActors.find(selfActorId);
    if (it == mode.m_runtimeActors.end() && g_session.m_gid != 0 && g_session.m_gid != selfActorId) {
        it = mode.m_runtimeActors.find(g_session.m_gid);
    }
    if (it == mode.m_runtimeActors.end() && g_session.m_aid != 0 && g_session.m_aid != selfActorId) {
        it = mode.m_runtimeActors.find(g_session.m_aid);
    }
    if (it != mode.m_runtimeActors.end()) {
        actor = it->second;
        if (actor && actor->m_gid != selfActorId) {
            if (it->first != selfActorId) {
                mode.m_runtimeActors.erase(it);
                mode.m_runtimeActors[selfActorId] = actor;
            }
            actor->m_gid = selfActorId;
        }
    } else {
        actor = new CPlayer();
        if (!actor) {
            return;
        }

        actor->m_gid = selfActorId;
        actor->m_isVisible = 1;
        actor->m_stateId = 0;
        actor->m_oldstateId = 0;
        actor->m_isLieOnGround = 0;
        actor->m_isMotionFinished = 0;
        actor->m_isMotionFreezed = 0;
        actor->m_stateStartTick = GetTickCount();
        actor->m_motionType = 0;
        actor->m_curAction = 0;
        actor->m_baseAction = 0;
        actor->m_curMotion = 0;
        actor->m_oldBaseAction = 0;
        actor->m_oldMotion = 0;
        actor->m_forceAct = 0;
        actor->m_forceMot = 0;
        actor->m_forceMaxMot = 0;
        actor->m_forceAnimSpeed = 0;
        actor->m_forceFinishedAct = 0;
        actor->m_forceFinishedMot = 0;
        actor->m_forceStartMot = 0;
        actor->m_isForceState = 0;
        actor->m_isForceAnimLoop = 0;
        actor->m_isForceAnimation = 0;
        actor->m_isForceAnimFinish = 0;
        actor->m_isForceState2 = 0;
        actor->m_isForceState3 = 0;
        actor->m_forceStateCnt = 0;
        actor->m_forceStateEndTick = 0;
        actor->m_isPc = 1;
        actor->m_objectType = 0;
        actor->m_actorType = 0;
        actor->m_bodyState = 0;
        actor->m_healthState = 0;
        actor->m_effectState = 0;
        actor->m_pkState = 0;
        actor->m_bodyPalette = 0;
        actor->m_birdEffect = nullptr;
        actor->m_sex = g_session.GetSex();
        actor->m_roty = static_cast<float>(45 * ((g_session.m_playerDir & 7) + 4));
        actor->m_speed = 150;
        actor->m_moveSrcX = g_session.m_playerPosX;
        actor->m_moveSrcY = g_session.m_playerPosY;
        actor->m_moveDestX = g_session.m_playerPosX;
        actor->m_moveDestY = g_session.m_playerPosY;
        actor->m_moveStartTime = 0;
        actor->m_moveEndTime = 0;
        actor->m_isMoving = 0;
        actor->m_dist = 0.0f;
        actor->m_Hp = 0;
        actor->m_MaxHp = 0;
        actor->m_Sp = 0;
        actor->m_MaxSp = 0;
        actor->m_targetGid = 0;
        actor->m_willBeDead = 0;
        actor->m_vanishTime = 0;
        actor->m_motionSpeed = 1.0f;
        actor->m_modifyFactorOfmotionSpeed = 1.0f;
        actor->m_modifyFactorOfmotionSpeed2 = 1.0f;
        actor->m_attackMotion = -1.0f;
        actor->m_sprRes = nullptr;
        actor->m_actRes = nullptr;
        actor->m_pos = vector3d{ 0.0f, 0.0f, 0.0f };
        actor->m_moveStartPos = actor->m_pos;
        actor->m_moveEndPos = actor->m_pos;
        actor->m_path.Reset();
        actor->m_msgEffectList.clear();
        mode.m_runtimeActors.emplace(selfActorId, actor);
        createdActor = true;

        DbgLog("[GameMode] bootstrap self actor gid=%u aid=%u charGid=%u map='%s' pos=%d,%d dir=%d\n",
            selfActorId,
            g_session.m_aid,
            g_session.m_gid,
            g_session.m_curMap,
            g_session.m_playerPosX,
            g_session.m_playerPosY,
            g_session.m_playerDir);
    }

    if (mode.m_world) {
        mode.m_world->m_player = static_cast<CPlayer*>(actor);
    }

    if(g_session.m_localGuild.characterId==g_session.m_gid) {
        actor->m_gdid=static_cast<int>(g_session.m_localGuild.guildId);
        actor->m_emblemVersion=static_cast<int>(g_session.m_localGuild.emblemVersion);
    }

    if (CPc* pc = dynamic_cast<CPc*>(actor)) {
        pc->m_job = g_session.m_playerJob;
        pc->m_sex = g_session.GetSex();
        pc->m_head = g_session.m_playerHead;
        pc->m_headPalette = g_session.m_playerHeadPalette;
        pc->m_bodyPalette = g_session.m_playerBodyPalette;
        pc->m_weapon = g_session.m_playerWeapon;
        pc->m_shield = g_session.m_playerShield;
        pc->m_accessory = g_session.m_playerAccessory;
        pc->m_accessory2 = g_session.m_playerAccessory2;
        pc->m_accessory3 = g_session.m_playerAccessory3;
    }

    if (const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo()) {
        actor->m_clevel = static_cast<u16>((std::max)(0, (std::min)(static_cast<int>(info->level), 0xFFFF)));
        actor->m_Hp = g_session.GetCharacterVitals().hp;
        actor->m_MaxHp = g_session.GetCharacterVitals().maxHp;
        actor->m_Sp = g_session.GetCharacterVitals().sp;
        actor->m_MaxSp = g_session.GetCharacterVitals().maxSp;

        if (createdActor) {
            actor->m_bodyState = info->bodystate;
            actor->m_healthState = info->healthstate;
            actor->m_effectState = info->effectstate;
            PrimeLocalCartStateFromEffectState(info->effectstate);
            constexpr int kFalconOption = 0x0010;
            constexpr int kFalconSuppressedState = 0x0002 | 0x0004 | 0x0040;
            if ((info->effectstate & kFalconOption) != 0
                && (info->effectstate & kFalconSuppressedState) == 0) {
                actor->SendMsg(actor, 101, 0, 0, 0);
            }
            DbgLog("[GameMode] bootstrap self selected-char states gid=%u body=%d health=%d effect=0x%X\n",
                g_session.m_gid,
                info->bodystate,
                info->healthstate,
                info->effectstate);
        }

        // Preserve independent server values while status packets arrive.
    }

    actor->m_moveDestX = g_session.m_playerPosX;
    actor->m_moveDestY = g_session.m_playerPosY;
    mode.m_actorPosList[g_session.m_gid] = CellPos{ g_session.m_playerPosX, g_session.m_playerPosY };
    mode.m_lastPcGid = g_session.m_gid;

    ApplyPendingLocalCartState(mode);
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

bool WorldToAttrCell(const CWorld* world, float worldX, float worldZ, int* outTileX, int* outTileY)
{
    if (!world || !world->m_attr || !outTileX || !outTileY || world->m_attr->m_zoom <= 0) {
        return false;
    }

    const float zoom = static_cast<float>(world->m_attr->m_zoom);
    const float localX = static_cast<float>(world->m_attr->m_width) * zoom * 0.5f + worldX;
    const float localZ = static_cast<float>(world->m_attr->m_height) * zoom * 0.5f + worldZ;
    const int tileX = static_cast<int>(localX / zoom);
    const int tileY = static_cast<int>(localZ / zoom);
    if (tileX < 0 || tileX >= world->m_attr->m_width || tileY < 0 || tileY >= world->m_attr->m_height) {
        return false;
    }

    *outTileX = tileX;
    *outTileY = tileY;
    return true;
}

float NormalizeGroundItemSubOffset(u8 value)
{
    if (value <= 5) {
        return (static_cast<float>(value) - 2.0f) / 5.0f;
    }

    return static_cast<float>(value) / 255.0f - 0.5f;
}

void SyncGroundItemsToWorld(CGameMode& mode)
{
    if (!mode.m_world) {
        return;
    }

    auto existingIt = mode.m_world->m_itemList.begin();
    while (existingIt != mode.m_world->m_itemList.end()) {
        CItem* item = *existingIt;
        if (!item || mode.m_groundItemList.find(item ? item->m_aid : 0) == mode.m_groundItemList.end()) {
            delete item;
            existingIt = mode.m_world->m_itemList.erase(existingIt);
            continue;
        }
        ++existingIt;
    }

    const float zoom = mode.m_world->m_attr
        ? static_cast<float>(mode.m_world->m_attr->m_zoom)
        : (mode.m_world->m_ground ? mode.m_world->m_ground->m_zoom : 5.0f);

    for (auto& entry : mode.m_groundItemList) {
        GroundItemState& state = entry.second;
        CItem* liveItem = nullptr;
        for (CItem* candidate : mode.m_world->m_itemList) {
            if (candidate && candidate->m_aid == state.objectId) {
                liveItem = candidate;
                break;
            }
        }

        if (!liveItem) {
            liveItem = new CItem();
            if (!liveItem) {
                continue;
            }
            liveItem->m_aid = state.objectId;
            mode.m_world->m_itemList.push_back(liveItem);
        }

        const bool resourceChanged = liveItem->m_itemId != state.itemId
            || liveItem->m_identified != state.identified;
        liveItem->m_itemId = state.itemId;
        liveItem->m_amount = state.amount;
        liveItem->m_identified = state.identified;
        liveItem->m_tileX = state.tileX;
        liveItem->m_tileY = state.tileY;
        liveItem->m_subX = state.subX;
        liveItem->m_subY = state.subY;
        liveItem->m_itemName = g_ttemmgr.GetDisplayName(state.itemId, state.identified != 0);
        liveItem->m_resourceName = g_ttemmgr.GetResourceName(state.itemId, state.identified != 0);
        liveItem->m_pos.x = TileToWorldCoordX(mode.m_world, state.tileX)
            + NormalizeGroundItemSubOffset(state.subX) * zoom * 0.35f;
        liveItem->m_pos.z = TileToWorldCoordZ(mode.m_world, state.tileY)
            + NormalizeGroundItemSubOffset(state.subY) * zoom * 0.35f;
        liveItem->m_pos.y = mode.m_world->m_attr
            ? mode.m_world->m_attr->GetHeight(liveItem->m_pos.x, liveItem->m_pos.z)
            : 0.0f;
        liveItem->m_isVisible = 1;

        if (resourceChanged) {
            liveItem->InvalidateBillboard();
        }
        if (state.pendingDropAnimation) {
            liveItem->TriggerDropAnimation();
            state.pendingDropAnimation = 0;
        }
    }
}

void SyncBootstrapSelfActorWorldPos(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player) {
        return;
    }

    CPlayer* player = mode.m_world->m_player;
    if (player->m_isMoving) {
        return;
    }

    player->m_moveDestX = g_session.m_playerPosX;
    player->m_moveDestY = g_session.m_playerPosY;
    player->m_pos.x = TileToWorldCoordX(mode.m_world, g_session.m_playerPosX);
    player->m_pos.z = TileToWorldCoordZ(mode.m_world, g_session.m_playerPosY);
    player->m_pos.y = mode.m_world->m_attr ? mode.m_world->m_attr->GetHeight(player->m_pos.x, player->m_pos.z) : 0.0f;
}

void SyncBootstrapRuntimeActorWorldPos(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_attr) {
        return;
    }

    size_t resyncedActors = 0;
    const u32 serverNow = g_session.GetServerTime();

    for (const auto& entry : mode.m_runtimeActors) {
        const u32 gid = entry.first;
        CGameActor* actor = entry.second;
        if (!actor || actor == mode.m_world->m_player || gid == g_session.m_gid || gid == g_session.m_aid) {
            continue;
        }

        const auto posIt = mode.m_actorPosList.find(gid);
        const bool hasTrackedCell = posIt != mode.m_actorPosList.end();
        if (!hasTrackedCell && actor->m_moveDestX == 0 && actor->m_moveDestY == 0) {
            continue;
        }

        const int destX = hasTrackedCell ? posIt->second.x : actor->m_moveDestX;
        const int destY = hasTrackedCell ? posIt->second.y : actor->m_moveDestY;
        const bool hasTrackedSrc = actor->m_isMoving && (actor->m_moveSrcX != 0 || actor->m_moveSrcY != 0);
        const int srcX = hasTrackedSrc ? actor->m_moveSrcX : destX;
        const int srcY = hasTrackedSrc ? actor->m_moveSrcY : destY;

        const vector3d srcPos{
            TileToWorldCoordX(mode.m_world, srcX),
            0.0f,
            TileToWorldCoordZ(mode.m_world, srcY)
        };
        const vector3d dstPos{
            TileToWorldCoordX(mode.m_world, destX),
            0.0f,
            TileToWorldCoordZ(mode.m_world, destY)
        };

        vector3d resolvedSrcPos = srcPos;
        vector3d resolvedDstPos = dstPos;
        resolvedSrcPos.y = mode.m_world->m_attr->GetHeight(resolvedSrcPos.x, resolvedSrcPos.z);
        resolvedDstPos.y = mode.m_world->m_attr->GetHeight(resolvedDstPos.x, resolvedDstPos.z);

        actor->UnRegisterPos();
        actor->m_moveSrcX = srcX;
        actor->m_moveSrcY = srcY;
        actor->m_moveDestX = destX;
        actor->m_moveDestY = destY;
        actor->m_moveStartPos = resolvedSrcPos;
        actor->m_moveEndPos = resolvedDstPos;

        if (actor->m_isMoving && actor->m_moveEndTime > actor->m_moveStartTime) {
            const float duration = static_cast<float>(actor->m_moveEndTime - actor->m_moveStartTime);
            const float elapsed = static_cast<float>(serverNow - actor->m_moveStartTime);
            const float ratio = duration > 0.0f
                ? (std::max)(0.0f, (std::min)(1.0f, elapsed / duration))
                : 1.0f;
            actor->m_pos.x = resolvedSrcPos.x + (resolvedDstPos.x - resolvedSrcPos.x) * ratio;
            actor->m_pos.y = resolvedSrcPos.y + (resolvedDstPos.y - resolvedSrcPos.y) * ratio;
            actor->m_pos.z = resolvedSrcPos.z + (resolvedDstPos.z - resolvedSrcPos.z) * ratio;
        } else {
            actor->m_pos = resolvedDstPos;
        }

        actor->RegisterPos();
        if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
            pcActor->InvalidateBillboard();
        }
        ++resyncedActors;
    }

    if (resyncedActors > 0) {
        DbgLog("[GameMode] resynced runtime actor world positions after bootstrap count=%zu\n",
            resyncedActors);
    }
}

void EnsureRealView(CGameMode& mode)
{
    if (mode.m_view || !mode.m_world || !mode.m_world->m_ground) {
        return;
    }

    CView* view = new CView();
    if (!view) {
        return;
    }

    view->SetWorld(mode.m_world);
    view->OnEnterFrame();
    mode.m_view = view;
    ConfigureCameraForMap(mode);
    view->OnCalcViewInfo();
    DbgLog("[GameMode] created real m_view for world '%s'\n", mode.m_rswName);
}

struct BootstrapWorldCache {
    struct RswInfo {
        int versionMajor = 0;
        int versionMinor = 0;
        std::string gndName;
        std::string attrName;
        std::string scrName;
        float waterLevel = 0.0f;
        int waterType = 0;
        float waveHeight = 1.0f;
        float waveSpeed = 2.0f;
        float wavePitch = 50.0f;
        int waterAnimSpeed = 3;
        int lightLongitude = 45;
        int lightLatitude = 45;
        vector3d diffuseCol{ 1.0f, 1.0f, 1.0f };
        vector3d ambientCol{ 0.3f, 0.3f, 0.3f };
        vector3d lightDir{ 0.5f, 0.70710677f, 0.5f };
        bool lightFromFile = false;
        int groundTop = -500;
        int groundBottom = 500;
        int groundLeft = -500;
        int groundRight = 500;
        bool loaded = false;
    };

    std::string mapName;
    std::string rswPath;
    std::string attrPath;
    std::string gndPath;
    C3dWorldRes* worldRes = nullptr;
    C3dAttr* attrRes = nullptr;
    const CGndRes* gndRes = nullptr;
    RswInfo worldInfo;
    std::vector<COLORREF> gndTextureColors;
    size_t nextTextureColorIndex = 0;
    size_t nextBackgroundActorIndex = 0;
    size_t nextFixedEffectIndex = 0;
    BootstrapLoadStage loadStage = BootstrapLoadStage::ResolveWorld;
    std::string minimapPath;
    std::vector<u32> minimapPixels;
    int minimapWidth;
    int minimapHeight;
};

#if RO_ENABLE_QT6_UI
void DrawBootstrapSceneTextQt(unsigned int* pixels,
    int width,
    int height,
    const CGameMode& mode,
    const BootstrapWorldCache& cache,
    int attrWidth,
    int attrHeight,
    int groundWidth,
    int groundHeight,
    int activeWidth,
    int activeHeight)
{
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    QImage image(reinterpret_cast<uchar*>(pixels), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setPen(Qt::white);

    char header[128];
    std::snprintf(header, sizeof(header), "Map: %s   Pos: %d, %d   Dir: %d",
        g_session.m_curMap[0] ? g_session.m_curMap : "(unknown)",
        g_session.m_playerPosX,
        g_session.m_playerPosY,
        g_session.m_playerDir);

    char subHeader[160];
    std::snprintf(subHeader, sizeof(subHeader), "World bootstrap active: %s", mode.m_rswName[0] ? mode.m_rswName : "(no world name)");

    char assetInfo[256];
    std::snprintf(assetInfo, sizeof(assetInfo), "MiniMap %dx%d   GAT %dx%d   GND %dx%d   Node %dx%d   Actors %zu/%zu",
        cache.minimapWidth,
        cache.minimapHeight,
        attrWidth,
        attrHeight,
        groundWidth,
        groundHeight,
        activeWidth,
        activeHeight,
        mode.m_actorPosList.size(),
        mode.m_runtimeActors.size());

    const char* hintText = mode.m_world && mode.m_world->m_ground && mode.m_world->m_attr
        ? "Connected to world server. Ref-style bootstrap now renders from the world ground and attr objects before full scene/view loading."
        : "Connected to world server. Ref-style world bootstrap is active, but the ground or attr resource is still incomplete.";

    painter.setFont(BuildBootstrapHeaderFont());
    painter.drawText(QRect(20, 18, width - 220, 24), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QString::fromLocal8Bit(header));

    painter.setFont(BuildBootstrapBodyFont());
    painter.drawText(QRect(20, 40, width - 220, 22), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QString::fromLocal8Bit(subHeader));

    const QRect assetRect(width - 184, 188, 164, 18);
    const QFontMetrics assetMetrics(painter.font());
    const QString assetLabel = assetMetrics.elidedText(QString::fromLocal8Bit(assetInfo), Qt::ElideRight, assetRect.width());
    painter.drawText(assetRect, Qt::AlignCenter | Qt::AlignVCenter | Qt::TextSingleLine, assetLabel);

    painter.drawText(QRect(20, height - 30, width - 40, 18), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QString::fromLocal8Bit(hintText));
}
#endif

bool IsBootstrapWorldReady(const BootstrapWorldCache& cache)
{
    return cache.loadStage == BootstrapLoadStage::Complete;
}

float GetBootstrapWorldLoadProgress(const BootstrapWorldCache& cache)
{
    switch (cache.loadStage) {
    case BootstrapLoadStage::ResolveWorld:
        return 0.06f;
    case BootstrapLoadStage::LoadAttr:
        return 0.14f;
    case BootstrapLoadStage::ResolveGround:
        return 0.22f;
    case BootstrapLoadStage::SampleGroundTextures: {
        const size_t totalTextures = cache.gndRes ? static_cast<size_t>(cache.gndRes->m_numTexture) : 0;
        const float frac = totalTextures > 0
            ? static_cast<float>(cache.nextTextureColorIndex) / static_cast<float>(totalTextures)
            : 1.0f;
        return 0.22f + frac * 0.28f;
    }
    case BootstrapLoadStage::BuildGround:
        return 0.54f;
    case BootstrapLoadStage::BuildBackgroundObjects: {
        const size_t totalActors = cache.worldRes ? cache.worldRes->m_3dActors.size() : 0;
        const float frac = totalActors > 0
            ? static_cast<float>(cache.nextBackgroundActorIndex) / static_cast<float>(totalActors)
            : 1.0f;
        return 0.58f + frac * 0.22f;
    }
    case BootstrapLoadStage::BuildFixedEffects: {
        const size_t totalEffects = cache.worldRes ? cache.worldRes->m_particles.size() : 0;
        const float frac = totalEffects > 0
            ? static_cast<float>(cache.nextFixedEffectIndex) / static_cast<float>(totalEffects)
            : 1.0f;
        return 0.82f + frac * 0.10f;
    }
    case BootstrapLoadStage::LoadMinimap:
        return 0.95f;
    case BootstrapLoadStage::Complete:
        return 1.0f;
    default:
        return 0.0f;
    }
}

bool BuildBootstrapPaletteOverride(const std::string& paletteName, std::array<unsigned int, 256>& outPalette)
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

POINT GetBootstrapLayerPoint(int layerPriority,
    int resolvedLayer,
    CImfRes* imfRes,
    const CMotion* motion,
    const std::string& bodyActName,
    int curAction,
    int curMotion)
{
    POINT point = imfRes->GetPoint(resolvedLayer, curAction, curMotion);
    if (layerPriority != 1 || !motion || motion->attachInfo.empty()) {
        return point;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return point;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, curMotion);
    if (!bodyMotion || bodyMotion->attachInfo.empty()) {
        return point;
    }

    const CAttachPointInfo& headAttach = motion->attachInfo.front();
    const CAttachPointInfo& bodyAttach = bodyMotion->attachInfo.front();
    if (headAttach.attr != bodyAttach.attr) {
        return point;
    }

    point.x += bodyAttach.x - headAttach.x;
    point.y += bodyAttach.y - headAttach.y;
    return point;
}

bool DrawBootstrapPcLayer(HDC hdc,
    int drawX,
    int drawY,
    int layerIndex,
    int curAction,
    int curMotion,
    const std::string& actName,
    const std::string& sprName,
    const std::string& imfName,
    const std::string& bodyActName,
    const std::string& paletteName)
{
    CActRes* actRes = g_resMgr.GetAs<CActRes>(actName.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprName.c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, curAction, curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(curAction, curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        return false;
    }

    const POINT point = GetBootstrapLayerPoint(layerIndex, resolvedLayer, imfRes, motion, bodyActName, curAction, curMotion);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (!paletteName.empty() && BuildBootstrapPaletteOverride(paletteName, paletteOverride)) {
        palette = paletteOverride.data();
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    return DrawActMotionToHdc(hdc, drawX + point.x, drawY + point.y, sprRes, &singleLayerMotion, palette);
}

void DrawBootstrapPlayerSprite(HDC hdc, int drawX, int drawY)
{
    char bodyAct[260] = {};
    char bodySpr[260] = {};
    char headAct[260] = {};
    char headSpr[260] = {};
    char imfName[260] = {};
    char bodyPalette[260] = {};
    char headPalette[260] = {};

    const int sex = g_session.GetSex();
    int head = g_session.m_playerHead;
    const int curAction = g_session.m_playerDir & 7;
    const int curMotion = 0;
    const int weaponValue = g_session.GetCurrentPlayerWeaponValue();

    const std::string bodyActName = g_session.GetPlayerBodyActName(g_session.m_playerJob, sex, weaponValue, bodyAct);
    const std::string bodySprName = g_session.GetPlayerBodySprName(g_session.m_playerJob, sex, weaponValue, bodySpr);
    const std::string headActName = g_session.GetHeadActName(g_session.m_playerJob, &head, sex, headAct);
    const std::string headSprName = g_session.GetHeadSprName(g_session.m_playerJob, &head, sex, headSpr);
    const std::string imfPath = g_session.GetImfName(g_session.m_playerJob, head, sex, imfName);
    const std::string bodyPaletteName = g_session.m_playerBodyPalette > 0
        ? g_session.GetBodyPaletteName(g_session.m_playerJob, sex, g_session.m_playerBodyPalette, bodyPalette)
        : std::string();
    const std::string headPaletteName = g_session.m_playerHeadPalette > 0
        ? g_session.GetHeadPaletteName(head, g_session.m_playerJob, sex, g_session.m_playerHeadPalette, headPalette)
        : std::string();

    DrawBootstrapPcLayer(hdc, drawX, drawY, 0, curAction, curMotion, bodyActName, bodySprName, imfPath, bodyActName, bodyPaletteName);
    DrawBootstrapPcLayer(hdc, drawX, drawY, 1, curAction, curMotion, headActName, headSprName, imfPath, bodyActName, headPaletteName);
}

#if RO_ENABLE_QT6_UI
bool DrawBootstrapPcLayerToArgb(unsigned int* pixels,
    int width,
    int height,
    int drawX,
    int drawY,
    int layerIndex,
    int curAction,
    int curMotion,
    const std::string& actName,
    const std::string& sprName,
    const std::string& imfName,
    const std::string& bodyActName,
    const std::string& paletteName)
{
    CActRes* actRes = g_resMgr.GetAs<CActRes>(actName.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprName.c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, curAction, curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(curAction, curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        return false;
    }

    const POINT point = GetBootstrapLayerPoint(layerIndex, resolvedLayer, imfRes, motion, bodyActName, curAction, curMotion);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (!paletteName.empty() && BuildBootstrapPaletteOverride(paletteName, paletteOverride)) {
        palette = paletteOverride.data();
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    return DrawActMotionToArgb(pixels, width, height, drawX + point.x, drawY + point.y, sprRes, &singleLayerMotion, palette);
}

void DrawBootstrapPlayerSpriteToArgb(unsigned int* pixels, int width, int height, int drawX, int drawY)
{
    char bodyAct[260] = {};
    char bodySpr[260] = {};
    char headAct[260] = {};
    char headSpr[260] = {};
    char imfName[260] = {};
    char bodyPalette[260] = {};
    char headPalette[260] = {};

    const int sex = g_session.GetSex();
    int head = g_session.m_playerHead;
    const int curAction = g_session.m_playerDir & 7;
    const int curMotion = 0;
    const int weaponValue = g_session.GetCurrentPlayerWeaponValue();

    const std::string bodyActName = g_session.GetPlayerBodyActName(g_session.m_playerJob, sex, weaponValue, bodyAct);
    const std::string bodySprName = g_session.GetPlayerBodySprName(g_session.m_playerJob, sex, weaponValue, bodySpr);
    const std::string headActName = g_session.GetHeadActName(g_session.m_playerJob, &head, sex, headAct);
    const std::string headSprName = g_session.GetHeadSprName(g_session.m_playerJob, &head, sex, headSpr);
    const std::string imfPath = g_session.GetImfName(g_session.m_playerJob, head, sex, imfName);
    const std::string bodyPaletteName = g_session.m_playerBodyPalette > 0
        ? g_session.GetBodyPaletteName(g_session.m_playerJob, sex, g_session.m_playerBodyPalette, bodyPalette)
        : std::string();
    const std::string headPaletteName = g_session.m_playerHeadPalette > 0
        ? g_session.GetHeadPaletteName(head, g_session.m_playerJob, sex, g_session.m_playerHeadPalette, headPalette)
        : std::string();

    DrawBootstrapPcLayerToArgb(pixels, width, height, drawX, drawY, 0, curAction, curMotion, bodyActName, bodySprName, imfPath, bodyActName, bodyPaletteName);
    DrawBootstrapPcLayerToArgb(pixels, width, height, drawX, drawY, 1, curAction, curMotion, headActName, headSprName, imfPath, bodyActName, headPaletteName);
}

void DrawPixelsStretchedQt(QPainter& painter, const u32* pixels, int width, int height, const RECT& dst)
{
    if (!pixels || width <= 0 || height <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }

    QImage image(reinterpret_cast<const uchar*>(pixels), width, height, width * static_cast<int>(sizeof(u32)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    painter.drawImage(QRect(dst.left, dst.top, dst.right - dst.left, dst.bottom - dst.top), image);
}

void DrawGatWorldToArgb(unsigned int* pixels, int width, int height, const RECT& viewRect, const CGameMode& mode, const BootstrapWorldCache& cache)
{
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    const CWorld* world = mode.m_world;
    if (!world) {
        return;
    }

    const SceneGraphNode* scene = world->m_Calculated ? world->m_Calculated : &world->m_rootNode;
    const C3dGround* ground = scene->m_ground ? scene->m_ground : world->m_ground;
    const C3dAttr* attr = scene->m_attr ? scene->m_attr : world->m_attr;
    const vector3d diffuseCol = ground ? ground->m_diffuseCol : cache.worldInfo.diffuseCol;
    const vector3d ambientCol = ground ? ground->m_ambientCol : cache.worldInfo.ambientCol;
    const vector3d lightDir = ground ? ground->m_lightDir : cache.worldInfo.lightDir;
    const float waterLevel = ground ? ground->m_waterLevel : cache.worldInfo.waterLevel;

    const COLORREF skyColor = BlendColor(
        ColorFromVector(diffuseCol, 0.72f),
        ColorFromVector(ambientCol, 1.15f),
        0.45f);
    const COLORREF horizonColor = BlendColor(
        ColorFromVector(ambientCol, 0.7f),
        RGB(36, 44, 52),
        0.35f);
    const COLORREF groundBaseColor = BlendColor(
        ColorFromVector(ambientCol, 0.95f),
        RGB(110, 102, 84),
        0.55f);
    const COLORREF waterColor = BlendColor(
        ColorFromVector(diffuseCol, 0.55f),
        RGB(38, 78, 130),
        0.7f);

    QImage image(reinterpret_cast<uchar*>(pixels), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);

    QRect skyRect(viewRect.left, viewRect.top, viewRect.right - viewRect.left, (viewRect.bottom - viewRect.top) / 2);
    painter.fillRect(skyRect, ToQColor(skyColor));
    QRect groundBackRect(viewRect.left, skyRect.bottom(), viewRect.right - viewRect.left, viewRect.bottom - skyRect.bottom());
    painter.fillRect(groundBackRect, ToQColor(horizonColor));

    const int terrainWidth = ground ? ground->m_width : (attr ? attr->m_width : 0);
    const int terrainHeight = ground ? ground->m_height : (attr ? attr->m_height : 0);
    if (terrainWidth <= 0 || terrainHeight <= 0 || (!ground && !attr)) {
        return;
    }

    RECT terrainArea = ground ? scene->m_groundArea : scene->m_attrArea;
    if (terrainArea.right <= terrainArea.left || terrainArea.bottom <= terrainArea.top) {
        SetRect(&terrainArea, 0, 0, terrainWidth, terrainHeight);
    }

    const float tileWidth = 24.0f;
    const float tileHeight = 12.0f;
    const float heightScale = 2.2f;
    const float sceneSpanTilesX = ground && ground->m_zoom > 0.0f
        ? (scene->m_aabb.max.x - scene->m_aabb.min.x) / ground->m_zoom
        : static_cast<float>(terrainArea.right - terrainArea.left);
    const float sceneSpanTilesY = ground && ground->m_zoom > 0.0f
        ? (scene->m_aabb.max.z - scene->m_aabb.min.z) / ground->m_zoom
        : static_cast<float>(terrainArea.bottom - terrainArea.top);
    const int radius = (std::max)(8, (std::min)(18, static_cast<int>((std::min)(sceneSpanTilesX, sceneSpanTilesY) * 0.5f)));
    const int areaLeft = static_cast<int>(terrainArea.left);
    const int areaRight = static_cast<int>(terrainArea.right);
    const int areaTop = static_cast<int>(terrainArea.top);
    const int areaBottom = static_cast<int>(terrainArea.bottom);
    const int startX = (std::max)(areaLeft, g_session.m_playerPosX - radius);
    const int endX = (std::min)(areaRight - 2, g_session.m_playerPosX + radius);
    const int startY = (std::max)(areaTop, g_session.m_playerPosY - radius);
    const int endY = (std::min)(areaBottom - 2, g_session.m_playerPosY + radius);

    if (endX < startX || endY < startY) {
        return;
    }

    for (int mapY = startY; mapY <= endY; ++mapY) {
        for (int mapX = startX; mapX <= endX; ++mapX) {
            const float h1 = GetTerrainCornerHeight(*scene, mapX, mapY, 0);
            const float h2 = GetTerrainCornerHeight(*scene, mapX, mapY, 1);
            const float h3 = GetTerrainCornerHeight(*scene, mapX, mapY, 2);
            const float h4 = GetTerrainCornerHeight(*scene, mapX, mapY, 3);
            const float avgHeight = (h1 + h2 + h3 + h4) * 0.25f;
            const float relX = static_cast<float>(mapX - g_session.m_playerPosX);
            const float relY = static_cast<float>(mapY - g_session.m_playerPosY);
            const float eastHeight = GetTerrainAverageHeightAt(*scene, mapX + 1, mapY);
            const float southHeight = GetTerrainAverageHeightAt(*scene, mapX, mapY + 1);
            const float slope = (std::min)(1.0f, (std::fabs(eastHeight - avgHeight) + std::fabs(southHeight - avgHeight)) / 18.0f);

            POINT poly[4];
            poly[0] = ProjectWorldPoint(viewRect, relX, relY, h1, tileWidth, tileHeight, heightScale);
            poly[1] = ProjectWorldPoint(viewRect, relX + 1.0f, relY, h2, tileWidth, tileHeight, heightScale);
            poly[2] = ProjectWorldPoint(viewRect, relX, relY + 1.0f, h3, tileWidth, tileHeight, heightScale);
            poly[3] = ProjectWorldPoint(viewRect, relX + 1.0f, relY + 1.0f, h4, tileWidth, tileHeight, heightScale);

            const float normalizedHeight = ClampUnit((avgHeight + 20.0f) / 50.0f);
            const bool isWater = (ground || cache.worldInfo.loaded) && avgHeight <= waterLevel + 0.25f;
            COLORREF fillColor = isWater ? waterColor : groundBaseColor;
            const COLORREF gndColor = GetGroundTextureColor(*scene, cache, mapX, mapY);
            if (gndColor != RGB(0, 0, 0)) {
                fillColor = BlendColor(fillColor, gndColor, 0.6f);
            }

            const vector3d normal = ComputeCellNormal(h1, h2, h3, h4);
            const vector3d lightColor = ComputeLightingColor(lightDir, diffuseCol, ambientCol, normal);
            fillColor = ModulateColor(fillColor, lightColor);
            fillColor = MultiplyColor(fillColor, 0.82f + normalizedHeight * 0.18f - slope * 0.12f);

            if (ground) {
                if (const CGroundCell* groundCell = ground->GetCell(mapX, mapY)) {
                    if (const CGroundSurface* topSurface = ground->GetSurface(groundCell->topSurfaceId)) {
                        const COLORREF surfaceColor = RGB(
                            (topSurface->color >> 16) & 0xFF,
                            (topSurface->color >> 8) & 0xFF,
                            topSurface->color & 0xFF);
                        fillColor = BlendColor(fillColor, surfaceColor, 0.22f);
                    }
                }
            }

            const CAttrCell* attrCell = GetAttrCellSafe(*scene, mapX, mapY);
            const int attrFlag = attrCell ? attrCell->flag : 0;
            if (attrFlag != 0) {
                fillColor = BlendColor(fillColor, RGB(128, 84, 72), 0.55f);
            }

            QPoint qpoly[4] = {
                QPoint(poly[0].x, poly[0].y),
                QPoint(poly[1].x, poly[1].y),
                QPoint(poly[3].x, poly[3].y),
                QPoint(poly[2].x, poly[2].y)
            };
            painter.setBrush(ToQColor(fillColor));
            painter.setPen(ToQColor(BlendColor(RGB(28, 32, 26), fillColor, 0.35f)));
            painter.drawPolygon(qpoly, 4);
        }
    }

    for (const auto& entry : mode.m_actorPosList) {
        const u32 gid = entry.first;
        if (gid == g_session.m_gid) {
            continue;
        }

        const int mapX = entry.second.x;
        const int mapY = entry.second.y;
        if (mapX < startX || mapX > endX || mapY < startY || mapY > endY) {
            continue;
        }

        const CGameActor* actor = nullptr;
        const auto actorIt = mode.m_runtimeActors.find(gid);
        if (actorIt != mode.m_runtimeActors.end()) {
            actor = actorIt->second;
        }

        const float relX = static_cast<float>(mapX - g_session.m_playerPosX);
        const float relY = static_cast<float>(mapY - g_session.m_playerPosY);
        const float actorHeight = GetTerrainAverageHeightAt(*scene, mapX, mapY);
        const POINT actorPoint = ProjectWorldPoint(viewRect, relX + 0.5f, relY + 0.5f, actorHeight, tileWidth, tileHeight, heightScale);

        COLORREF fillColor = RGB(226, 116, 70);
        COLORREF outlineColor = RGB(72, 28, 16);
        int radiusX = 4;
        int radiusY = 7;
        if (actor && actor->m_isPc) {
            fillColor = RGB(120, 214, 255);
            outlineColor = RGB(20, 52, 78);
            radiusX = 5;
            radiusY = 8;
        }

        painter.setBrush(ToQColor(fillColor));
        painter.setPen(ToQColor(outlineColor));
        painter.drawEllipse(QRect(actorPoint.x - radiusX, actorPoint.y - radiusY, radiusX * 2, radiusY * 2 + 1));
    }

    const float playerHeight = GetTerrainAverageHeightAt(*scene, g_session.m_playerPosX, g_session.m_playerPosY);
    const POINT playerPoint = ProjectWorldPoint(viewRect, 0.5f, 0.5f, playerHeight, tileWidth, tileHeight, heightScale);
    painter.end();

    if (g_session.m_playerJob >= 0) {
        DrawBootstrapPlayerSpriteToArgb(pixels, width, height, playerPoint.x, playerPoint.y - 10);
    }

    QPainter markerPainter(&image);
    markerPainter.setRenderHint(QPainter::Antialiasing, false);
    markerPainter.setBrush(ToQColor(RGB(255, 226, 102)));
    markerPainter.setPen(QPen(ToQColor(RGB(64, 40, 12)), 2));
    markerPainter.drawEllipse(QRect(playerPoint.x - 6, playerPoint.y - 10, 12, 12));

    POINT dirEnd = playerPoint;
    switch (g_session.m_playerDir & 7) {
    case 0: dirEnd.y -= 18; break;
    case 1: dirEnd.x += 12; dirEnd.y -= 12; break;
    case 2: dirEnd.x += 18; break;
    case 3: dirEnd.x += 12; dirEnd.y += 12; break;
    case 4: dirEnd.y += 18; break;
    case 5: dirEnd.x -= 12; dirEnd.y += 12; break;
    case 6: dirEnd.x -= 18; break;
    case 7: dirEnd.x -= 12; dirEnd.y -= 12; break;
    }
    markerPainter.drawLine(QPoint(playerPoint.x, playerPoint.y - 4), QPoint(dirEnd.x, dirEnd.y));
}
#endif

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

std::string BaseNameOf(const std::string& path)
{
    const std::string normalized = NormalizeSlash(path);
    const size_t slash = normalized.find_last_of('\\');
    if (slash == std::string::npos) {
        return normalized;
    }
    return normalized.substr(slash + 1);
}

BootstrapWorldCache& GetBootstrapWorldCache()
{
    static BootstrapWorldCache cache{};
    return cache;
}

const char* UiKorPrefix()
{
    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";
    return kUiKor;
}

const std::map<std::string, std::string>& GetResNameTableAliases()
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

        std::string key = NormalizeSlash(line.substr(0, firstHash));
        std::string value = NormalizeSlash(line.substr(firstHash + 1, secondHash - firstHash - 1));
        if (!key.empty() && !value.empty()) {
            s_aliases[ToLowerAscii(key)] = value;
        }
    }

    return s_aliases;
}

std::string ResolveAliasName(const std::string& candidate)
{
    if (candidate.empty()) {
        return std::string();
    }

    const auto& aliases = GetResNameTableAliases();
    const std::string normalized = ToLowerAscii(NormalizeSlash(candidate));
    const auto it = aliases.find(normalized);
    if (it != aliases.end()) {
        return it->second;
    }

    const std::string candidateBase = ToLowerAscii(BaseNameOf(normalized));
    for (const auto& entry : aliases) {
        if (ToLowerAscii(BaseNameOf(entry.first)) == candidateBase) {
            return entry.second;
        }
    }

    return std::string();
}

std::string ResolveExistingPath(const std::string& candidate, const std::vector<std::string>& directPrefixes)
{
    if (candidate.empty()) {
        return std::string();
    }

    const std::string normalized = NormalizeSlash(candidate);
    if (g_fileMgr.IsDataExist(normalized.c_str())) {
        return normalized;
    }

    for (const std::string& prefix : directPrefixes) {
        const std::string prefixed = NormalizeSlash(prefix + normalized);
        if (g_fileMgr.IsDataExist(prefixed.c_str())) {
            return prefixed;
        }
    }

    return std::string();
}

void ResetBootstrapWorldCache(BootstrapWorldCache& cache)
{
    cache.mapName.clear();
    cache.rswPath.clear();
    cache.attrPath.clear();
    cache.gndPath.clear();
    cache.worldRes = nullptr;
    cache.attrRes = nullptr;
    cache.gndRes = nullptr;
    cache.worldInfo = BootstrapWorldCache::RswInfo{};
    cache.gndTextureColors.clear();
    cache.nextTextureColorIndex = 0;
    cache.nextBackgroundActorIndex = 0;
    cache.nextFixedEffectIndex = 0;
    cache.loadStage = BootstrapLoadStage::ResolveWorld;
    cache.minimapPath.clear();
    cache.minimapPixels.clear();
    cache.minimapWidth = 0;
    cache.minimapHeight = 0;
}

const std::vector<std::string>& GetDataNamesByExtension(const char* ext)
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

std::string ResolveDataPath(const std::string& fileName, const char* ext, const std::vector<std::string>& directPrefixes)
{
    if (fileName.empty()) {
        return std::string();
    }

    const std::string normalizedName = NormalizeSlash(fileName);
    for (const std::string& prefix : directPrefixes) {
        const std::string candidate = NormalizeSlash(prefix + normalizedName);
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }

        const std::string alias = ResolveAliasName(candidate);
        if (!alias.empty()) {
            const std::string resolvedAlias = ResolveExistingPath(alias, directPrefixes);
            if (!resolvedAlias.empty()) {
                return resolvedAlias;
            }
        }
    }

    const std::string directAlias = ResolveAliasName(normalizedName);
    if (!directAlias.empty()) {
        const std::string resolvedDirectAlias = ResolveExistingPath(directAlias, directPrefixes);
        if (!resolvedDirectAlias.empty()) {
            return resolvedDirectAlias;
        }
    }

    const std::string wantedBase = ToLowerAscii(BaseNameOf(normalizedName));
    const std::string wantedStem = wantedBase.rfind('.') != std::string::npos
        ? wantedBase.substr(0, wantedBase.rfind('.'))
        : wantedBase;
    const auto& knownNames = GetDataNamesByExtension(ext);
    for (const std::string& known : knownNames) {
        if (ToLowerAscii(BaseNameOf(known)) == wantedBase) {
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
        const std::string alias = ResolveAliasName(prefix + normalizedName);
        if (!alias.empty()) {
            const std::string aliasBase = ToLowerAscii(BaseNameOf(alias));
            if (aliasBase == wantedBase || aliasBase.find(wantedStem) != std::string::npos) {
                const std::string resolvedAlias = ResolveExistingPath(alias, directPrefixes);
                if (!resolvedAlias.empty()) {
                    return resolvedAlias;
                }
            }
        }
    }

    return std::string();
}

bool ComputeBitmapAverageColor(const unsigned int* pixels, size_t pixelCount, COLORREF* outColor)
{
    if (!pixels || pixelCount == 0 || !outColor) {
        return false;
    }

    unsigned long long totalR = 0;
    unsigned long long totalG = 0;
    unsigned long long totalB = 0;
    for (size_t index = 0; index < pixelCount; ++index) {
        const unsigned int pixel = pixels[index];
        totalB += pixel & 0xFFu;
        totalG += (pixel >> 8) & 0xFFu;
        totalR += (pixel >> 16) & 0xFFu;
    }

    *outColor = RGB(
        static_cast<int>(totalR / static_cast<unsigned long long>(pixelCount)),
        static_cast<int>(totalG / static_cast<unsigned long long>(pixelCount)),
        static_cast<int>(totalB / static_cast<unsigned long long>(pixelCount)));
    return true;
}

bool LoadAverageBitmapColorFromGameData(const std::string& dataPath, COLORREF* outColor)
{
    u32* pixels = nullptr;
    int width = 0;
    int height = 0;
    if (!LoadBgraPixelsFromGameData(dataPath.c_str(), &pixels, &width, &height)
        || !pixels
        || width <= 0
        || height <= 0) {
        delete[] pixels;
        return false;
    }

    const bool ok = ComputeBitmapAverageColor(
        pixels,
        static_cast<size_t>(width) * static_cast<size_t>(height),
        outColor);
    delete[] pixels;
    return ok;
}

bool LoadRswWorldInfo(const std::string& dataPath, BootstrapWorldCache::RswInfo* outInfo)
{
    if (!outInfo || dataPath.empty()) {
        return false;
    }

    C3dWorldRes* worldRes = g_resMgr.GetAs<C3dWorldRes>(dataPath.c_str());
    if (!worldRes) {
        return false;
    }

    BootstrapWorldCache::RswInfo info{};
    info.versionMajor = worldRes->m_verMajor;
    info.versionMinor = worldRes->m_verMinor;
    info.gndName = worldRes->m_gndFile;
    info.attrName = worldRes->m_attrFile;
    info.scrName = worldRes->m_scrFile;
    info.waterLevel = worldRes->m_waterLevel;
    info.waterType = worldRes->m_waterType;
    info.waveHeight = worldRes->m_waveHeight;
    info.waveSpeed = worldRes->m_waveSpeed;
    info.wavePitch = worldRes->m_wavePitch;
    info.waterAnimSpeed = worldRes->m_waterAnimSpeed;
    info.lightLongitude = worldRes->m_lightLongitude;
    info.lightLatitude = worldRes->m_lightLatitude;
    info.diffuseCol = worldRes->m_diffuseCol;
    info.ambientCol = worldRes->m_ambientCol;
    info.lightDir = worldRes->m_lightDir;
    info.lightFromFile = worldRes->m_lightFromFile;
    info.groundTop = worldRes->m_groundTop;
    info.groundBottom = worldRes->m_groundBottom;
    info.groundLeft = worldRes->m_groundLeft;
    info.groundRight = worldRes->m_groundRight;
    info.loaded = true;
    *outInfo = info;
    return true;
}

void ApplyBootstrapWorldLighting(const BootstrapWorldCache& cache)
{
    vector3d lightDir = cache.worldInfo.lightDir;
    vector3d diffuseCol = cache.worldInfo.diffuseCol;
    vector3d ambientCol = cache.worldInfo.ambientCol;
    g_renderer.SetLight(&lightDir, &diffuseCol, &ambientCol);

    const char* source = cache.worldInfo.loaded
        ? (cache.worldInfo.lightFromFile ? "rsw" : "rsw-defaults")
        : "no-rsw";
    DbgLog("[GameMode] bootstrap world lighting map='%s' rsw='%s' source=%s lon=%d lat=%d diffuse=(%.3f,%.3f,%.3f) ambient=(%.3f,%.3f,%.3f)\n",
        cache.mapName.c_str(),
        cache.rswPath.empty() ? "(none)" : cache.rswPath.c_str(),
        source,
        cache.worldInfo.lightLongitude,
        cache.worldInfo.lightLatitude,
        cache.worldInfo.diffuseCol.x,
        cache.worldInfo.diffuseCol.y,
        cache.worldInfo.diffuseCol.z,
        cache.worldInfo.ambientCol.x,
        cache.worldInfo.ambientCol.y,
        cache.worldInfo.ambientCol.z);
}

void DrawPixelsStretched(HDC targetDC, const u32* pixels, int width, int height, const RECT& dst)
{
    if (!targetDC || !pixels || width <= 0 || height <= 0) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(targetDC, HALFTONE);
    StretchDIBits(targetDC,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        0,
        0,
        width,
        height,
        pixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
}

float ClampUnit(float value)
{
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

bool IsTileInsideRect(const RECT& area, int x, int y)
{
    return x >= static_cast<int>(area.left)
        && x < static_cast<int>(area.right)
        && y >= static_cast<int>(area.top)
        && y < static_cast<int>(area.bottom);
}

const CGroundCell* GetGroundCellSafe(const SceneGraphNode& scene, int x, int y)
{
    if (!scene.m_ground || !IsTileInsideRect(scene.m_groundArea, x, y)) {
        return nullptr;
    }
    return scene.m_ground->GetCell(x, y);
}

const CAttrCell* GetAttrCellSafe(const SceneGraphNode& scene, int x, int y)
{
    if (!scene.m_attr || !IsTileInsideRect(scene.m_attrArea, x, y)) {
        return nullptr;
    }
    if (x < 0 || y < 0 || x >= scene.m_attr->m_width || y >= scene.m_attr->m_height || scene.m_attr->m_cells.empty()) {
        return nullptr;
    }
    return &scene.m_attr->m_cells[static_cast<size_t>(y) * static_cast<size_t>(scene.m_attr->m_width) + static_cast<size_t>(x)];
}

float GetTerrainCornerHeight(const SceneGraphNode& scene, int x, int y, int corner)
{
    if (const CGroundCell* cell = GetGroundCellSafe(scene, x, y)) {
        if (corner >= 0 && corner < 4) {
            return cell->h[corner];
        }
    }

    if (const CAttrCell* cell = GetAttrCellSafe(scene, x, y)) {
        switch (corner) {
        case 0: return cell->h1;
        case 1: return cell->h2;
        case 2: return cell->h3;
        case 3: return cell->h4;
        default: return 0.0f;
        }
    }

    return 0.0f;
}

float GetTerrainAverageHeightAt(const SceneGraphNode& scene, int x, int y)
{
    if (const CGroundCell* cell = GetGroundCellSafe(scene, x, y)) {
        return (cell->h[0] + cell->h[1] + cell->h[2] + cell->h[3]) * 0.25f;
    }
    if (const CAttrCell* cell = GetAttrCellSafe(scene, x, y)) {
        return (cell->h1 + cell->h2 + cell->h3 + cell->h4) * 0.25f;
    }
    return 0.0f;
}

COLORREF GetGroundTextureColor(const SceneGraphNode& scene, const BootstrapWorldCache& cache, int x, int y)
{
    if (!scene.m_ground) {
        return RGB(0, 0, 0);
    }

    const CGroundCell* cell = GetGroundCellSafe(scene, x, y);
    if (!cell || cell->topSurfaceId < 0) {
        return RGB(0, 0, 0);
    }

    const CGroundSurface* surface = scene.m_ground->GetSurface(cell->topSurfaceId);
    if (!surface || surface->textureId < 0 || static_cast<size_t>(surface->textureId) >= cache.gndTextureColors.size()) {
        return RGB(0, 0, 0);
    }

    return cache.gndTextureColors[static_cast<size_t>(surface->textureId)];
}

COLORREF ColorFromVector(const vector3d& value, float scale)
{
    const int r = static_cast<int>((std::min)(255.0f, (std::max)(0.0f, value.x * scale * 255.0f)));
    const int g = static_cast<int>((std::min)(255.0f, (std::max)(0.0f, value.y * scale * 255.0f)));
    const int b = static_cast<int>((std::min)(255.0f, (std::max)(0.0f, value.z * scale * 255.0f)));
    return RGB(r, g, b);
}

COLORREF BlendColor(COLORREF a, COLORREF b, float amount)
{
    amount = ClampUnit(amount);
    const float inv = 1.0f - amount;
    const int r = static_cast<int>(GetRValue(a) * inv + GetRValue(b) * amount);
    const int g = static_cast<int>(GetGValue(a) * inv + GetGValue(b) * amount);
    const int bl = static_cast<int>(GetBValue(a) * inv + GetBValue(b) * amount);
    return RGB(r, g, bl);
}

COLORREF MultiplyColor(COLORREF color, float brightness)
{
    brightness = (std::max)(0.0f, brightness);
    const int r = static_cast<int>((std::min)(255.0f, GetRValue(color) * brightness));
    const int g = static_cast<int>((std::min)(255.0f, GetGValue(color) * brightness));
    const int b = static_cast<int>((std::min)(255.0f, GetBValue(color) * brightness));
    return RGB(r, g, b);
}

vector3d SubtractVec3(const vector3d& a, const vector3d& b)
{
    vector3d result{};
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

vector3d CrossVec3(const vector3d& a, const vector3d& b)
{
    vector3d result{};
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

float DotVec3(const vector3d& a, const vector3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector3d NormalizeVec3(const vector3d& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSq <= 0.000001f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

COLORREF ModulateColor(COLORREF color, const vector3d& lightColor)
{
    const int r = static_cast<int>((std::min)(255.0f, GetRValue(color) * lightColor.x));
    const int g = static_cast<int>((std::min)(255.0f, GetGValue(color) * lightColor.y));
    const int b = static_cast<int>((std::min)(255.0f, GetBValue(color) * lightColor.z));
    return RGB(r, g, b);
}

vector3d ComputeLightingColor(const vector3d& lightDir, const vector3d& diffuseCol, const vector3d& ambientCol, const vector3d& normal)
{
    const vector3d normalizedLightDir = NormalizeVec3(lightDir);
    const float intensity = (std::max)(0.0f, DotVec3(NormalizeVec3(normal), normalizedLightDir));

    vector3d lit{};
    lit.x = (std::min)(1.0f, diffuseCol.x * intensity + ambientCol.x);
    lit.y = (std::min)(1.0f, diffuseCol.y * intensity + ambientCol.y);
    lit.z = (std::min)(1.0f, diffuseCol.z * intensity + ambientCol.z);
    return lit;
}

vector3d ComputeCellNormal(float h1, float h2, float h3, float h4)
{
    const vector3d p0{ 0.0f, h1, 0.0f };
    const vector3d p1{ 1.0f, h2, 0.0f };
    const vector3d p2{ 0.0f, h3, 1.0f };
    const vector3d p3{ 1.0f, h4, 1.0f };

    const vector3d n0 = CrossVec3(SubtractVec3(p1, p0), SubtractVec3(p2, p0));
    const vector3d n1 = CrossVec3(SubtractVec3(p3, p1), SubtractVec3(p2, p1));
    return NormalizeVec3(vector3d{ n0.x + n1.x, n0.y + n1.y, n0.z + n1.z });
}

POINT ProjectWorldPoint(const RECT& viewRect,
    float localX,
    float localY,
    float heightValue,
    float tileWidth,
    float tileHeight,
    float heightScale)
{
    const float centerX = (viewRect.left + viewRect.right) * 0.5f;
    const float baseY = static_cast<float>(viewRect.bottom) - 72.0f;
    POINT point{};
    point.x = static_cast<LONG>(centerX + (localX - localY) * tileWidth * 0.5f);
    point.y = static_cast<LONG>(baseY + (localX + localY) * tileHeight * 0.5f - heightValue * heightScale);
    return point;
}

void DrawGatWorld(HDC hdc, const RECT& viewRect, const CGameMode& mode, const BootstrapWorldCache& cache)
{
    const CWorld* world = mode.m_world;
    if (!world) {
        return;
    }

    const SceneGraphNode* scene = world->m_Calculated ? world->m_Calculated : &world->m_rootNode;
    const C3dGround* ground = scene->m_ground ? scene->m_ground : world->m_ground;
    const C3dAttr* attr = scene->m_attr ? scene->m_attr : world->m_attr;
    const vector3d diffuseCol = ground ? ground->m_diffuseCol : cache.worldInfo.diffuseCol;
    const vector3d ambientCol = ground ? ground->m_ambientCol : cache.worldInfo.ambientCol;
    const vector3d lightDir = ground ? ground->m_lightDir : cache.worldInfo.lightDir;
    const float waterLevel = ground ? ground->m_waterLevel : cache.worldInfo.waterLevel;

    const COLORREF skyColor = BlendColor(
        ColorFromVector(diffuseCol, 0.72f),
        ColorFromVector(ambientCol, 1.15f),
        0.45f);
    const COLORREF horizonColor = BlendColor(
        ColorFromVector(ambientCol, 0.7f),
        RGB(36, 44, 52),
        0.35f);
    const COLORREF groundBaseColor = BlendColor(
        ColorFromVector(ambientCol, 0.95f),
        RGB(110, 102, 84),
        0.55f);
    const COLORREF waterColor = BlendColor(
        ColorFromVector(diffuseCol, 0.55f),
        RGB(38, 78, 130),
        0.7f);

    HBRUSH skyBrush = CreateSolidBrush(skyColor);
    RECT skyRect = viewRect;
    skyRect.bottom = viewRect.top + (viewRect.bottom - viewRect.top) / 2;
    FillRect(hdc, &skyRect, skyBrush);
    DeleteObject(skyBrush);

    HBRUSH hazeBrush = CreateSolidBrush(horizonColor);
    RECT groundBackRect = viewRect;
    groundBackRect.top = skyRect.bottom;
    FillRect(hdc, &groundBackRect, hazeBrush);
    DeleteObject(hazeBrush);

    const int terrainWidth = ground ? ground->m_width : (attr ? attr->m_width : 0);
    const int terrainHeight = ground ? ground->m_height : (attr ? attr->m_height : 0);
    if (terrainWidth <= 0 || terrainHeight <= 0 || (!ground && !attr)) {
        return;
    }

    RECT terrainArea = ground ? scene->m_groundArea : scene->m_attrArea;
    if (terrainArea.right <= terrainArea.left || terrainArea.bottom <= terrainArea.top) {
        SetRect(&terrainArea, 0, 0, terrainWidth, terrainHeight);
    }

    SelectObject(hdc, GetStockObject(DC_PEN));
    SelectObject(hdc, GetStockObject(DC_BRUSH));

    const float tileWidth = 24.0f;
    const float tileHeight = 12.0f;
    const float heightScale = 2.2f;
    const float sceneSpanTilesX = ground && ground->m_zoom > 0.0f
        ? (scene->m_aabb.max.x - scene->m_aabb.min.x) / ground->m_zoom
        : static_cast<float>(terrainArea.right - terrainArea.left);
    const float sceneSpanTilesY = ground && ground->m_zoom > 0.0f
        ? (scene->m_aabb.max.z - scene->m_aabb.min.z) / ground->m_zoom
        : static_cast<float>(terrainArea.bottom - terrainArea.top);
    const int radius = (std::max)(8, (std::min)(18, static_cast<int>((std::min)(sceneSpanTilesX, sceneSpanTilesY) * 0.5f)));
    const int areaLeft = static_cast<int>(terrainArea.left);
    const int areaRight = static_cast<int>(terrainArea.right);
    const int areaTop = static_cast<int>(terrainArea.top);
    const int areaBottom = static_cast<int>(terrainArea.bottom);
    const int startX = (std::max)(areaLeft, g_session.m_playerPosX - radius);
    const int endX = (std::min)(areaRight - 2, g_session.m_playerPosX + radius);
    const int startY = (std::max)(areaTop, g_session.m_playerPosY - radius);
    const int endY = (std::min)(areaBottom - 2, g_session.m_playerPosY + radius);

    if (endX < startX || endY < startY) {
        return;
    }

    for (int mapY = startY; mapY <= endY; ++mapY) {
        for (int mapX = startX; mapX <= endX; ++mapX) {
            const float h1 = GetTerrainCornerHeight(*scene, mapX, mapY, 0);
            const float h2 = GetTerrainCornerHeight(*scene, mapX, mapY, 1);
            const float h3 = GetTerrainCornerHeight(*scene, mapX, mapY, 2);
            const float h4 = GetTerrainCornerHeight(*scene, mapX, mapY, 3);
            const float avgHeight = (h1 + h2 + h3 + h4) * 0.25f;
            const float relX = static_cast<float>(mapX - g_session.m_playerPosX);
            const float relY = static_cast<float>(mapY - g_session.m_playerPosY);
            const float eastHeight = GetTerrainAverageHeightAt(*scene, mapX + 1, mapY);
            const float southHeight = GetTerrainAverageHeightAt(*scene, mapX, mapY + 1);
            const float slope = (std::min)(1.0f, (std::fabs(eastHeight - avgHeight) + std::fabs(southHeight - avgHeight)) / 18.0f);

            POINT poly[4];
            poly[0] = ProjectWorldPoint(viewRect, relX, relY, h1, tileWidth, tileHeight, heightScale);
            poly[1] = ProjectWorldPoint(viewRect, relX + 1.0f, relY, h2, tileWidth, tileHeight, heightScale);
            poly[2] = ProjectWorldPoint(viewRect, relX, relY + 1.0f, h3, tileWidth, tileHeight, heightScale);
            poly[3] = ProjectWorldPoint(viewRect, relX + 1.0f, relY + 1.0f, h4, tileWidth, tileHeight, heightScale);

            const float normalizedHeight = ClampUnit((avgHeight + 20.0f) / 50.0f);
            const bool isWater = (ground || cache.worldInfo.loaded) && avgHeight <= waterLevel + 0.25f;
            COLORREF fillColor = isWater ? waterColor : groundBaseColor;
            const COLORREF gndColor = GetGroundTextureColor(*scene, cache, mapX, mapY);
            if (gndColor != RGB(0, 0, 0)) {
                fillColor = BlendColor(fillColor, gndColor, 0.6f);
            }

            const vector3d normal = ComputeCellNormal(h1, h2, h3, h4);
            const vector3d lightColor = ComputeLightingColor(lightDir, diffuseCol, ambientCol, normal);
            fillColor = ModulateColor(fillColor, lightColor);
            fillColor = MultiplyColor(fillColor, 0.82f + normalizedHeight * 0.18f - slope * 0.12f);

            if (ground) {
                if (const CGroundCell* groundCell = ground->GetCell(mapX, mapY)) {
                    if (const CGroundSurface* topSurface = ground->GetSurface(groundCell->topSurfaceId)) {
                        const COLORREF surfaceColor = RGB(
                            (topSurface->color >> 16) & 0xFF,
                            (topSurface->color >> 8) & 0xFF,
                            topSurface->color & 0xFF);
                        fillColor = BlendColor(fillColor, surfaceColor, 0.22f);
                    }
                }
            }

            const CAttrCell* attrCell = GetAttrCellSafe(*scene, mapX, mapY);
            const int attrFlag = attrCell ? attrCell->flag : 0;
            if (attrFlag != 0) {
                fillColor = BlendColor(fillColor, RGB(128, 84, 72), 0.55f);
            }

            SetDCBrushColor(hdc, fillColor);
            SetDCPenColor(hdc, BlendColor(RGB(28, 32, 26), fillColor, 0.35f));
            Polygon(hdc, poly, 4);
        }
    }

    SelectObject(hdc, GetStockObject(DC_PEN));
    SelectObject(hdc, GetStockObject(DC_BRUSH));
    for (const auto& entry : mode.m_actorPosList) {
        const u32 gid = entry.first;
        if (gid == g_session.m_gid) {
            continue;
        }

        const int mapX = entry.second.x;
        const int mapY = entry.second.y;
        if (mapX < startX || mapX > endX || mapY < startY || mapY > endY) {
            continue;
        }

        const CGameActor* actor = nullptr;
        const auto actorIt = mode.m_runtimeActors.find(gid);
        if (actorIt != mode.m_runtimeActors.end()) {
            actor = actorIt->second;
        }

        const float relX = static_cast<float>(mapX - g_session.m_playerPosX);
        const float relY = static_cast<float>(mapY - g_session.m_playerPosY);
        const float actorHeight = GetTerrainAverageHeightAt(*scene, mapX, mapY);
        const POINT actorPoint = ProjectWorldPoint(viewRect, relX + 0.5f, relY + 0.5f, actorHeight, tileWidth, tileHeight, heightScale);

        COLORREF fillColor = RGB(226, 116, 70);
        COLORREF outlineColor = RGB(72, 28, 16);
        int radiusX = 4;
        int radiusY = 7;
        if (actor && actor->m_isPc) {
            fillColor = RGB(120, 214, 255);
            outlineColor = RGB(20, 52, 78);
            radiusX = 5;
            radiusY = 8;
        }

        SetDCBrushColor(hdc, fillColor);
        SetDCPenColor(hdc, outlineColor);
        Ellipse(hdc,
            actorPoint.x - radiusX,
            actorPoint.y - radiusY,
            actorPoint.x + radiusX,
            actorPoint.y + 1);
    }

    const float playerHeight = GetTerrainAverageHeightAt(*scene, g_session.m_playerPosX, g_session.m_playerPosY);
    const POINT playerPoint = ProjectWorldPoint(viewRect, 0.5f, 0.5f, playerHeight, tileWidth, tileHeight, heightScale);

    if (g_session.m_playerJob >= 0) {
        DrawBootstrapPlayerSprite(hdc, playerPoint.x, playerPoint.y - 10);
    }

    HBRUSH markerBrush = CreateSolidBrush(RGB(255, 226, 102));
    HPEN markerPen = CreatePen(PS_SOLID, 2, RGB(64, 40, 12));
    HGDIOBJ oldPen = SelectObject(hdc, markerPen);
    HGDIOBJ oldBrush = SelectObject(hdc, markerBrush);
    Ellipse(hdc, playerPoint.x - 6, playerPoint.y - 10, playerPoint.x + 6, playerPoint.y + 2);

    POINT dirEnd = playerPoint;
    switch (g_session.m_playerDir & 7) {
    case 0: dirEnd.y -= 18; break;
    case 1: dirEnd.x += 12; dirEnd.y -= 12; break;
    case 2: dirEnd.x += 18; break;
    case 3: dirEnd.x += 12; dirEnd.y += 12; break;
    case 4: dirEnd.y += 18; break;
    case 5: dirEnd.x -= 12; dirEnd.y += 12; break;
    case 6: dirEnd.x -= 18; break;
    case 7: dirEnd.x -= 12; dirEnd.y -= 12; break;
    }
    MoveToEx(hdc, playerPoint.x, playerPoint.y - 4, nullptr);
    LineTo(hdc, dirEnd.x, dirEnd.y);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(markerBrush);
    DeleteObject(markerPen);
}
void EnsureBootstrapWorldAssets(const CGameMode& mode)
{
    BootstrapWorldCache& cache = GetBootstrapWorldCache();
    std::string mapName = mode.m_rswName[0] ? mode.m_rswName : g_session.m_curMap;
    const size_t mapDot = mapName.find_last_of('.');
    if (mapDot != std::string::npos) {
        mapName.erase(mapDot);
    }
    const bool worldAlreadyBuilt = mode.m_world
        && mode.m_world->m_attr
        && mode.m_world->m_ground;
    if (cache.mapName == mapName && worldAlreadyBuilt && IsBootstrapWorldReady(cache)) {
        return;
    }

    if (cache.mapName == mapName && !worldAlreadyBuilt && IsBootstrapWorldReady(cache)) {
        if (mode.m_world) {
            mode.m_world->ClearBackgroundObjects();
            mode.m_world->ClearFixedObjects(true);
        }
        cache.nextBackgroundActorIndex = 0;
        cache.nextFixedEffectIndex = 0;
        cache.loadStage = BootstrapLoadStage::LoadAttr;
    }

    if (cache.mapName != mapName) {
        if (mode.m_world) {
            mode.m_world->ClearBackgroundObjects();
            mode.m_world->ClearFixedObjects(true);
        }

        ResetBootstrapWorldCache(cache);
        cache.mapName = mapName;
    }

    std::string baseName = mapName;
    const size_t dot = baseName.find_last_of('.');
    if (dot != std::string::npos) {
        baseName.erase(dot);
    }

    const DWORD deadline = GetTickCount() + kBootstrapLoadBudgetMs;
    bool keepStepping = true;
    while (keepStepping) {
        keepStepping = false;

        switch (cache.loadStage) {
        case BootstrapLoadStage::ResolveWorld: {
            const std::string rswName = mode.m_rswName[0] ? mode.m_rswName : (baseName + ".rsw");
            cache.rswPath = ResolveDataPath(rswName, "rsw", {
                "",
                "data\\",
                "map\\",
                "data\\map\\"
            });
            if (!cache.rswPath.empty()) {
                cache.worldRes = g_resMgr.GetAs<C3dWorldRes>(cache.rswPath.c_str());
                if (!LoadRswWorldInfo(cache.rswPath, &cache.worldInfo)) {
                    DbgLog("[GameMode] failed to load RSW world info path='%s'; using default world lighting\n",
                        cache.rswPath.c_str());
                }
            } else {
                DbgLog("[GameMode] no RSW found for map='%s'; using default world lighting\n", cache.mapName.c_str());
            }
            ApplyBootstrapWorldLighting(cache);
            cache.loadStage = BootstrapLoadStage::LoadAttr;
            keepStepping = GetTickCount() < deadline;
            break;
        }

        case BootstrapLoadStage::LoadAttr: {
            const std::string attrOrGatName = !cache.worldInfo.attrName.empty() ? cache.worldInfo.attrName : (baseName + ".gat");
            if (cache.attrPath.empty()) {
                cache.attrPath = ResolveDataPath(attrOrGatName, "gat", {
                    "",
                    "data\\",
                    "map\\",
                    "data\\map\\"
                });
            }
            if (!cache.attrPath.empty()) {
                cache.attrRes = g_resMgr.GetAs<C3dAttr>(cache.attrPath.c_str());
            }
            if (mode.m_world) {
                mode.m_world->SetMapTerrain(cache.attrRes);
                if (cache.attrRes) {
                    mode.m_world->RebuildSceneGraph();
                }
            }
            cache.loadStage = BootstrapLoadStage::ResolveGround;
            keepStepping = GetTickCount() < deadline;
            break;
        }

        case BootstrapLoadStage::ResolveGround: {
            const std::string gndName = !cache.worldInfo.gndName.empty() ? cache.worldInfo.gndName : (baseName + ".gnd");
            if (cache.gndPath.empty()) {
                cache.gndPath = ResolveDataPath(gndName, "gnd", {
                    "",
                    "data\\",
                    "map\\",
                    "data\\map\\"
                });
            }
            cache.gndRes = cache.gndPath.empty() ? nullptr : g_resMgr.GetAs<CGndRes>(cache.gndPath.c_str());
            if (cache.gndRes && cache.gndRes->m_numTexture > 0 && cache.gndTextureColors.size() != static_cast<size_t>(cache.gndRes->m_numTexture)) {
                cache.gndTextureColors.assign(static_cast<size_t>(cache.gndRes->m_numTexture), RGB(0, 0, 0));
                cache.nextTextureColorIndex = 0;
            } else if (cache.gndRes) {
                cache.nextTextureColorIndex = static_cast<size_t>(cache.gndRes->m_numTexture);
            }
            cache.loadStage = BootstrapLoadStage::SampleGroundTextures;
            keepStepping = GetTickCount() < deadline;
            break;
        }

        case BootstrapLoadStage::SampleGroundTextures: {
            if (!cache.gndRes || cache.gndRes->m_numTexture <= 0) {
                cache.loadStage = BootstrapLoadStage::BuildGround;
                keepStepping = GetTickCount() < deadline;
                break;
            }

            size_t batchCount = 0;
            while (cache.nextTextureColorIndex < static_cast<size_t>(cache.gndRes->m_numTexture)
                && batchCount < kBootstrapTextureBatch
                && GetTickCount() < deadline) {
                const int textureIndex = static_cast<int>(cache.nextTextureColorIndex);
                const char* textureName = cache.gndRes->GetTextureName(textureIndex);
                if (textureName && *textureName) {
                    const std::string texturePath = ResolveDataPath(textureName, "bmp", {
                        "",
                        "texture\\",
                        "data\\texture\\"
                    });
                    COLORREF avgColor = RGB(0, 0, 0);
                    if (!texturePath.empty() && LoadAverageBitmapColorFromGameData(texturePath, &avgColor)) {
                        cache.gndTextureColors[cache.nextTextureColorIndex] = avgColor;
                    }
                }
                ++cache.nextTextureColorIndex;
                ++batchCount;
            }

            if (cache.nextTextureColorIndex >= static_cast<size_t>(cache.gndRes->m_numTexture)) {
                cache.loadStage = BootstrapLoadStage::BuildGround;
                keepStepping = GetTickCount() < deadline;
            }
            break;
        }

        case BootstrapLoadStage::BuildGround:
            if (cache.gndRes && mode.m_world) {
                mode.m_world->BuildGroundFromGnd(
                    *cache.gndRes,
                    cache.worldInfo.lightDir,
                    cache.worldInfo.diffuseCol,
                    cache.worldInfo.ambientCol,
                    cache.worldInfo.waterLevel,
                    cache.worldInfo.waterType,
                    cache.worldInfo.waterAnimSpeed,
                    static_cast<int>(cache.worldInfo.wavePitch),
                    static_cast<int>(cache.worldInfo.waveSpeed),
                    cache.worldInfo.waveHeight);
            }
            cache.loadStage = BootstrapLoadStage::BuildBackgroundObjects;
            keepStepping = GetTickCount() < deadline;
            break;

        case BootstrapLoadStage::BuildBackgroundObjects:
            if (cache.worldRes && mode.m_world) {
                if (!mode.m_world->BuildBackgroundObjects(
                    *cache.worldRes,
                    cache.worldInfo.lightDir,
                    cache.worldInfo.diffuseCol,
                    cache.worldInfo.ambientCol)) {
                    return;
                }
                cache.nextBackgroundActorIndex = cache.worldRes->m_3dActors.size();
                LogNearbyBackgroundActorsOnce(mode);
            }
            cache.loadStage = BootstrapLoadStage::BuildFixedEffects;
            keepStepping = GetTickCount() < deadline;
            break;

        case BootstrapLoadStage::BuildFixedEffects:
            if (cache.worldRes && mode.m_world) {
                if (!mode.m_world->BuildFixedEffects(*cache.worldRes)) {
                    return;
                }
                cache.nextFixedEffectIndex = cache.worldRes->m_particles.size();
            }
            cache.loadStage = BootstrapLoadStage::LoadMinimap;
            keepStepping = GetTickCount() < deadline;
            break;

        case BootstrapLoadStage::LoadMinimap:
            if (cache.minimapPath.empty()) {
                cache.minimapPath = ResolveDataPath(mode.m_minimapBmpName, "bmp", {
                    "",
                    "texture\\",
                    "texture\\map\\",
                    "texture\\minimap\\",
                    "texture\\interface\\map\\",
                    "texture\\interface\\minimap\\",
                    std::string(UiKorPrefix()) + "map\\",
                    std::string(UiKorPrefix()) + "minimap\\",
                    "data\\",
                    "data\\texture\\",
                    "data\\texture\\map\\",
                    "data\\texture\\minimap\\",
                    "data\\texture\\interface\\map\\",
                    "data\\texture\\interface\\minimap\\",
                    std::string("data\\") + UiKorPrefix() + "map\\",
                    std::string("data\\") + UiKorPrefix() + "minimap\\"
                });
            }
            if (!cache.minimapPath.empty() && cache.minimapPixels.empty()) {
                u32* pixels = nullptr;
                if (LoadBgraPixelsFromGameData(cache.minimapPath.c_str(), &pixels, &cache.minimapWidth, &cache.minimapHeight)
                    && pixels
                    && cache.minimapWidth > 0
                    && cache.minimapHeight > 0) {
                    cache.minimapPixels.assign(
                        pixels,
                        pixels + static_cast<size_t>(cache.minimapWidth) * static_cast<size_t>(cache.minimapHeight));
                }
                delete[] pixels;
            }
            cache.loadStage = BootstrapLoadStage::Complete;
            break;

        case BootstrapLoadStage::Complete:
        default:
            return;
        }
    }
}

void BuildMinimapName(const char* worldName, char* outName, size_t outSize)
{
    if (!outName || outSize == 0) {
        return;
    }

    outName[0] = 0;
    if (!worldName || !*worldName) {
        return;
    }

    char baseName[64] = {};
    std::strncpy(baseName, worldName, sizeof(baseName) - 1);
    char* dot = std::strrchr(baseName, '.');
    if (dot) {
        *dot = 0;
    }
    std::snprintf(outName, outSize, "%s.bmp", baseName);
}

void DrawBootstrapScene(HWND hwnd, const CGameMode& mode)
{
    if (!hwnd) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    EnsureBootstrapWorldAssets(mode);
    if (mode.m_world) {
        mode.m_world->UpdateCalculatedNodeForTile(g_session.m_playerPosX, g_session.m_playerPosY);
    }
    const BootstrapWorldCache& cache = GetBootstrapWorldCache();

    static ArgbDibSurface s_bootstrapComposeSurface;
    if (!s_bootstrapComposeSurface.EnsureSize(width, height)) {
        return;
    }
    void* dibBits = s_bootstrapComposeSurface.GetBits();
    if (!dibBits) {
        return;
    }

    RECT worldRect{ 24, 64, width - 220, height - 36 };
    RECT mapRect{ width - 184, 20, width - 20, 184 };
    int markerX = (mapRect.left + mapRect.right) / 2;
    int markerY = (mapRect.top + mapRect.bottom) / 2;
    const int attrWidth = mode.m_world ? mode.m_world->m_rootNode.m_attrArea.right - mode.m_world->m_rootNode.m_attrArea.left : 0;
    const int attrHeight = mode.m_world ? mode.m_world->m_rootNode.m_attrArea.bottom - mode.m_world->m_rootNode.m_attrArea.top : 0;
    if (attrWidth > 0 && attrHeight > 0) {
        const float normX = static_cast<float>(g_session.m_playerPosX) / static_cast<float>(attrWidth);
        const float normY = 1.0f - (static_cast<float>(g_session.m_playerPosY) / static_cast<float>(attrHeight));
        markerX = mapRect.left + static_cast<int>((mapRect.right - mapRect.left) * (std::max)(0.0f, (std::min)(1.0f, normX)));
        markerY = mapRect.top + static_cast<int>((mapRect.bottom - mapRect.top) * (std::max)(0.0f, (std::min)(1.0f, normY)));
    }

    const int groundWidth = mode.m_world ? mode.m_world->m_rootNode.m_groundArea.right - mode.m_world->m_rootNode.m_groundArea.left : 0;
    const int groundHeight = mode.m_world ? mode.m_world->m_rootNode.m_groundArea.bottom - mode.m_world->m_rootNode.m_groundArea.top : 0;
    const SceneGraphNode* scene = mode.m_world && mode.m_world->m_Calculated ? mode.m_world->m_Calculated : (mode.m_world ? &mode.m_world->m_rootNode : nullptr);
    const int activeWidth = scene ? scene->m_groundArea.right - scene->m_groundArea.left : 0;
    const int activeHeight = scene ? scene->m_groundArea.bottom - scene->m_groundArea.top : 0;

#if RO_ENABLE_QT6_UI
    unsigned int* pixels = s_bootstrapComposeSurface.GetPixels();
    if (!pixels) {
        return;
    }
    std::fill_n(pixels, static_cast<size_t>(width) * static_cast<size_t>(height), 0xFF151B24u);
    DrawGatWorldToArgb(pixels, width, height, worldRect, mode, cache);

    QImage image(reinterpret_cast<uchar*>(pixels), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setPen(Qt::white);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(worldRect.left, worldRect.top, worldRect.right - worldRect.left - 1, worldRect.bottom - worldRect.top - 1));

    RECT fittedMapRect = mapRect;
    if (!cache.minimapPixels.empty() && cache.minimapWidth > 0 && cache.minimapHeight > 0) {
        const float mapAspect = static_cast<float>(cache.minimapWidth) / static_cast<float>(cache.minimapHeight);
        const float viewAspect = static_cast<float>(mapRect.right - mapRect.left) / static_cast<float>(mapRect.bottom - mapRect.top);
        if (mapAspect > viewAspect) {
            const int fittedHeight = static_cast<int>((mapRect.right - mapRect.left) / mapAspect);
            const int pad = ((mapRect.bottom - mapRect.top) - fittedHeight) / 2;
            fittedMapRect.top += pad;
            fittedMapRect.bottom = fittedMapRect.top + fittedHeight;
        } else {
            const int fittedWidth = static_cast<int>((mapRect.bottom - mapRect.top) * mapAspect);
            const int pad = ((mapRect.right - mapRect.left) - fittedWidth) / 2;
            fittedMapRect.left += pad;
            fittedMapRect.right = fittedMapRect.left + fittedWidth;
        }

        DrawPixelsStretchedQt(painter, cache.minimapPixels.data(), cache.minimapWidth, cache.minimapHeight, fittedMapRect);
    } else {
        painter.fillRect(QRect(mapRect.left, mapRect.top, mapRect.right - mapRect.left, mapRect.bottom - mapRect.top), ToQColor(RGB(62, 88, 52)));
        painter.setPen(ToQColor(RGB(118, 150, 96)));
        for (int i = 1; i < 12; ++i) {
            const int x = mapRect.left + ((mapRect.right - mapRect.left) * i) / 12;
            painter.drawLine(x, mapRect.top, x, mapRect.bottom);
        }
        for (int i = 1; i < 8; ++i) {
            const int y = mapRect.top + ((mapRect.bottom - mapRect.top) * i) / 8;
            painter.drawLine(mapRect.left, y, mapRect.right, y);
        }
    }

    painter.setPen(Qt::white);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(mapRect.left, mapRect.top, mapRect.right - mapRect.left - 1, mapRect.bottom - mapRect.top - 1));

    painter.setBrush(ToQColor(RGB(245, 224, 126)));
    painter.setPen(QPen(ToQColor(RGB(92, 60, 16)), 2));
    painter.drawEllipse(QRect(markerX - 8, markerY - 8, 16, 16));
    painter.drawLine(markerX, markerY - 14, markerX + (g_session.m_playerDir == 0 ? 0 : 8), markerY + (g_session.m_playerDir == 0 ? -18 : 0));
    painter.end();

    DrawBootstrapSceneTextQt(
        pixels,
        width,
        height,
        mode,
        cache,
        attrWidth,
        attrHeight,
        groundWidth,
        groundHeight,
        activeWidth,
        activeHeight);
#else
    HDC memDC = s_bootstrapComposeSurface.GetDC();
    if (!memDC) {
        return;
    }
    RECT fullRect{ 0, 0, width, height };
    HBRUSH bgBrush = CreateSolidBrush(RGB(21, 27, 36));
    FillRect(memDC, &fullRect, bgBrush);
    DeleteObject(bgBrush);

    DrawGatWorld(memDC, worldRect, mode, cache);
    FrameRect(memDC, &worldRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    if (!cache.minimapPixels.empty() && cache.minimapWidth > 0 && cache.minimapHeight > 0) {
        const float mapAspect = static_cast<float>(cache.minimapWidth) / static_cast<float>(cache.minimapHeight);
        const float viewAspect = static_cast<float>(mapRect.right - mapRect.left) / static_cast<float>(mapRect.bottom - mapRect.top);
        if (mapAspect > viewAspect) {
            const int fittedHeight = static_cast<int>((mapRect.right - mapRect.left) / mapAspect);
            const int pad = ((mapRect.bottom - mapRect.top) - fittedHeight) / 2;
            mapRect.top += pad;
            mapRect.bottom = mapRect.top + fittedHeight;
        } else {
            const int fittedWidth = static_cast<int>((mapRect.bottom - mapRect.top) * mapAspect);
            const int pad = ((mapRect.right - mapRect.left) - fittedWidth) / 2;
            mapRect.left += pad;
            mapRect.right = mapRect.left + fittedWidth;
        }

        DrawPixelsStretched(
            memDC,
            cache.minimapPixels.data(),
            cache.minimapWidth,
            cache.minimapHeight,
            mapRect);
    } else {
        HBRUSH fallbackBrush = CreateSolidBrush(RGB(62, 88, 52));
        FillRect(memDC, &mapRect, fallbackBrush);
        DeleteObject(fallbackBrush);

        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(118, 150, 96));
        HGDIOBJ oldPen = SelectObject(memDC, gridPen);
        for (int i = 1; i < 12; ++i) {
            const int x = mapRect.left + ((mapRect.right - mapRect.left) * i) / 12;
            MoveToEx(memDC, x, mapRect.top, nullptr);
            LineTo(memDC, x, mapRect.bottom);
        }
        for (int i = 1; i < 8; ++i) {
            const int y = mapRect.top + ((mapRect.bottom - mapRect.top) * i) / 8;
            MoveToEx(memDC, mapRect.left, y, nullptr);
            LineTo(memDC, mapRect.right, y);
        }
        SelectObject(memDC, oldPen);
        DeleteObject(gridPen);
    }

    FrameRect(memDC, &mapRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    HBRUSH markerBrush = CreateSolidBrush(RGB(245, 224, 126));
    HPEN markerPen = CreatePen(PS_SOLID, 2, RGB(92, 60, 16));
    HGDIOBJ oldPen = SelectObject(memDC, markerPen);
    HGDIOBJ oldBrush = SelectObject(memDC, markerBrush);
    Ellipse(memDC, markerX - 8, markerY - 8, markerX + 8, markerY + 8);
    MoveToEx(memDC, markerX, markerY - 14, nullptr);
    LineTo(memDC, markerX + (g_session.m_playerDir == 0 ? 0 : 8), markerY + (g_session.m_playerDir == 0 ? -18 : 0));
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(markerBrush);
    DeleteObject(markerPen);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));

    char header[128];
    std::snprintf(header, sizeof(header), "Map: %s   Pos: %d, %d   Dir: %d",
        g_session.m_curMap[0] ? g_session.m_curMap : "(unknown)",
        g_session.m_playerPosX,
        g_session.m_playerPosY,
        g_session.m_playerDir);
    RECT headerRect{ 20, 18, width - 200, 42 };
    DrawTextA(memDC, header, -1, &headerRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    char subHeader[160];
    std::snprintf(subHeader, sizeof(subHeader), "World bootstrap active: %s", mode.m_rswName[0] ? mode.m_rswName : "(no world name)");
    RECT subHeaderRect{ 20, 40, width - 200, 62 };
    DrawTextA(memDC, subHeader, -1, &subHeaderRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    char assetInfo[256];
    std::snprintf(assetInfo, sizeof(assetInfo), "MiniMap %dx%d   GAT %dx%d   GND %dx%d   Node %dx%d   Actors %zu/%zu",
        cache.minimapWidth,
        cache.minimapHeight,
        attrWidth,
        attrHeight,
        groundWidth,
        groundHeight,
        activeWidth,
        activeHeight,
        mode.m_actorPosList.size(),
        mode.m_runtimeActors.size());
    RECT assetRect{ width - 184, 188, width - 20, 206 };
    DrawTextA(memDC, assetInfo, -1, &assetRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    RECT hintRect{ 20, height - 30, width - 20, height - 12 };
    DrawTextA(memDC,
        mode.m_world && mode.m_world->m_ground && mode.m_world->m_attr
            ? "Connected to world server. Ref-style bootstrap now renders from the world ground and attr objects before full scene/view loading."
            : "Connected to world server. Ref-style world bootstrap is active, but the ground or attr resource is still incomplete.",
        -1,
        &hintRect,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER);
#endif

    BlitArgbBitsToWindow(hwnd, dibBits, width, height);
}
}

namespace {
void ResetCompanionActorOrders(CWorld* world, u32 gid, companion_ai::Kind kind)
{
    if (world && gid) {
        if (auto* actor = world->GetGameActorByAID(gid)) actor->ResetCompanionOrders(kind);
    }
}
}

bool CGameMode::RequestAuctionSearch(const AuctionSearchQuery& query)
{
    if (!m_auctionSearch.open || m_auctionSearch.awaitingReply
        || m_auctionTransaction != AuctionTransaction::None) return false;

    const auto type = static_cast<u16>(query.kind);
    const bool ownList = query.kind == AuctionSearchKind::OwnSales || query.kind == AuctionSearchKind::OwnBids;
    if (type > static_cast<u16>(AuctionSearchKind::OwnBids) || query.page == 0 || query.page > 32767
        || (ownList && query.page != 1) || query.name.size() > 23 || query.name.find('\0') != std::string::npos
        || query.maximumPrice > 0x7fffffffu) return false;

    auto* connection = CRagConnection::instance();
    if (ownList) {
        const u8 packet[] = {0x5c, 0x02, static_cast<u8>(query.kind == AuctionSearchKind::OwnBids), 0};
        if (!connection->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet))) return false;
    } else {
        std::array<u8, 34> packet{0x51, 0x02, static_cast<u8>(type), 0};
        if (query.kind == AuctionSearchKind::Price) {
            for (std::size_t i = 0; i < 4; ++i) {
                packet[4 + i] = static_cast<u8>(query.maximumPrice >> (8 * i));
            }
        }
        if (query.kind == AuctionSearchKind::Name) {
            std::copy(query.name.begin(), query.name.end(), packet.begin() + 8);
        }
        packet[32] = static_cast<u8>(query.page);
        packet[33] = static_cast<u8>(query.page >> 8);
        if (!connection->SendPacket(reinterpret_cast<const char*>(packet.data()), packet.size())) return false;
    }

    m_auctionSearch.query = query;
    m_auctionSearch.awaitingReply = true;
    // Both selected search parsers cancel the server's staged item.
    m_auctionStaging.stagedIndex = 0;
    m_auctionStaging.stagedItemId = 0;
    m_auctionStaging.discardReply = m_auctionStaging.pendingIndex != 0;
    return true;
}

void CGameMode::CloseAuctionSearch()
{
    m_auctionStaging.stagedIndex = 0;
    m_auctionStaging.stagedItemId = 0;
    m_auctionStaging.discardReply = m_auctionStaging.pendingIndex != 0;
    // The search reply has no request ID, so drain any outstanding page before reuse.
    const bool awaitingReply = m_auctionSearch.awaitingReply;
    m_auctionSearch = {};
    m_auctionSearch.awaitingReply = awaitingReply;
    m_auctionSearch.discardReply = awaitingReply;
}

bool CGameMode::RequestAuctionStageItem(u16 inventoryIndex)
{
    if (!m_auctionSearch.open || m_auctionStaging.pendingIndex || m_auctionStaging.stagedIndex
        || m_auctionTransaction != AuctionTransaction::None
        || inventoryIndex - ro::item_snapshot::kFirstInventoryIndex >= ro::item_snapshot::kInventorySlots) return false;

    const auto* item = g_session.GetInventoryItemByIndex(inventoryIndex);
    if (!item || !item->GetItemId() || item->m_num < 1 || !item->m_isIdentified || item->m_wearLocation
        || item->m_deleteTime) return false;

    const u8 packet[] = {0x4c, 0x02, static_cast<u8>(inventoryIndex), static_cast<u8>(inventoryIndex >> 8), 1, 0, 0, 0};
    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet))) return false;

    m_auctionStaging.pendingIndex = inventoryIndex;
    m_auctionStaging.pendingItemId = item->GetItemId();
    return true;
}

bool CGameMode::CancelAuctionStaging()
{
    auto& staging = m_auctionStaging;
    if (!staging.stagedIndex && (!staging.pendingIndex || staging.discardReply)) return true;

    const u8 packet[] = {0x4b, 0x02, 1, 0};
    const bool sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet));
    if (sent) {
        staging.stagedIndex = 0;
        staging.stagedItemId = 0;
        staging.discardReply = staging.pendingIndex != 0;
    }
    return sent;
}

bool CGameMode::ApplyAuctionStageResult(u16 index, u8 result)
{
    auto& staging = m_auctionStaging;
    if (!staging.pendingIndex || result > 1
        || (index != staging.pendingIndex && (result == 0 || index != staging.pendingIndex - 2))) return false;

    bool inventoryChanged = false;
    if (result == 0) {
        const auto* item = g_session.GetInventoryItemByIndex(staging.pendingIndex);
        if (item && item->GetItemId() == staging.pendingItemId && item->m_num > 0) {
            if (m_auctionSearch.open && !staging.discardReply) {
                staging.stagedIndex = staging.pendingIndex;
                staging.stagedItemId = staging.pendingItemId;
            }
            // Registration suppresses its deletion packet. Cancel instead returns
            // the reserved unit with the ordinary item pickup acknowledgement.
            g_session.RemoveInventoryItem(staging.pendingIndex, 1);
            inventoryChanged = true;
        } else if (m_auctionSearch.open && !staging.discardReply) {
            CancelAuctionStaging();
        }
    }
    staging.pendingIndex = 0;
    staging.pendingItemId = 0;
    staging.discardReply = false;
    return inventoryChanged;
}

bool CGameMode::RequestAuctionRegistration(u32 startingPrice, u32 buyNow, u16 hours)
{
    if (!m_auctionSearch.open || m_auctionSearch.awaitingReply || !m_auctionStaging.stagedIndex
        || m_auctionTransaction != AuctionTransaction::None || !startingPrice || startingPrice >= buyNow
        || buyNow > 0x7fffffffu || hours < 1 || hours > 48) return false;

    std::array<u8, 12> packet{0x4d, 2};
    for (std::size_t i = 0; i < 4; ++i) {
        packet[2 + i] = static_cast<u8>(startingPrice >> (8 * i));
        packet[6 + i] = static_cast<u8>(buyNow >> (8 * i));
    }
    packet[10] = static_cast<u8>(hours);
    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), packet.size())) return false;
    m_auctionTransaction = AuctionTransaction::Register;
    return true;
}

bool CGameMode::RequestAuctionTransaction(AuctionTransaction action, const AuctionSearchRow& quote, u32 bid)
{
    if (!m_auctionSearch.open || m_auctionSearch.awaitingReply || m_auctionStaging.pendingIndex
        || m_auctionStaging.stagedIndex || m_auctionTransaction != AuctionTransaction::None || !quote.auctionId) return false;

    const auto& rows = m_auctionSearch.rows;
    const auto current = std::find_if(rows.begin(), rows.end(), [&](const AuctionSearchRow& row) {
        return row.auctionId == quote.auctionId && row.seller == quote.seller && row.buyer == quote.buyer
            && row.price == quote.price && row.buyNow == quote.buyNow;
    });
    if (current == rows.end()) return false;
    const bool ownsSale = current->seller == g_session.GetPlayerName();
    u8 opcode;
    int length = 6;
    switch (action) {
    case AuctionTransaction::Bid:
        if (ownsSale || bid <= current->price || bid > 0x7fffffffu) return false;
        opcode = 0x4f;
        length = 10;
        break;
    case AuctionTransaction::Cancel:
        if (!ownsSale || !current->buyer.empty()) return false;
        opcode = 0x4e;
        break;
    case AuctionTransaction::Stop:
        if (!ownsSale || current->buyer.empty()) return false;
        opcode = 0x5d;
        break;
    default: return false;
    }

    std::array<u8, 10> packet{opcode, 2};
    for (std::size_t i = 0; i < 4; ++i) {
        packet[2 + i] = static_cast<u8>(quote.auctionId >> (8 * i));
        packet[6 + i] = static_cast<u8>(bid >> (8 * i));
    }
    if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), length)) return false;
    m_auctionTransaction = action;
    return true;
}

void CGameMode::ApplyAuctionTransactionResult(u16 opcode, u16 result)
{
    using Transaction = AuctionTransaction;
    const auto action = m_auctionTransaction;
    const bool closeReply = opcode == 0x025D;
    if (closeReply) {
        if (action != Transaction::Cancel && action != Transaction::Stop) return;
    } else {
        switch (action) {
        case Transaction::Register: if (result != 1 && result != 2 && result != 4 && result != 5) return; break;
        case Transaction::Bid: if (result != 0 && result != 1 && result != 8 && result != 9) return; break;
        case Transaction::Cancel: if (result != 2 && result != 3) return; break;
        default: return;
        }
    }

    m_auctionTransaction = Transaction::None;
    if (action == Transaction::Register) {
        if (result == 1) m_auctionStaging = {};
        else CancelAuctionStaging();
    }
    // The selected map server pushes a fresh own list after a successful bid or stop.
    if ((action == Transaction::Bid && result == 1) || (action == Transaction::Stop && result == 0)) {
        m_auctionSearch.awaitingReply = true;
        m_auctionSearch.discardReply = !m_auctionSearch.open;
        m_auctionSearch.query = {action == Transaction::Bid ? AuctionSearchKind::OwnBids : AuctionSearchKind::OwnSales};
    } else if (m_auctionSearch.open && (action == Transaction::Register
        || (action == Transaction::Cancel && !closeReply && result == 2))) {
        RequestAuctionSearch({AuctionSearchKind::OwnSales});
    }
}

void CGameMode::ResetBattlegroundMapState()
{
    g_session.m_isBattleChat = false;
    m_battlegroundCamp = 0;
    m_battlegroundScoreA = 0;
    m_battlegroundScoreB = 0;
    m_battlegroundPosList.clear();
    if (m_world && m_world->m_player) {
        m_world->m_player->m_camp = 0;
    }
}

void CGameMode::ResetCompanionMapState()
{
    ResetCompanionActorOrders(m_world, g_session.m_homunGID, companion_ai::Kind::Homunculus);
    ResetCompanionActorOrders(m_world, g_session.m_merGID, companion_ai::Kind::Mercenary);
    g_session.ResetCompanionMapState();
}

void CGameMode::DeactivateHomunculus()
{
    ResetCompanionActorOrders(m_world, g_session.m_homunGID, companion_ai::Kind::Homunculus);
    g_session.m_homunTarget = {};
    g_session.m_homunStatus.alive = false;
    g_windowMgr.CloseCompanionWindows(true);
}

void CGameMode::RetireMercenaryContract()
{
    ResetCompanionActorOrders(m_world, g_session.m_merGID, companion_ai::Kind::Mercenary);
    g_session.m_merGID = 0;
    g_session.m_merStatus = {};
    g_session.m_merTarget = {};
    g_session.ClearMercSkillItems();
    g_windowMgr.CloseCompanionWindows(false);
    if (SKILL_INFO::IsMercenarySkill(m_skillUseInfo.id)) {
        SendMsg(78, 0, 0, 0);
    }
}

void CGameMode::SynchronizePlayerSkillProceed(CPlayer* player,bool retire)
{
    if(!m_world || m_world->m_player!=player || !player)return;
    if(player->m_proceedType.type==3){
        if(m_skillChaseTargetGid!=player->m_proceedTargetGid || m_skillChaseSkillId!=player->m_skillId)
            m_lastSkillChaseRequestTick=0;
        m_skillChaseTargetGid=player->m_proceedTargetGid;m_skillChaseSkillId=player->m_skillId;
        m_skillChaseSkillLevel=player->m_skillUseLevel;m_skillChaseRange=player->m_skillAttackRange;
        m_hasSkillChase=1;m_hasGroundSkillChase=0;m_hasTouchSkillChase=0;
        m_lastMonGid=0;m_lastLockOnMonGid=0;ClearAttackChaseHint(*this);ClearPickupIntent(*this);
    }else if(player->m_proceedType.type==4){
        m_groundSkillCellX=player->m_gSkillDx;m_groundSkillCellY=player->m_gSkillDy;
        m_groundSkillId=player->m_skillId;m_groundSkillLevel=player->m_skillUseLevel;m_groundSkillRange=player->m_skillAttackRange;
        m_hasSkillChase=1;m_hasGroundSkillChase=1;m_hasTouchSkillChase=0;
    }else if(player->m_proceedType.type==6){
        m_touchSkillTargetAid=player->m_proceedTargetGid;m_touchSkillId=player->m_skillId;
        m_touchSkillLevel=player->m_skillUseLevel;m_touchSkillRange=player->m_skillAttackRange;
        m_hasSkillChase=1;m_hasTouchSkillChase=1;m_hasGroundSkillChase=0;
        m_lastMonGid=0;m_lastLockOnMonGid=0;
    }else if(retire){
        ClearSkillChase(*this);m_lastMonGid=0;m_lastLockOnMonGid=0;ClearAttackChaseHint(*this);
    }
}

#if defined(RO_PACKET_REPLAY_TESTS)
std::string ReplayResolveHoveredActorName(CGameMode& mode, CGameActor& actor)
{
    return ResolveHoveredActorName(mode, &actor);
}

void ReplayAdvanceMapLoading(CGameMode& mode, u32 now)
{
    AdvanceMapLoading(mode, now);
}

bool ReplayTryRequestAttackOnActor(CGameMode& mode, CGameActor* actor, bool shift, bool alt, bool control)
{
    return TryRequestAttackOnActor(mode, actor, shift, alt, control);
}

bool ReplayTryPetCaptureOnActor(CGameMode& mode,CGameActor* target,bool shift,bool alt,bool control)
{
    return TryRequestPetCaptureOnActor(mode,target,shift,alt,control);
}
bool ReplayTrySourceSkillOnActor(CGameMode& mode,CGameActor* target,bool shift,bool alt,bool control)
{
    return TryRequestSourceSkillOnActor(mode,target,shift,alt,control);
}
bool ReplayRequestPendingGroundSkill(CGameMode& mode, int x, int y, bool shift)
{
    return RequestPendingGroundSkill(mode, x, y, shift);
}
void ReplayPumpSkillChase(CGameMode& mode){PumpSkillChaseRequest(mode);}
void ReplayClearSkillChase(CGameMode& mode){ClearSkillChase(mode);}
bool ReplaySendMapMoveRequestPacket(int dstX, int dstY)
{
    return SendMoveRequestPacket(dstX, dstY);
}
bool ReplayRequestUseInventoryItem(u16 itemIndex)
{
    return RequestUseInventoryItem(itemIndex);
}
bool ReplaySendUseSkillToIdPacket(u16 skillId, u16 skillLevel, u32 targetGid)
{
    return SendUseSkillToIdPacket(skillId, skillLevel, targetGid);
}
bool ReplaySendTakeItemRequestPacket(u32 objectAid)
{
    return SendTakeItemRequestPacket(objectAid);
}
bool ReplaySendChangeDirectionPacket(u8 headDir, u8 dir)
{
    return SendChangeDirRequestPacket(headDir, dir);
}
bool ReplayRequestCartOffPacket()
{
    return RequestCartOff();
}
bool ReplayRequestNpcCloseDialog(u32 npcId)
{
    return RequestNpcCloseDialog(npcId);
}
bool ReplayRequestIncreaseStatus(u16 statusId)
{
    return RequestIncreaseStatus(statusId);
}
bool ReplayRequestNpcNext(u32 npcId)
{
    return RequestNpcNext(npcId);
}
bool ReplayRequestNpcInputNumber(u32 npcId, u32 value)
{
    return RequestNpcInputNumber(npcId, value);
}
bool ReplayRequestNpcInputString(u32 npcId, const char* text)
{
    return RequestNpcInputString(npcId, text);
}
bool ReplayRequestNpcContact(u32 npcId)
{
    return RequestNpcContact(npcId);
}
bool ReplayRequestNpcMenuSelection(u32 npcId, u8 choice)
{
    return RequestNpcMenuSelection(npcId, choice);
}
bool ReplayRequestReturnToSavePoint()
{
    return RequestReturnToSavePoint();
}
bool ReplayRequestUpgradeSkillLevel(u16 skillId)
{
    return RequestUpgradeSkillLevel(skillId);
}
bool ReplayRequestIdentifyInventoryItem(s16 itemIndex)
{
    return RequestIdentifyInventoryItem(itemIndex);
}
bool ReplayRequestShortcutSlotUse(int slot){return RequestShortcutSlotUse(slot);}
bool ReplayHasPendingSkillUse(const CGameMode& mode){return HasPendingSkillUse(mode);}
bool ReplayRequestShortcutSlotUpdate(int absoluteSlotIndex)
{
    return RequestShortcutSlotUpdate(absoluteSlotIndex);
}
bool ReplayRequestCloseStorage()
{
    return RequestCloseStorage();
}
bool ReplaySendGlobalChatMessage(const char* playerName, const std::string& message)
{
    return SendGlobalChatMessage(playerName, message);
}
#endif

bool ArmPendingSkillUseFromSkillList(int skillId,int family)
{
    const auto* skill=family==0?g_session.GetSkillItemBySkillId(skillId)
        :family==1?g_session.GetHomunSkillItemBySkillId(skillId):family==2?g_session.GetMercSkillItemBySkillId(skillId):nullptr;
    if(!skill || !skill->m_isValid || !skill->level || !skill->spcost)return false;
    auto* mode=g_modeMgr.GetCurrentGameMode();if(!mode)return false;
    const int level=family==0?g_session.GetSkillUseLevel(skillId)
        :family==1?g_session.GetHomunSkillUseLevel(skillId):g_session.GetMerSkillUseLevel(skillId);
    const PLAYER_SKILL_INFO snapshot=*skill;
    mode->SendMsg(126,reinterpret_cast<msgparam_t>(&snapshot),level,0);
    return true;
}

bool ArmPendingSkillUseFromSkillList(int skillId)
{
    return ArmPendingSkillUseFromSkillList(skillId,0);
}

bool CGameMode::SendSkillToGroundWithText(u16 skillId, u16 skillLevel, int cellX, int cellY, const std::string& contents)
{
    return SendUseSkillToPosInfoPacket(skillId, skillLevel, cellX, cellY, contents);
}

bool CGameMode::SendWarpMapSelection(const std::string& mapName)
{
    if (m_warpSkillId == 0 || mapName.empty() || mapName.size() >= sizeof(PACKET_CZ_USESKILLMAP::MapName)
        || std::find(m_warpMapDestinations.begin(), m_warpMapDestinations.end(), mapName) == m_warpMapDestinations.end()) {
        return false;
    }

    if (!SendUseSkillMapPacket(m_warpSkillId, mapName.c_str())) {
        return false;
    }

    m_warpSkillId = 0;
    m_warpMapDestinations.clear();
    return true;
}

bool CGameMode::CancelWarpMapSelection()
{
    if (m_warpSkillId == 0) {
        return false;
    }
    if (!SendUseSkillMapPacket(m_warpSkillId, "cancel")) {
        return false;
    }
    m_warpSkillId = 0;
    m_warpMapDestinations.clear();
    return true;
}

bool CGameMode::SendMakingArrowSelection(u16 itemId)
{
    struct MakingArrowPacket {
        u16 packetType;
        u16 itemId;
    };
    static_assert(sizeof(MakingArrowPacket) == 4, "CZ_REQ_MAKINGARROW is 4 bytes at packet_ver 23");

    MakingArrowPacket packet{};
    packet.packetType = 0x01AE;
    packet.itemId = itemId;
    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] makingarrow itemId=%u sent=%d\n",
        static_cast<unsigned int>(itemId),
        sent ? 1 : 0);
    return sent;
}

bool CGameMode::SendProductionSelection(u16 itemId, const std::array<u16, 3>& materials)
{
    const std::array<u16, 5> packet{0x018e, itemId, materials[0], materials[1], materials[2]};
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), sizeof(packet));
}

bool CGameMode::SendCookingSelection(u16 makeType, u16 itemId)
{
    const std::array<u16, 3> packet{0x025b, makeType, itemId};
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), sizeof(packet));
}

bool CGameMode::SendWeaponRefineSelection(u32 itemIndex)
{
    const std::array<u8, 6> packet{0x22, 0x02, static_cast<u8>(itemIndex), static_cast<u8>(itemIndex >> 8),
        static_cast<u8>(itemIndex >> 16), static_cast<u8>(itemIndex >> 24)};
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), packet.size());
}

bool CGameMode::SendItemRepairSelection(const std::array<u8, 13>& row)
{
    std::array<u8, 15> packet{0xfd, 0x01};
    std::copy(row.begin(), row.end(), packet.begin() + 2);
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), packet.size());
}

void CGameMode::ClearCraftingSelection()
{
    if (g_windowMgr.m_skillSelectWnd && g_windowMgr.m_skillSelectWnd->IsCraftingSelection()) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_skillSelectWnd);
    }
    if (auto* box = g_windowMgr.m_roomMessageBox; box && box->GetMessageId() == 424) box->Close();
}

void CGameMode::ClearItemComposition()
{
    m_lastCardItemIndex = 0;
    m_pendingItemComposition = {};
    if (auto* window = g_windowMgr.m_itemCompositionWnd) g_windowMgr.DeleteWindow(window);
}

void CGameMode::ClearItemIdentification()
{
    m_pendingIdentifyIndex = m_pendingIdentifyItemId = 0;
    if (auto* window = g_windowMgr.m_itemIdentifyWnd) g_windowMgr.DeleteWindow(window);
}

bool CGameMode::SendTimeSyncRequest(bool syncNow)
{
    return ::SendTimeSyncRequest(*this, syncNow);
}

bool CGameMode::SendStarPlaceAcceptance(u8 which)
{
    const auto* window = g_windowMgr.m_skillSelectWnd;
    if (!window || !window->IsStarPlaceConfirmation(which)) {
        return false;
    }
    const std::array<u8, 3> packet = {0x54, 0x02, which};
    return CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()));
}

bool CGameMode::SendAutoSpellSelection(u32 skillId, const std::vector<unsigned int>& offeredSkillIds)
{
    if (skillId == 0 || skillId > 0xFFFFu
        || std::find(offeredSkillIds.begin(), offeredSkillIds.end(), skillId) == offeredSkillIds.end()) {
        return false;
    }

    const std::array<u8, 6> packet = {
        0xCE, 0x01,
        static_cast<u8>(skillId),
        static_cast<u8>(skillId >> 8),
        0, 0,
    };
    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packet.data()),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] autospell skillId=%u sent=%d\n",
        static_cast<unsigned int>(skillId),
        sent ? 1 : 0);
    return sent;
}

bool CGameMode::ResolveCursorDrawPosition(int rawX, int rawY, int* drawX, int* drawY) const
{
    if (!m_world || !m_view || m_mapLoadingStage != MapLoading_None
        || m_canRotateView || m_isLeftButtonHeld || m_dragType != 0
        || IsWorldHoverBlockedByUi(rawX, rawY)) return false;
    const bool pendingSkill = HasPendingSkillUse(*this);
    if (pendingSkill) {
        const auto useMode = ResolveShortcutSkillUseMode(static_cast<u16>(m_skillUseInfo.id & 0xFFFF));
        if (useMode != ShortcutSkillUseMode::ActorTarget) return false;
    }
    // Ref ProcessMouseSnap changes the drawn cursor only. Never feed this
    // coordinate back into physical mouse movement, ground aiming or UI input.
    return m_world->ResolveCursorSnap(m_view->GetViewMatrix(), m_view->GetCameraLongitude(),
        rawX, rawY, !pendingSkill, drawX, drawY);
}

std::string CGameMode::ResolveCharacterName(u32 characterId)
{
    if (characterId == 0) return {};
    const auto cached = m_characterNameCache.find(characterId);
    if (cached != m_characterNameCache.end() && !cached->second.empty()) return cached->second;

    const u32 now = GetTickCount();
    const auto requested = m_characterNameReqTimer.find(characterId);
    if (requested != m_characterNameReqTimer.end() && now - requested->second < 2500u) return {};
    unsigned char packet[32]{};
    int length = 0;
    if (ro::net::BuildActiveCharacterNameRequestPacket(characterId, packet, sizeof(packet), &length)
        && CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), length)) {
        // A failed send must not suppress a later successful lookup.
        m_characterNameReqTimer[characterId] = now;
    }
    return {};
}

CGameMode::CGameMode() 
    : m_areaLeft(0), m_areaRight(0), m_areaTop(0), m_areaBottom(0),
      m_world(nullptr), m_view(nullptr), m_mousePointer(nullptr),
    m_leftBtnClickTick(0), m_oldMouseX(0), m_oldMouseY(0), m_rBtnClickX(0), m_rBtnClickY(0),
    m_lastRButtonClickTick(0), m_lastRButtonClickX(0), m_lastRButtonClickY(0), m_rButtonDragged(0),
            m_lastPcGid(0), m_lastMonGid(0), m_lastLockOnMonGid(0), m_selectedPcTargetGid(0),
      m_isAutoMoveClickOn(0), m_isWaitingWhisperSetting(0), m_isWaitingEnterRoom(0),
      m_isWaitingAddExchangeItem(0), m_waitingWearEquipAck(0), m_waitingTakeoffEquipAck(0),
      m_isReqUpgradeSkillLevel(0), m_exchangeItemCnt(0), m_isWaitingCancelExchangeItem(0),
      m_lastEmblemReqTick(0), m_lastNameWaitingListTick(0), 
      m_noMove(0), m_noMoveStartTick(0), m_isOnQuest(0), m_isPlayerDead(0),
      m_canRotateView(0), m_hasViewPoint(0), m_receiveSyneRequestTime(0),
      m_syncRequestTime(0), m_usedCachesUnloadTick(0), m_reqEmotionTick(0),
      m_reqTickChatRoom(0), m_reqTickMerchantShop(0), m_isReqEmotion(0),
      m_fixedLongitude(0.0f), m_lastCouplePacketAid(0), m_lastCouplePacketGid(0),
      m_nameBalloon(nullptr), m_targetNameBalloon(nullptr), m_starSkillNotice(nullptr), m_starSkillNoticeTick(0),
      m_playerGage(nullptr), m_skillNameBalloon(nullptr), m_skillMsgBalloon(nullptr), m_skillUsedMsgBalloon(nullptr),
      m_skillUsedTick(0), m_broadCastTick(0), m_nameDisplayed(0), m_nameDisplayed2(0),
      m_waitingUseItemAck(0), m_waitingItemThrowAck(0), m_waitingReqStatusAck(0),
      m_nameActorAid(0), m_lastNaid(0), m_menuTargetAID(0),
      m_nameBalloonWidth(0), m_nameBalloonHeight(0), m_dragType(0),
      m_sameChatRepeatCnt(0), m_numNotifyTime(0), m_isCheckGndAlpha(0),
      m_lastCardItemIndex(0), m_SkillBallonSkillId(0), m_nameBalloonType(0),
    m_showTimeStartTick(0), m_recordChatNum(0), m_strikeNum(0), m_isCtrlLock(0),
    m_sentLoadEndAck(0), m_isLeftButtonHeld(0), m_lastMoveRequestCellX(-1), m_lastMoveRequestCellY(-1),
        m_heldMoveTargetCellX(-1), m_heldMoveTargetCellY(-1), m_hasHeldMoveTarget(0), m_lastMoveRequestTick(0), m_lastAttackRequestTick(0), m_lastPickupRequestTick(0),
        m_lastAttackChaseHintTick(0), m_attackChaseTargetGid(0), m_attackChaseTargetCellX(-1), m_attackChaseTargetCellY(-1),
        m_attackChaseSourceCellX(-1), m_attackChaseSourceCellY(-1), m_attackChaseRange(1), m_hasAttackChaseHint(0),
        m_skillChaseTargetGid(0), m_lastSkillChaseRequestTick(0), m_skillChaseSkillId(0), m_skillChaseSkillLevel(0), m_skillChaseRange(1), m_hasSkillChase(0),
        m_groundSkillCellX(0), m_groundSkillCellY(0), m_groundSkillId(0), m_groundSkillLevel(0), m_groundSkillRange(0), m_hasGroundSkillChase(0),
        m_touchSkillTargetAid(0), m_touchSkillId(0), m_touchSkillLevel(0), m_touchSkillRange(0), m_hasTouchSkillChase(0),
        m_mapLoadingStage(MapLoading_None), m_mapLoadingStartTick(0), m_mapLoadingAckTick(0), m_lastActorBootstrapPacketTick(0),
        m_worldProcessTick(GetTickCount()), m_worldProcessCarryMs(kWorldProcessTickMs), m_lastHoverRefreshTick(0),
        m_pendingPartyCreateExpShare(false),
        m_pendingPartyCreateName()
{
    m_networkModeOperationGeneration = g_modeMgr.GetOperationGeneration();
    std::memset(m_rswName, 0, sizeof(m_rswName));
    std::memset(m_minimapBmpName, 0, sizeof(m_minimapBmpName));
    std::memset(ViewPointData, 0, sizeof(ViewPointData));
    std::memset(&m_skillUseInfo, 0, sizeof(m_skillUseInfo));
    std::memset(m_recordChatTime, 0, sizeof(m_recordChatTime));
    std::memset(m_strikeTime, 0, sizeof(m_strikeTime));
    std::memset(m_doritime, 0, sizeof(m_doritime));
}

CGameMode::~CGameMode() {
    ClearNpcProgress();
    if (m_starSkillNotice) {
        g_windowMgr.DeleteWindow(m_starSkillNotice);
    }
}

void CGameMode::HelpBalloonDeleted(UITransBalloonText* window)
{
    CMode::HelpBalloonDeleted(window);
    if (m_starSkillNotice == window) {
        m_starSkillNotice = nullptr;
    }
}

void CGameMode::ShowStarSkillNotice(const std::string& text, u32 now)
{
    if (!m_starSkillNotice) {
        m_starSkillNotice = new UITransBalloonText(this, 14, true);
        m_starSkillNotice->SetFontColor(0xC8C8FF); // Ref COLORREF 0x00FFC8C8.
        g_windowMgr.m_children.push_back(m_starSkillNotice);
        m_starSkillNotice->SetShow(1);
    }
    m_starSkillNoticeTick = now;
    m_starSkillNotice->SetText(text.c_str(), 72);
    const RECT client = g_windowMgr.GetChatClientRect();
    m_starSkillNotice->Move((client.right + client.left - m_starSkillNotice->m_w) / 2, 51);
}

void CGameMode::UpdateStarSkillNotice(u32 now)
{
    if (m_starSkillNotice && now - m_starSkillNoticeTick > 20000u) {
        g_windowMgr.DeleteWindow(m_starSkillNotice);
    }
}

void CGameMode::StartNpcProgress(u32 seconds, u32 now)
{
    if (!m_world || !m_world->m_player || seconds > 0x7fffffffu / 1000u) {
        return;
    }
    if (!m_npcProgressGauge) {
        m_npcProgressGauge = new UIRechargeGage();
        m_npcProgressGauge->Create(60, 6);
        g_windowMgr.AddWindowFront(m_npcProgressGauge);
        m_npcProgressGauge->SetShow(0);
    }
    m_npcProgressStartTick = now;
    m_npcProgressDuration = seconds * 1000u;
    m_npcProgressGauge->SetAmount(0, static_cast<int>(m_npcProgressDuration));
}

void CGameMode::ClearNpcProgress()
{
    if (!m_npcProgressGauge) {
        return;
    }
    auto* const gauge = m_npcProgressGauge;
    m_npcProgressGauge = nullptr;
    m_npcProgressStartTick = 0;
    m_npcProgressDuration = 0;
    if (g_windowMgr.ContainsWindow(gauge)) {
        g_windowMgr.DeleteWindow(gauge);
    }
}

void CGameMode::FinishNpcProgress(CRagConnection& connection)
{
    if (!m_npcProgressGauge) {
        return;
    }
    ClearNpcProgress();
    constexpr char packet[] = {static_cast<char>(0xf1), 0x02}; // CZ_PROGRESS
    connection.SendPacket(packet, static_cast<int>(sizeof(packet)));
}

void CGameMode::UpdateNpcProgress(CRagConnection& connection, u32 now)
{
    if (!m_npcProgressGauge) {
        return;
    }
    const u32 elapsed = now - m_npcProgressStartTick;
    if (elapsed > m_npcProgressDuration) {
        FinishNpcProgress(connection);
        return;
    }
    m_npcProgressGauge->SetAmount(static_cast<int>(elapsed), static_cast<int>(m_npcProgressDuration));
}

void CGameMode::OnInit(const char* worldName) {
    g_session.SetTextType(static_cast<u8>(g_session.m_isShowNameTypeOld),static_cast<u8>(g_session.m_isShowNameTypeOld));
    m_terminalNetworkHandled = false;
    m_networkModeOperationGeneration = g_modeMgr.GetOperationGeneration();
    g_session.InvalidatePartyPositions();
    g_session.ClearNpcShopState();
    ClearPendingDisconnectAction();
    RegisterDefaultGameModePacketHandlers(GetProductionGameModePacketRouter());
    ClearRuntimeActors(*this);
    ro::commerce::ResetForMapEntry();
    m_groundItemList.clear();
    m_playWaveList.clear();
    m_ambientWaveList.clear();
    m_worldProcessTick = GetTickCount();
    m_worldProcessCarryMs = kWorldProcessTickMs;
    m_lastHoverRefreshTick = 0;
    m_selectedPcTargetGid = 0;
    m_pendingPartyCreateExpShare = false;
    m_pendingPartyCreateName.clear();
    ClearPendingSkillUse(*this);

    DbgLog("[Build] marker=%s pkt0078=%d pkt0209=%d\n",
        kGameModeBuildMarker,
        ro::net::GetPacketSize(0x0078),
        ro::net::GetPacketSize(0x0209));

    std::memset(m_rswName, 0, sizeof(m_rswName));
    std::memset(m_minimapBmpName, 0, sizeof(m_minimapBmpName));
    if (worldName) {
        std::strncpy(m_rswName, worldName, sizeof(m_rswName) - 1);
        char mapName[sizeof(g_session.m_curMap)] = {};
        std::strncpy(mapName, worldName, sizeof(mapName) - 1);
        char* dot = std::strrchr(mapName, '.');
        if (dot) {
            *dot = '\0';
        }
        NormalizeSessionMapName(mapName);
        std::memset(g_session.m_curMap, 0, sizeof(g_session.m_curMap));
        std::strncpy(g_session.m_curMap, mapName, sizeof(g_session.m_curMap) - 1);
        BuildMinimapName(worldName, m_minimapBmpName, sizeof(m_minimapBmpName));
    }

    // A mode replacement can restart the same map before the old bootstrap finished.
    ResetBootstrapWorldCache(GetBootstrapWorldCache());
    m_world = &g_world;
    if (m_world) {
        m_world->m_curMode = this;
        m_world->ClearBackgroundObjects();
        m_world->ClearFixedObjects();
        m_world->m_ground = nullptr;
        m_world->m_player = nullptr;
        // The selected client constructs a fresh CWorld here.
        m_world->m_isPKZone = 0;
        m_world->m_isSiegeMode = 0;
        m_world->m_isBattleFieldMode = 0;
        m_world->m_isEventPVPMode = 0;
        m_world->BeginMapTerrainLoad();
    }
    m_view = nullptr;
    m_mousePointer = nullptr;

    g_windowMgr.RemoveAllWindows();
    g_windowMgr.SetLoginStatus("");
    g_windowMgr.OpenSourceStartupTip();
    m_loadingWallpaperName = ChooseRandomLoadingWallpaper();
    g_windowMgr.ShowLoadingScreen(m_loadingWallpaperName, "Preparing loading screen...", 0.02f);

    m_cursorActNum = 0;
    m_cursorMotNum = 0;
    m_mouseAnimStartTick = GetTickCount();
    m_canRotateView = 0;
    m_sentLoadEndAck = 0;
    m_isLeftButtonHeld = 0;
    m_lastMoveRequestCellX = -1;
    m_lastMoveRequestCellY = -1;
    m_heldMoveTargetCellX = -1;
    m_heldMoveTargetCellY = -1;
    m_hasHeldMoveTarget = 0;
    m_lastMoveRequestTick = 0;
    m_lastAttackRequestTick = 0;
    ClearAttackChaseHint(*this);
    m_mapLoadingStartTick = GetTickCount();
    m_mapLoadingAckTick = 0;
    m_lastActorBootstrapPacketTick = m_mapLoadingStartTick;
    m_mapLoadingStage = MapLoading_PresentScreen;
    ResetGamePacketTrace(m_mapLoadingStartTick);

    EnsureBootstrapSelfActor(*this);
    DbgLog("[GameMode] OnInit world='%s' map='%s' gid=%u pos=%d,%d dir=%d\n",
        m_rswName,
        g_session.m_curMap,
        g_session.m_gid,
        g_session.m_playerPosX,
        g_session.m_playerPosY,
        g_session.m_playerDir);

    CAudio* audio = CAudio::GetInstance();
    if (audio && m_rswName[0] != '\0') {
        m_streamFileName = audio->ResolveMapBgmPath(m_rswName);
        audio->PlayBGM(m_streamFileName.c_str());
    }

    g_renderer.m_nClearColor = ResolveRefMapClearColor(m_rswName);
    MakeFog(g_session.m_fogOn);
}

void CGameMode::MakeFog(int fogOn)
{
    ApplyRefFogForMap(m_rswName, fogOn);
}

bool CGameMode::CanRequestStandingResurrection() const
{
    if (!m_isPlayerDead || !m_world || m_world->m_isPKZone || m_world->m_isSiegeMode
        || m_world->m_isEventPVPMode) {
        return false;
    }
    const auto& items = g_session.GetInventoryItems();
    return std::any_of(items.begin(), items.end(), [](const ITEM_INFO& item) {
        return item.m_itemIndex != 0 && item.m_num > 0 && item.GetItemId() == 7621;
    });
}

namespace {
constexpr int kFriendInvitationMessageId = 818;
constexpr int kAdoptionInvitationMessageId = 794;
constexpr int kAdoptionConsequencesMessageId = 888;

bool SendAdoptionPacket(u8 opcode, u32 first, u32 second = 0, bool accepted = false)
{
    std::array<u8, 14> packet{opcode, 1};
    for (unsigned i = 0; i < 4; ++i) {
        packet[2 + i] = static_cast<u8>(first >> (8 * i));
        packet[6 + i] = static_cast<u8>(second >> (8 * i));
    }
    packet[10] = static_cast<u8>(accepted);
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),
        opcode == 0xf9 ? 6 : 14);
}
}

bool CGameMode::OpenPlayerContextMenu(u32 targetAid, const std::string& targetName, int x, int y)
{
    auto* menu = static_cast<UIPlayerContextMenuWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_PLAYERCONTEXTMENUWND));
    if (!menu) return false;

    m_menuTargetAID = targetAid;
    m_lastWhisperMenuCharacterName = targetName;
    auto entries = BuildPlayerContextMenuEntries(targetName);
    if (CanRequestGuildRelation(*this, targetAid, false))
        entries.push_back({PlayerContext_RequestGuildAlliance, "Request guild alliance"});
    if (CanRequestGuildRelation(*this, targetAid, true))
        entries.push_back({PlayerContext_DeclareGuildOpposition, "Declare guild opposition"});
    if (CanRequestAdoption(targetAid)) {
        ro::UiMessageStrings strings;
        strings.Load();
        auto label = strings.Get(887, "Send an adoption request to %s");
        if (const auto placeholder = label.find("%s"); placeholder != std::string::npos) {
            label.replace(placeholder, 2, targetName);
        }
        entries.push_back({PlayerContext_RequestAdoption, std::move(label)});
    }
    menu->SetMenu(targetAid, targetName, entries, x, y);
    DbgLog("[GameMode] opened player context menu targetAid=%u targetName='%s'\n",
        static_cast<unsigned int>(targetAid), targetName.c_str());
    return true;
}

bool CGameMode::CanRequestAdoption(u32 candidate) const
{
    const auto* character = g_session.GetSelectedCharacterInfo();
    if (!character || character->level < 70 || (character->job >= 4023 && character->job <= 4045)
        || !candidate || candidate == g_session.m_aid || !m_world) return false;
    const auto* actor = m_world->GetGameActorByAID(candidate);
    if (!IsPcLikeHoverActor(actor) || actor->m_job < 0 || (actor->m_job > 6 && actor->m_job != 23)) return false;
    const auto hasSkill = [](int id) {
        const auto* skill = g_session.GetSkillItemBySkillId(id);
        return skill && skill->m_isValid && skill->level > 0;
    };
    if (!hasSkill(336) || hasSkill(409) || hasSkill(410)) return false;
    const auto* member = g_session.FindPartyMemberByAid(candidate);
    return member && member->isValid;
}

void CGameMode::ClearAdoptionInvitation()
{
    if (auto* box = g_windowMgr.m_roomMessageBox; box && box->HasCancel()
        && (box->GetMessageId() == kAdoptionInvitationMessageId
            || box->GetMessageId() == kAdoptionConsequencesMessageId)) {
        box->Close();
    }
    m_adoptionInvitation = {};
}

void CGameMode::ReceiveAdoptionInvitation(u32 parent, u32 partner, std::string name)
{
    if (!parent || !partner || parent == partner || name.empty()) return;
    ClearAdoptionInvitation();
    m_adoptionInvitation = {parent, partner, std::move(name)};
    UpdateAdoptionInvitation();
}

void CGameMode::UpdateAdoptionInvitation()
{
    const auto& invite = m_adoptionInvitation;
    if (!invite.parent || g_windowMgr.m_modalWindow) return;
    const bool consequences = invite.reviewingConsequences;
    const auto parent = invite.parent;
    const auto partner = invite.partner;
    ro::UiMessageStrings strings;
    strings.Load();
    const int messageId = consequences ? kAdoptionConsequencesMessageId : kAdoptionInvitationMessageId;
    auto message = consequences
        ? strings.Get(messageId, "Adoption limits stats, reduces maximum HP and SP, and prevents advancement to a Transcendent Class. Continue?")
        : invite.name + strings.Get(messageId, " wishes to adopt you. Do you accept?");
    auto* box = new UIChatRoomMessageBox(std::move(message), messageId);
    const auto generation = g_modeMgr.GetOperationGeneration();
    box->ConfigureConfirmation(nullptr, [this, parent, partner, consequences, generation](bool accept) {
        if (g_modeMgr.GetOperationGeneration() != generation
            || m_adoptionInvitation.parent != parent || m_adoptionInvitation.partner != partner) return;
        if (accept && !consequences) {
            m_adoptionInvitation.reviewingConsequences = true;
            UpdateAdoptionInvitation();
            return;
        }
        if (SendAdoptionPacket(0xf7, parent, partner, accept)) ClearAdoptionInvitation();
    });
    if (consequences) box->SetMessageBodyHeight(105);
    g_windowMgr.m_roomMessageBox = box;
    g_windowMgr.m_children.push_back(box);
    g_windowMgr.m_modalWindow = box;
    g_windowMgr.m_editWindow = nullptr;
}

void CGameMode::CloseFriendInvitation()
{
    if (auto* box = g_windowMgr.m_roomMessageBox;
        box && box->GetMessageId() == kFriendInvitationMessageId && box->HasCancel()) {
        box->Close();
    }
}

void CGameMode::UpdateFriendInvitation()
{
    const auto& prompt = ro::commerce::state;
    if (prompt.promptKind != "friend" || g_windowMgr.m_modalWindow) return;

    ro::UiMessageStrings strings;
    strings.Load();
    auto message = strings.Get(kFriendInvitationMessageId, "(%s) wishes to be friends with you. Would you like to accept?");
    const auto placeholder = message.find("%s");
    if (placeholder != std::string::npos) {
        message.replace(placeholder, 2, prompt.promptName);
    } else {
        message = "(" + prompt.promptName + ") wishes to be friends with you. Would you like to accept?";
    }
    auto* box = new UIChatRoomMessageBox(std::move(message), kFriendInvitationMessageId);
    ro::commerce::Command reply{};
    reply.action = "prompt";
    reply.from = "friend";
    reply.promptAid = prompt.promptId;
    reply.promptCid = prompt.promptChar;
    const auto generation = g_modeMgr.GetOperationGeneration();
    box->ConfigureConfirmation(nullptr, [reply = std::move(reply), generation](bool accepted) mutable {
        if (g_modeMgr.GetOperationGeneration() != generation) return;
        reply.amount = accepted ? 1 : 0;
        ro::commerce::Action(reply);
    });
    g_windowMgr.m_roomMessageBox = box;
    g_windowMgr.m_children.push_back(box);
    g_windowMgr.m_modalWindow = box;
    g_windowMgr.m_editWindow = nullptr;
}

void CGameMode::OnExit() {
    if (g_modeMgr.GetRequestedModeType() != 1) {
        g_session.m_instanceStatus = {};
        g_session.m_questJournal = {};
        g_session.m_partyBookingRegistrationPending = false;
    }
    CloseFriendInvitation();
    ClearAdoptionInvitation();
    ClearCraftingSelection();
    ClearItemComposition();
    ClearItemIdentification();
    ro::commerce::CloseTransactions();
    g_session.m_showEquipmentToOthers = false;
    g_session.m_refuseRecall = false;
    g_session.m_refusePartyInvites = false;
    g_session.m_petStatus.ResetMapState();
    g_session.InvalidatePartyPositions();
    g_session.ClearNpcShopState();
    ClearPendingDisconnectAction();
    g_renderer.FogSwitch(0);
    GetProductionGameModePacketRouter().Clear();
    ClearRuntimeActors(*this);
    g_windowMgr.RemoveAllWindows();
    if(const auto* selected=g_session.GetSelectedCharacterInfo())g_session.SaveSkillUseLevels(selected->CharNum);
    m_groundItemList.clear();
    m_playWaveList.clear();
    m_ambientWaveList.clear();
    m_streamFileName.clear();
    if (GetCapture() == g_hMainWnd) {
        ReleaseCapture();
    }
    m_canRotateView = 0;
    SaveCameraStateForMap(*this);
    delete m_view;
    if (m_world) {
        m_world->ClearBackgroundObjects();
        m_world->ClearFixedObjects();
        m_world->ClearGround();
        m_world->SetMapTerrain(nullptr);
        m_world->m_rootNode.m_attr = nullptr;
        SetRect(&m_world->m_rootNode.m_attrArea, 0, 0, 0, 0);
    }
    m_world = nullptr;
    m_view = nullptr;
    m_mousePointer = nullptr;
    m_sentLoadEndAck = 0;
    m_isLeftButtonHeld = 0;
    m_lastMoveRequestCellX = -1;
    m_lastMoveRequestCellY = -1;
    m_heldMoveTargetCellX = -1;
    m_heldMoveTargetCellY = -1;
    m_hasHeldMoveTarget = 0;
    m_lastMoveRequestTick = 0;
    m_lastAttackRequestTick = 0;
    ClearAttackChaseHint(*this);
    m_mapLoadingStage = MapLoading_None;
    m_mapLoadingStartTick = 0;
    m_mapLoadingAckTick = 0;
    m_lastActorBootstrapPacketTick = 0;
    m_loadingWallpaperName.clear();
}
void CGameMode::PruneCompassMarkers(u32 now)
{
    for (auto it = m_compassPosList.begin(); it != m_compassPosList.end();) {
        if (it->second.IsExpired(now)) {
            it = m_compassPosList.erase(it);
        } else {
            ++it;
        }
    }
}

int  CGameMode::OnRun() {
    if (!m_compassPosList.empty()) {
        PruneCompassMarkers(timeGetTime());
    }
    if(m_world && m_world->m_player){
        int x=0,y=0;GetServerCoor(m_world->m_player->m_pos.x,m_world->m_player->m_pos.z,x,y);
        std::map<u32,ro::commerce::TradePeer> peers;
        for(const auto& entry:m_runtimeActors)if(entry.second&&entry.second->m_isPc&&entry.second->m_isVisible){
            int px=0,py=0;GetServerCoor(entry.second->m_pos.x,entry.second->m_pos.z,px,py);
            auto name=m_actorNameListByGID.find(entry.first);
            peers[entry.first]={name==m_actorNameListByGID.end()?std::string():name->second.name,(std::max)(std::abs(px-x),std::abs(py-y))};
        }
        ro::commerce::UpdateTradeContext(GetTickCount(),std::move(peers));
    }

    SyncRendererToWindowSize();

    const bool mapLoadingWasActive = IsMapLoadingActive(*this);
    const DWORD updateStart = GetTickCount();
    OnUpdate();
    const DWORD updateEnd = GetTickCount();
    ClearQueuedMsgEffects();

    const DWORD processUiStart = updateEnd;
    g_windowMgr.OnProcess();
    const DWORD processUiEnd = GetTickCount();
    if (mapLoadingWasActive && !IsMapLoadingActive(*this)) {
        DbgLog("[GameMode] deferring first gameplay render after map loading transition for world '%s'\n", m_rswName);
        Sleep(1);
        return 1;
    }
    if (IsMapLoadingActive(*this)) {
        const bool hasLegacyLoadingDevice = GetRenderDevice().GetLegacyDevice() != nullptr;
        if (!hasLegacyLoadingDevice && IsQtUiRuntimeEnabled()) {
            g_renderer.ClearBackground();
            g_renderer.Clear(0);
            const bool queuedLoadingOverlay = QueueModernOverlayQuad(*this, m_cursorActNum, m_mouseAnimStartTick);
            if (queuedLoadingOverlay) {
                QueueCursorOverlayQuad(m_cursorActNum, m_mouseAnimStartTick);
                g_renderer.DrawScene();
                g_renderer.Flip(false);
            } else {
                g_windowMgr.OnDraw();
            }
        } else {
            g_windowMgr.OnDraw();
            DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
        }
        Sleep(1);
        return 1;
    }

    if (!m_view) {
        static bool loggedBootstrapRenderer = false;
        if (!loggedBootstrapRenderer) {
            loggedBootstrapRenderer = true;
            DbgLog("[GameMode] bootstrap renderer active because m_view is null for world '%s'\n", m_rswName);
        }
        DrawBootstrapScene(g_hMainWnd, *this);
        DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
        Sleep(1);
        return 1;
    }

    const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;
    const bool isVulkanBackend = GetRenderDevice().GetBackendType() == RenderBackendType::Vulkan;
    const bool trackMovePerfFrame = m_world && m_world->m_player && m_world->m_player->m_isMoving;
    if (trackMovePerfFrame) {
        g_overlayMovePerfStats.frames += 1;
    }
    g_renderer.ClearBackground();
    g_renderer.Clear(0);
    static u32 s_gameplayRenderFrame = 0;
    ++s_gameplayRenderFrame;
    if(g_windowMgr.m_petTamingDeceiveWnd)g_windowMgr.m_petTamingDeceiveWnd->PrepareDrawFrame();
    DWORD uiDrawStart = GetTickCount();
    DWORD uiDrawEnd = uiDrawStart;
    bool queuedModernOverlayFrame = false;
    bool queuedCursorOverlayFrame = false;
    if (!hasLegacyDevice) {
        uiDrawStart = GetTickCount();
        const double modernQueueStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        queuedModernOverlayFrame = QueueModernOverlayQuad(*this, m_cursorActNum, m_mouseAnimStartTick);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.queueModernMs += QpcNowMs() - modernQueueStartMs;
        }
        if (!IsQtUiRuntimeEnabled()) {
            const double roMapStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
            QueueRoMapOverlayQuad();
            if (trackMovePerfFrame) {
                g_overlayMovePerfStats.queueRoMapMs += QpcNowMs() - roMapStartMs;
            }
        }
        const double lockedTargetStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        QueueLockedTargetOverlayQuad(*this);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.queueLockedTargetMs += QpcNowMs() - lockedTargetStartMs;
        }
        QueuePlayerVitalsOverlayQuad(*this);
        QueueOwnedCompanionVitalsOverlayQuads(*this);
        if (!IsQtUiRuntimeEnabled()) {
            QueueHoverLabelsOverlayQuad(*this);
        }
        const double cursorQueueStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        queuedCursorOverlayFrame = QueueCursorOverlayQuad(m_cursorActNum, m_mouseAnimStartTick);
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.queueCursorMs += QpcNowMs() - cursorQueueStartMs;
        }
        uiDrawEnd = GetTickCount();
    }
    // Qt overlay rendering pumps input events. Ground-click hit testing can
    // rebuild the player's billboard/aura textures, so submit world geometry
    // only after that work has finished. Render queues still put UI last.
    const DWORD renderPrepStart = GetTickCount();
    m_view->OnRender();
    // World rendering produces healing/damage digits and Miss sprite draws.
    // Flush them in this frame, after production and before DrawScene; the next
    // OnRun deliberately clears leftover queues before rendering.
    if (!hasLegacyDevice) {
        const double msgStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        QueueMsgEffectsOverlayQuad();
        if (trackMovePerfFrame) g_overlayMovePerfStats.queueMsgMs += QpcNowMs() - msgStartMs;
    }
    const DWORD renderPrepEnd = GetTickCount();
    const DWORD drawSceneStart = GetTickCount();
    g_renderer.DrawScene();
    const DWORD drawSceneEnd = GetTickCount();
    if (hasLegacyDevice) {
        const DWORD flipStart = GetTickCount();
        const double flipStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        g_renderer.Flip(false);
        const DWORD flipEnd = GetTickCount();
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.flipMs += QpcNowMs() - flipStartMs;
        }
        const DWORD uiDrawStart = GetTickCount();
        const double uiDrawStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        DrawGameplayFallbackToWindow(
            *this,
            m_cursorActNum,
            m_mouseAnimStartTick,
            trackMovePerfFrame,
            uiDrawStartMs,
            true,
            false);
        const DWORD uiDrawEnd = GetTickCount();
        if (s_gameplayRenderFrame <= 20 || (s_gameplayRenderFrame % 120u) == 0) {
            DbgLog("[GameMode] frame=%u legacy=1 update=%lu ui=%lu prep=%lu draw=%lu overlay=%lu flip=%lu\n",
                static_cast<unsigned int>(s_gameplayRenderFrame),
                static_cast<unsigned long>(updateEnd - updateStart),
                static_cast<unsigned long>(processUiEnd - processUiStart),
                static_cast<unsigned long>(renderPrepEnd - renderPrepStart),
                static_cast<unsigned long>(drawSceneEnd - drawSceneStart),
                static_cast<unsigned long>(uiDrawEnd - uiDrawStart),
                static_cast<unsigned long>(flipEnd - flipStart));
        }
    } else {
        bool composedModernOverlayFrame = queuedModernOverlayFrame;
        const DWORD flipStart = GetTickCount();
        const double flipStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        g_renderer.Flip(false);
        const DWORD flipEnd = GetTickCount();
        if (trackMovePerfFrame) {
            g_overlayMovePerfStats.flipMs += QpcNowMs() - flipStartMs;
        }
        if (!composedModernOverlayFrame) {
            const double uiDrawStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
            DrawGameplayFallbackToWindow(
                *this,
                m_cursorActNum,
                m_mouseAnimStartTick,
                trackMovePerfFrame,
                uiDrawStartMs,
                !isVulkanBackend,
                queuedCursorOverlayFrame);
        }
        g_framePerfStats.frames += 1;
        g_framePerfStats.updateMs += static_cast<u64>(updateEnd - updateStart);
        g_framePerfStats.processUiMs += static_cast<u64>(processUiEnd - processUiStart);
        g_framePerfStats.renderPrepMs += static_cast<u64>(renderPrepEnd - renderPrepStart);
        g_framePerfStats.drawSceneMs += static_cast<u64>(drawSceneEnd - drawSceneStart);
        g_framePerfStats.uiDrawMs += static_cast<u64>(uiDrawEnd - uiDrawStart);
        g_framePerfStats.flipMs += static_cast<u64>(flipEnd - flipStart);

        if (s_gameplayRenderFrame <= 20 || (s_gameplayRenderFrame % 120u) == 0) {
            DbgLog("[GameMode] frame=%u legacy=0 update=%lu ui=%lu prep=%lu draw=%lu overlay=%lu flip=%lu composed=%d\n",
                static_cast<unsigned int>(s_gameplayRenderFrame),
                static_cast<unsigned long>(updateEnd - updateStart),
                static_cast<unsigned long>(processUiEnd - processUiStart),
                static_cast<unsigned long>(renderPrepEnd - renderPrepStart),
                static_cast<unsigned long>(drawSceneEnd - drawSceneStart),
                static_cast<unsigned long>(uiDrawEnd - uiDrawStart),
                static_cast<unsigned long>(flipEnd - flipStart),
                composedModernOverlayFrame ? 1 : 0);
        }
    }

    if (trackMovePerfFrame && (g_overlayMovePerfStats.frames % 30u) == 0) {
        const double frameCount = static_cast<double>(g_overlayMovePerfStats.frames);
        const double refreshCount = static_cast<double>((std::max)(u64{1}, g_overlayMovePerfStats.modernRefreshes));
        DbgLog("[OverlayPerfHiRes] moveFrames=%llu queueModern=%.3fms queueRoMap=%.3fms queueLocked=%.3fms queueMsg=%.3fms queueCursor=%.3fms refreshes=%llu overlayDraw=%.3fms uiDraw=%.3fms convert=%.3fms texUpdate=%.3fms fallbackOverlay=%.3fms fallbackMsg=%.3fms fallbackUi=%.3fms fallbackCursorHdc=%.3fms fallbackCursor=%.3fms flip=%.3fms\n",
            static_cast<unsigned long long>(g_overlayMovePerfStats.frames),
            g_overlayMovePerfStats.queueModernMs / frameCount,
            g_overlayMovePerfStats.queueRoMapMs / frameCount,
            g_overlayMovePerfStats.queueLockedTargetMs / frameCount,
            g_overlayMovePerfStats.queueMsgMs / frameCount,
            g_overlayMovePerfStats.queueCursorMs / frameCount,
            static_cast<unsigned long long>(g_overlayMovePerfStats.modernRefreshes),
            g_overlayMovePerfStats.modernOverlayDrawMs / refreshCount,
            g_overlayMovePerfStats.modernUiDrawMs / refreshCount,
            g_overlayMovePerfStats.modernConvertMs / refreshCount,
            g_overlayMovePerfStats.modernTextureUpdateMs / refreshCount,
            g_overlayMovePerfStats.fallbackOverlayDrawMs / frameCount,
            g_overlayMovePerfStats.fallbackMsgMs / frameCount,
            g_overlayMovePerfStats.fallbackUiDrawMs / frameCount,
            g_overlayMovePerfStats.fallbackCursorHdcMs / frameCount,
            g_overlayMovePerfStats.fallbackCursorMs / frameCount,
            g_overlayMovePerfStats.flipMs / frameCount);
        g_overlayMovePerfStats = OverlayMovePerfStats{};
    }

    if (!isVulkanBackend) {
        Sleep(1);
    }
    return 1;
}
// Ref/GameMode.cpp:15448-15506 (0x57D870).
// Ref runtime-wave: 0x57D8ED 0x57D91C (PLAY_WAVE_INFO::wavName).
void CGameMode::ProcessPlayWave()
{
    for (PLAY_WAVE_INFO& info : m_playWaveList) {
        if (timeGetTime() < info.endTick) {
            continue;
        }
        if (info.nAID != 0) {
            CGameActor* actor = m_world ? m_world->GetGameActorByAID(info.nAID) : nullptr;
            if (actor && m_world->m_player) {
                const vector3d& player = m_world->m_player->m_pos;
                PlayWave(info.wavName.c_str(), player.x - actor->m_pos.x, 0.0f,
                    player.z - actor->m_pos.z);
            }
        } else {
            PlayWave(info.wavName.c_str(), 0.0f, 0.0f, 0.0f);
        }
        info.endTick = timeGetTime() + info.term;
    }
}

void CGameMode::PlaceEffect(const vector3d& pos, int effectId, int lastTime, int extra)
{
    if (!m_world) {
        return;
    }
    auto* launcher = new CDummyEffectLauncher();
    launcher->Initialize(pos, effectId, lastTime, extra);
    m_world->m_gameObjectList.push_back(launcher);
#if defined(RO_SKILL_REPLAY_TRACE)
    DbgLog("[REPLAY] PlaceEffect id=%d lastTime=%d extra=%d pos=(%.3f,%.3f,%.3f)\n",
        effectId, lastTime, extra, pos.x, pos.y, pos.z);
#endif
}

CGameModePacketRouter& GetProductionGameModePacketRouter()
{
    return g_gameModePacketRouter;
}

GameModePacketLoopResult ProcessGameModePacketLoop(
    CGameMode& mode,
    CRagConnection& connection,
    CGameModePacketRouter& router,
    int packetBudget)
{
    GameModePacketLoopResult result{};
    std::vector<u8> rawPacket;
    while (packetBudget-- > 0) {
        if (!connection.RecvPacket(rawPacket)) {
            result.receiveDrained = true;
            break;
        }

        PacketView packet{};
        int consumed = 0;
        if (!TryReadPacket(rawPacket.data(), static_cast<int>(rawPacket.size()), packet, consumed)) {
            continue;
        }
        TraceGamePacketFirstSeen(packet);
        ++result.packetsProcessed;
        const u64 operationBefore = g_modeMgr.GetOperationGeneration();
        if (router.Dispatch(mode, packet)) {
            ++result.packetsDispatched;
        } else {
            ++result.packetsUnhandled;
            LogFirstSeenUnhandledGamePacket(packet.packetId, packet.packetLength);
        }
        if (g_modeMgr.GetOperationGeneration() != operationBefore) {
            result.modeOperationRequested = true;
            break;
        }
    }
    return result;
}

bool CGameMode::ProcessNetworkUpdate(CRagConnection& connection, CGameModePacketRouter& router,
    int packetBudget, u32 nowTick, GameModePacketLoopResult* outResult)
{
    if (outResult) *outResult = {};
    if (m_terminalNetworkHandled) return false;
    // A UI or other production caller may already have scheduled a handoff.
    if (g_modeMgr.GetOperationGeneration() != m_networkModeOperationGeneration) {
        ClearNpcProgress();
        m_terminalNetworkHandled = true;
        return false;
    }

    const GameModePacketLoopResult packetLoop = ProcessGameModePacketLoop(
        *this, connection, router, packetBudget);
    if (outResult) *outResult = packetLoop;
    if (packetLoop.modeOperationRequested) {
        ClearNpcProgress();
        m_terminalNetworkHandled = true;
        return false;
    }
    if (!connection.IsOpen()) {
        ClearNpcProgress();
        m_terminalNetworkHandled = true;
        ClearPendingDisconnectAction();
        g_session.m_pendingReturnToCharSelect = 0;
        const auto error = connection.GetLastReceiveError();
        const char* status = error == CRagConnection::ReceiveError::UnknownOpcode
            ? "Login error: server sent an unknown packet; connection closed."
            : error == CRagConnection::ReceiveError::InvalidVariableLength
                ? "Login error: server sent a malformed variable-length packet; connection closed."
                : "Login: connection to map server lost.";
        ReturnToLoginModeWithStatus(status);
        return false;
    }
    if (ApplyDisconnectTimeoutAfterPacketLoop(packetLoop, nowTick)) {
        ClearNpcProgress();
        m_terminalNetworkHandled = true;
        return false;
    }
    return true;
}

void CGameMode::OnUpdate() {
    // Receive and decide handoffs before any actor, world or audio processing.
    CRagConnection* conn = CRagConnection::instance();
    const int packetBudget = (m_mapLoadingStage != MapLoading_None) ? 128 : 64;
    const DWORD packetStart = GetTickCount();
    GameModePacketLoopResult packetLoop{};
    if (!ProcessNetworkUpdate(*conn, GetProductionGameModePacketRouter(),
            packetBudget, packetStart, &packetLoop)) return;
    const DWORD packetEnd = GetTickCount();
    const int packetsProcessed = packetLoop.packetsProcessed;

    UpdateStarSkillNotice(timeGetTime());
    UpdateFriendInvitation();
    UpdateAdoptionInvitation();
    UpdateNpcProgress(*conn, timeGetTime());
    DestroyDeferredRuntimeActors(*this);
    EnsureBootstrapSelfActor(*this);
    SendTimeSyncRequest(false);
    const bool trackMovePerfFrame = IsMovePerfActive(*this);
    if (trackMovePerfFrame) {
        g_gameModeMovePerfStats.frames += 1;
    }

    const DWORD actorSummaryStart = GetTickCount();
    LogRuntimeActorSummary(*this);
    const DWORD actorSummaryEnd = GetTickCount();

    ProcessPlayWave();
    UpdateMapAudio(*this);
    const DWORD worldNow = GetTickCount();
    const DWORD worldElapsedMs = worldNow - m_worldProcessTick;
    m_worldProcessTick = worldNow;
    m_worldProcessCarryMs += static_cast<float>(worldElapsedMs);
    int worldStepCount = static_cast<int>(m_worldProcessCarryMs / kWorldProcessTickMs);
    if (worldStepCount > 0) {
        m_worldProcessCarryMs -= static_cast<float>(worldStepCount) * kWorldProcessTickMs;
    }
    if (worldStepCount > kMaxWorldProcessStepsPerUpdate) {
        worldStepCount = kMaxWorldProcessStepsPerUpdate;
        m_worldProcessCarryMs = 0.0f;
    }
    g_isSkipFrame = worldStepCount;
    const int actorSteps = worldStepCount > 0 ? worldStepCount : 1;

    if (IsMapLoadingActive(*this)) {
        AdvanceMapLoading(*this, worldNow);
        if (m_world) {
            SetRuntimeActorCameraLongitude(m_view ? m_view->GetCameraLongitude() : 0.0f);
            for (int step = 0; step < actorSteps; ++step) {
                m_world->UpdateActors();
                CleanupPendingActorDespawns(*this);
            }
            g_Weather.Process();
            if (m_view && m_world) {
                m_world->ProcessActorSkillRechargeGages(m_view->GetViewMatrix(), m_view->GetCameraLongitude(), m_npcProgressGauge);
            }
            const matrix* viewMatrix = m_view ? &m_view->GetViewMatrix() : nullptr;
            m_world->UpdateBackgroundObjects(viewMatrix);
        }
        SetModeCursorAction(*this, CursorAction::Arrow);
        return;
    }

    const DWORD bootstrapStart = GetTickCount();
    EnsureBootstrapWorldAssets(*this);
    // The session cell can be the destination of an unfinished walk. Initial
    // placement belongs to AdvanceMapLoading; gameplay positions come from
    // actor movement and authoritative position packets, including skill stops.
    SyncGroundItemsToWorld(*this);
    const DWORD bootstrapEnd = GetTickCount();

    DWORD actorUpdateEnd = bootstrapEnd;
    DWORD backgroundUpdateEnd = bootstrapEnd;
    if (m_world) {
        const DWORD actorUpdateStart = GetTickCount();
        SetRuntimeActorCameraLongitude(m_view ? m_view->GetCameraLongitude() : 0.0f);
        for (int step = 0; step < actorSteps; ++step) {
            m_world->UpdateActors();
            CleanupPendingActorDespawns(*this);
        }
        g_Weather.Process();
        if (m_world->m_player) {
            if (CPlayer* const localPlayer = dynamic_cast<CPlayer*>(m_world->m_player)) {
                localPlayer->ProcessPreMove();
            }
            // Present the local player at the current frame time even when the
            // wider world simulation remains budgeted on the coarser step. The
            // self actor now bypasses the shared billboard helper cache, so this
            // no longer drags cached UI/billboard data around with it.
            m_world->m_player->ProcessState();
        }
        actorUpdateEnd = GetTickCount();

        const matrix* viewMatrix = m_view ? &m_view->GetViewMatrix() : nullptr;
        const DWORD backgroundUpdateStart = GetTickCount();
        m_world->UpdateBackgroundObjects(viewMatrix);
        backgroundUpdateEnd = GetTickCount();
    }

    const DWORD ensureViewStart = GetTickCount();
    EnsureRealView(*this);
    const DWORD ensureViewEnd = GetTickCount();

    const DWORD heldMoveStart = GetTickCount();
    PumpSkillChaseRequest(*this);
    PumpAttackChaseRequest(*this);
    PumpPendingPickupRequest(*this);
    PumpHeldMoveRequest(*this);
    const DWORD heldMoveEnd = GetTickCount();

    DWORD viewCalcEnd = heldMoveEnd;
    if (m_view) {
        const DWORD viewCalcStart = GetTickCount();
        m_view->OnCalcViewInfo();
        viewCalcEnd = GetTickCount();
    }

    if (m_world && m_view) {
        const double skillRechargeStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        m_world->ProcessActorSkillRechargeGages(m_view->GetViewMatrix(), m_view->GetCameraLongitude(), m_npcProgressGauge);
        if (trackMovePerfFrame) {
            g_gameModeMovePerfStats.skillRechargeCalls += 1;
            g_gameModeMovePerfStats.skillRechargeMs += QpcNowMs() - skillRechargeStartMs;
        }
    }

    // Hover picking must run after OnCalcViewInfo: ScreenToAttrCell projects attr
    // quads with m_viewMatrix. Using a one-frame-stale matrix while the camera
    // moves makes the 9x9 region test miss and triggers the O(map) ray march every
    // tick (catastrophic FPS). UpdateGameplayCursor already ran after view calc
    // and was fine; this block was the outlier.
    DWORD hoverUpdateEnd = viewCalcEnd;
    bool refreshedHoverThisFrame = false;
    bool playerMovingForHover = false;
    bool needsPreciseHover = false;
    m_hasCursorSnap = false;
    if (m_view && !m_canRotateView && g_hMainWnd) {
        POINT cursorPos{};
        if (GetCursorPos(&cursorPos) && ScreenToClient(g_hMainWnd, &cursorPos)) {
            m_cursorSnapRawX = cursorPos.x;
            m_cursorSnapRawY = cursorPos.y;
            m_hasCursorSnap = ResolveCursorDrawPosition(cursorPos.x, cursorPos.y,
                &m_cursorSnapDrawX, &m_cursorSnapDrawY);
            const int prevMouseX = m_oldMouseX;
            const int prevMouseY = m_oldMouseY;
            const bool cursorMoved = cursorPos.x != prevMouseX || cursorPos.y != prevMouseY;
            playerMovingForHover = m_world && m_world->m_player && m_world->m_player->m_isMoving;
            needsPreciseHover = m_isLeftButtonHeld || HasPendingSkillUse(*this);
            const DWORD hoverNow = GetTickCount();
            const bool shouldRefreshHover = !playerMovingForHover
                || needsPreciseHover
                || cursorMoved
                || m_lastHoverRefreshTick == 0
                || hoverNow - m_lastHoverRefreshTick >= kMovingCameraHoverRefreshMs;
            m_oldMouseX = cursorPos.x;
            m_oldMouseY = cursorPos.y;

            if (shouldRefreshHover) {
                m_view->UpdateHoverCellFromScreen(cursorPos.x, cursorPos.y);
                m_lastHoverRefreshTick = hoverNow;
                refreshedHoverThisFrame = true;
            }
            if (m_isLeftButtonHeld) {
                UpdateHeldMoveTargetFromScreenPoint(*this, cursorPos.x, cursorPos.y);
            }
        }
        hoverUpdateEnd = GetTickCount();
    }

    const bool shouldRefreshGameplayCursor = !playerMovingForHover
        || needsPreciseHover
        || refreshedHoverThisFrame;
    if (shouldRefreshGameplayCursor) {
        const double cursorStartMs = trackMovePerfFrame ? QpcNowMs() : 0.0;
        UpdateGameplayCursor(*this);
        if (trackMovePerfFrame) {
            g_gameModeMovePerfStats.cursorCalls += 1;
            g_gameModeMovePerfStats.cursorMs += QpcNowMs() - cursorStartMs;
        }
    }

    g_updatePerfStats.frames += 1;
    g_updatePerfStats.packetMs += static_cast<u64>(packetEnd - packetStart);
    g_updatePerfStats.actorSummaryMs += static_cast<u64>(actorSummaryEnd - actorSummaryStart);
    g_updatePerfStats.bootstrapMs += static_cast<u64>(bootstrapEnd - bootstrapStart);
    g_updatePerfStats.actorUpdateMs += static_cast<u64>(actorUpdateEnd - bootstrapEnd);
    g_updatePerfStats.backgroundUpdateMs += static_cast<u64>(backgroundUpdateEnd - actorUpdateEnd);
    g_updatePerfStats.ensureViewMs += static_cast<u64>(ensureViewEnd - ensureViewStart);
    g_updatePerfStats.hoverUpdateMs += static_cast<u64>(hoverUpdateEnd - viewCalcEnd);
    g_updatePerfStats.heldMoveMs += static_cast<u64>(heldMoveEnd - heldMoveStart);
    g_updatePerfStats.viewCalcMs += static_cast<u64>(viewCalcEnd - heldMoveEnd);
    g_updatePerfStats.packetsProcessed += static_cast<u64>(packetsProcessed);
        if ((g_updatePerfStats.frames % kFramePerfLogIntervalFrames) == 0) {
        g_updatePerfStats.packetMs = 0;
        g_updatePerfStats.actorSummaryMs = 0;
        g_updatePerfStats.bootstrapMs = 0;
        g_updatePerfStats.actorUpdateMs = 0;
        g_updatePerfStats.backgroundUpdateMs = 0;
        g_updatePerfStats.ensureViewMs = 0;
        g_updatePerfStats.hoverUpdateMs = 0;
        g_updatePerfStats.heldMoveMs = 0;
        g_updatePerfStats.viewCalcMs = 0;
        g_updatePerfStats.packetsProcessed = 0;
    }

    if (trackMovePerfFrame) {
        LogGameModeMovePerfIfNeeded();
    }
}
void DecodeTalkScript(char* str)
{
    if (g_serviceType != ServiceKorea || str == nullptr) {
        return;
    }
    const int length = static_cast<int>(std::strlen(str));
    for (int index = 0; index < length; ++index) {
        unsigned char value = static_cast<unsigned char>(str[index]);
        if (value >= 0x61u && value <= 0x7Au) {
            value = static_cast<unsigned char>(static_cast<int>(-37) - static_cast<int>(value));
        }
        if (value < 0x41u || value > 0x5Au) {
            if (value >= 0xB0u && value <= 0xBFu) {
                value = static_cast<unsigned char>(111 - static_cast<int>(value));
            }
        } else {
            value = static_cast<unsigned char>(static_cast<int>(-101) - static_cast<int>(value));
        }
        str[index] = static_cast<char>(value);
    }
}

void SendTalkScriptLine(int variant)
{
    const char* fileName = variant == 1 ? "BA_frostjoke.txt" : "DC_scream.txt";
    if (g_serviceType == ServiceGermany) {
        fileName = variant == 1 ? "english\\BA_frostjoke.txt" : "english\\DC_scream.txt";
    }
    CFile file;
    if (!file.Open(fileName, 0)) {
        return;
    }
    const int length = file.Size();
    if (length <= 0) {
        file.Close();
        return;
    }
    std::vector<char> bytes(static_cast<size_t>(length) + 1, 0);
    file.Read(bytes.data(), static_cast<u32>(length));
    file.Close();
    const int chosen = (rand() % (variant == 1 ? 117 : 114)) + 1;
    int line = 0;
    std::string text;
    for (int index = 0; index < length; ++index) {
        const unsigned char value = static_cast<unsigned char>(bytes[static_cast<size_t>(index)]);
        if (value == 13) {
            ++line;
        }
        if (line == chosen) {
            if (value != 13 && value != 10 && value != 9) {
                text.push_back(static_cast<char>(value));
            }
        } else if (line == chosen + 1) {
            break;
        }
    }
    if (text.empty()) {
        return;
    }
    DecodeTalkScript(text.data());
    SendGlobalChatMessage(g_session.GetPlayerName(), text);
}

std::string SkillHeadBubbleName(int skillId)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    if (metadata == nullptr) {
        return {};
    }
    if (!metadata->displayName.empty()) {
        return metadata->displayName;
    }

    std::string fallback = metadata->skillIdName;
    std::replace(fallback.begin(), fallback.end(), '_', ' ');
    return fallback;
}

std::string CachedActorCharacterName(const CGameMode& mode, u32 actorId)
{
    const auto byGid = mode.m_actorNameListByGID.find(actorId);
    if (byGid != mode.m_actorNameListByGID.end() && !byGid->second.name.empty()) {
        return byGid->second.name;
    }
    const auto byAid = mode.m_actorNameList.find(actorId);
    if (byAid != mode.m_actorNameList.end() && !byAid->second.name.empty()) {
        return byAid->second.name;
    }
    if (actorId == g_session.m_aid || actorId == g_session.m_gid) {
        const char* playerName = g_session.GetPlayerName();
        if (playerName != nullptr && playerName[0] != '\0') {
            return playerName;
        }
    }
    return {};
}

// Ref/GameMode2.cpp case 0x63 (mode message 99). The overhead balloon is actor
// message 7, which stays up for 5 seconds. The written text is the skillnametable
// name plus " !!". Wedding skills use the partner line from msgstringtable.
void ShowSkillHeadBubble(CGameMode& mode, int skillId, CGameActor* actor, int extra)
{
    mode.m_skillUsedTick = GetTickCount();
    if (actor == nullptr || skillId < 0 || extra < 0 || skillId == 389) {
        return;
    }
    if (skillId == 326 || skillId == 318 || skillId == 492) {
        return;
    }

    std::string text;
    if (skillId == 334 || skillId == 335) {
        text = CachedActorCharacterName(mode, static_cast<u32>(extra));
        if (!text.empty()) {
            text += " !!  ";
        }
        text += "I love you.";
    } else if (skillId == 336) {
        text = mode.m_recallPartnerName;
        const std::string skillName = SkillHeadBubbleName(skillId);
        if (!text.empty()) {
            text += " !!  ";
        }
        text += skillName;
    } else {
        text = SkillHeadBubbleName(skillId);
        if (text.empty()) {
            return;
        }
        text += " !!";
    }

    if (text.empty()) {
        return;
    }
    actor->SetChatBubbleText(text, GetTickCount() + 5000);
}

void UpdateShowImageInfo(CGameMode& mode, const char* imageName, int type)
{
    if (imageName == nullptr) {
        return;
    }
    mode.m_showImageInfo.type = type;
    mode.m_showImageInfo.name = imageName;
    if (mode.m_showImageInfo.name.find(".bmp") == std::string::npos
        && mode.m_showImageInfo.name.find(".BMP") == std::string::npos) {
        mode.m_showImageInfo.name += ".bmp";
    }
}

bool CGameMode::RequestEnterChatRoom(u32 roomId,const char* password,u32 now)
{
    // Preserve the strict 1000ms threshold with wrap-safe elapsed arithmetic.
    if(static_cast<u32>(now-m_reqTickChatRoom)>1000)m_isWaitingEnterRoom=0;
    if(g_windowMgr.m_chatRoomWnd || m_isWaitingEnterRoom)return false;
    m_isWaitingEnterRoom=1;m_reqTickChatRoom=now;
    std::array<std::uint8_t,8> bytes{};
    if(password)for(std::size_t i=0;i<bytes.size() && password[i];++i)bytes[i]=static_cast<std::uint8_t>(password[i]);
    std::vector<std::uint8_t> packet;ro::chat::BuildRoomJoinRequest(roomId,bytes,packet);
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()));
}

bool CGameMode::SendChatRoomSettings(const ro::chat::RoomInfo& info,bool change)
{
    if(change && !g_windowMgr.m_chatRoomWnd)return false;
    // Original mode48/49 update pending metadata before attempting the send.
    m_lastChatroomInfo=info;std::array<std::uint8_t,8> password{};
    std::copy_n(info.pass.begin(),(std::min)(info.pass.size(),password.size()),password.begin());
    std::vector<std::uint8_t> packet;
    const bool built=change?ro::chat::BuildRoomChangeRequest(static_cast<u16>(info.maxNumPeople),static_cast<u8>(info.roomType),password,info.title,packet)
        :ro::chat::BuildRoomCreateRequest(static_cast<u16>(info.maxNumPeople),static_cast<u8>(info.roomType),password,info.title,packet);
    return built && CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()));
}

namespace {
std::filesystem::path& SourceAutoSaveChatFile(){static std::filesystem::path path;return path;}
bool CanAutoSaveSourceChat(){return std::find(s_dwAdminAID.begin(),s_dwAdminAID.end(),g_session.m_aid)!=s_dwAdminAID.end();}
}
std::string CGameMode::GetSourceAutoSaveChatPath() const{return SourceAutoSaveChatFile().u8string();}
bool CGameMode::ToggleSourceChatAutoSave()
{
    if(!CanAutoSaveSourceChat()){
        ro::UiMessageStrings table;table.Load();const auto message=table.Get(95,"Invalid command.");
        g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(message.c_str()),65535);return false;
    }
    g_session.m_isSaveChat=!g_session.m_isSaveChat;
    if(g_session.m_isSaveChat){
        m_sourceAutoSaveChatCount=0;SourceAutoSaveChatFile().clear();std::error_code error;
        const auto directory=GetOpenMidgardIniPath().parent_path()/"Chat";std::filesystem::create_directories(directory,error);
        if(!error){auto path=directory/"Chat.txt";unsigned suffix=0;
            while(std::filesystem::exists(path,error) && !error){std::ostringstream name;name<<"Chat"<<std::setfill('0')<<std::setw(3)<<++suffix<<".txt";path=directory/name.str();}
            if(!error)SourceAutoSaveChatFile()=path;
        }
    }
    const char* message=g_session.m_isSaveChat?"Chatting Auto Save Start..":"Chatting Auto Save END";
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(message),65535);return true;
}
bool CGameMode::ToggleSourceAutoMessage()
{
    g_session.m_onAutoMSG=!g_session.m_onAutoMSG;
    g_windowMgr.SendMsg(3,0,0,0);
    if(g_session.m_onAutoMSG)return g_windowMgr.MakeWindow(UIWindowMgr::WID_AUTOMESSAGEWND)!=nullptr;
    ro::UiMessageStrings table;table.Load();const auto notice=table.Get(943,"Automatic reply disabled.");
    g_windowMgr.SendMsg(1,reinterpret_cast<msgparam_t>(notice.c_str()),6619135);return true;
}
bool CGameMode::OpenPetContextMenu(CGameActor* actor,int logicalX,int logicalY)
{
    if(!m_world || !actor || g_windowMgr.m_storageWnd || g_windowMgr.m_modalWindow
        || actor->m_gid!=g_session.m_petStatus.actorId || actor->m_headType!=100
        || m_world->GetGameActorByAID(actor->m_gid)!=actor)return false;
    const int job=actor->m_job;
    if(!((job>=45 && job<4000) || (job>6000 && job<6048) || actor->m_objectType==9))return false;
    if(g_windowMgr.m_chatRoomMenuWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
    m_menuIdList={33,29,30,32,31};m_menuTargetAID=actor->m_gid;
    ro::UiMessageStrings table;table.Load();
    const int labels[]={596,592,593,595,594};std::vector<RoomMenuItem> items;
    for(std::size_t i=0;i<m_menuIdList.size();++i)items.push_back({m_menuIdList[i],table.Get(labels[i],"")});
    auto* menu=new UIChatRoomMenuWnd(items,logicalX,logicalY,true);
    // Source Move(mouse+1) is not clamped back into the client rectangle.
    menu->Move(logicalX+1,logicalY+1);g_windowMgr.m_chatRoomMenuWnd=menu;
    g_windowMgr.m_children.push_back(menu);return true;
}
bool CGameMode::OpenHomunculusContextMenu(CGameActor* actor,int logicalX,int logicalY)
{
    // Ref MakeMenu: owned homunculus gets MENUID_HOMUN_SHOWINFO/FEEDING/FOLLOW.
    if (!m_world || !actor || g_windowMgr.m_storageWnd || g_windowMgr.m_modalWindow
        || !g_session.m_homunStatus.alive || actor->m_gid != g_session.m_homunGID
        || m_world->GetGameActorByAID(actor->m_gid) != actor) {
        return false;
    }
    const int job = actor->m_job;
    if (!((job >= 45 && job < 4000) || (job > 6000 && job < 6048) || actor->m_objectType == 9)) {
        return false;
    }
    if (g_windowMgr.m_chatRoomMenuWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
    }
    m_menuIdList = {37, 38, 39};
    m_menuTargetAID = actor->m_gid;
    ro::UiMessageStrings table;
    table.Load();
    const int labels[] = {1112, 1113, 1114};
    const char* fallbacks[] = {"Homunculus Information", "Feed Homunculus", "Stand By"};
    std::vector<RoomMenuItem> items;
    for (std::size_t i = 0; i < m_menuIdList.size(); ++i) {
        items.push_back({m_menuIdList[i], table.Get(labels[i], fallbacks[i])});
    }
    auto* menu = new UIChatRoomMenuWnd(items, logicalX, logicalY, true);
    menu->Move(logicalX + 1, logicalY + 1);
    g_windowMgr.m_chatRoomMenuWnd = menu;
    g_windowMgr.m_children.push_back(menu);
    return true;
}
bool CGameMode::OpenMercenaryContextMenu(CGameActor* actor,int logicalX,int logicalY)
{
    // Ref MakeMenu: owned mercenary gets MENUID_MER_SHOWINFO/FOLLOW.
    if (!m_world || !actor || g_windowMgr.m_storageWnd || g_windowMgr.m_modalWindow
        || !g_session.m_merStatus.alive || actor->m_gid != g_session.m_merGID
        || m_world->GetGameActorByAID(actor->m_gid) != actor) {
        return false;
    }
    const int job = actor->m_job;
    if (!((job >= 45 && job < 4000) || (job > 6000 && job < 6048) || actor->m_objectType == 9)) {
        return false;
    }
    if (g_windowMgr.m_chatRoomMenuWnd) {
        g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
    }
    m_menuIdList = {40, 41};
    m_menuTargetAID = actor->m_gid;
    ro::UiMessageStrings table;
    table.Load();
    const int labels[] = {1112, 1114};
    const char* fallbacks[] = {"Mercenary Information", "Stand By"};
    std::vector<RoomMenuItem> items;
    for (std::size_t i = 0; i < m_menuIdList.size(); ++i) {
        items.push_back({m_menuIdList[i], table.Get(labels[i], fallbacks[i])});
    }
    auto* menu = new UIChatRoomMenuWnd(items, logicalX, logicalY, true);
    menu->Move(logicalX + 1, logicalY + 1);
    g_windowMgr.m_chatRoomMenuWnd = menu;
    g_windowMgr.m_children.push_back(menu);
    return true;
}
bool CGameMode::WriteSourceChat(const std::string& text)
{
    if(!g_session.m_isSaveChat || !CanAutoSaveSourceChat())return false;
    if(m_sourceAutoSaveChatCount<INT_MAX)++m_sourceAutoSaveChatCount;
    std::ofstream file(SourceAutoSaveChatFile(),std::ios::binary|std::ios::app);if(!file)return false;
    const std::string value(text.c_str());file.write(value.c_str(),static_cast<std::streamsize>(value.size()+1));file.write("\r\n",2);
    file.close();return static_cast<bool>(file);
}

#include "SourceChatSubmission.inc"

msgresult_t CGameMode::SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    switch (msg) {
    case 47: {
        const std::array<std::string,3> empty{};
        return ProcessSourceTalk(static_cast<int>(wparam),lparam?*reinterpret_cast<const std::array<std::string,3>*>(lparam):empty);
    }
    case 6: case 72: case 142: case 271:
        return SendBufferedSourceChat(msg)?1:0;
    case 11:
        return wparam && SendBufferedSourceChat(11,reinterpret_cast<const char*>(wparam))?1:0;
    case 33: {
        constexpr u32 cooldownMs = 2500;
        constexpr u32 replyTimeoutMs = 5000;
        if (wparam < 0 || wparam > 80) return 0;
        const u32 now = GetSourceChatTick();
        const u32 elapsed = now - m_reqEmotionTick;
        if (elapsed <= cooldownMs || (m_isReqEmotion && elapsed < replyTimeoutMs)) return 0;
        m_isReqEmotion = 0;
        const u8 packet[]={0xbf,0,static_cast<u8>(wparam)};
        if (!CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet))) return 0;
        m_reqEmotionTick = now;
        m_isReqEmotion = 1;
        return 1;
    }
    case 67: {
        const u8 packet[]={0x00,0x01};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet),sizeof(packet))?1:0;
    }
    case 34:
        if(g_serviceType==ServiceChina || g_serviceType==ServiceMalaysia || g_serviceType==ServiceSingapore)return 0;
        [[fallthrough]];
    case 83:case 194:case 195:case 197:case 201:case 262: {
        // Selected mode recipients, not TALKTYPE numbers. These requests are
        // fixed two-byte source opcodes, independent of movement profiles.
        const u16 opcode=msg==34?193:msg==83?285:msg==194?535:msg==195?536:msg==197?549:msg==201?567:633;
        const std::array<u8,2> packet{{static_cast<u8>(opcode),static_cast<u8>(opcode>>8)}};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 152:return SendChangeDirRequestPacket(static_cast<u16>(lparam),static_cast<u8>(wparam),false)?1:0;
    case 155: {
        if (wparam != 0 && wparam != 1) return 0;
        const u8 packet[]{0x97, 0x01, static_cast<u8>(wparam), 0};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet)) ? 1 : 0;
    }
    case 158: {
        if(!wparam)return 0;
        // Selected CZ_RENAME_PET: the24-byte field ends in NUL when the
        // source truncates a long name. Zero the otherwise undefined tail.
        std::array<u8,26> packet{};packet[0]=0xa5;packet[1]=0x01;
        const char* name=reinterpret_cast<const char*>(wparam);
        std::size_t count=0;while(count<23 && name[count])++count;
        std::memcpy(packet.data()+2,name,count);
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 161: {
        const s32 action = static_cast<s32>(wparam);
        // Ref's show-level 3 master check has no reachable allow branch.
        if (action == -1 || (action >= 50000 && g_session.m_petStatus.relationship / 10 < 90)) {
            return 0;
        }
        const u32 bits = static_cast<u32>(action);
        const u8 packet[] = {0xa9, 0x01, static_cast<u8>(bits), static_cast<u8>(bits >> 8),
            static_cast<u8>(bits >> 16), static_cast<u8>(bits >> 24)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet)) ? 1 : 0;
    }
    case 163: {
        // CZ_COMMAND_PET takes the source argument's low byte. Permission,
        // confirmation and local egg/window changes belong to the callers.
        const u8 packet[]={0xa1,0x01,static_cast<u8>(wparam)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet),sizeof(packet))?1:0;
    }
    case 270:
        return SendPrivacyConfiguration(0, wparam != 0);
    case 190: {
        const auto aid = static_cast<u32>(wparam);
        const auto cid = static_cast<u32>(lparam);
        const auto& friends = g_session.GetFriendList();
        if (!aid || !cid || std::none_of(friends.begin(), friends.end(), [&](const FRIEND_INFO& entry) {
            return entry.isValid && entry.AID == aid && entry.GID == cid;
        })) return 0;
        std::array<u8, 10> packet{0x03, 0x02};
        for (unsigned i = 0; i < 4; ++i) {
            packet[2 + i] = static_cast<u8>(aid >> (8 * i));
            packet[6 + i] = static_cast<u8>(cid >> (8 * i));
        }
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), packet.size());
    }
    case 198: {
        // CZ_COMMAND_HOMUN carries a zero type and a one-byte command.
        const u8 packet[] = {0x2d, 0x02, 0, 0, static_cast<u8>(wparam)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet)) ? 1 : 0;
    }
    case 199: {
        if (!wparam) return 0;
        std::array<u8, 26> packet{0x31, 0x02};
        const char* name = reinterpret_cast<const char*>(wparam);
        std::size_t count = 0;
        while (count < 23 && name[count]) ++count;
        std::memcpy(packet.data() + 2, name, count);
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size())) ? 1 : 0;
    }
    case 200: {
        const u8 packet[] = {0x9f, 0x02, static_cast<u8>(wparam)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet)) ? 1 : 0;
    }
    case 77:
        if(wparam)DispatchSourceGroundSkill(*reinterpret_cast<const std::array<int,4>*>(wparam));
        return 0;
    case 144:
        if(wparam)DispatchSourceGroundSkillText(*reinterpret_cast<const GroundSkillTextRequest*>(wparam));
        return 0;
    case 76:
        if(wparam==10000){
            if(static_cast<u32>(lparam)!=g_session.m_petStatus.actorId){
                auto* window=g_windowMgr.MakeWindow(UIWindowMgr::WID_PETTAMINGDECEIVEWND);
                if(window)window->SendMsg(nullptr,80,0,lparam,0);
            }
        }else DispatchSourcePlayerSkill(*this,static_cast<int>(wparam),static_cast<u32>(lparam),static_cast<int>(extra));
        return 0;
    case 150: {
        std::array<u8,sizeof(PACKET_CZ_ACTION_REQUEST_PACKETVER22)> packet{};int length=0;
        if(ro::net::BuildActiveActionRequestPacket(static_cast<u32>(lparam),static_cast<u8>(wparam),packet.data(),static_cast<int>(packet.size()),&length))
            CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),length);
        return 0;
    }
    case 78:
        m_skillUseInfo.useType=0;m_skillUseInfo.id=0;m_skillUseInfo.level=0;g_isGoodSkillHanging=false;
        return 0;
    case 79: {
        if(ro::commerce::state.vendingActive)return 0;
        m_skillUseInfo.useType=static_cast<int>(wparam);
        if(!lparam)return 0; // A null row changes only the use type.
        const auto& row=*reinterpret_cast<const PLAYER_SKILL_INFO*>(lparam);
        m_skillUseInfo.id=row.SKID;m_skillUseInfo.attackRange=row.attackRange;
        g_skillMgr.EnsureLoaded();
        if(g_skillMgr.IsLevelUseSkill(row.SKID))m_skillUseInfo.level=static_cast<int>(extra);
        else{
            const auto* learned=row.SKID>=8000 && row.SKID<8017?g_session.GetHomunSkillItemBySkillId(row.SKID)
                :SKILL_INFO::IsMercenarySkill(row.SKID)?g_session.GetMercSkillItemBySkillId(row.SKID):g_session.GetSkillItemBySkillId(row.SKID);
            m_skillUseInfo.level=learned && learned->m_isValid?learned->level:0;
        }
        g_isGoodSkillHanging=SKILL_INFO::IsGoodSkillForActor(row.SKID);
        return 0;
    }
    case 126: {
        if(!wparam)return 0;
        const auto& row=*reinterpret_cast<const PLAYER_SKILL_INFO*>(wparam);
        switch(row.type){
        case 1:SendMsg(79,2,wparam,lparam);break;
        case 2:SendMsg(79,1,wparam,lparam);break;
        case 4:{
            const u32 target=row.SKID>=8000 && row.SKID<8017?g_session.m_homunGID
                :SKILL_INFO::IsMercenarySkill(row.SKID)?g_session.m_merGID:g_session.m_aid;
            SendMsg(76,row.SKID,target,lparam);SendMsg(79,0,0,0);break;
        }
        case 8:SendMsg(79,3,wparam,lparam);break;
        case 16:SendMsg(79,4,wparam,lparam);break;
        case 32:SendMsg(79,5,wparam,lparam);break;
        default:break;
        }
        return 0;
    }
    case 159: {
        const u16 index=static_cast<u16>(wparam);
        const u8 packet[]={0xa7,0x01,static_cast<u8>(index),static_cast<u8>(index>>8)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet),sizeof(packet))?1:0;
    }
    case 162: {
        return CRagConnection::instance()->SendPetCaptureRequest(static_cast<u32>(wparam)) ? 1 : 0;
    }
    case 183: {
        const u8 packet[]={0xe7,0x01};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet),sizeof(packet))?1:0;
    }
    case 114:
        if(!wparam)return 0;
        m_lastWhisperMenuCharacterName=reinterpret_cast<const char*>(wparam);m_menuIdList={11,21};return 0;
    case 26: {
        // Source mode dispatch uses current menu state after removing menu18.
        if(g_windowMgr.m_chatRoomMenuWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_chatRoomMenuWnd);
        if(g_windowMgr.m_storageWnd || wparam<0 || static_cast<std::size_t>(wparam)>=m_menuIdList.size())return 0;
        if(m_menuIdList[wparam]==11)return g_windowMgr.WriteWhisperChatToFile(m_lastWhisperMenuCharacterName)?1:0;
        if(m_menuIdList[wparam]==21)return g_windowMgr.AddFriendCharName(m_lastWhisperMenuCharacterName)?1:0;
        switch(m_menuIdList[wparam]){
        case 29:UIPetInfoWnd::ConfirmFeed();break;
        case 30:SendMsg(163,2,0,0);break;
        case 31:SendMsg(163,3,0,0);UIPetInfoWnd::ReturnEggLocally();break;
        case 32:SendMsg(163,4,0,0);break;
        case 33:
            SendMsg(163,0,0,0);
            if(g_windowMgr.m_petInfoWnd)g_windowMgr.DeleteWindow(g_windowMgr.m_petInfoWnd);
            else g_windowMgr.MakeWindow(UIWindowMgr::WID_PETINFOWND);
            break;
        case 37: // MENUID_HOMUN_SHOWINFO
            SendMsg(198, 0, 0, 0);
            if (g_windowMgr.m_homunInfoWnd) {
                g_windowMgr.DeleteWindow(static_cast<UIWindow*>(g_windowMgr.m_homunInfoWnd));
            } else {
                g_windowMgr.MakeWindow(UIWindowMgr::WID_HOMUNINFOWND);
            }
            break;
        case 38: { // MENUID_HOMUN_FEEDING
            if (g_windowMgr.m_modalWindow) break;
            ro::UiMessageStrings table;
            table.Load();
            const auto text = table.Get(601, "Are you sure that you want to feed your pet?");
            const auto generation = g_modeMgr.GetOperationGeneration();
            const u32 homunGid = g_session.m_homunGID;
            auto* box = new UIChatRoomMessageBox(text, 601);
            box->ConfigureConfirmation(nullptr, [this, generation, homunGid](bool accepted) {
                if (accepted && g_modeMgr.GetOperationGeneration() == generation
                    && homunGid != 0 && homunGid == g_session.m_homunGID) {
                    SendMsg(198, 1, 0, 0);
                }
            });
            g_windowMgr.m_roomMessageBox = box;
            g_windowMgr.m_children.push_back(box);
            g_windowMgr.m_modalWindow = box;
            g_windowMgr.m_editWindow = nullptr;
            break;
        }
        case 39: { // MENUID_HOMUN_FOLLOW
            companion_ai::Msg2Ai msg;
            msg.command = companion_ai::FOLLOW_CMD;
            companion_ai::Runtime::QueueCommand(companion_ai::Kind::Homunculus, msg, false);
            break;
        }
        case 40: // MENUID_MER_SHOWINFO
            SendMsg(200, 1, 0, 0);
            if (g_windowMgr.m_merInfoWnd) {
                g_windowMgr.DeleteWindow(static_cast<UIWindow*>(g_windowMgr.m_merInfoWnd));
            } else {
                g_windowMgr.MakeWindow(UIWindowMgr::WID_MERINFOWND);
            }
            break;
        case 41: { // MENUID_MER_FOLLOW
            companion_ai::Msg2Ai msg;
            msg.command = companion_ai::FOLLOW_CMD;
            companion_ai::Runtime::QueueCommand(companion_ai::Kind::Mercenary, msg, false);
            break;
        }
        }
        return 0;
    }
    case 51:case 52: {
        if(!wparam)return 0;const std::string_view name(reinterpret_cast<const char*>(wparam));
        std::vector<std::uint8_t> packet;
        const bool built=msg==51?ro::chat::BuildRoomRoleRequest(static_cast<u32>(lparam),name,packet):ro::chat::BuildRoomKickRequest(name,packet);
        return built && CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),static_cast<int>(packet.size()))?1:0;
    }
    case 48:case 49:return wparam && SendChatRoomSettings(*reinterpret_cast<const ro::chat::RoomInfo*>(wparam),msg==49)?1:0;
    case 50:return RequestEnterChatRoom(static_cast<u32>(wparam),reinterpret_cast<const char*>(lparam),GetTickCount())?1:0;
    case 53: {
        if(!g_windowMgr.m_chatRoomWnd)return 0;
        const char bytes[]={static_cast<char>(0xe3),0};
        return CRagConnection::instance()->SendPacket(bytes,2)?1:0;
    }

    case 99:
        ShowSkillHeadBubble(*this,
            static_cast<int>(wparam),
            reinterpret_cast<CGameActor*>(lparam),
            static_cast<int>(extra));
        return 0;
    case 113:
        UpdateShowImageInfo(*this, reinterpret_cast<const char*>(wparam), static_cast<int>(lparam));
        return 0;
    case GameMsg_LButtonDown: {
        const int mouseX = static_cast<int>(wparam);
        const int mouseY = static_cast<int>(lparam);
        if(m_skillUseInfo.useType!=0){
            if(!m_isOnQuest && !m_isPlayerDead && !g_windowMgr.HasActiveNpcDialog()) {
                TryRequestPendingSkillFromScreenPoint(*this,mouseX,mouseY);
            }
            // Source ProcessInput sends mode78 after actor/ground picking,
            // including a miss. Character targeting never starts ground move.
            ClearPendingSkillUse(*this);
            m_isLeftButtonHeld=0;m_hasHeldMoveTarget=0;
            return 1;
        }
        if (m_isOnQuest || g_windowMgr.HasActiveNpcDialog()) {
            m_isLeftButtonHeld = 0;
            m_hasHeldMoveTarget = 0;
            m_lastMoveRequestCellX = -1;
            m_lastMoveRequestCellY = -1;
            m_lastMoveRequestTick = 0;
            m_lastAttackRequestTick = 0;
            ClearPickupIntent(*this);
            ClearAttackChaseHint(*this);
            ClearSkillChase(*this);
            return 1;
        }
        // A targeting cursor owns the click, including an invalid target.
        // Holding a shortcut modifier must not turn, move or attack instead.
        if (HasPendingSkillUse(*this)) {
            TryRequestPendingSkillFromScreenPoint(*this, mouseX, mouseY);
            m_isLeftButtonHeld = 0;
            m_hasHeldMoveTarget = 0;
            m_lastMoveRequestCellX = -1;
            m_lastMoveRequestCellY = -1;
            m_lastMoveRequestTick = 0;
            m_lastAttackRequestTick = 0;
            ClearPickupIntent(*this);
            ClearAttackChaseHint(*this);
            return 1;
        }

        if (m_world && m_world->m_player) {
            if (TryHandleCompanionActorClick(*this, companion_ai::Kind::Mercenary, mouseX, mouseY)
                || TryHandleCompanionGroundClick(*this, companion_ai::Kind::Mercenary, mouseX, mouseY)) {
                m_isLeftButtonHeld = 0;
                m_hasHeldMoveTarget = 0;
                return 1;
            }
            if (ShouldUseTurnOnlyGroundClick(*this)) {
                ClearPickupIntent(*this);
                m_lastMonGid = 0;
                m_lastLockOnMonGid = 0;
                m_isLeftButtonHeld = 0;
                m_hasHeldMoveTarget = 0;
                m_lastMoveRequestCellX = -1;
                m_lastMoveRequestCellY = -1;
                m_lastMoveRequestTick = 0;
                m_lastAttackRequestTick = 0;
                ClearAttackChaseHint(*this);
                ClearSkillChase(*this);
                TryRequestChangeDirFromScreenPoint(*this, mouseX, mouseY);
                return 1;
            }
        }

        if (TryRequestGroundItemFromScreenPoint(*this, mouseX, mouseY)) {
            ClearPendingSkillUse(*this);
            ClearSkillChase(*this);
            m_isLeftButtonHeld = 0;
            m_hasHeldMoveTarget = 0;
            return 1;
        }
        if (TryRequestAttackFromScreenPoint(*this, mouseX, mouseY)) {
            ClearPendingSkillUse(*this);
            m_isLeftButtonHeld = 0;
            return 1;
        }
        if (TryRequestNpcTalkFromScreenPoint(*this, mouseX, mouseY)) {
            ClearPendingSkillUse(*this);
            return 1;
        }

        if (TrySelectPcTargetFromScreenPoint(*this, mouseX, mouseY)) {
            ClearPendingSkillUse(*this);
            m_isLeftButtonHeld = 0;
            m_hasHeldMoveTarget = 0;
            m_lastMoveRequestCellX = -1;
            m_lastMoveRequestCellY = -1;
            m_lastMoveRequestTick = 0;
            m_lastAttackRequestTick = 0;
            return 1;
        }

        ClearPickupIntent(*this);
        m_lastMonGid = 0;
        m_lastLockOnMonGid = 0;
        m_leftBtnClickTick = GetTickCount();
        m_isLeftButtonHeld = 1;
        m_lastMoveRequestCellX = -1;
        m_lastMoveRequestCellY = -1;
        m_lastMoveRequestTick = 0;
        m_lastAttackRequestTick = 0;
        ClearAttackChaseHint(*this);
        ClearSkillChase(*this);
        UpdateHeldMoveTargetFromScreenPoint(*this, mouseX, mouseY);
        PumpHeldMoveRequest(*this);
        return 1;
    }

    case GameMsg_LButtonUp:
        m_isLeftButtonHeld = 0;
        m_lastMoveRequestCellX = -1;
        m_lastMoveRequestCellY = -1;
        m_heldMoveTargetCellX = -1;
        m_heldMoveTargetCellY = -1;
        m_hasHeldMoveTarget = 0;
        m_lastMoveRequestTick = 0;
        m_lastAttackRequestTick = 0;
        ClearAttackChaseHint(*this);
        return 1;

    case GameMsg_MouseMove: {
        const int mouseX = static_cast<int>(wparam);
        const int mouseY = static_cast<int>(lparam);
        if (m_view && !m_canRotateView) {
            m_view->UpdateHoverCellFromScreen(mouseX, mouseY);
            if (m_isLeftButtonHeld) {
                UpdateHeldMoveTargetFromScreenPoint(*this, mouseX, mouseY);
            }
        }
        if (m_canRotateView && m_view) {
            if (mouseX != m_oldMouseX || mouseY != m_oldMouseY) {
                m_rButtonDragged = 1;
            }
            const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            m_view->RotateByDrag(mouseX - m_oldMouseX, shiftHeld ? mouseY - m_oldMouseY : 0);
            m_view->ClearHoverCell();
        }
        m_oldMouseX = mouseX;
        m_oldMouseY = mouseY;
        return 1;
    }

    case GameMsg_RButtonDown:
    {
        if (HasPendingSkillUse(*this) || m_hasSkillChase) {
            ClearPendingSkillUse(*this);
            ClearSkillChase(*this);
            return 1;
        }
        const int mouseX = static_cast<int>(wparam);
        const int mouseY = static_cast<int>(lparam);
        if (TryHandleCompanionActorClick(*this, companion_ai::Kind::Homunculus, mouseX, mouseY)
            || TryHandleCompanionGroundClick(*this, companion_ai::Kind::Homunculus, mouseX, mouseY)) {
            // Suppress context-menu opening on release just as for a consumed camera drag.
            m_rButtonDragged = 1;
            return 1;
        }
        const u32 now = GetTickCount();
        const int doubleClickWidth = (std::max)(4, GetSystemMetrics(SM_CXDOUBLECLK));
        const int doubleClickHeight = (std::max)(4, GetSystemMetrics(SM_CYDOUBLECLK));
        const bool withinDoubleClickTime = m_lastRButtonClickTick != 0
            && now - m_lastRButtonClickTick <= GetDoubleClickTime();
        const bool withinDoubleClickRect = (std::abs)(mouseX - m_lastRButtonClickX) <= doubleClickWidth
            && (std::abs)(mouseY - m_lastRButtonClickY) <= doubleClickHeight;
        if (withinDoubleClickTime && withinDoubleClickRect) {
            m_lastRButtonClickTick = 0;
            m_rButtonDragged = 0;
            m_canRotateView = 0;
            if (m_view) {
                m_view->ResetToDefaultOrientation();
                m_view->ClearHoverCell();
            }
            if (GetCapture() == g_hMainWnd) {
                ReleaseCapture();
            }
            m_oldMouseX = mouseX;
            m_oldMouseY = mouseY;
            return 1;
        }

        m_canRotateView = 1;
        m_rBtnClickX = mouseX;
        m_rBtnClickY = mouseY;
        m_oldMouseX = mouseX;
        m_oldMouseY = mouseY;
        m_rButtonDragged = 0;
        if (m_view) {
            m_view->ClearHoverCell();
        }
        if (g_hMainWnd && GetCapture() != g_hMainWnd) {
            SetCapture(g_hMainWnd);
        }
        return 1;
    }

    case GameMsg_SubmitChat: {
        if(!wparam)return 0;
        auto* origin=reinterpret_cast<UIWindow*>(extra);
        if(origin && std::find(g_windowMgr.m_children.begin(),g_windowMgr.m_children.end(),origin)==g_windowMgr.m_children.end())origin=nullptr;
        return SubmitSourceChat(reinterpret_cast<const char*>(wparam),lparam?reinterpret_cast<const char*>(lparam):"",origin)?1:0;
    }

    case GameMsg_ToggleSitStand:
        return SendSitStandToggleRequest(*this) ? 1 : 0;

    case GameMsg_RequestReturnToCharSelect:
        return RequestReturnToCharSelect() ? 1 : 0;

    case GameMsg_RequestExitToWindows:
        return RequestExitToWindows() ? 1 : 0;

    case GameMsg_RequestReturnToSavePoint:
        return RequestReturnToSavePoint() ? 1 : 0;

    case GameMsg_RequestStandingResurrection: {
        if (!CanRequestStandingResurrection()) {
            return 0;
        }
        // CZ_STANDING_RESURRECTION has no payload.
        const char packet[]{static_cast<char>(0x92), 0x02};
        return CRagConnection::instance()->SendPacket(packet, sizeof(packet)) ? 1 : 0;
    }

    case GameMsg_RequestEquipInventoryItem:
        return RequestEquipInventoryItem(
            static_cast<u16>(wparam),
            static_cast<u16>(lparam), static_cast<unsigned>(extra)) ? 1 : 0;

    case GameMsg_RequestUnequipInventoryItem:
        return RequestUnequipInventoryItem(static_cast<u16>(wparam), static_cast<unsigned>(extra)) ? 1 : 0;

    case GameMsg_RequestUseInventoryItem:
        return RequestUseInventoryItem(static_cast<u16>(wparam)) ? 1 : 0;

    case GameMsg_RequestDropInventoryItem:
        return RequestDropInventoryItem(static_cast<u16>(wparam), static_cast<u16>(lparam), static_cast<unsigned>(extra)) ? 1 : 0;

    case GameMsg_RequestUpgradeSkillLevel:
        return RequestUpgradeSkillLevel(static_cast<u16>(wparam)) ? 1 : 0;

    case GameMsg_RequestIncreaseStatus:
        return RequestIncreaseStatus(static_cast<u16>(wparam)) ? 1 : 0;

    case GameMsg_RequestNpcContact:
        return RequestNpcContact(static_cast<u32>(wparam)) ? 1 : 0;

    case GameMsg_RequestNpcSelectMenu:
        return RequestNpcMenuSelection(static_cast<u32>(wparam), static_cast<u8>(lparam)) ? 1 : 0;

    case GameMsg_RequestNpcNext:
        return RequestNpcNext(static_cast<u32>(wparam)) ? 1 : 0;

    case GameMsg_RequestNpcInputNumber:
        return RequestNpcInputNumber(static_cast<u32>(wparam), static_cast<u32>(lparam)) ? 1 : 0;

    case GameMsg_RequestNpcInputString:
        return RequestNpcInputString(static_cast<u32>(wparam), reinterpret_cast<const char*>(lparam)) ? 1 : 0;

    case GameMsg_RequestNpcCloseDialog:
        return RequestNpcCloseDialog(static_cast<u32>(wparam)) ? 1 : 0;

    case GameMsg_RequestShopDealType:
        if (wparam != 0 && wparam != 1) return 0;
        return RequestNpcShopDealType(static_cast<u8>(wparam)) ? 1 : 0;

    case GameMsg_RequestShopBuyList:
        return RequestNpcShopPurchaseList() ? 1 : 0;

    case GameMsg_ConfirmCashShopContribution:
        return ConfirmCashShopContribution(static_cast<u32>(wparam), static_cast<u32>(lparam), static_cast<u32>(extra)) ? 1 : 0;

    case GameMsg_RequestShopSellList:
        return RequestNpcShopSellList() ? 1 : 0;

    case GameMsg_RequestShopCancel:
        return RequestNpcShopCancel() ? 1 : 0;

    case GameMsg_RequestShopAdjustDeal: {
        const unsigned int packed = static_cast<unsigned int>(wparam);
        const int sourceRow = static_cast<int>(packed & 0xFFFFu);
        const int quantity = static_cast<int>(lparam);
        if (sourceRow < 0 || quantity <= 0) {
            return 0;
        }
        const int delta = (packed & 0x10000u) != 0u ? -quantity : quantity;
        return g_session.AdjustNpcShopDealBySourceRow(static_cast<size_t>(sourceRow), delta) ? 1 : 0;
    }

    case GameMsg_RequestStorageStoreItem:
        return RequestMoveInventoryItemToStorage(static_cast<u16>(wparam), static_cast<u32>(lparam), static_cast<unsigned>(extra)) ? 1 : 0;

    case GameMsg_RequestStorageWithdrawItem:
        return RequestMoveStorageItemToInventory(static_cast<u16>(wparam), static_cast<u32>(lparam), static_cast<unsigned>(extra)) ? 1 : 0;

    case GameMsg_RequestStorageClose:
        return RequestCloseStorage() ? 1 : 0;

    case GameMsg_RequestShortcutUpdate:
        return RequestShortcutSlotUpdate(static_cast<int>(wparam),
            reinterpret_cast<const SHORTCUT_SLOT*>(lparam)) ? 1 : 0;

    case GameMsg_RequestShortcutUse:
        return RequestShortcutSlotUse(static_cast<int>(wparam)) ? 1 : 0;

    case GameMsg_RequestIdentifyInventoryItem:
        return RequestIdentifyInventoryItem(static_cast<s16>(wparam)) ? 1 : 0;

    case GameMsg_RequestComposeCardIntoEquipment:
        return RequestComposeCardIntoEquipment(static_cast<s16>(wparam), static_cast<s16>(lparam)) ? 1 : 0;

    case GameMsg_RequestStorageToCart: {
        ro::commerce::Command transfer;
        transfer.action = "transfer";
        transfer.from = "storage";
        transfer.to = "cart";
        transfer.index = static_cast<u32>(wparam);
        transfer.amount = static_cast<u32>(lparam);
        transfer.itemId = static_cast<u32>(extra);
        return ro::commerce::Action(transfer) ? 1 : 0;
    }
    case GameMsg_RequestCartOff:
        return RequestCartOff() ? 1 : 0;

    case GameMsg_RequestChangeCart:
        return RequestChangeCart(static_cast<u16>(wparam)) ? 1 : 0;

    case GameMsg_RequestPartyCreate: {
        const char* partyName = reinterpret_cast<const char*>(wparam);
        const unsigned int optionBits = static_cast<unsigned int>(lparam);
        const bool expShare = (optionBits & 0x01u) != 0;
        const bool itemShareParty = (optionBits & 0x02u) != 0;
        const bool itemSharingTypeShared = (optionBits & 0x04u) != 0;
        std::string name = NormalizePartyNameForPacket(partyName);
        if (!RequestCreateParty(name, itemShareParty, itemSharingTypeShared)) {
            return 0;
        }
        m_pendingPartyCreateName = std::move(name);
        m_pendingPartyCreateExpShare = expShare;
        return 1;
    }

    case GameMsg_RequestPartyChangeOptions: {
        const unsigned int optionBits = static_cast<unsigned int>(wparam);
        return RequestChangePartyOptions((optionBits & 0x01u) != 0) ? 1 : 0;
    }

    case GameMsg_RequestPartyDisband:
        return RequestDisbandParty() ? 1 : 0;

    case GameMsg_RequestQuestActive: {
        const u32 id = static_cast<u32>(wparam);
        const auto& states = g_session.m_questJournal.states;
        const auto quest = states.find(id);
        const auto active = lparam != 0 ? ro::quest::State::Active : ro::quest::State::Inactive;
        if (m_mapLoadingStage != MapLoading_None || quest == states.end() || quest->second == active) {
            return 0;
        }
        const u8 packet[] = {0xB6, 0x02, static_cast<u8>(id), static_cast<u8>(id >> 8),
            static_cast<u8>(id >> 16), static_cast<u8>(id >> 24), static_cast<u8>(active)};
        return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet));
    }

    case GameMsg_RequestPartyInvite:
        if (g_session.GetNumParty() == 0) {
            g_windowMgr.PushChatEvent("Create a party before inviting other players.", 0x00FF4040u, 6);
            return 0;
        }
        return RequestPartyInvite(static_cast<u32>(wparam)) ? 1 : 0;

    case GameMsg_RequestPartyInviteReply:
        return ReplyPartyInvite(static_cast<u32>(wparam), lparam != 0, extra != 0) ? 1 : 0;

    case GameMsg_RequestPlayerContextAction: {
        const PlayerContextAction action = static_cast<PlayerContextAction>(lparam);
        const u32 targetAid = static_cast<u32>(wparam);
        if (action == PlayerContext_RequestGuildAlliance || action == PlayerContext_DeclareGuildOpposition) {
            const bool hostile = action == PlayerContext_DeclareGuildOpposition;
            if (!CanRequestGuildRelation(*this, targetAid, hostile)) return 0;
            std::array<u8, 14> packet{static_cast<u8>(hostile ? 0x80 : 0x70), 1};
            for (unsigned i = 0; i < 4; ++i) {
                packet[2 + i] = static_cast<u8>(targetAid >> (8 * i));
                packet[6 + i] = static_cast<u8>(g_session.m_aid >> (8 * i));
                packet[10 + i] = static_cast<u8>(g_session.m_gid >> (8 * i));
            }
            return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet.data()),
                hostile ? 6 : static_cast<int>(packet.size()));
        }
        if (action == PlayerContext_RequestAdoption) {
            return CanRequestAdoption(targetAid) && SendAdoptionPacket(0xf9, targetAid);
        }
        if (action == PlayerContext_AskJoinParty) {
            DbgLog("[GameMode] player context action targetAid=%u action=%d (%s)\n",
                static_cast<unsigned int>(targetAid),
                static_cast<int>(action),
                GetPlayerContextActionName(action));
            return SendMsg(GameMsg_RequestPartyInvite, targetAid, 0, 0);
        }

        const auto entry=m_actorNameListByGID.find(targetAid);
        const std::string targetName=entry!=m_actorNameListByGID.end()?entry->second.name:std::string();
        const bool handled=ro::commerce::PlayerAction(static_cast<int>(action),targetAid,targetName);
        if(!handled&&ro::commerce::state.status.empty())g_windowMgr.PushChatEvent("That player action is unavailable right now.",0x00FF8080u,6);
        DbgLog("[GameMode] player context action targetAid=%u action=%d (%s)\n",
            static_cast<unsigned int>(targetAid),
            static_cast<int>(action),
            GetPlayerContextActionName(action));
        return handled?1:0;
    }

    case GameMsg_RButtonUp:
        m_canRotateView = 0;
        if (GetCapture() == g_hMainWnd) {
            ReleaseCapture();
        }
        if (!m_rButtonDragged) {
            m_lastRButtonClickTick = GetTickCount();
            m_lastRButtonClickX = static_cast<int>(wparam);
            m_lastRButtonClickY = static_cast<int>(lparam);
            const int screenX = static_cast<int>(wparam);
            const int screenY = static_cast<int>(lparam);
            bool openedCompanionMenu = false;
            if (m_world && m_view && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
                CGameActor* hoveredActor = nullptr;
                if (m_world->FindHoveredActorScreen(m_view->GetViewMatrix(),
                        m_view->GetCameraLongitude(),
                        screenX,
                        screenY,
                        &hoveredActor,
                        nullptr,
                        nullptr)
                    && hoveredActor) {
                    const int logicalX = IsQtUiRuntimeEnabled()
                        ? UiScaleRawToLogicalCoordinate(screenX)
                        : screenX;
                    const int logicalY = IsQtUiRuntimeEnabled()
                        ? UiScaleRawToLogicalCoordinate(screenY)
                        : screenY;
                    openedCompanionMenu = OpenHomunculusContextMenu(hoveredActor, logicalX, logicalY)
                        || OpenMercenaryContextMenu(hoveredActor, logicalX, logicalY);
                }
            }
            if (openedCompanionMenu) {
                g_windowMgr.ClosePlayerContextMenu();
            } else if (!TryOpenPlayerContextMenu(*this, screenX, screenY)) {
                g_windowMgr.ClosePlayerContextMenu();
                m_menuTargetAID = 0;
            }
        } else {
            m_lastRButtonClickTick = 0;
            g_windowMgr.ClosePlayerContextMenu();
            m_menuTargetAID = 0;
        }
        return 1;

    case GameMsg_MouseWheel:
        if (m_view) {
            m_view->ZoomByWheel(static_cast<int>(wparam));
        }
        return 1;

    case 135:
        // Ref/GameMode2.cpp case 0x87. val1 == 1 is the short shake.
        if (m_view) {
            const int shakeType = static_cast<int>(wparam);
            if (shakeType == 1) {
                m_view->SetQuake(true, 1, 1.0f, 0.24f, 0.2f);
            } else {
                m_view->SetQuake(true, 0, 1.0f, 0.24f, 0.2f);
            }
        }
        return 0;

    case 148:
        if (m_view != nullptr) {
            m_view->SetQuakeInfo(
                *reinterpret_cast<const float*>(wparam),
                *reinterpret_cast<const float*>(lparam),
                *reinterpret_cast<const float*>(extra));
        }
        return 0;

    case 179:
        SendTalkScriptLine(static_cast<int>(wparam));
        return 0;

    default:
        return 0;
    }
}
void CGameMode::OnChangeState(int newState) {}
