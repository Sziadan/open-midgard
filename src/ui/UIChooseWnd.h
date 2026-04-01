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
    void OnLBtnUp(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnKeyDown(int virtualKey);
    int GetSelectedIndex() const;

private:
    enum MenuEntry {
        MenuEntry_CharacterSelect = 0,
        MenuEntry_ReturnToGame = 1,
        MenuEntry_ExitToWindows = 2,
        MenuEntry_ReturnToSavePoint = 3,
        MenuEntry_Count = 4,
    };

    void EnsureCreated();
    void LayoutButtons();
    RECT GetEntryRect(int index) const;
    int HitTestEntry(int x, int y) const;
    void SyncSelectionVisuals();
    void UpdateSelectedIndexFromHover() const;
    void CloseMenu();
    msgresult_t ActivateSelection();

    bool m_controlsCreated;
    UIBitmapButton* m_entryButtons[MenuEntry_Count];
    int m_selectedIndex;
    int m_pressedIndex;
};