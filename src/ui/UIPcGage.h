#pragma once

#include "UIWindow.h"

class UIPcGage : public UIWindow {
public:
    UIPcGage();

    void SetHp(long long amount, long long totalAmount);
    void SetSp(long long amount, long long totalAmount);
    void SetMode(int mode);

    long long GetHp() const;
    long long GetMaxHp() const;
    long long GetSp() const;
    long long GetMaxSp() const;
    int GetMode() const;

    void OnDraw() override;

private:
    long long m_hp = 0;
    long long m_maxHp = 0;
    long long m_sp = 0;
    long long m_maxSp = 0;
    int m_mode = 0;
};