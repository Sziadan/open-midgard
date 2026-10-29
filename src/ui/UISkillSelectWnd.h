#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <string>
#include <unordered_map>
#include <vector>

// Selection dialog for skills that wait on a player choice after the cast.
// Warp Portal and Teleport level 2 list map names from ZC_WARPLIST (0x011C).
// Create Arrow lists material items from ZC_MAKINGARROW_LIST (0x01AD).
class UISkillSelectWnd : public UIFrameWnd {
public:
    struct DisplayButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool hovered = false;
        bool pressed = false;
        bool enabled = true;
        std::string label;
    };

    struct DisplayScrollBar {
        bool visible = false;
        int trackX = 0;
        int trackY = 0;
        int trackWidth = 0;
        int trackHeight = 0;
        int thumbX = 0;
        int thumbY = 0;
        int thumbWidth = 0;
        int thumbHeight = 0;
    };

    struct DisplayRow {
        unsigned int itemId = 0;
        bool identified = true;
        bool showIcon = false;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool hovered = false;
        bool selected = false;
        int count = 0;
        std::string label;
    };

    struct DisplayData {
        std::string title;
        std::string emptyText;
        DisplayButton closeButton;
        DisplayButton okButton;
        DisplayButton cancelButton;
        DisplayScrollBar scrollBar;
        std::vector<DisplayRow> rows;
    };

    UISkillSelectWnd();
    ~UISkillSelectWnd() override = default;

    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;
    bool HandleKeyDown(int virtualKey);

    void SetWarpDestinations(unsigned short skillId, const std::vector<std::string>& mapNames);
    void SetMakingArrowItems(const std::vector<unsigned int>& itemIds);
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    enum class Kind {
        None = 0,
        Warp,
        Arrow,
    };

    struct Entry {
        std::string label;
        std::string mapName;
        unsigned int itemId = 0;
        int count = 0;
        bool showIcon = false;
    };

    RECT GetCloseButtonRect() const;
    RECT GetListRect() const;
    RECT GetRowRect(int visibleRow) const;
    RECT GetScrollTrackRect() const;
    RECT GetScrollThumbRect(int entryCount) const;
    RECT GetOkButtonRect() const;
    RECT GetCancelButtonRect() const;
    int GetMaxViewOffset(int entryCount) const;
    bool IsScrollBarVisible(int entryCount) const;
    void ClampViewOffset(int entryCount);
    void UpdateScrollFromThumbPosition(int globalY, int entryCount);
    void UpdateInteraction(int x, int y);
    int HitTestVisibleRow(int x, int y) const;
    bool HasValidSelection() const;
    void SubmitSelection();
    void CancelSelection();
    void ResetInteraction();
    const shopui::BitmapPixels* GetItemIcon(unsigned int itemId);

    Kind m_kind;
    unsigned short m_skillId;
    std::string m_title;
    std::string m_emptyText;
    std::vector<Entry> m_entries;
    bool m_closeHovered;
    bool m_closePressed;
    bool m_okHovered;
    bool m_okPressed;
    bool m_cancelHovered;
    bool m_cancelPressed;
    int m_hoveredIndex;
    int m_selectedIndex;
    int m_viewOffset;
    bool m_isDraggingScrollThumb;
    int m_scrollDragOffsetY;
    std::unordered_map<unsigned int, shopui::BitmapPixels> m_iconCache;
};
