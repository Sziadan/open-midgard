#include "UINpcInputWnd.h"

#include "render/DC.h"
#include "UIWindow.h"
#include "UIWindowMgr.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <cstdlib>

namespace {

constexpr int kInputWidth = 250;
constexpr int kInputHeight = 96;
constexpr int kBorder = 1;
constexpr int kPadding = 10;
constexpr int kEditHeight = 22;
constexpr int kButtonWidth = 68;
constexpr int kButtonHeight = 22;
constexpr int kCornerRadius = 10;

HFONT GetNpcInputFont()
{
    static HFONT s_font = nullptr;
    if (!s_font) {
        s_font = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
    }
    return s_font;
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawRectOutline(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF fillColor, COLORREF borderColor, int radius)
{
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

RECT MakeRect(int left, int top, int width, int height)
{
    RECT rect{ left, top, left + width, top + height };
    return rect;
}

#if RO_ENABLE_QT6_UI
QFont BuildNpcInputFontFromHdc(HDC hdc)
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

void DrawNpcInputText(HDC hdc, const RECT& rect, const char* text, COLORREF color, Qt::Alignment alignment)
{
    if (!hdc || !text || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text);
    if (label.isEmpty()) {
        return;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildNpcInputFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), alignment | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

} // namespace

UINpcInputWnd::UINpcInputWnd()
    : m_npcId(0),
      m_mode(InputMode::String),
      m_editCtrl(new UIEditCtrl()),
      m_pressedTarget(ClickTarget::None)
{
    Create(kInputWidth, kInputHeight);
    m_editCtrl->Create(m_w - kPadding * 2, kEditHeight);
    m_editCtrl->m_id = 1;
    if (!IsQtUiRuntimeEnabled()) {
        AddChild(m_editCtrl);
    }

    int defaultX = 195;
    int defaultY = 250;
    LoadUiWindowPlacement("NpcInputDialog", &defaultX, &defaultY);
    g_windowMgr.ClampWindowToClient(&defaultX, &defaultY, m_w, m_h);
    UIWindow::Move(defaultX, defaultY);
    LayoutControls();
    UIWindow::SetShow(0);
}

UINpcInputWnd::~UINpcInputWnd()
{
    if (m_editCtrl && m_editCtrl->m_parent != this) {
        delete m_editCtrl;
        m_editCtrl = nullptr;
    }
}

void UINpcInputWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    m_editCtrl->SetShow(show);
    if (show == 0) {
        m_isDragging = 0;
        m_pressedTarget = ClickTarget::None;
    }
}

void UINpcInputWnd::Move(int x, int y)
{
    g_windowMgr.ClampWindowToClient(&x, &y, m_w, m_h);
    UIWindow::Move(x, y);
    LayoutControls();
}

void UINpcInputWnd::StoreInfo()
{
    SaveUiWindowPlacement("NpcInputDialog", m_x, m_y);
}

UIEditCtrl* UINpcInputWnd::GetEditCtrl() const
{
    return m_editCtrl;
}

u32 UINpcInputWnd::GetNpcId() const
{
    return m_npcId;
}

UINpcInputWnd::InputMode UINpcInputWnd::GetInputMode() const
{
    return m_mode;
}

const char* UINpcInputWnd::GetInputText() const
{
    return m_editCtrl ? m_editCtrl->GetText() : "";
}

bool UINpcInputWnd::IsOkPressed() const
{
    return m_pressedTarget == ClickTarget::Ok;
}

bool UINpcInputWnd::IsCancelPressed() const
{
    return m_pressedTarget == ClickTarget::Cancel;
}

bool UINpcInputWnd::GetOkRectForQt(RECT* outRect) const
{
    if (!outRect) {
        return false;
    }
    *outRect = GetOkRect();
    return true;
}

bool UINpcInputWnd::GetCancelRectForQt(RECT* outRect) const
{
    if (!outRect) {
        return false;
    }
    *outRect = GetCancelRect();
    return true;
}

void UINpcInputWnd::LayoutControls()
{
    if (!m_editCtrl) {
        return;
    }
    m_editCtrl->Move(m_x + kPadding, m_y + kPadding + 16);
    m_editCtrl->Resize(m_w - kPadding * 2, kEditHeight);
}

RECT UINpcInputWnd::GetOkRect() const
{
    return MakeRect(m_x + m_w - kPadding - kButtonWidth * 2 - 8,
        m_y + m_h - kPadding - kButtonHeight,
        kButtonWidth,
        kButtonHeight);
}

RECT UINpcInputWnd::GetCancelRect() const
{
    return MakeRect(m_x + m_w - kPadding - kButtonWidth,
        m_y + m_h - kPadding - kButtonHeight,
        kButtonWidth,
        kButtonHeight);
}

bool UINpcInputWnd::IsPointInRect(const RECT& rect, int x, int y) const
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void UINpcInputWnd::StartDragging(int x, int y)
{
    m_isDragging = 1;
    m_startGlobalX = x;
    m_startGlobalY = y;
    m_orgX = m_x;
    m_orgY = m_y;
}

void UINpcInputWnd::StopDragging()
{
    if (m_isDragging == 0) {
        return;
    }
    m_isDragging = 0;
    StoreInfo();
}

void UINpcInputWnd::DrawButton(HDC hdc, const RECT& rect, const char* label, bool pressed) const
{
    FillSolidRect(hdc, rect, pressed ? RGB(196, 196, 196) : RGB(240, 240, 240));
    DrawRectOutline(hdc, rect, RGB(110, 110, 110));
    RECT textRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
#if RO_ENABLE_QT6_UI
    DrawNpcInputText(hdc, textRect, label, RGB(0, 0, 0), Qt::AlignCenter | Qt::AlignVCenter);
#else
    DrawTextA(hdc, label, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
#endif
}

void UINpcInputWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    RECT outer = MakeRect(m_x, m_y, m_w, m_h);
    RECT inner = MakeRect(m_x + kBorder, m_y + kBorder, m_w - 2 * kBorder, m_h - 2 * kBorder);
    FillRoundedRect(hdc, outer, RGB(130, 130, 130), RGB(130, 130, 130), kCornerRadius);
    FillRoundedRect(hdc, inner, RGB(248, 248, 248), RGB(224, 224, 224), kCornerRadius - 2);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, GetNpcInputFont()));
    RECT labelRect = MakeRect(m_x + kPadding, m_y + kPadding - 1, m_w - kPadding * 2, 16);
    const char* label = (m_mode == InputMode::Number) ? "Enter a number" : "Enter text";
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
#if RO_ENABLE_QT6_UI
    DrawNpcInputText(hdc, labelRect, label, RGB(0, 0, 0), Qt::AlignLeft | Qt::AlignTop);
#else
    DrawTextA(hdc, label, -1, &labelRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
#endif

    DrawChildrenToHdc(hdc);

    DrawButton(hdc, GetOkRect(), "OK", m_pressedTarget == ClickTarget::Ok);
    DrawButton(hdc, GetCancelRect(), "Cancel", m_pressedTarget == ClickTarget::Cancel);

    SelectObject(hdc, oldFont);
    ReleaseDrawTarget(hdc);
}

void UINpcInputWnd::OpenForMode(u32 npcId, InputMode mode)
{
    m_npcId = npcId;
    m_mode = mode;
    m_editCtrl->SetText("");
    m_editCtrl->m_type = (mode == InputMode::Number) ? 1 : 0;
    m_editCtrl->m_hasFocus = true;
    m_pressedTarget = ClickTarget::None;
    SetShow(1);
    g_windowMgr.m_editWindow = m_editCtrl;
    Invalidate();
}

void UINpcInputWnd::OpenNumber(u32 npcId)
{
    OpenForMode(npcId, InputMode::Number);
}

void UINpcInputWnd::OpenString(u32 npcId)
{
    OpenForMode(npcId, InputMode::String);
}

void UINpcInputWnd::HideInput()
{
    m_npcId = 0;
    m_editCtrl->SetText("");
    m_editCtrl->m_hasFocus = false;
    if (g_windowMgr.m_editWindow == m_editCtrl) {
        g_windowMgr.m_editWindow = nullptr;
    }
    SetShow(0);
}

bool UINpcInputWnd::SubmitCurrentText()
{
    if (m_npcId == 0) {
        return false;
    }

    const char* text = m_editCtrl->GetText();
    if (!text || *text == '\0') {
        return false;
    }

    const u32 npcId = m_npcId;
    const std::string textCopy = text;
    HideInput();
    PlayUiButtonSound();
    if (m_mode == InputMode::Number) {
        return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestNpcInputNumber, npcId, std::strtoul(textCopy.c_str(), nullptr, 10), 0) != 0;
    }
    return g_modeMgr.SendMsg(CGameMode::GameMsg_RequestNpcInputString, npcId, reinterpret_cast<msgparam_t>(textCopy.c_str()), 0) != 0;
}

void UINpcInputWnd::CancelInput()
{
    if (m_npcId == 0) {
        HideInput();
        return;
    }

    const u32 npcId = m_npcId;
    HideInput();
    PlayUiButtonSound();
    g_windowMgr.CloseNpcDialogWindows();
    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestNpcCloseDialog, npcId, 0, 0);
}

bool UINpcInputWnd::HandleKeyDown(int virtualKey)
{
    if (m_show == 0) {
        return false;
    }

    if (virtualKey == VK_RETURN) {
        SubmitCurrentText();
        return true;
    }
    if (virtualKey == VK_ESCAPE) {
        CancelInput();
        return true;
    }
    return true;
}

void UINpcInputWnd::OnLBtnDown(int x, int y)
{
    if (m_show == 0) {
        return;
    }

    if (IsPointInRect(GetOkRect(), x, y)) {
        m_pressedTarget = ClickTarget::Ok;
        Invalidate();
        return;
    }
    if (IsPointInRect(GetCancelRect(), x, y)) {
        m_pressedTarget = ClickTarget::Cancel;
        Invalidate();
        return;
    }

    const RECT editRect = MakeRect(m_x + kPadding, m_y + kPadding + 16, m_w - kPadding * 2, kEditHeight);
    if (IsPointInRect(editRect, x, y)) {
        if (m_editCtrl) {
            m_editCtrl->m_hasFocus = true;
            g_windowMgr.m_editWindow = m_editCtrl;
        }
        return;
    }

    if (IsPointInRect(MakeRect(m_x, m_y, m_w, m_h), x, y)) {
        StartDragging(x, y);
    }
}

void UINpcInputWnd::OnMouseMove(int x, int y)
{
    if (m_isDragging != 0) {
        int snappedX = m_orgX + (x - m_startGlobalX);
        int snappedY = m_orgY + (y - m_startGlobalY);
        g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
        Move(snappedX, snappedY);
    }
}

void UINpcInputWnd::OnLBtnUp(int x, int y)
{
    if (m_show == 0) {
        return;
    }

    if (m_isDragging != 0) {
        StopDragging();
        return;
    }

    const ClickTarget pressedTarget = m_pressedTarget;
    m_pressedTarget = ClickTarget::None;
    Invalidate();
    if (pressedTarget == ClickTarget::Ok && IsPointInRect(GetOkRect(), x, y)) {
        SubmitCurrentText();
        return;
    }
    if (pressedTarget == ClickTarget::Cancel && IsPointInRect(GetCancelRect(), x, y)) {
        CancelInput();
    }
}
