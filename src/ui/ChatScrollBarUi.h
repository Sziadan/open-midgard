#pragma once
#include "core/ChatScrollBar.h"
#include "platform/WindowsCompat.h"

namespace ro::chat {
struct ScrollRects { RECT thumb{},up{},down{}; };
inline ScrollRects ScrollLayout(RECT rect,const ScrollBar& bar)
{
    return {{rect.left,rect.top+bar.ThumbStart(),rect.right,rect.top+bar.ThumbStart()+bar.ThumbSize()},
        {rect.left,rect.top,rect.right,rect.top+bar.button},
        {rect.left,rect.bottom-bar.button,rect.right,rect.bottom}};
}
inline void DrawScrollBar(HDC dc,RECT rect,RECT thumb,int button)
{
    if(rect.right<=rect.left || rect.bottom<=rect.top)return;
    const int saved=SaveDC(dc);IntersectClipRect(dc,rect.left,rect.top,rect.right,rect.bottom);
    const auto fill=[&](RECT r,COLORREF color){auto b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);};
    fill(rect,RGB(64,64,64));
    fill({rect.left,rect.top,rect.right,rect.top+button},RGB(96,96,96));
    fill({rect.left,rect.bottom-button,rect.right,rect.bottom},RGB(96,96,96));
    fill(thumb,RGB(184,184,184));
    auto pen=CreatePen(PS_SOLID,1,RGB(240,240,240));const auto old=SelectObject(dc,pen);
    const int middle=(rect.left+rect.right)/2;
    MoveToEx(dc,middle-3,rect.top+button/2+1,nullptr);LineTo(dc,middle,rect.top+button/2-2);LineTo(dc,middle+3,rect.top+button/2+1);
    MoveToEx(dc,middle-3,rect.bottom-button/2-2,nullptr);LineTo(dc,middle,rect.bottom-button/2+1);LineTo(dc,middle+3,rect.bottom-button/2-2);
    SelectObject(dc,old);DeleteObject(pen);RestoreDC(dc,saved);
}
} // namespace ro::chat
