#include <cassert>
#include <cstdio>
#include <list>

struct CGameActor {};
struct CSkill;
struct CWorld;
static CWorld* deletingWorld;
static int destroyedObjects, destroyedSkills, destroyedItems;
struct CGameObject {
    virtual ~CGameObject() { ++destroyedObjects; }
};
struct CMsgEffect : CGameObject {
    CGameActor* m_masterActor = nullptr;
    bool m_removedFromOwner = false;
};
struct CItem { ~CItem() { ++destroyedItems; } };
struct CSkill : CGameObject { ~CSkill() override; };
struct CWorld {
    std::list<CGameObject*> m_gameObjectList;
    std::list<CSkill*> m_skillList;
    std::list<CItem*> m_itemList;
    void ClearFixedObjects(bool preserveAttachedActorEffects = false);
};
CSkill::~CSkill() {
    // The non-owning index must already be empty during destruction.
    assert(deletingWorld->m_skillList.empty());
    ++destroyedSkills;
}
#include "WorldCleanupBody.inc"

static void AddSkill(CWorld& world) {
    auto* skill = new CSkill;
    world.m_gameObjectList.push_back(skill);
    world.m_skillList.push_back(skill);
}

int main() {
    CWorld world;
    deletingWorld = &world;
    CGameActor owner;
    // Wizard leaves active ground skills, fixed effects and drops behind.
    AddSkill(world);
    AddSkill(world);
    world.m_gameObjectList.push_back(new CGameObject);
    world.m_itemList.push_back(new CItem);
    world.ClearFixedObjects();
    assert(world.m_skillList.empty() && world.m_gameObjectList.empty() && world.m_itemList.empty());
    assert(destroyedSkills == 2 && destroyedObjects == 3 && destroyedItems == 1);
    // Character exit and new-character entry can both clear the shared world.
    world.ClearFixedObjects();
    assert(destroyedObjects == 3 && destroyedItems == 1);
    // The next character may safely repopulate and query its own skill index.
    AddSkill(world);
    assert(world.m_skillList.size() == 1);
    assert(world.m_skillList.front() == world.m_gameObjectList.front());
    world.ClearFixedObjects();
    assert(destroyedSkills == 3 && world.m_skillList.empty());

    // Map changes preserve live owner effects, but never old ground skills.
    auto* attached = new CMsgEffect;
    attached->m_masterActor = &owner;
    auto* removed = new CMsgEffect;
    removed->m_masterActor = &owner;
    removed->m_removedFromOwner = true;
    auto* unowned = new CMsgEffect;
    world.m_gameObjectList = {attached, removed, unowned, nullptr};
    AddSkill(world);
    world.m_itemList.push_back(new CItem);
    world.ClearFixedObjects(true);
    assert(world.m_skillList.empty() && destroyedSkills == 4);
    assert(world.m_gameObjectList.size() == 1 && world.m_gameObjectList.front() == attached);
    assert(world.m_itemList.empty() && destroyedItems == 2);
    world.ClearFixedObjects(true);
    assert(world.m_gameObjectList.size() == 1);
    world.ClearFixedObjects();
    assert(world.m_gameObjectList.empty() && world.m_skillList.empty());
    assert(destroyedObjects == 8);
    std::puts("World cleanup: character switch, repeated teardown, map change and attached-effect preservation passed");
}
