//===========================================================================
// WinMain.cpp  –  Application entry point
// Clean C++17 rewrite.
//
// Corresponds to Ref/WinMain.cpp, decompiled from 0x68bcd0.
// All v4/v5/v6 anonymous variables have been replaced by named locals.
// Decompiler artefacts (_BYTE casts, CPPEH_RECORD, _local_unwind2) removed.
//===========================================================================
#include <map>
#include <winsock2.h>
#include <windowsx.h>
#include "WinMain.h"
#include "Types.h"
#include "core/Timer.h"
#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "network/Connection.h"
#include "core/DllMgr.h"
#include "core/ClientInfoLocale.h"
#include "ui/UIWindowMgr.h"
#include "render3d/Device.h"
#include "render/Renderer.h"
#include "world/World.h"
#include "res/GndRes.h"
#include "res/Bitmap.h"
#include "res/ModelRes.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include "res/PaletteRes.h"
#include "res/ImfRes.h"
#include "res/WorldRes.h"
#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <cstring>
#include <cstdio>
#include <io.h>
#include <algorithm>
#include <objbase.h>

// ---------------------------------------------------------------------------
// Window constants
// ---------------------------------------------------------------------------
const char* const WINDOW_NAME = "Ragnarok";

int  WINDOW_WIDTH  = 1920;
int  WINDOW_HEIGHT = 1080;

// ---------------------------------------------------------------------------
// Globally accessible state
// (extern declarations are in WinMain.h and core/Types.h)
// ---------------------------------------------------------------------------
HWND      g_hMainWnd   = nullptr;
HINSTANCE g_hInstance  = nullptr;
bool      g_isAppActive = false;
bool      g_multiSTOP   = false;
int       g_soundMode   = 1;
int       g_frameskip   = 0;
char      g_baseDir [MAX_PATH] = {};
char      g_baseDir3[MAX_PATH] = {};

CFileMgr  g_fileMgr;
int       g_readFolderFirst = 0;   // 0=PAK-first, 1=disk-first

// Registry path used by the original client
static const char g_regPath[] = "Software\\Gravity Soft\\Ragnarok Online";

// ---------------------------------------------------------------------------
// MSS (Miles Sound System) shim stubs
// Replace with real MSS calls if the SDK is available.
// ---------------------------------------------------------------------------
bool InitMSS()   { return true; }
void UnInitMSS() {}

// ---------------------------------------------------------------------------
// ErrorMsg
// ---------------------------------------------------------------------------
void ErrorMsg(const char* msg)
{
    MessageBoxA(g_hMainWnd, msg, "Error", MB_OK | MB_ICONERROR);
}

void ErrorMsg(int /*msgId*/) {}

// ---------------------------------------------------------------------------
// Registry helper
// ---------------------------------------------------------------------------
int ReadRegistry()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, g_regPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return 0;

    DWORD type = REG_DWORD, len = 4;
    RegQueryValueExA(hKey, "IsFullScreen",  nullptr, &type, reinterpret_cast<LPBYTE>(&g_isAppActive), &len);
    RegQueryValueExA(hKey, "Width",         nullptr, &type, reinterpret_cast<LPBYTE>(&WINDOW_WIDTH),  &len);
    RegQueryValueExA(hKey, "Height",        nullptr, &type, reinterpret_cast<LPBYTE>(&WINDOW_HEIGHT), &len);

    // Override to always windowed for now
    WINDOW_WIDTH  = 1920;
    WINDOW_HEIGHT = 1080;

    RegCloseKey(hKey);
    return 1;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ACTIVATE:
        g_isAppActive = (LOWORD(wParam) != WA_INACTIVE);
        return 0;

    case WM_CLOSE:
        g_modeMgr.Quit();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_ERASEBKGND:
        // The client renders every frame; skip GDI erase to avoid flicker.
        return TRUE;

    case WM_LBUTTONDOWN:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const bool uiHit = g_windowMgr.HasWindowAtPoint(x, y);
        g_windowMgr.OnLBtnDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (!uiHit) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_LButtonDown, x, y);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const bool uiCaptured = (g_windowMgr.m_captureWindow != nullptr);
        const bool uiHit = g_windowMgr.HasWindowAtPoint(x, y);
        g_windowMgr.OnLBtnUp(x, y);
        if (!uiCaptured && !uiHit) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_LButtonUp, x, y);
        }
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (!g_windowMgr.HasWindowAtPoint(x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RButtonDown, x, y);
        }
        return 0;
    }

    case WM_RBUTTONUP:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (!g_windowMgr.HasWindowAtPoint(x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RButtonUp, x, y);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const bool uiCaptured = (g_windowMgr.m_captureWindow != nullptr);
        g_windowMgr.OnMouseMove(x, y);
        if (!uiCaptured && !g_windowMgr.HasWindowAtPoint(x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_MouseMove, x, y);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
        g_modeMgr.SendMsg(CGameMode::GameMsg_MouseWheel, GET_WHEEL_DELTA_WPARAM(wParam), 0);
        return 0;

    case WM_CHAR:
        g_windowMgr.OnChar(static_cast<char>(wParam));
        return 0;

    case WM_KEYDOWN:
        g_windowMgr.OnKeyDown(static_cast<int>(wParam));
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// InitApp  –  Register window class, create the main window
// ---------------------------------------------------------------------------
bool InitApp(HINSTANCE hInstance, int nCmdShow)
{
    g_hInstance = hInstance;

    // Check for duplicate instance
    if (FindWindowA(WINDOW_NAME, WINDOW_NAME))
        g_multiSTOP = true;

    WNDCLASSA wc    = {};
    wc.lpfnWndProc  = WindowProc;
    wc.hInstance    = hInstance;
    wc.hIcon        = LoadIconA(hInstance, MAKEINTRESOURCEA(0x77));
    wc.hCursor      = LoadCursorA(nullptr, (LPCSTR)IDC_ARROW);
    wc.hbrBackground= nullptr;
    wc.lpszClassName= WINDOW_NAME;
    RegisterClassA(&wc);

    RECT windowRect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int winLeft = (screenW - WINDOW_WIDTH) / 2;
    int winTop  = 0;

    g_hMainWnd = CreateWindowExA(
        0, WINDOW_NAME, WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        winLeft, winTop,
        windowRect.right  - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hMainWnd)
    {
        MessageBoxA(nullptr, "Failed to create window", "Error", MB_OK);
        return false;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    GetCurrentDirectoryA(MAX_PATH, g_baseDir3);
    std::strcpy(g_baseDir, g_baseDir3);
    std::strcat(g_baseDir, "\\");

    return true;
}

// ---------------------------------------------------------------------------
// CheckSystemMessage  –  Pump the Win32 message queue for one iteration
// ---------------------------------------------------------------------------
void CheckSystemMessage()
{
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void SetWindowActiveMode(int active) { g_isAppActive = (active != 0); }
bool GetWindowActiveMode()           { return g_isAppActive; }
void ExitApp()                       { g_modeMgr.Quit(); }

static bool InitClientSystems()
{
    g_resMgr.RegisterType("gat", "", new C3dAttr());
    g_resMgr.RegisterType("gnd", "", new CGndRes());
    g_resMgr.RegisterType("bmp", "", new CBitmapRes());
    g_resMgr.RegisterType("jpg", "", new CBitmapRes());
    g_resMgr.RegisterType("jpeg", "", new CBitmapRes());
    g_resMgr.RegisterType("tga", "", new CBitmapRes());
    g_resMgr.RegisterType("rsm", "", new C3dModelRes());
    g_resMgr.RegisterType("spr", "", new CSprRes());
    g_resMgr.RegisterType("act", "", new CActRes());
    g_resMgr.RegisterType("pal", "", new CPaletteRes());
    g_resMgr.RegisterType("imf", "", new CImfRes());
    g_resMgr.RegisterType("rsw", "", new C3dWorldRes());

    if (!g_windowMgr.Init()) {
        ErrorMsg("UI system initialization failed.");
        return false;
    }

    GUID deviceCandidates[] = {
        IID_IDirect3DTnLHalDevice,
        IID_IDirect3DHALDevice,
        IID_IDirect3DRGBDevice
    };

    int renderInitHr = -1;
    for (GUID& deviceGuid : deviceCandidates) {
        renderInitHr = g_3dDevice.Init(g_hMainWnd, nullptr, &deviceGuid, nullptr, 0);
        if (renderInitHr >= 0) {
            break;
        }
    }

    if (renderInitHr < 0) {
        ErrorMsg("3D device initialization failed. The game will exit.");
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int renderW = (std::max)(1L, clientRect.right - clientRect.left);
    const int renderH = (std::max)(1L, clientRect.bottom - clientRect.top);

    g_renderer.Init();
    g_renderer.SetSize(renderW, renderH);
    return true;
}

// ---------------------------------------------------------------------------
// WinMain  –  Application entry point
// ---------------------------------------------------------------------------
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                       char* lpCmdLine, int nCmdShow)
{
    // Set CWD to the directory containing this exe so that relative paths
    // like "data.grf" and "data\sprite\cursors.spr" resolve correctly
    // regardless of how/where the process was launched.
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        char* lastSlash = std::strrchr(exePath, '\\');
        if (lastSlash) { *lastSlash = '\0'; SetCurrentDirectoryA(exePath); }
    }

    // Set unhandled exception filter
    SetUnhandledExceptionFilter(nullptr);

    // Initialise high-resolution timer
    TIMECAPS tc{};
    UINT timerRes = 0;
    if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR)
    {
        timerRes = std::max(static_cast<UINT>(tc.wPeriodMin), 1u);
        timerRes = std::min(timerRes, static_cast<UINT>(tc.wPeriodMax));
        timeBeginPeriod(timerRes);
    }
    ResetTimer();

    CoInitialize(nullptr);

    // Read registry once. If missing, try setup; if that is unavailable, keep defaults
    // so we can still create and show the game window.
    if (!ReadRegistry())
    {
        const bool hasSetupExe = (_access("Setup\\SetupHp.exe", 0) == 0);
        if (hasSetupExe) {
            STARTUPINFOA si{};
            PROCESS_INFORMATION pi{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOW;

            if (CreateProcessA(nullptr, (LPSTR)"Setup\\SetupHp.exe", nullptr, nullptr, FALSE,
                               0, nullptr, nullptr, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ReadRegistry();
            }
        }

        // Safe defaults when registry/setup are unavailable.
        WINDOW_WIDTH = 1920;
        WINDOW_HEIGHT = 1080;
        g_isAppActive = true;
    }

    // Floating-point control
    _controlfp(0x20000, 0x30000);

    // Create the game window
    if (!InitApp(hInstance, nCmdShow))
    {
        if (timerRes) timeEndPeriod(timerRes);
        return 1;
    }

    // Phase 3: Dynamic DLL Loading
    // Must happen before AddPak: GRF v0x200 index decompression requires
    // zlib's uncompress(), which is resolved from a DLL by CDllMgr::LoadAll().
    if (!CDllMgr::LoadAll())
    {
        ErrorMsg("Failed to load one or more supporting DLLs (Granny, Miles, Bink, IJL, etc.).\nPlease ensure the 'dlls' folder exists and contains the required binaries.");
        // We could return 1 here to enforce DLL presence
    }

    // Register PAK archives (must come after DLL loading for zlib decompression)
    if (lpCmdLine && strstr(lpCmdLine, "/pc"))
    {
        // Offline / test mode: skip data.grf
    }
    else
    {
        g_fileMgr.AddPak("data.grf");
    }
    g_fileMgr.AddPak("data_hp.grf");
    g_fileMgr.AddPak("event.grf");
    g_fileMgr.AddPak("fdata.grf");

    // Initialise Winsock
    if (!CConnection::Startup())
    {
        if (timerRes) timeEndPeriod(timerRes);
        return 1;
    }

    if (!InitClientSystems())
    {
        CConnection::Cleanup();
        if (timerRes) timeEndPeriod(timerRes);
        return 1;
    }

    // Initialise frame timer (60 fps target)
    InitTimer(60);

    SetFocus(g_hMainWnd);

    // --- Enter the main game loop ---
    // Mode 0 = Login, Mode 1 = Game.
    g_modeMgr.Run(0, "");

    // --- Cleanup ---
    // CDllMgr cleanup will happen automatically if we add it to a destructor or call it explicitly
    CConnection::Cleanup();
    g_windowMgr.Reset();
    g_3dDevice.DestroyObjects();

    CoUninitialize();
    DestroyWindow(g_hMainWnd);

    if (timerRes)
        timeEndPeriod(timerRes);

    return 0;
}
