#include "Mode.h"
#include "LoginMode.h"
#include "GameMode.h"
#include "DebugLog.h"
#include "core/ClientInfoLocale.h"
#include "core/MainLoopWatchdog.h"
#include "qtui/QtUiRuntime.h"
#include "render/Renderer.h"
#include "render3d/RenderDevice.h"
#include "res/Texture.h"
#include "ui/UIWindowMgr.h"
#if RO_PLATFORM_WINDOWS
#include "ui/UIGroundSkillTextWnd.h"
#include <imm.h>
#endif
#include "ui/UITransBalloonText.h"
#include "main/WinMain.h"
#include "network/Connection.h"
#include <cstring>
#include <windows.h>

namespace {

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

bool QueueFullScreenOverlayQuad(CTexture* texture, int width, int height, float sortKey)
{
    if (!texture || width <= 0 || height <= 0) {
        return false;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float left = OverlayScreenEdge(0);
    const float top = OverlayScreenEdge(0);
    const float right = OverlayScreenEdge(width);
    const float bottom = OverlayScreenEdge(height);
    const unsigned int overlayContentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
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
    face->alphaSortKey = sortKey;

    face->m_verts[0] = { left, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, top, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { left, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueLoadingMenuOverlayQuad(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    static CTexture* s_qtLoadingOverlayTexture = nullptr;
    static int s_qtLoadingOverlayTextureWidth = 0;
    static int s_qtLoadingOverlayTextureHeight = 0;
    if (!s_qtLoadingOverlayTexture
        || s_qtLoadingOverlayTextureWidth != width
        || s_qtLoadingOverlayTextureHeight != height) {
        delete s_qtLoadingOverlayTexture;
        s_qtLoadingOverlayTexture = new CTexture();
        if (!s_qtLoadingOverlayTexture
            || !s_qtLoadingOverlayTexture->Create(width, height, PF_A8R8G8B8, false)) {
            delete s_qtLoadingOverlayTexture;
            s_qtLoadingOverlayTexture = nullptr;
            s_qtLoadingOverlayTextureWidth = 0;
            s_qtLoadingOverlayTextureHeight = 0;
            return false;
        }
        s_qtLoadingOverlayTextureWidth = width;
        s_qtLoadingOverlayTextureHeight = height;
    }

    if (!RenderQtUiMenuOverlayTexture(s_qtLoadingOverlayTexture, width, height)) {
        return false;
    }
    return QueueFullScreenOverlayQuad(s_qtLoadingOverlayTexture, width, height, 2.0f);
}

void DrawLoadingScreenFrame(const char* message, float progress)
{
    const std::string wallpaperName = ChooseRandomLoadingWallpaper();
    g_windowMgr.ShowLoadingScreen(wallpaperName, message ? message : "Loading...", progress);
    g_windowMgr.OnProcess();
    const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;
    if (!hasLegacyDevice && IsQtUiRuntimeEnabled() && g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        const int clientWidth = clientRect.right - clientRect.left;
        const int clientHeight = clientRect.bottom - clientRect.top;
        if (clientWidth > 0 && clientHeight > 0) {
            g_renderer.ClearBackground();
            g_renderer.Clear(0);
            if (QueueLoadingMenuOverlayQuad(clientWidth, clientHeight)) {
                g_renderer.DrawScene();
                g_renderer.Flip(false);
            } else {
                g_windowMgr.OnDraw();
            }
        } else {
            g_windowMgr.OnDraw();
        }
    } else {
        g_windowMgr.OnDraw();
    }
    if (g_hMainWnd) {
        UpdateWindow(g_hMainWnd);
    }
}

CMode* CreateModeInstance(int modeType)
{
    switch (modeType) {
    case 0:
        return new CLoginMode();
    case 1:
        return new CGameMode();
    default:
        return new CLoginMode();
    }
}

} // namespace

CMode::CMode() 
    : m_subMode(0), m_subModeCnt(0), m_nextSubMode(-1), m_fadeInCount(0)
    , m_loopCond(1), m_isConnected(0), m_helpBalloon(nullptr), m_helpBalloonTick(0)
    , m_mouseAnimStartTick(0), m_isMouseLockOn(0), m_screenShotNow(0)
    , m_cursorActNum(0), m_cursorMotNum(0)
{
    m_mouseSnapDiff.x = 0;
    m_mouseSnapDiff.y = 0;
}

CMode::~CMode()
{
    if(m_helpBalloon){auto* window=m_helpBalloon;m_helpBalloon=nullptr;window->DetachOwner();g_windowMgr.DeleteWindow(window);}
}
void CMode::HelpBalloonDeleted(UITransBalloonText* window){if(m_helpBalloon==window)m_helpBalloon=nullptr;}
void CMode::MakeHelpBalloon(const char* text,int x,int y,u32 color)
{
    if(!text || !*text)return;
    const auto sanitized=UITransBalloonText::ReplaceUnderbars(text);m_helpBalloonTick=timeGetTime();
    if(!m_helpBalloon){m_helpBalloon=new UITransBalloonText(this);g_windowMgr.m_children.push_back(m_helpBalloon);}
    else {g_windowMgr.m_children.remove(m_helpBalloon);g_windowMgr.m_children.push_back(m_helpBalloon);}
    m_helpBalloon->SetText(sanitized.c_str(),35);
    m_helpBalloon->SetFontColor(((color&255)<<16)|(color&0xff00)|((color>>16)&255));
    const auto client=g_windowMgr.GetChatClientRect();
    x=(std::max)(-3,x);x=(std::min)(x,static_cast<int>(client.right-client.left)-m_helpBalloon->m_w+3);
    y=(std::max)(-3,y);y=(std::min)(y,static_cast<int>(client.bottom-client.top)-m_helpBalloon->m_h+3);
    m_helpBalloon->Move(x,y);
}
void CMode::ProcessHelpBalloon(u32 now)
{
    if(m_helpBalloon && now>=m_helpBalloonTick+100u){auto* window=m_helpBalloon;m_helpBalloon=nullptr;window->DetachOwner();g_windowMgr.PostQuit(window);}
}

void CMode::SetCursorAction(CursorAction cursorActNum)
{
    SetCursorAction(static_cast<int>(cursorActNum));
}

void CMode::SetCursorAction(int cursorActNum)
{
    if (m_cursorActNum == cursorActNum) {
        return;
    }

    m_cursorActNum = cursorActNum;
    m_cursorMotNum = 0;
    m_mouseAnimStartTick = GetTickCount();
    g_windowMgr.SendMsg(15, m_cursorActNum, 0);
}

CModeMgr g_modeMgr;

CModeMgr::CModeMgr() 
    : m_loopCond(0), m_curMode(nullptr), m_curModeType(0), m_nextModeType(0)
{
    std::memset(m_curModeName, 0, 40);
    std::memset(m_nextModeName, 0, 40);
}

CModeMgr::~CModeMgr() = default;

void CModeMgr::Quit()
{
    ++m_operationGeneration;
    m_lastOperation = ModeOperationKind::Quit;
    m_loopCond = 0;
    PostQuitMessage(0);
}

void CModeMgr::Switch(int newMode, const char* worldName)
{
    ++m_operationGeneration;
    m_lastOperation = ModeOperationKind::Switch;
    m_switchPending = true;
    m_nextModeType = newMode;
    if (worldName) {
        std::strncpy(m_nextModeName, worldName, 39);
        m_nextModeName[39] = '\0';
    }
}

void CModeMgr::PresentLoadingScreen(const char* message, float progress)
{
    DrawLoadingScreenFrame(message, progress);
}

msgresult_t CModeMgr::SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    if (!m_curMode) {
        return 0;
    }
    return m_curMode->SendMsg(msg, wparam, lparam, extra);
}

CGameMode* CModeMgr::GetCurrentGameMode() const
{
    return m_curModeType == 1 ? dynamic_cast<CGameMode*>(m_curMode) : nullptr;
}

CLoginMode* CModeMgr::GetCurrentLoginMode() const
{
    return m_curModeType == 0 ? dynamic_cast<CLoginMode*>(m_curMode) : nullptr;
}

void CModeMgr::Run(int startMode, const char* worldName,const ModeRunServices* services)
{
    m_loopCond = 1;
    m_switchPending = true;
    m_curModeType = -1;
    m_nextModeType = startMode;
    if (worldName) {
        std::strncpy(m_nextModeName, worldName, 39);
        m_nextModeName[39] = '\0';
    }

    MainLoopWatchdogStart();

    MSG msg{};
    while (m_loopCond)
    {
        MainLoopWatchdogSetStage("PeekMessage");
        if (
#if RO_PLATFORM_WINDOWS
            PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)
#else
            PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)
#endif
        )
        {
            if (msg.message == WM_QUIT) {
                m_loopCond = 0;
                break;
            }
            MainLoopWatchdogSetStage("DispatchMessage");
#if RO_PLATFORM_WINDOWS
            if (msg.hwnd == g_hMainWnd && msg.message == WM_KEYDOWN && msg.wParam == VK_PROCESSKEY) {
                if (auto* prompt = g_windowMgr.GetFocusedGroundSkillText())
                    prompt->RecordImeKeystroke(static_cast<int>(ImmGetVirtualKey(msg.hwnd)));
            }
#endif
            TranslateMessage(&msg);
#if RO_PLATFORM_WINDOWS
            DispatchMessageW(&msg);
#else
            DispatchMessageA(&msg);
#endif
        }
        else {
            const bool reloadCurrentMode = m_curMode
                && m_curModeType == m_nextModeType
                && std::strncmp(m_curModeName, m_nextModeName, sizeof(m_curModeName)) != 0;
            if (!m_curMode || m_switchPending || m_curModeType != m_nextModeType || reloadCurrentMode) {
                MainLoopWatchdogSetStage("ModeSwitch");
                DbgLog("[Mode] applying switch generation=%llu type=%d world='%s'\n",
                    static_cast<unsigned long long>(m_operationGeneration),m_nextModeType,m_nextModeName);
                CGameMode::AuctionStagingState auctionStaging;
                CGameMode::AuctionSearchState auctionSearch;
                auto auctionTransaction = CGameMode::AuctionTransaction::None;
                u32 auctionTransactionSentAt = 0;
                auto* leaving = GetCurrentGameMode();
                const bool preserveCommerceReplies = leaving && leaving->m_preserveCommerceRepliesOnMapReload
                    && m_nextModeType == 1 && CRagConnection::instance()->IsOpen();
                if (leaving) leaving->m_preserveCommerceRepliesOnMapReload = preserveCommerceReplies;
                if (preserveCommerceReplies) {
                    auctionStaging = leaving->m_auctionStaging;
                    auctionSearch = std::move(leaving->m_auctionSearch);
                    auctionTransaction = leaving->m_auctionTransaction;
                    auctionTransactionSentAt = leaving->m_auctionTransactionSentAt;
                }
                if (m_curMode) {
                    if(services && services->presentLoading)services->presentLoading("Loading new map...",0.01f);
                    else DrawLoadingScreenFrame("Loading new map...", 0.01f);
                }
                if (m_curMode) {
                    m_curMode->OnExit();
                    delete m_curMode;
                    m_curMode = nullptr;
                }

                m_curModeType = m_nextModeType;
                std::strncpy(m_curModeName, m_nextModeName, 39);
                m_curModeName[39] = '\0';

                // A request is consumed even when type/name match the current
                // mode. Otherwise its terminal network latch can never clear.
                // Consume before construction/OnInit so a new request from
                // initialization remains pending for the next iteration.
                m_switchPending = false;
                m_curMode = services && services->createMode?services->createMode(m_curModeType):CreateModeInstance(m_curModeType);
                if (auto* entering = GetCurrentGameMode()) {
                    entering->m_preserveCommerceRepliesOnMapReload = preserveCommerceReplies;
                }
                if (m_curMode) {
                    MainLoopWatchdogSetStage("Mode::OnInit");
                    m_curMode->OnInit(m_curModeName);
                }
                if (auto* entering = GetCurrentGameMode()) {
                    entering->m_preserveCommerceRepliesOnMapReload = false;
                    if (preserveCommerceReplies) {
                        entering->m_auctionStaging = auctionStaging;
                        entering->m_auctionSearch = std::move(auctionSearch);
                        entering->m_auctionTransaction = auctionTransaction;
                        entering->m_auctionTransactionSentAt = auctionTransactionSentAt;
                    }
                }
            }

            if (m_curMode) {
                MainLoopWatchdogSetStage("ProcessQtUiRuntimeEvents");
                ProcessQtUiRuntimeEvents();
                MainLoopWatchdogSetStage("Mode::OnRun");
                m_curMode->OnRun();
                m_curMode->ProcessHelpBalloon(timeGetTime());
                MainLoopWatchdogSetStage("RecordMainWindowFrame");
                RecordMainWindowFrame();
                MainLoopWatchdogMarkFrame(m_curModeType);
            } else {
                Sleep(1);
            }
        }
    }

    MainLoopWatchdogSetStage("Shutdown");
    MainLoopWatchdogStop();

    if (m_curMode) {
        DbgLog("[Mode] Final mode exit starting type=%d name='%s' ptr=%p\n",
            m_curModeType,
            m_curModeName,
            static_cast<void*>(m_curMode));
        m_curMode->OnExit();
        DbgLog("[Mode] Final mode exit completed type=%d name='%s' ptr=%p\n",
            m_curModeType,
            m_curModeName,
            static_cast<void*>(m_curMode));
        delete m_curMode;
        DbgLog("[Mode] Final mode delete completed type=%d name='%s'\n",
            m_curModeType,
            m_curModeName);
        m_curMode = nullptr;
    }
}
