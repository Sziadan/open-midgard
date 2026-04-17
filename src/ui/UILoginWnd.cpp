#include "UILoginWnd.h"

#include "core/File.h"
#include "core/SettingsIni.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "res/Bitmap.h"
#include "ui/UIWindowMgr.h"
#include "DebugLog.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <vector>

namespace {

constexpr int kLoginFieldLeft = 92;
constexpr int kLoginFieldTop = 29;
constexpr int kPasswordFieldTop = 61;
constexpr int kFieldWidth = 125;
constexpr int kFieldHeight = 18;
constexpr int kSaveCheckLeft = 232;
constexpr int kSaveCheckTop = 33;
constexpr int kSaveCheckSize = 16;

struct LoginButtonHitArea {
    int id;
    int x;
    int y;
    int width;
    int height;
    const char* label;
};

constexpr LoginButtonHitArea kQtLoginButtonAreas[] = {
    { 201, 4, 96, 52, 20, "Request" },
    { 219, 137, 96, 44, 20, "Intro" },
    { 120, 189, 96, 40, 20, "Connect" },
    { 155, 234, 96, 40, 20, "Exit" },
};

shopui::BitmapPixels LoadBitmapPixelsFromGameData(const char* path)
{
    return shopui::LoadBitmapPixelsFromGameData(path ? path : "", false);
}

bool ContainsPoint(int left, int top, int width, int height, int x, int y)
{
    return x >= left && x < left + width && y >= top && y < top + height;
}

std::string GetNormalizedFileNameLower(std::string value)
{
    value = shopui::NormalizeSlash(std::move(value));
    const size_t slashPos = value.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < value.size()) {
        value = value.substr(slashPos + 1);
    }
    return shopui::ToLowerAscii(std::move(value));
}

bool IsClassicLoginPanelAssetName(const std::string& value)
{
    const std::string fileName = GetNormalizedFileNameLower(value);
    return fileName == "win_login.bmp"
        || fileName == "loginwin.bmp"
        || fileName == "win_login_interface.bmp"
        || fileName == "login_interface.bmp"
        || fileName == "win_msgbox.bmp";
}

void DrawBitmapPixelsStretched(HDC target, const shopui::BitmapPixels& bmp, const RECT& dst)
{
    if (!target || !bmp.IsValid() || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.width;
    bmi.bmiHeader.biHeight = -bmp.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int oldStretchMode = SetStretchBltMode(target, HALFTONE);
    StretchDIBits(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        0,
        0,
        bmp.width,
        bmp.height,
        bmp.pixels.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
    SetStretchBltMode(target, oldStretchMode);
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

const std::vector<std::string>& GetArchiveNamesByExtension(const char* ext)
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

std::string GetLowerFileExtension(const std::string& value)
{
    const std::string normalized = NormalizeSlash(value);
    const size_t dotPos = normalized.find_last_of('.');
    if (dotPos == std::string::npos || dotPos + 1 >= normalized.size()) {
        return {};
    }
    return ToLowerAscii(normalized.substr(dotPos + 1));
}

int ScoreArchiveUiPath(const std::string& requestedPath, const std::string& resolvedPath)
{
    const std::string loweredRequested = ToLowerAscii(NormalizeSlash(requestedPath));
    const std::string loweredResolved = ToLowerAscii(NormalizeSlash(resolvedPath));
    int score = 0;

    if (loweredResolved == loweredRequested) {
        score += 10000;
    }
    if (loweredResolved.rfind("data\\", 0) == 0) {
        score += 800;
    }
    if (loweredResolved.find("\\texture\\") != std::string::npos) {
        score += 300;
    }
    if (loweredResolved.find("\\login_interface\\") != std::string::npos) {
        score += 400;
    }
    if (loweredResolved.find("\\basic_interface\\") != std::string::npos) {
        score += 350;
    }
    if (loweredResolved.find("\\interface\\") != std::string::npos) {
        score += 200;
    }
    if (loweredResolved.find("\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA") != std::string::npos) {
        score += 450;
    }

    return score;
}

std::string ResolveArchiveAssetByFileName(const std::string& requestedPath)
{
    const std::string extension = GetLowerFileExtension(requestedPath);
    if (extension.empty()) {
        return {};
    }

    const std::string requestedFileName = GetNormalizedFileNameLower(requestedPath);
    if (requestedFileName.empty()) {
        return {};
    }

    const std::vector<std::string>& names = GetArchiveNamesByExtension(extension.c_str());
    int bestScore = -1;
    std::string bestPath;
    for (const std::string& name : names) {
        if (GetNormalizedFileNameLower(name) != requestedFileName) {
            continue;
        }

        const int score = ScoreArchiveUiPath(requestedPath, name);
        if (score > bestScore) {
            bestScore = score;
            bestPath = name;
        }
    }

    return bestPath;
}

constexpr char kLoginSettingsSection[] = "Login";
constexpr char kRememberUserIdEnabledValue[] = "RememberUserIdEnabled";
constexpr char kRememberUserIdValue[] = "RememberUserId";

void StoreRememberedUserId(const char* userId, bool enabled)
{
    SaveSettingsIniInt(kLoginSettingsSection, kRememberUserIdEnabledValue, enabled ? 1 : 0);
    SaveSettingsIniString(kLoginSettingsSection, kRememberUserIdValue, (enabled && userId) ? std::string(userId) : std::string());
}

void LoadRememberedUserId(std::string* userId, bool* enabled)
{
    if (userId) {
        userId->clear();
    }
    if (enabled) {
        *enabled = false;
    }

    const int enabledValue = LoadSettingsIniInt(kLoginSettingsSection, kRememberUserIdEnabledValue, 0);
    const std::string storedUserId = LoadSettingsIniString(kLoginSettingsSection, kRememberUserIdValue, "");
    if (userId) {
        *userId = storedUserId;
    }
    if (enabled) {
        *enabled = enabledValue != 0 && !storedUserId.empty();
    }
}

void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
{
    if (raw.empty()) {
        return;
    }

    std::string normalized = NormalizeSlash(raw);
    if (normalized.empty()) {
        return;
    }

    const std::string lowered = ToLowerAscii(normalized);
    for (const std::string& existing : out) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out.push_back(std::move(normalized));
}

std::vector<std::string> BuildWallpaperCandidates(const std::string& requestedWallpaper)
{
    std::vector<std::string> out;

    // Legacy RO GRFs commonly store UI under a Korean-named folder. Keep raw
    // byte escapes to match archive keys regardless of source-file encoding.
    static const char* kUiKor =
        "texture\\"
        "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
        "\\";

    const char* directDefaults[] = {
        "ad_title.png",
        "ad_title.jpg",
        "rag_title.jpg",
        "title.bmp",
        "title.jpg",
        "login_background.jpg",
        "login_background.bmp",
        nullptr
    };

    std::vector<std::string> baseNames;
    if (!requestedWallpaper.empty()) {
        baseNames.push_back(requestedWallpaper);
    }
    for (int i = 0; directDefaults[i]; ++i) {
        baseNames.push_back(directDefaults[i]);
    }

    const char* pathPrefixes[] = {
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "ui\\",
        kUiKor,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        nullptr
    };

    for (const std::string& baseRaw : baseNames) {
        std::string base = NormalizeSlash(baseRaw);
        if (base.empty()) {
            continue;
        }

        std::string filenameOnly = base;
        const size_t slashPos = filenameOnly.find_last_of('\\');
        if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
            filenameOnly = filenameOnly.substr(slashPos + 1);
        }

        const bool hasExtension = filenameOnly.find('.') != std::string::npos;
        std::vector<std::string> nameForms;
        nameForms.push_back(base);
        if (filenameOnly != base) {
            nameForms.push_back(filenameOnly);
        }

        if (!hasExtension) {
            nameForms.push_back(filenameOnly + ".bmp");
            nameForms.push_back(filenameOnly + ".jpg");
            nameForms.push_back(filenameOnly + ".png");
            nameForms.push_back(filenameOnly + ".tga");
        }

        for (const std::string& nameForm : nameForms) {
            for (int i = 0; pathPrefixes[i]; ++i) {
                AddUniqueCandidate(out, std::string(pathPrefixes[i]) + nameForm);
            }
        }
    }

    return out;
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
        "skin\\default\\login_interface\\",
        "skin\\default\\interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        kUiKor,
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
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

    // Mirror legacy UIBmp behavior for names that already contain UI prefixes.
    static const char* uiPrefixCandidates[] = {
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        "data\\texture\\login_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\basic_interface\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\login_interface\\",
        nullptr
    };

    for (int i = 0; uiPrefixCandidates[i]; ++i) {
        const std::string prefix = uiPrefixCandidates[i];
        if (base.size() > prefix.size() && ToLowerAscii(base.substr(0, prefix.size())) == ToLowerAscii(prefix)) {
            const std::string suffix = base.substr(prefix.size());
            AddUniqueCandidate(out, std::string("skin\\default\\") + suffix);
            AddUniqueCandidate(out, std::string("skin\\default\\basic_interface\\") + suffix);
            AddUniqueCandidate(out, std::string("skin\\default\\login_interface\\") + suffix);
        }
    }

    return out;
}

shopui::BitmapPixels LoadFirstBitmapPixelsFromCandidates(const std::vector<std::string>& candidates, std::string* outPath)
{
    for (const std::string& candidate : candidates) {
        shopui::BitmapPixels bmp = LoadBitmapPixelsFromGameData(candidate.c_str());
        if (bmp.IsValid()) {
            if (outPath) {
                *outPath = candidate;
            }
            return bmp;
        }
    }

    std::vector<std::string> fallbackCandidates;
    for (const std::string& candidate : candidates) {
        const std::string resolved = ResolveArchiveAssetByFileName(candidate);
        if (!resolved.empty()) {
            AddUniqueCandidate(fallbackCandidates, resolved);
        }
    }

    for (const std::string& candidate : fallbackCandidates) {
        shopui::BitmapPixels bmp = LoadBitmapPixelsFromGameData(candidate.c_str());
        if (bmp.IsValid()) {
            if (outPath) {
                *outPath = candidate;
            }
            return bmp;
        }
    }

    return {};
}

std::string ResolveUiAssetPath(const char* fileName)
{
    if (!fileName || !*fileName) {
        return {};
    }

    const std::vector<std::string> candidates = BuildUiAssetCandidates(fileName);
    for (const std::string& candidate : candidates) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return {};
}

} // namespace

UILoginWnd::UILoginWnd()
    : m_controlsCreated(false),
      m_assetsProbed(false),
      m_wallpaperBmp(),
      m_composeSurface(),
      m_saveAccountChecked(false),
      m_login(nullptr),
      m_password(nullptr),
      m_cancelButton(nullptr),
        m_saveAccountCheck(nullptr),
      m_hasRememberedUserIdSnapshot(false),
      m_lastRememberedUserIdEnabled(false) {
    m_uiAssets.fill(shopui::BitmapPixels{});
}

UILoginWnd::~UILoginWnd()
{
    if (m_login && m_login->m_parent != this) {
        delete m_login;
        m_login = nullptr;
    }
    if (m_password && m_password->m_parent != this) {
        delete m_password;
        m_password = nullptr;
    }
    if (m_saveAccountCheck && m_saveAccountCheck->m_parent != this) {
        delete m_saveAccountCheck;
        m_saveAccountCheck = nullptr;
    }
    if (m_cancelButton && m_cancelButton->m_parent != this) {
        delete m_cancelButton;
        m_cancelButton = nullptr;
    }
    ReleaseComposeSurface();
    ClearUiAssets();
}

void SetLoginFieldFocus(UIEditCtrl* login, UIEditCtrl* password, bool focusPassword)
{
    if (login) {
        login->m_hasFocus = !focusPassword;
        login->Invalidate();
    }
    if (password) {
        password->m_hasFocus = focusPassword;
        password->Invalidate();
    }
    g_windowMgr.m_editWindow = focusPassword
        ? static_cast<UIWindow*>(password)
        : static_cast<UIWindow*>(login);
}

const char* UILoginWnd::GetLoginText() const
{
    return m_login ? m_login->GetText() : "";
}

int UILoginWnd::GetPasswordLength() const
{
    const char* text = m_password ? m_password->GetText() : "";
    return text ? static_cast<int>(std::strlen(text)) : 0;
}

bool UILoginWnd::IsSaveAccountChecked() const
{
    return m_saveAccountCheck ? (m_saveAccountCheck->m_isChecked != 0) : m_saveAccountChecked;
}

bool UILoginWnd::IsPasswordFocused() const
{
    return m_password && m_password->m_hasFocus;
}

int UILoginWnd::GetQtButtonCount() const
{
    return static_cast<int>(std::size(kQtLoginButtonAreas));
}

bool UILoginWnd::GetQtButtonDisplayForQt(int index, QtButtonDisplay* outButton) const
{
    if (!outButton || index < 0 || index >= GetQtButtonCount()) {
        return false;
    }

    const LoginButtonHitArea& button = kQtLoginButtonAreas[index];
    outButton->id = button.id;
    outButton->x = m_x + button.x;
    outButton->y = m_y + button.y;
    outButton->width = button.width;
    outButton->height = button.height;
    outButton->label = button.label;
    return true;
}

bool UILoginWnd::HandleQtMouseDown(int x, int y)
{
    if (m_show == 0) {
        return false;
    }

    if (!m_controlsCreated && g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    }

    const int localX = x - m_x;
    const int localY = y - m_y;
    if (localX < 0 || localY < 0 || localX >= m_w || localY >= m_h) {
        return false;
    }

    if (ContainsPoint(kLoginFieldLeft, kLoginFieldTop, kFieldWidth, kFieldHeight, localX, localY)) {
        SetLoginFieldFocus(m_login, m_password, false);
        return true;
    }

    if (ContainsPoint(kLoginFieldLeft, kPasswordFieldTop, kFieldWidth, kFieldHeight, localX, localY)) {
        SetLoginFieldFocus(m_login, m_password, true);
        return true;
    }

    if (ContainsPoint(kSaveCheckLeft, kSaveCheckTop, kSaveCheckSize, kSaveCheckSize, localX, localY)) {
        m_saveAccountChecked = !m_saveAccountChecked;
        if (m_saveAccountCheck) {
            m_saveAccountCheck->SetCheck(m_saveAccountChecked ? 1 : 0);
        }
        StoreRememberedUserId(m_login ? m_login->GetText() : "", m_saveAccountChecked);
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SaveAccount, m_saveAccountChecked ? 1 : 0, 0, m_saveAccountChecked ? 1 : 0);
        return true;
    }

    for (const LoginButtonHitArea& button : kQtLoginButtonAreas) {
        if (ContainsPoint(button.x, button.y, button.width, button.height, localX, localY)) {
            SendMsg(nullptr, 6, button.id, 0, 0);
            return true;
        }
    }

    return false;
}

void UILoginWnd::EnsureQtLayout()
{
    if (!g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    if (!GetClientRect(g_hMainWnd, &clientRect)) {
        return;
    }

    OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
}

bool UILoginWnd::GetQtPanelBitmap(const unsigned int** pixels, int* width, int* height)
{
    if (!pixels || !width || !height) {
        return false;
    }

    *pixels = nullptr;
    *width = 0;
    *height = 0;

    EnsureResourceCache();
    const shopui::BitmapPixels& panel = m_uiAssets[UiPanel];
    if (!panel.IsValid() || panel.pixels.empty()) {
        return false;
    }

    *pixels = panel.pixels.data();
    *width = panel.width;
    *height = panel.height;
    return true;
}

void UILoginWnd::RefreshRememberedUserIdStorage()
{
    const bool enabled = IsSaveAccountChecked();
    const std::string userId = m_login ? m_login->GetText() : "";
    if (m_hasRememberedUserIdSnapshot
        && m_lastRememberedUserIdEnabled == enabled
        && m_lastRememberedUserId == userId) {
        return;
    }

    StoreRememberedUserId(userId.c_str(), enabled);
    m_hasRememberedUserIdSnapshot = true;
    m_lastRememberedUserIdEnabled = enabled;
    m_lastRememberedUserId = userId;
}

void UILoginWnd::OnCreate(int cx, int cy)
{
    if (m_controlsCreated) {
        return;
    }
    m_controlsCreated = true;

    Create(280, 120);
    Move((cx - 640) / 2 + 185, (300 * cy) / 480);

    struct ButtonSpec {
        const char* normal;
        const char* hover;
        const char* pressed;
        int id;
        int x;
        int y;
    };

    const ButtonSpec specs[] = {
        { "btn_connect.bmp", "btn_connect_a.bmp", "btn_connect_b.bmp", 120, 189, 96 },
        { "btn_exit.bmp", "btn_exit_a.bmp", "btn_exit_b.bmp", 155, 234, 96 },
        { "btn_cancel.bmp", "btn_cancel_a.bmp", "btn_cancel_b.bmp", 119, 434, 96 },
        { "btn_request.bmp", "btn_request_a.bmp", "btn_request_b.bmp", 201, 4, 96 },
        { "btn_intro.bmp", "btn_intro_a.bmp", "btn_intro_b.bmp", 219, 137, 96 },
    };

    if (!IsQtUiRuntimeEnabled()) {
        for (const ButtonSpec& spec : specs) {
            auto* button = new UIBitmapButton();
            button->SetBitmapName(ResolveUiAssetPath(spec.normal).c_str(), 0);
            button->SetBitmapName(ResolveUiAssetPath(spec.hover).c_str(), 1);
            button->SetBitmapName(ResolveUiAssetPath(spec.pressed).c_str(), 2);
            const int buttonWidth = button->m_bitmapWidth > 0 ? button->m_bitmapWidth : 40;
            const int buttonHeight = button->m_bitmapHeight > 0 ? button->m_bitmapHeight : 20;
            if (spec.x < 0 || spec.y < 0 || spec.x + buttonWidth > m_w || spec.y + buttonHeight > m_h) {
                delete button;
                continue;
            }
            button->Create(buttonWidth, buttonHeight);
            button->Move(m_x + spec.x, m_y + spec.y);
            button->m_id = spec.id;
            AddChild(button);
            if (spec.id == 119) {
                m_cancelButton = button;
            }
        }
    }

    constexpr int kLoginFieldWidth = 125;

    m_password = new UIEditCtrl();
    m_password->Create(kLoginFieldWidth, 18);
    m_password->m_maxchar = 24;
    m_password->Move(m_x + 92, m_y + 61);
    m_password->m_maskchar = '*';
    m_password->SetFrameColor(242, 242, 242);
    if (!IsQtUiRuntimeEnabled()) {
        AddChild(m_password);
    }

    m_login = new UIEditCtrl();
    m_login->Create(kLoginFieldWidth, 18);
    m_login->m_maxchar = 24;
    m_login->Move(m_x + 92, m_y + 29);
    m_login->SetFrameColor(242, 242, 242);
    if (!IsQtUiRuntimeEnabled()) {
        AddChild(m_login);
    }

    std::string rememberedUserId;
    bool rememberUserId = false;
    LoadRememberedUserId(&rememberedUserId, &rememberUserId);
    m_saveAccountChecked = rememberUserId;
    if (rememberUserId && !rememberedUserId.empty()) {
        m_login->SetText(rememberedUserId.c_str());
    }
    m_hasRememberedUserIdSnapshot = true;
    m_lastRememberedUserIdEnabled = rememberUserId;
    m_lastRememberedUserId = rememberedUserId;

    if (!IsQtUiRuntimeEnabled()) {
        m_saveAccountCheck = new UICheckBox();
        m_saveAccountCheck->SetBitmap(
            ResolveUiAssetPath("chk_saveon.bmp").c_str(),
            ResolveUiAssetPath("chk_saveoff.bmp").c_str());
        m_saveAccountCheck->Create(m_saveAccountCheck->m_w > 0 ? m_saveAccountCheck->m_w : 16,
            m_saveAccountCheck->m_h > 0 ? m_saveAccountCheck->m_h : 16);
        m_saveAccountCheck->Move(m_x + 232, m_y + 33);
        m_saveAccountCheck->SetCheck(rememberUserId ? 1 : 0);
        AddChild(m_saveAccountCheck);
    }

    const bool focusPassword = (m_login && m_login->GetText() && m_login->GetText()[0] != '\0');
    SetLoginFieldFocus(m_login, m_password, focusPassword);
}

void UILoginWnd::ClearUiAssets()
{
    m_wallpaperBmp.Clear();
    for (shopui::BitmapPixels& bmp : m_uiAssets) {
        bmp.Clear();
    }
    for (std::string& path : m_uiAssetPaths) {
        path.clear();
    }
}

void UILoginWnd::SetWallpaperName(const std::string& wallpaperName)
{
    m_requestedWallpaper = wallpaperName;
    m_assetsProbed = false;
    ClearUiAssets();
    m_wallpaperPath.clear();
}

void UILoginWnd::ReleaseComposeSurface()
{
    m_composeSurface.Release();
}

bool UILoginWnd::EnsureComposeSurface(int width, int height)
{
    return m_composeSurface.EnsureSize(width, height);
}

void UILoginWnd::EnsureResourceCache()
{
    if (m_assetsProbed) {
        return;
    }
    m_assetsProbed = true;

    const std::vector<std::string> candidates = BuildWallpaperCandidates(m_requestedWallpaper);
    for (const std::string& candidate : candidates) {
        shopui::BitmapPixels bmp = LoadBitmapPixelsFromGameData(candidate.c_str());
        if (bmp.IsValid()) {
            m_wallpaperBmp = bmp;
            m_wallpaperPath = candidate;
            break;
        }
    }

    const char* panelNames[] = {
        "win_login.bmp",
        "loginwin.bmp",
        "win_login_interface.bmp",
        "login_interface.bmp",
        "win_msgbox.bmp",
        nullptr
    };
    for (int i = 0; panelNames[i] && !m_uiAssets[UiPanel].IsValid(); ++i) {
        m_uiAssets[UiPanel] = LoadFirstBitmapPixelsFromCandidates(
            BuildUiAssetCandidates(panelNames[i]), &m_uiAssetPaths[UiPanel]);
    }
    if (!m_uiAssets[UiPanel].IsValid()
        && m_wallpaperBmp.IsValid()
        && IsClassicLoginPanelAssetName(m_wallpaperPath)) {
        m_uiAssets[UiPanel] = m_wallpaperBmp;
        m_uiAssetPaths[UiPanel] = m_wallpaperPath;
        DbgLog("[LoginUIAsset] PANEL REUSE WALLPAPER: %s\n", m_uiAssetPaths[UiPanel].c_str());
    }
    if (m_uiAssets[UiPanel].IsValid()) {
        DbgLog("[LoginUIAsset] PANEL HIT: %s\n", m_uiAssetPaths[UiPanel].c_str());
    } else {
        DbgLog("[LoginUIAsset] PANEL MISS\n");
    }

    const char* logoNames[] = {
        "logo.bmp",
        "ragnarok_logo.bmp",
        "title_logo.bmp",
        "ad_title.jpg",
        nullptr
    };
    for (int i = 0; logoNames[i] && !m_uiAssets[UiLogo].IsValid(); ++i) {
        m_uiAssets[UiLogo] = LoadFirstBitmapPixelsFromCandidates(
            BuildUiAssetCandidates(logoNames[i]), &m_uiAssetPaths[UiLogo]);
    }
    if (m_uiAssets[UiLogo].IsValid()) {
        DbgLog("[LoginUIAsset] LOGO HIT: %s\n", m_uiAssetPaths[UiLogo].c_str());
    } else {
        DbgLog("[LoginUIAsset] LOGO MISS\n");
    }

    const char* buttonNames[] = {
        "BTN_OK.BMP",
        "BTN_OK_A.BMP",
        "btn_connect.bmp",
        "btn_login.bmp",
        "btn_ok.bmp",
        nullptr
    };
    for (int i = 0; buttonNames[i] && !m_uiAssets[UiButton].IsValid(); ++i) {
        m_uiAssets[UiButton] = LoadFirstBitmapPixelsFromCandidates(
            BuildUiAssetCandidates(buttonNames[i]), &m_uiAssetPaths[UiButton]);
    }
    if (m_uiAssets[UiButton].IsValid()) {
        DbgLog("[LoginUIAsset] BUTTON HIT: %s\n", m_uiAssetPaths[UiButton].c_str());
    } else {
        DbgLog("[LoginUIAsset] BUTTON MISS\n");
    }

    const char* fieldNames[] = {
        "btn_edit.bmp",
        "btn_edit_a.bmp",
        nullptr
    };
    for (int i = 0; fieldNames[i] && !m_uiAssets[UiField].IsValid(); ++i) {
        m_uiAssets[UiField] = LoadFirstBitmapPixelsFromCandidates(
            BuildUiAssetCandidates(fieldNames[i]), &m_uiAssetPaths[UiField]);
    }
    if (m_uiAssets[UiField].IsValid()) {
        DbgLog("[LoginUIAsset] FIELD HIT: %s\n", m_uiAssetPaths[UiField].c_str());
    } else {
        DbgLog("[LoginUIAsset] FIELD MISS\n");
    }
}

void UILoginWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    EnsureResourceCache();

    RECT rcClient{};
    GetClientRect(g_hMainWnd, &rcClient);

    const int clientW = rcClient.right - rcClient.left;
    const int clientH = rcClient.bottom - rcClient.top;
    if (clientW <= 0 || clientH <= 0) {
        return;
    }

    if (!m_controlsCreated) {
        OnCreate(clientW, clientH);
    }

    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    const bool useCompose = EnsureComposeSurface(clientW, clientH);
    HDC targetDC = nullptr;
    HDC drawDC = nullptr;
    if (useCompose) {
        drawDC = m_composeSurface.GetDC();
    } else {
        targetDC = AcquireDrawTarget();
        if (!targetDC) {
            return;
        }
        drawDC = targetDC;
    }

    if (m_wallpaperBmp.IsValid()) {
        DrawBitmapPixelsStretched(drawDC, m_wallpaperBmp, rcClient);
    }

    RECT rcPanel = { m_x, m_y, m_x + m_w, m_y + m_h };
    if (m_uiAssets[UiPanel].IsValid()) {
        rcPanel.right = rcPanel.left + m_uiAssets[UiPanel].width;
        rcPanel.bottom = rcPanel.top + m_uiAssets[UiPanel].height;
        DrawBitmapPixelsStretched(drawDC, m_uiAssets[UiPanel], rcPanel);
    } else {
        HBRUSH panelBg = CreateSolidBrush(RGB(235, 235, 228));
        FillRect(drawDC, &rcPanel, panelBg);
        DeleteObject(panelBg);
        FrameRect(drawDC, &rcPanel, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    DrawChildrenToHdc(drawDC);

    if (useCompose) {
        if (!BlitArgbBitsToDrawTarget(m_composeSurface.GetBits(), clientW, clientH)) {
            return;
        }
    } else {
        ReleaseDrawTarget(targetDC);
    }
}

msgresult_t UILoginWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)lparam;

    // Message 6 = button click (Ref pattern: val1/wparam = button ID)
    if (msg == 6) {
        switch (wparam) {
        case 120: // connect
            PlayUiButtonSound();
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                IsSaveAccountChecked());
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetPassword,
                reinterpret_cast<msgparam_t>(m_password ? m_password->GetText() : ""), 0, 0);
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetUserId,
                reinterpret_cast<msgparam_t>(m_login ? m_login->GetText() : ""), 0, 0);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestConnect, 0, 0, 0);

        case 119: // cancel
            PlayUiButtonSound();
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_ReturnToLogin, 0, 0, 0);

        case 155: // exit
            PlayUiButtonSound();
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Quit, 0, 0, 0);

        case 201: // request
            PlayUiButtonSound();
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestAccount, 0, 0, 0);

        case 219: // intro
            PlayUiButtonSound();
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Intro, 0, 0, 0);

        default:
            break;
        }

        // Check if it's from the save-account checkbox (its m_id may not be a well-known value)
        if (sender == m_saveAccountCheck) {
            m_saveAccountChecked = (m_saveAccountCheck && m_saveAccountCheck->m_isChecked != 0);
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                m_saveAccountChecked);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_SaveAccount,
                m_saveAccountChecked ? 1 : 0, 0, m_saveAccountChecked ? 1 : 0);
        }
    }

    // For legacy routing: also check sender->m_id in case called with msg != 6
    if (msg != 6 && sender) {
        const int senderId = sender->m_id;
        switch (senderId) {
        case 120:
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                IsSaveAccountChecked());
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetPassword,
                reinterpret_cast<msgparam_t>(m_password ? m_password->GetText() : ""), 0, 0);
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetUserId,
                reinterpret_cast<msgparam_t>(m_login ? m_login->GetText() : ""), 0, 0);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestConnect, 0, 0, 0);
        case 119:
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_ReturnToLogin, 0, 0, 0);
        case 155:
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Quit, 0, 0, 0);
        case 201:
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestAccount, 0, 0, 0);
        case 219:
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Intro, 0, 0, 0);
        default:
            break;
        }
        if (sender == m_saveAccountCheck) {
            m_saveAccountChecked = (m_saveAccountCheck && m_saveAccountCheck->m_isChecked != 0);
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                m_saveAccountChecked);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_SaveAccount,
                m_saveAccountChecked ? 1 : 0, extra, m_saveAccountChecked ? 1 : 0);
        }
    }

    return 0;
}

void UILoginWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_RETURN) {
        // Enter → fire Connect button (id 120)
        SendMsg(nullptr, 6, 120, 0, 0);
    } else if (virtualKey == VK_ESCAPE) {
        // Escape → fire Exit button (id 155)
        SendMsg(nullptr, 6, 155, 0, 0);
    } else if (virtualKey == VK_TAB) {
        // Tab → cycle focus between login and password fields
        if (m_login && m_password) {
            if (m_login->m_hasFocus) {
                m_login->m_hasFocus = false;
                m_password->m_hasFocus = true;
                m_login->Invalidate();
                m_password->Invalidate();
                // Update UIWindowMgr focused edit
                g_windowMgr.m_editWindow = m_password;
            } else {
                m_password->m_hasFocus = false;
                m_login->m_hasFocus = true;
                m_password->Invalidate();
                m_login->Invalidate();
                g_windowMgr.m_editWindow = m_login;
            }
        }
    } else if (virtualKey == VK_BACK)
    {
        // Pass-through events to the UIEditCtrl element
        if (m_login && m_login->m_hasFocus)
        {
            m_login->OnKeyDown(virtualKey);
        } else if (m_password && m_password->m_hasFocus)
        {
            m_password->OnKeyDown(virtualKey);
        }
    }
}
