#include "UILoginWnd.h"

#include "core/File.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "ui/UIWindowMgr.h"
#include "DebugLog.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

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

constexpr char kLoginRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";
constexpr char kRememberUserIdEnabledValue[] = "RememberUserIdEnabled";
constexpr char kRememberUserIdValue[] = "RememberUserId";

void StoreRememberedUserId(const char* userId, bool enabled)
{
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kLoginRegPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    const DWORD enabledValue = enabled ? 1u : 0u;
    RegSetValueExA(key,
        kRememberUserIdEnabledValue,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&enabledValue),
        sizeof(enabledValue));

    if (enabled && userId && *userId) {
        const DWORD userIdSize = static_cast<DWORD>(std::strlen(userId) + 1);
        RegSetValueExA(key,
            kRememberUserIdValue,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(userId),
            userIdSize);
    } else {
        RegDeleteValueA(key, kRememberUserIdValue);
    }

    RegCloseKey(key);
}

void LoadRememberedUserId(std::string* userId, bool* enabled)
{
    if (userId) {
        userId->clear();
    }
    if (enabled) {
        *enabled = false;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kLoginRegPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }

    DWORD enabledValue = 0;
    DWORD enabledSize = sizeof(enabledValue);
    if (enabled && RegQueryValueExA(
            key,
            kRememberUserIdEnabledValue,
            nullptr,
            nullptr,
            reinterpret_cast<BYTE*>(&enabledValue),
            &enabledSize) == ERROR_SUCCESS) {
        *enabled = (enabledValue != 0);
    }

    if (userId) {
        char buffer[64] = {};
        DWORD size = sizeof(buffer);
        if (RegQueryValueExA(
                key,
                kRememberUserIdValue,
                nullptr,
                nullptr,
                reinterpret_cast<BYTE*>(buffer),
                &size) == ERROR_SUCCESS) {
            *userId = buffer;
        }
    }

    RegCloseKey(key);
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
        "ad_title.jpg",
        "rag_title.jpg",
        "win_login.bmp",
        "title.bmp",
        "title.jpg",
        "login_background.jpg",
        "login_background.bmp",
        "loginwin.bmp",
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
        "",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "texture\\login_interface\\",
        "ui\\",
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

    for (const std::string& baseRaw : baseNames) {
        std::string base = NormalizeSlash(baseRaw);
        if (base.empty()) {
            continue;
        }

        AddUniqueCandidate(out, base);
        std::replace(base.begin(), base.end(), '\\', '/');
        AddUniqueCandidate(out, base);
        base = NormalizeSlash(base);

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

HBITMAP LoadFirstBitmapFromCandidates(const std::vector<std::string>& candidates, std::string* outPath)
{
    for (const std::string& candidate : candidates) {
        HBITMAP bmp = LoadBitmapFromGameData(candidate.c_str());
        if (bmp) {
            if (outPath) {
                *outPath = candidate;
            }
            return bmp;
        }
    }
    return nullptr;
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
      m_wallpaperBmp(nullptr),
      m_composeDC(nullptr),
      m_composeBitmap(nullptr),
      m_composeWidth(0),
      m_composeHeight(0),
      m_login(nullptr),
      m_password(nullptr),
      m_cancelButton(nullptr),
            m_saveAccountCheck(nullptr) {
    m_uiAssets.fill(nullptr);
}

UILoginWnd::~UILoginWnd()
{
    ReleaseComposeSurface();
    ClearUiAssets();
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

    constexpr int kLoginFieldWidth = 125;

    m_password = new UIEditCtrl();
    m_password->Create(kLoginFieldWidth, 18);
    m_password->m_maxchar = 24;
    m_password->Move(m_x + 92, m_y + 61);
    m_password->m_maskchar = '*';
    m_password->SetFrameColor(242, 242, 242);
    AddChild(m_password);

    m_login = new UIEditCtrl();
    m_login->Create(kLoginFieldWidth, 18);
    m_login->m_maxchar = 24;
    m_login->Move(m_x + 92, m_y + 29);
    m_login->SetFrameColor(242, 242, 242);
    AddChild(m_login);

    std::string rememberedUserId;
    bool rememberUserId = false;
    LoadRememberedUserId(&rememberedUserId, &rememberUserId);
    if (rememberUserId && !rememberedUserId.empty()) {
        m_login->SetText(rememberedUserId.c_str());
    }

    m_saveAccountCheck = new UICheckBox();
    m_saveAccountCheck->SetBitmap(
        ResolveUiAssetPath("chk_saveon.bmp").c_str(),
        ResolveUiAssetPath("chk_saveoff.bmp").c_str());
    m_saveAccountCheck->Create(m_saveAccountCheck->m_w > 0 ? m_saveAccountCheck->m_w : 16,
        m_saveAccountCheck->m_h > 0 ? m_saveAccountCheck->m_h : 16);
    m_saveAccountCheck->Move(m_x + 232, m_y + 33);
    m_saveAccountCheck->SetCheck(rememberUserId ? 1 : 0);
    AddChild(m_saveAccountCheck);

    const bool focusPassword = (m_login && m_login->GetText() && m_login->GetText()[0] != '\0');
    if (m_login) {
        m_login->m_hasFocus = !focusPassword;
    }
    if (m_password) {
        m_password->m_hasFocus = focusPassword;
    }
    g_windowMgr.m_editWindow = focusPassword ? static_cast<UIWindow*>(m_password) : static_cast<UIWindow*>(m_login);
}

void UILoginWnd::ClearUiAssets()
{
    if (m_wallpaperBmp) {
        DeleteObject(m_wallpaperBmp);
        m_wallpaperBmp = nullptr;
    }
    for (HBITMAP& bmp : m_uiAssets) {
        if (bmp) {
            DeleteObject(bmp);
            bmp = nullptr;
        }
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
    if (m_composeBitmap) {
        DeleteObject(m_composeBitmap);
        m_composeBitmap = nullptr;
    }
    if (m_composeDC) {
        DeleteDC(m_composeDC);
        m_composeDC = nullptr;
    }
    m_composeWidth = 0;
    m_composeHeight = 0;
}

bool UILoginWnd::EnsureComposeSurface(HDC referenceDC, int width, int height)
{
    if (!referenceDC || width <= 0 || height <= 0) {
        return false;
    }

    if (m_composeDC && m_composeBitmap && m_composeWidth == width && m_composeHeight == height) {
        return true;
    }

    ReleaseComposeSurface();

    m_composeDC = CreateCompatibleDC(referenceDC);
    if (!m_composeDC) {
        return false;
    }

    m_composeBitmap = CreateCompatibleBitmap(referenceDC, width, height);
    if (!m_composeBitmap) {
        ReleaseComposeSurface();
        return false;
    }

    SelectObject(m_composeDC, m_composeBitmap);
    m_composeWidth = width;
    m_composeHeight = height;
    return true;
}

void UILoginWnd::EnsureResourceCache()
{
    if (m_assetsProbed) {
        return;
    }
    m_assetsProbed = true;

    const std::vector<std::string> candidates = BuildWallpaperCandidates(m_requestedWallpaper);
    for (const std::string& candidate : candidates) {
        HBITMAP bmp = LoadBitmapFromGameData(candidate.c_str());
        if (bmp) {
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
    for (int i = 0; panelNames[i] && !m_uiAssets[UiPanel]; ++i) {
        m_uiAssets[UiPanel] = LoadFirstBitmapFromCandidates(
            BuildUiAssetCandidates(panelNames[i]), &m_uiAssetPaths[UiPanel]);
    }
    if (m_uiAssets[UiPanel]) {
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
    for (int i = 0; logoNames[i] && !m_uiAssets[UiLogo]; ++i) {
        m_uiAssets[UiLogo] = LoadFirstBitmapFromCandidates(
            BuildUiAssetCandidates(logoNames[i]), &m_uiAssetPaths[UiLogo]);
    }
    if (m_uiAssets[UiLogo]) {
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
    for (int i = 0; buttonNames[i] && !m_uiAssets[UiButton]; ++i) {
        m_uiAssets[UiButton] = LoadFirstBitmapFromCandidates(
            BuildUiAssetCandidates(buttonNames[i]), &m_uiAssetPaths[UiButton]);
    }
    if (m_uiAssets[UiButton]) {
        DbgLog("[LoginUIAsset] BUTTON HIT: %s\n", m_uiAssetPaths[UiButton].c_str());
    } else {
        DbgLog("[LoginUIAsset] BUTTON MISS\n");
    }

    const char* fieldNames[] = {
        "btn_edit.bmp",
        "btn_edit_a.bmp",
        nullptr
    };
    for (int i = 0; fieldNames[i] && !m_uiAssets[UiField]; ++i) {
        m_uiAssets[UiField] = LoadFirstBitmapFromCandidates(
            BuildUiAssetCandidates(fieldNames[i]), &m_uiAssetPaths[UiField]);
    }
    if (m_uiAssets[UiField]) {
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

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC targetDC = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!targetDC) {
        return;
    }

    RECT rcClient{};
    GetClientRect(g_hMainWnd, &rcClient);

    const int clientW = rcClient.right - rcClient.left;
    const int clientH = rcClient.bottom - rcClient.top;
    if (clientW <= 0 || clientH <= 0) {
        if (!useShared) {
            ReleaseDC(g_hMainWnd, targetDC);
        }
        return;
    }

    if (!m_controlsCreated) {
        OnCreate(clientW, clientH);
    }

    HDC drawDC = targetDC;
    const bool useCompose = EnsureComposeSurface(targetDC, clientW, clientH);
    if (useCompose) {
        drawDC = m_composeDC;
    }

    if (m_wallpaperBmp) {
        DrawBitmapStretched(drawDC, m_wallpaperBmp, rcClient);
    }

    BITMAP bm{};
    RECT rcPanel = { m_x, m_y, m_x + m_w, m_y + m_h };
    if (m_uiAssets[UiPanel]) {
        if (GetObjectA(m_uiAssets[UiPanel], sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            rcPanel.right = rcPanel.left + bm.bmWidth;
            rcPanel.bottom = rcPanel.top + bm.bmHeight;
        }
        DrawBitmapStretched(drawDC, m_uiAssets[UiPanel], rcPanel);
    } else {
        HBRUSH panelBg = CreateSolidBrush(RGB(235, 235, 228));
        FillRect(drawDC, &rcPanel, panelBg);
        DeleteObject(panelBg);
        FrameRect(drawDC, &rcPanel, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    HDC prevSharedDC = UIWindow::GetSharedDrawDC();
    UIWindow::SetSharedDrawDC(drawDC);
    DrawChildren();
    UIWindow::SetSharedDrawDC(prevSharedDC);

    if (useCompose) {
        BitBlt(targetDC, 0, 0, clientW, clientH, drawDC, 0, 0, SRCCOPY);
    }

    if (!useShared) {
        ReleaseDC(g_hMainWnd, targetDC);
    }
}

int UILoginWnd::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra)
{
    (void)lparam;

    // Message 6 = button click (Ref pattern: val1/wparam = button ID)
    if (msg == 6) {
        switch (wparam) {
        case 120: // connect
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                m_saveAccountCheck && m_saveAccountCheck->m_isChecked != 0);
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetPassword,
                reinterpret_cast<int>(m_password ? m_password->GetText() : ""), 0, 0);
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetUserId,
                reinterpret_cast<int>(m_login ? m_login->GetText() : ""), 0, 0);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestConnect, 0, 0, 0);

        case 119: // cancel
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_ReturnToLogin, 0, 0, 0);

        case 155: // exit
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Quit, 0, 0, 0);

        case 201: // request
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestAccount, 0, 0, 0);

        case 219: // intro
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Intro, 0, 0, 0);

        default:
            break;
        }

        // Check if it's from the save-account checkbox (its m_id may not be a well-known value)
        if (sender == m_saveAccountCheck) {
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                m_saveAccountCheck->m_isChecked != 0);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_SaveAccount,
                m_saveAccountCheck->m_isChecked, 0, m_saveAccountCheck->m_isChecked);
        }
    }

    // For legacy routing: also check sender->m_id in case called with msg != 6
    if (msg != 6 && sender) {
        const int senderId = sender->m_id;
        switch (senderId) {
        case 120:
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                m_saveAccountCheck && m_saveAccountCheck->m_isChecked != 0);
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetPassword,
                reinterpret_cast<int>(m_password ? m_password->GetText() : ""), 0, 0);
            g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetUserId,
                reinterpret_cast<int>(m_login ? m_login->GetText() : ""), 0, 0);
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
            StoreRememberedUserId(
                m_login ? m_login->GetText() : "",
                m_saveAccountCheck->m_isChecked != 0);
            return g_modeMgr.SendMsg(CLoginMode::LoginMsg_SaveAccount,
                m_saveAccountCheck->m_isChecked, extra, m_saveAccountCheck->m_isChecked);
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
                // Update UIWindowMgr focused edit
                g_windowMgr.m_editWindow = m_password;
            } else {
                m_password->m_hasFocus = false;
                m_login->m_hasFocus = true;
                g_windowMgr.m_editWindow = m_login;
            }
        }
    }
}
