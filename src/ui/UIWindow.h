#pragma once
#include "Types.h"
#include "UIShopCommon.h"
#include <list>
#include <vector>
#include <string>

void PlayUiButtonSound();
bool LoadUiWindowPlacement(const char* windowName, int* x, int* y);
void SaveUiWindowPlacement(const char* windowName, int x, int y);
bool BlitArgbBitsToMainWindow(const void* bits, int width, int height);

struct BOXINFO {
    int x;
    int y;
    int cx;
    int cy;
    int drawEdge;
    int color;
    int color2;
};

//===========================================================================
// UIWindow  –  Base class for all UI elements
//===========================================================================
class UIWindow {
public:
    UIWindow();
    virtual ~UIWindow();

    virtual void Create(int w, int h);
    virtual void AddChild(UIWindow* child);
    virtual void DrawChildren();
    virtual void DrawChildrenToHdc(HDC dc);
    virtual HDC AcquireDrawTarget() const;
    virtual void ReleaseDrawTarget(HDC dc) const;
    bool BlitArgbBitsToDrawTarget(const void* bits, int width, int height) const;

    static void SetSharedDrawDC(HDC dc);
    static HDC GetSharedDrawDC();

    virtual void Invalidate();
    virtual void InvalidateWallPaper();
    virtual void Resize(int w, int h);
    virtual bool IsFrameWnd();
    virtual bool IsUpdateNeed();
    virtual void Move(int x, int y);
    virtual bool CanGetFocus();
    virtual bool GetTransBoxInfo(BOXINFO* info);
    virtual bool IsTransmitMouseInput();
    virtual bool ShouldDoHitTest();
    virtual void DragAndDrop(int x, int y, const DRAG_INFO* const info);
    virtual void StoreInfo();
    virtual void SaveOriginalPos();
    virtual void MoveDelta(int dx, int dy);
    virtual u32  GetColor();
    virtual void SetShow(int show);
    
    // Event handlers
    virtual void OnCreate(int x, int y);
    virtual void OnDestroy();
    virtual void OnProcess();
    virtual void OnDraw();
    virtual void OnDraw2();
    virtual void OnRun();
    virtual void OnSize(int w, int h);
    virtual void OnBeginEdit();
    virtual void OnFinishEdit();
    virtual void OnLBtnDown(int x, int y);
    virtual void OnLBtnDblClk(int x, int y);
    virtual void OnMouseMove(int x, int y);
    virtual void OnMouseHover(int x, int y);
    virtual void OnMouseShape(int x, int y);
    virtual void OnLBtnUp(int x, int y);
    virtual void OnRBtnDown(int x, int y);
    virtual void OnRBtnUp(int x, int y);
    virtual void OnRBtnDblClk(int x, int y);
    virtual void OnWheel(int delta);
    virtual void RefreshSnap();
    virtual msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra);
    virtual void OnChar(char c);
    virtual bool CanReceiveKeyInput() const;

    // Recursive hit test: returns deepest visible window at (x,y), nullptr if none
    UIWindow* HitTestDeep(int x, int y);

    // Memory layout from HighPriest.exe.h:8962
    UIWindow* m_parent;
    std::list<UIWindow*> m_children;
    int m_x, m_y, m_w, m_h;
    int m_isDirty;
    int m_id;
    int m_state;
    int m_stateCnt;
    int m_show;
    u32 m_trans;
    u32 m_transTarget;
    u32 m_transTime;
};

//===========================================================================
// Specialized UI Components (Stubs)
//===========================================================================

class UIStaticText : public UIWindow {
public:
    int m_drawBackGround;
    int m_backR, m_backG, m_backB;
    int m_textR, m_textG, m_textB;
    int m_drawTwice;
    int m_drawBold;
    int m_fontHeight;
    int m_fontType;
    int m_isShorten;
    std::string m_text;
    std::string m_fullText;
};

class UIButton : public UIWindow {
public:
    UIButton();

    void SetToolTip(const char* text);
    void OnDraw() override;

    std::string m_text;
    std::string m_toolTip;
    int m_isDisabled;
};

class UIBitmapButton : public UIButton {
public:
    UIBitmapButton();
    ~UIBitmapButton() override;

    void SetBitmapName(const char* name, int stateIndex);
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnLBtnUp(int x, int y) override;

    int m_bitmapWidth;
    int m_bitmapHeight;
    std::string m_normalBitmapName;
    std::string m_mouseonBitmapName;
    std::string m_pressedBitmapName;
    shopui::BitmapPixels m_normalBitmap;
    shopui::BitmapPixels m_mouseonBitmap;
    shopui::BitmapPixels m_pressedBitmap;
};

class UIEditCtrl : public UIWindow {
public:
    UIEditCtrl();

    void SetFrameColor(int r, int g, int b);
    void SetText(const char* text);
    const char* GetText() const;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnChar(char c) override;
    bool CanReceiveKeyInput() const override;

    int m_selectionOrigin;
    int m_selectionCursor;
    int m_maskchar;
    int m_maxchar;
    int m_isSingColorFrame;
    int m_r, m_g, m_b;
    int m_textR, m_textG, m_textB;
    int m_xOffset, m_yOffset;
    int m_type;
    bool m_hasFocus;
    std::string m_text;
};

class UICheckBox : public UIWindow {
public:
    UICheckBox();
    ~UICheckBox() override;

    void SetBitmap(const char* onBitmap, const char* offBitmap);
    void SetCheck(int checked);
    void OnDraw() override;
    void OnLBtnUp(int x, int y) override;

    int m_isChecked;
    std::string m_onBitmapName;
    std::string m_offBitmapName;
    shopui::BitmapPixels m_onBitmap;
    shopui::BitmapPixels m_offBitmap;
};
