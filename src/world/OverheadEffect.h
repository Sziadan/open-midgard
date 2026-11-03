#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <vector>

constexpr float kOverheadIconScale = 2.25f;

inline float OverheadAnchorBottom(std::vector<float> frameBottoms, bool normalizeTransientPeaks)
{
    if (frameBottoms.empty()) return 0.0f;
    std::sort(frameBottoms.begin(),frameBottoms.end());
    const float peak=frameBottoms.back();
    if (!normalizeTransientPeaks) return peak;
    const size_t middle=frameBottoms.size()/2;
    const float typical=frameBottoms.size()%2 ? frameBottoms[middle]
        : (frameBottoms[middle-1]+frameBottoms[middle])*0.5f;
    // Keep small normal bounces (including gg), but do not move the entire
    // animation upward to accommodate an explosion or flying secondary clip.
    const float allowance=(std::max)(4.0f,std::fabs(typical)*0.5f);
    return peak-typical>allowance ? typical : peak;
}

inline float OverheadAnimationPosition(unsigned int elapsedMs, float actionDelay)
{
    // ACT delays are multiples of the client's 24 ms animation tick.
    // Ref/GameActor.cpp CRenderObject::SetAction clamps valid delays to 1.
    const float delay = std::isfinite(actionDelay) ? (actionDelay < 1.0f ? 1.0f : actionDelay) : 4.0f;
    return static_cast<float>(elapsedMs) / (24.0f * delay);
}

struct OverheadPoint { float x, y; };

inline std::array<OverheadPoint, 4> OverheadClipQuad(float x, float y, float width, float height,
    float zoomX, float zoomY, int angle)
{
    // Ref/GameActor.cpp RenderBody scales ACT offsets as well as image size.
    // DrawSpr rotates the resulting rectangle about its center.
    const float left=x*zoomX, top=y*zoomY;
    const float right=(x+width)*zoomX, bottom=(y+height)*zoomY;
    const float cx=(left+right)*0.5f, cy=(top+bottom)*0.5f;
    const float radians=static_cast<float>(angle)*0.017453292519943295f;
    const float cs=std::cos(radians), sn=std::sin(radians);
    std::array<OverheadPoint,4> points={{{left,top},{right,top},{left,bottom},{right,bottom}}};
    for (auto& point : points) {
        const float dx=point.x-cx, dy=point.y-cy;
        point={cx+dx*cs-dy*sn,cy+dx*sn+dy*cs};
    }
    return points;
}

constexpr bool IsOverheadMsgEffect(int type)
{
    return type == 1 || type == 7 || type == 8 || type == 9 || type == 10 || type == 11 || type == 17;
}

constexpr int OverheadIconTop(int headTop, int iconHeight, int gap = 6)
{
    return headTop - gap - iconHeight;
}
