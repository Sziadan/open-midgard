#include "UISkillListWnd.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "UIShortCutWnd.h"
#include "UIWindowMgr.h"
#include "core/File.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "res/Bitmap.h"
#include "session/Session.h"
#include "skill/Skill.h"

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
#include <string>

#pragma comment(lib, "msimg32.lib")

namespace {

constexpr int kWindowWidth = 317;
constexpr int kWindowHeight = 291;
constexpr int kTitleBarHeight = 17;
constexpr int kBottomBarHeight = 50;
constexpr int kLeftGutterWidth = 41;
constexpr int kListTop = 20;
constexpr int kListRightMargin = 18;
constexpr int kListBottomMargin = 10;
constexpr int kRowHeight = 37;
constexpr int kIconSize = 24;
constexpr int kIconCellSize = 32;
constexpr int kScrollBarWidth = 10;
constexpr int kQtButtonWidth = 12;
constexpr int kQtButtonHeight = 11;
constexpr int kButtonIdBase = 148;
constexpr int kButtonIdMini = 149;
constexpr int kButtonIdClose = 150;
constexpr int kBottomButtonUse = 0;
constexpr int kBottomButtonClose = 1;

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

void HashTokenValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<unsigned long long>(ch));
    }
    HashTokenValue(hash, 0xFFull);
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

    const char* prefixes[] = {
        "",
        "skin\\default\\",
        "skin\\default\\basic_interface\\",
        "texture\\",
        "texture\\interface\\",
        "texture\\interface\\basic_interface\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\interface\\",
        "data\\texture\\interface\\basic_interface\\",
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string filenameOnly = base;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + filenameOnly);
    }

    return out;
}

std::string ResolveUiAssetPath(const char* fileName)
{
    for (const std::string& candidate : BuildUiAssetCandidates(fileName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return NormalizeSlash(fileName ? fileName : "");
}

shopui::BitmapPixels LoadBitmapPixelsFromGameData(const std::string& path)
{
    return shopui::LoadBitmapPixelsFromGameData(path, true);
}

void DrawBitmapPixelsStretched(HDC target, const shopui::BitmapPixels& bitmap, const RECT& dst)
{
    if (!target || !bitmap.IsValid() || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }
    AlphaBlendArgbToHdc(target,
                        dst.left,
                        dst.top,
                        dst.right - dst.left,
                        dst.bottom - dst.top,
                        bitmap.pixels.data(),
                        bitmap.width,
                        bitmap.height);
}

void TileBitmapPixels(HDC target, const shopui::BitmapPixels& bitmap, const RECT& rect)
{
    if (!target || !bitmap.IsValid() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const int rectRight = static_cast<int>(rect.right);
    const int rectBottom = static_cast<int>(rect.bottom);
    for (int y = rect.top; y < rect.bottom; y += bitmap.height) {
        for (int x = rect.left; x < rect.right; x += bitmap.width) {
            RECT dst{ x, y, (std::min)(x + bitmap.width, rectRight), (std::min)(y + bitmap.height, rectBottom) };
            DrawBitmapPixelsStretched(target, bitmap, dst);
        }
    }
}

void DrawThreePieceBarPixels(HDC hdc, const RECT& rect, const shopui::BitmapPixels& left, const shopui::BitmapPixels& mid, const shopui::BitmapPixels& right)
{
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    int leftWidth = 0;
    int rightWidth = 0;
    if (left.IsValid()) {
        leftWidth = left.width;
    }
    if (right.IsValid()) {
        rightWidth = right.width;
    }

    if (left.IsValid()) {
        RECT dst{ rect.left, rect.top, rect.left + leftWidth, rect.bottom };
        DrawBitmapPixelsStretched(hdc, left, dst);
    }
    if (right.IsValid()) {
        RECT dst{ rect.right - rightWidth, rect.top, rect.right, rect.bottom };
        DrawBitmapPixelsStretched(hdc, right, dst);
    }

    RECT midRect{ rect.left + leftWidth, rect.top, rect.right - rightWidth, rect.bottom };
    TileBitmapPixels(hdc, mid, midRect);
}

#if RO_ENABLE_QT6_UI
QFont BuildSkillListFontFromHdc(HDC hdc)
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
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawSkillTextLineQt(HDC hdc, int x, int y, COLORREF color, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text.c_str());
    if (label.isEmpty()) {
        return;
    }

    const QFont font = BuildSkillListFontFromHdc(hdc);
    const QFontMetrics metrics(font);
    const int width = (std::max)(1, metrics.horizontalAdvance(label) + 4);
    const int height = (std::max)(1, metrics.height() + 2);
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
    painter.drawText(0, metrics.ascent(), label);
    AlphaBlendArgbToHdc(hdc, x, y, width, height, pixels.data(), width, height);
}

void DrawSkillButtonTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text.c_str());
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
    painter.setFont(BuildSkillListFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), Qt::AlignCenter | Qt::AlignVCenter | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawTextLine(HDC hdc, int x, int y, COLORREF color, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
#if RO_ENABLE_QT6_UI
    DrawSkillTextLineQt(hdc, x, y, color, text);
#else
    TextOutA(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
#endif
}

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

RECT MakeSkillRect(int x, int y, int left, int top, int width, int height)
{
    RECT rect{ x + left, y + top, x + left + width, y + top + height };
    return rect;
}

std::string ResolveSkillIconPath(const SkillMetadata& metadata)
{
    const std::string lowered = ToLowerAscii(metadata.skillIdName);
    const std::string direct = "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\" + lowered + ".bmp";
    const std::string dataPath = "data\\" + direct;
    if (g_fileMgr.IsDataExist(direct.c_str())) {
        return direct;
    }
    if (g_fileMgr.IsDataExist(dataPath.c_str())) {
        return dataPath;
    }
    return direct;
}

std::string BuildSkillRightText(const PLAYER_SKILL_INFO& skill)
{
    if (skill.spcost > 0) {
        return "Sp : " + std::to_string(skill.spcost);
    }
    return "Passive";
}

std::string BuildSkillDisplayName(const PLAYER_SKILL_INFO& skill)
{
    return skill.skillName.empty() ? skill.skillIdName : skill.skillName;
}

HFONT GetUiFont()
{
    static HFONT s_font = CreateFontA(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma");
    return s_font;
}

HFONT GetUiBoldFont()
{
    static HFONT s_font = CreateFontA(
        -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma");
    return s_font;
}

} // namespace

UISkillListWnd::UISkillListWnd()
    : m_controlsCreated(false),
      m_hoveredRow(-1),
      m_viewOffset(0),
      m_selectedSkillId(0),
      m_pressedUpgradeSkillId(0),
      m_dragSkillId(0),
      m_dragSkillLevel(0),
      m_dragArmed(false),
      m_dragStartPoint{},
      m_isDraggingScrollThumb(0),
      m_scrollDragOffsetY(0),
      m_systemButtons{},
      m_bottomButtons{},
      m_titleBarBitmap(),
      m_titleBarLeftBitmap(),
      m_titleBarMidBitmap(),
      m_titleBarRightBitmap(),
      m_btnBarLeftBitmap(),
      m_btnBarMidBitmap(),
      m_btnBarRightBitmap(),
      m_btnBarLeft2Bitmap(),
      m_btnBarMid2Bitmap(),
      m_btnBarRight2Bitmap(),
      m_itemRowBitmap(),
      m_itemInvertBitmap(),
      m_upgradeNormalBitmap(),
      m_upgradeHoverBitmap(),
      m_upgradePressedBitmap(),
      m_mesBtnLeftBitmap(),
      m_mesBtnMidBitmap(),
      m_mesBtnRightBitmap(),
      m_lastVisualStateToken(0ull),
      m_hasVisualStateToken(false)
{
    m_show = 0;
    m_w = kWindowWidth;
    m_h = kWindowHeight;
    m_bottomButtons[kBottomButtonUse].label = "use";
    m_bottomButtons[kBottomButtonClose].label = "close";
    Move(281, 121);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("SkillListWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UISkillListWnd::~UISkillListWnd()
{
    ReleaseAssets();
}

void UISkillListWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        LayoutChildren();
        RefreshVisibleSkillsForInteractionState();
    }
}

void UISkillListWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    LayoutChildren();
    if (m_show != 0) {
        RefreshVisibleSkillsForInteractionState();
    }
}

bool UISkillListWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasVisualStateToken) {
        return true;
    }
    return BuildVisualStateToken() != m_lastVisualStateToken;
}

msgresult_t UISkillListWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;
    if (msg != 6) {
        return 0;
    }

    switch (wparam) {
    case kButtonIdClose:
        SetShow(0);
        return 1;
    case kButtonIdMini:
    case kButtonIdBase:
        return 1;
    default:
        return 0;
    }
}

void UISkillListWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;
    LoadAssets();

    if (IsQtUiRuntimeEnabled()) {
        LayoutChildren();
        return;
    }

    struct ButtonSpec {
        const char* offName;
        const char* onName;
        int id;
        const char* tooltip;
    };

    const std::array<ButtonSpec, 3> specs = {{
        { "sys_base_off.bmp", "sys_base_on.bmp", kButtonIdBase, "Base" },
        { "sys_mini_off.bmp", "sys_mini_on.bmp", kButtonIdMini, "Mini" },
        { "sys_close_off.bmp", "sys_close_on.bmp", kButtonIdClose, "Close" },
    }};

    for (size_t index = 0; index < specs.size(); ++index) {
        auto* button = new UIBitmapButton();
        button->SetBitmapName(ResolveUiAssetPath(specs[index].offName).c_str(), 0);
        button->SetBitmapName(ResolveUiAssetPath(specs[index].onName).c_str(), 1);
        button->SetBitmapName(ResolveUiAssetPath(specs[index].onName).c_str(), 2);
        button->Create(button->m_bitmapWidth, button->m_bitmapHeight);
        button->m_id = specs[index].id;
        button->SetToolTip(specs[index].tooltip);
        AddChild(button);
        m_systemButtons[index] = button;
    }

    LayoutChildren();
}

void UISkillListWnd::OnDestroy()
{
}

void UISkillListWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }

    EnsureCreated();
    RefreshVisibleSkillsForInteractionState();

    const std::vector<const PLAYER_SKILL_INFO*> skills = GetSortedSkills();

    if (IsQtUiRuntimeEnabled()) {
        m_lastVisualStateToken = BuildVisualStateToken();
        m_hasVisualStateToken = true;
        m_isDirty = 0;
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    HGDIOBJ oldFont = SelectObject(hdc, GetUiFont());

    DrawWindowChrome(hdc);
    SelectObject(hdc, GetUiFont());
    DrawTextLine(hdc, m_x + 18, m_y + 3, RGB(255, 255, 255), "Skill Tree");
    DrawTextLine(hdc, m_x + 17, m_y + 2, RGB(0, 0, 0), "Skill Tree");
    SelectObject(hdc, GetUiBoldFont());
    DrawTextLine(hdc, m_x + 13, m_y + m_h - 18, RGB(176, 145, 48),
        "Skill Point : " + std::to_string(g_session.GetPlayerSkillPointCount()));
    SelectObject(hdc, GetUiFont());

    const int rowLeft = m_x + kLeftGutterWidth + 4;
    const int rowRight = m_x + m_w - kListRightMargin - 4;
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    const bool hideDraggedSkill = gameMode
        && gameMode->m_dragType == static_cast<int>(DragType::ShortcutSkill)
        && gameMode->m_dragInfo.source == static_cast<int>(DragSource::SkillListWindow)
        && gameMode->m_dragInfo.skillId != 0;
    for (size_t rowIndex = 0; rowIndex < m_visibleSkills.size(); ++rowIndex) {
        const VisibleSkill& visible = m_visibleSkills[rowIndex];
        const RECT& rowRect = visible.rowRect;
        const RECT& upgradeRect = visible.upgradeRect;
        const PLAYER_SKILL_INFO* skill = visible.skill;
        const bool isSelected = skill && skill->SKID == m_selectedSkillId;
        RECT iconCellRect{ rowRect.left + 4, rowRect.top + 1, rowRect.left + 4 + kIconCellSize, rowRect.top + 1 + kIconCellSize };
        RECT iconInvertRect{ iconCellRect.left + 2, iconCellRect.top + 8, iconCellRect.left + 2 + 28, iconCellRect.top + 8 + 15 };
        RECT iconRect{
            iconCellRect.left + ((kIconCellSize - kIconSize) / 2),
            iconCellRect.top + ((kIconCellSize - kIconSize) / 2),
            iconCellRect.left + ((kIconCellSize - kIconSize) / 2) + kIconSize,
            iconCellRect.top + ((kIconCellSize - kIconSize) / 2) + kIconSize
        };
        if (m_itemRowBitmap.IsValid()) {
            DrawBitmapPixelsStretched(hdc, m_itemRowBitmap, iconCellRect);
        }
        if (isSelected && m_itemInvertBitmap.IsValid()) {
            DrawBitmapPixelsStretched(hdc, m_itemInvertBitmap, iconInvertRect);
        }

        if (skill) {
            const bool isDraggedSource = hideDraggedSkill && skill->SKID == gameMode->m_dragInfo.skillId;
            if (!isDraggedSource) {
                if (const shopui::BitmapPixels* icon = GetSkillIcon(skill->SKID)) {
                    shopui::DrawBitmapPixelsTransparent(hdc, *icon, iconRect);
                }
            }

            const std::string skillName = skill->skillName.empty() ? skill->skillIdName : skill->skillName;
            const int textLeft = iconCellRect.right + 12;
            DrawTextLine(hdc, textLeft, rowRect.top + 3, RGB(0, 0, 0), skillName);
            DrawTextLine(hdc, textLeft, rowRect.top + 18, RGB(0, 0, 0),
                "Lv : " + std::to_string(skill->level));
            DrawTextLine(hdc, rowRect.left + 165, rowRect.top + 18, RGB(0, 0, 0), BuildSkillRightText(*skill));

            const bool canUpgrade = skill->upgradable != 0 && g_session.GetPlayerSkillPointCount() > 0;
            if (canUpgrade) {
                const shopui::BitmapPixels& upgradeBitmap = m_upgradeNormalBitmap;
                if (upgradeBitmap.IsValid()) {
                    DrawBitmapPixelsStretched(hdc, upgradeBitmap, upgradeRect);
                }
            }
        }
    }

    if (IsScrollBarVisible(static_cast<int>(skills.size()))) {
        const RECT scrollTrackRect = GetScrollTrackRect();
        const RECT scrollThumbRect = GetScrollThumbRect(static_cast<int>(skills.size()));
        FillRectColor(hdc, scrollTrackRect, RGB(227, 231, 238));
        FrameRectColor(hdc, scrollTrackRect, RGB(164, 173, 189));
        FillRectColor(hdc, scrollThumbRect, RGB(180, 188, 205));
        FrameRectColor(hdc, scrollThumbRect, RGB(120, 130, 150));
    }

    for (const TextButton& button : m_bottomButtons) {
        DrawBottomButton(hdc, button);
    }

    DrawChildrenToHdc(hdc);

    SelectObject(hdc, oldFont);

    ReleaseDrawTarget(hdc);

    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UISkillListWnd::OnLBtnDown(int x, int y)
{
    EnsureCreated();
    if (IsQtUiRuntimeEnabled()) {
        RefreshVisibleSkillsForInteractionState();
    }

    m_dragArmed = false;
    m_dragSkillId = 0;
    m_dragSkillLevel = 0;

    if (IsQtUiRuntimeEnabled()) {
        const RECT baseRect = MakeSkillRect(m_x, m_y, 231, 2, kQtButtonWidth, kQtButtonHeight);
        const RECT miniRect = MakeSkillRect(m_x, m_y, 247, 2, kQtButtonWidth, kQtButtonHeight);
        const RECT closeRect = MakeSkillRect(m_x, m_y, 263, 2, kQtButtonWidth, kQtButtonHeight);
        if (IsInsideRect(baseRect, x, y) || IsInsideRect(miniRect, x, y) || IsInsideRect(closeRect, x, y)) {
            UIWindow::OnLBtnDown(x, y);
            return;
        }
    }

    if (y >= m_y && y < m_y + kTitleBarHeight) {
        UIFrameWnd::OnLBtnDown(x, y);
        return;
    }

    UpdateHover(x, y);
    m_pressedUpgradeSkillId = 0;

    const int skillCount = static_cast<int>(GetSortedSkills().size());
    if (IsScrollBarVisible(skillCount)) {
        const RECT scrollThumbRect = GetScrollThumbRect(skillCount);
        const RECT scrollTrackRect = GetScrollTrackRect();
        if (IsInsideRect(scrollThumbRect, x, y)) {
            m_isDraggingScrollThumb = 1;
            m_scrollDragOffsetY = y - scrollThumbRect.top;
            return;
        }
        if (IsInsideRect(scrollTrackRect, x, y)) {
            UpdateScrollFromThumbPosition(y - ((scrollThumbRect.bottom - scrollThumbRect.top) / 2), skillCount);
            return;
        }
    }

    const int bottomButtonIndex = HitTestBottomButton(x, y);
    if (bottomButtonIndex >= 0) {
        m_bottomButtons[bottomButtonIndex].pressed = true;
        return;
    }

    for (const VisibleSkill& visible : m_visibleSkills) {
        if (!visible.skill) {
            continue;
        }
        if (!IsInsideRect(visible.upgradeRect, x, y)) {
            continue;
        }
        if (visible.skill->upgradable == 0 || g_session.GetPlayerSkillPointCount() <= 0) {
            return;
        }
        m_selectedSkillId = visible.skill->SKID;
        m_pressedUpgradeSkillId = visible.skill->SKID;
        return;
    }

    if (m_hoveredRow >= 0 && m_hoveredRow < static_cast<int>(m_visibleSkills.size())) {
        const VisibleSkill& visible = m_visibleSkills[m_hoveredRow];
        if (visible.skill) {
            m_selectedSkillId = visible.skill->SKID;
            m_dragArmed = true;
            m_dragSkillId = visible.skill->SKID;
            m_dragSkillLevel = visible.skill->level;
            m_dragStartPoint = POINT{ x, y };
        }
    }
}

void UISkillListWnd::OnLBtnUp(int x, int y)
{
    if (IsQtUiRuntimeEnabled()) {
        RefreshVisibleSkillsForInteractionState();
    }

    if (IsQtUiRuntimeEnabled()) {
        const bool wasDragging = m_isDragging != 0;
        UIFrameWnd::OnLBtnUp(x, y);
        if (wasDragging) {
            return;
        }

        const RECT baseRect = MakeSkillRect(m_x, m_y, 231, 2, kQtButtonWidth, kQtButtonHeight);
        const RECT miniRect = MakeSkillRect(m_x, m_y, 247, 2, kQtButtonWidth, kQtButtonHeight);
        const RECT closeRect = MakeSkillRect(m_x, m_y, 263, 2, kQtButtonWidth, kQtButtonHeight);

        if (IsInsideRect(baseRect, x, y)) {
            SendMsg(this, 6, kButtonIdBase, 0, 0);
            return;
        }
        if (IsInsideRect(miniRect, x, y)) {
            SendMsg(this, 6, kButtonIdMini, 0, 0);
            return;
        }
        if (IsInsideRect(closeRect, x, y)) {
            SendMsg(this, 6, kButtonIdClose, 0, 0);
            return;
        }
    } else {
        UIFrameWnd::OnLBtnUp(x, y);
    }

    m_isDraggingScrollThumb = 0;
    const int bottomButtonIndex = HitTestBottomButton(x, y);
    for (size_t index = 0; index < m_bottomButtons.size(); ++index) {
        const bool activate = m_bottomButtons[index].pressed && static_cast<int>(index) == bottomButtonIndex;
        m_bottomButtons[index].pressed = false;
        if (!activate) {
            continue;
        }
        if (index == kBottomButtonClose) {
            SetShow(0);
        } else if (index == kBottomButtonUse) {
            if (m_selectedSkillId > 0) {
                ArmPendingSkillUseFromSkillList(m_selectedSkillId);
            }
        }
    }

    if (m_pressedUpgradeSkillId != 0) {
        for (const VisibleSkill& visible : m_visibleSkills) {
            if (!visible.skill || visible.skill->SKID != m_pressedUpgradeSkillId) {
                continue;
            }
            if (IsInsideRect(visible.upgradeRect, x, y) &&
                visible.skill->upgradable != 0 &&
                g_session.GetPlayerSkillPointCount() > 0) {
                g_modeMgr.SendMsg(CGameMode::GameMsg_RequestUpgradeSkillLevel, m_pressedUpgradeSkillId, 0, 0);
            }
            break;
        }
    }
    m_pressedUpgradeSkillId = 0;
    m_dragArmed = false;
    m_dragSkillId = 0;
    m_dragSkillLevel = 0;
}

void UISkillListWnd::OnMouseMove(int x, int y)
{
    if (IsQtUiRuntimeEnabled()) {
        RefreshVisibleSkillsForInteractionState();
    }

    UIFrameWnd::OnMouseMove(x, y);
    UpdateHover(x, y);
    if (m_dragArmed) {
        const int dx = x - m_dragStartPoint.x;
        const int dy = y - m_dragStartPoint.y;
        if ((dx * dx) + (dy * dy) >= 16) {
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_dragType = static_cast<int>(DragType::ShortcutSkill);
                gameMode->m_dragInfo = DRAG_INFO{};
                gameMode->m_dragInfo.type = static_cast<int>(DragType::ShortcutSkill);
                gameMode->m_dragInfo.source = static_cast<int>(DragSource::SkillListWindow);
                gameMode->m_dragInfo.skillId = m_dragSkillId;
                gameMode->m_dragInfo.skillLevel = m_dragSkillLevel;
                Invalidate();
                if (g_windowMgr.m_shortCutWnd) {
                    g_windowMgr.m_shortCutWnd->Invalidate();
                }
            }
            m_dragArmed = false;
        }
    }
    if (m_isDraggingScrollThumb) {
        UpdateScrollFromThumbPosition(y - m_scrollDragOffsetY, static_cast<int>(GetSortedSkills().size()));
    }
    for (size_t index = 0; index < m_bottomButtons.size(); ++index) {
        m_bottomButtons[index].hovered = (static_cast<int>(index) == HitTestBottomButton(x, y));
    }
}

void UISkillListWnd::OnWheel(int delta)
{
    const int skillCount = static_cast<int>(GetSortedSkills().size());
    const int maxOffset = GetMaxViewOffset(skillCount);
    if (delta > 0) {
        m_viewOffset = std::max(0, m_viewOffset - 1);
    } else if (delta < 0) {
        m_viewOffset = std::min(maxOffset, m_viewOffset + 1);
    }
    RefreshVisibleSkillsForInteractionState();
}

void UISkillListWnd::StoreInfo()
{
    SaveUiWindowPlacement("SkillListWnd", m_x, m_y);
}

bool UISkillListWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    DisplayData data{};
    const std::vector<const PLAYER_SKILL_INFO*> skills = GetSortedSkills();
    data.skillPointCount = g_session.GetPlayerSkillPointCount();
    data.viewOffset = m_viewOffset;
    data.maxViewOffset = GetMaxViewOffset(static_cast<int>(skills.size()));
    data.scrollBarVisible = IsScrollBarVisible(static_cast<int>(skills.size()));

    if (data.scrollBarVisible) {
        const RECT trackRect = GetScrollTrackRect();
        const RECT thumbRect = GetScrollThumbRect(static_cast<int>(skills.size()));
        data.scrollTrackX = trackRect.left;
        data.scrollTrackY = trackRect.top;
        data.scrollTrackWidth = trackRect.right - trackRect.left;
        data.scrollTrackHeight = trackRect.bottom - trackRect.top;
        data.scrollThumbX = thumbRect.left;
        data.scrollThumbY = thumbRect.top;
        data.scrollThumbWidth = thumbRect.right - thumbRect.left;
        data.scrollThumbHeight = thumbRect.bottom - thumbRect.top;
    }

    const int visibleRows = std::max(1, (m_h - kTitleBarHeight - kBottomBarHeight - kListTop - kListBottomMargin) / kRowHeight);
    const int rowLeft = m_x + kLeftGutterWidth + 4;
    const int rowRight = m_x + m_w - kListRightMargin - 4;
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    const bool hideDraggedSkill = gameMode
        && gameMode->m_dragType == static_cast<int>(DragType::ShortcutSkill)
        && gameMode->m_dragInfo.source == static_cast<int>(DragSource::SkillListWindow)
        && gameMode->m_dragInfo.skillId != 0;
    const int firstIndex = data.viewOffset;
    const int lastIndex = std::min(static_cast<int>(skills.size()), firstIndex + visibleRows);
    data.rows.reserve(static_cast<size_t>(std::max(0, lastIndex - firstIndex)));
    for (int drawIndex = firstIndex; drawIndex < lastIndex; ++drawIndex) {
        const PLAYER_SKILL_INFO* const skill = skills[drawIndex];
        if (!skill) {
            continue;
        }

        const int rowIndex = drawIndex - firstIndex;
        const int rowTop = m_y + kListTop + rowIndex * kRowHeight;
        RECT rowRect{ rowLeft, rowTop, rowRight, rowTop + kRowHeight };
        RECT upgradeRect{ rowRect.right - 28, rowRect.top + 4, rowRect.right - 4, rowRect.top + 28 };

        DisplayRow row{};
        row.skillId = skill->SKID;
        row.x = rowRect.left;
        row.y = rowRect.top;
        row.width = rowRect.right - rowRect.left;
        row.height = rowRect.bottom - rowRect.top;
        row.iconVisible = !(hideDraggedSkill && skill->SKID == gameMode->m_dragInfo.skillId);
        row.selected = skill->SKID == m_selectedSkillId;
        row.hovered = drawIndex == (m_viewOffset + m_hoveredRow);
        row.upgradeVisible = skill->upgradable != 0 && g_session.GetPlayerSkillPointCount() > 0;
        row.upgradePressed = skill->SKID == m_pressedUpgradeSkillId;
        row.upgradeX = upgradeRect.left;
        row.upgradeY = upgradeRect.top;
        row.upgradeWidth = upgradeRect.right - upgradeRect.left;
        row.upgradeHeight = upgradeRect.bottom - upgradeRect.top;
        row.name = BuildSkillDisplayName(*skill);
        row.levelText = "Lv : " + std::to_string(skill->level);
        row.rightText = BuildSkillRightText(*skill);
        data.rows.push_back(std::move(row));
    }

    data.bottomButtons.reserve(m_bottomButtons.size());
    for (const TextButton& button : m_bottomButtons) {
        DisplayButton displayButton{};
        displayButton.x = button.rect.left;
        displayButton.y = button.rect.top;
        displayButton.width = button.rect.right - button.rect.left;
        displayButton.height = button.rect.bottom - button.rect.top;
        displayButton.hovered = button.hovered;
        displayButton.pressed = button.pressed;
        displayButton.label = button.label;
        data.bottomButtons.push_back(std::move(displayButton));
    }

    *outData = std::move(data);
    return true;
}

int UISkillListWnd::GetQtSystemButtonCount() const
{
    return 3;
}

bool UISkillListWnd::GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const
{
    if (!outData || index < 0 || index >= GetQtSystemButtonCount()) {
        return false;
    }

    switch (index) {
    case 0:
        outData->id = kButtonIdBase;
        outData->x = m_x + 231;
        outData->y = m_y + 2;
        outData->width = kQtButtonWidth;
        outData->height = kQtButtonHeight;
        outData->label = "B";
        outData->visible = true;
        return true;
    case 1:
        outData->id = kButtonIdMini;
        outData->x = m_x + 247;
        outData->y = m_y + 2;
        outData->width = kQtButtonWidth;
        outData->height = kQtButtonHeight;
        outData->label = "_";
        outData->visible = true;
        return true;
    case 2:
        outData->id = kButtonIdClose;
        outData->x = m_x + 263;
        outData->y = m_y + 2;
        outData->width = kQtButtonWidth;
        outData->height = kQtButtonHeight;
        outData->label = "X";
        outData->visible = true;
        return true;
    default:
        return false;
    }
}

void UISkillListWnd::EnsureCreated()
{
    if (m_controlsCreated) {
        return;
    }

    Create(m_w, m_h);
    OnCreate(m_x, m_y);
}

void UISkillListWnd::LayoutChildren()
{
    int buttonX = m_x + m_w - 49;
    for (UIBitmapButton* button : m_systemButtons) {
        if (!button) {
            continue;
        }
        button->Move(buttonX, m_y + 2);
        buttonX += 16;
    }

    const int bottomY = m_y + m_h - 28;
    m_bottomButtons[kBottomButtonClose].rect = RECT{ m_x + m_w - 56, bottomY, m_x + m_w - 12, bottomY + 20 };
    m_bottomButtons[kBottomButtonUse].rect = RECT{ m_bottomButtons[kBottomButtonClose].rect.left - 44, bottomY, m_bottomButtons[kBottomButtonClose].rect.left, bottomY + 20 };
}

void UISkillListWnd::LoadAssets()
{
    ReleaseAssets();
    m_titleBarBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_fix.bmp"));
    m_titleBarLeftBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_left.bmp"));
    m_titleBarMidBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_mid.bmp"));
    m_titleBarRightBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_right.bmp"));
    m_btnBarLeftBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("btnbar_left.bmp"));
    m_btnBarMidBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("btnbar_mid.bmp"));
    m_btnBarRightBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("btnbar_right.bmp"));
    m_btnBarLeft2Bitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("btnbar_left2.bmp"));
    m_btnBarMid2Bitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("btnbar_mid2.bmp"));
    m_btnBarRight2Bitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("btnbar_right2.bmp"));
    m_itemRowBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("itemwin_mid.bmp"));
    m_itemInvertBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("item_invert.bmp"));
    m_upgradeNormalBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("skill_up_a.bmp"));
    m_upgradeHoverBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("skill_up_b.bmp"));
    m_upgradePressedBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("skill_up_c.bmp"));
    m_mesBtnLeftBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("mesbtn_left.bmp"));
    m_mesBtnMidBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("mesbtn_mid.bmp"));
    m_mesBtnRightBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("mesbtn_right.bmp"));
}

void UISkillListWnd::ReleaseAssets()
{
    m_titleBarBitmap.Clear();
    m_titleBarLeftBitmap.Clear();
    m_titleBarMidBitmap.Clear();
    m_titleBarRightBitmap.Clear();
    m_btnBarLeftBitmap.Clear();
    m_btnBarMidBitmap.Clear();
    m_btnBarRightBitmap.Clear();
    m_btnBarLeft2Bitmap.Clear();
    m_btnBarMid2Bitmap.Clear();
    m_btnBarRight2Bitmap.Clear();
    m_itemRowBitmap.Clear();
    m_itemInvertBitmap.Clear();
    m_upgradeNormalBitmap.Clear();
    m_upgradeHoverBitmap.Clear();
    m_upgradePressedBitmap.Clear();
    m_mesBtnLeftBitmap.Clear();
    m_mesBtnMidBitmap.Clear();
    m_mesBtnRightBitmap.Clear();

    m_iconCache.clear();
}

void UISkillListWnd::RefreshVisibleSkillsForInteractionState()
{
    const std::vector<const PLAYER_SKILL_INFO*> skills = GetSortedSkills();
    if (m_selectedSkillId == 0 && !skills.empty()) {
        m_selectedSkillId = skills.front()->SKID;
    }

    const int visibleRows = std::max(1, (m_h - kTitleBarHeight - kBottomBarHeight - kListTop - kListBottomMargin) / kRowHeight);
    m_viewOffset = std::max(0, std::min(m_viewOffset, GetMaxViewOffset(static_cast<int>(skills.size()))));
    m_visibleSkills.clear();

    const int rowLeft = m_x + kLeftGutterWidth + 4;
    const int rowRight = m_x + m_w - kListRightMargin - 4;
    const int firstIndex = m_viewOffset;
    const int lastIndex = std::min(static_cast<int>(skills.size()), firstIndex + visibleRows);
    for (int drawIndex = firstIndex; drawIndex < lastIndex; ++drawIndex) {
        const int rowIndex = drawIndex - firstIndex;
        const int rowTop = m_y + kListTop + rowIndex * kRowHeight;
        RECT rowRect{ rowLeft, rowTop, rowRight, rowTop + kRowHeight };
        RECT upgradeRect{ rowRect.right - 28, rowRect.top + 4, rowRect.right - 4, rowRect.top + 28 };
        m_visibleSkills.push_back({ skills[drawIndex], rowRect, upgradeRect });
    }
}

void UISkillListWnd::UpdateHover(int globalX, int globalY)
{
    m_hoveredRow = -1;
    for (size_t index = 0; index < m_visibleSkills.size(); ++index) {
        const RECT& rowRect = m_visibleSkills[index].rowRect;
        if (IsInsideRect(rowRect, globalX, globalY)) {
            m_hoveredRow = static_cast<int>(index);
            break;
        }
    }
}

void UISkillListWnd::DrawWindowChrome(HDC hdc) const
{
    RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &outer, whiteBrush);
    DeleteObject(whiteBrush);

    if (m_titleBarBitmap.IsValid() && m_w == 280) {
        RECT titleRect{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
        DrawBitmapPixelsStretched(hdc, m_titleBarBitmap, titleRect);
    } else {
        RECT titleRect{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
        DrawThreePieceBarPixels(hdc, titleRect, m_titleBarLeftBitmap, m_titleBarMidBitmap, m_titleBarRightBitmap);
    }

    RECT bottomTopRect{ m_x, m_y + m_h - kBottomBarHeight, m_x + m_w, m_y + m_h - 29 };
    RECT bottomBottomRect{ m_x, m_y + m_h - 29, m_x + m_w, m_y + m_h };
    DrawThreePieceBarPixels(hdc, bottomTopRect, m_btnBarLeftBitmap, m_btnBarMidBitmap, m_btnBarRightBitmap);
    DrawThreePieceBarPixels(hdc, bottomBottomRect, m_btnBarLeft2Bitmap, m_btnBarMid2Bitmap, m_btnBarRight2Bitmap);

    RECT leftBarRect{ m_x, m_y + kTitleBarHeight, m_x + kLeftGutterWidth, m_y + m_h - kBottomBarHeight };
    if (leftBarRect.bottom > leftBarRect.top) {
        int y = leftBarRect.top;
        const int limit = leftBarRect.bottom;
        if (m_btnBarMidBitmap.IsValid()) {
            while (y + m_btnBarMidBitmap.height <= limit) {
                RECT dst{ leftBarRect.left, y, leftBarRect.right, y + m_btnBarMidBitmap.height };
                DrawBitmapPixelsStretched(hdc, m_btnBarMidBitmap, dst);
                y += m_btnBarMidBitmap.height;
                if (m_btnBarMid2Bitmap.IsValid()) {
                    if (y + m_btnBarMid2Bitmap.height <= limit) {
                        RECT dst2{ leftBarRect.left, y, leftBarRect.right, y + m_btnBarMid2Bitmap.height };
                        DrawBitmapPixelsStretched(hdc, m_btnBarMid2Bitmap, dst2);
                        y += m_btnBarMid2Bitmap.height;
                    }
                }
            }
        }
    }

    RECT contentRect{ m_x + kLeftGutterWidth, m_y + kTitleBarHeight, m_x + m_w, m_y + m_h - kBottomBarHeight };
    HBRUSH contentBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &contentRect, contentBrush);
    DeleteObject(contentBrush);

    HPEN lightPen = CreatePen(PS_SOLID, 1, RGB(214, 214, 214));
    HPEN darkPen = CreatePen(PS_SOLID, 1, RGB(151, 151, 151));
    HGDIOBJ oldPen = SelectObject(hdc, lightPen);
    MoveToEx(hdc, m_x + 1, m_y + m_h - kBottomBarHeight, nullptr);
    LineTo(hdc, m_x + m_w - 2, m_y + m_h - kBottomBarHeight);
    SelectObject(hdc, darkPen);
    MoveToEx(hdc, m_x + 1, m_y + m_h - kBottomBarHeight + 1, nullptr);
    LineTo(hdc, m_x + m_w - 2, m_y + m_h - kBottomBarHeight + 1);

    MoveToEx(hdc, m_x + m_w - 10, m_y + m_h - 4, nullptr);
    LineTo(hdc, m_x + m_w - 4, m_y + m_h - 10);
    MoveToEx(hdc, m_x + m_w - 14, m_y + m_h - 4, nullptr);
    LineTo(hdc, m_x + m_w - 4, m_y + m_h - 14);
    MoveToEx(hdc, m_x + m_w - 18, m_y + m_h - 4, nullptr);
    LineTo(hdc, m_x + m_w - 4, m_y + m_h - 18);
    SelectObject(hdc, oldPen);
    DeleteObject(lightPen);
    DeleteObject(darkPen);
}

void UISkillListWnd::DrawBottomButton(HDC hdc, const TextButton& button) const
{
    DrawThreePieceBarPixels(hdc, button.rect, m_mesBtnLeftBitmap, m_mesBtnMidBitmap, m_mesBtnRightBitmap);
    RECT textRect{ button.rect.left, button.rect.top + 3, button.rect.right, button.rect.bottom };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
#if RO_ENABLE_QT6_UI
    DrawSkillButtonTextQt(hdc, textRect, button.label, RGB(0, 0, 0));
#else
    DrawTextA(hdc, button.label.c_str(), static_cast<int>(button.label.size()), &textRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
#endif
}

int UISkillListWnd::HitTestBottomButton(int globalX, int globalY) const
{
    for (size_t index = 0; index < m_bottomButtons.size(); ++index) {
        if (IsInsideRect(m_bottomButtons[index].rect, globalX, globalY)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

RECT UISkillListWnd::GetScrollTrackRect() const
{
    return RECT{
        m_x + m_w - kListRightMargin + 1,
        m_y + kListTop,
        m_x + m_w - 5,
        m_y + m_h - kBottomBarHeight - kListBottomMargin
    };
}

RECT UISkillListWnd::GetScrollThumbRect(int skillCount) const
{
    RECT trackRect = GetScrollTrackRect();
    const int maxOffset = GetMaxViewOffset(skillCount);
    const int trackHeight = trackRect.bottom - trackRect.top - 8;
    int thumbHeight = std::max(18, trackHeight / 4);
    if (maxOffset <= 0) {
        return RECT{ trackRect.left + 2, trackRect.top + 4, trackRect.right - 2, trackRect.top + 4 + thumbHeight };
    }

    const int thumbTop = trackRect.top + 4 + ((trackHeight - thumbHeight) * m_viewOffset) / maxOffset;
    return RECT{ trackRect.left + 2, thumbTop, trackRect.right - 2, thumbTop + thumbHeight };
}

bool UISkillListWnd::IsScrollBarVisible(int skillCount) const
{
    return GetMaxViewOffset(skillCount) > 0;
}

void UISkillListWnd::UpdateScrollFromThumbPosition(int globalY, int skillCount)
{
    const RECT trackRect = GetScrollTrackRect();
    const int maxOffset = GetMaxViewOffset(skillCount);
    if (maxOffset <= 0) {
        m_viewOffset = 0;
        return;
    }

    const RECT thumbRect = GetScrollThumbRect(skillCount);
    const int thumbHeight = thumbRect.bottom - thumbRect.top;
    const int minTop = trackRect.top + 4;
    const int maxTop = trackRect.bottom - 4 - thumbHeight;
    const int clampedTop = std::max(minTop, std::min(globalY, maxTop));
    const int denominator = std::max(1, maxTop - minTop);
    m_viewOffset = ((clampedTop - minTop) * maxOffset) / denominator;
}

std::vector<const PLAYER_SKILL_INFO*> UISkillListWnd::GetSortedSkills() const
{
    std::vector<const PLAYER_SKILL_INFO*> out;
    for (const PLAYER_SKILL_INFO& skill : g_session.GetSkillItems()) {
        // Match Ref UISkillListWnd refresh (message 23): show only learnable or learned skills.
        if (skill.upgradable == 0 && skill.level <= 0) {
            continue;
        }
        out.push_back(&skill);
    }

    std::sort(out.begin(), out.end(), [](const PLAYER_SKILL_INFO* lhs, const PLAYER_SKILL_INFO* rhs) {
        if (!lhs || !rhs) {
            return lhs != nullptr;
        }
        if (lhs->skillPos != rhs->skillPos) {
            return lhs->skillPos < rhs->skillPos;
        }
        return lhs->SKID < rhs->SKID;
    });
    return out;
}

int UISkillListWnd::GetMaxViewOffset(int skillCount) const
{
    const int visibleRows = std::max(1, (m_h - kTitleBarHeight - kBottomBarHeight - kListTop - kListBottomMargin) / kRowHeight);
    return std::max(0, skillCount - visibleRows);
}

const shopui::BitmapPixels* UISkillListWnd::GetSkillIcon(int skillId)
{
    const auto existing = m_iconCache.find(skillId);
    if (existing != m_iconCache.end()) {
        return existing->second.IsValid() ? &existing->second : nullptr;
    }

    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    std::string path = metadata ? ResolveSkillIconPath(*metadata) : g_skillMgr.GetSkillIconPath(skillId);
    shopui::BitmapPixels bitmap;
    if (!path.empty()) {
        bitmap = shopui::LoadBitmapPixelsFromGameData(path, true);
    }
    auto inserted = m_iconCache.emplace(skillId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

const PLAYER_SKILL_INFO* UISkillListWnd::GetSelectedSkill() const
{
    for (const PLAYER_SKILL_INFO& skill : g_session.GetSkillItems()) {
        if (skill.SKID == m_selectedSkillId) {
            return &skill;
        }
    }
    return nullptr;
}

unsigned long long UISkillListWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(m_hoveredRow)));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_viewOffset));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_selectedSkillId));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedUpgradeSkillId));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_isDraggingScrollThumb));
    HashTokenValue(&hash, static_cast<unsigned long long>(g_session.GetPlayerSkillPointCount()));
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragType));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.source));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.skillId));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.skillLevel));
    }

    const std::list<PLAYER_SKILL_INFO>& skillItems = g_session.GetSkillItems();
    HashTokenValue(&hash, static_cast<unsigned long long>(skillItems.size()));
    for (const PLAYER_SKILL_INFO& skill : skillItems) {
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.m_isValid));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.SKID));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.type));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.level));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.spcost));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.upgradable));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.attackRange));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.skillPos));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.skillMaxLv));
        HashTokenString(&hash, skill.skillIdName);
        HashTokenString(&hash, skill.skillName);
    }

    return hash;
}
