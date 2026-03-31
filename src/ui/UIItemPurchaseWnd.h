#pragma once

#include "UIFrameWnd.h"

class UIItemPurchaseWnd : public UIFrameWnd {
public:
    UIItemPurchaseWnd();
    ~UIItemPurchaseWnd() override;

    void SetShow(int show) override;
    bool IsUpdateNeed() override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnWheel(int delta) override;
    void HandleKeyDown(int virtualKey);
    int GetViewOffset() const;
    int GetHoverRow() const;
    int GetVisibleRowCountForQt() const;
    int GetHoverButton() const;
    int GetPressedButton() const;

private:
    enum ButtonId {
        ButtonNone = -1,
        ButtonAdd = 0,
        ButtonRemove = 1,
        ButtonConfirm = 2,
        ButtonCancel = 3,
    };

    RECT GetListRect() const;
    int GetVisibleRowCount() const;
    int GetMaxViewOffset() const;
    int HitTestDealRow(int x, int y) const;
    RECT GetButtonRect(ButtonId buttonId) const;
    ButtonId HitTestButton(int x, int y) const;
    void ActivateButton(ButtonId buttonId);
    unsigned long long BuildDisplayStateToken() const;

    int m_viewOffset;
    int m_hoverRow;
    ButtonId m_hoverButton;
    ButtonId m_pressedButton;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
};
