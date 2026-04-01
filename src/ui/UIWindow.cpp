#include "UIWindow.h"

#include "audio/Audio.h"
#include "core/File.h"
#include "DebugLog.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <array>
#include <cstring>

#pragma comment(lib, "msimg32.lib")

namespace {

HDC g_sharedUiDrawDC = nullptr;
constexpr char kUiWindowRegPath[] = "Software\\Gravity Soft\\Ragnarok Online";

class ScopedUiDrawTarget {
public:
    explicit ScopedUiDrawTarget(HDC dc)
        : m_previous(g_sharedUiDrawDC)
    {
        g_sharedUiDrawDC = dc;
    }

    ~ScopedUiDrawTarget()
    {
        g_sharedUiDrawDC = m_previous;
    }

private:
    HDC m_previous;
};

HDC GetActiveUiDrawTarget()
{
    return g_sharedUiDrawDC;
}

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

#if RO_ENABLE_QT6_UI
QFont BuildUiEditFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QStringLiteral("MS Sans Serif");
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, std::abs(logFont.lfHeight)) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawUiEditTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color)
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    QString label = QString::fromLocal8Bit(text.c_str());
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const QFont font = BuildUiEditFontFromHdc(hdc);
    const QFontMetrics metrics(font);
    label = metrics.elidedText(label, Qt::ElideRight, width);

    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

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

bool IsPointInsideWindow(const UIWindow* window, int x, int y)
{
    return window && window->m_w > 0 && window->m_h > 0 &&
        x >= window->m_x && x < window->m_x + window->m_w &&
        y >= window->m_y && y < window->m_y + window->m_h;
}

} // namespace

UIWindow::UIWindow() 
    : m_parent(nullptr), m_x(0), m_y(0), m_w(0), m_h(0), 
      m_isDirty(0), m_id(0), m_state(0), m_stateCnt(0), 
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

void UIWindow::DrawChildrenToHdc(HDC dc)
{
    if (!dc) {
        return;
    }

    for (UIWindow* child : m_children) {
        if (child && child->m_show != 0) {
            child->DrawToHdc(dc);
        }
    }
}

void UIWindow::DrawToHdc(HDC dc)
{
    if (!dc) {
        return;
    }

    ScopedUiDrawTarget scopedTarget(dc);
    OnDraw();
}

HDC UIWindow::AcquireDrawTarget() const
{
    const HDC shared = GetActiveUiDrawTarget();
    if (shared) {
        return shared;
    }
    return AcquireMainWindowDrawTarget();
}

void UIWindow::ReleaseDrawTarget(HDC dc) const
{
    if (dc && dc != GetActiveUiDrawTarget()) {
        ReleaseMainWindowDrawTarget(dc);
    }
}

HDC AcquireMainWindowDrawTarget()
{
    return g_hMainWnd ? GetDC(g_hMainWnd) : nullptr;
}

void ReleaseMainWindowDrawTarget(HDC dc)
{
    if (dc && g_hMainWnd) {
        ReleaseDC(g_hMainWnd, dc);
    }
}

bool BlitArgbBitsToMainWindow(const void* bits, int width, int height)
{
    if (!g_hMainWnd || !bits || width <= 0 || height <= 0) {
        return false;
    }

    HDC targetDc = AcquireMainWindowDrawTarget();
    if (!targetDc) {
        return false;
    }

    const bool success = StretchArgbToHdc(targetDc,
                                          0,
                                          0,
                                          width,
                                          height,
                                          static_cast<const unsigned int*>(bits),
                                          width,
                                          height);
    ReleaseMainWindowDrawTarget(targetDc);
    return success;
}

bool UIWindow::BlitArgbBitsToDrawTarget(const void* bits, int width, int height) const
{
    if (!bits || width <= 0 || height <= 0) {
        return false;
    }

    HDC targetDc = AcquireDrawTarget();
    if (!targetDc) {
        return false;
    }

    const bool success = StretchArgbToHdc(targetDc,
                                          0,
                                          0,
                                          width,
                                          height,
                                          static_cast<const unsigned int*>(bits),
                                          width,
                                          height);
    ReleaseDrawTarget(targetDc);
    return success;
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
msgresult_t UIWindow::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) { return 0; }
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
      m_normalBitmap(), m_mouseonBitmap(), m_pressedBitmap()
{
}

UIBitmapButton::~UIBitmapButton()
{
    m_normalBitmap.Clear();
    m_mouseonBitmap.Clear();
    m_pressedBitmap.Clear();
}

void UIBitmapButton::SetBitmapName(const char* name, int stateIndex)
{
    const std::string value = name ? name : "";
    if (stateIndex == 0) {
        m_normalBitmapName = value;
        m_normalBitmap.Clear();
    } else if (stateIndex == 1) {
        m_mouseonBitmapName = value;
        m_mouseonBitmap.Clear();
    } else if (stateIndex == 2) {
        m_pressedBitmapName = value;
        m_pressedBitmap.Clear();
    }

    if (value.empty()) {
        return;
    }

    const shopui::BitmapPixels loaded = shopui::LoadBitmapPixelsFromGameData(value, true);
    if (!loaded.IsValid()) {
        return;
    }

    if (m_bitmapWidth <= 0) {
        m_bitmapWidth = loaded.width;
    }
    if (m_bitmapHeight <= 0) {
        m_bitmapHeight = loaded.height;
    }
    if (m_w <= 0) {
        m_w = loaded.width;
    }
    if (m_h <= 0) {
        m_h = loaded.height;
    }

    if (stateIndex == 0) {
        m_normalBitmap = loaded;
    } else if (stateIndex == 1) {
        m_mouseonBitmap = loaded;
    } else if (stateIndex == 2) {
        m_pressedBitmap = loaded;
    }
}

void UIBitmapButton::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }
    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const shopui::BitmapPixels* drawBmp = nullptr;
    if (m_state == 1 && m_pressedBitmap.IsValid()) {
        drawBmp = &m_pressedBitmap;
    } else if (m_state == 2 && m_mouseonBitmap.IsValid()) {
        drawBmp = &m_mouseonBitmap;
    } else if (m_normalBitmap.IsValid()) {
        drawBmp = &m_normalBitmap;
    } else if (m_mouseonBitmap.IsValid()) {
        drawBmp = &m_mouseonBitmap;
    } else {
        drawBmp = m_pressedBitmap.IsValid() ? &m_pressedBitmap : nullptr;
    }

    if (drawBmp) {
        m_bitmapWidth = drawBmp->width;
        m_bitmapHeight = drawBmp->height;
        RECT dst = { m_x, m_y, m_x + drawBmp->width, m_y + drawBmp->height };
        shopui::DrawBitmapPixelsTransparent(hdc, *drawBmp, dst);
    }
    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
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
        if (m_type == 1 && (c < '0' || c > '9')) {
            return;
        }
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
    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
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
#if RO_ENABLE_QT6_UI
    DrawUiEditTextQt(hdc, textRc, drawText, RGB(m_textR, m_textG, m_textB));
#else
    DrawTextA(hdc, drawText.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
#endif

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
}

UICheckBox::UICheckBox()
    : m_isChecked(0), m_onBitmap(), m_offBitmap()
{
}

UICheckBox::~UICheckBox()
{
    m_onBitmap.Clear();
    m_offBitmap.Clear();
}

void UICheckBox::SetBitmap(const char* onBitmap, const char* offBitmap)
{
    m_onBitmapName = onBitmap ? onBitmap : "";
    m_offBitmapName = offBitmap ? offBitmap : "";

    m_onBitmap.Clear();
    m_offBitmap.Clear();

    if (!m_onBitmapName.empty()) {
        m_onBitmap = shopui::LoadBitmapPixelsFromGameData(m_onBitmapName, true);
    }
    if (!m_offBitmapName.empty()) {
        m_offBitmap = shopui::LoadBitmapPixelsFromGameData(m_offBitmapName, true);
    }

    const shopui::BitmapPixels* probe = m_offBitmap.IsValid() ? &m_offBitmap : (m_onBitmap.IsValid() ? &m_onBitmap : nullptr);
    if (probe) {
        m_w = probe->width;
        m_h = probe->height;
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
    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const shopui::BitmapPixels* drawBmp = m_isChecked ? &m_onBitmap : &m_offBitmap;
    if (!drawBmp->IsValid()) {
        drawBmp = m_isChecked ? &m_offBitmap : &m_onBitmap;
    }

    if (drawBmp->IsValid()) {
        RECT dst = { m_x, m_y, m_x + drawBmp->width, m_y + drawBmp->height };
        shopui::DrawBitmapPixelsTransparent(hdc, *drawBmp, dst);
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
    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
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
