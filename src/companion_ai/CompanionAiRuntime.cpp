#include "CompanionAiRuntime.h"

#include "DebugLog.h"
#include "GameActor.h"
#include "World.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "network/Connection.h"
#include "network/MapSendProfile.h"
#include "network/Packet.h"
#include "session/Session.h"
#include "skill/Skill.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <windows.h>

namespace companion_ai {
namespace {

constexpr int kEffectStateHidingMask = 0x0004;
constexpr int kEffectStateSpecialHidingMask = 0x0040;
constexpr int kEffectStateBurrowMask = 0x0002;
constexpr int kNpcObjectType = 1;
constexpr int kEventNpcObjectType = 6;
constexpr int kPetObjectType = 7;
constexpr int kHomunculusObjectType = 8;
constexpr int kMercenaryObjectType = 9;
constexpr int kElementalObjectType = 10;

bool EncodeDest(int dstX, int dstY, u8 outDest[3])
{
    if (!outDest || dstX < 0 || dstX > kMaxMoveCoordinate || dstY < 0 || dstY > kMaxMoveCoordinate) {
        return false;
    }
    outDest[0] = static_cast<u8>((dstX >> 2) & 0xFF);
    outDest[1] = static_cast<u8>(((dstX & 0x03) << 6) | ((dstY >> 4) & 0x3F));
    outDest[2] = static_cast<u8>((dstY & 0x0F) << 4);
    return true;
}

CGameMode* CurrentGameMode()
{
    return g_modeMgr.GetCurrentGameMode();
}

CWorld* CurrentWorld()
{
    if (CGameMode* mode = CurrentGameMode()) {
        return mode->m_world;
    }
    return nullptr;
}

Msg2Ai& MsgSlot(CGameActor& actor, Kind kind, bool reserved)
{
    if (kind == Kind::Homunculus) {
        return reserved ? actor.m_homunResMsg : actor.m_homunMsg;
    }
    return reserved ? actor.m_merResMsg : actor.m_merMsg;
}

bool CanSendSkill(const CGameActor& actor)
{
    return actor.m_stateId == MOTION_MOVE || actor.m_stateId == MOTION_STAND || actor.m_stateId == 4;
}

} // namespace

Runtime::Runtime(Kind kind)
    : m_kind(kind)
{
}

void Runtime::Trace(const char* text) const
{
    if (text) {
        DbgLog("[CompanionAI] %s\n", text);
    }
}

u32 Runtime::OwnerId() const
{
    return g_session.m_aid != 0 ? g_session.m_aid : g_session.m_gid;
}

CGameActor* Runtime::FindActor(u32 id) const
{
    CWorld* world = CurrentWorld();
    if (!world || id == 0) {
        return nullptr;
    }
    if (id == OwnerId() && world->m_player) {
        return world->m_player;
    }
    return world->GetGameActorByAID(id);
}

CGameActor* Runtime::FindCompanionActor(Kind kind)
{
    CWorld* world = CurrentWorld();
    if (!world) {
        return nullptr;
    }
    const u32 gid = (kind == Kind::Homunculus) ? g_session.m_homunGID : g_session.m_merGID;
    return gid ? world->GetGameActorByAID(gid) : nullptr;
}

bool Runtime::IsOwnedCompanion(u32 id) const
{
    if (m_kind == Kind::Homunculus) {
        return id != 0 && id == g_session.m_homunGID;
    }
    return id != 0 && id == g_session.m_merGID;
}

bool Runtime::IsHiddenFromAi(const CGameActor& actor, u32 id) const
{
    if (id == OwnerId()) {
        return false;
    }
    const int state = actor.m_effectState;
    return (state & (kEffectStateHidingMask | kEffectStateSpecialHidingMask | kEffectStateBurrowMask)) != 0;
}

int Runtime::AttackIntervalMs() const
{
    const CompanionCombatStatus& combat = (m_kind == Kind::Homunculus)
        ? g_session.m_homunStatus.combat
        : g_session.m_merStatus.combat;
    if (combat.attackSpeed > 0) {
        return combat.attackSpeed;
    }
    return kDefaultAttackIntervalMs;
}

Position Runtime::GetPosition(u32 id) const
{
    Position pos;
    CGameActor* actor = FindActor(id);
    CGameMode* mode = CurrentGameMode();
    if (!actor || !mode || IsHiddenFromAi(*actor, id)) {
        return pos;
    }
    mode->GetServerCoor(actor->m_pos.x, actor->m_pos.z, pos.x, pos.y);
    return pos;
}

int Runtime::GetMotion(u32 id) const
{
    CGameActor* actor = FindActor(id);
    return actor ? actor->m_stateId : -1;
}

int Runtime::GetAttackRange(u32 id) const
{
    if (!FindActor(id)) {
        return -1;
    }
    const CompanionCombatStatus& combat = (m_kind == Kind::Homunculus)
        ? g_session.m_homunStatus.combat
        : g_session.m_merStatus.combat;
    return combat.attackRange > 0 ? combat.attackRange : kDefaultAttackRange;
}

int Runtime::GetSkillAttackRange(u32 /*id*/, int skillId) const
{
    const PLAYER_SKILL_INFO* skill = (m_kind == Kind::Homunculus)
        ? g_session.GetHomunSkillItemBySkillId(skillId)
        : g_session.GetMercSkillItemBySkillId(skillId);
    if (skill && skill->m_isValid && skill->attackRange > 0) {
        return skill->attackRange;
    }
    return kDefaultAttackRange;
}

u32 Runtime::GetTarget(u32 id) const
{
    CGameActor* actor = FindActor(id);
    return actor ? actor->m_targetGid : 0;
}

int Runtime::GetHomunType(u32 id) const
{
    CGameActor* actor = FindActor(id);
    if (!actor) {
        return -1;
    }
    if (actor->m_job >= kHomunJobMin && actor->m_job <= kHomunJobMax) {
        return actor->m_job - kHomunJobBase;
    }
    return actor->m_job;
}

int Runtime::GetMerType(u32 id) const
{
    CGameActor* actor = FindActor(id);
    if (!actor) {
        return -1;
    }
    if (actor->m_job >= kMerJobMin && actor->m_job <= kMerJobMax) {
        return actor->m_job - kMerJobBase;
    }
    return actor->m_job;
}

int Runtime::GetHp(u32 id) const
{
    if (id == OwnerId()) {
        if (CGameActor* owner = FindActor(id)) {
            return static_cast<int>(owner->m_Hp);
        }
        return -1;
    }
    if (m_kind == Kind::Homunculus && id == g_session.m_homunGID) {
        return g_session.m_homunStatus.combat.hp;
    }
    if (m_kind == Kind::Mercenary && id == g_session.m_merGID) {
        return g_session.m_merStatus.combat.hp;
    }
    return -1;
}

int Runtime::GetSp(u32 id) const
{
    if (id == OwnerId()) {
        if (CGameActor* owner = FindActor(id)) {
            return static_cast<int>(owner->m_Sp);
        }
        return -1;
    }
    if (m_kind == Kind::Homunculus && id == g_session.m_homunGID) {
        return g_session.m_homunStatus.combat.sp;
    }
    if (m_kind == Kind::Mercenary && id == g_session.m_merGID) {
        return g_session.m_merStatus.combat.sp;
    }
    return -1;
}

int Runtime::GetMaxHp(u32 id) const
{
    if (id == OwnerId()) {
        if (CGameActor* owner = FindActor(id)) {
            return static_cast<int>(owner->m_MaxHp);
        }
        return -1;
    }
    if (m_kind == Kind::Homunculus && id == g_session.m_homunGID) {
        return g_session.m_homunStatus.combat.maxHp;
    }
    if (m_kind == Kind::Mercenary && id == g_session.m_merGID) {
        return g_session.m_merStatus.combat.maxHp;
    }
    return -1;
}

int Runtime::GetMaxSp(u32 id) const
{
    if (id == OwnerId()) {
        if (CGameActor* owner = FindActor(id)) {
            return static_cast<int>(owner->m_MaxSp);
        }
        return -1;
    }
    if (m_kind == Kind::Homunculus && id == g_session.m_homunGID) {
        return g_session.m_homunStatus.combat.maxSp;
    }
    if (m_kind == Kind::Mercenary && id == g_session.m_merGID) {
        return g_session.m_merStatus.combat.maxSp;
    }
    return -1;
}

std::vector<u32> Runtime::GetActors() const
{
    std::vector<u32> ids;
    CWorld* world = CurrentWorld();
    if (!world) {
        return ids;
    }
    ids.reserve(world->m_actorList.size() + 1);
    for (CGameActor* actor : world->m_actorList) {
        if (actor && actor->m_gid) {
            ids.push_back(actor->m_gid);
        }
    }
    const u32 owner = OwnerId();
    if (owner) {
        ids.push_back(owner);
    }
    return ids;
}

bool Runtime::IsMonster(u32 id) const
{
    const CGameActor* actor = FindActor(id);
    if (!actor || actor->m_isPc) {
        return false;
    }
    // NPCs and companions can use the same sprite jobs as monsters.
    switch (actor->m_objectType) {
    case kNpcObjectType:
    case kEventNpcObjectType:
    case kPetObjectType:
    case kHomunculusObjectType:
    case kMercenaryObjectType:
    case kElementalObjectType:
        return false;
    }
    const int job = actor->m_job;
    return job >= kMonsterJobMin && (job < kHomunJobMin || job > kMerJobMax);
}

u32 Runtime::GetWalkCellDuration(u32 id) const
{
    const auto* actor = FindActor(id);
    return actor && actor->m_speed != 0 ? actor->m_speed : 150u;
}

Msg2Ai Runtime::ConsumeMsg(u32 myId)
{
    Msg2Ai empty;
    if (!IsOwnedCompanion(myId)) {
        return empty;
    }
    CGameActor* self = FindActor(myId);
    if (!self) {
        return empty;
    }
    Msg2Ai& slot = MsgSlot(*self, m_kind, false);
    Msg2Ai msg = slot;
    slot.Clear();
    return msg;
}

Msg2Ai Runtime::ConsumeResMsg(u32 myId)
{
    Msg2Ai empty;
    if (!IsOwnedCompanion(myId)) {
        return empty;
    }
    CGameActor* self = FindActor(myId);
    if (!self) {
        return empty;
    }
    Msg2Ai& slot = MsgSlot(*self, m_kind, true);
    Msg2Ai msg = slot;
    slot.Clear();
    return msg;
}

bool Runtime::QueueCommand(Kind kind, const Msg2Ai& msg, bool reserved)
{
    const bool alive = kind == Kind::Homunculus ? g_session.m_homunStatus.alive : g_session.m_merStatus.alive;
    if (!alive) {
        return false;
    }
    CGameActor* actor = FindCompanionActor(kind);
    if (!actor) {
        return false;
    }
    MsgSlot(*actor, kind, reserved) = msg;
    return true;
}

bool Runtime::Move(u32 id, int x, int y) const
{
    if (!IsOwnedCompanion(id)) {
        return false;
    }
    CGameActor* actor = FindActor(id);
    CGameMode* mode = CurrentGameMode();
    if (!actor || !mode) {
        return false;
    }

    int sx = 0;
    int sy = 0;
    mode->GetServerCoor(actor->m_pos.x, actor->m_pos.z, sx, sy);
    if (std::abs(x - sx) + std::abs(y - sy) > kServerPathLimitManhattan) {
        return false;
    }

    PACKET_CZ_REQUEST_MOVENPC packet{};
    packet.PacketType = kPacketRequestMoveNpc;
    packet.GID = id;
    if (!EncodeDest(x, y, packet.dest)) {
        return false;
    }
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void Runtime::MoveToOwner(u32 id) const
{
    if (!IsOwnedCompanion(id)) {
        return;
    }
    PACKET_CZ_REQUEST_MOVETOOWNER packet{};
    packet.PacketType = kPacketRequestMoveToOwner;
    packet.GID = id;
    CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

bool Runtime::CanTargetForAttack(u32 id) const
{
    if (id == OwnerId() || id == g_session.m_homunGID || id == g_session.m_merGID) {
        return false;
    }
    const CGameActor* target = FindActor(id);
    return target && target->m_stateId != MOTION_DEAD && !IsHiddenFromAi(*target, id);
}

void Runtime::Attack(u32 id, u32 enemyId) const
{
    if (!IsOwnedCompanion(id) || !FindActor(id) || !CanTargetForAttack(enemyId)) {
        return;
    }

    const u32 now = timeGetTime();
    if (m_lastAttackTick != 0 && (now - m_lastAttackTick) < static_cast<u32>(AttackIntervalMs())) {
        return;
    }

    PACKET_CZ_REQUEST_ACTNPC packet{};
    packet.PacketType = kPacketRequestActNpc;
    packet.GID = id;
    packet.targetGID = enemyId;
    packet.action = kActionAttack;
    if (CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet))) {
        m_lastAttackTick = now;
    }
}

void Runtime::StopAttack(u32 id) const
{
    if (!IsOwnedCompanion(id)) {
        return;
    }
    // The selected server clears the attack target when it resolves GID zero.
    const PACKET_CZ_REQUEST_ACTNPC packet{kPacketRequestActNpc, id, 0, kActionAttack};
    CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

void Runtime::ClearAttackSelection(u32 enemyId) const
{
    auto& selection = m_kind == Kind::Homunculus ? g_session.m_homunTarget : g_session.m_merTarget;
    if (selection.attacking && selection.gid == enemyId) {
        selection = {};
    }
}

SkillSendResult Runtime::SkillObject(u32 id, int level, int skillId, u32 targetId) const
{
    if (!IsOwnedCompanion(id) || level <= 0 || skillId <= 0 || targetId == 0) {
        return SkillSendResult::Invalid;
    }
    const bool homunSkill = skillId > 8000 && skillId < 8017;
    const bool mercSkill = skillId > SKILL_INFO::kMercenarySkillFirst && SKILL_INFO::IsMercenarySkill(skillId);
    if ((m_kind == Kind::Homunculus && !homunSkill) || (m_kind == Kind::Mercenary && !mercSkill)) {
        return SkillSendResult::Invalid;
    }

    CGameActor* self = FindActor(id);
    CGameActor* target = FindActor(targetId);
    if (!self || !target || self->m_stateId == MOTION_DEAD) {
        return SkillSendResult::Invalid;
    }
    if (!CanSendSkill(*self)) {
        return SkillSendResult::Retry;
    }

    std::array<u8, 32> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveUseSkillToIdPacket(
            static_cast<u16>(skillId),
            static_cast<u16>(level),
            targetId,
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return SkillSendResult::Invalid;
    }
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packetBuffer.data()), packetLength)
        ? SkillSendResult::Sent : SkillSendResult::Invalid;
}

SkillSendResult Runtime::SkillGround(u32 id, int level, int skillId, int x, int y) const
{
    if (!IsOwnedCompanion(id) || level <= 0 || skillId <= 0) {
        return SkillSendResult::Invalid;
    }
    const bool homunSkill = skillId > 8000 && skillId < 8017;
    const bool mercSkill = skillId > 8200 && skillId < 8238;
    if ((m_kind == Kind::Homunculus && !homunSkill) || (m_kind == Kind::Mercenary && !mercSkill)) {
        return SkillSendResult::Invalid;
    }
    CGameActor* self = FindActor(id);
    if (!self || self->m_stateId == MOTION_DEAD) {
        return SkillSendResult::Invalid;
    }
    if (!CanSendSkill(*self)) {
        return SkillSendResult::Retry;
    }

    std::array<u8, 128> packetBuffer{};
    int packetLength = 0;
    if (!ro::net::BuildActiveUseSkillToPosInfoPacket(
            static_cast<u16>(skillId),
            static_cast<u16>(level),
            static_cast<s16>(x),
            static_cast<s16>(y),
            "",
            packetBuffer.data(),
            static_cast<int>(packetBuffer.size()),
            &packetLength)) {
        return SkillSendResult::Invalid;
    }
    return CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packetBuffer.data()), packetLength)
        ? SkillSendResult::Sent : SkillSendResult::Invalid;
}

int Runtime::GetDistance(int x1, int y1, int x2, int y2) const
{
    const double dx = static_cast<double>(x1 - x2);
    const double dy = static_cast<double>(y1 - y2);
    return static_cast<int>(std::floor(std::sqrt(dx * dx + dy * dy)));
}

int Runtime::GetDistance2(u32 id1, u32 id2) const
{
    const Position a = GetPosition(id1);
    const Position b = GetPosition(id2);
    if (!a.valid() || !b.valid()) {
        return -1;
    }
    return GetDistance(a.x, a.y, b.x, b.y);
}

int Runtime::GetDistanceFromOwner(u32 id) const
{
    return GetDistance2(OwnerId(), id);
}

bool Runtime::IsOutOfSight(u32 id1, u32 id2) const
{
    const Position a = GetPosition(id1);
    const Position b = GetPosition(id2);
    if (!a.valid() || !b.valid()) {
        return true;
    }
    return GetDistance(a.x, a.y, b.x, b.y) > kOutOfSightDistance;
}

bool Runtime::IsInAttackSight(u32 id1, u32 id2, int skillId) const
{
    const Position a = GetPosition(id1);
    const Position b = GetPosition(id2);
    if (!a.valid() || !b.valid()) {
        return false;
    }
    const int d = GetDistance(a.x, a.y, b.x, b.y);
    const int range = (skillId == 0) ? GetAttackRange(id1) : GetSkillAttackRange(id1, skillId);
    return range >= d;
}

} // namespace companion_ai
