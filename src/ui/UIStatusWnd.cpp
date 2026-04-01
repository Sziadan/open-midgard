#include "UIStatusWnd.h"

#include "UIWindowMgr.h"
#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "res/Bitmap.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "msimg32.lib")

namespace {

constexpr int kWindowWidth = 280;
constexpr int kWindowHeight = 120;
constexpr int kMiniHeight = 17;
constexpr int kTitleBarHeight = 17;
constexpr int kBodyHeight = 103;
constexpr int kButtonIdBase = 134;
constexpr int kButtonIdClose = 135;
constexpr int kButtonIdMini = 136;
constexpr int kButtonIdIncStr = 139;
constexpr int kButtonIdIncAgi = 140;
constexpr int kButtonIdIncVit = 141;
constexpr int kButtonIdIncInt = 142;
constexpr int kButtonIdIncDex = 143;
constexpr int kButtonIdIncLuk = 144;

constexpr std::array<int, 6> kIncrementButtonIds = {
    kButtonIdIncStr,
    kButtonIdIncAgi,
    kButtonIdIncVit,
    kButtonIdIncInt,
    kButtonIdIncDex,
    kButtonIdIncLuk,
};

constexpr std::array<u16, 6> kStatusIds = {
    13,
    14,
    15,
    16,
    17,
    18,
};

constexpr std::array<int, 6> kStatRows = { 6, 22, 38, 54, 70, 86 };
constexpr std::array<int, 6> kRightLeftRows = { 21, 37, 53, 69, 85, 101 };
constexpr std::array<int, 4> kRightRightRows = { 21, 37, 53, 69 };
constexpr const char* kExternalSkinDir = "D:\\Spel\\OldRO\\skin\\default\\basic_interface\\";
constexpr int kQtButtonWidth = 12;
constexpr int kQtButtonHeight = 11;

RECT MakeStatusRect(int x, int y, int left, int top, int width, int height)
{
    RECT rect{ x + left, y + top, x + left + width, y + top + height };
    return rect;
}

bool IsPointInRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
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
        kExternalSkinDir,
        nullptr
    };

    std::string base = NormalizeSlash(fileName);
    AddUniqueCandidate(out, base);

    std::string fileNameOnly = base;
    const size_t slashPos = fileNameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < fileNameOnly.size()) {
        fileNameOnly = fileNameOnly.substr(slashPos + 1);
    }

    for (int index = 0; prefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(prefixes[index]) + fileNameOnly);
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

void HashValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashValue(hash, static_cast<unsigned long long>(ch));
    }
    HashValue(hash, 0xFFull);
}

HFONT GetStatusFont()
{
    static HFONT s_font = nullptr;
    if (s_font) {
        return s_font;
    }

    s_font = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma");
    return s_font;
}

std::string FormatCompositeValue(int primary, int secondary)
{
    char text[64] = {};
    if (secondary > 0) {
        std::snprintf(text, sizeof(text), "%d + %d", primary, secondary);
    } else {
        std::snprintf(text, sizeof(text), "%d", primary);
    }
    return text;
}

std::string FormatMatkValue(int minValue, int maxValue)
{
    char text[64] = {};
    if (minValue == maxValue) {
        std::snprintf(text, sizeof(text), "%d", maxValue);
    } else {
        std::snprintf(text, sizeof(text), "%d~%d", minValue, maxValue);
    }
    return text;
}

std::string FormatStatValue(int baseValue, int plusValue)
{
    char text[64] = {};
    if (plusValue > 0) {
        std::snprintf(text, sizeof(text), "%d+%d", baseValue, plusValue);
    } else {
        std::snprintf(text, sizeof(text), "%d", baseValue);
    }
    return text;
}

} // namespace

UIStatusWnd::UIStatusWnd()
    : m_controlsCreated(false),
      m_page(0),
      m_systemButtons{ nullptr, nullptr, nullptr },
      m_incrementButtons{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },
      m_titleBarBitmap(),
      m_pageBackgrounds{},
      m_lastDrawStateToken(0ull),
      m_hasDrawStateToken(false)
{
    Create(kWindowWidth, kWindowHeight);
    Move(185, 300);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("StatusWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UIStatusWnd::~UIStatusWnd()
{
    ReleaseAssets();
}

void UIStatusWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        LayoutChildren();
        RefreshIncrementButtons();
    }
}

void UIStatusWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    if (m_controlsCreated) {
        LayoutChildren();
    }
}

bool UIStatusWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasDrawStateToken) {
        return true;
    }
    return BuildDisplayStateToken() != m_lastDrawStateToken;
}

msgresult_t UIStatusWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    switch (wparam) {
    case kButtonIdBase:
        SetMiniMode(false);
        return 1;
    case kButtonIdMini:
        SetMiniMode(true);
        return 1;
    case kButtonIdClose:
        SetShow(0);
        return 1;
    case kButtonIdIncStr:
    case kButtonIdIncAgi:
    case kButtonIdIncVit:
    case kButtonIdIncInt:
    case kButtonIdIncDex:
    case kButtonIdIncLuk: {
        const int index = static_cast<int>(wparam) - kButtonIdIncStr;
        if (index >= 0 && index < static_cast<int>(kStatusIds.size())) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestIncreaseStatus, kStatusIds[static_cast<size_t>(index)], 1, 0);
        }
        return 1;
    }
    default:
        return 1;
    }
}

void UIStatusWnd::OnCreate(int x, int y)
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
        RefreshIncrementButtons();
        return;
    }

    struct ButtonSpec {
        const char* offName;
        const char* onName;
        int id;
        const char* tooltip;
    };

    const std::array<ButtonSpec, 3> topButtons = {{
        { "sys_base_off.bmp", "sys_base_on.bmp", kButtonIdBase, "Base" },
        { "sys_mini_off.bmp", "sys_mini_on.bmp", kButtonIdMini, "Mini" },
        { "sys_close_off.bmp", "sys_close_on.bmp", kButtonIdClose, "Close" },
    }};

    for (size_t index = 0; index < topButtons.size(); ++index) {
        auto* button = new UIBitmapButton();
        button->SetBitmapName(ResolveUiAssetPath(topButtons[index].offName).c_str(), 0);
        button->SetBitmapName(ResolveUiAssetPath(topButtons[index].onName).c_str(), 1);
        button->SetBitmapName(ResolveUiAssetPath(topButtons[index].onName).c_str(), 2);
        button->Create(button->m_bitmapWidth, button->m_bitmapHeight);
        button->m_id = topButtons[index].id;
        button->SetToolTip(topButtons[index].tooltip);
        AddChild(button);
        m_systemButtons[index] = button;
    }

    const std::string arrowOff = ResolveUiAssetPath("arw_right.bmp");
    const std::string arrowOn = ResolveUiAssetPath("arw_right_on.bmp");
    for (size_t index = 0; index < m_incrementButtons.size(); ++index) {
        auto* button = new UIBitmapButton();
        button->SetBitmapName(arrowOff.c_str(), 0);
        button->SetBitmapName(arrowOn.c_str(), 1);
        button->SetBitmapName(arrowOn.c_str(), 2);
        button->Create(button->m_bitmapWidth, button->m_bitmapHeight);
        button->m_id = kIncrementButtonIds[index];
        AddChild(button);
        m_incrementButtons[index] = button;
    }

    LayoutChildren();
    RefreshIncrementButtons();
}

void UIStatusWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawStateToken = BuildDisplayStateToken();
        m_hasDrawStateToken = true;
        m_isDirty = 0;
        return;
    }

    if (m_show == 0) {
        return;
    }

    EnsureCreated();
    RefreshIncrementButtons();

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    RECT titleRect{ m_x, m_y, m_x + m_w, m_y + kTitleBarHeight };
    shopui::DrawBitmapPixelsTransparent(hdc, m_titleBarBitmap, titleRect);
    DrawWindowText(hdc, m_x + 18, m_y + 3, "Status", RGB(255, 255, 255));
    DrawWindowText(hdc, m_x + 17, m_y + 2, "Status", RGB(0, 0, 0));

    if (m_h > kMiniHeight) {
        RECT bodyRect{ m_x, m_y + kTitleBarHeight, m_x + m_w, m_y + kTitleBarHeight + kBodyHeight };
        shopui::DrawBitmapPixelsTransparent(hdc, m_pageBackgrounds[m_page == 0 ? 0 : 1], bodyRect);

        if (m_page == 0) {
            const DisplayData data = BuildDisplayData();
            for (size_t index = 0; index < data.baseStats.size(); ++index) {
                const int rowY = m_y + kStatRows[index] + 16;
                const std::string statText = FormatStatValue(data.baseStats[index], data.plusStats[index]);
                DrawWindowText(hdc, m_x + 54, rowY, statText.c_str(), RGB(0, 0, 0));
            }

            DrawRightAlignedValue(hdc, m_x + 192, m_y + kRightLeftRows[0], FormatCompositeValue(data.attack, data.refineAttack));
            DrawRightAlignedValue(hdc, m_x + 192, m_y + kRightLeftRows[1], FormatMatkValue(data.matkMin, data.matkMax));
            DrawRightAlignedValue(hdc, m_x + 192, m_y + kRightLeftRows[2], std::to_string(data.hit));
            DrawRightAlignedValue(hdc, m_x + 192, m_y + kRightLeftRows[3], std::to_string(data.critical));
            DrawRightAlignedValue(hdc, m_x + 273, m_y + kRightLeftRows[4], std::to_string(data.statusPoint));

            DrawRightAlignedValue(hdc, m_x + 273, m_y + kRightRightRows[0], FormatCompositeValue(data.itemDef, data.plusDef));
            DrawRightAlignedValue(hdc, m_x + 273, m_y + kRightRightRows[1], FormatCompositeValue(data.itemMdef, data.plusMdef));
            DrawRightAlignedValue(hdc, m_x + 273, m_y + kRightRightRows[2], FormatCompositeValue(data.flee, data.plusFlee));
            DrawRightAlignedValue(hdc, m_x + 273, m_y + kRightRightRows[3], FormatCompositeValue(data.aspd, data.plusAspd));
        }
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    m_isDirty = 0;
}

void UIStatusWnd::OnLBtnDblClk(int x, int y)
{
    if (y >= m_y && y < m_y + kTitleBarHeight) {
        SetMiniMode(m_h != kMiniHeight);
        return;
    }
    UIFrameWnd::OnLBtnDblClk(x, y);
}

void UIStatusWnd::OnLBtnDown(int x, int y)
{
    if (IsQtUiRuntimeEnabled()) {
        const RECT baseRect = MakeStatusRect(m_x, m_y, 3, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT miniRect = MakeStatusRect(m_x, m_y, 252, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT closeRect = MakeStatusRect(m_x, m_y, 266, 3, kQtButtonWidth, kQtButtonHeight);
        if (IsPointInRect(baseRect, x, y) || IsPointInRect(miniRect, x, y) || IsPointInRect(closeRect, x, y)) {
            UIWindow::OnLBtnDown(x, y);
            return;
        }
    }

    if (m_h > kMiniHeight && x >= m_x && x < m_x + 20 && y >= m_y + kTitleBarHeight) {
        if (y < m_y + 43) {
            SetPage(0);
        } else if (y < m_y + 67) {
            SetPage(1);
        }
    }

    if (y < m_y + kTitleBarHeight) {
        UIFrameWnd::OnLBtnDown(x, y);
    } else {
        UIWindow::OnLBtnDown(x, y);
    }
}

void UIStatusWnd::OnLBtnUp(int x, int y)
{
    if (IsQtUiRuntimeEnabled()) {
        const bool wasDragging = m_isDragging != 0;
        UIFrameWnd::OnLBtnUp(x, y);
        if (wasDragging) {
            return;
        }

        const RECT baseRect = MakeStatusRect(m_x, m_y, 3, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT miniRect = MakeStatusRect(m_x, m_y, 252, 3, kQtButtonWidth, kQtButtonHeight);
        const RECT closeRect = MakeStatusRect(m_x, m_y, 266, 3, kQtButtonWidth, kQtButtonHeight);

        if (m_h == kMiniHeight && IsPointInRect(baseRect, x, y)) {
            SendMsg(this, 6, kButtonIdBase, 0, 0);
            return;
        }
        if (m_h > kMiniHeight && IsPointInRect(miniRect, x, y)) {
            SendMsg(this, 6, kButtonIdMini, 0, 0);
            return;
        }
        if (IsPointInRect(closeRect, x, y)) {
            SendMsg(this, 6, kButtonIdClose, 0, 0);
            return;
        }

        if (m_h > kMiniHeight && m_page == 0) {
            const DisplayData data = BuildDisplayData();
            for (size_t index = 0; index < kIncrementButtonIds.size(); ++index) {
                if (data.statCosts[index] <= 0
                    || data.baseStats[index] >= 99
                    || data.statCosts[index] > data.statusPoint) {
                    continue;
                }

                const RECT incrementRect = MakeStatusRect(
                    m_x,
                    m_y,
                    92,
                    23 + static_cast<int>(index) * 16,
                    kQtButtonWidth,
                    kQtButtonHeight);
                if (IsPointInRect(incrementRect, x, y)) {
                    SendMsg(this, 6, kIncrementButtonIds[index], 0, 0);
                    return;
                }
            }
        }
        return;
    }

    UIFrameWnd::OnLBtnUp(x, y);
}

void UIStatusWnd::OnMouseHover(int x, int y)
{
    UIFrameWnd::OnMouseHover(x, y);
}

void UIStatusWnd::StoreInfo()
{
    SaveUiWindowPlacement("StatusWnd", m_x, m_y);
}

bool UIStatusWnd::IsMiniMode() const
{
    return m_h == kMiniHeight;
}

int UIStatusWnd::GetPageForQt() const
{
    return m_page;
}

bool UIStatusWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    *outData = BuildDisplayData();
    return true;
}

int UIStatusWnd::GetQtSystemButtonCount() const
{
    return 3;
}

bool UIStatusWnd::GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* out) const
{
    if (!out || index < 0 || index >= GetQtSystemButtonCount()) {
        return false;
    }

    switch (index) {
    case 0:
        out->id = kButtonIdBase;
        out->x = m_x + 3;
        out->y = m_y + 3;
        out->width = kQtButtonWidth;
        out->height = kQtButtonHeight;
        out->label = "B";
        out->visible = IsMiniMode();
        out->active = false;
        return true;
    case 1:
        out->id = kButtonIdMini;
        out->x = m_x + 252;
        out->y = m_y + 3;
        out->width = kQtButtonWidth;
        out->height = kQtButtonHeight;
        out->label = "_";
        out->visible = !IsMiniMode();
        out->active = false;
        return true;
    case 2:
        out->id = kButtonIdClose;
        out->x = m_x + 266;
        out->y = m_y + 3;
        out->width = kQtButtonWidth;
        out->height = kQtButtonHeight;
        out->label = "X";
        out->visible = true;
        out->active = false;
        return true;
    default:
        return false;
    }
}

int UIStatusWnd::GetQtPageTabCount() const
{
    return 2;
}

bool UIStatusWnd::GetQtPageTabDisplayForQt(int index, QtButtonDisplay* out) const
{
    if (!out || index < 0 || index >= GetQtPageTabCount()) {
        return false;
    }

    out->id = index;
    out->x = m_x;
    out->y = m_y + kTitleBarHeight + (index == 0 ? 0 : 26);
    out->width = 20;
    out->height = index == 0 ? 26 : 24;
    out->label = index == 0 ? "1" : "2";
    out->visible = !IsMiniMode();
    out->active = m_page == index;
    return true;
}

int UIStatusWnd::GetQtIncrementButtonCount() const
{
    return static_cast<int>(kIncrementButtonIds.size());
}

bool UIStatusWnd::GetQtIncrementButtonDisplayForQt(int index, QtButtonDisplay* out) const
{
    if (!out || index < 0 || index >= GetQtIncrementButtonCount()) {
        return false;
    }

    const DisplayData data = BuildDisplayData();
    out->id = kIncrementButtonIds[static_cast<size_t>(index)];
    out->x = m_x + 88;
    out->y = m_y + 17 + kStatRows[static_cast<size_t>(index)];
    out->width = kQtButtonWidth;
    out->height = kQtButtonHeight;
    out->label = ">";
    out->visible =
        !IsMiniMode()
        && m_page == 0
        && data.statCosts[static_cast<size_t>(index)] > 0
        && data.baseStats[static_cast<size_t>(index)] < 99
        && data.statCosts[static_cast<size_t>(index)] <= data.statusPoint;
    out->active = false;
    return true;
}

void UIStatusWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

void UIStatusWnd::LayoutChildren()
{
    if (!m_controlsCreated) {
        return;
    }

    if (m_systemButtons[0]) {
        m_systemButtons[0]->Move(m_x + 3, m_y + 3);
        m_systemButtons[0]->SetShow(m_h == kMiniHeight ? 1 : 0);
    }
    if (m_systemButtons[1]) {
        m_systemButtons[1]->Move(m_x + 252, m_y + 3);
        m_systemButtons[1]->SetShow(m_h == kMiniHeight ? 0 : 1);
    }
    if (m_systemButtons[2]) {
        m_systemButtons[2]->Move(m_x + 266, m_y + 3);
        m_systemButtons[2]->SetShow(1);
    }

    for (size_t index = 0; index < m_incrementButtons.size(); ++index) {
        if (!m_incrementButtons[index]) {
            continue;
        }
        m_incrementButtons[index]->Move(m_x + 88, m_y + 17 + kStatRows[index]);
    }
}

void UIStatusWnd::RefreshIncrementButtons()
{
    if (!m_controlsCreated) {
        return;
    }

    const DisplayData data = BuildDisplayData();
    for (size_t index = 0; index < m_incrementButtons.size(); ++index) {
        UIBitmapButton* const button = m_incrementButtons[index];
        if (!button) {
            continue;
        }

        const bool canIncrease =
            m_show != 0 &&
            m_h > kMiniHeight &&
            m_page == 0 &&
            data.statCosts[index] > 0 &&
            data.baseStats[index] < 99 &&
            data.statCosts[index] <= data.statusPoint;
        button->SetShow(canIncrease ? 1 : 0);
    }
}

void UIStatusWnd::SetMiniMode(bool miniMode)
{
    Resize(kWindowWidth, miniMode ? kMiniHeight : kWindowHeight);
    LayoutChildren();
    RefreshIncrementButtons();
    Invalidate();
}

void UIStatusWnd::SetPage(int page)
{
    const int normalizedPage = page == 0 ? 0 : 1;
    if (m_page == normalizedPage) {
        return;
    }
    m_page = normalizedPage;
    RefreshIncrementButtons();
    Invalidate();
}

UIStatusWnd::DisplayData UIStatusWnd::BuildDisplayData() const
{
    DisplayData data{};

    if (const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo()) {
        data.baseStats[0] = static_cast<int>(info->Str);
        data.baseStats[1] = static_cast<int>(info->Agi);
        data.baseStats[2] = static_cast<int>(info->Vit);
        data.baseStats[3] = static_cast<int>(info->Int);
        data.baseStats[4] = static_cast<int>(info->Dex);
        data.baseStats[5] = static_cast<int>(info->Luk);
        data.statusPoint = (std::max)(0, static_cast<int>(info->sppoint));
    }

    data.plusStats[0] = (std::max)(0, g_session.m_plusStr);
    data.plusStats[1] = (std::max)(0, g_session.m_plusAgi);
    data.plusStats[2] = (std::max)(0, g_session.m_plusVit);
    data.plusStats[3] = (std::max)(0, g_session.m_plusInt);
    data.plusStats[4] = (std::max)(0, g_session.m_plusDex);
    data.plusStats[5] = (std::max)(0, g_session.m_plusLuk);

    data.statCosts[0] = (std::max)(0, g_session.m_standardStr);
    data.statCosts[1] = (std::max)(0, g_session.m_standardAgi);
    data.statCosts[2] = (std::max)(0, g_session.m_standardVit);
    data.statCosts[3] = (std::max)(0, g_session.m_standardInt);
    data.statCosts[4] = (std::max)(0, g_session.m_standardDex);
    data.statCosts[5] = (std::max)(0, g_session.m_standardLuk);

    data.attack = (std::max)(0, g_session.m_attPower);
    data.refineAttack = (std::max)(0, g_session.m_refiningPower);
    data.matkMax = (std::max)(0, g_session.m_maxMatkPower);
    data.matkMin = (std::max)(0, g_session.m_minMatkPower);
    data.itemDef = (std::max)(0, g_session.m_itemDefPower);
    data.plusDef = (std::max)(0, g_session.m_plusDefPower);
    data.itemMdef = (std::max)(0, g_session.m_mdefPower);
    data.plusMdef = (std::max)(0, g_session.m_plusMdefPower);
    data.hit = (std::max)(0, g_session.m_hitSuccessValue);
    data.flee = (std::max)(0, g_session.m_avoidSuccessValue);
    data.plusFlee = (std::max)(0, g_session.m_plusAvoidSuccessValue);
    data.critical = (std::max)(0, g_session.m_criticalSuccessValue);
    data.aspd = (std::max)(0, g_session.m_aspd);
    data.plusAspd = (std::max)(0, g_session.m_plusAspd);

    if (g_world.m_player) {
        if (data.baseStats[0] <= 0) {
            data.baseStats[0] = static_cast<int>(g_world.m_player->m_Str);
        }
        if (data.baseStats[1] <= 0) {
            data.baseStats[1] = static_cast<int>(g_world.m_player->m_Agi);
        }
        if (data.baseStats[2] <= 0) {
            data.baseStats[2] = static_cast<int>(g_world.m_player->m_Vit);
        }
        if (data.baseStats[3] <= 0) {
            data.baseStats[3] = static_cast<int>(g_world.m_player->m_Int);
        }
        if (data.baseStats[4] <= 0) {
            data.baseStats[4] = static_cast<int>(g_world.m_player->m_Dex);
        }
        if (data.baseStats[5] <= 0) {
            data.baseStats[5] = static_cast<int>(g_world.m_player->m_Luk);
        }
    }

    return data;
}

unsigned long long UIStatusWnd::BuildDisplayStateToken() const
{
    const DisplayData data = BuildDisplayData();

    unsigned long long hash = 1469598103934665603ull;
    HashValue(&hash, static_cast<unsigned long long>(m_show));
    HashValue(&hash, static_cast<unsigned long long>(m_x));
    HashValue(&hash, static_cast<unsigned long long>(m_y));
    HashValue(&hash, static_cast<unsigned long long>(m_w));
    HashValue(&hash, static_cast<unsigned long long>(m_h));
    HashValue(&hash, static_cast<unsigned long long>(m_page));

    for (int value : data.baseStats) {
        HashValue(&hash, static_cast<unsigned long long>(value));
    }
    for (int value : data.plusStats) {
        HashValue(&hash, static_cast<unsigned long long>(value));
    }
    for (int value : data.statCosts) {
        HashValue(&hash, static_cast<unsigned long long>(value));
    }

    HashValue(&hash, static_cast<unsigned long long>(data.attack));
    HashValue(&hash, static_cast<unsigned long long>(data.refineAttack));
    HashValue(&hash, static_cast<unsigned long long>(data.matkMin));
    HashValue(&hash, static_cast<unsigned long long>(data.matkMax));
    HashValue(&hash, static_cast<unsigned long long>(data.itemDef));
    HashValue(&hash, static_cast<unsigned long long>(data.plusDef));
    HashValue(&hash, static_cast<unsigned long long>(data.itemMdef));
    HashValue(&hash, static_cast<unsigned long long>(data.plusMdef));
    HashValue(&hash, static_cast<unsigned long long>(data.hit));
    HashValue(&hash, static_cast<unsigned long long>(data.flee));
    HashValue(&hash, static_cast<unsigned long long>(data.plusFlee));
    HashValue(&hash, static_cast<unsigned long long>(data.critical));
    HashValue(&hash, static_cast<unsigned long long>(data.aspd));
    HashValue(&hash, static_cast<unsigned long long>(data.plusAspd));
    HashValue(&hash, static_cast<unsigned long long>(data.statusPoint));
    return hash;
}

void UIStatusWnd::DrawWindowText(HDC hdc, int x, int y, const char* text, COLORREF color, UINT format) const
{
    RECT rect = { x, y, m_x + m_w - 4, y + 16 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, GetStatusFont());
    DrawTextA(hdc, text ? text : "", -1, &rect, format);
    SelectObject(hdc, oldFont);
}

void UIStatusWnd::DrawRightAlignedValue(HDC hdc, int right, int y, const std::string& text) const
{
    RECT rect = { m_x, y, right, y + 16 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    HGDIOBJ oldFont = SelectObject(hdc, GetStatusFont());
    DrawTextA(hdc, text.c_str(), -1, &rect, DT_RIGHT | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

void UIStatusWnd::LoadAssets()
{
    if (!m_titleBarBitmap.IsValid()) {
        m_titleBarBitmap = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("titlebar_fix.bmp"));
    }
    if (!m_pageBackgrounds[0].IsValid()) {
        m_pageBackgrounds[0] = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("statwin0_bg.bmp"));
    }
    if (!m_pageBackgrounds[1].IsValid()) {
        m_pageBackgrounds[1] = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("statwin1_bg.bmp"));
    }
}

void UIStatusWnd::ReleaseAssets()
{
    m_titleBarBitmap.Clear();
    m_pageBackgrounds[0].Clear();
    m_pageBackgrounds[1].Clear();
}
