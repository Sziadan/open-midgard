#include "UIMonsterInfoWnd.h"

#include "UIShopCommon.h"
#include "UIWindowMgr.h"

UIMonsterInfoWnd::UIMonsterInfoWnd()
{
    Create(400, 200);
    Move(0, 0);
}

void UIMonsterInfoWnd::SetEstimate(const Estimate& estimate)
{
    m_estimate = estimate;
    m_closePressed = false;
    Invalidate();
}

RECT UIMonsterInfoWnd::GetCloseRect() const
{
    return shopui::MakeRect(m_x + m_w - 66, m_y + m_h - 28, 56, 20);
}

void UIMonsterInfoWnd::OnDraw()
{
    if (!m_show) return;
    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) return;

    shopui::DrawFrameWindow(hdc, shopui::MakeRect(m_x, m_y, m_w, m_h), "Monster Information");
    const auto drawText = [&](int x, int y, int width, const std::string& text) {
        shopui::DrawWindowTextRect(hdc, shopui::MakeRect(m_x + x, m_y + y, width, 18), text,
            RGB(0, 0, 0), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    };
    const auto drawField = [&](int x, int y, const char* label, const std::string& value) {
        drawText(x, y, 52, label);
        drawText(x + 54, y, 81, value);
    };
    static constexpr const char* sizes[] = {"Small", "Medium", "Large"};
    static constexpr const char* races[] = {
        "Formless", "Undead", "Animal", "Plant", "Insect", "Marine", "Devil", "Human", "Angel", "Dragon"};
    static constexpr const char* elements[] = {
        "Neutral", "Water", "Earth", "Fire", "Wind", "Poison", "Holy", "Shadow", "Ghost", "Undead"};
    drawField(120, 30, "Name", "Monster");
    drawField(120, 50, "Level", std::to_string(m_estimate.level));
    drawField(120, 70, "HP", std::to_string(m_estimate.hp));
    drawField(120, 90, "DEF", std::to_string(m_estimate.defense));
    drawField(260, 30, "Size", m_estimate.size < std::size(sizes) ? sizes[m_estimate.size] : "Unknown");
    drawField(260, 50, "Race", m_estimate.race < std::size(races) ? races[m_estimate.race] : "Unknown");
    drawField(260, 70, "Element", m_estimate.element < std::size(elements) ? elements[m_estimate.element] : "Unknown");
    drawField(260, 90, "MDEF", std::to_string(m_estimate.magicDefense));

    for (std::size_t index = 0; index < m_estimate.rates.size(); ++index) {
        const int x = 120 + static_cast<int>(index / 3) * 80;
        const int y = 110 + static_cast<int>(index % 3) * 18;
        drawText(x, y, 43, elements[index + 1]);
        drawText(x + 43, y, 37, std::to_string(m_estimate.rates[index]) + "%");
    }
    shopui::FillRectColor(hdc, GetCloseRect(), m_closePressed ? RGB(185, 199, 222) : RGB(231, 226, 214));
    shopui::FrameRectColor(hdc, GetCloseRect(), RGB(127, 122, 112));
    shopui::DrawWindowTextRect(hdc, GetCloseRect(), "Close", RGB(0, 0, 0), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ReleaseMainWindowDrawTarget(hdc);
}

void UIMonsterInfoWnd::OnLBtnDown(int x, int y)
{
    const RECT close = GetCloseRect();
    m_closePressed = PtInRect(&close, POINT{x, y}) != 0;
    if (m_closePressed) Invalidate();
    else UIFrameWnd::OnLBtnDown(x, y);
}

void UIMonsterInfoWnd::OnLBtnUp(int x, int y)
{
    const RECT close = GetCloseRect();
    if (m_closePressed && PtInRect(&close, POINT{x, y})) {
        g_windowMgr.DeleteWindow(this);
        return;
    }
    m_closePressed = false;
    Invalidate();
    UIFrameWnd::OnLBtnUp(x, y);
}

void UIMonsterInfoWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE || virtualKey == VK_RETURN) g_windowMgr.DeleteWindow(this);
    else UIFrameWnd::OnKeyDown(virtualKey);
}
