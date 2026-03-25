#include "UIOptionWnd.h"

#include "audio/Audio.h"
#include "core/File.h"
#include "main/WinMain.h"
#include "ui/UIWindowMgr.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
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

constexpr int kDefaultWidth = 280;
constexpr int kDefaultHeight = 182;
constexpr int kCollapsedHeight = 17;
constexpr int kTitleBarHeight = 17;
constexpr int kDefaultX = 185;
constexpr int kDefaultY = 300;
constexpr int kSliderMin = 0;
constexpr int kSliderMax = 127;
constexpr int kRendererEntryCount = 4;
constexpr int kRendererRowHeight = 16;

constexpr int kCheckIdBgm = 401;
constexpr int kCheckIdSound = 402;
constexpr int kCheckIdNoCtrl = 403;
constexpr int kCheckIdAttack = 404;
constexpr int kCheckIdSkill = 405;
constexpr int kCheckIdItem = 406;

constexpr std::array<RenderBackendType, kRendererEntryCount> kRendererEntries = {
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

void DrawRectFrame(HDC hdc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rc, brush);
    DeleteObject(brush);
}

int ClampSliderValue(int value)
{
    return (std::max)(kSliderMin, (std::min)(kSliderMax, value));
}

RenderBackendType NormalizePreferredBackend(RenderBackendType backend)
{
    return IsRenderBackendImplemented(backend) ? backend : RenderBackendType::Direct3D11;
}

const char* GetRendererEntryStateText(
    RenderBackendType entry,
    RenderBackendType preferredBackend,
    RenderBackendType activeBackend)
{
    if (!IsRenderBackendImplemented(entry)) {
        return "Not implemented";
    }

    if (!IsRenderBackendSupported(entry)) {
        return "Unsupported";
    }

    if (entry == preferredBackend && entry != activeBackend) {
        return "Restart required";
    }

    if (entry == activeBackend) {
        return "Active";
    }

    return "Available";
}

const char* GetRendererEntryPrefix(
    RenderBackendType entry,
    RenderBackendType preferredBackend,
    RenderBackendType activeBackend)
{
    if (entry == preferredBackend && entry == activeBackend) {
        return "*> ";
    }

    if (entry == preferredBackend) {
        return "> ";
    }

    if (entry == activeBackend) {
        return "* ";
    }

    return "";
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

} // namespace

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
    m_preferredRenderBackend(static_cast<int>(GetConfiguredRenderBackend())),
      m_dragMode(DragMode_None),
      m_dragAnchorX(0),
      m_dragAnchorY(0),
      m_dragWindowStartX(0),
      m_dragWindowStartY(0)
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
        "win_option.bmp",
        "optionwin.bmp",
        "win_option2.bmp",
        "win_option_t.bmp",
    });
    if (!m_frameBitmapPath.empty()) {
        m_frameBitmap = LoadBitmapFromGameData(m_frameBitmapPath.c_str());
    }

    m_bodyBitmapPath = ResolveUiAssetPath({
        "win_option_sub.bmp",
        "win_option_body.bmp",
        "option_sub.bmp",
    });
    if (!m_bodyBitmapPath.empty()) {
        m_bodyBitmap = LoadBitmapFromGameData(m_bodyBitmapPath.c_str());
    }
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
        LoadDwordSetting(key, kOptionWndBgmVolumeValue, &m_bgmVolume);
        LoadDwordSetting(key, kOptionWndSoundVolumeValue, &m_soundVolume);
        LoadDwordSetting(key, kOptionWndBgmOnValue, &m_bgmEnabled);
        LoadDwordSetting(key, kOptionWndSoundOnValue, &m_soundEnabled);
        LoadDwordSetting(key, kOptionWndNoCtrlValue, &m_noCtrl);
        LoadDwordSetting(key, kOptionWndAttackSnapValue, &m_attackSnap);
        LoadDwordSetting(key, kOptionWndSkillSnapValue, &m_skillSnap);
        LoadDwordSetting(key, kOptionWndItemSnapValue, &m_itemSnap);
        LoadDwordSetting(key, kOptionWndCollapsedValue, &m_collapsed);
        RegCloseKey(key);
    }

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

    m_bgmVolume = ClampSliderValue(m_bgmVolume);
    m_soundVolume = ClampSliderValue(m_soundVolume);
    m_bgmEnabled = (m_bgmEnabled != 0) ? 1 : 0;
    m_soundEnabled = (m_soundEnabled != 0) ? 1 : 0;
    m_noCtrl = (m_noCtrl != 0) ? 1 : 0;
    m_attackSnap = (m_attackSnap != 0) ? 1 : 0;
    m_skillSnap = (m_skillSnap != 0) ? 1 : 0;
    m_itemSnap = (m_itemSnap != 0) ? 1 : 0;
    m_collapsed = (m_collapsed != 0) ? 1 : 0;
    m_preferredRenderBackend = static_cast<int>(NormalizePreferredBackend(GetConfiguredRenderBackend()));
    if (m_collapsed) {
        m_h = kCollapsedHeight;
    }
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
    RegCloseKey(key);
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
    if (m_bgmOnCheckBox) {
        m_bgmOnCheckBox->Move(m_x + m_w - 31, m_y + 20);
        m_bgmOnCheckBox->SetShow(m_collapsed ? 0 : 1);
    }
    if (m_soundOnCheckBox) {
        m_soundOnCheckBox->Move(m_x + m_w - 31, m_y + 39);
        m_soundOnCheckBox->SetShow(m_collapsed ? 0 : 1);
    }
    if (m_noCtrlCheckBox) {
        m_noCtrlCheckBox->Move(m_x + 11, m_y + 160);
        m_noCtrlCheckBox->SetShow(m_collapsed ? 0 : 1);
    }
    if (m_attackSnapCheckBox) {
        m_attackSnapCheckBox->Move(m_x + 112, m_y + 160);
        m_attackSnapCheckBox->SetShow(m_collapsed ? 0 : 1);
    }
    if (m_skillSnapCheckBox) {
        m_skillSnapCheckBox->Move(m_x + 162, m_y + 160);
        m_skillSnapCheckBox->SetShow(m_collapsed ? 0 : 1);
    }
    if (m_itemSnapCheckBox) {
        m_itemSnapCheckBox->Move(m_x + 204, m_y + 160);
        m_itemSnapCheckBox->SetShow(m_collapsed ? 0 : 1);
    }
}

void UIOptionWnd::SetCollapsed(bool collapsed)
{
    m_collapsed = collapsed ? 1 : 0;
    m_h = m_collapsed ? kCollapsedHeight : m_orgHeight;
    LayoutControls();
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

RECT UIOptionWnd::GetBaseButtonRect() const
{
    RECT rc = { m_x + 3, m_y + 3, m_x + 14, m_y + 14 };
    return rc;
}

RECT UIOptionWnd::GetMiniButtonRect() const
{
    RECT rc = { m_x + 252, m_y + 3, m_x + 263, m_y + 14 };
    return rc;
}

RECT UIOptionWnd::GetCloseButtonRect() const
{
    RECT rc = { m_x + 266, m_y + 3, m_x + 277, m_y + 14 };
    return rc;
}

RECT UIOptionWnd::GetSkinRect() const
{
    RECT rc = { m_x + 75, m_y + 64, m_x + 256, m_y + 80 };
    return rc;
}

RECT UIOptionWnd::GetRendererRect() const
{
    RECT rc = { m_x + 75, m_y + 64, m_x + 256, m_y + 64 + 4 + (kRendererEntryCount * kRendererRowHeight) };
    return rc;
}

RECT UIOptionWnd::GetRendererEntryRect(int index) const
{
    RECT rendererRect = GetRendererRect();
    if (index < 0) {
        index = 0;
    } else if (index >= kRendererEntryCount) {
        index = kRendererEntryCount - 1;
    }

    const int rowHeight = kRendererRowHeight;
    RECT rc = {
        rendererRect.left + 2,
        rendererRect.top + 2 + (index * rowHeight),
        rendererRect.right - 2,
        rendererRect.top + 2 + ((index + 1) * rowHeight),
    };
    return rc;
}

RECT UIOptionWnd::GetRestartButtonRect() const
{
    RECT rc = {
        m_x + 184,
        m_y + 138,
        m_x + 256,
        m_y + 154,
    };
    return rc;
}

RECT UIOptionWnd::GetBgmSliderRect() const
{
    RECT rc = { m_x + 71, m_y + 21, m_x + m_w - 58, m_y + 34 };
    return rc;
}

RECT UIOptionWnd::GetSoundSliderRect() const
{
    RECT rc = { m_x + 71, m_y + 40, m_x + m_w - 58, m_y + 53 };
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
    FillRectColor(hdc, track, RGB(116, 98, 82));
    DrawRectFrame(hdc, track, RGB(72, 58, 45));

    RECT knob = GetSliderKnobRect(sliderRect, value);
    FillRectColor(hdc, knob, RGB(240, 229, 206));
    DrawRectFrame(hdc, knob, RGB(79, 60, 38));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    const int labelBaseX = static_cast<int>(sliderRect.left) - 48;
    const int labelX = (std::max)(m_x + 18, labelBaseX);
    TextOutA(hdc, labelX, sliderRect.top - 1, label, static_cast<int>(std::strlen(label)));
}

void UIOptionWnd::DrawHeaderButton(HDC hdc, const RECT& rect, const char* text) const
{
    FillRectColor(hdc, rect, RGB(222, 208, 190));
    DrawRectFrame(hdc, rect, RGB(82, 63, 45));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutA(hdc, rect.left + 2, rect.top - 1, text, static_cast<int>(std::strlen(text)));
}

bool UIOptionWnd::HasPendingRendererRestart() const
{
    const RenderBackendType preferredBackend = NormalizePreferredBackend(static_cast<RenderBackendType>(m_preferredRenderBackend));
    return preferredBackend != GetActiveRenderBackend();
}

void UIOptionWnd::PromptForRendererRestart()
{
    const int restartNow = MessageBoxA(
        g_hMainWnd,
        "Renderer changes require a client restart. If you are currently in-game, the client will disconnect from the map server before it relaunches. Restart now?",
        "Restart Required",
        MB_ICONQUESTION | MB_YESNO);
    if (restartNow == IDYES) {
        if (!RelaunchCurrentApplication()) {
            MessageBoxA(
                g_hMainWnd,
                "Failed to relaunch the client. The renderer setting was saved and will apply the next time you start the game.",
                "Restart Failed",
                MB_ICONERROR | MB_OK);
        }
    }
}

void UIOptionWnd::SelectPreferredRenderBackend(RenderBackendType backend)
{
    if (!IsRenderBackendImplemented(backend) || !IsRenderBackendSupported(backend)) {
        return;
    }

    const RenderBackendType preferredBackend = NormalizePreferredBackend(static_cast<RenderBackendType>(m_preferredRenderBackend));
    if (backend == preferredBackend) {
        return;
    }

    if (SetConfiguredRenderBackend(backend)) {
        m_preferredRenderBackend = static_cast<int>(backend);
        SaveSettings();

        if (backend != GetActiveRenderBackend()) {
            PromptForRendererRestart();
        }
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

    m_bgmOnCheckBox = makeCheckBox(kCheckIdBgm, m_bgmEnabled);
    m_soundOnCheckBox = makeCheckBox(kCheckIdSound, m_soundEnabled);
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

    RECT titleRect = GetTitleBarRect();
    RECT bodyRect = { m_x, m_y + kTitleBarHeight, m_x + m_w, m_y + m_h };

    if (m_frameBitmap) {
        DrawBitmapStretched(hdc, m_frameBitmap, titleRect);
    } else {
        FillRectColor(hdc, titleRect, RGB(199, 178, 152));
        DrawRectFrame(hdc, titleRect, RGB(83, 65, 44));
    }

    if (!m_collapsed) {
        if (m_bodyBitmap) {
            DrawBitmapStretched(hdc, m_bodyBitmap, bodyRect);
        } else {
            FillRectColor(hdc, bodyRect, RGB(231, 217, 197));
            DrawRectFrame(hdc, bodyRect, RGB(96, 76, 54));
            RECT dividerRect = { m_x + 72, m_y + 62, m_x + m_w - 24, m_y + 154 };
            FillRectColor(hdc, dividerRect, RGB(215, 199, 177));
            DrawRectFrame(hdc, dividerRect, RGB(118, 98, 80));
        }
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutA(hdc, m_x + 17, m_y + 4, "Game option", 11);
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutA(hdc, m_x + 16, m_y + 3, "Game option", 11);

    DrawHeaderButton(hdc, GetBaseButtonRect(), "B");
    DrawHeaderButton(hdc, GetMiniButtonRect(), "_");
    DrawHeaderButton(hdc, GetCloseButtonRect(), "X");

    if (!m_collapsed) {
        DrawSlider(hdc, GetBgmSliderRect(), m_bgmVolume, "BGM");
        DrawSlider(hdc, GetSoundSliderRect(), m_soundVolume, "Sound");

        const RECT rendererRect = GetRendererRect();
        FillRectColor(hdc, rendererRect, RGB(244, 239, 228));
        DrawRectFrame(hdc, rendererRect, RGB(118, 98, 80));
        const RenderBackendType preferredBackend = NormalizePreferredBackend(static_cast<RenderBackendType>(m_preferredRenderBackend));
        const RenderBackendType activeBackend = GetActiveRenderBackend();
        for (int index = 0; index < kRendererEntryCount; ++index) {
            const RenderBackendType entry = kRendererEntries[static_cast<size_t>(index)];
            const RECT entryRect = GetRendererEntryRect(index);
            const bool implemented = IsRenderBackendImplemented(entry);
            const bool supported = implemented && IsRenderBackendSupported(entry);
            const bool selected = (entry == preferredBackend);
            const bool active = (entry == activeBackend);

            COLORREF fillColor = RGB(244, 239, 228);
            if (!implemented) {
                fillColor = RGB(223, 216, 206);
            } else if (!supported) {
                fillColor = RGB(230, 214, 206);
            } else if (selected && active) {
                fillColor = RGB(214, 228, 206);
            } else if (selected) {
                fillColor = RGB(235, 222, 196);
            } else if (active) {
                fillColor = RGB(222, 231, 214);
            }

            FillRectColor(hdc, entryRect, fillColor);
            DrawRectFrame(hdc, entryRect, RGB(158, 137, 113));

            const char* backendName = GetRenderBackendName(entry);
            const char* stateText = GetRendererEntryStateText(entry, preferredBackend, activeBackend);
            const char* prefixText = GetRendererEntryPrefix(entry, preferredBackend, activeBackend);
            std::string label = std::string(prefixText) + backendName;
            const int textY = entryRect.top + 2;
            SetTextColor(hdc, (implemented && supported) ? RGB(0, 0, 0) : RGB(90, 82, 74));
            TextOutA(hdc, entryRect.left + 4, textY, label.c_str(), static_cast<int>(label.size()));

            SIZE stateSize{};
            GetTextExtentPoint32A(hdc, stateText, static_cast<int>(std::strlen(stateText)), &stateSize);
            TextOutA(
                hdc,
                entryRect.right - stateSize.cx - 4,
                textY,
                stateText,
                static_cast<int>(std::strlen(stateText)));
        }

        if (preferredBackend != activeBackend) {
            const RECT restartRect = GetRestartButtonRect();
            FillRectColor(hdc, restartRect, RGB(222, 208, 190));
            DrawRectFrame(hdc, restartRect, RGB(82, 63, 45));
            TextOutA(hdc, restartRect.left + 10, restartRect.top + 3, "Restart", 7);
        }

        SetTextColor(hdc, RGB(0, 0, 0));
        TextOutA(hdc, m_x + 76, m_y + 160, "Snap", 4);

        TextOutA(hdc, m_x + 31, m_y + 160, "NoCtrl", 7);
        TextOutA(hdc, m_x + 130, m_y + 160, "Attack", 6);
        TextOutA(hdc, m_x + 180, m_y + 160, "Skill", 5);
        TextOutA(hdc, m_x + 222, m_y + 160, "Item", 4);
    }

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

    if (PointInRectXY(GetBaseButtonRect(), x, y)) {
        ResetToDefaultPlacement();
        return;
    }

    if (!m_collapsed && HasPendingRendererRestart() && PointInRectXY(GetRestartButtonRect(), x, y)) {
        PromptForRendererRestart();
        return;
    }

    if (!m_collapsed && PointInRectXY(GetRendererRect(), x, y)) {
        for (int index = 0; index < kRendererEntryCount; ++index) {
            if (PointInRectXY(GetRendererEntryRect(index), x, y)) {
                SelectPreferredRenderBackend(kRendererEntries[static_cast<size_t>(index)]);
                return;
            }
        }
    }

    if (!m_collapsed && PointInRectXY(GetBgmSliderRect(), x, y)) {
        m_dragMode = DragMode_BgmSlider;
        m_bgmVolume = SliderValueFromMouseX(x);
        ApplyAudioSettings();
        return;
    }

    if (!m_collapsed && PointInRectXY(GetSoundSliderRect(), x, y)) {
        m_dragMode = DragMode_SoundSlider;
        m_soundVolume = SliderValueFromMouseX(x);
        ApplyAudioSettings();
        return;
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
        Move(m_dragWindowStartX + (x - m_dragAnchorX), m_dragWindowStartY + (y - m_dragAnchorY));
        LayoutControls();
        break;

    case DragMode_BgmSlider:
        m_bgmVolume = SliderValueFromMouseX(x);
        ApplyAudioSettings();
        break;

    case DragMode_SoundSlider:
        m_soundVolume = SliderValueFromMouseX(x);
        ApplyAudioSettings();
        break;

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
    if (y < m_y + kTitleBarHeight) {
        SetCollapsed(!m_collapsed);
    }
}

int UIOptionWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra)
{
    (void)wparam;
    (void)extra;

    if (msg != 6 || !sender) {
        return 0;
    }

    if (sender == m_bgmOnCheckBox) {
        m_bgmEnabled = (lparam != 0) ? 1 : 0;
    } else if (sender == m_soundOnCheckBox) {
        m_soundEnabled = (lparam != 0) ? 1 : 0;
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

    ApplyAudioSettings();
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