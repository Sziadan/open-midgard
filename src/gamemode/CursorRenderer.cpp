#include "CursorRenderer.h"

#include "DebugLog.h"
#include "main/WinMain.h"
#include "render/DC.h"
#include "res/ActRes.h"
#include "res/Sprite.h"

#include <string>

namespace {

struct CursorResCache
{
    bool resolved;
    std::string sprName;
    std::string actName;
};

CursorResCache ResolveCursorResources()
{
    CursorResCache cache{};
    cache.resolved = false;

    static const struct { const char* spr; const char* act; } kCandidates[] = {
        { "data\\sprite\\cursors.spr", "data\\sprite\\cursors.act" },
        { "data\\sprite\\interface\\cursors.spr", "data\\sprite\\interface\\cursors.act" },
        { nullptr, nullptr }
    };

    for (int i = 0; kCandidates[i].spr != nullptr; ++i) {
        DbgLog("[Cursor] Trying candidate %d: spr=%s\n", i, kCandidates[i].spr);
        CSprRes* spr = g_resMgr.GetAs<CSprRes>(kCandidates[i].spr);
        DbgLog("[Cursor]   spr result=%p\n", (void*)spr);
        DbgLog("[Cursor]   Trying act=%s\n", kCandidates[i].act);
        CActRes* act = g_resMgr.GetAs<CActRes>(kCandidates[i].act);
        DbgLog("[Cursor]   act result=%p\n", (void*)act);
        if (spr && act) {
            cache.resolved = true;
            cache.sprName = kCandidates[i].spr;
            cache.actName = kCandidates[i].act;
            DbgLog("[Cursor] RESOLVED: spr=%s act=%s\n", kCandidates[i].spr, kCandidates[i].act);
            return cache;
        }
        DbgLog("[Cursor] Not found: spr=%s act=%s\n", kCandidates[i].spr, kCandidates[i].act);
    }

    DbgLog("[Cursor] FAILED to resolve cursor assets (using fallback arrow).\n");
    return cache;
}

void DrawFallbackCursor(HDC hdc)
{
    if (!hdc || !g_hMainWnd) {
        return;
    }

    POINT pt{};
    if (!GetCursorPos(&pt) || !ScreenToClient(g_hMainWnd, &pt)) {
        return;
    }

    HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
    if (cursor) {
        DrawIconEx(hdc, pt.x, pt.y, cursor, 0, 0, 0, nullptr, DI_NORMAL);
    }
}

} // namespace

bool DrawModeCursorToHdc(HDC hdc, int cursorActNum, u32 mouseAnimStartTick)
{
    if (!g_hMainWnd || !hdc) {
        return false;
    }

    static bool s_cacheInit = false;
    static CursorResCache s_cache{};
    if (!s_cacheInit) {
        DbgLog("[Cursor] Resolving cursor resources...\n");
        s_cache = ResolveCursorResources();
        DbgLog("[Cursor] resolved=%d spr='%s' act='%s'\n",
            (int)s_cache.resolved, s_cache.sprName.c_str(), s_cache.actName.c_str());
        s_cacheInit = true;
    }

    CActRes* actRes = s_cache.resolved ? g_resMgr.GetAs<CActRes>(s_cache.actName.c_str()) : nullptr;
    CSprRes* sprRes = s_cache.resolved ? g_resMgr.GetAs<CSprRes>(s_cache.sprName.c_str()) : nullptr;

    int action = cursorActNum;
    if (!actRes || !sprRes || action < 0 || action >= static_cast<int>(actRes->actions.size())) {
        action = 0;
    }

    bool drewCustomCursor = false;
    if (actRes && sprRes) {
        const int motionCount = actRes->GetMotionCount(action);
        if (motionCount > 0) {
            const unsigned int elapsed = GetTickCount() - mouseAnimStartTick;
            const float stateTicks = static_cast<float>(elapsed) * 0.041666668f;
            const float motionDelay = (std::max)(0.0001f, actRes->GetDelay(action));
            const int motionIndex = static_cast<int>(stateTicks / motionDelay) % motionCount;
            const CMotion* motion = actRes->GetMotion(action, motionIndex);
            if (motion) {
                POINT pt{};
                if (GetCursorPos(&pt) && ScreenToClient(g_hMainWnd, &pt)) {
                    drewCustomCursor = DrawActMotionToHdc(hdc, pt.x, pt.y, sprRes, motion, sprRes->m_pal);
                }
            }
        }
    }

    if (!drewCustomCursor) {
        DrawFallbackCursor(hdc);
    }

    return drewCustomCursor;
}

void DrawModeCursor(int cursorActNum, u32 mouseAnimStartTick)
{
    if (!g_hMainWnd) {
        return;
    }

    HDC hdc = GetDC(g_hMainWnd);
    if (!hdc) {
        return;
    }

    DrawModeCursorToHdc(hdc, cursorActNum, mouseAnimStartTick);

    ReleaseDC(g_hMainWnd, hdc);
}