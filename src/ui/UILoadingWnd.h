#pragma once

#include "UIWindow.h"

#include <string>

class UILoadingWnd : public UIWindow {
public:
    UILoadingWnd();
    ~UILoadingWnd() override;

    void SetProgress(float progress);
    void SetMessage(const std::string& message);
    float GetProgress() const;
    const std::string& GetMessage() const;

    void OnCreate(int x, int y) override;
    void OnDraw() override;

private:
    float m_progress;
    std::string m_message;
};