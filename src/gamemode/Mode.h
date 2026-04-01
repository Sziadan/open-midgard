#pragma once

#include "Types.h"
#include <vector>
#include <string>

// Forward declarations for UI/World classes
class UITransBalloonText;
class UINameBalloonText;
class UIWaitWnd;
class UIPlayerGage;
class CWorld;
class CView;
class CMousePointer;
class CGameMode;
class CLoginMode;

enum class CursorAction : int {
    Arrow = 0,
    Talk = 1,
    UiHand = 2,
    Warp = 3,
    CameraPan = 4,
    Attack = 5,
    AttackAlt = 6,
    Portal = 7,
    Forbidden = 8,
    Item = 9,
    SkillTarget = 10,
    SkillTargetAlt = 11,
    PortalAlt = 12,
};

//===========================================================================
// CMode  –  Abstract base for all major game states
//===========================================================================
class CMode
{
public:
    CMode();
    virtual ~CMode();

    virtual void OnInit(const char* worldName) {}
    virtual void OnExit() {}
    virtual int  OnRun() { return 0; }
    virtual msgresult_t SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) { return 0; }
    virtual void OnUpdate() {}
    void SetCursorAction(CursorAction cursorActNum);
    void SetCursorAction(int cursorActNum);
    u32 GetCursorAnimStartTick() const { return m_mouseAnimStartTick; }
    int GetCursorAction() const { return m_cursorActNum; }

protected:
    virtual void OnChangeState(int newState) {}

    // Memory layout from HighPriest.exe.h:30948
    int m_subMode;
    int m_subModeCnt;
    int m_nextSubMode;
    int m_fadeInCount;
    int m_loopCond;
    int m_isConnected;
    UITransBalloonText* m_helpBalloon;
    u32 m_helpBalloonTick;
    u32 m_mouseAnimStartTick;
    int m_isMouseLockOn;
    int m_screenShotNow;
    vector2d m_mouseSnapDiff;
    int m_cursorActNum;
    int m_cursorMotNum;
};

//===========================================================================
// CModeMgr  –  Owns the Login and Game mode instances
//===========================================================================
class CModeMgr
{
public:
    CModeMgr();
    ~CModeMgr();

    void Run(int startMode, const char* worldName);
    void Switch(int newMode, const char* worldName);
    void PresentLoadingScreen(const char* message, float progress);
    msgresult_t SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra = 0);
    CGameMode* GetCurrentGameMode() const;
    CLoginMode* GetCurrentLoginMode() const;
    void Quit();

private:
    // Memory layout from HighPriest.exe.h:30965
    int m_loopCond;
    CMode* m_curMode;
    char m_curModeName[40];
    char m_nextModeName[40];
    int m_curModeType;
    int m_nextModeType;
};

extern CModeMgr g_modeMgr;
