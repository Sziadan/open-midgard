#include "CompanionAiBrain.h"
#include "skill/Skill.h"

#include <algorithm>
#include <cstdlib>

namespace companion_ai {

Brain::Brain(Kind kind)
    : m_runtime(kind)
{
}

void Brain::Tick(u32 myId, u32 now)
{
    m_myId = myId;
    m_now = now;
    const Msg2Ai msg = m_runtime.ConsumeMsg(myId);
    const Msg2Ai rmsg = m_runtime.ConsumeResMsg(myId);

    if (msg.command == NONE_CMD) {
        if (rmsg.command != NONE_CMD && static_cast<int>(m_resCmdList.size()) < kReservedCmdQueueCap) {
            m_resCmdList.push_back(rmsg);
        }
    } else {
        m_resCmdList.clear();
        ProcessCommand(msg);
    }

    switch (m_state) {
    case IDLE_ST: OnIDLE_ST(); break;
    case CHASE_ST: OnCHASE_ST(); break;
    case ATTACK_ST: OnATTACK_ST(); break;
    case FOLLOW_ST: OnFOLLOW_ST(); break;
    case MOVE_CMD_ST: OnMOVE_CMD_ST(); break;
    case STOP_CMD_ST: OnSTOP_CMD_ST(); break;
    case ATTACK_OBJECT_CMD_ST: OnATTACK_OBJECT_CMD_ST(); break;
    case ATTACK_AREA_CMD_ST: OnATTACK_AREA_CMD_ST(); break;
    case PATROL_CMD_ST: OnPATROL_CMD_ST(); break;
    case HOLD_CMD_ST: OnHOLD_CMD_ST(); break;
    case SKILL_OBJECT_CMD_ST: OnSKILL_OBJECT_CMD_ST(); break;
    case SKILL_AREA_CMD_ST: OnSKILL_AREA_CMD_ST(); break;
    case FOLLOW_CMD_ST: OnFOLLOW_CMD_ST(); break;
    default: break;
    }
}

void Brain::ProcessCommand(const Msg2Ai& msg)
{
    m_approach = {};
    switch (msg.command) {
    case MOVE_CMD: OnMOVE_CMD(msg.x, msg.y); break;
    case STOP_CMD: OnSTOP_CMD(); break;
    case ATTACK_OBJECT_CMD: OnATTACK_OBJECT_CMD(msg.targetId); break;
    case ATTACK_AREA_CMD: OnATTACK_AREA_CMD(msg.x, msg.y); break;
    case PATROL_CMD: OnPATROL_CMD(msg.x, msg.y); break;
    case HOLD_CMD: OnHOLD_CMD(); break;
    case SKILL_OBJECT_CMD: OnSKILL_OBJECT_CMD(msg.skillLevel, msg.skillId, msg.targetId); break;
    case SKILL_AREA_CMD: OnSKILL_AREA_CMD(msg.skillLevel, msg.skillId, msg.x, msg.y); break;
    case FOLLOW_CMD: OnFOLLOW_CMD(); break;
    default: break;
    }
}

bool Brain::ContinueApproach(const Position& destination)
{
    const Position current = m_runtime.GetPosition(m_myId);
    if (!current.valid() || !destination.valid()) {
        return false;
    }

    auto& approach = m_approach;
    const bool firstRequest = !approach.destination.valid();
    if (firstRequest || current.x != approach.lastPosition.x || current.y != approach.lastPosition.y) {
        approach.lastPosition = current;
        approach.lastProgress = m_now;
        approach.retried = false;
    }

    // 0232 has no dedicated reply. Allow time for several cells plus network delay.
    const u32 stallDelay = std::max(1000u, 3u * m_runtime.GetWalkCellDuration(m_myId));
    const bool stalled = m_now - approach.lastProgress >= stallDelay;
    if (stalled && approach.retried) {
        return false;
    }
    const bool changed = destination.x != approach.destination.x || destination.y != approach.destination.y;
    if (firstRequest || stalled || (changed && m_now - approach.lastRequest >= stallDelay / 2)) {
        if (!m_runtime.Move(m_myId, destination.x, destination.y)) {
            return false;
        }
        approach.destination = destination;
        approach.lastRequest = m_now;
        if (stalled) {
            approach.retried = true;
            approach.lastProgress = m_now;
        }
    }
    return true;
}

void Brain::CancelApproach()
{
    m_state = IDLE_ST;
    m_enemy = 0;
    m_skill = 0;
    m_destX = 0;
    m_destY = 0;
    m_approach = {};
}

void Brain::OnMOVE_CMD(int x, int y)
{
    m_runtime.Trace("OnMOVE_CMD");
    if (x == m_destX && y == m_destY && m_runtime.GetMotion(m_myId) == MOTION_MOVE) {
        return;
    }

    const Position cur = m_runtime.GetPosition(m_myId);
    if (cur.valid() && (std::abs(x - cur.x) + std::abs(y - cur.y) > kLongMoveManhattan)) {
        Msg2Ai reserved;
        reserved.command = MOVE_CMD;
        reserved.x = x;
        reserved.y = y;
        m_resCmdList.push_front(reserved);
        x = (x + cur.x) / 2;
        y = (y + cur.y) / 2;
    }

    m_runtime.Move(m_myId, x, y);
    m_state = MOVE_CMD_ST;
    m_destX = x;
    m_destY = y;
    m_enemy = 0;
    m_skill = 0;
}

void Brain::OnSTOP_CMD()
{
    m_runtime.Trace("OnSTOP_CMD");
    m_runtime.StopAttack(m_myId);
    m_state = IDLE_ST;
    m_destX = 0;
    m_destY = 0;
    m_enemy = 0;
    m_skill = 0;
}

void Brain::OnATTACK_OBJECT_CMD(u32 id)
{
    m_runtime.Trace("OnATTACK_OBJECT_CMD");
    m_skill = 0;
    m_enemy = id;
    m_state = CHASE_ST;
}

void Brain::OnATTACK_AREA_CMD(int x, int y)
{
    m_runtime.Trace("OnATTACK_AREA_CMD");
    if (x != m_destX || y != m_destY || m_runtime.GetMotion(m_myId) != MOTION_MOVE) {
        m_runtime.Move(m_myId, x, y);
    }
    m_destX = x;
    m_destY = y;
    m_enemy = 0;
    m_state = ATTACK_AREA_CMD_ST;
}

void Brain::OnPATROL_CMD(int x, int y)
{
    m_runtime.Trace("OnPATROL_CMD");
    const Position cur = m_runtime.GetPosition(m_myId);
    m_patrolX = cur.x;
    m_patrolY = cur.y;
    m_destX = x;
    m_destY = y;
    m_runtime.Move(m_myId, x, y);
    m_state = PATROL_CMD_ST;
}

void Brain::OnHOLD_CMD()
{
    m_runtime.Trace("OnHOLD_CMD");
    m_destX = 0;
    m_destY = 0;
    m_enemy = 0;
    m_state = HOLD_CMD_ST;
}

void Brain::OnSKILL_OBJECT_CMD(int level, int skill, u32 id)
{
    m_runtime.Trace("OnSKILL_OBJECT_CMD");
    m_skillLevel = level;
    m_skill = skill;
    m_enemy = id;
    m_state = CHASE_ST;
}

void Brain::OnSKILL_AREA_CMD(int level, int skill, int x, int y)
{
    m_runtime.Trace("OnSKILL_AREA_CMD");
    m_destX = x;
    m_destY = y;
    m_skillLevel = level;
    m_skill = skill;
    m_state = SKILL_AREA_CMD_ST;
}

void Brain::OnFOLLOW_CMD()
{
    if (m_state != FOLLOW_CMD_ST) {
        m_runtime.MoveToOwner(m_myId);
        m_state = FOLLOW_CMD_ST;
        const Position owner = m_runtime.GetPosition(m_runtime.OwnerId());
        m_destX = owner.x;
        m_destY = owner.y;
        m_enemy = 0;
        m_skill = 0;
        m_runtime.Trace("OnFOLLOW_CMD");
    } else {
        m_state = IDLE_ST;
        m_enemy = 0;
        m_skill = 0;
        m_runtime.Trace("FOLLOW_CMD_ST --> IDLE_ST");
    }
}

void Brain::OnIDLE_ST()
{
    m_runtime.Trace("OnIDLE_ST");
    if (!m_resCmdList.empty()) {
        const Msg2Ai cmd = m_resCmdList.front();
        m_resCmdList.pop_front();
        ProcessCommand(cmd);
        return;
    }

    u32 object = GetOwnerEnemy(m_myId);
    if (object != 0) {
        m_state = CHASE_ST;
        m_enemy = object;
        m_runtime.Trace("IDLE_ST -> CHASE_ST : MYOWNER_ATTACKED_IN");
        return;
    }

    object = GetMyEnemy(m_myId);
    if (object != 0) {
        m_state = CHASE_ST;
        m_enemy = object;
        m_runtime.Trace("IDLE_ST -> CHASE_ST : ATTACKED_IN");
        return;
    }

    const int distance = m_runtime.GetDistanceFromOwner(m_myId);
    if (distance > kFollowDistance || distance == -1) {
        m_state = FOLLOW_ST;
        m_runtime.Trace("IDLE_ST -> FOLLOW_ST");
    }
}

void Brain::OnFOLLOW_ST()
{
    m_runtime.Trace("OnFOLLOW_ST");
    if (m_runtime.GetDistanceFromOwner(m_myId) <= kFollowDistance) {
        m_state = IDLE_ST;
        m_runtime.Trace("FOLLOW_ST -> IDLE_ST");
    } else if (m_runtime.GetMotion(m_myId) == MOTION_STAND) {
        m_runtime.MoveToOwner(m_myId);
        m_runtime.Trace("FOLLOW_ST -> FOLLOW_ST");
    }
}

void Brain::OnCHASE_ST()
{
    m_runtime.Trace("OnCHASE_ST");
    if (m_runtime.IsOutOfSight(m_myId, m_enemy) || m_runtime.GetMotion(m_enemy) == MOTION_DEAD
        || (m_skill == 0 && !m_runtime.CanTargetForAttack(m_enemy))) {
        CancelApproach();
        m_runtime.Trace("CHASE_ST -> IDLE_ST : ENEMY_OUTSIGHT_IN");
        return;
    }
    if (m_runtime.IsInAttackSight(m_myId, m_enemy, m_skill)) {
        m_state = ATTACK_ST;
        m_approach = {};
        m_runtime.Trace("CHASE_ST -> ATTACK_ST : ENEMY_INATTACKSIGHT_IN");
        return;
    }

    const Position enemyPos = m_runtime.GetPosition(m_enemy);
    if (m_skill != 0) {
        if (!ContinueApproach(enemyPos)) {
            CancelApproach();
        }
        return;
    }
    if (enemyPos.valid() && (m_destX != enemyPos.x || m_destY != enemyPos.y)) {
        m_destX = enemyPos.x;
        m_destY = enemyPos.y;
        m_runtime.Move(m_myId, m_destX, m_destY);
        m_runtime.Trace("CHASE_ST -> CHASE_ST : DESTCHANGED_IN");
    }
}

void Brain::OnATTACK_ST()
{
    m_runtime.Trace("OnATTACK_ST");
    if (m_runtime.IsOutOfSight(m_myId, m_enemy) || m_runtime.GetMotion(m_enemy) == MOTION_DEAD
        || (m_skill == 0 && !m_runtime.CanTargetForAttack(m_enemy))) {
        CancelApproach();
        m_runtime.Trace("ATTACK_ST -> IDLE_ST");
        return;
    }
    if (!m_runtime.IsInAttackSight(m_myId, m_enemy, m_skill)) {
        m_state = CHASE_ST;
        if (m_skill != 0) {
            OnCHASE_ST();
        } else {
            const Position enemyPos = m_runtime.GetPosition(m_enemy);
            m_destX = enemyPos.x;
            m_destY = enemyPos.y;
            m_runtime.Move(m_myId, m_destX, m_destY);
        }
        m_runtime.Trace("ATTACK_ST -> CHASE_ST  : ENEMY_OUTATTACKSIGHT_IN");
        return;
    }

    if (m_skill == 0) {
        m_runtime.Attack(m_myId, m_enemy);
    } else {
        const auto result = m_runtime.SkillObject(m_myId, m_skillLevel, m_skill, m_enemy);
        if (result == SkillSendResult::Retry) {
            return;
        }
        if (result == SkillSendResult::Invalid || SKILL_INFO::IsGoodSkillForActor(m_skill)) {
            m_enemy = 0;
            m_state = IDLE_ST;
        }
        m_skill = 0;
    }
    m_runtime.Trace("ATTACK_ST -> ATTACK_ST  : ENERGY_RECHARGED_IN");
}

void Brain::OnMOVE_CMD_ST()
{
    m_runtime.Trace("OnMOVE_CMD_ST");
    const Position cur = m_runtime.GetPosition(m_myId);
    if (cur.valid() && cur.x == m_destX && cur.y == m_destY) {
        m_state = IDLE_ST;
    }
}

void Brain::OnSTOP_CMD_ST() {}
void Brain::OnATTACK_OBJECT_CMD_ST() {}
void Brain::OnSKILL_OBJECT_CMD_ST() {}

void Brain::OnATTACK_AREA_CMD_ST()
{
    m_runtime.Trace("OnATTACK_AREA_CMD_ST");
    u32 object = GetOwnerEnemy(m_myId);
    if (object == 0) {
        object = GetMyEnemy(m_myId);
    }
    if (object != 0) {
        m_state = CHASE_ST;
        m_enemy = object;
        return;
    }
    const Position cur = m_runtime.GetPosition(m_myId);
    if (cur.valid() && cur.x == m_destX && cur.y == m_destY) {
        m_state = IDLE_ST;
    }
}

void Brain::OnPATROL_CMD_ST()
{
    m_runtime.Trace("OnPATROL_CMD_ST");
    u32 object = GetOwnerEnemy(m_myId);
    if (object == 0) {
        object = GetMyEnemy(m_myId);
    }
    if (object != 0) {
        m_state = CHASE_ST;
        m_enemy = object;
        m_runtime.Trace("PATROL_CMD_ST -> CHASE_ST : ATTACKED_IN");
        return;
    }

    const Position cur = m_runtime.GetPosition(m_myId);
    if (cur.valid() && cur.x == m_destX && cur.y == m_destY) {
        m_destX = m_patrolX;
        m_destY = m_patrolY;
        m_patrolX = cur.x;
        m_patrolY = cur.y;
        m_runtime.Move(m_myId, m_destX, m_destY);
    }
}

void Brain::OnHOLD_CMD_ST()
{
    m_runtime.Trace("OnHOLD_CMD_ST");
    if (m_enemy != 0) {
        const int d = m_runtime.GetDistance2(m_enemy, m_myId);
        if (d != -1 && d <= m_runtime.GetAttackRange(m_myId) && m_runtime.CanTargetForAttack(m_enemy)) {
            m_runtime.Attack(m_myId, m_enemy);
        } else {
            m_enemy = 0;
        }
        return;
    }

    u32 object = GetOwnerEnemy(m_myId);
    if (object == 0) {
        object = GetMyEnemy(m_myId);
        if (object == 0) {
            return;
        }
    }
    m_enemy = object;
}

void Brain::OnSKILL_AREA_CMD_ST()
{
    m_runtime.Trace("OnSKILL_AREA_CMD_ST");
    const Position cur = m_runtime.GetPosition(m_myId);
    if (!cur.valid()) {
        CancelApproach();
        return;
    }
    if (m_runtime.GetDistance(cur.x, cur.y, m_destX, m_destY)
        <= m_runtime.GetSkillAttackRange(m_myId, m_skill)) {
        m_approach = {};
        if (m_runtime.SkillGround(m_myId, m_skillLevel, m_skill, m_destX, m_destY) == SkillSendResult::Retry) {
            return;
        }
        CancelApproach();
    } else if (!ContinueApproach({m_destX, m_destY})) {
        CancelApproach();
    }
}

void Brain::OnFOLLOW_CMD_ST()
{
    m_runtime.Trace("OnFOLLOW_CMD_ST");
    const Position owner = m_runtime.GetPosition(m_runtime.OwnerId());
    const Position me = m_runtime.GetPosition(m_myId);
    if (!owner.valid() || !me.valid()) {
        return;
    }

    int d = m_runtime.GetDistance(owner.x, owner.y, me.x, me.y);
    if (d <= kFollowDistance) {
        return;
    }

    if (m_runtime.GetMotion(m_myId) == MOTION_MOVE) {
        d = m_runtime.GetDistance(owner.x, owner.y, m_destX, m_destY);
        if (d > kFollowDistance) {
            m_runtime.MoveToOwner(m_myId);
            m_destX = owner.x;
            m_destY = owner.y;
        }
    } else {
        m_runtime.MoveToOwner(m_myId);
        m_destX = owner.x;
        m_destY = owner.y;
    }
}

u32 Brain::GetOwnerEnemy(u32 myid)
{
    u32 result = 0;
    const u32 owner = m_runtime.OwnerId();
    const auto actors = m_runtime.GetActors();
    int minDis = kOutOfSightDistance + 1;

    for (u32 v : actors) {
        if (!m_runtime.CanTargetForAttack(v)) {
            continue;
        }
        if (m_runtime.GetTarget(v) != owner) {
            continue;
        }

        bool enemy = false;
        if (m_runtime.IsMonster(v)) {
            enemy = true;
        } else {
            const int motion = m_runtime.GetMotion(v);
            enemy = (motion == MOTION_ATTACK || motion == MOTION_ATTACK2);
        }
        if (!enemy) {
            continue;
        }

        const int dis = m_runtime.GetDistance2(myid, v);
        if (dis >= 0 && dis < minDis) {
            result = v;
            minDis = dis;
        }
    }
    return result;
}

bool Brain::IsPassiveHomunType(int type) const
{
    return type == LIF || type == LIF_H || type == AMISTR || type == AMISTR_H
        || type == LIF2 || type == LIF_H2 || type == AMISTR2 || type == AMISTR_H2;
}

bool Brain::IsAggressiveHomunType(int type) const
{
    return type == FILIR || type == FILIR_H || type == VANILMIRTH || type == VANILMIRTH_H
        || type == FILIR2 || type == FILIR_H2 || type == VANILMIRTH2 || type == VANILMIRTH_H2;
}

u32 Brain::GetMyEnemy(u32 myid)
{
    if (m_runtime.kind() == Kind::Homunculus) {
        const int type = m_runtime.GetHomunType(myid);
        if (IsPassiveHomunType(type)) {
            return GetMyEnemyA(myid);
        }
        if (IsAggressiveHomunType(type)) {
            return GetMyEnemyB(myid);
        }
        return 0;
    }

    const int type = m_runtime.GetMerType(myid);
    if (type >= ARCHER01 && type <= SWORDMAN10) {
        return GetMyEnemyA(myid);
    }
    return GetMyEnemyB(myid);
}

u32 Brain::GetMyEnemyA(u32 myid)
{
    u32 result = 0;
    const auto actors = m_runtime.GetActors();
    int minDis = kOutOfSightDistance + 1;
    for (u32 v : actors) {
        if (!m_runtime.CanTargetForAttack(v)) {
            continue;
        }
        if (m_runtime.GetTarget(v) != myid) {
            continue;
        }
        const int dis = m_runtime.GetDistance2(myid, v);
        if (dis >= 0 && dis < minDis) {
            result = v;
            minDis = dis;
        }
    }
    return result;
}

u32 Brain::GetMyEnemyB(u32 myid)
{
    u32 result = 0;
    const auto actors = m_runtime.GetActors();
    int minDis = kOutOfSightDistance + 1;
    for (u32 v : actors) {
        if (!m_runtime.CanTargetForAttack(v)) {
            continue;
        }
        if (!m_runtime.IsMonster(v)) {
            continue;
        }
        const int dis = m_runtime.GetDistance2(myid, v);
        if (dis >= 0 && dis < minDis) {
            result = v;
            minDis = dis;
        }
    }
    return result;
}

} // namespace companion_ai
