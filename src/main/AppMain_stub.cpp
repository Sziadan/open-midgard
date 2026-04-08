#include "WinMain.h"

#include "DebugLog.h"
#include "audio/Audio.h"
#include "core/ClientInfoLocale.h"
#include "core/DllMgr.h"
#include "core/File.h"
#include "core/SettingsIni.h"
#include "core/Timer.h"
#include "gamemode/CursorRenderer.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "lua/LuaBridge.h"
#include "network/Connection.h"
#include "qtui/QtPlatformWindow.h"
#include "qtui/QtUiRuntime.h"
#include "render/Prim.h"
#include "render/Renderer.h"
#include "render3d/GraphicsSettings.h"
#include "render3d/RenderDevice.h"
#include "res/ActRes.h"
#include "res/Bitmap.h"
#include "res/EzEffectRes.h"
#include "res/GndRes.h"
#include "res/ImfRes.h"
#include "res/ModelRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "res/WorldRes.h"
#include "ui/UIOptionWnd.h"
#include "ui/UIWindowMgr.h"
#include "world/World.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr unsigned int WM_MOUSEMOVE = 0x0200u;
constexpr unsigned int WM_LBUTTONDOWN = 0x0201u;
constexpr unsigned int WM_LBUTTONUP = 0x0202u;
constexpr unsigned int WM_LBUTTONDBLCLK = 0x0203u;
constexpr unsigned int WM_RBUTTONDOWN = 0x0204u;
constexpr unsigned int WM_RBUTTONUP = 0x0205u;
constexpr unsigned int WM_MOUSEWHEEL = 0x020Au;
constexpr unsigned int WM_CHAR = 0x0102u;
constexpr unsigned int WM_KEYDOWN = 0x0100u;
constexpr unsigned int WM_SYSKEYDOWN = 0x0104u;
constexpr unsigned int WM_ACTIVATE = 0x0006u;
constexpr unsigned int WM_CLOSE = 0x0010u;

constexpr std::uintptr_t WA_INACTIVE = 0u;

RenderBackendType g_activeRenderBackend = RenderBackendType::LegacyDirect3D7;
std::string g_windowTitleStatusSuffix;
int g_windowTitleFps = -1;
DWORD g_windowTitleFpsTick = 0;
unsigned int g_windowTitleFrameCount = 0;

int GetLParamX(LPARAM lParam)
{
    return static_cast<int>(static_cast<short>(lParam & 0xFFFF));
}

int GetLParamY(LPARAM lParam)
{
    return static_cast<int>(static_cast<short>((static_cast<unsigned long>(lParam) >> 16) & 0xFFFF));
}

int GetWheelDeltaWParam(WPARAM wParam)
{
    return static_cast<int>(static_cast<short>((static_cast<unsigned long>(wParam) >> 16) & 0xFFFF));
}

void ApplyConfiguredWindowSize()
{
    const GraphicsSettings& settings = GetCachedGraphicsSettings();
    WINDOW_WIDTH = settings.width;
    WINDOW_HEIGHT = settings.height;
}

std::filesystem::path GetExecutableDirectory()
{
    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) == 0 || modulePath[0] == '\0') {
        return {};
    }

    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path NormalizeRuntimeCandidate(std::filesystem::path candidate)
{
    if (candidate.empty()) {
        return {};
    }

#if !defined(_WIN32)
    std::string candidateString = candidate.generic_string();
    if (candidateString.size() >= 3
        && std::isalpha(static_cast<unsigned char>(candidateString[0])) != 0
        && candidateString[1] == ':'
        && candidateString[2] == '/') {
        std::string mountPath = "/mnt/";
        mountPath.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(candidateString[0]))));
        mountPath.append(candidateString.substr(2));
        candidate = std::filesystem::path(mountPath);
    }
#endif

    return candidate;
}

bool HasAnyRuntimeAsset(const std::filesystem::path& root)
{
    if (root.empty()) {
        return false;
    }

    std::error_code ec;
    return std::filesystem::exists(root / "data.grf", ec)
        || std::filesystem::exists(root / "data_hp.grf", ec)
        || std::filesystem::exists(root / "event.grf", ec)
        || std::filesystem::exists(root / "fdata.grf", ec)
        || std::filesystem::exists(root / "clientinfo.xml", ec)
        || std::filesystem::exists(root / "sclientinfo.xml", ec)
        || std::filesystem::exists(root / "System" / "clientinfo.xml", ec)
        || std::filesystem::exists(root / "System" / "sclientinfo.xml", ec)
        || std::filesystem::is_directory(root / "data", ec)
        || std::filesystem::is_directory(root / "System", ec);
}

void AppendSearchCandidates(std::vector<std::filesystem::path>* outCandidates, const std::filesystem::path& start)
{
    if (!outCandidates || start.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::path current = NormalizeRuntimeCandidate(start);
    current = std::filesystem::weakly_canonical(current, ec);
    if (ec) {
        current = NormalizeRuntimeCandidate(start);
    }

    for (;;) {
        if (current.empty()) {
            break;
        }

        if (std::find(outCandidates->begin(), outCandidates->end(), current) == outCandidates->end()) {
            outCandidates->push_back(current);
        }

        const std::filesystem::path parent = current.parent_path();
        if (parent.empty() || parent == current) {
            break;
        }
        current = parent;
    }
}

std::filesystem::path ResolveRuntimeRoot()
{
    std::vector<std::filesystem::path> candidates;

#ifdef RO_DEV_DEPLOY_DIR
    AppendSearchCandidates(&candidates, std::filesystem::path(RO_DEV_DEPLOY_DIR));
#endif

    if (const char* overrideRoot = std::getenv("OPEN_MIDGARD_DATA_DIR")) {
        if (*overrideRoot) {
            AppendSearchCandidates(&candidates, std::filesystem::path(overrideRoot));
        }
    }

    std::error_code ec;
    AppendSearchCandidates(&candidates, std::filesystem::current_path(ec));
    const std::filesystem::path executableDirectory = GetExecutableDirectory();
    AppendSearchCandidates(&candidates, executableDirectory);

    for (const std::filesystem::path& candidate : candidates) {
        if (HasAnyRuntimeAsset(candidate)) {
            return candidate;
        }
    }

    if (!executableDirectory.empty()) {
        return executableDirectory;
    }
    return std::filesystem::current_path(ec);
}

void ApplyRuntimeRoot(const std::filesystem::path& runtimeRoot)
{
    if (runtimeRoot.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::current_path(runtimeRoot, ec);
    const std::string runtimeRootString = runtimeRoot.string();
    if (ec) {
        DbgLog("[Runtime] Failed to switch runtime root to '%s' (%s).\n",
            runtimeRootString.c_str(),
            ec.message().c_str());
        return;
    }

    DbgLog("[Runtime] Using runtime root '%s'.\n", runtimeRootString.c_str());
}

WindowMode GetStartupWindowMode()
{
    return GetEffectiveWindowModeForBackend(GetRequestedRenderBackend(), GetCachedGraphicsSettings().windowMode);
}

void ApplyMainWindowTitle()
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
    RoQtSetWindowTitle(g_hMainWnd, title.c_str());
}

bool InitClientSystems()
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

    if (!CAudio::GetInstance()->Init()) {
        return false;
    }
    ApplySavedAudioSettings();

    if (!g_windowMgr.Init()) {
        std::fputs("UI system initialization failed.\n", stderr);
        return false;
    }

    if (!g_buabridge.Initialize()) {
        std::fputs("Lua runtime initialization failed.\n", stderr);
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

    DbgLog("[Render] Requested backend is '%s'.\n", GetRenderBackendName(GetRequestedRenderBackend()));

    RenderBackendBootstrapResult renderBootstrap{};
    if (!GetRenderDevice().Initialize(g_hMainWnd, &renderBootstrap)) {
        std::fputs("3D device initialization failed. The game will exit.\n", stderr);
        return false;
    }

    g_activeRenderBackend = renderBootstrap.backend;
    DbgLog("[Render] Active backend confirmed as '%s'.\n", GetRenderBackendName(g_activeRenderBackend));
    g_windowTitleFps = -1;
    g_windowTitleFpsTick = 0;
    g_windowTitleFrameCount = 0;
    RefreshMainWindowTitle();

    GetRenderDevice().RefreshRenderSize();
    g_renderer.Init();
    g_renderer.SetSize(GetRenderDevice().GetRenderWidth(), GetRenderDevice().GetRenderHeight());
    InitializeQtUiRuntime(g_hMainWnd);
    return true;
}

} // namespace

void ErrorMsg(const char* msg)
{
    if (msg && *msg) {
        std::fputs(msg, stderr);
        std::fputc('\n', stderr);
    }
}

void ErrorMsg(int)
{
}

const char* const WINDOW_NAME = "open-midgard";

int WINDOW_WIDTH = 1920;
int WINDOW_HEIGHT = 1080;

HWND g_hMainWnd = nullptr;
HINSTANCE g_hInstance = nullptr;
bool g_isAppActive = false;
bool g_multiSTOP = false;
int g_soundMode = 1;
int g_isSoundOn = 1;
int g_frameskip = 0;
char g_baseDir[MAX_PATH] = {};
char g_baseDir3[MAX_PATH] = {};

CFileMgr g_fileMgr;
int g_readFolderFirst = 0;

bool InitApp(HINSTANCE, int)
{
    ApplyConfiguredWindowSize();
    const bool fullscreen = GetStartupWindowMode() == WindowMode::Fullscreen;
    if (!RoQtCreateMainWindow(WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT, fullscreen, WindowProc, &g_hMainWnd)) {
        return false;
    }

    RoQtShowWindow(g_hMainWnd);
    const std::string cwd = std::filesystem::current_path().string();
    std::snprintf(g_baseDir3, sizeof(g_baseDir3), "%s", cwd.c_str());
    std::snprintf(g_baseDir, sizeof(g_baseDir), "%s/", cwd.c_str());
    g_isAppActive = true;
    return true;
}

int ReadRegistry()
{
    EnsureOpenMidgardIniDefaults();
    ApplyConfiguredWindowSize();
    (void)GetConfiguredRenderBackend();
    return 1;
}

LRESULT CALLBACK WindowProc(HWND, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (HandleQtUiRuntimeWindowMessage(msg, wParam, lParam)) {
        return 0;
    }

    switch (msg)
    {
    case WM_ACTIVATE:
        g_isAppActive = (wParam != WA_INACTIVE);
        return 0;

    case WM_CLOSE:
        g_modeMgr.Quit();
        return 0;

    case WM_LBUTTONDOWN:
    {
        const int x = GetLParamX(lParam);
        const int y = GetLParamY(lParam);
        const bool uiHit = g_windowMgr.HasWindowAtPoint(x, y);
        UpdateModeCursorClientPos(x, y);
        g_windowMgr.OnLBtnDown(x, y);
        if (!uiHit) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_LButtonDown, x, y);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        const int x = GetLParamX(lParam);
        const int y = GetLParamY(lParam);
        const bool uiCaptured = (g_windowMgr.m_captureWindow != nullptr);
        const bool uiHit = g_windowMgr.HasWindowAtPoint(x, y);
        UpdateModeCursorClientPos(x, y);
        g_windowMgr.OnLBtnUp(x, y);
        if (!uiCaptured && !uiHit) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_LButtonUp, x, y);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        g_windowMgr.OnLBtnDblClk(GetLParamX(lParam), GetLParamY(lParam));
        return 0;

    case WM_RBUTTONDOWN:
    {
        const int x = GetLParamX(lParam);
        const int y = GetLParamY(lParam);
        UpdateModeCursorClientPos(x, y);
        if (!g_windowMgr.HasWindowAtPoint(x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RButtonDown, x, y);
        }
        return 0;
    }

    case WM_RBUTTONUP:
    {
        const int x = GetLParamX(lParam);
        const int y = GetLParamY(lParam);
        UpdateModeCursorClientPos(x, y);
        if (!g_windowMgr.HasWindowAtPoint(x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RButtonUp, x, y);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        const int x = GetLParamX(lParam);
        const int y = GetLParamY(lParam);
        const bool uiCaptured = (g_windowMgr.m_captureWindow != nullptr);
        UpdateModeCursorClientPos(x, y);
        g_windowMgr.OnMouseMove(x, y);
        if (!uiCaptured && !g_windowMgr.HasWindowAtPoint(x, y)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_MouseMove, x, y);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        const int x = GetLParamX(lParam);
        const int y = GetLParamY(lParam);
        const int delta = GetWheelDeltaWParam(wParam);
        UpdateModeCursorClientPos(x, y);
        if (!g_windowMgr.OnWheel(x, y, delta)) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_MouseWheel, delta, 0);
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

    return 0;
}

void ExitApp()
{
    g_modeMgr.Quit();
}

void CheckSystemMessage()
{
    RoQtProcessEvents();
}

void SetWindowActiveMode(int active)
{
    g_isAppActive = active != 0;
}

bool GetWindowActiveMode()
{
    return g_isAppActive;
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
    return false;
}

bool UpdatePatch(const char*, const char*)
{
    return false;
}

void* ExcuteProgram(const char*)
{
    return nullptr;
}

bool SearchProcessIn9X()
{
    return false;
}

bool SearchProcessInNT()
{
    return false;
}

int main(int, char**)
{
    ApplyRuntimeRoot(ResolveRuntimeRoot());

    ResetTimer();
    CreateTrigonometricTable();
    ReadRegistry();

    if (!InitApp(nullptr, 1)) {
        return 1;
    }

    if (!CDllMgr::LoadAll()) {
        std::fputs(CDllMgr::GetLastLoadReport().c_str(), stderr);
    } else if (!CDllMgr::GetLastLoadReport().empty()) {
        DbgLog("%s", CDllMgr::GetLastLoadReport().c_str());
    }

    g_fileMgr.AddPak("data.grf");
    g_fileMgr.AddPak("data_hp.grf");
    g_fileMgr.AddPak("event.grf");
    g_fileMgr.AddPak("fdata.grf");

    if (!CConnection::Startup()) {
        return 1;
    }

    if (!InitClientSystems()) {
        CConnection::Cleanup();
        return 1;
    }

    InitTimer(60);
    RoQtFocusWindow(g_hMainWnd);
    g_modeMgr.Run(0, "");

    CConnection::Cleanup();
    g_windowMgr.Reset();
    g_buabridge.Shutdown();
    GetRenderDevice().Shutdown();
    ShutdownQtUiRuntime();
    CAudio::GetInstance()->Shutdown();
    // Qt owns the surface, so avoid calling GetRenderDevice().Shutdown();
    RoQtDestroyMainWindow(g_hMainWnd);
    g_hMainWnd = nullptr;
    return 0;
}