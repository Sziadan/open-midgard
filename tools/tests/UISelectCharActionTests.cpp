#include <cassert>
#include <cstdint>

using msgparam_t = std::intptr_t;
using msgresult_t = std::intptr_t;

struct RECT { int left{}, top{}, right{}, bottom{}; };
void* g_hMainWnd = reinterpret_cast<void*>(1);
bool GetClientRect(void*, RECT* rect) { *rect = {}; return true; }

struct CHARACTER_INFO { unsigned char CharNum = 0; };
struct UIWindow {};
struct CLoginMode {
    enum {
        LoginMsg_ReturnToLogin = 30,
        LoginMsg_SelectCharacter = 10034,
        LoginMsg_RequestMakeCharacter = 10035,
        LoginMsg_RequestDeleteCharacter = 10036,
    };
};
struct ModeMgr {
    int message = 0;
    msgparam_t value = 0;
    msgresult_t SendMsg(int id, msgparam_t wparam, msgparam_t, msgparam_t) {
        message = id;
        value = wparam;
        return 1;
    }
} g_modeMgr;
struct UIWindowMgr { void SetLoginStatus(const char*) {} } g_windowMgr;
void PlayUiButtonSound() {}

constexpr int kSelectCharButtonY = 318;
constexpr int kSelectCharButtonHeight = 24;
constexpr int kSelectCharCancelX = 484;
constexpr int kSelectCharCancelW = 90;
constexpr int kSelectCharPrimaryX = 404;
constexpr int kSelectCharPrimaryW = 80;
constexpr int kSelectCharDeleteX = 5;
constexpr int kSelectCharDeleteW = 80;
constexpr int kSelectCharChargeX = 314;
constexpr int kSelectCharChargeW = 85;

struct UISelectCharWnd {
    int m_x = 100;
    int m_y = 20;
    int m_w = 576;
    int m_h = 342;
    int m_show = 1;
    int m_selectedSlot = 7;
    int m_defPushId = 0;
    int characterCount = 1;
    mutable CHARACTER_INFO characters[2]{{7}, {2}};

    void OnCreate(int, int) {}
    void ClampSelection() {}
    void UpdateActionButtons() {}
    void OnLBtnUp(int, int) {}
    int GetCharacterCount() const { return characterCount; }
    CHARACTER_INFO* GetCharacters() const { return characters; }

    bool HandleQtMouseDown(int, int);
    bool HandleQtMouseUp(int, int);
    msgresult_t SendMsg(UIWindow*, int, msgparam_t, msgparam_t, msgparam_t);
    int FindCharacterIndexForSlot(int) const;
    void ActivateSelectedSlot();
    void ActivateCreate();
    void ActivateDelete();
};

#include "UISelectCharActionBodies.inc"

int main()
{
    UISelectCharWnd window;
    const int centerY = window.m_y + kSelectCharButtonY + kSelectCharButtonHeight / 2;
    auto clickX = [&](int left, int width) { return window.m_x + left + width / 2; };

    assert(window.HandleQtMouseUp(clickX(kSelectCharPrimaryX, kSelectCharPrimaryW), centerY));
    assert(g_modeMgr.message == CLoginMode::LoginMsg_SelectCharacter && g_modeMgr.value == 0);

    g_modeMgr = {};
    window.m_selectedSlot = 2;
    assert(window.HandleQtMouseUp(clickX(kSelectCharPrimaryX, kSelectCharPrimaryW), centerY));
    assert(g_modeMgr.message == CLoginMode::LoginMsg_RequestMakeCharacter && g_modeMgr.value == 2);

    g_modeMgr = {};
    window.m_selectedSlot = 7;
    assert(window.HandleQtMouseUp(clickX(kSelectCharDeleteX, kSelectCharDeleteW), centerY));
    assert(g_modeMgr.message == CLoginMode::LoginMsg_RequestDeleteCharacter && g_modeMgr.value == 7);

    g_modeMgr = {};
    window.m_selectedSlot = 2;
    assert(window.HandleQtMouseUp(clickX(kSelectCharDeleteX, kSelectCharDeleteW), centerY));
    assert(g_modeMgr.message == 0);

    window.m_show = 0;
    assert(!window.HandleQtMouseUp(clickX(kSelectCharCancelX, kSelectCharCancelW), centerY));
    assert(g_modeMgr.message == 0);

    window.m_show = 1;
    assert(!window.HandleQtMouseUp(window.m_x - 1, centerY));
    assert(g_modeMgr.message == 0);

    assert(window.HandleQtMouseUp(clickX(kSelectCharCancelX, kSelectCharCancelW), centerY));
    assert(g_modeMgr.message == CLoginMode::LoginMsg_ReturnToLogin);
}
