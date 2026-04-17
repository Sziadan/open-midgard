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
#include "core/SettingsIni.h"
#include "core/Timer.h"
#include "core/File.h"
#include "gamemode/CursorRenderer.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "network/Connection.h"
#include "core/DllMgr.h"
#include "core/ClientInfoLocale.h"
#include "lua/LuaBridge.h"
#include "qtui/QtUiRuntime.h"
#include "ui/UIOptionWnd.h"
#include "ui/UiScale.h"
#include "ui/UIWindowMgr.h"
#include "render3d/Device.h"
#include "render3d/RenderBackend.h"
#include "render3d/GraphicsSettings.h"
#include "render3d/RenderDevice.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "world/World.h"
#include "res/GndRes.h"
#include "res/Bitmap.h"
#include "res/ModelRes.h"
#include "res/Sprite.h"
#include "res/ActRes.h"
#include "res/EzEffectRes.h"
#include "res/PaletteRes.h"
#include "res/ImfRes.h"
#include "res/WorldRes.h"
#include "audio/Audio.h"
#include "DebugLog.h"
#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <io.h>
#include <algorithm>
#include <objbase.h>
#include <string>

// ---------------------------------------------------------------------------
// Window constants
// ---------------------------------------------------------------------------
const char* const WINDOW_NAME = "open-midgard";
constexpr WORD kAppIconResourceId = 101;

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
int       g_isSoundOn   = 1;
int       g_frameskip   = 0;
char      g_baseDir [MAX_PATH] = {};
char      g_baseDir3[MAX_PATH] = {};

CFileMgr  g_fileMgr;
int       g_readFolderFirst = 0;   // 0=PAK-first, 1=disk-first
static RenderBackendType g_activeRenderBackend = RenderBackendType::LegacyDirect3D7;
static std::string g_windowTitleStatusSuffix;

static void LogWinMainAtExit()
{
    DbgLog("[Shutdown] atexit callback entered\n");
}
static int g_windowTitleFps = -1;
static DWORD g_windowTitleFpsTick = 0;
static unsigned int g_windowTitleFrameCount = 0;

namespace {

constexpr int kCrashMiniDumpTypePrimary = 0x21065;
constexpr int kCrashMiniDumpTypeFallback = 0x20000;

using MiniDumpWriteDumpFn = BOOL (WINAPI*)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);

struct CrashMiniDumpExceptionInfo {
    DWORD threadId;
    EXCEPTION_POINTERS* exceptionPointers;
    BOOL clientPointers;
};

RECT GetPrimaryMonitorRect()
{
    RECT rect{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    POINT origin{ 0, 0 };
    const HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoA(monitor, &monitorInfo)) {
        rect = monitorInfo.rcMonitor;
    }
    return rect;
}

WindowMode GetStartupWindowMode()
{
    return GetEffectiveWindowModeForBackend(GetRequestedRenderBackend(), GetCachedGraphicsSettings().windowMode);
}

void ApplyConfiguredWindowSize()
{
    const GraphicsSettings& settings = GetCachedGraphicsSettings();
    WINDOW_WIDTH = settings.width;
    WINDOW_HEIGHT = settings.height;
}

std::string GetExecutableDirectory()
{
    char modulePath[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return std::string();
    }

    for (int index = static_cast<int>(len) - 1; index >= 0; --index) {
        if (modulePath[index] == '\\' || modulePath[index] == '/') {
            modulePath[index] = '\0';
            break;
        }
    }

    return std::string(modulePath);
}

std::string BuildCrashArtifactStem()
{
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);

    const std::string exeDir = GetExecutableDirectory();
    const std::string crashDir = exeDir.empty()
        ? std::string("crash-dumps")
        : exeDir + "\\crash-dumps";
    CreateDirectoryA(crashDir.c_str(), nullptr);

    char stem[512] = {};
    std::snprintf(stem,
        sizeof(stem),
        "%s\\open-midgard-crash-%04u%02u%02u-%02u%02u%02u-pid%lu",
        crashDir.c_str(),
        static_cast<unsigned int>(localTime.wYear),
        static_cast<unsigned int>(localTime.wMonth),
        static_cast<unsigned int>(localTime.wDay),
        static_cast<unsigned int>(localTime.wHour),
        static_cast<unsigned int>(localTime.wMinute),
        static_cast<unsigned int>(localTime.wSecond),
        static_cast<unsigned long>(GetCurrentProcessId()));
    return std::string(stem);
}

std::string BuildSystemDllPath(const char* dllName)
{
    if (!dllName || !*dllName) {
        return std::string();
    }

    char systemDir[MAX_PATH] = {};
    const UINT len = GetSystemDirectoryA(systemDir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return std::string(dllName);
    }

    std::string fullPath(systemDir, systemDir + len);
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath.push_back('\\');
    }
    fullPath += dllName;
    return fullPath;
}

void WriteCrashReportText(const std::string& reportPath,
    DWORD exceptionCode,
    const void* exceptionAddress,
    const std::string& dumpPath,
    bool dumpWritten,
    DWORD dumpError,
    const std::string& dbghelpPath,
    int dumpTypeUsed)
{
    HANDLE reportFile = CreateFileA(
        reportPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (reportFile == INVALID_HANDLE_VALUE) {
        return;
    }

    char report[1024] = {};
    const int written = std::snprintf(
        report,
        sizeof(report),
        "Unhandled exception code=0x%08lX address=%p\r\n"
        "Minidump=%s\r\n"
        "MinidumpWritten=%d\r\n"
        "MinidumpError=%lu\r\n"
        "MinidumpType=0x%X\r\n"
        "DbgHelp=%s\r\n",
        static_cast<unsigned long>(exceptionCode),
        exceptionAddress,
        dumpPath.c_str(),
        dumpWritten ? 1 : 0,
        static_cast<unsigned long>(dumpError),
        static_cast<unsigned int>(dumpTypeUsed),
        dbghelpPath.c_str());
    if (written > 0) {
        DWORD bytesWritten = 0;
        WriteFile(reportFile, report, static_cast<DWORD>(written), &bytesWritten, nullptr);
    }

    CloseHandle(reportFile);
}

bool TryWriteCrashDumpFile(MiniDumpWriteDumpFn miniDumpWriteDump,
    HANDLE dumpFile,
    EXCEPTION_POINTERS* exceptionInfo,
    int dumpType,
    DWORD* outError)
{
    if (!miniDumpWriteDump || dumpFile == INVALID_HANDLE_VALUE) {
        if (outError) {
            *outError = ERROR_INVALID_HANDLE;
        }
        return false;
    }

    SetFilePointer(dumpFile, 0, nullptr, FILE_BEGIN);
    SetEndOfFile(dumpFile);

    CrashMiniDumpExceptionInfo dumpInfo{};
    dumpInfo.threadId = GetCurrentThreadId();
    dumpInfo.exceptionPointers = exceptionInfo;
    dumpInfo.clientPointers = FALSE;

    const BOOL wroteDump = miniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        dumpFile,
        dumpType,
        exceptionInfo ? &dumpInfo : nullptr,
        nullptr,
        nullptr);
    if (wroteDump == TRUE) {
        if (outError) {
            *outError = ERROR_SUCCESS;
        }
        return true;
    }

    if (outError) {
        *outError = GetLastError();
    }
    return false;
}

LONG WINAPI WriteUnhandledCrashDump(EXCEPTION_POINTERS* exceptionInfo)
{
    const DWORD exceptionCode = (exceptionInfo && exceptionInfo->ExceptionRecord)
        ? exceptionInfo->ExceptionRecord->ExceptionCode
        : 0;
    const void* exceptionAddress = (exceptionInfo && exceptionInfo->ExceptionRecord)
        ? exceptionInfo->ExceptionRecord->ExceptionAddress
        : nullptr;

    const std::string artifactStem = BuildCrashArtifactStem();
    const std::string dumpPath = artifactStem + ".dmp";
    const std::string reportPath = artifactStem + ".txt";

    bool dumpWritten = false;
    DWORD dumpError = 0;
    int dumpTypeUsed = 0;
    const std::string dbghelpPath = BuildSystemDllPath("dbghelp.dll");
    HMODULE dbghelp = LoadLibraryA(dbghelpPath.c_str());
    if (dbghelp) {
        MiniDumpWriteDumpFn miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFn>(
            GetProcAddress(dbghelp, "MiniDumpWriteDump"));
        if (miniDumpWriteDump) {
            HANDLE dumpFile = CreateFileA(
                dumpPath.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (dumpFile != INVALID_HANDLE_VALUE) {
                dumpWritten = TryWriteCrashDumpFile(
                    miniDumpWriteDump,
                    dumpFile,
                    exceptionInfo,
                    kCrashMiniDumpTypePrimary,
                    &dumpError);
                if (dumpWritten) {
                    dumpTypeUsed = kCrashMiniDumpTypePrimary;
                } else {
                    dumpWritten = TryWriteCrashDumpFile(
                        miniDumpWriteDump,
                        dumpFile,
                        exceptionInfo,
                        kCrashMiniDumpTypeFallback,
                        &dumpError);
                    if (dumpWritten) {
                        dumpTypeUsed = kCrashMiniDumpTypeFallback;
                    }
                }
                if (!dumpWritten) {
                    dumpWritten = TryWriteCrashDumpFile(
                        miniDumpWriteDump,
                        dumpFile,
                        nullptr,
                        0,
                        &dumpError);
                    if (dumpWritten) {
                        dumpTypeUsed = 0;
                    }
                }

                CloseHandle(dumpFile);
                if (!dumpWritten) {
                    DeleteFileA(dumpPath.c_str());
                }
            } else {
                dumpError = GetLastError();
            }
        } else {
            dumpError = GetLastError();
        }
        FreeLibrary(dbghelp);
    } else {
        dumpError = GetLastError();
    }

    WriteCrashReportText(reportPath, exceptionCode, exceptionAddress, dumpPath, dumpWritten, dumpError, dbghelpPath, dumpTypeUsed);
    DbgLog("[Crash] Unhandled exception code=0x%08lX address=%p dump='%s' report='%s' wroteDump=%d dumpError=%lu dumpType=0x%X dbghelp='%s'\n",
        static_cast<unsigned long>(exceptionCode),
        exceptionAddress,
        dumpPath.c_str(),
        reportPath.c_str(),
        dumpWritten ? 1 : 0,
        static_cast<unsigned long>(dumpError),
        static_cast<unsigned int>(dumpTypeUsed),
        dbghelpPath.c_str());
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

// ---------------------------------------------------------------------------
// MSS (Miles Sound System) shim stubs
// Replace with real MSS calls if the SDK is available.
// ---------------------------------------------------------------------------
bool InitMSS()   { return CAudio::GetInstance()->Init(); }
void UnInitMSS() { CAudio::GetInstance()->Shutdown(); }

// ---------------------------------------------------------------------------
// ErrorMsg
// ---------------------------------------------------------------------------
void ErrorMsg(const char* msg)
{
    MessageBoxA(g_hMainWnd, msg, "Error", MB_OK | MB_ICONERROR);
}

void ErrorMsg(int /*msgId*/) {}

static void ApplyMainWindowTitle()
{
    if (!g_hMainWnd) {
        return;
    }

    std::string title = WINDOW_NAME;
    if (!g_windowTitleStatusSuffix.empty()) {
        title += " - ";
        title += g_windowTitleStatusSuffix;
    }
    title += " [";
    title += GetRenderBackendName(g_activeRenderBackend);
    title += "]";
    if (g_windowTitleFps >= 0) {
        char fpsText[32] = {};
        std::snprintf(fpsText, sizeof(fpsText), " - %d FPS", g_windowTitleFps);
        title += fpsText;
    }
    SetWindowTextA(g_hMainWnd, title.c_str());
}

void RefreshMainWindowTitle(const char* status)
{
    if (status && *status) {
        g_windowTitleStatusSuffix = status;
    } else {
        g_windowTitleStatusSuffix.clear();
    }
    ApplyMainWindowTitle();
}

void RecordMainWindowFrame()
{
    const DWORD now = GetTickCount();
    if (g_windowTitleFpsTick == 0) {
        g_windowTitleFpsTick = now;
        g_windowTitleFrameCount = 0;
    }

    ++g_windowTitleFrameCount;
    const DWORD elapsed = now - g_windowTitleFpsTick;
    if (elapsed < 1000) {
        return;
    }

    g_windowTitleFps = static_cast<int>((static_cast<unsigned long long>(g_windowTitleFrameCount) * 1000ull + (elapsed / 2ull)) / elapsed);
    g_windowTitleFpsTick = now;
    g_windowTitleFrameCount = 0;
    ApplyMainWindowTitle();
}

RenderBackendType GetActiveRenderBackend()
{
    return g_activeRenderBackend;
}

bool RelaunchCurrentApplication()
{
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0 || exePath[0] == '\0') {
        return false;
    }

    char workingDirectory[MAX_PATH] = {};
    if (GetCurrentDirectoryA(MAX_PATH, workingDirectory) == 0) {
        workingDirectory[0] = '\0';
    }

    const char* originalCommandLine = GetCommandLineA();
    if (!originalCommandLine || !*originalCommandLine) {
        return false;
    }

    std::string commandLine(originalCommandLine);
    std::vector<char> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back('\0');

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    if (!CreateProcessA(
            exePath,
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            workingDirectory[0] != '\0' ? workingDirectory : nullptr,
            &startupInfo,
            &processInfo)) {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    CRagConnection::instance()->Disconnect();
    g_modeMgr.Quit();
    return true;
}

// ---------------------------------------------------------------------------
// Settings bootstrap
// ---------------------------------------------------------------------------
int ReadRegistry()
{
    EnsureOpenMidgardIniDefaults();
    ApplyConfiguredWindowSize();
    (void)GetConfiguredRenderBackend();
    return 1;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        UpdateModeCursorClientPos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        SetCursor(nullptr);
        break;
    case WM_MOUSEWHEEL:
    {
        POINT screenPoint{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT clientPoint = screenPoint;
        if (ScreenToClient(hwnd, &clientPoint)) {
            UpdateModeCursorClientPos(clientPoint.x, clientPoint.y);
        }
        SetCursor(nullptr);
        break;
    }
    default:
        break;
    }

    if (HandleQtUiRuntimeWindowMessage(msg, wParam, lParam)) {
        return 0;
    }

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
        const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(x) : x;
        const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(y) : y;
        const bool uiHit = g_windowMgr.HasWindowAtPoint(uiX, uiY);
        g_windowMgr.OnLBtnDown(uiX, uiY);
        if (!uiHit) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_LButtonDown, x, y);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(x) : x;
        const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(y) : y;
        const bool uiCaptured = (g_windowMgr.m_captureWindow != nullptr);
        const bool uiHit = g_windowMgr.HasWindowAtPoint(uiX, uiY);
        g_windowMgr.OnLBtnUp(uiX, uiY);
        if (!uiCaptured && !uiHit) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_LButtonUp, x, y);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        g_windowMgr.OnLBtnDblClk(
            IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(x) : x,
            IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(y) : y);
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(x) : x;
        const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(y) : y;
        if (!g_windowMgr.HasWindowAtPoint(uiX, uiY)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RButtonDown, x, y);
        }
        return 0;
    }

    case WM_RBUTTONUP:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(x) : x;
        const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(y) : y;
        if (!g_windowMgr.HasWindowAtPoint(uiX, uiY)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RButtonUp, x, y);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(x) : x;
        const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(y) : y;
        const bool uiCaptured = (g_windowMgr.m_captureWindow != nullptr);
        g_windowMgr.OnMouseMove(uiX, uiY);
        if (!uiCaptured && !g_windowMgr.HasWindowAtPoint(uiX, uiY)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_MouseMove, x, y);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        POINT screenPoint{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT clientPoint = screenPoint;
        ScreenToClient(hwnd, &clientPoint);
        const int uiX = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(clientPoint.x) : clientPoint.x;
        const int uiY = IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(clientPoint.y) : clientPoint.y;
        if (!g_windowMgr.OnWheel(uiX, uiY, GET_WHEEL_DELTA_WPARAM(wParam))) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_MouseWheel, GET_WHEEL_DELTA_WPARAM(wParam), 0);
        }
        return 0;
    }

    case WM_CHAR:
        g_windowMgr.OnChar(static_cast<char>(wParam));
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
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
    if (FindWindowA(WINDOW_NAME, nullptr))
        g_multiSTOP = true;

    WNDCLASSA wc    = {};
    wc.style        = CS_DBLCLKS;
    wc.lpfnWndProc  = WindowProc;
    wc.hInstance    = hInstance;
    wc.hIcon        = LoadIconA(hInstance, MAKEINTRESOURCEA(kAppIconResourceId));
    wc.hCursor      = nullptr;
    wc.hbrBackground= nullptr;
    wc.lpszClassName= WINDOW_NAME;
    RegisterClassA(&wc);

    const WindowMode windowMode = GetStartupWindowMode();
    const RECT monitorRect = GetPrimaryMonitorRect();
    DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    int winLeft = monitorRect.left;
    int winTop = monitorRect.top;
    int windowWidth = WINDOW_WIDTH;
    int windowHeight = WINDOW_HEIGHT;

    if (windowMode == WindowMode::Windowed) {
        RECT windowRect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        AdjustWindowRect(&windowRect, windowStyle, FALSE);
        windowWidth = windowRect.right - windowRect.left;
        windowHeight = windowRect.bottom - windowRect.top;
        const int monitorWidth = monitorRect.right - monitorRect.left;
        const int monitorHeight = monitorRect.bottom - monitorRect.top;
        winLeft = monitorRect.left + (monitorWidth - windowWidth) / 2;
        winTop = monitorRect.top + (monitorHeight - windowHeight) / 2;
    } else {
        windowStyle = WS_POPUP;
        windowWidth = monitorRect.right - monitorRect.left;
        windowHeight = monitorRect.bottom - monitorRect.top;
    }

    g_hMainWnd = CreateWindowExA(
        0, WINDOW_NAME, WINDOW_NAME,
        windowStyle,
        winLeft, winTop,
        windowWidth,
        windowHeight,
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
    g_resMgr.RegisterType("str", "data\\texture\\effect\\", new CEZeffectRes());
    g_resMgr.RegisterType("pal", "", new CPaletteRes());
    g_resMgr.RegisterType("imf", "", new CImfRes());
    g_resMgr.RegisterType("rsw", "", new C3dWorldRes());
    g_resMgr.RegisterType("wav", "", new CWave());

    InitMSS();
    ApplySavedAudioSettings();

    if (!g_windowMgr.Init()) {
        ErrorMsg("UI system initialization failed.");
        return false;
    }

    if (!g_buabridge.Initialize()) {
        ErrorMsg("Lua runtime initialization failed.");
        return false;
    }

    const char* const kBootstrapLuaScripts[] = {
        "lua files\\datainfo\\enumvar.lub",
        "lua files\\datainfo\\weapontable.lub",
        "lua files\\admin\\pcidentity.lub",
        "lua files\\admin\\pcjobname.lub",
        "lua files\\datainfo\\pcjobnamegender.lub",
        "lua files\\datainfo\\jobname.lub",
    };
    for (const char* scriptPath : kBootstrapLuaScripts) {
        if (!g_buabridge.LoadRagnarokScriptOnce(scriptPath)) {
            DbgLog("[Lua] Bootstrap preload failed for '%s': %s\n",
                scriptPath,
                g_buabridge.GetLastError().c_str());
        }
    }

    DbgLog("[Render] Requested backend is '%s'.\n",
        GetRenderBackendName(GetRequestedRenderBackend()));

    RenderBackendBootstrapResult renderBootstrap{};
    if (!GetRenderDevice().Initialize(g_hMainWnd, &renderBootstrap)) {
        ErrorMsg("3D device initialization failed. The game will exit.");
        return false;
    }

    g_activeRenderBackend = renderBootstrap.backend;
    DbgLog("[Render] Active backend confirmed as '%s'.\n", GetRenderBackendName(g_activeRenderBackend));
    g_windowTitleFps = -1;
    g_windowTitleFpsTick = 0;
    g_windowTitleFrameCount = 0;
    RefreshMainWindowTitle();

    GetRenderDevice().RefreshRenderSize();
    const int renderW = GetRenderDevice().GetRenderWidth();
    const int renderH = GetRenderDevice().GetRenderHeight();

    g_renderer.Init();
    g_renderer.SetSize(renderW, renderH);
    InitializeQtUiRuntime(g_hMainWnd);
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

    // Install an unhandled exception filter that writes a minidump next to the executable.
    SetUnhandledExceptionFilter(WriteUnhandledCrashDump);
    std::atexit(LogWinMainAtExit);

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

    // Initialize persisted settings from open-midgard.ini beside the executable.
    ReadRegistry();
    g_isAppActive = true;

    // Floating-point control
    _controlfp(0x20000, 0x30000);

    // The legacy client initializes shared trig lookup tables during startup.
    // Several world/effect paths still rely on these helpers.
    CreateTrigonometricTable();

    // Create the game window
    if (!InitApp(hInstance, nCmdShow))
    {
        if (timerRes) timeEndPeriod(timerRes);
        return 1;
    }

    // Phase 3: Dynamic DLL Loading
    // Legacy middleware is now optional in x64 milestone builds; GRF zlib
    // decompression is linked in-process and no longer depends on cps.dll.
    if (!CDllMgr::LoadAll())
    {
        ErrorMsg(CDllMgr::GetLastLoadReport().c_str());
    }
    else if (!CDllMgr::GetLastLoadReport().empty())
    {
        DbgLog("%s", CDllMgr::GetLastLoadReport().c_str());
    }

    // Register PAK archives.
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
    DbgLog("[Shutdown] Begin cleanup after mode loop.\n");
    DbgLog("[Shutdown] CConnection::Cleanup start\n");
    CConnection::Cleanup();
    DbgLog("[Shutdown] CConnection::Cleanup done\n");
    DbgLog("[Shutdown] UIWindowMgr::Reset start\n");
    g_windowMgr.Reset();
    DbgLog("[Shutdown] UIWindowMgr::Reset done\n");
    DbgLog("[Shutdown] Lua shutdown start\n");
    g_buabridge.Shutdown();
    DbgLog("[Shutdown] Lua shutdown done\n");
    DbgLog("[Shutdown] Qt UI runtime shutdown start\n");
    ShutdownQtUiRuntime();
    DbgLog("[Shutdown] Qt UI runtime shutdown done\n");
    DbgLog("[Shutdown] Resource manager unload start\n");
    g_resMgr.Reset();
    DbgLog("[Shutdown] Resource manager unload done\n");
    DbgLog("[Shutdown] Render device shutdown start\n");
    GetRenderDevice().Shutdown();
    DbgLog("[Shutdown] Render device shutdown done\n");
    DbgLog("[Shutdown] Audio shutdown start\n");
    UnInitMSS();
    DbgLog("[Shutdown] Audio shutdown done\n");

    DbgLog("[Shutdown] CoUninitialize start\n");
    CoUninitialize();
    DbgLog("[Shutdown] CoUninitialize done\n");
    DbgLog("[Shutdown] DestroyWindow start hwnd=%p\n", g_hMainWnd);
    DestroyWindow(g_hMainWnd);
    g_hMainWnd = nullptr;
    DbgLog("[Shutdown] DestroyWindow done\n");

    if (timerRes) {
        DbgLog("[Shutdown] timeEndPeriod start res=%u\n", timerRes);
        timeEndPeriod(timerRes);
        DbgLog("[Shutdown] timeEndPeriod done\n");
    }

    DbgLog("[Shutdown] WinMain returning 0\n");

    return 0;
}
