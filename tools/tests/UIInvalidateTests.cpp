#include <cassert>
#include <list>

class UIWindow {
public:
    virtual ~UIWindow() = default;
    virtual bool IsUpdateNeed() const { return m_isDirty != 0; }
    virtual void InvalidateWallPaper() { ++m_invalidateCount; }
    void InvalidateChildren();

    int m_isDirty = 0;
    int m_invalidateCount = 0;
    std::list<UIWindow*> m_children;
};

class UIWindowMgr {
public:
    void InvalidateUpdateNeededUI();
    bool m_isInvalidatedByForce = false;
    std::list<UIWindow*> m_children;
};

UIWindowMgr g_windowMgr;

class CSession {
public:
    void InvalidateUI();
};

#include "UIInvalidateBodies.inc"

int main()
{
    CSession session;
    UIWindow root;
    UIWindow child;
    UIWindow grandchild;
    UIWindow cleanRoot;
    UIWindow cleanChild;
    root.m_isDirty = 1;
    root.m_children.push_back(&child);
    child.m_children.push_back(&grandchild);
    cleanRoot.m_children.push_back(&cleanChild);
    g_windowMgr.m_children = { &root, &cleanRoot };

    session.InvalidateUI();
    assert(g_windowMgr.m_isInvalidatedByForce);
    assert(root.m_isDirty && child.m_isDirty && grandchild.m_isDirty);
    assert(root.m_invalidateCount == 1 && child.m_invalidateCount == 1 && grandchild.m_invalidateCount == 1);
    assert(cleanRoot.m_isDirty == 0 && cleanChild.m_isDirty == 0);

    root.m_isDirty = child.m_isDirty = grandchild.m_isDirty = 0;
    session.InvalidateUI();
    assert(root.m_isDirty == 0 && child.m_isDirty == 0 && grandchild.m_isDirty == 0);

    g_windowMgr.m_isInvalidatedByForce = false; // RenderWallPaper resets this before the next invalidation cycle.
    session.InvalidateUI();
    assert(root.m_isDirty && child.m_isDirty && grandchild.m_isDirty);
    return 0;
}
