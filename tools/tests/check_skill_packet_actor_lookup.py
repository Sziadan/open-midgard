#!/usr/bin/env python3
"""Guard the shared, non-creating actor/unit lookup contract in WP3.2."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
packet = (ROOT / "src/gamemode/GameModePacket.cpp").read_text(encoding="utf-8")
world = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")
actor = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
mode_h = (ROOT / "src/gamemode/GameMode.h").read_text(encoding="utf-8")


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated body: {signature}")


actor_lookup = body(packet, "CGameActor* ResolveSkillPacketActor(CGameMode& mode, u32 aid)")
compact = re.sub(r"\s+", "", actor_lookup)
expected = ("mode.m_world==nullptr", "aid==g_session.m_aid", "mode.m_world->m_player",
            "mode.m_world->GetGameActorByAID(aid)")
positions = [compact.find(token) for token in expected]
assert all(position >= 0 for position in positions) and positions == sorted(positions), \
    "actor resolver must be own AID -> player, else CWorld actor list"
assert "m_runtimeActors" not in actor_lookup and "EnsureRuntimeActor" not in actor_lookup
assert "m_gid" not in actor_lookup and "ResolveCombatActor" not in actor_lookup

unit_lookup = body(packet, "CSkill* ResolveSkillPacketUnit(CGameMode& mode, u32 aid)")
assert "mode.m_world ? mode.m_world->GetSkillByAID(aid) : nullptr" in unit_lookup

# These are WP3.2 packet paths. Keep all world-list and unit-list reads behind
# the shared lookup functions; spawn/movement handlers outside this set still
# legitimately use EnsureRuntimeActor and m_runtimeActors.
handlers = (
    "void HandleNotifyEffect(", "void HandleNotifyEffect2(", "void HandleNotifyEffect3(",
    "void HandleSpirits(", "void HandleSpirits2(", "void HandleDevotionList(",
    "void HandleSkillCastCancel(", "void HandleSound(", "void HandleComboDelay(",
    "void HandleBladeStop(", "void HandleSkillCastAck(", "void DeliverSkillUse(",
    "void HandleSkillDamageNotify(", "void HandleGroundSkillNotify(",
    "void DeliverUseSkill(", "void HandleSkillNoDamageNotify(",
    "void PlaceSkillUnit(", "void HandleSkillUpdate(", "void HandleTalkboxChatContents(",
    "void HandleChangeGuild(", "void MakePotionEffect(",
)
for signature in handlers:
    current = body(packet, signature)
    for forbidden in ("ResolveNotifyEffectActor", "ResolveCombatActor", "EnsureRuntimeActor",
                      "m_runtimeActors", "GetGameActorByAID(", "GetSkillByAID("):
        assert forbidden not in current, f"{signature} retains ad-hoc actor/unit lookup: {forbidden}"

world_lookup = body(world, "CGameActor* CWorld::GetGameActorByAID(u32 aid) const")
assert "for (CGameActor* actor : m_actorList)" in world_lookup
assert "actor->m_gid == aid" in world_lookup
unit_world_lookup = body(world, "CSkill* CWorld::GetSkillByAID(u32 aid)")
assert "m_skillList" in unit_world_lookup and "skill->m_aid == aid" in unit_world_lookup
assert "void CGameActor::RegisterPos()" in actor and "g_world.RegisterActor(this)" in actor
assert "void CGameActor::UnRegisterPos()" in actor and "g_world.UnregisterActor(this)" in actor
assert "std::map<u32, CGameActor*> m_runtimeActors;" in mode_h

print(f"WP3.2 shared actor/unit lookup contract passed for {len(handlers)} handler bodies")
