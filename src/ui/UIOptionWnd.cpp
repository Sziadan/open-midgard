#include "UIOptionWnd.h"

#include "audio/Audio.h"
#include "core/File.h"
#include "main/WinMain.h"
#include "ui/UIWindowMgr.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

constexpr char kRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";
constexpr char kOptionWndXValue[] = "OptionWndX";
constexpr char kOptionWndYValue[] = "OptionWndY";
constexpr char kOptionWndWValue[] = "OptionWndW";
constexpr char kOptionWndHValue[] = "OptionWndH";
constexpr char kOptionWndOrgHValue[] = "OptionWndOrgH";
constexpr char kOptionWndShowValue[] = "OptionWndShow";
constexpr char kOptionWndBgmVolumeValue[] = "OptionWndBgmVolume";
constexpr char kOptionWndSoundVolumeValue[] = "OptionWndSoundVolume";
constexpr char kOptionWndBgmOnValue[] = "OptionWndBgmOn";
constexpr char kOptionWndSoundOnValue[] = "OptionWndSoundOn";
constexpr char kOptionWndNoCtrlValue[] = "OptionWndNoCtrl";
constexpr char kOptionWndAttackSnapValue[] = "OptionWndAttackSnap";
constexpr char kOptionWndSkillSnapValue[] = "OptionWndSkillSnap";
constexpr char kOptionWndItemSnapValue[] = "OptionWndItemSnap";
constexpr char kOptionWndCollapsedValue[] = "OptionWndCollapsed";
constexpr char kOptionWndTabValue[] = "OptionWndTab";

constexpr int kDefaultWidth = 308;
constexpr int kDefaultHeight = 238;
constexpr int kCollapsedHeight = 17;
constexpr int kTitleBarHeight = 17;
constexpr int kDefaultX = 185;
constexpr int kDefaultY = 300;
constexpr int kSliderMin = 0;
constexpr int kSliderMax = 127;
constexpr int kTabHeight = 18;
constexpr int kGraphicsRowHeight = 24;
constexpr int kWindowCornerRadius = 10;

constexpr COLORREF kFallbackTitleBarColor = RGB(98, 114, 158);
constexpr COLORREF kFallbackBodyColor = RGB(244, 247, 252);
constexpr COLORREF kWindowBorderColor = RGB(57, 66, 86);
constexpr COLORREF kContentFillColor = RGB(255, 255, 255);
constexpr COLORREF kContentBorderColor = RGB(160, 171, 194);
constexpr COLORREF kInactiveTabFillColor = RGB(220, 228, 241);
constexpr COLORREF kInactiveTabBorderColor = RGB(122, 136, 167);
constexpr COLORREF kActiveTabFillColor = RGB(255, 255, 255);
constexpr COLORREF kActiveTabBorderColor = RGB(95, 112, 150);
constexpr COLORREF kHeaderButtonFillColor = RGB(248, 250, 255);
constexpr COLORREF kHeaderButtonBorderColor = RGB(96, 112, 150);
constexpr COLORREF kHeaderButtonTextColor = RGB(40, 55, 92);
constexpr COLORREF kSettingRowFillColor = RGB(247, 250, 255);
constexpr COLORREF kSettingRowBorderColor = RGB(176, 186, 208);
constexpr COLORREF kSliderTrackFillColor = RGB(182, 194, 219);
constexpr COLORREF kSliderTrackBorderColor = RGB(111, 125, 160);
constexpr COLORREF kSliderKnobFillColor = RGB(255, 255, 255);
constexpr COLORREF kSliderKnobBorderColor = RGB(87, 101, 136);

constexpr int kCheckIdBgm = 401;
constexpr int kCheckIdSound = 402;
constexpr int kCheckIdNoCtrl = 403;
constexpr int kCheckIdAttack = 404;
constexpr int kCheckIdSkill = 405;
constexpr int kCheckIdItem = 406;

constexpr std::array<RenderBackendType, 4> kRendererEntries = {
    RenderBackendType::LegacyDirect3D7,
    RenderBackendType::Direct3D11,
    RenderBackendType::Direct3D12,
    RenderBackendType::Vulkan,
};

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

HBITMAP LoadBitmapFromGameData(const char* path)
{
    if (!path || !*path || !EnsureGdiplusStarted()) {
        return nullptr;
    }

    int size = 0;
    unsigned char* bytes = g_fileMgr.GetData(path, &size);
    if (!bytes || size <= 0) {
        delete[] bytes;
        return nullptr;
    }

    HBITMAP outBmp = nullptr;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(size));
    if (mem) {
        void* dst = GlobalLock(mem);
        if (dst) {
            std::memcpy(dst, bytes, static_cast<size_t>(size));
            GlobalUnlock(mem);

            IStream* stream = nullptr;
            if (CreateStreamOnHGlobal(mem, TRUE, &stream) == S_OK) {
                auto* bmp = Gdiplus::Bitmap::FromStream(stream, FALSE);
                if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) {
                    bmp->GetHBITMAP(RGB(0, 0, 0), &outBmp);
                }
                delete bmp;
                stream->Release();
            } else {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }

    delete[] bytes;
    return outBmp;
}

void DrawBitmapStretched(HDC target, HBITMAP bmp, const RECT& dst)
{
    if (!target || !bmp) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(target);
    if (!srcDC) {
        return;
    }

    HGDIOBJ old = SelectObject(srcDC, bmp);
    SetStretchBltMode(target, HALFTONE);
    StretchBlt(target,
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

void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
{
    if (raw.empty()) {
        return;
    }

    const std::string normalized = NormalizeSlash(raw);
    const std::string lowered = ToLowerAscii(normalized);
    for (const std::string& existing : out) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out.push_back(normalized);
}

std::vector<std::string> BuildUiAssetCandidates(const char* fileName)
{
    std::vector<std::string> out;
    if (!fileName || !*fileName) {
        return out;
    }

    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* pathPrefixes[] = {
        "",
        "skin\\default\\",
        "skin\\default\\basic_interface\\",
        "skin\\default\\interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        kUiKor,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string filenameOnly = base;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (int i = 0; pathPrefixes[i]; ++i) {
        AddUniqueCandidate(out, std::string(pathPrefixes[i]) + filenameOnly);
    }

    return out;
}

std::string ResolveUiAssetPath(const std::vector<const char*>& names)
{
    for (const char* name : names) {
        if (!name || !*name) {
            continue;
        }

        const std::vector<std::string> candidates = BuildUiAssetCandidates(name);
        for (const std::string& candidate : candidates) {
            if (g_fileMgr.IsDataExist(candidate.c_str())) {
                return candidate;
            }
        }
    }

    return {};
}

bool PointInRectXY(const RECT& rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

void FillRectColor(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void FillRoundedRectColor(HDC hdc, const RECT& rc, COLORREF color, int radius)
{
    if (!hdc) {
        return;
    }

    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

void DrawRectFrame(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void DrawWindowTitleText(HDC hdc, int x, int y, int windowRight, const char* text, COLORREF color)
{
    if (!hdc) {
        return;
    }

    RECT rect = { x, y, windowRight - 4, y + 16 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text ? text : "", -1, &rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

int ClampSliderValue(int value)
{
    return (std::max)(kSliderMin, (std::min)(kSliderMax, value));
}

RenderBackendType NormalizeSelectableBackend(RenderBackendType backend)
{
    if (IsRenderBackendImplemented(backend) && IsRenderBackendSupported(backend)) {
        return backend;
    }

    for (RenderBackendType candidate : kRendererEntries) {
        if (IsRenderBackendImplemented(candidate) && IsRenderBackendSupported(candidate)) {
            return candidate;
        }
    }

    return RenderBackendType::Direct3D11;
}

const char* GetWindowModeName(WindowMode mode)
{
    switch (mode) {
    case WindowMode::Windowed:
        return "Windowed";
    case WindowMode::Fullscreen:
        return "Fullscreen";
    case WindowMode::BorderlessFullscreen:
        return "Borderless";
    default:
        return "Windowed";
    }
}

const char* GetAntiAliasingName(AntiAliasingMode mode)
{
    switch (mode) {
    case AntiAliasingMode::None:
        return "Off";
    case AntiAliasingMode::FXAA:
        return "FXAA";
    case AntiAliasingMode::SMAA:
        return "SMAA";
    default:
        return "Off";
    }
}

std::string FormatResolutionText(int width, int height)
{
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%dx%d", width, height);
    return std::string(buffer);
}

std::string FormatMultiplierText(int value)
{
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%dx", value);
    return std::string(buffer);
}

std::string FormatAnisotropicText(int level)
{
    if (level <= 1) {
        return "Off";
    }

    return FormatMultiplierText(level);
}

void LoadDwordSetting(HKEY key, const char* valueName, int* target)
{
    if (!key || !valueName || !target) {
        return;
    }

    DWORD value = static_cast<DWORD>(*target);
    DWORD size = sizeof(value);
    if (RegQueryValueExA(key, valueName, nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS) {
        *target = static_cast<int>(value);
    }
}

void SaveDwordSetting(HKEY key, const char* valueName, int value)
{
    if (!key || !valueName) {
        return;
    }

    const DWORD raw = static_cast<DWORD>(value);
    RegSetValueExA(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&raw), sizeof(raw));
}

void LoadSavedAudioSettingsFromRegistry(int* bgmVolume, int* soundVolume, int* bgmEnabled, int* soundEnabled)
{
    if (!bgmVolume || !soundVolume || !bgmEnabled || !soundEnabled) {
        return;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        LoadDwordSetting(key, kOptionWndBgmVolumeValue, bgmVolume);
        LoadDwordSetting(key, kOptionWndSoundVolumeValue, soundVolume);
        LoadDwordSetting(key, kOptionWndBgmOnValue, bgmEnabled);
        LoadDwordSetting(key, kOptionWndSoundOnValue, soundEnabled);
        RegCloseKey(key);
    }

    *bgmVolume = ClampSliderValue(*bgmVolume);
    *soundVolume = ClampSliderValue(*soundVolume);
    *bgmEnabled = (*bgmEnabled != 0) ? 1 : 0;
    *soundEnabled = (*soundEnabled != 0) ? 1 : 0;
}

} // namespace

void ApplySavedAudioSettings()
{
    int bgmVolume = 100;
    int soundVolume = 100;
    int bgmEnabled = 1;
    int soundEnabled = 1;
    LoadSavedAudioSettingsFromRegistry(&bgmVolume, &soundVolume, &bgmEnabled, &soundEnabled);

    CAudio* audio = CAudio::GetInstance();
    if (!audio) {
        return;
    }

    g_soundMode = soundEnabled ? 1 : 0;
    audio->SetVolume(static_cast<float>(soundVolume) / 127.0f);
    audio->SetBgmVolume(bgmVolume);
    audio->SetBgmPaused(bgmEnabled == 0);
}

UIOptionWnd::UIOptionWnd()
    : m_controlsCreated(false),
      m_assetsProbed(false),
      m_frameBitmap(nullptr),
      m_bodyBitmap(nullptr),
      m_bgmOnCheckBox(nullptr),
      m_soundOnCheckBox(nullptr),
      m_noCtrlCheckBox(nullptr),
      m_attackSnapCheckBox(nullptr),
      m_skillSnapCheckBox(nullptr),
      m_itemSnapCheckBox(nullptr),
      m_orgHeight(kDefaultHeight),
      m_bgmVolume(100),
      m_soundVolume(100),
      m_bgmEnabled(1),
      m_soundEnabled(1),
      m_noCtrl(0),
      m_attackSnap(0),
      m_skillSnap(0),
      m_itemSnap(0),
      m_collapsed(0),
      m_activeTab(TabId_Game),
      m_dragMode(DragMode_None),
      m_dragAnchorX(0),
      m_dragAnchorY(0),
      m_dragWindowStartX(0),
      m_dragWindowStartY(0),
      m_graphicsSettings(GetCachedGraphicsSettings()),
      m_appliedGraphicsSettings(GetCachedGraphicsSettings()),
      m_selectedRenderBackend(NormalizeSelectableBackend(GetConfiguredRenderBackend())),
      m_appliedRenderBackend(GetActiveRenderBackend())
{
    m_defCancelPushId = 135;
    Create(kDefaultWidth, kDefaultHeight);
    LoadSettings();
}

UIOptionWnd::~UIOptionWnd()
{
    ClearResources();
}

void UIOptionWnd::ClearResources()
{
    if (m_frameBitmap) {
        DeleteObject(m_frameBitmap);
        m_frameBitmap = nullptr;
    }
    if (m_bodyBitmap) {
        DeleteObject(m_bodyBitmap);
        m_bodyBitmap = nullptr;
    }
    m_frameBitmapPath.clear();
    m_bodyBitmapPath.clear();
}

void UIOptionWnd::EnsureResources()
{
    if (m_assetsProbed) {
        return;
    }

    m_assetsProbed = true;
    m_frameBitmapPath = ResolveUiAssetPath({
        "titlebar_fix.bmp",
        "win_option.bmp",
        "optionwin.bmp",
        "win_option2.bmp",
        "win_option_t.bmp",
    });
    if (!m_frameBitmapPath.empty()) {
        m_frameBitmap = LoadBitmapFromGameData(m_frameBitmapPath.c_str());
    }

    m_bodyBitmapPath = ResolveUiAssetPath({
        "itemwin_mid.bmp",
        "win_option_sub.bmp",
        "win_option_body.bmp",
        "option_sub.bmp",
    });
    if (!m_bodyBitmapPath.empty()) {
        m_bodyBitmap = LoadBitmapFromGameData(m_bodyBitmapPath.c_str());
    }
}

void UIOptionWnd::RefreshResolutionEntries()
{
    m_resolutionEntries.clear();

    DEVMODEA displayMode{};
    displayMode.dmSize = sizeof(displayMode);
    for (DWORD modeIndex = 0; EnumDisplaySettingsA(nullptr, modeIndex, &displayMode); ++modeIndex) {
        if (displayMode.dmPelsWidth < 640 || displayMode.dmPelsHeight < 480) {
            continue;
        }

        bool exists = false;
        for (const ResolutionEntry& entry : m_resolutionEntries) {
            if (entry.width == static_cast<int>(displayMode.dmPelsWidth)
                && entry.height == static_cast<int>(displayMode.dmPelsHeight)) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_resolutionEntries.push_back({ static_cast<int>(displayMode.dmPelsWidth), static_cast<int>(displayMode.dmPelsHeight) });
        }
    }

    if (FindResolutionIndex(m_graphicsSettings.width, m_graphicsSettings.height) < 0) {
        m_resolutionEntries.push_back({ m_graphicsSettings.width, m_graphicsSettings.height });
    }

    std::sort(m_resolutionEntries.begin(), m_resolutionEntries.end(), [](const ResolutionEntry& lhs, const ResolutionEntry& rhs) {
        if (lhs.width != rhs.width) {
            return lhs.width < rhs.width;
        }
        return lhs.height < rhs.height;
    });
}

int UIOptionWnd::FindResolutionIndex(int width, int height) const
{
    for (size_t index = 0; index < m_resolutionEntries.size(); ++index) {
        if (m_resolutionEntries[index].width == width && m_resolutionEntries[index].height == height) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::vector<UIOptionWnd::GraphicsRowId> UIOptionWnd::GetVisibleGraphicsRows() const
{
    std::vector<GraphicsRowId> rows;
    const RenderBackendType backend = m_selectedRenderBackend;
    if (DoesBackendSupportResolutionSelection(backend)) {
        rows.push_back(GraphicsRow_Resolution);
    }
    rows.push_back(GraphicsRow_Renderer);
    if (!GetSupportedWindowModes().empty()) {
        rows.push_back(GraphicsRow_WindowMode);
    }
    if (!GetSupportedAntiAliasingModes().empty()) {
        rows.push_back(GraphicsRow_AntiAliasing);
    }
    if (DoesBackendSupportTextureUpscaling(backend)) {
        rows.push_back(GraphicsRow_TextureUpscale);
    }
    if (!GetSupportedAnisotropicLevels().empty()) {
        rows.push_back(GraphicsRow_AnisotropicFiltering);
    }
    return rows;
}

std::vector<RenderBackendType> UIOptionWnd::GetSupportedRenderBackends() const
{
    std::vector<RenderBackendType> backends;
    for (RenderBackendType backend : kRendererEntries) {
        if (IsRenderBackendImplemented(backend) && IsRenderBackendSupported(backend)) {
            backends.push_back(backend);
        }
    }
    if (backends.empty()) {
        backends.push_back(RenderBackendType::Direct3D11);
    }
    return backends;
}

std::vector<WindowMode> UIOptionWnd::GetSupportedWindowModes() const
{
    std::vector<WindowMode> modes;
    const WindowMode orderedModes[] = {
        WindowMode::Windowed,
        WindowMode::BorderlessFullscreen,
        WindowMode::Fullscreen,
    };
    for (WindowMode mode : orderedModes) {
        if (DoesBackendSupportWindowMode(m_selectedRenderBackend, mode)) {
            modes.push_back(mode);
        }
    }
    return modes;
}

std::vector<int> UIOptionWnd::GetSupportedAnisotropicLevels() const
{
    if (!DoesBackendSupportAnisotropicFiltering(m_selectedRenderBackend)) {
        return {};
    }
    return { 1, 2, 4, 8, 16 };
}

std::vector<AntiAliasingMode> UIOptionWnd::GetSupportedAntiAliasingModes() const
{
    return GetSupportedAntiAliasingModesForBackend(m_selectedRenderBackend);
}

RenderBackendType UIOptionWnd::NormalizeSelectedBackend(RenderBackendType backend) const
{
    return NormalizeSelectableBackend(backend);
}

void UIOptionWnd::LoadSettings()
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        LoadDwordSetting(key, kOptionWndXValue, &m_x);
        LoadDwordSetting(key, kOptionWndYValue, &m_y);
        LoadDwordSetting(key, kOptionWndWValue, &m_w);
        LoadDwordSetting(key, kOptionWndHValue, &m_h);
        LoadDwordSetting(key, kOptionWndOrgHValue, &m_orgHeight);
        LoadDwordSetting(key, kOptionWndShowValue, &m_show);
        LoadDwordSetting(key, kOptionWndNoCtrlValue, &m_noCtrl);
        LoadDwordSetting(key, kOptionWndAttackSnapValue, &m_attackSnap);
        LoadDwordSetting(key, kOptionWndSkillSnapValue, &m_skillSnap);
        LoadDwordSetting(key, kOptionWndItemSnapValue, &m_itemSnap);
        LoadDwordSetting(key, kOptionWndCollapsedValue, &m_collapsed);
        LoadDwordSetting(key, kOptionWndTabValue, &m_activeTab);
        RegCloseKey(key);
    }

    LoadSavedAudioSettingsFromRegistry(&m_bgmVolume, &m_soundVolume, &m_bgmEnabled, &m_soundEnabled);
    m_graphicsSettings = GetCachedGraphicsSettings();
    m_appliedGraphicsSettings = m_graphicsSettings;
    m_selectedRenderBackend = NormalizeSelectedBackend(GetConfiguredRenderBackend());
    m_appliedRenderBackend = GetActiveRenderBackend();
    RefreshResolutionEntries();

    if (m_w <= 0) {
        m_w = kDefaultWidth;
    }
    if (m_orgHeight < kDefaultHeight) {
        m_orgHeight = kDefaultHeight;
    }
    if (m_h <= 0) {
        m_h = m_collapsed ? kCollapsedHeight : m_orgHeight;
    } else if (!m_collapsed && m_h < kDefaultHeight) {
        m_h = kDefaultHeight;
    }
    if (m_x == 0 && m_y == 0) {
        if (g_hMainWnd) {
            RECT clientRect{};
            GetClientRect(g_hMainWnd, &clientRect);
            const int clientW = clientRect.right - clientRect.left;
            const int clientH = clientRect.bottom - clientRect.top;
            m_x = (kDefaultX * clientW) / 640;
            m_y = (kDefaultY * clientH) / 480;
        } else {
            m_x = kDefaultX;
            m_y = kDefaultY;
        }
    }

    m_noCtrl = (m_noCtrl != 0) ? 1 : 0;
    m_attackSnap = (m_attackSnap != 0) ? 1 : 0;
    m_skillSnap = (m_skillSnap != 0) ? 1 : 0;
    m_itemSnap = (m_itemSnap != 0) ? 1 : 0;
    m_collapsed = (m_collapsed != 0) ? 1 : 0;
    if (m_activeTab < 0 || m_activeTab >= TabId_Count) {
        m_activeTab = TabId_Game;
    }
    if (m_collapsed) {
        m_h = kCollapsedHeight;
    }
}

void UIOptionWnd::SaveGraphicsPreferences() const
{
    GraphicsSettings settings = m_graphicsSettings;
    ClampGraphicsSettingsToBackend(m_selectedRenderBackend, &settings);
    SaveGraphicsSettings(settings);
    SetConfiguredRenderBackend(m_selectedRenderBackend);
}

void UIOptionWnd::SaveSettings() const
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    SaveDwordSetting(key, kOptionWndXValue, m_x);
    SaveDwordSetting(key, kOptionWndYValue, m_y);
    SaveDwordSetting(key, kOptionWndWValue, m_w);
    SaveDwordSetting(key, kOptionWndHValue, m_h);
    SaveDwordSetting(key, kOptionWndOrgHValue, m_orgHeight);
    SaveDwordSetting(key, kOptionWndShowValue, m_show != 0 ? 1 : 0);
    SaveDwordSetting(key, kOptionWndBgmVolumeValue, m_bgmVolume);
    SaveDwordSetting(key, kOptionWndSoundVolumeValue, m_soundVolume);
    SaveDwordSetting(key, kOptionWndBgmOnValue, m_bgmEnabled);
    SaveDwordSetting(key, kOptionWndSoundOnValue, m_soundEnabled);
    SaveDwordSetting(key, kOptionWndNoCtrlValue, m_noCtrl);
    SaveDwordSetting(key, kOptionWndAttackSnapValue, m_attackSnap);
    SaveDwordSetting(key, kOptionWndSkillSnapValue, m_skillSnap);
    SaveDwordSetting(key, kOptionWndItemSnapValue, m_itemSnap);
    SaveDwordSetting(key, kOptionWndCollapsedValue, m_collapsed != 0 ? 1 : 0);
    SaveDwordSetting(key, kOptionWndTabValue, m_activeTab);
    RegCloseKey(key);

    SaveGraphicsPreferences();
}

void UIOptionWnd::ApplyAudioSettings() const
{
    CAudio* audio = CAudio::GetInstance();
    if (!audio) {
        return;
    }

    g_soundMode = m_soundEnabled ? 1 : 0;
    audio->SetVolume(static_cast<float>(m_soundVolume) / 127.0f);
    audio->SetBgmVolume(m_bgmVolume);
    audio->SetBgmPaused(m_bgmEnabled == 0);
}

void UIOptionWnd::LayoutControls()
{
    const bool showAudio = !m_collapsed && m_activeTab == TabId_Audio;
    const bool showGame = !m_collapsed && m_activeTab == TabId_Game;
    const RECT contentRect = GetContentRect();

    if (m_bgmOnCheckBox) {
        const RECT sliderRect = GetBgmSliderRect();
        m_bgmOnCheckBox->Move(sliderRect.right + 10, sliderRect.top - 2);
        m_bgmOnCheckBox->SetShow(showAudio ? 1 : 0);
        m_bgmOnCheckBox->SetCheck(m_bgmEnabled == 0);
    }
    if (m_soundOnCheckBox) {
        const RECT sliderRect = GetSoundSliderRect();
        m_soundOnCheckBox->Move(sliderRect.right + 10, sliderRect.top - 2);
        m_soundOnCheckBox->SetShow(showAudio ? 1 : 0);
        m_soundOnCheckBox->SetCheck(m_soundEnabled == 0);
    }
    if (m_noCtrlCheckBox) {
        m_noCtrlCheckBox->Move(contentRect.left + 16, contentRect.top + 16);
        m_noCtrlCheckBox->SetShow(showGame ? 1 : 0);
        m_noCtrlCheckBox->SetCheck(m_noCtrl);
    }
    if (m_attackSnapCheckBox) {
        m_attackSnapCheckBox->Move(contentRect.left + 16, contentRect.top + 42);
        m_attackSnapCheckBox->SetShow(showGame ? 1 : 0);
        m_attackSnapCheckBox->SetCheck(m_attackSnap);
    }
    if (m_skillSnapCheckBox) {
        m_skillSnapCheckBox->Move(contentRect.left + 16, contentRect.top + 68);
        m_skillSnapCheckBox->SetShow(showGame ? 1 : 0);
        m_skillSnapCheckBox->SetCheck(m_skillSnap);
    }
    if (m_itemSnapCheckBox) {
        m_itemSnapCheckBox->Move(contentRect.left + 16, contentRect.top + 94);
        m_itemSnapCheckBox->SetShow(showGame ? 1 : 0);
        m_itemSnapCheckBox->SetCheck(m_itemSnap);
    }
}

void UIOptionWnd::SetCollapsed(bool collapsed)
{
    m_collapsed = collapsed ? 1 : 0;
    m_h = m_collapsed ? kCollapsedHeight : m_orgHeight;
    LayoutControls();
    Invalidate();
    SaveSettings();
}

void UIOptionWnd::ResetToDefaultPlacement()
{
    if (g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        const int clientW = clientRect.right - clientRect.left;
        const int clientH = clientRect.bottom - clientRect.top;
        Move((kDefaultX * clientW) / 640, (kDefaultY * clientH) / 480);
    } else {
        Move(kDefaultX, kDefaultY);
    }

    Resize(kDefaultWidth, m_collapsed ? kCollapsedHeight : kDefaultHeight);
    m_orgHeight = kDefaultHeight;
    LayoutControls();
    Invalidate();
    SaveSettings();
}

int UIOptionWnd::SliderValueFromMouseX(int mouseX) const
{
    const RECT sliderRect = (m_dragMode == DragMode_SoundSlider) ? GetSoundSliderRect() : GetBgmSliderRect();
    const int trackStart = sliderRect.left + 4;
    const int sliderWidth = static_cast<int>(sliderRect.right - sliderRect.left);
    const int trackWidth = (std::max)(1, sliderWidth - 8);
    const int relative = (std::max)(0, (std::min)(trackWidth, mouseX - trackStart));
    return ClampSliderValue((relative * kSliderMax + trackWidth / 2) / trackWidth);
}

RECT UIOptionWnd::GetTitleBarRect() const
{
    RECT rc = { m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
    return rc;
}

RECT UIOptionWnd::GetBodyRect() const
{
    RECT rc = { m_x, m_y + kTitleBarHeight, m_x + m_w, m_y + m_h };
    return rc;
}

RECT UIOptionWnd::GetBaseButtonRect() const
{
    RECT rc = { m_x + 3, m_y + 3, m_x + 14, m_y + 14 };
    return rc;
}

RECT UIOptionWnd::GetMiniButtonRect() const
{
    RECT rc = { m_x + m_w - 28, m_y + 3, m_x + m_w - 17, m_y + 14 };
    return rc;
}

RECT UIOptionWnd::GetCloseButtonRect() const
{
    RECT rc = { m_x + m_w - 14, m_y + 3, m_x + m_w - 3, m_y + 14 };
    return rc;
}

RECT UIOptionWnd::GetTabRect(int tabIndex) const
{
    RECT bodyRect = GetBodyRect();
    const int margin = 10;
    const int tabWidth = (bodyRect.right - bodyRect.left - margin * 2) / TabId_Count;
    RECT rc = {
        bodyRect.left + margin + tabIndex * tabWidth,
        bodyRect.top + 6,
        bodyRect.left + margin + (tabIndex + 1) * tabWidth - 2,
        bodyRect.top + 6 + kTabHeight + 5,
    };
    return rc;
}

RECT UIOptionWnd::GetContentRect() const
{
    RECT bodyRect = GetBodyRect();
    RECT rc = {
        bodyRect.left + 10,
        bodyRect.top + 6 + kTabHeight + 4,
        bodyRect.right - 10,
        bodyRect.bottom - 12,
    };
    return rc;
}

RECT UIOptionWnd::GetRowRect(int rowIndex) const
{
    RECT contentRect = GetContentRect();
    RECT rc = {
        contentRect.left + 4,
        contentRect.top + 6 + rowIndex * kGraphicsRowHeight,
        contentRect.right - 4,
        contentRect.top + 6 + (rowIndex + 1) * kGraphicsRowHeight - 4,
    };
    return rc;
}

RECT UIOptionWnd::GetRowPrevButtonRect(int rowIndex) const
{
    RECT rowRect = GetRowRect(rowIndex);
    RECT rc = { rowRect.right - 52, rowRect.top + 3, rowRect.right - 34, rowRect.bottom - 3 };
    return rc;
}

RECT UIOptionWnd::GetRowNextButtonRect(int rowIndex) const
{
    RECT rowRect = GetRowRect(rowIndex);
    RECT rc = { rowRect.right - 28, rowRect.top + 3, rowRect.right - 10, rowRect.bottom - 3 };
    return rc;
}

RECT UIOptionWnd::GetRestartButtonRect() const
{
    RECT contentRect = GetContentRect();
    RECT rc = {
        contentRect.right - 92,
        contentRect.bottom - 22,
        contentRect.right - 8,
        contentRect.bottom - 4,
    };
    return rc;
}

RECT UIOptionWnd::GetBgmSliderRect() const
{
    RECT contentRect = GetContentRect();
    RECT rc = { contentRect.left + 54, contentRect.top + 22, contentRect.right - 72, contentRect.top + 36 };
    return rc;
}

RECT UIOptionWnd::GetSoundSliderRect() const
{
    RECT contentRect = GetContentRect();
    RECT rc = { contentRect.left + 54, contentRect.top + 58, contentRect.right - 72, contentRect.top + 72 };
    return rc;
}

RECT UIOptionWnd::GetSliderKnobRect(const RECT& sliderRect, int value) const
{
    const int trackStart = sliderRect.left + 4;
    const int sliderWidth = static_cast<int>(sliderRect.right - sliderRect.left);
    const int trackWidth = (std::max)(1, sliderWidth - 8);
    const int knobCenter = trackStart + (trackWidth * ClampSliderValue(value)) / kSliderMax;
    RECT rc = { knobCenter - 4, sliderRect.top - 2, knobCenter + 4, sliderRect.bottom + 2 };
    return rc;
}

void UIOptionWnd::DrawSlider(HDC hdc, const RECT& sliderRect, int value, const char* label) const
{
    RECT track = sliderRect;
    track.top += 4;
    track.bottom -= 4;
    FillRectColor(hdc, track, kSliderTrackFillColor);
    DrawRectFrame(hdc, track, kSliderTrackBorderColor);

    RECT knob = GetSliderKnobRect(sliderRect, value);
    FillRoundedRectColor(hdc, knob, kSliderKnobFillColor, 6);
    DrawRectFrame(hdc, knob, kSliderKnobBorderColor);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutA(hdc, sliderRect.left - 44, sliderRect.top - 1, label, static_cast<int>(std::strlen(label)));
}

void UIOptionWnd::DrawHeaderButton(HDC hdc, const RECT& rect, const char* text) const
{
    FillRoundedRectColor(hdc, rect, kHeaderButtonFillColor, 6);
    DrawRectFrame(hdc, rect, kHeaderButtonBorderColor);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kHeaderButtonTextColor);
    TextOutA(hdc, rect.left + 3, rect.top - 1, text, static_cast<int>(std::strlen(text)));
}

void UIOptionWnd::DrawTabButton(HDC hdc, const RECT& rect, const char* text, bool active) const
{
    FillRoundedRectColor(hdc, rect, active ? kActiveTabFillColor : kInactiveTabFillColor, 8);
    DrawRectFrame(hdc, rect, active ? kActiveTabBorderColor : kInactiveTabBorderColor);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutA(hdc, rect.left + 10, rect.top + 3, text, static_cast<int>(std::strlen(text)));
}

void UIOptionWnd::DrawSettingRow(HDC hdc, int rowIndex, const char* label, const std::string& value) const
{
    const RECT rowRect = GetRowRect(rowIndex);
    FillRoundedRectColor(hdc, rowRect, kSettingRowFillColor, 8);
    DrawRectFrame(hdc, rowRect, kSettingRowBorderColor);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutA(hdc, rowRect.left + 8, rowRect.top + 4, label, static_cast<int>(std::strlen(label)));

    SIZE valueSize{};
    GetTextExtentPoint32A(hdc, value.c_str(), static_cast<int>(value.size()), &valueSize);
    const int valueX = rowRect.right - 60 - valueSize.cx;
    TextOutA(hdc, valueX, rowRect.top + 4, value.c_str(), static_cast<int>(value.size()));

    DrawHeaderButton(hdc, GetRowPrevButtonRect(rowIndex), "<");
    DrawHeaderButton(hdc, GetRowNextButtonRect(rowIndex), ">");
}

bool UIOptionWnd::HasPendingGraphicsRestart() const
{
    return m_selectedRenderBackend != m_appliedRenderBackend
        || GraphicsSettingsRequireRestart(m_appliedGraphicsSettings, m_graphicsSettings);
}

void UIOptionWnd::PromptForGraphicsRestart()
{
    // Persist pending graphics changes before prompting/relaunching so the
    // restarted client actually boots with the selected resolution/backend.
    SaveSettings();

    const int restartNow = MessageBoxA(
        g_hMainWnd,
        "Graphics changes require a client restart. If you are currently in-game, the client will disconnect from the map server before it relaunches. Restart now?",
        "Restart Required",
        MB_ICONQUESTION | MB_YESNO);
    if (restartNow == IDYES) {
        if (!RelaunchCurrentApplication()) {
            MessageBoxA(
                g_hMainWnd,
                "Failed to relaunch the client. The graphics settings were saved and will apply the next time you start the game.",
                "Restart Failed",
                MB_ICONERROR | MB_OK);
        }
    }
}

void UIOptionWnd::CycleGraphicsSetting(GraphicsRowId rowId, int direction)
{
    if (direction == 0) {
        return;
    }

    if (rowId == GraphicsRow_Resolution) {
        if (m_resolutionEntries.empty()) {
            RefreshResolutionEntries();
        }
        if (!m_resolutionEntries.empty()) {
            int index = FindResolutionIndex(m_graphicsSettings.width, m_graphicsSettings.height);
            if (index < 0) {
                index = 0;
            }
            index += direction > 0 ? 1 : -1;
            if (index < 0) {
                index = static_cast<int>(m_resolutionEntries.size()) - 1;
            } else if (index >= static_cast<int>(m_resolutionEntries.size())) {
                index = 0;
            }
            m_graphicsSettings.width = m_resolutionEntries[static_cast<size_t>(index)].width;
            m_graphicsSettings.height = m_resolutionEntries[static_cast<size_t>(index)].height;
        }
    } else if (rowId == GraphicsRow_Renderer) {
        const std::vector<RenderBackendType> backends = GetSupportedRenderBackends();
        for (size_t index = 0; index < backends.size(); ++index) {
            if (backends[index] == m_selectedRenderBackend) {
                int nextIndex = static_cast<int>(index) + (direction > 0 ? 1 : -1);
                if (nextIndex < 0) {
                    nextIndex = static_cast<int>(backends.size()) - 1;
                } else if (nextIndex >= static_cast<int>(backends.size())) {
                    nextIndex = 0;
                }
                m_selectedRenderBackend = backends[static_cast<size_t>(nextIndex)];
                ClampGraphicsSettingsToBackend(m_selectedRenderBackend, &m_graphicsSettings);
                break;
            }
        }
    } else if (rowId == GraphicsRow_WindowMode) {
        const std::vector<WindowMode> modes = GetSupportedWindowModes();
        for (size_t index = 0; index < modes.size(); ++index) {
            if (modes[index] == m_graphicsSettings.windowMode) {
                int nextIndex = static_cast<int>(index) + (direction > 0 ? 1 : -1);
                if (nextIndex < 0) {
                    nextIndex = static_cast<int>(modes.size()) - 1;
                } else if (nextIndex >= static_cast<int>(modes.size())) {
                    nextIndex = 0;
                }
                m_graphicsSettings.windowMode = modes[static_cast<size_t>(nextIndex)];
                break;
            }
        }
    } else if (rowId == GraphicsRow_AntiAliasing) {
        const std::vector<AntiAliasingMode> modes = GetSupportedAntiAliasingModes();
        for (size_t index = 0; index < modes.size(); ++index) {
            if (modes[index] == m_graphicsSettings.antiAliasing) {
                int nextIndex = static_cast<int>(index) + (direction > 0 ? 1 : -1);
                if (nextIndex < 0) {
                    nextIndex = static_cast<int>(modes.size()) - 1;
                } else if (nextIndex >= static_cast<int>(modes.size())) {
                    nextIndex = 0;
                }
                m_graphicsSettings.antiAliasing = modes[static_cast<size_t>(nextIndex)];
                break;
            }
        }
    } else if (rowId == GraphicsRow_TextureUpscale) {
        int nextValue = m_graphicsSettings.textureUpscaleFactor + (direction > 0 ? 1 : -1);
        if (nextValue < 1) {
            nextValue = 4;
        } else if (nextValue > 4) {
            nextValue = 1;
        }
        m_graphicsSettings.textureUpscaleFactor = nextValue;
    } else if (rowId == GraphicsRow_AnisotropicFiltering) {
        const std::vector<int> levels = GetSupportedAnisotropicLevels();
        for (size_t index = 0; index < levels.size(); ++index) {
            if (levels[index] == m_graphicsSettings.anisotropicLevel) {
                int nextIndex = static_cast<int>(index) + (direction > 0 ? 1 : -1);
                if (nextIndex < 0) {
                    nextIndex = static_cast<int>(levels.size()) - 1;
                } else if (nextIndex >= static_cast<int>(levels.size())) {
                    nextIndex = 0;
                }
                m_graphicsSettings.anisotropicLevel = levels[static_cast<size_t>(nextIndex)];
                break;
            }
        }
    }

    Invalidate();
    SaveSettings();
}

std::string UIOptionWnd::GetGraphicsRowValue(GraphicsRowId rowId) const
{
    switch (rowId) {
    case GraphicsRow_Resolution:
        return FormatResolutionText(m_graphicsSettings.width, m_graphicsSettings.height);
    case GraphicsRow_Renderer:
        return std::string(GetRenderBackendName(m_selectedRenderBackend));
    case GraphicsRow_WindowMode:
        return std::string(GetWindowModeName(m_graphicsSettings.windowMode));
    case GraphicsRow_AntiAliasing:
        return std::string(GetAntiAliasingName(m_graphicsSettings.antiAliasing));
    case GraphicsRow_TextureUpscale:
        return FormatMultiplierText(m_graphicsSettings.textureUpscaleFactor);
    case GraphicsRow_AnisotropicFiltering:
        return FormatAnisotropicText(m_graphicsSettings.anisotropicLevel);
    default:
        return std::string();
    }
}

void UIOptionWnd::OnCreate(int cx, int cy)
{
    if (m_controlsCreated) {
        return;
    }
    m_controlsCreated = true;

    if (m_x == 0 && m_y == 0) {
        Move((kDefaultX * cx) / 640, (kDefaultY * cy) / 480);
    }

    auto makeCheckBox = [this](int id, int checked) {
        auto* checkBox = new UICheckBox();
        checkBox->SetBitmap(
            ResolveUiAssetPath({ "chk_saveon.bmp", "checkon.bmp", "chk_on.bmp" }).c_str(),
            ResolveUiAssetPath({ "chk_saveoff.bmp", "checkoff.bmp", "chk_off.bmp" }).c_str());
        checkBox->Create(checkBox->m_w > 0 ? checkBox->m_w : 16, checkBox->m_h > 0 ? checkBox->m_h : 16);
        checkBox->m_id = id;
        checkBox->SetCheck(checked);
        AddChild(checkBox);
        return checkBox;
    };

    m_bgmOnCheckBox = makeCheckBox(kCheckIdBgm, m_bgmEnabled == 0);
    m_soundOnCheckBox = makeCheckBox(kCheckIdSound, m_soundEnabled == 0);
    m_noCtrlCheckBox = makeCheckBox(kCheckIdNoCtrl, m_noCtrl);
    m_attackSnapCheckBox = makeCheckBox(kCheckIdAttack, m_attackSnap);
    m_skillSnapCheckBox = makeCheckBox(kCheckIdSkill, m_skillSnap);
    m_itemSnapCheckBox = makeCheckBox(kCheckIdItem, m_itemSnap);

    LayoutControls();
    ApplyAudioSettings();
}

void UIOptionWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    EnsureResources();

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC hdc = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientW = clientRect.right - clientRect.left;
    const int clientH = clientRect.bottom - clientRect.top;
    if (!m_controlsCreated && clientW > 0 && clientH > 0) {
        OnCreate(clientW, clientH);
    }

    const RECT titleRect = GetTitleBarRect();
    const RECT bodyRect = GetBodyRect();
    RECT windowRect = { m_x, m_y, m_x + m_w, m_y + m_h };
    HRGN clipRegion = CreateRoundRectRgn(windowRect.left, windowRect.top, windowRect.right + 1, windowRect.bottom + 1,
        kWindowCornerRadius, kWindowCornerRadius);
    int savedDc = SaveDC(hdc);
    if (clipRegion) {
        SelectClipRgn(hdc, clipRegion);
    }

    if (m_frameBitmap) {
        DrawBitmapStretched(hdc, m_frameBitmap, titleRect);
    } else {
        FillRectColor(hdc, titleRect, kFallbackTitleBarColor);
    }

    if (!m_collapsed) {
        if (m_bodyBitmap) {
            DrawBitmapStretched(hdc, m_bodyBitmap, bodyRect);
        } else {
            FillRectColor(hdc, bodyRect, kFallbackBodyColor);
        }
    }

    RestoreDC(hdc, savedDc);
    if (clipRegion) {
        DeleteObject(clipRegion);
    }

    DrawWindowTitleText(hdc, m_x + 18, m_y + 3, m_x + m_w, "Options", RGB(255, 255, 255));
    DrawWindowTitleText(hdc, m_x + 17, m_y + 2, m_x + m_w, "Options", RGB(0, 0, 0));

    DrawHeaderButton(hdc, GetMiniButtonRect(), "_");
    DrawHeaderButton(hdc, GetCloseButtonRect(), "X");

    if (!m_collapsed) {
        const RECT contentRect = GetContentRect();
        FillRectColor(hdc, contentRect, kContentFillColor);
        DrawRectFrame(hdc, contentRect, kContentBorderColor);

        DrawTabButton(hdc, GetTabRect(TabId_Game), "Game", m_activeTab == TabId_Game);
        DrawTabButton(hdc, GetTabRect(TabId_Graphics), "Graphics", m_activeTab == TabId_Graphics);
        DrawTabButton(hdc, GetTabRect(TabId_Audio), "Audio", m_activeTab == TabId_Audio);

        if (m_activeTab == TabId_Audio) {
            DrawSlider(hdc, GetBgmSliderRect(), m_bgmVolume, "BGM");
            DrawSlider(hdc, GetSoundSliderRect(), m_soundVolume, "Sound");
            if (m_bgmOnCheckBox) {
                TextOutA(hdc, m_bgmOnCheckBox->m_x + 18, m_bgmOnCheckBox->m_y - 1, "Mute", 4);
            }
            if (m_soundOnCheckBox) {
                TextOutA(hdc, m_soundOnCheckBox->m_x + 18, m_soundOnCheckBox->m_y - 1, "Mute", 4);
            }
        } else if (m_activeTab == TabId_Game) {
            if (m_noCtrlCheckBox) {
                TextOutA(hdc, m_noCtrlCheckBox->m_x + 18, m_noCtrlCheckBox->m_y - 1, "Disable Ctrl+Click movement", 26);
            }
            if (m_attackSnapCheckBox) {
                TextOutA(hdc, m_attackSnapCheckBox->m_x + 18, m_attackSnapCheckBox->m_y - 1, "Attack target snap", 18);
            }
            if (m_skillSnapCheckBox) {
                TextOutA(hdc, m_skillSnapCheckBox->m_x + 18, m_skillSnapCheckBox->m_y - 1, "Skill target snap", 17);
            }
            if (m_itemSnapCheckBox) {
                TextOutA(hdc, m_itemSnapCheckBox->m_x + 18, m_itemSnapCheckBox->m_y - 1, "Item target snap", 16);
            }
        } else if (m_activeTab == TabId_Graphics) {
            const std::vector<GraphicsRowId> rows = GetVisibleGraphicsRows();
            for (size_t index = 0; index < rows.size(); ++index) {
                const char* label = "";
                switch (rows[index]) {
                case GraphicsRow_Resolution:
                    label = "Resolution";
                    break;
                case GraphicsRow_Renderer:
                    label = "Renderer";
                    break;
                case GraphicsRow_WindowMode:
                    label = "Window mode";
                    break;
                case GraphicsRow_AntiAliasing:
                    label = "3D AA";
                    break;
                case GraphicsRow_TextureUpscale:
                    label = "Texture upscale";
                    break;
                case GraphicsRow_AnisotropicFiltering:
                    label = "Anisotropic";
                    break;
                }
                DrawSettingRow(hdc, static_cast<int>(index), label, GetGraphicsRowValue(rows[index]));
            }
        }

        if (HasPendingGraphicsRestart()) {
            const RECT restartRect = GetRestartButtonRect();
            DrawHeaderButton(hdc, restartRect, "Restart");
        }
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, kWindowBorderColor);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, windowRect.left, windowRect.top, windowRect.right, windowRect.bottom,
        kWindowCornerRadius, kWindowCornerRadius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    DrawChildren();

    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }
}

void UIOptionWnd::OnLBtnDown(int x, int y)
{
    if (m_show == 0) {
        return;
    }

    if (PointInRectXY(GetCloseButtonRect(), x, y)) {
        SetShow(0);
        SaveSettings();
        return;
    }

    if (PointInRectXY(GetMiniButtonRect(), x, y)) {
        SetCollapsed(!m_collapsed);
        return;
    }

    if (!m_collapsed) {
        for (int tabIndex = 0; tabIndex < TabId_Count; ++tabIndex) {
            if (PointInRectXY(GetTabRect(tabIndex), x, y)) {
                m_activeTab = tabIndex;
                LayoutControls();
                Invalidate();
                SaveSettings();
                return;
            }
        }

        if (HasPendingGraphicsRestart() && PointInRectXY(GetRestartButtonRect(), x, y)) {
            PromptForGraphicsRestart();
            return;
        }

        if (m_activeTab == TabId_Graphics) {
            const std::vector<GraphicsRowId> rows = GetVisibleGraphicsRows();
            for (size_t index = 0; index < rows.size(); ++index) {
                const int rowIndex = static_cast<int>(index);
                if (PointInRectXY(GetRowPrevButtonRect(rowIndex), x, y)) {
                    CycleGraphicsSetting(rows[index], -1);
                    return;
                }
                if (PointInRectXY(GetRowNextButtonRect(rowIndex), x, y) || PointInRectXY(GetRowRect(rowIndex), x, y)) {
                    CycleGraphicsSetting(rows[index], 1);
                    return;
                }
            }
        }

        if (m_activeTab == TabId_Audio && PointInRectXY(GetBgmSliderRect(), x, y)) {
            m_dragMode = DragMode_BgmSlider;
            const int nextVolume = SliderValueFromMouseX(x);
            if (m_bgmVolume != nextVolume) {
                m_bgmVolume = nextVolume;
                Invalidate();
                ApplyAudioSettings();
            }
            return;
        }

        if (m_activeTab == TabId_Audio && PointInRectXY(GetSoundSliderRect(), x, y)) {
            m_dragMode = DragMode_SoundSlider;
            const int nextVolume = SliderValueFromMouseX(x);
            if (m_soundVolume != nextVolume) {
                m_soundVolume = nextVolume;
                Invalidate();
                ApplyAudioSettings();
            }
            return;
        }
    }

    if (y < m_y + kTitleBarHeight) {
        m_dragMode = DragMode_Window;
        m_dragAnchorX = x;
        m_dragAnchorY = y;
        m_dragWindowStartX = m_x;
        m_dragWindowStartY = m_y;
    }
}

void UIOptionWnd::OnMouseMove(int x, int y)
{
    switch (m_dragMode) {
    case DragMode_Window:
    {
        int snappedX = m_dragWindowStartX + (x - m_dragAnchorX);
        int snappedY = m_dragWindowStartY + (y - m_dragAnchorY);
        g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
        Move(snappedX, snappedY);
        LayoutControls();
        break;
    }

    case DragMode_BgmSlider:
    {
        const int nextVolume = SliderValueFromMouseX(x);
        if (m_bgmVolume != nextVolume) {
            m_bgmVolume = nextVolume;
            Invalidate();
            ApplyAudioSettings();
        }
        break;
    }

    case DragMode_SoundSlider:
    {
        const int nextVolume = SliderValueFromMouseX(x);
        if (m_soundVolume != nextVolume) {
            m_soundVolume = nextVolume;
            Invalidate();
            ApplyAudioSettings();
        }
        break;
    }

    default:
        break;
    }
}

void UIOptionWnd::OnLBtnUp(int x, int y)
{
    (void)x;
    (void)y;

    if (m_dragMode != DragMode_None) {
        SaveSettings();
    }
    m_dragMode = DragMode_None;
}

void UIOptionWnd::OnLBtnDblClk(int x, int y)
{
    (void)x;
    if (y < m_y + kTitleBarHeight) {
        SetCollapsed(!m_collapsed);
    }
}

msgresult_t UIOptionWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)wparam;
    (void)extra;

    if (msg != 6 || !sender) {
        return 0;
    }

    bool applyAudio = false;
    if (sender == m_bgmOnCheckBox) {
        m_bgmEnabled = (lparam != 0) ? 0 : 1;
        applyAudio = true;
    } else if (sender == m_soundOnCheckBox) {
        m_soundEnabled = (lparam != 0) ? 0 : 1;
        applyAudio = true;
    } else if (sender == m_noCtrlCheckBox) {
        m_noCtrl = (lparam != 0) ? 1 : 0;
    } else if (sender == m_attackSnapCheckBox) {
        m_attackSnap = (lparam != 0) ? 1 : 0;
    } else if (sender == m_skillSnapCheckBox) {
        m_skillSnap = (lparam != 0) ? 1 : 0;
    } else if (sender == m_itemSnapCheckBox) {
        m_itemSnap = (lparam != 0) ? 1 : 0;
    } else {
        return 0;
    }

    if (applyAudio) {
        Invalidate();
        ApplyAudioSettings();
    } else {
        Invalidate();
    }
    SaveSettings();
    return 1;
}

void UIOptionWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE) {
        SetShow(0);
        SaveSettings();
    }
}