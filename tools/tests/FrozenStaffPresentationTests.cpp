#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "world/ActorSpriteDepth.h"

namespace staff {
struct CGameActor { int weaponType = 10; };
struct ResourceManager {
    std::set<std::string> files;
    bool IsExist(const char* path) { return files.count(path) != 0; }
} g_resMgr;
struct Session {
    std::string GetPlayerWeaponToken(int) { return {}; }
} g_session;
int ResolvePcWeaponOverlayType(const CGameActor& actor, int, bool* dual) {
    *dual = false;
    return actor.weaponType;
}
bool SupportsPcWeaponOverlaySecondLayer(int) { return false; }
#include "StaffOverlayBodies.inc"
#include "StaffOverlayAssets.inc"

void Test() {
    CGameActor wizard;
    for (const auto& asset : assets) {
        g_resMgr.files.insert(asset.overlay);
    }
    for (const auto& asset : assets) {
        for (int type : {10, 23}) {
            wizard.weaponType = type;
            std::string path;
            assert(BuildWeaponOverlayPath(wizard, asset.body, 1602, false, asset.extension, &path));
            assert(path == asset.overlay);
            assert(!BuildWeaponOverlayPath(wizard, asset.body, 1602, true, asset.extension, &path));
        }
    }
}
}

namespace ice {
using u32 = unsigned;
struct matrix {};
struct CPc {};
struct CWorld {
    struct BillboardScreenEntry {
        CPc* actor{};
        float depthKey{};
        float renderY[4]{};
        float renderZ[4]{};
    };
};
struct tlvertex3d { float x{}, y{}, z{}, oow{}; };
struct CMsgEffect {
    bool m_isVisible = true;
    int m_msgEffectType = 5;
    CPc* m_masterActor{};
    tlvertex3d m_pos{100, 200, .9f, .002f};
    int m_spriteActionIndex = 0, m_spriteMotionIndex = 2;
    std::string m_customSpritePath = "ice.spr", m_customActPath = "ice.act";
    u32 m_colorArgb = 0xFFFFFFFF;
    int m_alpha = 255;
    float m_zoom = 1;
};
struct QueuedMsgSpriteEffectDraw {
    int screenX{}, screenY{}, actionIndex{}, motionIndex{}, alpha{};
    std::string spritePath, actPath;
    u32 colorArgb{};
    float zoom{}, referencePixelRatio{}, projectedZ{}, projectedOow{}, alphaSortBase{};
    bool useReferencePixelRatio{}, useProjectedDepth{}, useOwnerDepth{};
    float ownerTopY{}, ownerBottomY{}, ownerTopZ{}, ownerBottomZ{};
};
std::vector<QueuedMsgSpriteEffectDraw> g_queuedMsgSpriteEffects;
struct { float m_hpc = 600; } g_renderer;
bool ProjectMsgEffectPoint(const matrix&, const tlvertex3d& pos, tlvertex3d* out) {
    *out = pos;
    return pos.oow > 0;
}
#include "FrozenShellBody.inc"

void Test() {
    CPc actor, other;
    CWorld::BillboardScreenEntry entry{};
    entry.actor = &actor;
    entry.depthKey = .002f;
    entry.renderY[0] = 100; entry.renderY[2] = 200;
    entry.renderZ[0] = .8f; entry.renderZ[2] = .9f;
    CMsgEffect effect;
    effect.m_masterActor = &actor;
    matrix view;
    RenderFrozenActorEffectPass(effect, &view, entry);
    assert(g_queuedMsgSpriteEffects.size() == 1);
    const auto draw = g_queuedMsgSpriteEffects.back();
    assert(draw.useOwnerDepth && draw.useProjectedDepth && draw.useReferencePixelRatio);
    assert(draw.spritePath == "ice.spr" && draw.motionIndex == 2);
    assert(draw.alphaSortBase > entry.depthKey && draw.alphaSortBase < .0021f);
    // Different camera depths/scales, both within and beyond the cropped body.
    for (float topZ : {.2f, .7f, .95f}) {
        for (float height : {20.f, 100.f, 300.f}) {
            for (float ratio : {-.25f, 0.f, .5f, 1.f, 1.25f}) {
                const float bottomZ = topZ + .01f;
                const float y = 100 + height * ratio;
                const float ownerZ = topZ + (bottomZ - topZ) * ((y - 100) / height)
                    - kActorBillboardDepthBias;
                const float shellZ = SampleActorShellDepth(y, 100, 100 + height, topZ, bottomZ);
                assert(shellZ < ownerZ);
                // A nearer scene surface must still occlude the shell.
                assert(shellZ > ownerZ - .001f);
            }
        }
    }
    assert(SampleActorShellDepth(100, 100, 100, .8f, .9f) < .8f);
    assert(SampleActorShellDepth(100, 100, 200, 0, 0) == 0);
    effect.m_isVisible = false;
    RenderFrozenActorEffectPass(effect, &view, entry);
    effect.m_isVisible = true;
    effect.m_masterActor = &other;
    RenderFrozenActorEffectPass(effect, &view, entry);
    effect.m_masterActor = &actor;
    RenderFrozenActorEffectPass(effect, nullptr, entry);
    effect.m_msgEffectType = 18;
    RenderFrozenActorEffectPass(effect, &view, entry);
    effect.m_msgEffectType = 5;
    effect.m_pos.oow = 0;
    RenderFrozenActorEffectPass(effect, &view, entry);
    assert(g_queuedMsgSpriteEffects.size() == 1);
}
}

int main() {
    staff::Test();
    ice::Test();
    std::puts("Staff resource resolution and frozen shell depth regressions passed");
}
