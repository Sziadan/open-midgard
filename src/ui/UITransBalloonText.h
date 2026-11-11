#pragma once
#include "UIWindow.h"

class CMode;
class UITransBalloonText : public UIWindow {
public:
    explicit UITransBalloonText(CMode* owner, int fontPixels = 12, bool transparentBackground = false);
    ~UITransBalloonText() override;
    void DetachOwner(){m_owner=nullptr;}
    void SetText(const char* text,int maximumBytes);
    const std::vector<std::string>& GetLines() const {return m_lines;}
    const std::vector<std::string>& GetDisplayLines() const {return m_displayLines;}
    int GetLineHeight() const {return m_lineHeight;}
    u32 GetFontColor() const {return m_fontColor;}
    void SetFontColor(u32 rgb){m_fontColor=rgb;Invalidate();}
    bool CanGetFocus() override {return false;}
    bool ShouldDoHitTest() override {return false;}
    void OnDraw() override;
    static std::string ReplaceUnderbars(const char* text);
private:
    CMode* m_owner;
    std::vector<std::string> m_lines,m_displayLines;
    int m_fontPixels;
    bool m_transparentBackground;
    int m_lineHeight=12;
    u32 m_fontColor=0xffffff;
};
