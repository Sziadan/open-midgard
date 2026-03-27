#include "UIWindowMgr.h"
#include "UILoadingWnd.h"
#include "UINewChatWnd.h"
#include "UIBasicInfoWnd.h"
#include "UIChooseWnd.h"
#include "UIEquipWnd.h"
#include "UIItemWnd.h"
#include "UILoginWnd.h"
#include "UISelectServerWnd.h"
#include "UIMakeCharWnd.h"
#include "UINotifyLevelUpWnd.h"
#include "UIOptionWnd.h"
#include "UISelectCharWnd.h"
#include "UIWaitWnd.h"
#include "gamemode/CursorRenderer.h"

#include "core/File.h"
#include "res/Bitmap.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "res/Texture.h"
#include "render/Renderer.h"
#include "main/WinMain.h"
#include "DebugLog.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <vector>

namespace {
constexpr int kUiChatEventMsg = 101;
constexpr size_t kMaxChatEvents = 256;
constexpr bool kLogWallpaperLoad = false;

#define LOG_WALLPAPER_LOAD(...) do { if constexpr (kLogWallpaperLoad) { DbgLog(__VA_ARGS__); } } while (0)

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

std::vector<std::string> BuildWallpaperCandidates(const std::string& requestedWallpaper)
{
    std::vector<std::string> out;

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
        AddUniqueCandidate(out, base);

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

bool HasDirtyWindowRecursive(UIWindow* window)
{
    if (!window || window->m_show == 0) {
        return false;
    }
    if (window->IsUpdateNeed()) {
        return true;
    }
    for (UIWindow* child : window->m_children) {
        if (HasDirtyWindowRecursive(child)) {
            return true;
        }
    }
    return false;
}

void ClearDirtyWindowRecursive(UIWindow* window)
{
    if (!window) {
        return;
    }
    window->m_isDirty = 0;
    for (UIWindow* child : window->m_children) {
        ClearDirtyWindowRecursive(child);
    }
}
}

UIWindowMgr g_windowMgr;

UIWindowMgr::UIWindowMgr() 
    : m_chatWndX(0), m_chatWndY(0), m_chatWndHeight(0), m_chatWndShow(0),
      m_gronMsnWndShow(0), m_gronMsgWndShow(0), m_chatWndStatus(0),
      m_miniMapZoomFactor(1.0f), m_miniMapArgb(0), m_isDrawCompass(0),
      m_isDragAll(0), m_conversionMode(0),
      m_captureWindow(nullptr), m_editWindow(nullptr), m_modalWindow(nullptr), m_lastHitWindow(nullptr),
      m_loadingWnd(nullptr), m_minimapZoomWnd(nullptr), m_statusWnd(nullptr), m_chatWnd(nullptr),
    m_loginWnd(nullptr), m_selectServerWnd(nullptr), m_selectCharWnd(nullptr), m_makeCharWnd(nullptr), m_chooseWnd(nullptr), m_optionWnd(nullptr), m_itemWnd(nullptr), m_questWnd(nullptr), m_basicInfoWnd(nullptr), m_notifyLevelUpWnd(nullptr), m_notifyJobLevelUpWnd(nullptr), m_equipWnd(nullptr),
      m_wallpaperSurface(nullptr), m_uiComposeDC(nullptr), m_uiComposeBitmap(nullptr), m_uiComposeBits(nullptr), m_uiComposeWidth(0), m_uiComposeHeight(0),
      m_composeCursorActNum(0), m_composeCursorStartTick(0), m_composeCursorEnabled(false)
{
    m_loginStatus = "Login: idle";
}

UIWindowMgr::~UIWindowMgr() {
    Reset();
}

void UIWindowMgr::ReleaseComposeSurface()
{
    if (m_uiComposeBitmap) {
        DeleteObject(m_uiComposeBitmap);
        m_uiComposeBitmap = nullptr;
    }
    if (m_uiComposeDC) {
        DeleteDC(m_uiComposeDC);
        m_uiComposeDC = nullptr;
    }
    m_uiComposeWidth = 0;
    m_uiComposeHeight = 0;
    m_uiComposeBits = nullptr;
}

bool UIWindowMgr::EnsureComposeSurface(HDC referenceDC, int width, int height)
{
    if (!referenceDC || width <= 0 || height <= 0) {
        return false;
    }

    if (m_uiComposeDC && m_uiComposeBitmap && m_uiComposeWidth == width && m_uiComposeHeight == height) {
        return true;
    }

    ReleaseComposeSurface();

    m_uiComposeDC = CreateCompatibleDC(referenceDC);
    if (!m_uiComposeDC) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    m_uiComposeBitmap = CreateDIBSection(referenceDC, &bmi, DIB_RGB_COLORS, &m_uiComposeBits, nullptr, 0);
    if (!m_uiComposeBitmap || !m_uiComposeBits) {
        ReleaseComposeSurface();
        return false;
    }

    SelectObject(m_uiComposeDC, m_uiComposeBitmap);
    m_uiComposeWidth = width;
    m_uiComposeHeight = height;
    return true;
}

bool UIWindowMgr::Init() {
    if (!m_loginWnd) {
        m_loginWnd = new UILoginWnd();
        if (!m_loginWallpaper.empty()) {
            m_loginWnd->SetWallpaperName(m_loginWallpaper);
        }
        m_children.push_back(m_loginWnd);
    }

    if (!m_chatWnd) {
        m_chatWnd = new UINewChatWnd();
        m_children.push_back(m_chatWnd);
    }
    return true;
}

UIWindow* UIWindowMgr::MakeWindow(int windowId)
{
    switch (windowId) {
    case WID_BASICINFOWND:
        if (!m_basicInfoWnd) {
            m_basicInfoWnd = new UIBasicInfoWnd();
            m_children.push_back(m_basicInfoWnd);
        }
        m_children.remove(m_basicInfoWnd);
        m_children.push_front(m_basicInfoWnd);
        m_basicInfoWnd->SetShow(1);
        return m_basicInfoWnd;

    case WID_NOTIFYLEVELUPWND:
        if (!m_notifyLevelUpWnd) {
            m_notifyLevelUpWnd = new UINotifyLevelUpWnd();
            m_children.push_back(m_notifyLevelUpWnd);
        }
        m_children.remove(m_notifyLevelUpWnd);
        m_children.push_back(m_notifyLevelUpWnd);
        m_notifyLevelUpWnd->SetShow(1);
        return m_notifyLevelUpWnd;

    case WID_NOTIFYJOBLEVELUPWND:
        if (!m_notifyJobLevelUpWnd) {
            m_notifyJobLevelUpWnd = new UINotifyJobLevelUpWnd();
            m_children.push_back(m_notifyJobLevelUpWnd);
        }
        m_children.remove(m_notifyJobLevelUpWnd);
        m_children.push_back(m_notifyJobLevelUpWnd);
        m_notifyJobLevelUpWnd->SetShow(1);
        return m_notifyJobLevelUpWnd;

    case WID_ITEMWND:
        if (!m_itemWnd) {
            m_itemWnd = new UIItemWnd();
            m_children.push_back(m_itemWnd);
        }
        m_children.remove(m_itemWnd);
        m_children.push_back(m_itemWnd);
        m_itemWnd->SetShow(1);
        return m_itemWnd;

    case WID_EQUIPWND:
        if (!m_equipWnd) {
            m_equipWnd = new UIEquipWnd();
            m_children.push_back(m_equipWnd);
        }
        m_children.remove(m_equipWnd);
        m_children.push_back(m_equipWnd);
        m_equipWnd->SetShow(1);
        return m_equipWnd;

    case WID_LOGINWND:
        if (!m_loginWnd) {
            m_loginWnd = new UILoginWnd();
            if (!m_loginWallpaper.empty()) {
                m_loginWnd->SetWallpaperName(m_loginWallpaper);
            }
            m_children.push_back(m_loginWnd);
        }
        m_loginWnd->SetShow(1);
        return m_loginWnd;

    case WID_SELECTSERVERWND:
        if (!m_selectServerWnd) {
            m_selectServerWnd = new UISelectServerWnd();
            m_children.push_back(m_selectServerWnd);
        }
        m_children.remove(m_selectServerWnd);
        m_children.push_back(m_selectServerWnd);
        m_selectServerWnd->SetShow(1);
        return m_selectServerWnd;

    case WID_WAITWND: {
        UIWaitWnd* waitWnd = new UIWaitWnd();
        waitWnd->SetShow(1);
        m_children.push_back(waitWnd);
        return waitWnd;
    }

    case WID_LOADINGWND:
        if (!m_loadingWnd) {
            m_loadingWnd = new UILoadingWnd();
            m_children.push_back(m_loadingWnd);
        }
        m_loadingWnd->SetShow(1);
        return m_loadingWnd;

    case WID_SELECTCHARWND:
        if (!m_selectCharWnd) {
            m_selectCharWnd = new UISelectCharWnd();
            m_children.push_back(m_selectCharWnd);
        }
        m_selectCharWnd->SetShow(1);
        return m_selectCharWnd;

    case WID_MAKECHARWND:
        if (!m_makeCharWnd) {
            m_makeCharWnd = new UIMakeCharWnd();
            m_children.push_back(m_makeCharWnd);
        }
        m_makeCharWnd->SetShow(1);
        return m_makeCharWnd;

    case WID_SAYDIALOGWND:
        if (!m_chatWnd) {
            m_chatWnd = new UINewChatWnd();
            m_children.push_back(m_chatWnd);
        }
        m_chatWnd->SetShow(1);
        return m_chatWnd;

    case WID_CHOOSEWND:
        if (!m_chooseWnd) {
            m_chooseWnd = new UIChooseWnd();
            m_children.push_back(m_chooseWnd);
        }
        m_children.remove(m_chooseWnd);
        m_children.push_back(m_chooseWnd);
        m_chooseWnd->SetShow(1);
        return m_chooseWnd;

    case WID_OPTIONWND:
        if (!m_optionWnd) {
            m_optionWnd = new UIOptionWnd();
            m_children.push_back(m_optionWnd);
        }
        m_children.remove(m_optionWnd);
        m_children.push_back(m_optionWnd);
        m_optionWnd->SetShow(1);
        return m_optionWnd;

    default:
        return nullptr;
    }
}

void UIWindowMgr::DeleteWindow(UIWindow* window)
{
    if (!window) {
        return;
    }

    if (m_captureWindow == window) {
        m_captureWindow = nullptr;
    }
    if (m_editWindow == window) {
        m_editWindow = nullptr;
    }
    if (m_modalWindow == window) {
        m_modalWindow = nullptr;
    }
    if (m_lastHitWindow == window) {
        m_lastHitWindow = nullptr;
    }

    m_children.remove(window);

    if (window == m_loginWnd) {
        m_loginWnd = nullptr;
    }
    if (window == m_selectServerWnd) {
        m_selectServerWnd = nullptr;
    }
    if (window == m_selectCharWnd) {
        m_selectCharWnd = nullptr;
    }
    if (window == m_makeCharWnd) {
        m_makeCharWnd = nullptr;
    }
    if (window == m_chooseWnd) {
        m_chooseWnd = nullptr;
    }
    if (window == m_optionWnd) {
        m_optionWnd = nullptr;
    }
    if (window == m_loadingWnd) {
        m_loadingWnd = nullptr;
    }
    if (window == m_chatWnd) {
        m_chatWnd = nullptr;
    }
    if (window == m_basicInfoWnd) {
        m_basicInfoWnd = nullptr;
    }
    if (window == m_notifyLevelUpWnd) {
        m_notifyLevelUpWnd = nullptr;
    }
    if (window == m_notifyJobLevelUpWnd) {
        m_notifyJobLevelUpWnd = nullptr;
    }
    if (window == m_itemWnd) {
        m_itemWnd = nullptr;
    }
    if (window == m_equipWnd) {
        m_equipWnd = nullptr;
    }

    delete window;
}

void UIWindowMgr::RemoveAllWindows()
{
    while (!m_children.empty()) {
        DeleteWindow(m_children.front());
    }

    m_captureWindow = nullptr;
    m_editWindow = nullptr;
    m_modalWindow = nullptr;
    m_lastHitWindow = nullptr;

    m_loadingWnd = nullptr;
    m_minimapZoomWnd = nullptr;
    m_statusWnd = nullptr;
    m_chatWnd = nullptr;
    m_basicInfoWnd = nullptr;
    m_notifyLevelUpWnd = nullptr;
    m_notifyJobLevelUpWnd = nullptr;
    m_loginWnd = nullptr;
    m_selectServerWnd = nullptr;
    m_selectCharWnd = nullptr;
    m_makeCharWnd = nullptr;
    m_chooseWnd = nullptr;
    m_optionWnd = nullptr;
    m_itemWnd = nullptr;
    m_questWnd = nullptr;
    m_basicInfoWnd = nullptr;
    m_equipWnd = nullptr;
}

void UIWindowMgr::Reset() {
    RemoveAllWindows();
    m_loginWnd = nullptr;
    m_selectCharWnd = nullptr;
    m_makeCharWnd = nullptr;
    m_chooseWnd = nullptr;
    m_optionWnd = nullptr;
    m_chatWnd = nullptr;
    m_chatEvents.clear();

    if (m_wallpaperSurface) {
        delete m_wallpaperSurface;
        m_wallpaperSurface = nullptr;
    }
    ReleaseComposeSurface();
    m_loadedWallpaperPath.clear();
}

void UIWindowMgr::OnProcess() {
    for (auto child : m_children) {
        child->OnProcess();
    }
}

bool UIWindowMgr::HasDirtyVisualState() const
{
    for (UIWindow* child : const_cast<UIWindowMgr*>(this)->m_children) {
        if (HasDirtyWindowRecursive(child)) {
            return true;
        }
    }
    return false;
}

void UIWindowMgr::OnDraw() {
    if (!g_hMainWnd) {
        return;
    }

    if (HDC sharedDC = UIWindow::GetSharedDrawDC()) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        for (auto child : m_children) {
            if (child && child->m_show != 0) {
                child->OnDraw();
            }
        }
        if (m_itemWnd && m_itemWnd->m_show != 0) {
            m_itemWnd->DrawHoverOverlay(sharedDC, clientRect);
        }
        for (UIWindow* child : m_children) {
            ClearDirtyWindowRecursive(child);
        }
        return;
    }

    const bool hasMenuUi =
        (m_loginWnd && m_loginWnd->m_show != 0) ||
        (m_selectCharWnd && m_selectCharWnd->m_show != 0) ||
        (m_makeCharWnd && m_makeCharWnd->m_show != 0) ||
        (m_loadingWnd && m_loadingWnd->m_show != 0);

    if (!hasMenuUi && GetRenderDevice().GetLegacyDevice() != nullptr) {
        HDC backBufferDC = nullptr;
        if (GetRenderDevice().AcquireBackBufferDC(&backBufferDC) && backBufferDC) {
            RECT clientRect{};
            GetClientRect(g_hMainWnd, &clientRect);
            HDC previousSharedDC = UIWindow::GetSharedDrawDC();
            UIWindow::SetSharedDrawDC(backBufferDC);
            for (auto child : m_children) {
                if (child && child->m_show != 0) {
                    child->OnDraw();
                }
            }
            if (m_itemWnd && m_itemWnd->m_show != 0) {
                m_itemWnd->DrawHoverOverlay(backBufferDC, clientRect);
            }
            UIWindow::SetSharedDrawDC(previousSharedDC);
            GetRenderDevice().ReleaseBackBufferDC(backBufferDC);
            for (UIWindow* child : m_children) {
                ClearDirtyWindowRecursive(child);
            }
            return;
        }
    }

    HDC targetDC = GetDC(g_hMainWnd);
    if (!targetDC) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        ReleaseDC(g_hMainWnd, targetDC);
        return;
    }

    HDC drawDC = targetDC;
    const bool useCompose = EnsureComposeSurface(targetDC, clientWidth, clientHeight);
    if (useCompose) {
        drawDC = m_uiComposeDC;
        if (m_wallpaperSurface && m_wallpaperSurface->m_hBitmap) {
            DrawWallpaperToDC(drawDC, clientWidth, clientHeight);
        } else {
            HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(drawDC, &clientRect, clearBrush);
            DeleteObject(clearBrush);
        }
    }

    HDC previousSharedDC = UIWindow::GetSharedDrawDC();
    UIWindow::SetSharedDrawDC(drawDC);
    for (auto child : m_children) {
        if (child && child->m_show != 0) {
            child->OnDraw();
        }
    }
    if (m_itemWnd && m_itemWnd->m_show != 0) {
        m_itemWnd->DrawHoverOverlay(drawDC, clientRect);
    }
    UIWindow::SetSharedDrawDC(previousSharedDC);
    if (useCompose && m_composeCursorEnabled) {
        DrawModeCursorToHdc(drawDC, m_composeCursorActNum, m_composeCursorStartTick);
    }
    const bool hasModernBackend = GetRenderDevice().GetLegacyDevice() == nullptr;
    const bool allowModernUiPresent = hasModernBackend
        && GetRenderDevice().GetBackendType() != RenderBackendType::Vulkan;
    bool presentedModernUiFrame = false;
    if (useCompose && allowModernUiPresent && m_uiComposeBits) {
        presentedModernUiFrame = GetRenderDevice().UpdateBackBufferFromMemory(
            m_uiComposeBits,
            clientWidth,
            clientHeight,
            clientWidth * static_cast<int>(sizeof(unsigned int)));
        if (presentedModernUiFrame) {
            g_renderer.Flip(false);
        }
    }

    if (useCompose) {
        if (!presentedModernUiFrame) {
            BitBlt(targetDC, 0, 0, clientWidth, clientHeight, drawDC, 0, 0, SRCCOPY);
        }
    }

    for (UIWindow* child : m_children) {
        ClearDirtyWindowRecursive(child);
    }

    ReleaseDC(g_hMainWnd, targetDC);
}

void UIWindowMgr::RenderWallPaper() {
    if (!m_wallpaperSurface || !g_hMainWnd) {
        return;
    }

    RECT rc{};
    GetClientRect(g_hMainWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        return;
    }

    m_wallpaperSurface->DrawSurfaceStretch(0, 0, w, h);
}

void UIWindowMgr::DrawWallpaperToDC(HDC targetDC, int width, int height) {
    if (!targetDC || !m_wallpaperSurface || !m_wallpaperSurface->m_hBitmap || width <= 0 || height <= 0) {
        return;
    }

    BITMAP bm{};
    if (!GetObjectA(m_wallpaperSurface->m_hBitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return;
    }

    HDC srcDC = CreateCompatibleDC(targetDC);
    if (!srcDC) {
        return;
    }

    HGDIOBJ old = SelectObject(srcDC, m_wallpaperSurface->m_hBitmap);
    SetStretchBltMode(targetDC, HALFTONE);
    StretchBlt(targetDC, 0, 0, width, height,
               srcDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    SelectObject(srcDC, old);
    DeleteDC(srcDC);
}

void UIWindowMgr::SetWallpaper(CBitmapRes* bitmap) {
    if (!bitmap || !bitmap->m_data || bitmap->m_width <= 0 || bitmap->m_height <= 0) {
        DbgLog("[SetWallpaper] Called with null/empty bitmap, clearing surface\n");
        if (m_wallpaperSurface) {
            delete m_wallpaperSurface;
            m_wallpaperSurface = nullptr;
        }
        return;
    }

    if (!m_wallpaperSurface) {
        m_wallpaperSurface = g_3dDevice.CreateWallPaper(bitmap->m_width, bitmap->m_height);
        DbgLog("[SetWallpaper] CreateWallPaper(%d,%d) -> %p\n",
               bitmap->m_width, bitmap->m_height, (void*)m_wallpaperSurface);
    }

    if (!m_wallpaperSurface) {
        DbgLog("[SetWallpaper] FAIL: surface is null after CreateWallPaper\n");
        return;
    }

    DbgLog("[SetWallpaper] Calling Update with %dx%d pixels\n",
           bitmap->m_width, bitmap->m_height);
    m_wallpaperSurface->Update(0, 0, bitmap->m_width, bitmap->m_height, bitmap->m_data, false, bitmap->m_width * 4);
    DbgLog("[SetWallpaper] Update done, m_hBitmap=%p\n", (void*)m_wallpaperSurface);
}

bool UIWindowMgr::SetWallpaperFromGameData(const std::string& wallpaperName) {
    const std::vector<std::string> candidates = BuildWallpaperCandidates(wallpaperName);
    LOG_WALLPAPER_LOAD("[WallpaperLoad] Searching for wallpaper '%s', %zu candidates\n",
           wallpaperName.c_str(), candidates.size());

    for (const std::string& candidate : candidates) {
        int size = 0;
        unsigned char* bytes = g_fileMgr.GetData(candidate.c_str(), &size);
        if (!bytes || size <= 0) {
            LOG_WALLPAPER_LOAD("[WallpaperLoad]   MISS: %s\n", candidate.c_str());
            delete[] bytes;
            continue;
        }
        LOG_WALLPAPER_LOAD("[WallpaperLoad]   HIT:  %s (%d bytes)\n", candidate.c_str(), size);

        CBitmapRes bitmap;
        const bool loaded = bitmap.LoadFromBuffer(candidate.c_str(), bytes, size);
        delete[] bytes;

        if (!loaded || !bitmap.m_data || bitmap.m_width <= 0 || bitmap.m_height <= 0) {
                 LOG_WALLPAPER_LOAD("[WallpaperLoad]   DECODE FAIL: loaded=%d w=%d h=%d\n",
                   loaded, bitmap.m_width, bitmap.m_height);
            continue;
        }
        LOG_WALLPAPER_LOAD("[WallpaperLoad]   Decoded OK: %dx%d\n", bitmap.m_width, bitmap.m_height);

        SetWallpaper(&bitmap);
        m_loadedWallpaperPath = candidate;
        LOG_WALLPAPER_LOAD("[WallpaperLoad] SUCCESS: loaded '%s'\n", candidate.c_str());
        return true;
    }

    LOG_WALLPAPER_LOAD("[WallpaperLoad] FAIL: no candidate resolved (tried %zu)\n", candidates.size());
    m_loadedWallpaperPath.clear();
    SetWallpaper(nullptr);
    return false;
}

void UIWindowMgr::ShowLoadingScreen(const std::string& wallpaperName, const std::string& message, float progress)
{
    const bool loadingAlreadyVisible = m_loadingWnd && m_loadingWnd->m_show != 0;
    if (!wallpaperName.empty() && !loadingAlreadyVisible) {
        SetWallpaperFromGameData(wallpaperName);
    }

    UILoadingWnd* loadingWnd = m_loadingWnd ? m_loadingWnd : static_cast<UILoadingWnd*>(MakeWindow(WID_LOADINGWND));
    if (!loadingWnd) {
        return;
    }

    loadingWnd->SetMessage(message);
    loadingWnd->SetProgress(progress);
    loadingWnd->SetShow(1);
}

void UIWindowMgr::UpdateLoadingScreen(const std::string& message, float progress)
{
    if (!m_loadingWnd) {
        return;
    }

    m_loadingWnd->SetMessage(message);
    m_loadingWnd->SetProgress(progress);
    m_loadingWnd->SetShow(1);
}

void UIWindowMgr::HideLoadingScreen()
{
    if (m_loadingWnd) {
        m_loadingWnd->SetShow(0);
    }
    m_loadedWallpaperPath.clear();
    SetWallpaper(nullptr);
}

void UIWindowMgr::SetComposeCursorState(int cursorActNum, u32 mouseAnimStartTick, bool enabled)
{
    m_composeCursorActNum = cursorActNum;
    m_composeCursorStartTick = mouseAnimStartTick;
    m_composeCursorEnabled = enabled;
}

void UIWindowMgr::SendMsg(int msg, int wparam, int lparam) {
    if (msg != kUiChatEventMsg || wparam == 0) {
        return;
    }

    const char* text = reinterpret_cast<const char*>(wparam);
    if (!text || *text == '\0') {
        return;
    }

    UIChatEvent event{};
    event.text = text;
    event.color = static_cast<u32>(lparam) & 0x00FFFFFFu;
    event.channel = static_cast<u8>((static_cast<u32>(lparam) >> 24) & 0xFFu);
    event.tick = GetTickCount();

    if (m_chatEvents.size() >= kMaxChatEvents) {
        m_chatEvents.erase(m_chatEvents.begin());
    }
    m_chatEvents.push_back(std::move(event));

    if (m_chatWnd) {
        m_chatWnd->AddChatLine(text, static_cast<u32>(lparam) & 0x00FFFFFFu,
            static_cast<u8>((static_cast<u32>(lparam) >> 24) & 0xFFu), GetTickCount());
    }
}

void UIWindowMgr::OnLBtnDown(int x, int y)
{
    UIWindow* hit = HitTestWindow(x, y);

    m_captureWindow = hit;

    if (hit) {
        UIWindow* topLevel = hit;
        while (topLevel && topLevel->m_parent) {
            topLevel = topLevel->m_parent;
        }
        if (topLevel) {
            auto found = std::find(m_children.begin(), m_children.end(), topLevel);
            if (found != m_children.end() && std::next(found) != m_children.end()) {
                m_children.erase(found);
                m_children.push_back(topLevel);
            }
        }

        // Track focused edit control
        if (hit->CanReceiveKeyInput()) {
            m_editWindow = hit;
        }
        hit->OnLBtnDown(x, y);
    }
}

void UIWindowMgr::OnLBtnDblClk(int x, int y)
{
    UIWindow* hit = HitTestWindow(x, y);
    if (!hit) {
        return;
    }

    UIWindow* topLevel = hit;
    while (topLevel && topLevel->m_parent) {
        topLevel = topLevel->m_parent;
    }
    if (topLevel) {
        auto found = std::find(m_children.begin(), m_children.end(), topLevel);
        if (found != m_children.end() && std::next(found) != m_children.end()) {
            m_children.erase(found);
            m_children.push_back(topLevel);
        }
    }

    if (hit->CanReceiveKeyInput()) {
        m_editWindow = hit;
    }
    hit->OnLBtnDblClk(x, y);
}

void UIWindowMgr::OnLBtnUp(int x, int y)
{
    if (m_captureWindow) {
        m_captureWindow->OnLBtnUp(x, y);
        m_captureWindow = nullptr;
    }
}

void UIWindowMgr::OnMouseMove(int x, int y)
{
    if (m_captureWindow) {
        m_captureWindow->OnMouseMove(x, y);
        return;
    }

    UIWindow* hit = HitTestWindow(x, y);

    if (m_lastHitWindow != hit) {
        if (m_lastHitWindow) {
            m_lastHitWindow->OnMouseHover(-1, -1);
        }
        if (hit) {
            hit->OnMouseHover(x, y);
        }
        m_lastHitWindow = hit;
    }

    if (hit) {
        hit->OnMouseMove(x, y);
    }
}

UIWindow* UIWindowMgr::HitTestWindow(int x, int y) const
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        UIWindow* hit = (*it)->HitTestDeep(x, y);
        if (hit) {
            return hit;
        }
    }

    return nullptr;
}

bool UIWindowMgr::HasWindowAtPoint(int x, int y) const
{
    return HitTestWindow(x, y) != nullptr;
}

void UIWindowMgr::OnChar(char c)
{
    if (c == '\r' || c == '\n') {
        DbgLog("[UI] char c=0x%02X chat=%p login=%p edit=%p\n",
            static_cast<unsigned char>(c),
            static_cast<void*>(m_chatWnd),
            static_cast<void*>(m_loginWnd),
            static_cast<void*>(m_editWindow));
    }

    if (m_chatWnd && m_chatWnd->m_show != 0 && m_chatWnd->HandleChar(c)) {
        return;
    }

    if (m_editWindow) {
        m_editWindow->OnChar(c);
    }
}

void UIWindowMgr::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_RETURN || virtualKey == VK_ESCAPE) {
        DbgLog("[UI] keydown vk=%d chat=%p show=%d login=%p select=%p make=%p edit=%p\n",
            virtualKey,
            static_cast<void*>(m_chatWnd),
            (m_chatWnd ? m_chatWnd->m_show : 0),
            static_cast<void*>(m_loginWnd),
            static_cast<void*>(m_selectCharWnd),
            static_cast<void*>(m_makeCharWnd),
            static_cast<void*>(m_editWindow));
    }

    if (m_chatWnd && m_chatWnd->m_show != 0 && m_chatWnd->HandleKeyDown(virtualKey)) {
        if (virtualKey == VK_RETURN || virtualKey == VK_ESCAPE) {
            DbgLog("[UI] keydown consumed by chat vk=%d\n", virtualKey);
        }
        return;
    }

    if (m_chooseWnd && m_chooseWnd->m_show != 0) {
        m_chooseWnd->OnKeyDown(virtualKey);
        return;
    }

    if (m_selectServerWnd && m_selectServerWnd->m_show != 0
        && (virtualKey == VK_UP || virtualKey == VK_DOWN)) {
        m_selectServerWnd->OnKeyDown(virtualKey);
        return;
    }

    if (m_optionWnd && m_optionWnd->m_show != 0) {
        m_optionWnd->OnKeyDown(virtualKey);
        return;
    }

    const bool hasFrontMenuUi =
        (m_loginWnd && m_loginWnd->m_show != 0) ||
        (m_selectCharWnd && m_selectCharWnd->m_show != 0) ||
        (m_makeCharWnd && m_makeCharWnd->m_show != 0);

    if (virtualKey == VK_ESCAPE && !hasFrontMenuUi) {
        UIWindow* chooseWnd = MakeWindow(WID_CHOOSEWND);
        if (chooseWnd) {
            chooseWnd->SetShow(1);
        }
        return;
    }

    // Route Enter/Escape to the login window or the topmost frame window
    if (m_loginWnd && m_loginWnd->m_show != 0) {
        m_loginWnd->OnKeyDown(virtualKey);
    } else if (m_selectCharWnd && m_selectCharWnd->m_show != 0) {
        m_selectCharWnd->OnKeyDown(virtualKey);
    } else if (m_makeCharWnd && m_makeCharWnd->m_show != 0) {
        m_makeCharWnd->OnKeyDown(virtualKey);
    }
}

void UIWindowMgr::SetLoginStatus(const std::string& status) {
    if (status.empty()) {
        return;
    }

    m_loginStatus = status;
}

void UIWindowMgr::SetLoginWallpaper(const std::string& wallpaperName) {
    m_loginWallpaper = wallpaperName;
    SetWallpaperFromGameData(wallpaperName);

    if (m_loginWnd) {
        m_loginWnd->SetWallpaperName(wallpaperName);
    }
}

const std::string& UIWindowMgr::GetLoginStatus() const {
    return m_loginStatus;
}

const std::vector<UIChatEvent>& UIWindowMgr::GetChatEvents() const {
    return m_chatEvents;
}

std::vector<UIChatEvent> UIWindowMgr::GetChatPreviewEvents(size_t maxCount) const {
    std::vector<UIChatEvent> preview;
    if (!m_chatWnd || maxCount == 0) {
        return preview;
    }

    const auto& visible = m_chatWnd->GetVisibleLines();
    const size_t start = visible.size() > maxCount ? (visible.size() - maxCount) : 0;
    preview.reserve(visible.size() - start);

    for (size_t i = start; i < visible.size(); ++i) {
        UIChatEvent ev{};
        ev.text = visible[i].text;
        ev.color = visible[i].color;
        ev.channel = visible[i].channel;
        ev.tick = visible[i].tick;
        preview.push_back(std::move(ev));
    }
    return preview;
}

void UIWindowMgr::ClearChatEvents() {
    m_chatEvents.clear();
    if (m_chatWnd) {
        m_chatWnd->ClearLines();
    }
}
