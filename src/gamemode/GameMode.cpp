#include "network/Connection.h"
#include "network/Packet.h"
#include "network/GronPacket.h"
#include "CursorRenderer.h"
#include "GameMode.h"
#include "View.h"
#include "audio/Audio.h"
#include "GameModePacket.h"
#include "pathfinder/PathFinder.h"
#include "core/ClientInfoLocale.h"
#include "core/File.h"
#include "DebugLog.h"
#include "render/DC.h"
#include "res/ActRes.h"
#include "res/GndRes.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "res/WorldRes.h"
#include "session/Session.h"
#include "ui/UIWindow.h"
#include "ui/UIWindowMgr.h"
#include "render/DrawUtil.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "world/GameActor.h"
#include "world/MsgEffect.h"
#include "world/3dActor.h"
#include "world/World.h"
#include "main/WinMain.h"

#include <gdiplus.h>
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

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace {
CGameModePacketRouter g_gameModePacketRouter;

constexpr char kGameModeBuildMarker[] = "2026-03-23 20:14 actor-trace-f";
constexpr u32 kFramePerfLogIntervalFrames = 120;
constexpr u32 kLoadingScreenMinShowMs = 700;
constexpr u32 kLoadingActorQuietMs = 250;
constexpr u32 kLoadingPostAckWaitMs = 450;
constexpr u32 kLoadingAckRetryDelayMs = 900;
constexpr u32 kPacketTraceWindowMs = 8000;
constexpr u32 kBootstrapLoadBudgetMs = 6;
constexpr size_t kBootstrapTextureBatch = 2;
constexpr size_t kBootstrapBackgroundActorBatch = 6;
constexpr size_t kBootstrapFixedEffectBatch = 12;
constexpr int kHoverNameFontHeight = 20;
constexpr unsigned char kHoverNameFontBold = 1;
constexpr int kHoverNameTextPadding = 4;
constexpr int kHoverNameVerticalOffset = 22;
constexpr u32 kPickupRetryIntervalMs = 250;
constexpr int kPlayerVitalsBarWidth = 56;
constexpr int kPlayerVitalsBarHeight = 4;
constexpr int kPlayerVitalsBorderThickness = 1;
constexpr int kPlayerVitalsNameTopPadding = 3;
constexpr int kPlayerVitalsVerticalOffset = 10;
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
constexpr float kLockedTargetArrowBouncePerMs = 0.0045f;
constexpr float kLockedTargetArrowBouncePixels = 4.0f;
constexpr u32 kHoverNameRequestCooldownMs = 1000;
constexpr size_t kNearbyBackgroundLogCount = 12;
constexpr float kRefCloseCameraDistance = 150.0f;
constexpr float kRefIndoorCameraDistance = 420.0f;
constexpr float kRefAverageCameraDistance = 500.0f;
constexpr float kRefFarCameraDistance = 700.0f;
constexpr float kRefIndoorLatitudeMin = -55.0f;
constexpr float kRefIndoorLatitudeMax = -35.0f;
constexpr float kRefOutdoorLatitudeMin = -65.0f;
constexpr float kRefOutdoorLatitudeMax = -25.0f;
constexpr u32 kAmbientSoundRetryMs = 200;
constexpr int kAmbientSoundMaxDist = 250;
constexpr int kAmbientSoundMinDist = 40;

float g_savedOutdoorCameraLatitude = -45.0f;
float g_savedOutdoorCameraDistance = kRefAverageCameraDistance;
float g_savedIndoorCameraLatitude = -45.0f;
float g_savedIndoorCameraDistance = kRefIndoorCameraDistance;

u32 g_packetTraceStartTick = 0;
std::map<u16, bool> g_packetTraceLoggedIds;

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
        const float volumeFactor = (std::max)(0.0f, sound->vol);
        const u32 cycleMs = sound->cycle > 0.0f ? static_cast<u32>(sound->cycle * 1000.0f) : kAmbientSoundRetryMs;
        bool found = false;
        for (PLAY_WAVE_INFO& playing : mode.m_playWaveList) {
            if (playing.wavName == wavName && playing.nAID == static_cast<u32>(soundIndex)) {
                found = true;
                if (playing.endTick <= now) {
                    if (audio->PlaySound3D(wavName.c_str(), sound->pos, listenerPos,
                            sound->range > 0.0f ? static_cast<int>(sound->range) : kAmbientSoundMaxDist,
                            kAmbientSoundMinDist,
                            volumeFactor)) {
                        playing.endTick = now + cycleMs;
                    } else {
                        playing.endTick = now + kAmbientSoundRetryMs;
                    }
                }
                break;
            }
        }

        if (!found) {
            PLAY_WAVE_INFO info{};
            info.wavName = wavName;
            info.nAID = static_cast<u32>(soundIndex);
            info.term = cycleMs;
            info.endTick = now;
            info.pos = sound->pos;
            info.volumeFactor = volumeFactor;
            info.volumeMaxDist = sound->range > 0.0f ? static_cast<int>(sound->range) : kAmbientSoundMaxDist;
            info.volumeMinDist = kAmbientSoundMinDist;
            mode.m_playWaveList.push_back(info);
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
void DrawPlayerVitalsOverlay(CGameMode& mode, HDC hdc);
void DrawGameplayOverlayToHdc(CGameMode& mode, HDC targetDc);
std::string ResolveHoveredActorName(CGameMode& mode, CGameActor* actor);

void ReleaseOverlayComposeSurface(HDC* composeDc, HBITMAP* composeBitmap, void** composeBits, int* composeWidth, int* composeHeight)
{
    if (composeBitmap && *composeBitmap) {
        DeleteObject(*composeBitmap);
        *composeBitmap = nullptr;
    }
    if (composeDc && *composeDc) {
        DeleteDC(*composeDc);
        *composeDc = nullptr;
    }
    if (composeBits) {
        *composeBits = nullptr;
    }
    if (composeWidth) {
        *composeWidth = 0;
    }
    if (composeHeight) {
        *composeHeight = 0;
    }
}

bool EnsureOverlayComposeSurface(HDC referenceDc, int width, int height,
    HDC* composeDc, HBITMAP* composeBitmap, void** composeBits, int* composeWidth, int* composeHeight)
{
    if (!referenceDc || !composeDc || !composeBitmap || !composeBits || !composeWidth || !composeHeight || width <= 0 || height <= 0) {
        return false;
    }

    if (*composeDc && *composeBitmap && *composeBits && *composeWidth == width && *composeHeight == height) {
        return true;
    }

    ReleaseOverlayComposeSurface(composeDc, composeBitmap, composeBits, composeWidth, composeHeight);

    *composeDc = CreateCompatibleDC(referenceDc);
    if (!*composeDc) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    *composeBitmap = CreateDIBSection(*composeDc, &bmi, DIB_RGB_COLORS, composeBits, nullptr, 0);
    if (!*composeBitmap || !*composeBits) {
        ReleaseOverlayComposeSurface(composeDc, composeBitmap, composeBits, composeWidth, composeHeight);
        return false;
    }

    SelectObject(*composeDc, *composeBitmap);
    *composeWidth = width;
    *composeHeight = height;
    return true;
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

bool HasAnimatedLockedTargetOverlay(const CGameMode& mode)
{
    if (!mode.m_world || !mode.m_view || mode.m_lastLockOnMonGid == 0) {
        return false;
    }

    const auto actorIt = mode.m_runtimeActors.find(mode.m_lastLockOnMonGid);
    return actorIt != mode.m_runtimeActors.end() && actorIt->second && actorIt->second->m_isVisible;
}

std::string ResolveGroundItemHoverLabel(const CItem* item);
void DrawHoveredGroundItemName(CGameMode& mode, HDC hdc);

std::uint64_t ComputeGameplayOverlayStateToken(CGameMode& mode, int cursorActNum, u32 mouseAnimStartTick, int clientWidth, int clientHeight)
{
    std::uint64_t hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<std::uint64_t>(clientWidth));
    HashTokenValue(&hash, static_cast<std::uint64_t>(clientHeight));
    HashTokenValue(&hash, static_cast<std::uint64_t>(cursorActNum));
    HashTokenValue(&hash, static_cast<std::uint64_t>(GetModeCursorVisualFrame(cursorActNum, mouseAnimStartTick)));

    POINT cursorPos{};
    if (g_hMainWnd && GetCursorPos(&cursorPos) && ScreenToClient(g_hMainWnd, &cursorPos)) {
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(cursorPos.x)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(cursorPos.y)));
    }

    if (mode.m_world && mode.m_view && mode.m_world->m_player) {
        int labelX = 0;
        int labelY = 0;
        if (mode.m_world->GetPlayerScreenLabel(mode.m_view->GetViewMatrix(), mode.m_view->GetCameraLongitude(), &labelX, &labelY)) {
            HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(labelX)));
            HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(labelY)));
        }

        const CPlayer* player = mode.m_world->m_player;
        HashTokenValue(&hash, static_cast<std::uint64_t>(player->m_gid));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_Hp)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_MaxHp)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_Sp)));
        HashTokenValue(&hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(player->m_MaxSp)));

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

    HashTokenValue(&hash, static_cast<std::uint64_t>(mode.m_lastLockOnMonGid));
    return hash;
}

bool QueueModernOverlayQuad(CGameMode& mode, int cursorActNum, u32 mouseAnimStartTick)
{
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

    HDC windowDc = GetDC(g_hMainWnd);
    if (!windowDc) {
        return false;
    }

    static HDC s_overlayComposeDc = nullptr;
    static HBITMAP s_overlayComposeBitmap = nullptr;
    static void* s_overlayComposeBits = nullptr;
    static int s_overlayComposeWidth = 0;
    static int s_overlayComposeHeight = 0;
    static std::uint64_t s_overlayStateToken = 0ull;
    static bool s_overlayTextureValid = false;
    const bool composeReady = EnsureOverlayComposeSurface(windowDc, clientWidth, clientHeight,
        &s_overlayComposeDc, &s_overlayComposeBitmap, &s_overlayComposeBits, &s_overlayComposeWidth, &s_overlayComposeHeight);
    ReleaseDC(g_hMainWnd, windowDc);
    if (!composeReady) {
        return false;
    }

    static CTexture* s_overlayTexture = nullptr;
    static int s_overlayTextureWidth = 0;
    static int s_overlayTextureHeight = 0;
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

    const bool overlayIsAnimated = HasAnimatedLockedTargetOverlay(mode) || HasQueuedMsgEffects();
    const bool uiDirty = g_windowMgr.HasDirtyVisualState();
    const std::uint64_t overlayStateToken = ComputeGameplayOverlayStateToken(mode, cursorActNum, mouseAnimStartTick, clientWidth, clientHeight);
    const bool needOverlayRefresh = !s_overlayTextureValid || overlayIsAnimated || uiDirty || overlayStateToken != s_overlayStateToken;

    if (needOverlayRefresh) {
        ClearOverlayComposeBits(s_overlayComposeBits, clientWidth, clientHeight);
        DrawGameplayOverlayToHdc(mode, s_overlayComposeDc);

        HDC previousSharedDc = UIWindow::GetSharedDrawDC();
        UIWindow::SetSharedDrawDC(s_overlayComposeDc);
        g_windowMgr.OnDraw();
        UIWindow::SetSharedDrawDC(previousSharedDc);
        DrawModeCursorToHdc(s_overlayComposeDc, cursorActNum, mouseAnimStartTick);

        ConvertOverlayComposeBitsToAlpha(s_overlayComposeBits, clientWidth, clientHeight);
        s_overlayTexture->Update(0,
            0,
            clientWidth,
            clientHeight,
            static_cast<unsigned int*>(s_overlayComposeBits),
            true,
            clientWidth * static_cast<int>(sizeof(unsigned int)));
        s_overlayTextureValid = true;
        s_overlayStateToken = overlayStateToken;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float right = static_cast<float>(clientWidth) - 0.5f;
    const float bottom = static_cast<float>(clientHeight) - 0.5f;
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
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 1.0f;

    face->m_verts[0] = { -0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, -0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { -0.5f, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

void DrawGameplayOverlayToHdc(CGameMode& mode, HDC targetDc)
{
    if (!targetDc) {
        return;
    }

    DrawPlayerVitalsOverlay(mode, targetDc);
    DrawLockedTargetArrow(mode, targetDc);
    DrawLockedTargetName(mode, targetDc);
    DrawHoveredGroundItemName(mode, targetDc);
    DrawHoveredActorName(mode, targetDc);
    DrawQueuedMsgEffects(targetDc);
}

bool ComposeModernOverlayFrame(CGameMode& mode, int cursorActNum, u32 mouseAnimStartTick)
{
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

    HDC sceneDc = nullptr;
    if (!GetRenderDevice().AcquireBackBufferDC(&sceneDc) || !sceneDc) {
        return false;
    }

    static HDC s_overlayComposeDc = nullptr;
    static HBITMAP s_overlayComposeBitmap = nullptr;
    static void* s_overlayComposeBits = nullptr;
    static int s_overlayComposeWidth = 0;
    static int s_overlayComposeHeight = 0;
    if (!EnsureOverlayComposeSurface(sceneDc, clientWidth, clientHeight,
        &s_overlayComposeDc, &s_overlayComposeBitmap, &s_overlayComposeBits, &s_overlayComposeWidth, &s_overlayComposeHeight)) {
        GetRenderDevice().ReleaseBackBufferDC(sceneDc);
        return false;
    }

    BitBlt(s_overlayComposeDc, 0, 0, clientWidth, clientHeight, sceneDc, 0, 0, SRCCOPY);
    DrawGameplayOverlayToHdc(mode, s_overlayComposeDc);

    HDC previousSharedDc = UIWindow::GetSharedDrawDC();
    UIWindow::SetSharedDrawDC(s_overlayComposeDc);
    g_windowMgr.OnDraw();
    DrawModeCursorToHdc(s_overlayComposeDc, cursorActNum, mouseAnimStartTick);
    UIWindow::SetSharedDrawDC(previousSharedDc);
    GetRenderDevice().ReleaseBackBufferDC(sceneDc);

    return GetRenderDevice().UpdateBackBufferFromMemory(
        s_overlayComposeBits,
        clientWidth,
        clientHeight,
        clientWidth * static_cast<int>(sizeof(unsigned int)));
}

bool RequestReturnToCharSelect()
{
    PACKET_CZ_RESTART packet{};
    packet.PacketType = PACKETID_CZ_RESTART;
    packet.Type = 1;

    SetPendingDisconnectAction(PendingDisconnectAction::ReturnToCharSelect);
    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    if (!sent) {
        ClearPendingDisconnectAction();
    }
    return sent;
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

void RequestExitToWindows()
{
    PACKET_CZ_QUITGAME packet{};
    packet.PacketType = PACKETID_CZ_QUITGAME;
    packet.Type = 0;
    CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    CRagConnection::instance()->Disconnect();
    g_modeMgr.Quit();
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
bool LoadBitmapFromGameData(const std::string& dataPath, HBITMAP* outBmp, int* outWidth, int* outHeight);
void DrawBitmapTransparent(HDC targetDC, HBITMAP bitmap, const RECT& dst);

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

    PACKET_CZ_REQNAME2 packet{};
    packet.PacketType = PacketProfile::ActiveMapServerSend::kGetCharNameRequest;
    packet.GID = gid;
    if (CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), static_cast<int>(sizeof(packet)))) {
        mode.m_actorNameByGIDReqTimer[gid] = now;
    }
}

bool IsMonsterLikeHoverActor(const CGameActor* actor)
{
    if (!actor) {
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

void SetModeCursorAction(CGameMode& mode, CursorAction cursorActNum)
{
    mode.SetCursorAction(cursorActNum);
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

    CGameActor* hoveredActor = nullptr;
    if (!mode.m_world->FindHoveredActorScreen(mode.m_view->GetViewMatrix(),
        mode.m_view->GetCameraLongitude(),
        mode.m_oldMouseX,
        mode.m_oldMouseY,
        &hoveredActor,
        nullptr,
        nullptr)) {
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
    if (hoveredActor == mode.m_world->m_player || hoveredActor->m_gid == g_session.m_gid) {
            drawY += kPlayerVitalsBarHeight * 2 + kPlayerVitalsBorderThickness * 3 + kPlayerVitalsNameTopPadding - 10;
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
    static HBITMAP s_bitmap = nullptr;
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
            if (LoadBitmapFromGameData(s_bitmapPath, &s_bitmap, &s_width, &s_height) && s_bitmap) {
                DbgLog("[GameMode] locked target arrow loaded path='%s' size=%dx%d\n",
                    s_bitmapPath.c_str(),
                    s_width,
                    s_height);
            } else {
                DbgLog("[GameMode] locked target arrow bitmap decode failed path='%s'\n",
                    s_bitmapPath.c_str());
            }
        } else {
            DbgLog("[GameMode] locked target arrow asset not found name='%s'\n",
                kLockedTargetArrowBitmapName);
        }
    }

    if (!s_bitmap || s_width <= 0 || s_height <= 0) {
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
    RECT dst{ drawX, drawY, drawX + scaledWidth, drawY + scaledHeight };
    DrawBitmapTransparent(hdc, s_bitmap, dst);
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
    const int maxHp = static_cast<int>(player->m_MaxHp);
    const int maxSp = static_cast<int>(player->m_MaxSp);
    if (maxHp <= 0 && maxSp <= 0) {
        return;
    }

    const int totalHeight = kPlayerVitalsBarHeight * 2 + kPlayerVitalsBorderThickness * 3;
    const int outerLeft = labelX - ((kPlayerVitalsBarWidth + kPlayerVitalsBorderThickness * 2) / 2);
    const int outerTop = labelY;
    RECT outerRect{
        outerLeft,
        outerTop,
        outerLeft + kPlayerVitalsBarWidth + kPlayerVitalsBorderThickness * 2,
        outerTop + totalHeight,
    };
    FillSolidRect(hdc, outerRect, RGB(0, 0, 0));

    const int innerLeft = outerRect.left + kPlayerVitalsBorderThickness;
    const int innerTop = outerRect.top + kPlayerVitalsBorderThickness;
    RECT hpRect{ innerLeft, innerTop, innerLeft + kPlayerVitalsBarWidth, innerTop + kPlayerVitalsBarHeight };
    RECT spRect{
        innerLeft,
        hpRect.bottom + kPlayerVitalsBorderThickness,
        innerLeft + kPlayerVitalsBarWidth,
        hpRect.bottom + kPlayerVitalsBorderThickness + kPlayerVitalsBarHeight,
    };

    FillSolidRect(hdc, hpRect, RGB(255, 255, 255));
    FillSolidRect(hdc, spRect, RGB(255, 255, 255));

    if (maxHp > 0) {
        const int hp = (std::max)(0, (std::min)(static_cast<int>(player->m_Hp), maxHp));
        const int hpFillWidth = kPlayerVitalsBarWidth * hp / maxHp;
        if (hpFillWidth > 0) {
            RECT hpFillRect{ hpRect.left, hpRect.top, hpRect.left + hpFillWidth, hpRect.bottom };
            const bool lowHp = hp * 4 < maxHp;
            FillSolidRect(hdc, hpFillRect, lowHp ? RGB(220, 32, 32) : RGB(48, 192, 48));
        }
    }

    if (maxSp > 0) {
        const int sp = (std::max)(0, (std::min)(static_cast<int>(player->m_Sp), maxSp));
        const int spFillWidth = kPlayerVitalsBarWidth * sp / maxSp;
        if (spFillWidth > 0) {
            RECT spFillRect{ spRect.left, spRect.top, spRect.left + spFillWidth, spRect.bottom };
            FillSolidRect(hdc, spFillRect, RGB(48, 96, 220));
        }
    }

}

bool SendLoadEndAckPacket();
ULONG_PTR EnsureGdiplusStarted();
void SyncBootstrapSelfActorWorldPos(CGameMode& mode);
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

void FinishMapLoading(CGameMode& mode)
{
    g_windowMgr.HideLoadingScreen();
    g_windowMgr.ClearChatEvents();
    g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND);
    g_windowMgr.MakeWindow(UIWindowMgr::WID_SAYDIALOGWND);
    DbgLog("[GameMode] map loading finished world='%s' actors=%zu runtime=%zu\n",
        mode.m_rswName,
        mode.m_actorPosList.size(),
        mode.m_runtimeActors.size());
    mode.m_mapLoadingStage = CGameMode::MapLoading_None;
}

void AdvanceMapLoading(CGameMode& mode)
{
    const DWORD now = GetTickCount();
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

        if (mode.m_sentLoadEndAck == 1 && sinceAck >= kLoadingAckRetryDelayMs && mode.m_runtimeActors.size() <= 4) {
            const bool resent = SendLoadEndAckPacket();
            DbgLog("[GameMode] retry load end ack sent=%d runtime=%zu pos=%zu\n",
                resent ? 1 : 0,
                mode.m_runtimeActors.size(),
                mode.m_actorPosList.size());
            mode.m_sentLoadEndAck = resent ? 2 : 1;
            if (resent) {
                mode.m_mapLoadingAckTick = now;
                mode.m_lastActorBootstrapPacketTick = now;
            }
        }

        const float settleProgress = (std::min)(1.0f, static_cast<float>(sinceAck) / static_cast<float>(kLoadingPostAckWaitMs));
        UpdateMapLoadingUi(mode, "Waiting for map actors...", 0.86f + settleProgress * 0.14f);
        if (visibleFor >= kLoadingScreenMinShowMs
            && sinceAck >= kLoadingPostAckWaitMs
            && sinceActor >= kLoadingActorQuietMs) {
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

constexpr u16 kPacketCzRequestMove = PacketProfile::ActiveMapServerSend::kWalkToXY;
constexpr u16 kPacketCzRequestTime = PacketProfile::ActiveMapServerSend::kTickSend;
constexpr u16 kPacketCzNotifyActorInit = PacketProfile::ActiveMapServerSend::kNotifyActorInit;
constexpr u32 kHeldMoveRequestIntervalMs = 75;
constexpr int kHeldMoveRetargetThresholdCells = 2;
constexpr int kHeldMoveDirectionalExtensionCells = 4;
constexpr u32 kHeldMoveDirectionalRetargetNearEndMs = 80;
constexpr u32 kMoveAckTimeoutMs = 1000;
constexpr u32 kAttackChaseRequestIntervalMs = 1200;
constexpr u32 kAttackRetryIntervalMs = 1200;
constexpr u32 kTimeSyncIntervalMs = 12000;

bool SendLoadEndAckPacket()
{
    u8 packet[2]{};
    packet[0] = static_cast<u8>(kPacketCzNotifyActorInit & 0xFF);
    packet[1] = static_cast<u8>((kPacketCzNotifyActorInit >> 8) & 0xFF);

    const bool sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet), sizeof(packet));
    DbgLog("[GameMode] load end ack opcode=0x%04X sent=%d\n",
        kPacketCzNotifyActorInit,
        sent ? 1 : 0);
    return sent;
}

bool SendTimeSyncRequest(CGameMode& mode, bool syncNow)
{
    const u32 now = timeGetTime();
    if (!syncNow && mode.m_syncRequestTime != 0 && now <= mode.m_syncRequestTime + kTimeSyncIntervalMs) {
        return false;
    }

    PACKET_CZ_TICKSEND2 packet{};
    packet.PacketType = kPacketCzRequestTime;
    packet.padding = 0;
    packet.ClientTick = now;

    mode.m_receiveSyneRequestTime = 0;
    mode.m_syncRequestTime = now;

    const bool sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
    DbgLog("[GameMode] time sync request opcode=0x%04X tick=%u sent=%d\n",
        kPacketCzRequestTime,
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

    if (mode.m_world && mode.m_world->m_player && mode.m_world->m_player->m_isMoving && g_session.m_gid != 0) {
        const auto runtimeIt = mode.m_runtimeActors.find(g_session.m_gid);
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

    g_pathFinder.SetMap(mode.m_world->m_attr);
    CPathInfo pathInfo;
    return g_pathFinder.FindPath(timeGetTime(), sx, sy, cellX, cellY, dx, dy, 150, &pathInfo);
}

bool CanFindMovePath(const CGameMode& mode, int sx, int sy, int dx, int dy)
{
    return CanFindMovePath(mode, sx, sy, dx, dy, dx, dy);
}

bool IsWalkableAttrCell(const CGameMode& mode, int tileX, int tileY)
{
    if (!mode.m_world || !mode.m_world->m_attr || tileX < 0 || tileY < 0
        || tileX >= mode.m_world->m_attr->m_width || tileY >= mode.m_world->m_attr->m_height
        || mode.m_world->m_attr->m_cells.empty()) {
        return false;
    }

    const CAttrCell& cell = mode.m_world->m_attr->m_cells[
        static_cast<size_t>(tileY) * static_cast<size_t>(mode.m_world->m_attr->m_width) + static_cast<size_t>(tileX)];
    return cell.flag == 0;
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
    PACKET_CZ_REQUEST_MOVE2 packet{};
    packet.PacketType = kPacketCzRequestMove;
    if (!EncodeMoveDestination(dstX, dstY, packet.Dest)) {
        DbgLog("[GameMode] move request encode failed dst=%d,%d\n", dstX, dstY);
        return false;
    }

    const bool sent = CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
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

    std::vector<u8> packet(packetSize, 0);
    packet[0] = static_cast<u8>(PacketProfile::ActiveMapServerSend::kGlobalMessage & 0xFFu);
    packet[1] = static_cast<u8>((PacketProfile::ActiveMapServerSend::kGlobalMessage >> 8) & 0xFFu);
    packet[2] = static_cast<u8>(packetSize & 0xFFu);
    packet[3] = static_cast<u8>((packetSize >> 8) & 0xFFu);
    std::memcpy(packet.data() + 4, payload.c_str(), payload.size());

    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()));
    DbgLog("[GameMode] chat send opcode=0x%04X name='%s' text='%s' sent=%d\n",
        PacketProfile::ActiveMapServerSend::kGlobalMessage,
        playerName,
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

    if (IsTargetWithinAttackRange(sourceTileX, sourceTileY, targetTileX, targetTileY, 1)) {
        return false;
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

    return TryRequestMoveToCell(mode, attrX, attrY);
}

constexpr u8 kActionRequestSingleAttack = 0x00;
constexpr u8 kActionRequestSitDown = 0x02;
constexpr u8 kActionRequestStandUp = 0x03;
constexpr u8 kActionRequestContinuousAttack = 0x07;

bool SendActionRequestPacket(u32 targetGid, u8 action)
{
    if ((targetGid == 0) && (action == kActionRequestSingleAttack || action == kActionRequestContinuousAttack)) {
        return false;
    }

    PACKET_CZ_ACTION_REQUEST2 packet{};
    packet.PacketType = PacketProfile::ActiveMapServerSend::kActionRequest;
    packet.TargetGID = targetGid;
    packet.Action = action;
    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] action request gid=%u action=%u opcode=0x%04X sent=%d\n",
        targetGid,
        static_cast<unsigned int>(action),
        PacketProfile::ActiveMapServerSend::kActionRequest,
        sent ? 1 : 0);
    return sent;
}

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

    PACKET_CZ_TAKE_ITEM2 packet{};
    packet.PacketType = PacketProfile::ActiveMapServerSend::kTakeItem;
    packet.padding = 0;
    packet.ObjectAID = objectAid;
    const bool sent = CRagConnection::instance()->SendPacket(
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(packet)));
    DbgLog("[GameMode] take item aid=%u opcode=0x%04X sent=%d\n",
        objectAid,
        PacketProfile::ActiveMapServerSend::kTakeItem,
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
    mode.m_pickupReqItemNaidList.clear();
    mode.m_pickupReqItemNaidList.push_back(objectAid);
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

bool RequestEquipInventoryItem(u16 itemIndex, u16 equipLocation)
{
    if (itemIndex == 0 || equipLocation == 0) {
        return false;
    }

    PACKET_CZ_REQ_WEAR_EQUIP packet{};
    packet.PacketType = PacketProfile::ActiveMapServerSend::kEquipItem;
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

bool RequestUnequipInventoryItem(u16 itemIndex)
{
    if (itemIndex == 0) {
        return false;
    }

    PACKET_CZ_REQ_TAKEOFF_EQUIP packet{};
    packet.PacketType = PacketProfile::ActiveMapServerSend::kUnequipItem;
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

    if (!IsMonsterLikeHoverActor(hoveredActor)) {
        return false;
    }

    if (!SendAttackRequestPacket(hoveredActor->m_gid, kActionRequestContinuousAttack)) {
        return false;
    }

    mode.m_lastMonGid = hoveredActor->m_gid;
    mode.m_lastLockOnMonGid = hoveredActor->m_gid;
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
    return true;
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

    if ((std::max)(std::abs(heldDeltaX), std::abs(heldDeltaY)) > GetHeldMoveDirectionalExtensionCells(*player)) {
        return false;
    }

    const int currentSignX = ApproachSign(currentDeltaX);
    const int currentSignY = ApproachSign(currentDeltaY);
    const int heldSignX = ApproachSign(heldDeltaX);
    const int heldSignY = ApproachSign(heldDeltaY);

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

void PumpAttackChaseRequest(CGameMode& mode)
{
    if (!mode.m_world || !mode.m_world->m_player || mode.m_lastLockOnMonGid == 0) {
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

void ClearRuntimeActors(CGameMode& mode)
{
    for (auto& entry : mode.m_runtimeActors) {
        CGameActor* actor = entry.second;
        if (!actor) {
            continue;
        }
        actor->UnRegisterPos();
        delete actor;
    }
    mode.m_runtimeActors.clear();
    mode.m_actorPosList.clear();
}

void EnsureBootstrapSelfActor(CGameMode& mode)
{
    if (g_session.m_gid == 0) {
        return;
    }

    CGameActor* actor = nullptr;
    const auto it = mode.m_runtimeActors.find(g_session.m_gid);
    if (it != mode.m_runtimeActors.end()) {
        actor = it->second;
    } else {
        actor = new CPlayer();
        if (!actor) {
            return;
        }

        actor->m_gid = g_session.m_gid;
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
        mode.m_runtimeActors.emplace(g_session.m_gid, actor);

        DbgLog("[GameMode] bootstrap self actor gid=%u map='%s' pos=%d,%d dir=%d\n",
            g_session.m_gid,
            g_session.m_curMap,
            g_session.m_playerPosX,
            g_session.m_playerPosY,
            g_session.m_playerDir);
    }

    if (mode.m_world) {
        mode.m_world->m_player = static_cast<CPlayer*>(actor);
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
        actor->m_Hp = static_cast<u16>((std::max)(0, (std::min)(static_cast<int>(info->hp), 0xFFFF)));
        actor->m_MaxHp = static_cast<u16>((std::max)(0, (std::min)(static_cast<int>(info->maxhp), 0xFFFF)));
        actor->m_Sp = static_cast<u16>((std::max)(0, (std::min)(static_cast<int>(info->sp), 0xFFFF)));
        actor->m_MaxSp = static_cast<u16>((std::max)(0, (std::min)(static_cast<int>(info->maxsp), 0xFFFF)));

        if (actor->m_Hp > actor->m_MaxHp) {
            actor->m_Hp = actor->m_MaxHp;
        }
        if (actor->m_Sp > actor->m_MaxSp) {
            actor->m_Sp = actor->m_MaxSp;
        }
    }

    actor->m_lastTlvertX = g_session.m_playerPosX;
    actor->m_lastTlvertY = g_session.m_playerPosY;
    actor->m_moveDestX = g_session.m_playerPosX;
    actor->m_moveDestY = g_session.m_playerPosY;
    mode.m_actorPosList[g_session.m_gid] = CellPos{ g_session.m_playerPosX, g_session.m_playerPosY };
    mode.m_lastPcGid = g_session.m_gid;
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

    player->m_lastTlvertX = g_session.m_playerPosX;
    player->m_lastTlvertY = g_session.m_playerPosY;
    player->m_moveDestX = g_session.m_playerPosX;
    player->m_moveDestY = g_session.m_playerPosY;
    player->m_pos.x = TileToWorldCoordX(mode.m_world, g_session.m_playerPosX);
    player->m_pos.z = TileToWorldCoordZ(mode.m_world, g_session.m_playerPosY);
    player->m_pos.y = mode.m_world->m_attr ? mode.m_world->m_attr->GetHeight(player->m_pos.x, player->m_pos.z) : 0.0f;
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
    HBITMAP minimapBmp;
    int minimapWidth;
    int minimapHeight;
};

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

    const std::string bodyActName = g_session.GetJobActName(g_session.m_playerJob, sex, bodyAct);
    const std::string bodySprName = g_session.GetJobSprName(g_session.m_playerJob, sex, bodySpr);
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

ULONG_PTR EnsureGdiplusStarted()
{
    static ULONG_PTR s_token = 0;
    static bool s_started = false;
    if (!s_started) {
        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&s_token, &startupInput, nullptr) == Gdiplus::Ok) {
            s_started = true;
        }
    }
    return s_token;
}

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
    if (cache.minimapBmp) {
        DeleteObject(cache.minimapBmp);
        cache.minimapBmp = nullptr;
    }
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

bool LoadBitmapFromGameData(const std::string& dataPath, HBITMAP* outBmp, int* outWidth, int* outHeight)
{
    if (!outBmp || dataPath.empty() || !EnsureGdiplusStarted()) {
        return false;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(dataPath.c_str(), &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return false;
    }

    bool loaded = false;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(size));
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            std::memcpy(dst, bytes, static_cast<size_t>(size));
            GlobalUnlock(mem);

            IStream* stream = nullptr;
            if (CreateStreamOnHGlobal(mem, TRUE, &stream) == S_OK) {
                Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(stream, FALSE);
                if (bitmap && bitmap->GetLastStatus() == Gdiplus::Ok) {
                    HBITMAP loadedBmp = nullptr;
                    if (bitmap->GetHBITMAP(RGB(0, 0, 0), &loadedBmp) == Gdiplus::Ok && loadedBmp) {
                        BITMAP bm{};
                        if (GetObjectA(loadedBmp, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
                            *outBmp = loadedBmp;
                            if (outWidth) {
                                *outWidth = bm.bmWidth;
                            }
                            if (outHeight) {
                                *outHeight = bm.bmHeight;
                            }
                            loaded = true;
                        } else {
                            DeleteObject(loadedBmp);
                        }
                    }
                }
                delete bitmap;
                stream->Release();
            } else {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }

    delete[] bytes;
    return loaded;
}

bool ComputeBitmapAverageColor(HBITMAP bitmap, COLORREF* outColor)
{
    if (!bitmap || !outColor) {
        return false;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<unsigned int> pixels(static_cast<size_t>(bm.bmWidth) * static_cast<size_t>(bm.bmHeight));
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) {
        return false;
    }

    const int scanLines = GetDIBits(screenDC,
        bitmap,
        0,
        static_cast<UINT>(bm.bmHeight),
        pixels.data(),
        &bmi,
        DIB_RGB_COLORS);
    ReleaseDC(nullptr, screenDC);
    if (scanLines != bm.bmHeight) {
        return false;
    }

    unsigned long long totalR = 0;
    unsigned long long totalG = 0;
    unsigned long long totalB = 0;
    const unsigned long long pixelCount = static_cast<unsigned long long>(pixels.size());
    if (pixelCount == 0) {
        return false;
    }

    for (unsigned int pixel : pixels) {
        totalB += pixel & 0xFFu;
        totalG += (pixel >> 8) & 0xFFu;
        totalR += (pixel >> 16) & 0xFFu;
    }

    *outColor = RGB(
        static_cast<int>(totalR / pixelCount),
        static_cast<int>(totalG / pixelCount),
        static_cast<int>(totalB / pixelCount));
    return true;
}

bool LoadAverageBitmapColorFromGameData(const std::string& dataPath, COLORREF* outColor)
{
    HBITMAP bitmap = nullptr;
    int width = 0;
    int height = 0;
    if (!LoadBitmapFromGameData(dataPath, &bitmap, &width, &height) || !bitmap) {
        return false;
    }

    const bool ok = ComputeBitmapAverageColor(bitmap, outColor);
    DeleteObject(bitmap);
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
    info.groundTop = worldRes->m_groundTop;
    info.groundBottom = worldRes->m_groundBottom;
    info.groundLeft = worldRes->m_groundLeft;
    info.groundRight = worldRes->m_groundRight;
    info.loaded = true;
    *outInfo = info;
    return true;
}

void DrawBitmapStretched(HDC targetDC, HBITMAP bitmap, const RECT& dst)
{
    if (!targetDC || !bitmap) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(targetDC);
    if (!srcDC) {
        return;
    }

    HGDIOBJ old = SelectObject(srcDC, bitmap);
    SetStretchBltMode(targetDC, HALFTONE);
    StretchBlt(targetDC,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        0,
        0,
        bm.bmWidth,
        bm.bmHeight,
        SRCCOPY);
    SelectObject(srcDC, old);
    DeleteDC(srcDC);
}

void DrawBitmapTransparent(HDC targetDC, HBITMAP bitmap, const RECT& dst)
{
    if (!targetDC || !bitmap) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(targetDC);
    if (!srcDC) {
        return;
    }

    HGDIOBJ old = SelectObject(srcDC, bitmap);
    TransparentBlt(targetDC,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        srcDC,
        0,
        0,
        bm.bmWidth,
        bm.bmHeight,
        RGB(255, 0, 255));
    SelectObject(srcDC, old);
    DeleteDC(srcDC);
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
            mode.m_world->ClearFixedObjects();
        }
        cache.nextBackgroundActorIndex = 0;
        cache.nextFixedEffectIndex = 0;
        cache.loadStage = BootstrapLoadStage::LoadAttr;
    }

    if (cache.mapName != mapName) {
        if (mode.m_world) {
            mode.m_world->ClearBackgroundObjects();
            mode.m_world->ClearFixedObjects();
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
                LoadRswWorldInfo(cache.rswPath, &cache.worldInfo);
            }
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
                if (cache.attrRes && mode.m_world) {
                    mode.m_world->m_attr = cache.attrRes;
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
            if (!cache.minimapPath.empty() && !cache.minimapBmp) {
                LoadBitmapFromGameData(cache.minimapPath, &cache.minimapBmp, &cache.minimapWidth, &cache.minimapHeight);
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

    HDC windowDC = GetDC(hwnd);
    if (!windowDC) {
        return;
    }

    HDC memDC = CreateCompatibleDC(windowDC);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDC, width, height);
    HGDIOBJ oldBitmap = bitmap ? SelectObject(memDC, bitmap) : nullptr;
    if (!memDC || !bitmap || !oldBitmap) {
        if (oldBitmap) {
            SelectObject(memDC, oldBitmap);
        }
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (memDC) {
            DeleteDC(memDC);
        }
        ReleaseDC(hwnd, windowDC);
        return;
    }

    RECT fullRect{ 0, 0, width, height };
    HBRUSH bgBrush = CreateSolidBrush(RGB(21, 27, 36));
    FillRect(memDC, &fullRect, bgBrush);
    DeleteObject(bgBrush);

    RECT worldRect{ 24, 64, width - 220, height - 36 };
    DrawGatWorld(memDC, worldRect, mode, cache);
    FrameRect(memDC, &worldRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    RECT mapRect{ width - 184, 20, width - 20, 184 };
    if (cache.minimapBmp && cache.minimapWidth > 0 && cache.minimapHeight > 0) {
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

        DrawBitmapStretched(memDC, cache.minimapBmp, mapRect);
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
    const int groundWidth = mode.m_world ? mode.m_world->m_rootNode.m_groundArea.right - mode.m_world->m_rootNode.m_groundArea.left : 0;
    const int groundHeight = mode.m_world ? mode.m_world->m_rootNode.m_groundArea.bottom - mode.m_world->m_rootNode.m_groundArea.top : 0;
    const SceneGraphNode* scene = mode.m_world && mode.m_world->m_Calculated ? mode.m_world->m_Calculated : (mode.m_world ? &mode.m_world->m_rootNode : nullptr);
    const int activeWidth = scene ? scene->m_groundArea.right - scene->m_groundArea.left : 0;
    const int activeHeight = scene ? scene->m_groundArea.bottom - scene->m_groundArea.top : 0;
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

    BitBlt(windowDC, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd, windowDC);
}
}

CGameMode::CGameMode() 
    : m_areaLeft(0), m_areaRight(0), m_areaTop(0), m_areaBottom(0),
      m_world(nullptr), m_view(nullptr), m_mousePointer(nullptr),
    m_leftBtnClickTick(0), m_oldMouseX(0), m_oldMouseY(0), m_rBtnClickX(0), m_rBtnClickY(0),
    m_lastRButtonClickTick(0), m_lastRButtonClickX(0), m_lastRButtonClickY(0), m_rButtonDragged(0),
      m_lastPcGid(0), m_lastMonGid(0), m_lastLockOnMonGid(0),
      m_isAutoMoveClickOn(0), m_isWaitingWhisperSetting(0), m_isWaitingEnterRoom(0),
      m_isWaitingAddExchangeItem(0), m_waitingWearEquipAck(0), m_waitingTakeoffEquipAck(0),
      m_isReqUpgradeSkillLevel(0), m_exchangeItemCnt(0), m_isWaitingCancelExchangeItem(0),
      m_lastEmblemReqTick(0), m_lastNameWaitingListTick(0), 
      m_noMove(0), m_noMoveStartTick(0), m_isOnQuest(0), m_isPlayerDead(0),
      m_canRotateView(0), m_hasViewPoint(0), m_receiveSyneRequestTime(0),
      m_syncRequestTime(0), m_usedCachesUnloadTick(0), m_reqEmotionTick(0),
      m_reqTickChatRoom(0), m_reqTickMerchantShop(0), m_isReqEmotion(0),
      m_fixedLongitude(0.0f), m_lastCouplePacketAid(0), m_lastCouplePacketGid(0),
      m_nameBalloon(nullptr), m_targetNameBalloon(nullptr), m_broadcastBalloon(nullptr),
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
        m_mapLoadingStage(MapLoading_None), m_mapLoadingStartTick(0), m_mapLoadingAckTick(0), m_lastActorBootstrapPacketTick(0)
{
    std::memset(m_rswName, 0, sizeof(m_rswName));
    std::memset(m_minimapBmpName, 0, sizeof(m_minimapBmpName));
    std::memset(ViewPointData, 0, sizeof(ViewPointData));
    std::memset(m_CoupleName, 0, sizeof(m_CoupleName));
    std::memset(m_recordChatTime, 0, sizeof(m_recordChatTime));
    std::memset(m_strikeTime, 0, sizeof(m_strikeTime));
    std::memset(m_doritime, 0, sizeof(m_doritime));
}

CGameMode::~CGameMode() {
}

void CGameMode::OnInit(const char* worldName) {
    RegisterDefaultGameModePacketHandlers(g_gameModePacketRouter);
    ClearRuntimeActors(*this);
    m_groundItemList.clear();
    m_playWaveList.clear();

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

    m_world = &g_world;
    if (m_world) {
        m_world->m_curMode = this;
        m_world->ClearBackgroundObjects();
        m_world->ClearFixedObjects();
        m_world->m_ground = nullptr;
        m_world->m_player = nullptr;
        m_world->m_attr = nullptr;
    }
    m_view = nullptr;
    m_mousePointer = nullptr;

    g_windowMgr.RemoveAllWindows();
    g_windowMgr.SetLoginStatus("");
    g_windowMgr.ClearChatEvents();
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

    SendTimeSyncRequest(*this, true);

    CAudio* audio = CAudio::GetInstance();
    if (audio && m_rswName[0] != '\0') {
        m_streamFileName = audio->ResolveMapBgmPath(m_rswName);
        audio->PlayBGM(m_streamFileName.c_str());
    }

    g_renderer.m_nClearColor = RGB(0, 0, 0);
}
void CGameMode::OnExit() {
    g_gameModePacketRouter.Clear();
    g_windowMgr.RemoveAllWindows();
    ClearRuntimeActors(*this);
    m_groundItemList.clear();
    m_playWaveList.clear();
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
        m_world->m_attr = nullptr;
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
int  CGameMode::OnRun() {
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
        g_windowMgr.OnDraw();
        DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
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
    g_renderer.ClearBackground();
    g_renderer.Clear(0);
    static u32 s_gameplayRenderFrame = 0;
    ++s_gameplayRenderFrame;
    const DWORD renderPrepStart = GetTickCount();
    m_view->OnRender();
    const DWORD renderPrepEnd = GetTickCount();
    DWORD uiDrawStart = renderPrepEnd;
    DWORD uiDrawEnd = renderPrepEnd;
    bool queuedModernOverlayFrame = false;
    if (!hasLegacyDevice && isVulkanBackend) {
        uiDrawStart = GetTickCount();
        queuedModernOverlayFrame = QueueModernOverlayQuad(*this, m_cursorActNum, m_mouseAnimStartTick);
        uiDrawEnd = GetTickCount();
    }
    const DWORD drawSceneStart = GetTickCount();
    g_renderer.DrawScene();
    const DWORD drawSceneEnd = GetTickCount();
    if (hasLegacyDevice) {
        {
            HDC backBufferDc = nullptr;
            if (GetRenderDevice().AcquireBackBufferDC(&backBufferDc) && backBufferDc) {
                DrawGameplayOverlayToHdc(*this, backBufferDc);
                GetRenderDevice().ReleaseBackBufferDC(backBufferDc);
            }
        }
        const DWORD uiDrawStart = GetTickCount();
        g_windowMgr.OnDraw();
        const DWORD uiDrawEnd = GetTickCount();
        bool drewCursorOnBackBuffer = false;
        {
            HDC backBufferDc = nullptr;
            if (GetRenderDevice().AcquireBackBufferDC(&backBufferDc) && backBufferDc) {
                drewCursorOnBackBuffer = DrawModeCursorToHdc(backBufferDc, m_cursorActNum, m_mouseAnimStartTick);
                GetRenderDevice().ReleaseBackBufferDC(backBufferDc);
            }
        }
        const DWORD flipStart = GetTickCount();
        g_renderer.Flip(false);
        const DWORD flipEnd = GetTickCount();
        if (!drewCursorOnBackBuffer) {
            DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
        }

        g_framePerfStats.frames += 1;
        g_framePerfStats.updateMs += static_cast<u64>(updateEnd - updateStart);
        g_framePerfStats.processUiMs += static_cast<u64>(processUiEnd - processUiStart);
        g_framePerfStats.renderPrepMs += static_cast<u64>(renderPrepEnd - renderPrepStart);
        g_framePerfStats.drawSceneMs += static_cast<u64>(drawSceneEnd - drawSceneStart);
        g_framePerfStats.uiDrawMs += static_cast<u64>(uiDrawEnd - uiDrawStart);
        g_framePerfStats.flipMs += static_cast<u64>(flipEnd - flipStart);

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
        if (!isVulkanBackend) {
            uiDrawStart = GetTickCount();
            composedModernOverlayFrame = ComposeModernOverlayFrame(*this, m_cursorActNum, m_mouseAnimStartTick);
            uiDrawEnd = GetTickCount();
        }
        const DWORD flipStart = GetTickCount();
        g_renderer.Flip(false);
        const DWORD flipEnd = GetTickCount();
        if (!composedModernOverlayFrame) {
            HDC windowDc = GetDC(g_hMainWnd);
            if (windowDc) {
                DrawGameplayOverlayToHdc(*this, windowDc);
                HDC previousSharedDc = UIWindow::GetSharedDrawDC();
                UIWindow::SetSharedDrawDC(windowDc);
                g_windowMgr.OnDraw();
                UIWindow::SetSharedDrawDC(previousSharedDc);
                DrawModeCursorToHdc(windowDc, m_cursorActNum, m_mouseAnimStartTick);
                ReleaseDC(g_hMainWnd, windowDc);
            } else if (!isVulkanBackend) {
                DrawModeCursor(m_cursorActNum, m_mouseAnimStartTick);
            }
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

    if (!isVulkanBackend) {
        Sleep(1);
    }
    return 1;
}
void CGameMode::OnUpdate() {
    EnsureBootstrapSelfActor(*this);
    SendTimeSyncRequest(*this, false);

    // Poll a bounded number of packets per frame to avoid starvation.
    CRagConnection* conn = CRagConnection::instance();
    std::vector<u8> rawPacket;
    int packetBudget = (m_mapLoadingStage != MapLoading_None) ? 128 : 64;
    int packetsProcessed = 0;

    const DWORD packetStart = GetTickCount();
    while (packetBudget-- > 0 && conn->RecvPacket(rawPacket)) {
        PacketView packet{};
        int consumed = 0;
        if (!TryReadPacket(rawPacket.data(), static_cast<int>(rawPacket.size()), packet, consumed)) {
            continue;
        }
        TraceGamePacketFirstSeen(packet);
        ++packetsProcessed;
        if (!g_gameModePacketRouter.Dispatch(*this, packet)) {
            LogFirstSeenUnhandledGamePacket(packet.packetId, packet.packetLength);
        }
    }
    const DWORD packetEnd = GetTickCount();

    const DWORD actorSummaryStart = GetTickCount();
    LogRuntimeActorSummary(*this);
    const DWORD actorSummaryEnd = GetTickCount();

    UpdateMapAudio(*this);

    if (IsMapLoadingActive(*this)) {
        AdvanceMapLoading(*this);
        if (m_world) {
            SetRuntimeActorCameraLongitude(m_view ? m_view->GetCameraLongitude() : 0.0f);
            m_world->UpdateActors();
            CleanupPendingActorDespawns(*this);
            const matrix* viewMatrix = m_view ? &m_view->GetViewMatrix() : nullptr;
            m_world->UpdateBackgroundObjects(viewMatrix);
        }
        SetModeCursorAction(*this, CursorAction::Arrow);
        return;
    }

    const DWORD bootstrapStart = GetTickCount();
    EnsureBootstrapWorldAssets(*this);
    SyncBootstrapSelfActorWorldPos(*this);
    SyncGroundItemsToWorld(*this);
    const DWORD bootstrapEnd = GetTickCount();

    DWORD actorUpdateEnd = bootstrapEnd;
    DWORD backgroundUpdateEnd = bootstrapEnd;
    if (m_world) {
        const DWORD actorUpdateStart = GetTickCount();
        SetRuntimeActorCameraLongitude(m_view ? m_view->GetCameraLongitude() : 0.0f);
        m_world->UpdateActors();
        CleanupPendingActorDespawns(*this);
        actorUpdateEnd = GetTickCount();

        const matrix* viewMatrix = m_view ? &m_view->GetViewMatrix() : nullptr;
        const DWORD backgroundUpdateStart = GetTickCount();
        m_world->UpdateBackgroundObjects(viewMatrix);
        backgroundUpdateEnd = GetTickCount();
    }

    const DWORD ensureViewStart = GetTickCount();
    EnsureRealView(*this);
    const DWORD ensureViewEnd = GetTickCount();

    DWORD hoverUpdateEnd = ensureViewEnd;
    if (m_view && !m_canRotateView && g_hMainWnd) {
        const DWORD hoverUpdateStart = GetTickCount();
        POINT cursorPos{};
        if (GetCursorPos(&cursorPos) && ScreenToClient(g_hMainWnd, &cursorPos)) {
            m_oldMouseX = cursorPos.x;
            m_oldMouseY = cursorPos.y;
            m_view->UpdateHoverCellFromScreen(cursorPos.x, cursorPos.y);
            if (m_isLeftButtonHeld) {
                UpdateHeldMoveTargetFromScreenPoint(*this, cursorPos.x, cursorPos.y);
            }
        }
        hoverUpdateEnd = GetTickCount();
    }

    const DWORD heldMoveStart = GetTickCount();
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

    UpdateGameplayCursor(*this);

    g_updatePerfStats.frames += 1;
    g_updatePerfStats.packetMs += static_cast<u64>(packetEnd - packetStart);
    g_updatePerfStats.actorSummaryMs += static_cast<u64>(actorSummaryEnd - actorSummaryStart);
    g_updatePerfStats.bootstrapMs += static_cast<u64>(bootstrapEnd - bootstrapStart);
    g_updatePerfStats.actorUpdateMs += static_cast<u64>(actorUpdateEnd - bootstrapEnd);
    g_updatePerfStats.backgroundUpdateMs += static_cast<u64>(backgroundUpdateEnd - actorUpdateEnd);
    g_updatePerfStats.ensureViewMs += static_cast<u64>(ensureViewEnd - ensureViewStart);
    g_updatePerfStats.hoverUpdateMs += static_cast<u64>(hoverUpdateEnd - ensureViewEnd);
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
}
int CGameMode::SendMsg(int msg, int wparam, int lparam, int extra)
{
    (void)extra;
    (void)lparam;

    switch (msg) {
    case GameMsg_LButtonDown:
        if (TryRequestGroundItemFromScreenPoint(*this, wparam, lparam)) {
            m_isLeftButtonHeld = 0;
            m_hasHeldMoveTarget = 0;
            return 1;
        }
        if (TryRequestAttackFromScreenPoint(*this, wparam, lparam)) {
            m_isLeftButtonHeld = 0;
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
        UpdateHeldMoveTargetFromScreenPoint(*this, wparam, lparam);
        PumpHeldMoveRequest(*this);
        return 1;

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

    case GameMsg_MouseMove:
        if (m_view && !m_canRotateView) {
            m_view->UpdateHoverCellFromScreen(wparam, lparam);
            if (m_isLeftButtonHeld) {
                UpdateHeldMoveTargetFromScreenPoint(*this, wparam, lparam);
            }
        }
        if (m_canRotateView && m_view) {
            if (wparam != m_oldMouseX || lparam != m_oldMouseY) {
                m_rButtonDragged = 1;
            }
            m_view->RotateByDrag(wparam - m_oldMouseX, lparam - m_oldMouseY);
            m_view->ClearHoverCell();
        }
        m_oldMouseX = wparam;
        m_oldMouseY = lparam;
        return 1;

    case GameMsg_RButtonDown:
    {
        const u32 now = GetTickCount();
        const int doubleClickWidth = (std::max)(4, GetSystemMetrics(SM_CXDOUBLECLK));
        const int doubleClickHeight = (std::max)(4, GetSystemMetrics(SM_CYDOUBLECLK));
        const bool withinDoubleClickTime = m_lastRButtonClickTick != 0
            && now - m_lastRButtonClickTick <= GetDoubleClickTime();
        const bool withinDoubleClickRect = (std::abs)(wparam - m_lastRButtonClickX) <= doubleClickWidth
            && (std::abs)(lparam - m_lastRButtonClickY) <= doubleClickHeight;
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
            m_oldMouseX = wparam;
            m_oldMouseY = lparam;
            return 1;
        }

        m_canRotateView = 1;
        m_rBtnClickX = wparam;
        m_rBtnClickY = lparam;
        m_oldMouseX = wparam;
        m_oldMouseY = lparam;
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
        const char* text = reinterpret_cast<const char*>(wparam);
        if (!text || *text == '\0') {
            return 0;
        }
        std::string command = text;
        std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (command == "/sit") {
            return SendSitStandToggleRequest(*this) ? 1 : 0;
        }
        if (command == "/stand") {
            return SendSitStandRequest(*this, false) ? 1 : 0;
        }
        return SendGlobalChatMessage(g_session.GetPlayerName(), text) ? 1 : 0;
    }

    case GameMsg_ToggleSitStand:
        return SendSitStandToggleRequest(*this) ? 1 : 0;

    case GameMsg_RequestReturnToCharSelect:
        return RequestReturnToCharSelect() ? 1 : 0;

    case GameMsg_RequestExitToWindows:
        RequestExitToWindows();
        return 1;

    case GameMsg_RequestReturnToSavePoint:
        return RequestReturnToSavePoint() ? 1 : 0;

    case GameMsg_RequestEquipInventoryItem:
        return RequestEquipInventoryItem(
            static_cast<u16>(wparam),
            static_cast<u16>(lparam)) ? 1 : 0;

    case GameMsg_RequestUnequipInventoryItem:
        return RequestUnequipInventoryItem(static_cast<u16>(wparam)) ? 1 : 0;

    case GameMsg_RButtonUp:
        m_canRotateView = 0;
        if (GetCapture() == g_hMainWnd) {
            ReleaseCapture();
        }
        if (!m_rButtonDragged) {
            m_lastRButtonClickTick = GetTickCount();
            m_lastRButtonClickX = wparam;
            m_lastRButtonClickY = lparam;
        } else {
            m_lastRButtonClickTick = 0;
        }
        return 1;

    case GameMsg_MouseWheel:
        if (m_view) {
            m_view->ZoomByWheel(wparam);
        }
        return 1;

    default:
        return 0;
    }
}
void CGameMode::OnChangeState(int newState) {}
