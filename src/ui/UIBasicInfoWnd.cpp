#include "UIBasicInfoWnd.h"

#include "UIWindowMgr.h"
#include "core/File.h"
#include "res/Bitmap.h"
#include "qtui/QtUiRuntime.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kFullWidth = 280;
constexpr int kFullHeight = 120;
constexpr int kMiniHeight = 34;
constexpr int kTitleBarHeight = 17;
constexpr int kBarWidth = 85;
constexpr int kBarHeight = 9;
constexpr int kExpBarWidth = 102;
constexpr int kExpBarHeight = 6;
constexpr int kQtMenuButtonWidth = 32;
constexpr int kQtMenuButtonHeight = 20;
constexpr int kQtTopButtonWidth = 11;
constexpr int kQtTopButtonHeight = 11;
constexpr int kTopButtonY = 3;
constexpr int kBaseButtonX = 3;
constexpr int kMiniButtonX = 266;
constexpr int kButtonIdStatus = 126;
constexpr int kButtonIdOption = 127;
constexpr int kButtonIdItems = 128;
constexpr int kButtonIdEquip = 129;
constexpr int kButtonIdSkill = 130;
constexpr int kButtonIdFriend = 133;
constexpr int kButtonIdBase = 134;
constexpr int kButtonIdMini = 136;
constexpr int kButtonIdComm = 148;
constexpr int kButtonIdMap = 153;

constexpr std::array<int, 8> kMenuButtonIds = {
    kButtonIdStatus,
    kButtonIdOption,
    kButtonIdItems,
    kButtonIdEquip,
    kButtonIdSkill,
    kButtonIdMap,
    kButtonIdComm,
    kButtonIdFriend,
};

constexpr std::array<const char*, 8> kMenuButtonNames = {
    "status",
    "option",
    "items",
    "equip",
    "skill",
    "map",
    "comm",
    "friend",
};

constexpr std::array<const char*, 8> kMenuButtonTooltips = {
    "Alt+A",
    "Alt+O",
    "Alt+E",
    "Alt+Q",
    "Alt+S",
    "Ctrl+Tab",
    "Alt+C",
    "Alt+Z",
};

RECT MakeBasicInfoRect(int x, int y, int left, int top, int width, int height)
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
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

std::string NormalizeJobDisplayName(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());

    bool startOfWord = true;
    bool previousWasSpace = false;
    for (unsigned char ch : raw) {
        if (ch == '_') {
            if (!out.empty() && !previousWasSpace) {
                out.push_back(' ');
            }
            startOfWord = true;
            previousWasSpace = true;
            continue;
        }

        if (std::isspace(ch)) {
            if (!out.empty() && !previousWasSpace) {
                out.push_back(' ');
            }
            startOfWord = true;
            previousWasSpace = true;
            continue;
        }

        if (std::isalpha(ch)) {
            out.push_back(static_cast<char>(startOfWord ? std::toupper(ch) : std::tolower(ch)));
        } else {
            out.push_back(static_cast<char>(ch));
        }

        startOfWord = false;
        previousWasSpace = false;
    }

    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

HFONT GetBasicInfoSmallFont()
{
    static HFONT s_font = nullptr;
    if (s_font) {
        return s_font;
    }
    s_font = CreateFontA(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Tahoma");
    return s_font;
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

void DrawBitmapPixelsTransparent(HDC target, const shopui::BitmapPixels& bitmap, int x, int y, int width = -1, int height = -1)
{
    if (!target || !bitmap.IsValid()) {
        return;
    }

    const int drawWidth = width > 0 ? width : bitmap.width;
    const int drawHeight = height > 0 ? height : bitmap.height;
    AlphaBlendArgbToHdc(target,
                        x,
                        y,
                        drawWidth,
                        drawHeight,
                        bitmap.pixels.data(),
                        bitmap.width,
                        bitmap.height);
}

std::string CopyCharacterName(const CHARACTER_INFO& info)
{
    char buffer[25] = {};
    std::memcpy(buffer, info.name, sizeof(info.name));
    buffer[sizeof(info.name)] = '\0';
    return buffer;
}

std::string FormatNumber(int value)
{
    char source[32] = {};
    std::snprintf(source, sizeof(source), "%d", value);

    std::string out;
    const bool negative = value < 0;
    const char* digits = negative ? source + 1 : source;
    const size_t digitCount = std::strlen(digits);
    for (size_t index = 0; index < digitCount; ++index) {
        if (index != 0 && (digitCount - index) % 3 == 0) {
            out += ',';
        }
        out += digits[index];
    }
    if (negative) {
        out.insert(out.begin(), '-');
    }
    return out;
}

int ClampPercent(int value)
{
    return (std::max)(0, (std::min)(100, value));
}

} // namespace

UIBasicInfoWnd::UIBasicInfoWnd()
    : m_controlsCreated(false),
      m_menuButtons{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },
      m_baseButton(nullptr),
      m_miniButton(nullptr),
      m_backgroundFull(),
      m_backgroundMini(),
      m_barBackground(),
      m_redLeft(),
      m_redMid(),
      m_redRight(),
      m_blueLeft(),
      m_blueMid(),
      m_blueRight(),
      m_lastDrawStateToken(0ull),
      m_hasDrawStateToken(false)
{
    Create(kFullWidth, kFullHeight);
    Move(0, 0);
    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement("BasicInfoWnd", &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UIBasicInfoWnd::~UIBasicInfoWnd()
{
    ReleaseAssets();
}

void UIBasicInfoWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        LayoutChildren();
    }
}

void UIBasicInfoWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    if (m_controlsCreated) {
        LayoutChildren();
    }
}

bool UIBasicInfoWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasDrawStateToken) {
        return true;
    }
    return BuildDisplayStateToken() != m_lastDrawStateToken;
}

msgresult_t UIBasicInfoWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;

    if (msg != 6) {
        return 0;
    }

    switch (wparam) {
    case kButtonIdStatus:
        g_windowMgr.ToggleWindow(UIWindowMgr::WID_STATUSWND);
        return 1;
    case kButtonIdBase:
        SetMiniMode(false);
        return 1;
    case kButtonIdMini:
        SetMiniMode(true);
        return 1;
    case kButtonIdItems:
        g_windowMgr.ToggleWindow(UIWindowMgr::WID_ITEMWND);
        return 1;
    case kButtonIdEquip:
        g_windowMgr.ToggleWindow(UIWindowMgr::WID_EQUIPWND);
        return 1;
    case kButtonIdSkill:
        g_windowMgr.ToggleWindow(UIWindowMgr::WID_SKILLLISTWND);
        return 1;
    case kButtonIdOption:
        g_windowMgr.ToggleWindow(UIWindowMgr::WID_OPTIONWND);
        return 1;
    case kButtonIdMap:
        g_windowMgr.ToggleWindow(UIWindowMgr::WID_ROMAPWND);
        return 1;
    default:
        return 1;
    }
}

void UIBasicInfoWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;
    LoadAssets();

    if (IsQtUiRuntimeEnabled()) {
        SetMiniMode(false);
        return;
    }

    for (size_t index = 0; index < kMenuButtonNames.size(); ++index) {
        auto* button = new UIBitmapButton();
        const std::string offName = std::string("btn_") + kMenuButtonNames[index] + "_off.bmp";
        const std::string onName = std::string("btn_") + kMenuButtonNames[index] + "_on.bmp";
        const std::string offPath = ResolveUiAssetPath(offName.c_str());
        const std::string onPath = ResolveUiAssetPath(onName.c_str());
        button->SetBitmapName(offPath.c_str(), 0);
        button->SetBitmapName(onPath.c_str(), 1);
        button->SetBitmapName(onPath.c_str(), 2);
        button->Create(button->m_bitmapWidth, button->m_bitmapHeight);
        button->m_id = kMenuButtonIds[index];
        button->SetToolTip(kMenuButtonTooltips[index]);
        AddChild(button);
        m_menuButtons[index] = button;
    }

    m_baseButton = new UIBitmapButton();
    m_baseButton->SetBitmapName(ResolveUiAssetPath("sys_base_off.bmp").c_str(), 0);
    m_baseButton->SetBitmapName(ResolveUiAssetPath("sys_base_on.bmp").c_str(), 1);
    m_baseButton->SetBitmapName(ResolveUiAssetPath("sys_base_on.bmp").c_str(), 2);
    m_baseButton->Create(m_baseButton->m_bitmapWidth, m_baseButton->m_bitmapHeight);
    m_baseButton->m_id = kButtonIdBase;
    m_baseButton->SetToolTip("Base");
    AddChild(m_baseButton);

    m_miniButton = new UIBitmapButton();
    m_miniButton->SetBitmapName(ResolveUiAssetPath("sys_mini_off.bmp").c_str(), 0);
    m_miniButton->SetBitmapName(ResolveUiAssetPath("sys_mini_on.bmp").c_str(), 1);
    m_miniButton->SetBitmapName(ResolveUiAssetPath("sys_mini_on.bmp").c_str(), 2);
    m_miniButton->Create(m_miniButton->m_bitmapWidth, m_miniButton->m_bitmapHeight);
    m_miniButton->m_id = kButtonIdMini;
    m_miniButton->SetToolTip("Mini");
    AddChild(m_miniButton);

    SetMiniMode(false);
}

void UIBasicInfoWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawStateToken = BuildDisplayStateToken();
        m_hasDrawStateToken = true;
        m_isDirty = 0;
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    EnsureCreated();

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const DisplayData data = BuildDisplayData();
    if (m_h == kMiniHeight) {
        DrawCachedBitmap(hdc, m_backgroundMini, m_x, m_y);

        DrawWindowText(hdc, m_x + 17, m_y + 3, data.name.c_str(), RGB(0, 0, 0));

        char topLine[128] = {};
        std::snprintf(topLine, sizeof(topLine), "Lv. %2d / %s / Exp. %d %%", data.level, data.jobName.c_str(), data.expPercent);
        DrawWindowText(hdc, m_x + 264, m_y + 2, topLine, RGB(0, 0, 0), DT_RIGHT | DT_TOP | DT_SINGLELINE);

        const std::string zenyText = FormatNumber(data.money);
        char bottomLine[160] = {};
        std::snprintf(bottomLine,
            sizeof(bottomLine),
            "HP %3d / %3d  |  SP %3d / %3d  |  %s Z",
            data.hp,
            data.maxHp,
            data.sp,
            data.maxSp,
            zenyText.c_str());
        DrawWindowText(hdc, m_x + 275, m_y + 18, bottomLine, RGB(0, 0, 0), DT_RIGHT | DT_TOP | DT_SINGLELINE);
    } else {
        DrawCachedBitmap(hdc, m_backgroundFull, m_x, m_y);

        DrawWindowText(hdc, m_x + 18, m_y + 3, "Basic Info", RGB(255, 255, 255));
        DrawWindowText(hdc, m_x + 17, m_y + 2, "Basic Info", RGB(0, 0, 0));
        DrawWindowText(hdc, m_x + 9, m_y + 23, data.name.c_str(), RGB(0, 0, 0));
        DrawWindowText(hdc, m_x + 9, m_y + 36, data.jobName.c_str(), RGB(0, 0, 0));

        const int hpPercent = data.maxHp > 0 ? (data.hp * 100) / data.maxHp : 0;
        DrawBar(hdc, m_x + 110, m_y + 22, hpPercent, hpPercent < 25);
        DrawBar(hdc, m_x + 110, m_y + 43, data.maxSp > 0 ? (data.sp * 100) / data.maxSp : 0, false);

        char text[128] = {};
        std::snprintf(text, sizeof(text), "HP      %3d  /  %3d", data.hp, data.maxHp);
        DrawWindowText(hdc, m_x + 95, m_y + 30, text, RGB(0, 0, 0), DT_LEFT | DT_TOP | DT_SINGLELINE, GetBasicInfoSmallFont(), 15);

        std::snprintf(text, sizeof(text), "SP      %3d  /  %3d", data.sp, data.maxSp);
        DrawWindowText(hdc, m_x + 95, m_y + 51, text, RGB(0, 0, 0), DT_LEFT | DT_TOP | DT_SINGLELINE, GetBasicInfoSmallFont(), 15);

        std::snprintf(text, sizeof(text), "Base Lv. %d", data.level);
        DrawWindowText(hdc, m_x + 17, m_y + 72, text, RGB(0, 0, 0));
        DrawExpBar(hdc, m_x + 84, m_y + 77, data.expPercent);

        std::snprintf(text, sizeof(text), "Job Lv. %d", data.jobLevel);
        DrawWindowText(hdc, m_x + 17, m_y + 82, text, RGB(0, 0, 0));
        DrawExpBar(hdc, m_x + 84, m_y + 87, data.jobExpPercent);

        std::snprintf(text, sizeof(text), "Weight : %3d / %3d", data.weight, data.maxWeight);
        const COLORREF weightColor = (data.maxWeight > 0 && data.weight * 100 >= data.maxWeight * 50) ? RGB(255, 0, 0) : RGB(0, 0, 0);
        DrawWindowText(hdc, m_x + 5, m_y + 103, text, weightColor);

        const std::string zenyText = FormatNumber(data.money);
        std::snprintf(text, sizeof(text), "Zeny : %s", zenyText.c_str());
        DrawWindowText(hdc, m_x + 107, m_y + 103, text, RGB(0, 0, 0));
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    m_isDirty = 0;
}

void UIBasicInfoWnd::OnLBtnDown(int x, int y)
{
    if (IsQtUiRuntimeEnabled()) {
        const RECT baseRect = MakeBasicInfoRect(m_x, m_y, kBaseButtonX, kTopButtonY, kQtTopButtonWidth, kQtTopButtonHeight);
        const RECT miniRect = MakeBasicInfoRect(m_x, m_y, kMiniButtonX, kTopButtonY, kQtTopButtonWidth, kQtTopButtonHeight);
        if (IsPointInRect(baseRect, x, y) || IsPointInRect(miniRect, x, y)) {
            UIWindow::OnLBtnDown(x, y);
            return;
        }
    }

    UIFrameWnd::OnLBtnDown(x, y);
}

void UIBasicInfoWnd::OnLBtnUp(int x, int y)
{
    if (IsQtUiRuntimeEnabled()) {
        const bool wasDragging = m_isDragging != 0;
        UIFrameWnd::OnLBtnUp(x, y);
        if (wasDragging) {
            return;
        }

        if (m_h == kMiniHeight) {
            const RECT baseRect = MakeBasicInfoRect(m_x, m_y, kBaseButtonX, kTopButtonY, kQtTopButtonWidth, kQtTopButtonHeight);
            if (IsPointInRect(baseRect, x, y)) {
                SendMsg(this, 6, kButtonIdBase, 0, 0);
            }
            return;
        }

        const RECT miniRect = MakeBasicInfoRect(m_x, m_y, kMiniButtonX, kTopButtonY, kQtTopButtonWidth, kQtTopButtonHeight);
        if (IsPointInRect(miniRect, x, y)) {
            SendMsg(this, 6, kButtonIdMini, 0, 0);
            return;
        }

        for (size_t index = 0; index < kMenuButtonIds.size(); ++index) {
            const RECT buttonRect = MakeBasicInfoRect(
                m_x,
                m_y,
                207 + static_cast<int>(36 * (index % 2)),
                22 + static_cast<int>(24 * (index / 2)),
                kQtMenuButtonWidth,
                kQtMenuButtonHeight);
            if (IsPointInRect(buttonRect, x, y)) {
                SendMsg(this, 6, kMenuButtonIds[index], 0, 0);
                return;
            }
        }
        return;
    }

    UIFrameWnd::OnLBtnUp(x, y);
}

void UIBasicInfoWnd::OnLBtnDblClk(int x, int y)
{
    if (y >= m_y && y < m_y + kTitleBarHeight) {
        SetMiniMode(m_h != kMiniHeight);
        return;
    }
    UIFrameWnd::OnLBtnDblClk(x, y);
}

void UIBasicInfoWnd::OnMouseHover(int x, int y)
{
    UIFrameWnd::OnMouseHover(x, y);
}

void UIBasicInfoWnd::StoreInfo()
{
    SaveUiWindowPlacement("BasicInfoWnd", m_x, m_y);
}

void UIBasicInfoWnd::NewHeight(int height)
{
    Resize(kFullWidth, height == kMiniHeight ? kMiniHeight : kFullHeight);
    LayoutChildren();
}

bool UIBasicInfoWnd::IsMiniMode() const
{
    return m_h == kMiniHeight;
}

bool UIBasicInfoWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    *outData = BuildDisplayData();
    return true;
}

void UIBasicInfoWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

void UIBasicInfoWnd::LayoutChildren()
{
    if (!m_controlsCreated) {
        return;
    }

    for (size_t index = 0; index < m_menuButtons.size(); ++index) {
        if (!m_menuButtons[index]) {
            continue;
        }
        m_menuButtons[index]->Move(m_x + 207 + static_cast<int>(36 * (index % 2)), m_y + 22 + static_cast<int>(24 * (index / 2)));
        m_menuButtons[index]->SetShow(m_h == kFullHeight ? 1 : 0);
    }

    if (m_baseButton) {
        m_baseButton->Move(m_x + kBaseButtonX, m_y + kTopButtonY);
        m_baseButton->SetShow(m_h == kMiniHeight ? 1 : 0);
    }
    if (m_miniButton) {
        m_miniButton->Move(m_x + kMiniButtonX, m_y + kTopButtonY);
        m_miniButton->SetShow(m_h == kFullHeight ? 1 : 0);
    }
}

void UIBasicInfoWnd::SetMiniMode(bool miniMode)
{
    NewHeight(miniMode ? kMiniHeight : kFullHeight);
}

UIBasicInfoWnd::DisplayData UIBasicInfoWnd::BuildDisplayData() const
{
    DisplayData data{};
    data.name = g_session.GetPlayerName() ? g_session.GetPlayerName() : "";
    data.jobName = g_session.GetJobName(g_session.m_playerJob) ? g_session.GetJobName(g_session.m_playerJob) : "";

    if (const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo()) {
        if (data.name.empty()) {
            data.name = CopyCharacterName(*info);
        }
        data.jobName = g_session.GetJobName(info->job) ? g_session.GetJobName(info->job) : data.jobName;
        data.level = info->level;
        data.jobLevel = info->joblevel;
        data.hp = info->hp;
        data.maxHp = info->maxhp;
        data.sp = info->sp;
        data.maxSp = info->maxsp;
        data.money = info->money;
        data.expPercent = ClampPercent(info->exp);
        data.jobExpPercent = ClampPercent(info->jobexp);
    }

    if (g_world.m_player) {
        data.hp = static_cast<int>(g_world.m_player->m_Hp);
        data.maxHp = static_cast<int>(g_world.m_player->m_MaxHp);
        data.sp = static_cast<int>(g_world.m_player->m_Sp);
        data.maxSp = static_cast<int>(g_world.m_player->m_MaxSp);
        if (data.level <= 0) {
            data.level = static_cast<int>(g_world.m_player->m_clevel);
        }
        if (data.jobName.empty()) {
            data.jobName = g_session.GetJobName(g_world.m_player->m_job) ? g_session.GetJobName(g_world.m_player->m_job) : "";
        }
    }

    int liveBaseExpPercent = 0;
    if (g_session.TryGetBaseExpPercent(&liveBaseExpPercent)) {
        data.expPercent = liveBaseExpPercent;
    }

    int liveJobExpPercent = 0;
    if (g_session.TryGetJobExpPercent(&liveJobExpPercent)) {
        data.jobExpPercent = liveJobExpPercent;
    }

    data.level = (std::max)(data.level, 0);
    data.jobLevel = (std::max)(data.jobLevel, 0);
    data.hp = (std::max)(data.hp, 0);
    data.maxHp = (std::max)(data.maxHp, data.hp);
    data.sp = (std::max)(data.sp, 0);
    data.maxSp = (std::max)(data.maxSp, data.sp);
    data.jobName = NormalizeJobDisplayName(data.jobName);
    data.weight = 0;
    data.maxWeight = 0;
    return data;
}

unsigned long long UIBasicInfoWnd::BuildDisplayStateToken() const
{
    const DisplayData data = BuildDisplayData();

    unsigned long long hash = 1469598103934665603ull;
    const auto mixValue = [&hash](unsigned long long value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    const auto mixString = [&mixValue](const std::string& value) {
        for (unsigned char ch : value) {
            mixValue(static_cast<unsigned long long>(ch));
        }
        mixValue(0xFFull);
    };

    mixValue(static_cast<unsigned long long>(m_show));
    mixValue(static_cast<unsigned long long>(m_x));
    mixValue(static_cast<unsigned long long>(m_y));
    mixValue(static_cast<unsigned long long>(m_w));
    mixValue(static_cast<unsigned long long>(m_h));
    mixString(data.name);
    mixString(data.jobName);
    mixValue(static_cast<unsigned long long>(data.level));
    mixValue(static_cast<unsigned long long>(data.jobLevel));
    mixValue(static_cast<unsigned long long>(data.hp));
    mixValue(static_cast<unsigned long long>(data.maxHp));
    mixValue(static_cast<unsigned long long>(data.sp));
    mixValue(static_cast<unsigned long long>(data.maxSp));
    mixValue(static_cast<unsigned long long>(data.money));
    mixValue(static_cast<unsigned long long>(data.weight));
    mixValue(static_cast<unsigned long long>(data.maxWeight));
    mixValue(static_cast<unsigned long long>(data.expPercent));
    mixValue(static_cast<unsigned long long>(data.jobExpPercent));
    return hash;
}

void UIBasicInfoWnd::DrawCachedBitmap(HDC hdc, const shopui::BitmapPixels& bitmap, int x, int y) const
{
    DrawBitmapPixelsTransparent(hdc, bitmap, x, y);
}

void UIBasicInfoWnd::DrawBar(HDC hdc, int x, int y, int percent, bool redBar) const
{
    DrawBitmapPixelsTransparent(hdc, m_barBackground, x, y, kBarWidth, kBarHeight);

    const shopui::BitmapPixels& left = redBar ? m_redLeft : m_blueLeft;
    const shopui::BitmapPixels& mid = redBar ? m_redMid : m_blueMid;
    const shopui::BitmapPixels& right = redBar ? m_redRight : m_blueRight;
    if (!left.IsValid() || !mid.IsValid() || !right.IsValid()) {
        return;
    }

    const int fillWidth = kBarWidth * ClampPercent(percent) / 100;
    if (fillWidth <= 0) {
        return;
    }

    if (fillWidth <= left.width) {
        DrawBitmapPixelsTransparent(hdc, left, x, y, fillWidth, left.height);
        return;
    }

    DrawBitmapPixelsTransparent(hdc, left, x, y, left.width, left.height);
    int remainingWidth = fillWidth - left.width;
    int cursorX = x + left.width;

    const int rightWidth = remainingWidth > right.width ? right.width : 0;
    const int midWidth = (std::max)(0, remainingWidth - rightWidth);
    if (midWidth > 0) {
        DrawBitmapPixelsTransparent(hdc, mid, cursorX, y, midWidth, mid.height);
        cursorX += midWidth;
    }
    if (rightWidth > 0) {
        DrawBitmapPixelsTransparent(hdc, right, cursorX, y, rightWidth, right.height);
    }
}

void UIBasicInfoWnd::DrawExpBar(HDC hdc, int x, int y, int percent) const
{
    RECT outerRect = { x, y, x + kExpBarWidth, y + kExpBarHeight };
    HBRUSH backgroundBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &outerRect, backgroundBrush);
    DeleteObject(backgroundBrush);

    HBRUSH borderBrush = CreateSolidBrush(RGB(56, 64, 88));
    FrameRect(hdc, &outerRect, borderBrush);
    DeleteObject(borderBrush);

    const int clampedPercent = ClampPercent(percent);
    const int innerWidth = (outerRect.right - outerRect.left - 2) * clampedPercent / 100;
    if (innerWidth <= 0) {
        return;
    }

    RECT fillRect = { x + 1, y + 1, x + 1 + innerWidth, y + kExpBarHeight - 1 };
    HBRUSH fillBrush = CreateSolidBrush(RGB(129, 146, 199));
    FillRect(hdc, &fillRect, fillBrush);
    DeleteObject(fillBrush);
}

void UIBasicInfoWnd::DrawWindowText(HDC hdc, int x, int y, const char* text, COLORREF color, UINT format, HFONT font, int height) const
{
    RECT rect = { x, y, m_x + m_w - 4, y + height };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text ? text : "", -1, &rect, format);
    SelectObject(hdc, oldFont);
}

void UIBasicInfoWnd::LoadAssets()
{
    if (!m_backgroundFull.IsValid()) {
        m_backgroundFull = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("basewin_bg.bmp"));
    }
    if (!m_backgroundMini.IsValid()) {
        m_backgroundMini = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("basewin_mini.bmp"));
    }
    if (!m_barBackground.IsValid()) {
        m_barBackground = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("GZE_BG.BMP"));
    }
    if (!m_redLeft.IsValid()) {
        m_redLeft = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("gzered_left.bmp"));
    }
    if (!m_redMid.IsValid()) {
        m_redMid = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("gzered_mid.bmp"));
    }
    if (!m_redRight.IsValid()) {
        m_redRight = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("gzered_right.bmp"));
    }
    if (!m_blueLeft.IsValid()) {
        m_blueLeft = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("gzeblue_left.bmp"));
    }
    if (!m_blueMid.IsValid()) {
        m_blueMid = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("gzeblue_mid.bmp"));
    }
    if (!m_blueRight.IsValid()) {
        m_blueRight = LoadBitmapPixelsFromGameData(ResolveUiAssetPath("gzeblue_right.bmp"));
    }
}

void UIBasicInfoWnd::ReleaseAssets()
{
    m_backgroundFull.Clear();
    m_backgroundMini.Clear();
    m_barBackground.Clear();
    m_redLeft.Clear();
    m_redMid.Clear();
    m_redRight.Clear();
    m_blueLeft.Clear();
    m_blueMid.Clear();
    m_blueRight.Clear();
}