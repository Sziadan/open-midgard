#include "UINotifyLevelUpWnd.h"

#include "UIBasicInfoWnd.h"
#include "UIWindowMgr.h"
#include "core/File.h"
#include "main/WinMain.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace {

constexpr int kNotifyButtonId = 126;
constexpr int kNotifyMarginLeft = 0;
constexpr int kNotifyMarginRight = 0;
constexpr int kNotifyMarginBottom = 0;

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

    for (int index = 0; pathPrefixes[index]; ++index) {
        AddUniqueCandidate(out, std::string(pathPrefixes[index]) + filenameOnly);
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

}

UINotifyLevelUpWnd::UINotifyLevelUpWnd()
    : m_controlsCreated(false)
    , m_button(nullptr)
{
    Create(1, 1);
    Move(0, 0);
}

UINotifyLevelUpWnd::~UINotifyLevelUpWnd() = default;

namespace {
void LayoutNotifyButton(UIBitmapButton* button, int x, int y)
{
    if (!button) {
        return;
    }
    button->Move(x, y);
}
}

void UINotifyLevelUpWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        UpdateAnchor();
    }
}

bool UINotifyLevelUpWnd::IsUpdateNeed()
{
    return m_show != 0 && m_isDirty != 0;
}

void UINotifyLevelUpWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }

    m_controlsCreated = true;

    m_button = new UIBitmapButton();
    const std::string offPath = ResolveUiAssetPath("lv_up_off.bmp");
    const std::string onPath = ResolveUiAssetPath("LV_UP_ON.BMP");
    m_button->SetBitmapName(offPath.c_str(), 0);
    m_button->SetBitmapName(offPath.c_str(), 1);
    m_button->SetBitmapName(onPath.c_str(), 2);
    m_button->Create((std::max)(1, m_button->m_bitmapWidth), (std::max)(1, m_button->m_bitmapHeight));
    m_button->m_id = kNotifyButtonId;
    AddChild(m_button);

    Resize((std::max)(1, m_button->m_w), (std::max)(1, m_button->m_h));
    UpdateAnchor();
}

void UINotifyLevelUpWnd::OnProcess()
{
    if (m_show == 0) {
        return;
    }

    EnsureCreated();
    UpdateAnchor();
}

void UINotifyLevelUpWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }

    DrawChildren();
}

msgresult_t UINotifyLevelUpWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    if (msg != 6 || wparam != kNotifyButtonId) {
        return UIFrameWnd::SendMsg(sender, msg, wparam, lparam, extra);
    }

    if (auto* basicInfo = static_cast<UIBasicInfoWnd*>(g_windowMgr.MakeWindow(UIWindowMgr::WID_BASICINFOWND))) {
        basicInfo->SendMsg(this, 6, 134, 0, 0);
        basicInfo->Invalidate();
    }

    g_windowMgr.DeleteWindow(this);
    return 0;
}

int UINotifyLevelUpWnd::GetTargetWindowId() const
{
    return UIWindowMgr::WID_NOTIFYLEVELUPWND;
}

void UINotifyLevelUpWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

void UINotifyLevelUpWnd::UpdateAnchor()
{
    if (!g_hMainWnd) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_hMainWnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return;
    }

    int x = 0;
    if (GetTargetWindowId() == UIWindowMgr::WID_NOTIFYJOBLEVELUPWND) {
        x = kNotifyMarginLeft;
    } else {
        x = clientWidth - m_w - kNotifyMarginRight;
    }
    int y = clientHeight - m_h - kNotifyMarginBottom;

    x = (std::max)(0, x);
    y = (std::max)(0, y);
    if (m_x != x || m_y != y) {
        UIWindow::Move(x, y);
    }
    LayoutNotifyButton(m_button, m_x, m_y);
}

UINotifyJobLevelUpWnd::UINotifyJobLevelUpWnd() = default;

UINotifyJobLevelUpWnd::~UINotifyJobLevelUpWnd() = default;

int UINotifyJobLevelUpWnd::GetTargetWindowId() const
{
    return UIWindowMgr::WID_NOTIFYJOBLEVELUPWND;
}