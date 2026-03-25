#pragma once

#include "UIFrameWnd.h"

class UIBitmapButton;

class UIChooseWnd : public UIFrameWnd {
public:
    UIChooseWnd();
    ~UIChooseWnd() override;

    void SetShow(int show) override;
    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    int SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    void OnKeyDown(int virtualKey);

private:
    enum MenuEntry {
        MenuEntry_CharacterSelect = 0,
        MenuEntry_GameSettings = 1,
        MenuEntry_ExitToWindows = 2,
        MenuEntry_ReturnToGame = 3,
        MenuEntry_Count = 4,
    };

    void EnsureCreated();
    void LayoutButtons();
    void SyncSelectionVisuals();
    void UpdateSelectedIndexFromHover() const;
    void CloseMenu();
    int ActivateSelection();

    bool m_controlsCreated;
    UIBitmapButton* m_entryButtons[MenuEntry_Count];
    int m_selectedIndex;
};