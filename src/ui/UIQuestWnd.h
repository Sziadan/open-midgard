#pragma once

#include "UIFrameWnd.h"
#include "core/QuestDisplayCatalog.h"

#include <string>
#include <vector>

class UIQuestWnd final : public UIFrameWnd {
public:
    struct Row {
        u32 id;
        std::string title;
        bool active;
        bool selected;
    };

    struct Display {
        std::vector<Row> rows;
        std::string title;
        std::string description;
        std::string expiry;
        std::vector<std::string> objectives;
        std::string action;
        bool previous = false;
        bool next = false;
    };

    explicit UIQuestWnd(int windowId);
    Display GetDisplay() const;
    Display GetDisplay(u64 now) const;
    void OnDraw() override;
    bool IsUpdateNeed() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnWheel(int delta) override;

private:
    int Hit(int x, int y) const;
    u32 SelectedId() const;
    std::size_t PageOffset() const;
    std::string Title(u32 id) const;

    ro::QuestDisplayCatalog m_catalog;
    u32 m_selectedId = 0;
    u32 m_pressedId = 0;
    std::size_t m_offset = 0;
    int m_pressed = 0;
    bool m_requestedActive = false;
};
