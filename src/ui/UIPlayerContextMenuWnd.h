#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <vector>

struct PlayerContextMenuEntry {
    int actionId = 0;
    std::string label;
};

class UIPlayerContextMenuWnd : public UIFrameWnd {
public:
    UIPlayerContextMenuWnd();
    ~UIPlayerContextMenuWnd() override;

    void SetShow(int show) override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnLBtnUp(int x, int y) override;

    void SetMenu(u32 targetAid,
        const std::string& targetName,
        const std::vector<PlayerContextMenuEntry>& options,
        int screenX,
        int screenY);
    void HideMenu();
    bool HandleKeyDown(int virtualKey);
    u32 GetTargetAid() const;
    const std::string& GetTargetName() const;
    const std::vector<std::string>& GetOptions() const;
    int GetSelectedIndex() const;
    int GetHoverIndex() const;
    bool IsOkPressed() const;
    bool IsCancelPressed() const;
    bool GetOkRectForQt(RECT* outRect) const;
    bool GetCancelRectForQt(RECT* outRect) const;

private:
    int GetOptionIndexAtPoint(int x, int y) const;
    RECT GetOptionRect(int index) const;
    void SetHighlightedIndex(int index);
    void SubmitSelection(int index);

    u32 m_targetAid;
    std::string m_targetName;
    std::vector<PlayerContextMenuEntry> m_entries;
    std::vector<std::string> m_optionLabels;
    int m_selectedIndex;
    int m_hoverIndex;
    int m_pressedIndex;
};