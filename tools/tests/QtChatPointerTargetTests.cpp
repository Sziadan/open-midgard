#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <list>

struct UIWindow {
    bool m_show = true;
    int x = 0, y = 0, w = 0, h = 0;
    UIWindow* child = nullptr;

    UIWindow* HitTestDeep(int px, int py) {
        if (!m_show) return nullptr;
        if (child) {
            if (auto* hit = child->HitTestDeep(px, py)) return hit;
        }
        return px >= x && px < x + w && py >= y && py < y + h ? this : nullptr;
    }
};

struct ChatWindow : UIWindow {
    bool IsQtInteractionPoint(int px, int py) const {
        return m_show && px >= x - 5 && px < x + w + 20 && py >= y && py < y + h;
    }
};

struct UIWindowMgr {
    ChatWindow* m_chatWnd = nullptr;
    UIWindow* m_modalWindow = nullptr;
    std::list<UIWindow*> m_children;
    bool IsQtChatPointerTarget(int x, int y) const;
};

#include "QtChatPointerTarget.inc"

int main()
{
    ChatWindow chat;
    chat.x = 100; chat.y = 100; chat.w = 200; chat.h = 100;
    UIWindow options;
    options.x = 150; options.y = 100; options.w = 100; options.h = 100;
    UIWindow button;
    button.x = 170; button.y = 120; button.w = 20; button.h = 20;
    options.child = &button;
    UIWindowMgr manager;
    manager.m_chatWnd = &chat;
    manager.m_children = {&chat, &options};

    assert(!manager.IsQtChatPointerTarget(180, 130));
    assert(manager.IsQtChatPointerTarget(110, 110));
    assert(manager.IsQtChatPointerTarget(310, 110));
    assert(!manager.IsQtChatPointerTarget(90, 110));
    options.m_show = false;
    assert(manager.IsQtChatPointerTarget(180, 130));
    options.m_show = true;
    manager.m_children = {&options, &chat};
    assert(manager.IsQtChatPointerTarget(180, 130));
    options.x = 300;
    options.child = nullptr;
    manager.m_children = {&chat, &options};
    assert(!manager.IsQtChatPointerTarget(310, 110));
    manager.m_children = {&options, &chat};
    assert(manager.IsQtChatPointerTarget(310, 110));
    manager.m_modalWindow = &options;
    assert(!manager.IsQtChatPointerTarget(110, 110));
    manager.m_modalWindow = nullptr;
    chat.m_show = false;
    assert(!manager.IsQtChatPointerTarget(110, 110));
    manager.m_children = {&options};
    chat.m_show = true;
    assert(!manager.IsQtChatPointerTarget(110, 110));
}
