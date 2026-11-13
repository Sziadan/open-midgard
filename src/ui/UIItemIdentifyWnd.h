#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ITEM_INFO;

class UIItemIdentifyWnd : public UIFrameWnd {
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
        unsigned int itemIndex = 0;
        unsigned int itemId = 0;
        bool identified = false;
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

    UIItemIdentifyWnd();
    ~UIItemIdentifyWnd() override = default;

    void OnDraw() override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;
    void SetIdentifyItemIndices(const std::vector<unsigned int>& itemIndices);
    bool GetDisplayDataForQt(DisplayData* outData) const;

private:
    RECT GetCloseButtonRect() const;
    RECT GetListRect() const;
    RECT GetRowRect(int visibleRow) const;
    RECT GetScrollTrackRect() const;
    RECT GetScrollThumbRect(int itemCount) const;
    RECT GetOkButtonRect() const;
    RECT GetCancelButtonRect() const;
    int GetMaxViewOffset(int itemCount) const;
    bool IsScrollBarVisible(int itemCount) const;
    void ClampViewOffset(int itemCount);
    void UpdateScrollFromThumbPosition(int globalY, int itemCount);
    void UpdateInteraction(int x, int y);
    int HitTestVisibleRow(int x, int y) const;
    void SubmitSelection();
    void CancelSelection();
    std::vector<const ITEM_INFO*> GetAvailableItems() const;
    const shopui::BitmapPixels* GetItemIcon(const ITEM_INFO& item);
    bool HasValidSelection() const;

    struct Candidate {
        unsigned int index;
        unsigned int itemId;
    };
    std::vector<Candidate> m_candidates;
    bool m_closeHovered;
    bool m_closePressed;
    bool m_okHovered;
    bool m_okPressed;
    bool m_cancelHovered;
    bool m_cancelPressed;
    bool m_hasStoredPlacement;
    int m_hoveredItemIndex;
    int m_selectedItemIndex;
    int m_viewOffset;
    bool m_isDraggingScrollThumb;
    int m_scrollDragOffsetY;
    std::unordered_map<unsigned long long, shopui::BitmapPixels> m_iconCache;
};
