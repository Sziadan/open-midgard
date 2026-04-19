#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <vector>

class UIMessengerGroupWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool visible = true;
        bool active = false;
    };

    struct DisplayRow {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool selected = false;
        unsigned int color = 0;
        std::string name;
        std::string status;
        std::string detail;
    };

    struct DisplayData {
        std::string title;
        std::string summaryTitle;
        std::string summaryValue;
        std::string summarySecondaryValue;
        int currentTab = 0;
        int selectedIndex = -1;
        int viewOffset = 0;
        int maxViewOffset = 0;
        bool scrollBarVisible = false;
        int scrollTrackX = 0;
        int scrollTrackY = 0;
        int scrollTrackWidth = 0;
        int scrollTrackHeight = 0;
        int scrollThumbX = 0;
        int scrollThumbY = 0;
        int scrollThumbWidth = 0;
        int scrollThumbHeight = 0;
        std::vector<QtButtonDisplay> actionButtons;
        std::vector<DisplayRow> rows;
    };

    UIMessengerGroupWnd();
    ~UIMessengerGroupWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;

    bool GetDisplayDataForQt(DisplayData* outData) const;
    int GetQtSystemButtonCount() const;
    bool GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const;
    int GetQtTabCount() const;
    bool GetQtTabDisplayForQt(int index, QtButtonDisplay* outData) const;

private:
    enum TabId {
        TabFriend = 0,
        TabParty = 1,
    };

    void NormalizeStateForData();
    void SetCurrentTab(int tabId);
    int GetEntryCount() const;
    int GetMaxViewOffset() const;
    int GetVisibleRowCount() const;
    int GetRowAtPoint(int x, int y) const;
    RECT GetCloseButtonRect() const;
    RECT GetTabRect(int index) const;
    RECT GetListRect() const;
    RECT GetRowRect(int visibleIndex) const;
    RECT GetActionButtonRect(int slot) const;
    RECT GetScrollTrackRect() const;
    RECT GetScrollThumbRect() const;
    unsigned long long BuildVisualStateToken() const;

    int m_currentTab;
    int m_selectedIndex;
    int m_viewOffset;
    bool m_controlsCreated;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
};