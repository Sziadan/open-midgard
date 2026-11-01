#include "ui/SkillTreeModel.h"
#include <cassert>
#include <iostream>
using namespace skilltree;

static Model Acolyte(int points = 5)
{
    Skill heal; heal.id=28; heal.cap=10; heal.name="Heal";
    Skill increase; increase.id=29; increase.cap=10; increase.name="Increase Agility"; increase.requires={{28,3}};
    Skill decrease; decrease.id=30; decrease.cap=10; decrease.name="Decrease Agility"; decrease.requires={{29,1}};
    Model model; assert(model.Load({heal,increase,decrease}));
    Snapshot state; state.points=points; state.baseLevel=30; state.jobLevel=20; model.Sync(state);
    return model;
}
int main()
{
    // Family/link skills are absent until the server reports a learned level.
    // They remain read-only even if catalog metadata exposes a larger cap.
    Skill basic;basic.id=1;basic.cap=9;
    Skill callBaby;callBaby.id=410;callBaby.cap=2;callBaby.grantedOnly=true;
    const std::vector<Skill> novice{basic,callBaby};
    assert(VisibleSkills(novice,{}).size()==1);
    assert(VisibleSkills(novice,{{410,0}}).size()==1);
    Model parent;assert(parent.Load(VisibleSkills(novice,{{410,1}})));
    Snapshot adopted;adopted.points=5;adopted.levels[410]=1;parent.Sync(adopted);
    assert(parent.skills.count(410)&&!parent.Stage(410)&&parent.draft.empty());
    assert(parent.Load(VisibleSkills(novice,{}))&&!parent.skills.count(410));
    auto model=Acolyte();
    // A second-job tree remains visible, but cannot reserve points until the
    // server exposes that group. Drafting first-job points alone is not enough.
    for (auto& [id,skill] : model.skills) skill.accessGroup=4;
    Skill magnificat;magnificat.id=74;magnificat.cap=5;magnificat.name="Magnificat";
    magnificat.accessGroup=8;magnificat.requires={{28,3}};
    model.skills.emplace(74,magnificat);
    auto gatedState=model.confirmed;gatedState.enforceServerGroups=true;gatedState.availableGroups={4};
    model.Sync(gatedState);
    assert(!model.Stage(74)&&model.draft.empty()&&!model.Unlocked(74));
    assert(model.Stage(30)&&model.Stage(30)&&model.Stage(30));
    assert(!model.Stage(74)&&model.draft.size()==3);
    // The server can unlock a group without making every skill in it available.
    gatedState.availableGroups.insert(8);model.Sync(gatedState);
    assert(model.Stage(74)&&model.draft.back()==74);
    model.Reset();gatedState.availableGroups.erase(8);model.Sync(gatedState);
    assert(!model.CanStage(74));
    model=Acolyte();
    const auto requirements=model.Requirements(30);
    assert(requirements.at(28)==3 && requirements.at(29)==1);
    for(int i=0;i<5;++i)assert(model.Stage(30));
    assert((model.draft==std::vector<int>{28,28,28,29,30}));
    assert(model.Remaining()==0 && model.Learned(28)==0 && model.Projected(28)==3);
    assert(!model.Stage(30));
    model.Reset();assert(model.draft.empty()&&model.Remaining()==5&&model.Projected(28)==0);

    model=Acolyte(1);assert(model.Stage(30));assert(model.draft.size()==1&&model.Projected(28)==1);assert(!model.Stage(30));
    model=Acolyte();model.skills.at(29).jobLevel=50;assert(!model.Stage(30)&&model.draft.empty());
    model=Acolyte();model.skills.at(28).quest=true;assert(!model.Stage(30)&&model.draft.empty());
    model=Acolyte();model.confirmed.levels[28]=7;model.Sync(model.confirmed);model.Select(28,2);
    assert(model.Selected(28)==7);model.Select(28,-4);assert(model.Selected(28)==3);
    assert(model.Stage(28)&&model.Projected(28)==8&&model.Selected(28)==3);
    model.Select(28,5);assert(model.Selected(28)==8);model.Reset();assert(model.Selected(28)==7);
    model.confirmed.levels[28]=10;model.Sync(model.confirmed);assert(!model.Stage(28));
    Skill cyclic;cyclic.id=1;cyclic.cap=10;cyclic.requires={{1,1}};
    Model invalid;assert(!invalid.Load({cyclic}));
    cyclic.requires={{2,1}};assert(!invalid.Load({cyclic}));

    // Sequential confirmation, point update before skill ack, no duplicate sends.
    model=Acolyte();for(int i=0;i<5;++i)assert(model.Stage(30));
    ApplyQueue apply;assert(apply.Begin(model,0));assert(!apply.Begin(model,0));
    const std::vector<int> sequence{28,28,28,29,30};
    std::uint64_t clock=1;
    for(int id:sequence) {
        assert(apply.Poll(model,{28,29,30},clock++)==id);
        assert(apply.Poll(model,{28,29,30},clock++)==0);
        auto state=model.confirmed;--state.points;model.Sync(state);
        assert(apply.Poll(model,{28,29,30},clock++)==0);
        ++state.levels[id];model.Sync(state);
        assert(apply.Poll(model,{28,29,30},clock++)==0);
    }
    assert(apply.Poll(model,{28,29,30},clock++)==0&&!apply.active);
    assert(model.Learned(28)==3&&model.Learned(29)==1&&model.Learned(30)==1&&model.confirmed.points==0);

    // Skill ack before point update: X2 is not double-counted; timeout cannot
    // undo the applied point or send an unconfirmed second request.
    model=Acolyte();assert(model.Stage(30));assert(model.Stage(30));
    apply={};assert(apply.Begin(model,0));assert(apply.Poll(model,{28},1)==28);
    auto state=model.confirmed;state.levels[28]=1;model.Sync(state);
    assert(apply.Poll(model,{28},2)==0&&model.Projected(28)==2&&apply.applied==1);
    assert(apply.Poll(model,{28},8002)==0&&!apply.active&&apply.balanceUncertain);
    assert(model.Learned(28)==1&&model.draft.empty());model.Reset();assert(model.Learned(28)==1);
    state.points=4;model.Sync(state);assert(apply.Poll(model,{28},8003)==0&&!apply.balanceUncertain);

    model=Acolyte();assert(model.Stage(30));apply={};assert(apply.Begin(model,0));
    assert(apply.Poll(model,{},8001)==0&&!apply.active&&model.draft.empty());
    model=Acolyte();assert(model.Stage(30));apply={};assert(apply.Begin(model,0));
    assert(apply.Poll(model,{28},1)==28);apply.Stop(model,"Send failed");
    assert(!apply.active&&model.draft.empty()&&model.Learned(28)==0);

    // Skill-first success waits for the balance, then resumes exactly once.
    model=Acolyte();assert(model.Stage(30));assert(model.Stage(30));apply={};
    assert(apply.Begin(model,0));assert(apply.Poll(model,{28},1)==28);
    state=model.confirmed;state.levels[28]=1;model.Sync(state);
    assert(apply.Poll(model,{28},2)==0&&model.Projected(28)==2);
    --state.points;model.Sync(state);assert(apply.Poll(model,{28},3)==0);
    assert(apply.Poll(model,{28},4)==28&&model.Projected(28)==2);
    state.levels[28]=3;model.Sync(state);
    assert(apply.Poll(model,{28},5)==0&&!apply.active&&model.draft.empty());

    // Drafts do not move nodes. Every dependency goes to a lower layer.
    model=Acolyte();auto positions=Layout(model);assert(model.Stage(30));
    auto pendingPositions=Layout(model);
    for(const auto& [id,position]:positions) {
        assert(position.x==pendingPositions.at(id).x&&position.y==pendingPositions.at(id).y);
        for(const auto& req:model.skills.at(id).requires)assert(positions.at(req.id).y<position.y);
    }
    // Wrap wide roots into vertical rows; terminal siblings and independent
    // skills each keep a column. No overlaps or horizontal overflow.
    Skill root;root.id=1;root.cap=10;
    std::vector<Skill> definitions{root};
    for(int id=2;id<=18;++id) {
        Skill skill;skill.id=id;skill.cap=10;
        if(id<12)skill.requires={{1,1}};
        definitions.push_back(skill);
    }
    assert(model.Load(definitions));
    for(int width:{264,400,764}) {
        positions=Layout(model,width);
        for(const auto& [id,p]:positions) {
            assert(p.x>=0&&p.x+Geometry::width+Geometry::margin<=width);
            for(const auto& req:model.skills.at(id).requires)assert(positions.at(req.id).y<p.y);
            for(const auto& [other,q]:positions)if(id!=other)
                assert(p.x+Geometry::width<=q.x||q.x+Geometry::width<=p.x
                    ||p.y+Geometry::height<=q.y||q.y+Geometry::height<=p.y);
        }
        for(int id=3;id<12;++id)assert(positions.at(id).x==positions.at(2).x);
        for(int id=13;id<=18;++id)assert(positions.at(id).x==positions.at(12).x);
    }
    definitions.clear();
    for(int id=1;id<=24;++id) {
        Skill r;r.id=id;r.cap=10;definitions.push_back(r);
        Skill child;child.id=id+100;child.cap=10;child.requires={{id,1}};definitions.push_back(child);
    }
    assert(model.Load(definitions));positions=Layout(model,400);
    for(const auto& [id,p]:positions) {
        assert(p.x+Geometry::width+Geometry::margin<=400);
        for(const auto& req:model.skills.at(id).requires)assert(positions.at(req.id).y<p.y);
    }
    assert(HitPart(Geometry::iconX,Geometry::iconY)==2);
    assert(HitPart(Geometry::leftX,Geometry::arrowY)==-1);
    assert(HitPart(Geometry::rightX,Geometry::arrowY)==1);
    assert(HitPart(Geometry::iconX+Geometry::iconSize,Geometry::iconY)==0);
    assert(HitPart(0,0)==0);
    std::cout<<"Skill tree planner and apply queue checks passed\n";
}
