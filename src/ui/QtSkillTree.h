#pragma once
#if RO_ENABLE_QT6_UI
#include "SkillTreeModel.h"
#include <QVariantMap>
#include <map>
#include <cstdint>
class UISkillListWnd;
class QtSkillTree {
public:
    explicit QtSkillTree(UISkillListWnd& owner);
    void Tick();
    QVariantMap Data() const;
    bool Press(int x, int y);
    bool Release(int x, int y);
    bool Move(int x, int y);
    void Wheel(int delta);
    void Describe(int x, int y);
    void CancelCapture();
private:
    using Position = skilltree::Position;
    UISkillListWnd& m_owner;
    skilltree::Model m_model;
    skilltree::ApplyQueue m_apply;
    std::map<int, Position> m_positions;
    int m_job = -1, m_character = -1;
    int m_scrollX = 0, m_scrollY = 0, m_contentWidth = 0, m_contentHeight = 0;
    int m_hover = 0, m_pressed = 0, m_pressedPart = 0;
    int m_startX = 0, m_startY = 0, m_originX = 0, m_originY = 0;
    bool m_pan = false, m_dragged = false, m_collapsed = false;
    int m_fullHeight = 500;
    std::string m_jobName, m_catalogError;
    void LoadJob();
    void Layout();
    void ClampScroll();
    int Hit(int x, int y, int* part = nullptr) const;
    bool InViewport(int x, int y) const;
    void Apply();
    void Stop(const std::string& message, bool clear = true);
};
#endif
