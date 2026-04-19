#pragma once

#include "UIWindow.h"

class UIPcGage : public UIWindow {
public:
    UIPcGage();

    void SetHp(int amount, int totalAmount);
    void SetSp(int amount, int totalAmount);
    void SetMode(int mode);

    int GetHp() const;
    int GetMaxHp() const;
    int GetSp() const;
    int GetMaxSp() const;
    int GetMode() const;

    void OnDraw() override;

private:
    int m_hp = 0;
    int m_maxHp = 0;
    int m_sp = 0;
    int m_maxSp = 0;
    int m_mode = 0;
};