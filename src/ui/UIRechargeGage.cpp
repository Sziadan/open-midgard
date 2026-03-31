#include "UIRechargeGage.h"

#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"

#include <algorithm>
#include <windows.h>

UIRechargeGage::UIRechargeGage() = default;

void UIRechargeGage::SetAmount(int amount, int totalAmount)
{
    m_amount = amount;
    m_totalAmount = totalAmount;
    Invalidate();
}

int UIRechargeGage::GetAmount() const
{
    return m_amount;
}

int UIRechargeGage::GetTotalAmount() const
{
    return m_totalAmount;
}

void UIRechargeGage::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_isDirty = 0;
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    bool useShared = false;
    HDC hdc = AcquireDrawTarget(&useShared);
    if (!hdc) {
        return;
    }

    RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    HBRUSH edgeBrush = CreateSolidBrush(RGB(m_edgeR, m_edgeG, m_edgeB));
    FillRect(hdc, &outer, edgeBrush);
    DeleteObject(edgeBrush);

    RECT inner{ m_x + 1, m_y + 1, m_x + m_w - 1, m_y + m_h - 1 };
    HBRUSH backBrush = CreateSolidBrush(RGB(m_backR, m_backG, m_backB));
    FillRect(hdc, &inner, backBrush);
    DeleteObject(backBrush);

    const int total = (std::max)(1, m_totalAmount);
    const double pct = 100.0 * static_cast<double>(m_amount) / static_cast<double>(total);
    if (pct > 0.0) {
        const int fillW = static_cast<int>(static_cast<double>(m_w) * pct * 0.01 - 2.0);
        if (fillW > 0) {
            RECT fill{ m_x + 1, m_y + 1, m_x + 1 + fillW, m_y + m_h - 1 };
            HBRUSH fillBrush = CreateSolidBrush(RGB(m_r, m_g, m_b));
            FillRect(hdc, &fill, fillBrush);
            DeleteObject(fillBrush);
        }
    }

    DrawChildrenToCurrentTarget(hdc, useShared);
    ReleaseDrawTarget(hdc, useShared);
}
