#include "UIWindow.h"

#include "audio/Audio.h"
#include "core/File.h"
#include "DebugLog.h"
#include "main/WinMain.h"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstring>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

namespace {

HDC g_sharedUiDrawDC = nullptr;
constexpr char kUiWindowRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";

std::string ToLowerAsciiUi(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeSlashUi(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

void ClampWindowPositionToClient(int* x, int* y, int w, int h)
{
    if (!x || !y) {
        return;
    }

    RECT clientRect{ 0, 0, 640, 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }

    const int minX = static_cast<int>(clientRect.left);
    const int minY = static_cast<int>(clientRect.top);
    const int clientRight = static_cast<int>(clientRect.right);
    const int clientBottom = static_cast<int>(clientRect.bottom);
    const int maxX = std::max(minX, clientRight - std::max(1, w));
    const int maxY = std::max(minY, clientBottom - std::max(17, std::min(h, 64)));
    *x = std::clamp(*x, minX, maxX);
    *y = std::clamp(*y, minY, maxY);
}

bool ContainsUiToken(const std::string& lowered, const char* token)
{
    return lowered.find(token) != std::string::npos;
}

int ScoreUiButtonSoundCandidate(const std::string& path)
{
    const std::string lowered = ToLowerAsciiUi(NormalizeSlashUi(path));
    if (lowered.size() < 4 || lowered.substr(lowered.size() - 4) != ".wav") {
        return -1000000;
    }

    const size_t slashPos = lowered.find_last_of('\\');
    const std::string fileName = slashPos == std::string::npos ? lowered : lowered.substr(slashPos + 1);

    int score = 0;
    if (ContainsUiToken(lowered, "\\wav\\")) {
        score += 80;
    }
    if (ContainsUiToken(lowered, "interface") || ContainsUiToken(lowered, "login")) {
        score += 80;
    }
    if (fileName == "click.wav") {
        score += 1000;
    }
    if (fileName == "button.wav") {
        score += 900;
    }
    if (fileName == "btnok.wav" || fileName == "btn_ok.wav") {
        score += 850;
    }
    if (fileName == "ok.wav") {
        score += 700;
    }
    if (ContainsUiToken(fileName, "click")) {
        score += 450;
    }
    if (ContainsUiToken(fileName, "button")) {
        score += 350;
    }
    if (ContainsUiToken(fileName, "btn")) {
        score += 300;
    }
    if (ContainsUiToken(fileName, "ok")) {
        score += 200;
    }
    if (ContainsUiToken(fileName, "enter") || ContainsUiToken(fileName, "confirm")) {
        score += 150;
    }
    if (ContainsUiToken(fileName, "cancel") || ContainsUiToken(fileName, "close") || ContainsUiToken(fileName, "back")) {
        score += 40;
    }

    const std::array<const char*, 12> penalizedTokens = {
        "attack", "atk", "monster", "mob", "npc", "skill", "magic", "foot", "step", "weapon", "arrow", "warp"
    };
    for (const char* token : penalizedTokens) {
        if (ContainsUiToken(lowered, token)) {
            score -= 250;
        }
    }

    return score;
}

std::string ResolveUiButtonSoundPath()
{
    static bool s_resolved = false;
    static std::string s_cachedPath;
    if (s_resolved) {
        return s_cachedPath;
    }
    s_resolved = true;

    const std::array<const char*, 14> directCandidates = {
        "data\\wav\\\xB9\xF6\xC6\xB0\xBC\xD2\xB8\xAE.wav",
        "wav\\\xB9\xF6\xC6\xB0\xBC\xD2\xB8\xAE.wav",
        "wav\\click.wav",
        "data\\wav\\click.wav",
        "wav\\button.wav",
        "data\\wav\\button.wav",
        "wav\\btnok.wav",
        "data\\wav\\btnok.wav",
        "wav\\btn_ok.wav",
        "data\\wav\\btn_ok.wav",
        "wav\\ok.wav",
        "data\\wav\\ok.wav",
        "wav\\enter.wav",
        "data\\wav\\enter.wav"
    };

    for (const char* candidate : directCandidates) {
        if (g_fileMgr.IsDataExist(candidate)) {
            s_cachedPath = candidate;
            DbgLog("[UI] button sound resolved direct: %s\n", s_cachedPath.c_str());
            return s_cachedPath;
        }
    }

    std::vector<std::string> wavNames;
    g_fileMgr.CollectDataNamesByExtension("wav", wavNames);

    int bestScore = -1000000;
    for (const std::string& name : wavNames) {
        const int score = ScoreUiButtonSoundCandidate(name);
        if (score > bestScore) {
            bestScore = score;
            s_cachedPath = NormalizeSlashUi(name);
        }
    }

    if (!s_cachedPath.empty()) {
        DbgLog("[UI] button sound resolved scored=%d path=%s\n", bestScore, s_cachedPath.c_str());
    } else {
        DbgLog("[UI] button sound unresolved\n");
    }
    return s_cachedPath;
}

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

void DrawBitmapTransparent(HDC target, HBITMAP bmp, const RECT& dst)
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
    TransparentBlt(target,
        dst.left, dst.top,
        dst.right - dst.left, dst.bottom - dst.top,
        srcDC, 0, 0, bm.bmWidth, bm.bmHeight,
        RGB(255, 0, 255));
    SelectObject(srcDC, old);
    DeleteDC(srcDC);
}

void DrawBitmapAt(HDC target, const std::string& path, int x, int y, int* outW = nullptr, int* outH = nullptr)
{
    if (!target || path.empty()) {
        return;
    }

    HBITMAP bmp = LoadBitmapFromGameData(path.c_str());
    if (!bmp) {
        return;
    }

    BITMAP bm{};
    if (GetObjectA(bmp, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
        if (outW) {
            *outW = bm.bmWidth;
        }
        if (outH) {
            *outH = bm.bmHeight;
        }

        RECT dst = { x, y, x + bm.bmWidth, y + bm.bmHeight };
        DrawBitmapStretched(target, bmp, dst);
    }

    DeleteObject(bmp);
}

bool IsPointInsideWindow(const UIWindow* window, int x, int y)
{
    return window && window->m_w > 0 && window->m_h > 0 &&
        x >= window->m_x && x < window->m_x + window->m_w &&
        y >= window->m_y && y < window->m_y + window->m_h;
}

} // namespace

UIWindow::UIWindow() 
    : m_parent(nullptr), m_x(0), m_y(0), m_w(0), m_h(0), 
      m_isDirty(0), m_dc(nullptr), m_id(0), m_state(0), m_stateCnt(0), 
      m_show(1), m_trans(0), m_transTarget(0), m_transTime(0)
{
}

void UIWindow::Create(int w, int h) { m_w = w; m_h = h; }

void UIWindow::AddChild(UIWindow* child)
{
    if (!child) {
        return;
    }
    child->m_parent = this;
    m_children.push_back(child);
}

void UIWindow::DrawChildren()
{
    for (UIWindow* child : m_children) {
        if (child && child->m_show != 0) {
            child->OnDraw();
        }
    }
}

    void PlayUiButtonSound()
    {
        const std::string path = ResolveUiButtonSoundPath();
        if (path.empty()) {
            return;
        }

        if (CAudio* audio = CAudio::GetInstance()) {
            audio->PlaySound(path.c_str());
        }
    }

void UIWindow::SetSharedDrawDC(HDC dc)
{
    g_sharedUiDrawDC = dc;
}

HDC UIWindow::GetSharedDrawDC()
{
    return g_sharedUiDrawDC;
}

bool LoadUiWindowPlacement(const char* windowName, int* x, int* y)
{
    if (!windowName || !*windowName || !x || !y) {
        return false;
    }

    char valueNameX[64] = {};
    char valueNameY[64] = {};
    std::snprintf(valueNameX, sizeof(valueNameX), "%sX", windowName);
    std::snprintf(valueNameY, sizeof(valueNameY), "%sY", windowName);

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kUiWindowRegPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    bool loadedX = false;
    bool loadedY = false;

    DWORD rawValue = 0;
    DWORD size = sizeof(rawValue);
    if (RegQueryValueExA(key, valueNameX, nullptr, nullptr, reinterpret_cast<BYTE*>(&rawValue), &size) == ERROR_SUCCESS) {
        *x = static_cast<int>(rawValue);
        loadedX = true;
    }

    rawValue = 0;
    size = sizeof(rawValue);
    if (RegQueryValueExA(key, valueNameY, nullptr, nullptr, reinterpret_cast<BYTE*>(&rawValue), &size) == ERROR_SUCCESS) {
        *y = static_cast<int>(rawValue);
        loadedY = true;
    }

    RegCloseKey(key);
    return loadedX && loadedY;
}

void SaveUiWindowPlacement(const char* windowName, int x, int y)
{
    if (!windowName || !*windowName) {
        return;
    }

    char valueNameX[64] = {};
    char valueNameY[64] = {};
    std::snprintf(valueNameX, sizeof(valueNameX), "%sX", windowName);
    std::snprintf(valueNameY, sizeof(valueNameY), "%sY", windowName);

    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kUiWindowRegPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    const DWORD rawX = static_cast<DWORD>(x);
    const DWORD rawY = static_cast<DWORD>(y);
    RegSetValueExA(key, valueNameX, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&rawX), sizeof(rawX));
    RegSetValueExA(key, valueNameY, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&rawY), sizeof(rawY));
    RegCloseKey(key);
}

UIWindow::~UIWindow() {
    for (auto child : m_children) {
        delete child;
    }
    m_children.clear();
}

void UIWindow::Invalidate()
{
    m_isDirty = 1;
    if (m_parent) {
        m_parent->Invalidate();
    }
}

void UIWindow::InvalidateWallPaper()
{
    Invalidate();
}

void UIWindow::Resize(int w, int h)
{
    if (m_w != w || m_h != h) {
        m_w = w;
        m_h = h;
        Invalidate();
    }
}
bool UIWindow::IsFrameWnd() { return false; }
bool UIWindow::IsUpdateNeed() { return m_isDirty != 0; }
void UIWindow::Move(int x, int y)
{
    if (m_x != x || m_y != y) {
        m_x = x;
        m_y = y;
        Invalidate();
    }
}
bool UIWindow::CanGetFocus() { return true; }
bool UIWindow::GetTransBoxInfo(BOXINFO* info) { return false; }
bool UIWindow::IsTransmitMouseInput() { return false; }
bool UIWindow::ShouldDoHitTest() { return true; }
void UIWindow::DragAndDrop(int x, int y, const DRAG_INFO* const info) {}
void UIWindow::StoreInfo() {}
void UIWindow::SaveOriginalPos() {}
void UIWindow::MoveDelta(int dx, int dy)
{
    if (dx != 0 || dy != 0) {
        m_x += dx;
        m_y += dy;
        Invalidate();
    }
}
u32  UIWindow::GetColor() { return 0xFFFFFFFF; }
void UIWindow::SetShow(int show)
{
    if (m_show != show) {
        m_show = show;
        Invalidate();
    }
}

void UIWindow::OnCreate(int x, int y) {}
void UIWindow::OnDestroy() {}
void UIWindow::OnProcess() {}
void UIWindow::OnDraw() {}
void UIWindow::OnDraw2() {}
void UIWindow::OnRun() {}
void UIWindow::OnSize(int w, int h) {}
void UIWindow::OnBeginEdit() {}
void UIWindow::OnFinishEdit() {}
void UIWindow::OnLBtnDown(int x, int y) {}
void UIWindow::OnLBtnDblClk(int x, int y) {}
void UIWindow::OnMouseMove(int x, int y) {}
void UIWindow::OnMouseHover(int x, int y) {}
void UIWindow::OnMouseShape(int x, int y) {}
void UIWindow::OnLBtnUp(int x, int y) {}
void UIWindow::OnRBtnDown(int x, int y) {}
void UIWindow::OnRBtnUp(int x, int y) {}
void UIWindow::OnRBtnDblClk(int x, int y) {}
void UIWindow::OnWheel(int delta) {}
void UIWindow::RefreshSnap() {}
int  UIWindow::SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) { return 0; }
void UIWindow::OnChar(char c) { (void)c; }
bool UIWindow::CanReceiveKeyInput() const { return false; }

UIWindow* UIWindow::HitTestDeep(int x, int y)
{
    if (m_show == 0) {
        return nullptr;
    }
    // Check children first (reverse order = front-most first)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        UIWindow* hit = (*it)->HitTestDeep(x, y);
        if (hit) {
            return hit;
        }
    }
    // Check self bounds
    if (m_w > 0 && m_h > 0 &&
        x >= m_x && x < m_x + m_w &&
        y >= m_y && y < m_y + m_h) {
        return this;
    }
    return nullptr;
}

UIButton::UIButton()
    : m_isDisabled(0)
{
}

void UIButton::SetToolTip(const char* text)
{
    m_toolTip = text ? text : "";
}

void UIButton::OnDraw()
{
    DrawChildren();
}

UIBitmapButton::UIBitmapButton()
    : m_bitmapWidth(0), m_bitmapHeight(0),
      m_normalBitmap(nullptr), m_mouseonBitmap(nullptr), m_pressedBitmap(nullptr)
{
}

UIBitmapButton::~UIBitmapButton()
{
    if (m_normalBitmap) {
        DeleteObject(m_normalBitmap);
        m_normalBitmap = nullptr;
    }
    if (m_mouseonBitmap) {
        DeleteObject(m_mouseonBitmap);
        m_mouseonBitmap = nullptr;
    }
    if (m_pressedBitmap) {
        DeleteObject(m_pressedBitmap);
        m_pressedBitmap = nullptr;
    }
}

void UIBitmapButton::SetBitmapName(const char* name, int stateIndex)
{
    const std::string value = name ? name : "";
    if (stateIndex == 0) {
        m_normalBitmapName = value;
        if (m_normalBitmap) {
            DeleteObject(m_normalBitmap);
            m_normalBitmap = nullptr;
        }
    } else if (stateIndex == 1) {
        m_mouseonBitmapName = value;
        if (m_mouseonBitmap) {
            DeleteObject(m_mouseonBitmap);
            m_mouseonBitmap = nullptr;
        }
    } else if (stateIndex == 2) {
        m_pressedBitmapName = value;
        if (m_pressedBitmap) {
            DeleteObject(m_pressedBitmap);
            m_pressedBitmap = nullptr;
        }
    }

    if (value.empty()) {
        return;
    }

    HBITMAP loaded = LoadBitmapFromGameData(value.c_str());
    if (!loaded) {
        return;
    }

    BITMAP bm{};
    if (GetObjectA(loaded, sizeof(bm), &bm)) {
        if (m_bitmapWidth <= 0) {
            m_bitmapWidth = bm.bmWidth;
        }
        if (m_bitmapHeight <= 0) {
            m_bitmapHeight = bm.bmHeight;
        }
        if (m_w <= 0) {
            m_w = bm.bmWidth;
        }
        if (m_h <= 0) {
            m_h = bm.bmHeight;
        }
    }

    if (stateIndex == 0) {
        m_normalBitmap = loaded;
    } else if (stateIndex == 1) {
        m_mouseonBitmap = loaded;
    } else if (stateIndex == 2) {
        m_pressedBitmap = loaded;
    } else {
        DeleteObject(loaded);
    }
}

void UIBitmapButton::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC hdc = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    HBITMAP drawBmp = nullptr;
    if (m_state == 1 && m_pressedBitmap) {
        drawBmp = m_pressedBitmap;
    } else if (m_state == 2 && m_mouseonBitmap) {
        drawBmp = m_mouseonBitmap;
    } else if (m_normalBitmap) {
        drawBmp = m_normalBitmap;
    } else if (m_mouseonBitmap) {
        drawBmp = m_mouseonBitmap;
    } else {
        drawBmp = m_pressedBitmap;
    }

    if (drawBmp) {
        BITMAP bm{};
        if (GetObjectA(drawBmp, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            m_bitmapWidth = bm.bmWidth;
            m_bitmapHeight = bm.bmHeight;
            RECT dst = { m_x, m_y, m_x + bm.bmWidth, m_y + bm.bmHeight };
            DrawBitmapTransparent(hdc, drawBmp, dst);
        }
    }
    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }
    DrawChildren();
}

void UIBitmapButton::OnLBtnDown(int x, int y)
{
    const int nextState = IsPointInsideWindow(this, x, y) ? 1 : 0;
    if (m_state != nextState) {
        m_state = nextState;
        Invalidate();
    }
}

void UIBitmapButton::OnLBtnDblClk(int x, int y)
{
    OnLBtnDown(x, y);
}

void UIBitmapButton::OnMouseMove(int x, int y)
{
    const bool inside = IsPointInsideWindow(this, x, y);
    if (m_state == 1) {
        if (!inside) {
            m_state = 0;
            Invalidate();
        }
        return;
    }

    const int nextState = inside ? 2 : 0;
    if (m_state != nextState) {
        m_state = nextState;
        Invalidate();
    }
}

void UIBitmapButton::OnMouseHover(int x, int y)
{
    if (m_state == 1) {
        return;
    }

    const int nextState = IsPointInsideWindow(this, x, y) ? 2 : 0;
    if (m_state != nextState) {
        m_state = nextState;
        Invalidate();
    }
}

void UIBitmapButton::OnLBtnUp(int x, int y)
{
    const bool inside = IsPointInsideWindow(this, x, y);
    if (m_state == 1 && inside) {
        m_state = 2;
        Invalidate();
        if (m_parent) {
            m_parent->SendMsg(this, 6, m_id, 0, 0);
        }
    } else {
        const int nextState = inside ? 2 : 0;
        if (m_state != nextState) {
            m_state = nextState;
            Invalidate();
        }
    }
}

UIEditCtrl::UIEditCtrl()
    : m_selectionOrigin(0),
      m_selectionCursor(0),
      m_maskchar(0),
      m_maxchar(0),
      m_isSingColorFrame(0),
      m_r(242), m_g(242), m_b(242),
      m_textR(0), m_textG(0), m_textB(0),
      m_xOffset(4), m_yOffset(2),
      m_type(0),
      m_hasFocus(false)
{
}

void UIEditCtrl::SetFrameColor(int r, int g, int b)
{
    m_r = r;
    m_g = g;
    m_b = b;
}

void UIEditCtrl::SetText(const char* text)
{
    const std::string nextText = text ? text : "";
    if (m_text == nextText) {
        return;
    }
    m_text = nextText;
    if (m_maxchar > 0 && static_cast<int>(m_text.size()) > m_maxchar) {
        m_text.resize(static_cast<size_t>(m_maxchar));
    }
    Invalidate();
}

const char* UIEditCtrl::GetText() const
{
    return m_text.c_str();
}

void UIEditCtrl::OnLBtnDown(int x, int y)
{
    (void)x; (void)y;
    if (!m_hasFocus) {
        m_hasFocus = true;
        Invalidate();
    }
}

void UIEditCtrl::OnChar(char c)
{
    const std::string before = m_text;
    if (c == '\b') {
        if (!m_text.empty()) {
            m_text.pop_back();
        }
    } else if (static_cast<unsigned char>(c) >= 0x20u) {
        if (m_maxchar <= 0 || static_cast<int>(m_text.size()) < m_maxchar) {
            m_text += c;
        }
    }
    if (m_text != before) {
        Invalidate();
    }
}

bool UIEditCtrl::CanReceiveKeyInput() const
{
    return true;
}

void UIEditCtrl::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC hdc = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    RECT rc = { m_x, m_y, m_x + m_w, m_y + m_h };
    const COLORREF bgCol = m_hasFocus ? RGB(255, 255, 200) : RGB(m_r, m_g, m_b);
    HBRUSH bg = CreateSolidBrush(bgCol);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    FrameRect(hdc, &rc, static_cast<HBRUSH>(m_hasFocus ? GetStockObject(BLACK_BRUSH) : GetStockObject(GRAY_BRUSH)));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(m_textR, m_textG, m_textB));

    std::string drawText = m_text;
    if (m_maskchar != 0 && !drawText.empty()) {
        drawText.assign(drawText.size(), static_cast<char>(m_maskchar));
    }

    RECT textRc = { m_x + m_xOffset, m_y + m_yOffset, m_x + m_w - 2, m_y + m_h - 2 };
    DrawTextA(hdc, drawText.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }
    DrawChildren();
}

UICheckBox::UICheckBox()
    : m_isChecked(0), m_onBitmap(nullptr), m_offBitmap(nullptr)
{
}

UICheckBox::~UICheckBox()
{
    if (m_onBitmap) {
        DeleteObject(m_onBitmap);
        m_onBitmap = nullptr;
    }
    if (m_offBitmap) {
        DeleteObject(m_offBitmap);
        m_offBitmap = nullptr;
    }
}

void UICheckBox::SetBitmap(const char* onBitmap, const char* offBitmap)
{
    m_onBitmapName = onBitmap ? onBitmap : "";
    m_offBitmapName = offBitmap ? offBitmap : "";

    if (m_onBitmap) {
        DeleteObject(m_onBitmap);
        m_onBitmap = nullptr;
    }
    if (m_offBitmap) {
        DeleteObject(m_offBitmap);
        m_offBitmap = nullptr;
    }

    if (!m_onBitmapName.empty()) {
        m_onBitmap = LoadBitmapFromGameData(m_onBitmapName.c_str());
    }
    if (!m_offBitmapName.empty()) {
        m_offBitmap = LoadBitmapFromGameData(m_offBitmapName.c_str());
    }

    HBITMAP probe = m_offBitmap ? m_offBitmap : m_onBitmap;
    if (probe) {
        BITMAP bm{};
        if (GetObjectA(probe, sizeof(bm), &bm)) {
            m_w = bm.bmWidth;
            m_h = bm.bmHeight;
        }
    }
}

void UICheckBox::SetCheck(int checked)
{
    const int nextChecked = checked != 0;
    if (m_isChecked != nextChecked) {
        m_isChecked = nextChecked;
        Invalidate();
    }
}

void UICheckBox::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    const bool useShared = (UIWindow::GetSharedDrawDC() != nullptr);
    HDC hdc = useShared ? UIWindow::GetSharedDrawDC() : GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    HBITMAP drawBmp = m_isChecked ? m_onBitmap : m_offBitmap;
    if (!drawBmp) {
        drawBmp = m_isChecked ? m_offBitmap : m_onBitmap;
    }

    if (drawBmp) {
        BITMAP bm{};
        if (GetObjectA(drawBmp, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            RECT dst = { m_x, m_y, m_x + bm.bmWidth, m_y + bm.bmHeight };
            DrawBitmapTransparent(hdc, drawBmp, dst);
        }
    } else {
        const int fallbackSize = (std::max)(12, (std::min)(m_w > 0 ? m_w : 16, m_h > 0 ? m_h : 16));
        RECT boxRect = { m_x, m_y, m_x + fallbackSize, m_y + fallbackSize };
        HBRUSH fillBrush = CreateSolidBrush(RGB(244, 239, 228));
        FillRect(hdc, &boxRect, fillBrush);
        DeleteObject(fillBrush);

        HBRUSH frameBrush = CreateSolidBrush(RGB(82, 63, 45));
        FrameRect(hdc, &boxRect, frameBrush);
        DeleteObject(frameBrush);

        if (m_isChecked) {
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            MoveToEx(hdc, boxRect.left + 3, boxRect.top + 3, nullptr);
            LineTo(hdc, boxRect.right - 3, boxRect.bottom - 3);
            MoveToEx(hdc, boxRect.right - 3, boxRect.top + 3, nullptr);
            LineTo(hdc, boxRect.left + 3, boxRect.bottom - 3);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
    }
    if (!useShared) {
        ReleaseDC(g_hMainWnd, hdc);
    }
    DrawChildren();
}

void UICheckBox::OnLBtnUp(int x, int y)
{
    (void)x; (void)y;
    m_isChecked = !m_isChecked;
    Invalidate();
    if (m_parent) {
        m_parent->SendMsg(this, 6, m_id, m_isChecked, 0);
    }
}
