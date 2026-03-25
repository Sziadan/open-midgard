#pragma once
#include "UIFrameWnd.h"

//===========================================================================
// UIWaitWnd  –  A modal-like "waiting" window for network operations
//===========================================================================
class UIWaitWnd : public UIFrameWnd {
public:
    UIWaitWnd();
    virtual ~UIWaitWnd();

    virtual int  SendMsg(UIWindow* sender, int msg, int wparam, int lparam, int extra) override;
    virtual void OnCreate(int x, int y) override;
    virtual void OnDraw() override;

    void SetMsg(const char* waitMsg, int fontHeight, int fontType);

public:
    void* m_textViewer;
    UIBitmapButton* m_pctrCancel;
    std::string m_waitMsg;
    int m_fontHeight;
    int m_fontType;
};
