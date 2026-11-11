#pragma once

#include "UIFrameWnd.h"

#include <array>

class UIMonsterInfoWnd : public UIFrameWnd {
public:
    struct Estimate {
        u16 job = 0;
        u16 level = 0;
        u16 size = 0;
        u32 hp = 0;
        s16 defense = 0;
        u16 race = 0;
        s16 magicDefense = 0;
        u16 element = 0;
        std::array<u8, 9> rates{};
    };

    UIMonsterInfoWnd();
    void SetEstimate(const Estimate& estimate);
    const Estimate& GetEstimate() const { return m_estimate; }

    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnKeyDown(int virtualKey) override;

private:
    RECT GetCloseRect() const;

    Estimate m_estimate;
    bool m_closePressed = false;
};
