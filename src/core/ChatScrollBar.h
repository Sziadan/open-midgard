#pragma once
#include <algorithm>
#include <climits>

namespace ro::chat {
enum class ScrollHit { None=-1, Up=0, PageUp=1, Thumb=2, PageDown=3, Down=4 };

struct ScrollBar {
    int axis=0, button=13, maximum=0, position=0, page=0, total=0;
    static int Bounded(long long value) { return static_cast<int>(std::clamp(value,static_cast<long long>(INT_MIN),static_cast<long long>(INT_MAX))); }
    int ThumbSize() const {
        const long long rail=static_cast<long long>(axis)-2LL*button;
        const auto raw=total>0 ? rail*page/total : rail;
        return Bounded((std::max)(8LL,raw));
    }
    int Travel() const { return Bounded(static_cast<long long>(axis)-2LL*button-ThumbSize()); }
    int ThumbStart() const {
        return Bounded(button+(maximum>0 ? static_cast<long long>(Travel())*position/maximum : 0));
    }
    ScrollHit Hit(int x,int y) const {
        if(maximum<=0 || x<0 || x>=13 || y<0 || y>=axis)return ScrollHit::None;
        if(y<button)return ScrollHit::Up;
        if(y<ThumbStart())return ScrollHit::PageUp;
        if(static_cast<long long>(y)<static_cast<long long>(ThumbStart())+ThumbSize())return ScrollHit::Thumb;
        return y>=axis-button ? ScrollHit::Down : ScrollHit::PageDown;
    }
    int Step(ScrollHit hit) const {
        switch(hit){
        case ScrollHit::Up:return -1;
        case ScrollHit::PageUp:return 1-page;
        case ScrollHit::PageDown:return page-1;
        case ScrollHit::Down:return 1;
        default:return 0;
        }
    }
};

// UIScrollBar uses pointer displacement and the ACTUAL movement returned by its
// parent. Keep the clamp residual so reversing after an overshoot does not jump.
struct ScrollDrag {
    int start=0;
    long long applied=0;
    void Begin(int pointer){start=pointer;applied=0;}
    int Request(int pointer,const ScrollBar& bar) const {
        if(bar.maximum<=0 || bar.Travel()==0)return 0;
        const auto requested=(static_cast<long long>(bar.maximum)+1)
            *(static_cast<long long>(pointer)-start)/bar.Travel()-applied;
        return ScrollBar::Bounded(requested);
    }
    void Applied(int before,int after){applied+=static_cast<long long>(after)-before;}
};
} // namespace ro::chat
