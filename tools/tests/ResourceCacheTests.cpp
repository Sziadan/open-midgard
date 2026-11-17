#include "core/File.h"
#include "res/Res.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

CFileMgr g_fileMgr;
CFileMgr::CFileMgr() = default;
CFileMgr::~CFileMgr() = default;
unsigned char* CFileMgr::GetData(const char*, int*) { return nullptr; }
bool CFileMgr::IsDataExist(const char*) { return false; }

struct TestResource : CRes {
    static int loads;
    static int destroyed;
    ~TestResource() override { ++destroyed; }
    CRes* Clone() override { return new TestResource; }
    bool Load(const char*) override { ++loads; return true; }
};

int TestResource::loads = 0;
int TestResource::destroyed = 0;

int main()
{
    CResMgr manager;
    manager.RegisterType("spr", "data\\", new TestResource);
    CRes* first = manager.Get("data\\monster.spr");
    assert(first && TestResource::loads == 1);
    assert(manager.Get("data\\MONSTER.SPR") == first);
    assert(TestResource::loads == 1);

    CRes* refreshed = manager.GetAlways("data\\monster.spr");
    assert(refreshed && refreshed != first && TestResource::loads == 2);
    manager.Unload(refreshed);
    assert(manager.Get("data\\monster.spr") == first);
    refreshed = manager.GetAlways("data\\monster.spr");
    assert(refreshed && refreshed != first && TestResource::loads == 3);
    manager.Unload(first);
    assert(manager.Get("data\\monster.spr") == refreshed);
    refreshed->Lock();
    CRes* other = manager.Get("data\\other.spr");
    assert(other && TestResource::loads == 4);
    manager.UnloadUnlockedRes();
    assert(manager.Get("data\\monster.spr") == refreshed);
    assert(TestResource::destroyed == 3);
    refreshed->Unlock();
    manager.UnloadResByExt("spr");
    assert(TestResource::destroyed == 4);
    assert(manager.Get("data\\monster.spr") && TestResource::loads == 5);
    manager.UnloadAll();
    assert(TestResource::destroyed == 5);
}
