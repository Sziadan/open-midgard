#pragma once

#include "CompanionAiTypes.h"

#include <utility>
#include <vector>

class CGameActor;

namespace companion_ai {

struct Position {
    int x = -1;
    int y = -1;
    bool valid() const { return x >= 0 && y >= 0; }
};

enum class SkillSendResult { Invalid, Retry, Sent };

class Runtime {
public:
    explicit Runtime(Kind kind);

    Kind kind() const { return m_kind; }

    void Trace(const char* text) const;

    u32 OwnerId() const;
    Position GetPosition(u32 id) const;
    int GetMotion(u32 id) const;
    int GetAttackRange(u32 id) const;
    int GetSkillAttackRange(u32 id, int skillId) const;
    u32 GetTarget(u32 id) const;
    int GetHomunType(u32 id) const;
    int GetMerType(u32 id) const;
    int GetHp(u32 id) const;
    int GetSp(u32 id) const;
    int GetMaxHp(u32 id) const;
    int GetMaxSp(u32 id) const;

    std::vector<u32> GetActors() const;
    bool IsMonster(u32 id) const;
    u32 GetTick() const;

    Msg2Ai ConsumeMsg(u32 myId);
    Msg2Ai ConsumeResMsg(u32 myId);

    void Move(u32 id, int x, int y) const;
    void MoveToOwner(u32 id) const;
    void Attack(u32 id, u32 enemyId) const;
    void StopAttack(u32 id) const;
    SkillSendResult SkillObject(u32 id, int level, int skillId, u32 targetId) const;
    SkillSendResult SkillGround(u32 id, int level, int skillId, int x, int y) const;

    int GetDistance(int x1, int y1, int x2, int y2) const;
    int GetDistance2(u32 id1, u32 id2) const;
    int GetDistanceFromOwner(u32 id) const;
    bool IsOutOfSight(u32 id1, u32 id2) const;
    bool IsInAttackSight(u32 id1, u32 id2, int skillId) const;

    static bool QueueCommand(Kind kind, const Msg2Ai& msg, bool reserved);
    static CGameActor* FindCompanionActor(Kind kind);

private:
    CGameActor* FindActor(u32 id) const;
    bool IsOwnedCompanion(u32 id) const;
    bool IsHiddenFromAi(const CGameActor& actor, u32 id) const;
    int AttackIntervalMs() const;

    Kind m_kind;
    mutable u32 m_lastAttackTick = 0;
};

} // namespace companion_ai
