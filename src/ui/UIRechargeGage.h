#pragma once

#include "UIWindow.h"

// Ref UIRechargeGage — small horizontal cast / recharge progress bar.
class UIRechargeGage : public UIWindow {
public:
    UIRechargeGage();

    void SetAmount(int amount, int totalAmount);
    void OnDraw() override;
    int GetAmount() const;
    int GetTotalAmount() const;

private:
    int m_amount = 0;
    int m_totalAmount = 0;
    int m_edgeR = 82;
    int m_edgeG = 63;
    int m_edgeB = 45;
    int m_backR = 244;
    int m_backG = 239;
    int m_backB = 228;
    int m_r = 48;
    int m_g = 120;
    int m_b = 48;
};
