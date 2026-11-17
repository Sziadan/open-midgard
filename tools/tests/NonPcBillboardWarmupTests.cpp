#include "world/BillboardWarmup.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <map>
#include <tuple>
#include <vector>

struct SharedNonPcBillboardKey {
    int job, action, motion;
    bool operator<(const SharedNonPcBillboardKey& other) const {
        return std::tie(job, action, motion) < std::tie(other.job, other.action, other.motion);
    }
};
struct SharedNonPcBillboardStripKey {
    int job, action;
    bool operator<(const SharedNonPcBillboardStripKey& other) const {
        return std::tie(job, action) < std::tie(other.job, other.action);
    }
};
struct CSprRes {};
struct CActRes {
    int count = 4;
    std::vector<int> actions{0, 1};
    int GetMotionCount(int) const { return count; }
};
struct CPc {
    CActRes act;
    CSprRes spr;
    int m_isPc = 0;
    void* m_billboardTexture = this;
    int m_billboardTextureOwned = 0;
    int m_cachedBillboardJob = 42;
    int m_cachedBillboardBodyAction = 0;
    int m_cachedBillboardHeadMotion = 0;
    bool WarmupNonPcBillboardCache();
};
enum class RenderBackendType { Vulkan, Other };
struct RenderDevice {
    RenderBackendType backend = RenderBackendType::Vulkan;
    RenderBackendType GetBackendType() const { return backend; }
};
RenderDevice device;
RenderDevice& GetRenderDevice() { return device; }

constexpr int kSharedNonPcWarmupMotionLimit = 32;
std::map<SharedNonPcBillboardKey, int> cache;
std::map<SharedNonPcBillboardStripKey, BillboardWarmupProgress> progress;
std::vector<int> populated;
bool failPopulate = false;
int errors = 0;

bool ResolveCachedNonPcResourcesForActor(CPc& actor, CActRes** act, CSprRes** spr) {
    *act = &actor.act;
    *spr = &actor.spr;
    return true;
}
auto& GetSharedNonPcBillboardCache() { return cache; }
auto& GetNonPcBillboardWarmupProgress() { return progress; }
bool PopulateSharedNonPcBillboardFrame(CPc&, int job, int action, int motion) {
    populated.push_back(motion);
    if (failPopulate) return false;
    cache[{job, action, motion}] = 1;
    return true;
}
void DbgLog(const char*, ...) { ++errors; }
void DbgError(const char*, ...) { ++errors; }

#include "NonPcBillboardWarmup.inc"

int main()
{
    CPc actor;
    cache[{1002, 8, 2}] = 1;
    assert(PrimeNonPcBillboardStrip(actor, 1002, 8, 2));
    assert((populated == std::vector<int>{3}));
    assert(PrimeNonPcBillboardStrip(actor, 1002, 8, 2));
    assert((populated == std::vector<int>{3, 0}));
    cache[{1002, 8, 1}] = 1;
    assert(!PrimeNonPcBillboardStrip(actor, 1002, 8, 2));
    assert(populated.size() == 2);

    failPopulate = true;
    assert(PrimeNonPcBillboardStrip(actor, 1002, 9, 0));
    assert(errors == 1 && populated.back() == 1);
    failPopulate = false;
    assert(PrimeNonPcBillboardStrip(actor, 1002, 9, 0));
    assert(populated.back() == 2);

    actor.act.count = 1;
    assert(!PrimeNonPcBillboardStrip(actor, 1002, 10, 0));
    actor.act.count = 33;
    assert(!PrimeNonPcBillboardStrip(actor, 1002, 10, 0));
    actor.act.count = 3;
    assert(!PrimeNonPcBillboardStrip(actor, 1002, 10, -2));
    assert(PrimeNonPcBillboardStrip(actor, 1002, 9, 0));
    assert(populated.back() == 1);
    assert(PrimeNonPcBillboardStrip(actor, 1002, 10, -1));
    assert(populated.back() == 0);
    assert(PrimeNonPcBillboardStrip(actor, 1002, 10, -1));
    assert(populated.back() == 1);
    assert(PrimeNonPcBillboardStrip(actor, 1002, 10, -1));
    assert(populated.back() == 2);
    assert(!PrimeNonPcBillboardStrip(actor, 1002, 10, -1));

    cache[{42, 0, 0}] = 1;
    assert(actor.WarmupNonPcBillboardCache() && populated.back() == 1);
    assert(actor.WarmupNonPcBillboardCache() && populated.back() == 2);
    assert(actor.WarmupNonPcBillboardCache() && populated.back() == 0);
    assert(actor.WarmupNonPcBillboardCache() && populated.back() == 1);
    assert(actor.WarmupNonPcBillboardCache() && populated.back() == 2);
    assert(!actor.WarmupNonPcBillboardCache());
    actor.m_isPc = 1;
    assert(!actor.WarmupNonPcBillboardCache());
}
