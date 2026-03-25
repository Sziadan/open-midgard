#include "Mode.h"
#include "LoginMode.h"
#include "GameMode.h"
#include "core/ClientInfoLocale.h"
#include "ui/UIWindowMgr.h"
#include "main/WinMain.h"
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

void DrawLoadingScreenFrame(const char* message, float progress)
{
    const std::string wallpaperName = ChooseRandomLoadingWallpaper();
    g_windowMgr.ShowLoadingScreen(wallpaperName, message ? message : "Loading...", progress);
    g_windowMgr.OnProcess();
    g_windowMgr.OnDraw();
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

CMode::~CMode() = default;

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
    m_loopCond = 0;
    PostQuitMessage(0);
}

void CModeMgr::Switch(int newMode, const char* worldName)
{
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

int CModeMgr::SendMsg(int msg, int wparam, int lparam, int extra)
{
    if (!m_curMode) {
        return 0;
    }
    return m_curMode->SendMsg(msg, wparam, lparam, extra);
}

void CModeMgr::Run(int startMode, const char* worldName)
{
    m_loopCond = 1;
    m_curModeType = -1;
    m_nextModeType = startMode;
    if (worldName) {
        std::strncpy(m_nextModeName, worldName, 39);
        m_nextModeName[39] = '\0';
    }

    MSG msg{};
    while (m_loopCond)
    {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) {
                m_loopCond = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        else {
            const bool reloadCurrentMode = m_curMode
                && m_curModeType == m_nextModeType
                && std::strncmp(m_curModeName, m_nextModeName, sizeof(m_curModeName)) != 0;
            if (!m_curMode || m_curModeType != m_nextModeType || reloadCurrentMode) {
                if (m_curMode) {
                    DrawLoadingScreenFrame("Loading new map...", 0.01f);
                }
                if (m_curMode) {
                    m_curMode->OnExit();
                    delete m_curMode;
                    m_curMode = nullptr;
                }

                m_curModeType = m_nextModeType;
                std::strncpy(m_curModeName, m_nextModeName, 39);
                m_curModeName[39] = '\0';

                m_curMode = CreateModeInstance(m_curModeType);
                if (m_curMode) {
                    m_curMode->OnInit(m_curModeName);
                }
            }

            if (m_curMode) {
                m_curMode->OnRun();
            } else {
                Sleep(1);
            }
        }
    }

    if (m_curMode) {
        m_curMode->OnExit();
        delete m_curMode;
        m_curMode = nullptr;
    }
}
