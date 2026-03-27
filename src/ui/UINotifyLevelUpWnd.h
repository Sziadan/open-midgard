#pragma once

#include "UIFrameWnd.h"

class UIBitmapButton;

class UINotifyLevelUpWnd : public UIFrameWnd {
public:
    UINotifyLevelUpWnd();
    ~UINotifyLevelUpWnd() override;

    void SetShow(int show) override;
    void OnCreate(int x, int y) override;
    void OnProcess() override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;

protected:
    virtual int GetTargetWindowId() const;

private:
    void EnsureCreated();
    void UpdateAnchor();

    bool m_controlsCreated;
    UIBitmapButton* m_button;
};

class UINotifyJobLevelUpWnd : public UINotifyLevelUpWnd {
public:
    UINotifyJobLevelUpWnd();
    ~UINotifyJobLevelUpWnd() override;

protected:
    int GetTargetWindowId() const override;
};