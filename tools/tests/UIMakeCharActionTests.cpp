#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using msgparam_t = std::intptr_t;
using msgresult_t = std::intptr_t;
struct RECT { int left{}, top{}, right{}, bottom{}; };
void* g_hMainWnd = reinterpret_cast<void*>(1);
bool GetClientRect(void*, RECT* rect) { *rect = {}; return true; }
struct UIWindow {};
struct UIEditCtrl {
    bool m_hasFocus = false;
    void Invalidate() {}
    const char* GetText() const { return "RuneA"; }
};
struct CLoginMode {
    enum {
        LoginMsg_SetMakingCharName = 10015,
        LoginMsg_SetCharParam = 10017,
        LoginMsg_CreateCharacter = 10037,
        LoginMsg_ReturnToCharSelect = 10038,
    };
};
struct ModeMgr {
    struct Call { int id; msgparam_t value; std::vector<int> fields; std::string text; };
    std::vector<Call> calls;
    msgresult_t SendMsg(int id, msgparam_t value, msgparam_t, msgparam_t) {
        Call call{id, value, {}, {}};
        if (id == CLoginMode::LoginMsg_SetCharParam) {
            const int* const data = reinterpret_cast<const int*>(value);
            call.fields.assign(data, data + 8);
        } else if (id == CLoginMode::LoginMsg_SetMakingCharName) {
            call.text = reinterpret_cast<const char*>(value);
        }
        calls.push_back(std::move(call));
        return 1;
    }
} g_modeMgr;
struct UIWindowMgr { void* m_editWindow = nullptr; } g_windowMgr;
void PlayUiButtonSound() {}

constexpr int kMakeCharOkX = 483;
constexpr int kMakeCharNameLeft = 62;
constexpr int kMakeCharNameTop = 244;
constexpr int kMakeCharNameWidth = 100;
constexpr int kMakeCharNameHeight = 18;
constexpr int kMakeCharOkY = 318;
constexpr int kMakeCharOkW = 44;
constexpr int kMakeCharOkH = 22;
constexpr int kMakeCharCancelX = 530;
constexpr int kMakeCharCancelY = 318;
constexpr int kMakeCharCancelW = 44;
constexpr int kMakeCharCancelH = 22;
constexpr int kMakeCharStatIds[6] = {139, 142, 140, 144, 141, 143};
constexpr int kMakeCharStatX[6] = {270, 270, 190, 349, 349, 190};
constexpr int kMakeCharStatY[6] = {50, 244, 104, 190, 104, 190};
constexpr int kMakeCharHairIds[3] = {161, 160, 213};
constexpr int kMakeCharHairX[3] = {48, 130, 89};
constexpr int kMakeCharHairY[3] = {135, 135, 101};
constexpr int kMakeCharSmallButtonW = 16;
constexpr int kMakeCharSmallButtonH = 14;

struct UIMakeCharWnd {
    struct QtButtonDisplay { int id = 0; int x = 0; int y = 0; int width = 0; int height = 0; bool pressed = false; const char* label = nullptr; };
    int m_x = 100;
    int m_y = 20;
    int m_w = 576;
    int m_h = 342;
    int m_show = 1;
    int m_pressedQtButtonIndex = -1;
    int m_stats[6]{1, 2, 3, 4, 5, 6};
    int m_hairColor = 18;
    int m_hairIdx = 42;
    UIEditCtrl edit;
    UIEditCtrl* m_nameEditCtrl = &edit;

    void OnCreate(int, int) {}
    void Invalidate() {}
    void RebuildPreview() {}
    int GetQtButtonCount() const { return 11; }
    bool GetQtButtonBitmap(int, const unsigned int**, int*, int*) { return false; }
    bool HandleQtMouseDown(int, int);
    bool HandleQtMouseUp(int, int);
    bool GetQtButtonDisplayForQt(int, QtButtonDisplay*) const;
    bool HitQtButton(int, int, int*) const;
    void SetQtPressedButtonIndex(int);
    msgresult_t SendMsg(UIWindow*, int, msgparam_t, msgparam_t, msgparam_t);
};

#include "UIMakeCharActionBodies.inc"

int main()
{
    UIMakeCharWnd window;
    const int okX = window.m_x + kMakeCharOkX + kMakeCharOkW / 2;
    const int okY = window.m_y + kMakeCharOkY + kMakeCharOkH / 2;
    assert(window.HandleQtMouseDown(okX, okY));
    assert(window.HandleQtMouseUp(okX, okY));
    assert(g_modeMgr.calls.size() == 3);
    assert(g_modeMgr.calls[0].id == CLoginMode::LoginMsg_SetCharParam);
    assert(g_modeMgr.calls[0].fields == std::vector<int>({1, 2, 3, 4, 5, 6, 18, 42}));
    assert(g_modeMgr.calls[1].id == CLoginMode::LoginMsg_SetMakingCharName);
    assert(g_modeMgr.calls[1].text == "RuneA");
    assert(g_modeMgr.calls[2].id == CLoginMode::LoginMsg_CreateCharacter);

    g_modeMgr = {};
    const int cancelX = window.m_x + kMakeCharCancelX + kMakeCharCancelW / 2;
    const int cancelY = window.m_y + kMakeCharCancelY + kMakeCharCancelH / 2;
    assert(window.HandleQtMouseDown(cancelX, cancelY));
    assert(window.HandleQtMouseUp(cancelX, cancelY));
    assert(g_modeMgr.calls.size() == 1);
    assert(g_modeMgr.calls[0].id == CLoginMode::LoginMsg_ReturnToCharSelect);

    g_modeMgr = {};
    assert(window.HandleQtMouseDown(okX, okY));
    assert(window.HandleQtMouseUp(window.m_x + 300, window.m_y + 300));
    assert(g_modeMgr.calls.empty());
}
