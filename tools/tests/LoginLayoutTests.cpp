#include "ui/LoginLayout.h"
#include <cassert>
#include <string>
#include <vector>

struct RECT { int left{},top{},right{},bottom{}; };
void* g_hMainWnd = nullptr;
bool GetClientRect(void*,RECT*) { return false; }
struct Edit {
    bool focused = false;
    const char* GetText() const { return "test-account"; }
};
struct Check { void SetCheck(int value) { checked = value; } int checked = 0; };
void SetLoginFieldFocus(Edit* login,Edit* password,bool focusPassword) {
    login->focused = !focusPassword; password->focused = focusPassword;
}
bool storedRemember = false;
std::string storedId;
void StoreRememberedUserId(const char* id,bool remember) { storedId=id;storedRemember=remember; }
struct CLoginMode { enum { LoginMsg_SaveAccount = 1 }; };
struct ModeMgr {
    int lastMessage = 0, lastValue = 0;
    void SendMsg(int message,int value,int,int) { lastMessage=message;lastValue=value; }
} g_modeMgr;
struct UILoginWnd {
    struct QtButtonDisplay { int id{},x{},y{},width{},height{};const char* label{}; };
    int m_x{},m_y{},m_w{},m_h{},m_show=1,clicked=-1;
    bool m_saveAccountChecked=false;
    Edit login,password;
    Edit* m_login=&login;Edit* m_password=&password;
    Check check;Check* m_saveAccountCheck=&check;
    void OnCreate(int,int) {}
    void SendMsg(void*,int message,int id,int,int) { assert(message==6);clicked=id; }
    loginLayout::Layout GetQtLayout() const;
    int GetQtButtonCount() const;
    bool GetQtButtonDisplayForQt(int,QtButtonDisplay*) const;
    bool HandleQtMouseDown(int,int);
};
#include "LoginLayoutBodies.inc"

int main() {
    for(int width:{320,512,640,1024,1280,1920})for(int height:{240,384,480,720,1080}) {
        const auto layout=loginLayout::Place(width,height);
        assert(layout.x>=0&&layout.y>=0&&layout.x+layout.width<=width&&layout.y+layout.height<=height);
        std::vector<loginLayout::Rect> controls{layout.account,layout.password,layout.remember};
        for(const auto& b:layout.buttons)controls.push_back(b.rect);
        for(size_t i=0;i<controls.size();++i) {
            const auto& r=controls[i];
            assert(r.x>=0&&r.y>=17&&r.width>0&&r.height>0&&r.x+r.width<=layout.width&&r.y+r.height<=layout.height);
            for(size_t j=0;j<i;++j) {
                const auto& q=controls[j];
                assert(!(r.x<q.x+q.width&&r.x+r.width>q.x&&r.y<q.y+q.height&&r.y+r.height>q.y));
            }
        }
        UILoginWnd window;window.m_x=layout.x;window.m_y=layout.y;window.m_w=layout.width;window.m_h=layout.height;
        auto click=[&](const loginLayout::Rect& r,int dx=0){return window.HandleQtMouseDown(layout.x+r.x+(dx?dx:r.width/2),layout.y+r.y+r.height/2);};
        assert(click(layout.account)&&window.login.focused&&!window.password.focused);
        assert(click(layout.password)&&!window.login.focused&&window.password.focused);
        // The label at the far end of the remember row is clickable too.
        assert(click(layout.remember,layout.remember.width-1)&&window.check.checked==1&&storedRemember&&storedId=="test-account");
        assert(click(layout.remember)&&window.check.checked==0&&!storedRemember);
        assert(g_modeMgr.lastMessage==CLoginMode::LoginMsg_SaveAccount&&g_modeMgr.lastValue==0);
        for(int i=0;i<window.GetQtButtonCount();++i) {
            const auto& b=layout.buttons[i];assert(click(b.rect)&&window.clicked==b.id);
            UILoginWnd::QtButtonDisplay display;
            assert(window.GetQtButtonDisplayForQt(i,&display));
            assert(display.id==b.id&&display.x==layout.x+b.rect.x&&display.y==layout.y+b.rect.y);
            assert(display.width==b.rect.width&&display.height==b.rect.height);
        }
        assert(!window.GetQtButtonDisplayForQt(-1,nullptr));
        assert(!window.HandleQtMouseDown(layout.x-1,layout.y));
        window.m_show=0;assert(!click(layout.account));
    }
}
