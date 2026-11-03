#include "UIPcGage.h"
#include "core/Vitals.h"

#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"

#include <algorithm>
#include <windows.h>

namespace {

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void DrawBar(HDC hdc, const RECT& rect, long long amount, long long totalAmount, COLORREF fillColor)
{
    FillRectColor(hdc, rect, RGB(20, 25, 155));

    RECT inner{ rect.left + 1, rect.top + 1, rect.right - 1, rect.bottom - 1 };
    FillRectColor(hdc, inner, RGB(30, 36, 58));

    const long long total = (std::max)(1LL, totalAmount);
    const long long clampedAmount = (std::max)(0LL, (std::min)(amount, total));
    const int innerWidth = inner.right - inner.left;
    if (innerWidth <= 0 || clampedAmount <= 0) {
        return;
    }

    const int fillWidth = VitalBarFill(clampedAmount, total, innerWidth);
    if (fillWidth <= 0) {
        return;
    }

    RECT fillRect{ inner.left, inner.top, inner.left + fillWidth, inner.bottom };
    FillRectColor(hdc, fillRect, fillColor);
}

} // namespace

UIPcGage::UIPcGage() = default;

void UIPcGage::SetHp(long long amount, long long totalAmount)
{
    m_hp = amount;
    m_maxHp = totalAmount;
    Invalidate();
}

void UIPcGage::SetSp(long long amount, long long totalAmount)
{
    m_sp = amount;
    m_maxSp = totalAmount;
    Invalidate();
}

void UIPcGage::SetMode(int mode)
{
    m_mode = mode;
    Invalidate();
}

long long UIPcGage::GetHp() const
{
    return m_hp;
}

long long UIPcGage::GetMaxHp() const
{
    return m_maxHp;
}

long long UIPcGage::GetSp() const
{
    return m_sp;
}

long long UIPcGage::GetMaxSp() const
{
    return m_maxSp;
}

int UIPcGage::GetMode() const
{
    return m_mode;
}

void UIPcGage::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_isDirty = 0;
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    if (m_mode == 0 || m_maxSp <= 0) {
        const RECT hpRect{ m_x, m_y, m_x + m_w, m_y + m_h };
        const bool danger = m_maxHp > 0 && (m_hp * 4) < m_maxHp;
        DrawBar(hdc, hpRect, m_hp, m_maxHp, danger ? RGB(234, 22, 22) : RGB(22, 234, 37));
    } else {
        const int hpHeight = (std::max)(3, (m_h - 1) / 2);
        const RECT hpRect{ m_x, m_y, m_x + m_w, m_y + hpHeight };
        const RECT spRect{ m_x, m_y + hpHeight + 1, m_x + m_w, m_y + m_h };
        const bool danger = m_maxHp > 0 && (m_hp * 4) < m_maxHp;
        DrawBar(hdc, hpRect, m_hp, m_maxHp, danger ? RGB(234, 22, 22) : RGB(22, 234, 37));
        DrawBar(hdc, spRect, m_sp, m_maxSp, RGB(29, 101, 219));
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
}