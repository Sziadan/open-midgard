import re
import sys
from pathlib import Path

actor_path, ref_actor_path, ref_npc_path = map(Path, sys.argv[1:4])
actor = actor_path.read_text(encoding="utf-8", errors="replace")
ref_actor = ref_actor_path.read_text(encoding="utf-8", errors="replace")
ref_npc = ref_npc_path.read_text(encoding="utf-8", errors="replace")

def body(source: str, signature: str, end: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing source signature: {signature}")
    finish = source.find(end, start + len(signature))
    if finish < 0:
        raise AssertionError(f"missing end marker after: {signature}")
    return source[start:finish]

current = body(actor, "bool CGameActor::ProcessStatusMsgEffectMessage(int msg)", "void CGameActor::SendMsg(")
reference = body(ref_actor, "case 58:", "case 101:")
sendmsg_start = actor.find("void CGameActor::SendMsg(")
if sendmsg_start < 0:
    raise AssertionError("missing CGameActor::SendMsg production receiver")
sendmsg = actor[sendmsg_start:]
local_resurrection = body(sendmsg, "    case 98:", "    case 88: {")
ref_resurrection = body(ref_actor, "    case 98:", "    case 99:")
expected_cases = [58,59,62,63,67,68,69,70,71,72,73,74,75,76]
for case in expected_cases:
    if not re.search(rf"\bcase\s+{case}\s*:", current):
        raise AssertionError(f"current status dispatcher missing message {case}")
    if not re.search(rf"\bcase\s+{case}\s*:", reference):
        raise AssertionError(f"Ref/GameActor.cpp missing message {case}")

for token in ("m_iceEffect", "m_curseEffect", "m_sleepEffect", "m_stunEffect", "m_silenceEffect", "m_angelusEffect"):
    if token not in current or token not in reference:
        raise AssertionError(f"status slot {token} differs from the reference lifecycle")
for token in ("ReleaseMsgSprite(m_iceEffect)", "ReleaseMsgSprite(m_curseEffect)",
              "ReleaseMsgSprite(m_sleepEffect)", "ReleaseMsgSprite(m_stunEffect)",
              "ReleaseMsgSprite(m_silenceEffect)", "ReleaseMsgSprite(m_angelusEffect)"):
    if token not in current:
        raise AssertionError(f"missing release operation: {token}")
if "SetState(10)" not in current or "SetState(m_oldstateId)" not in current:
    raise AssertionError("sleep state entry/restore behavior is missing")
if "case 107:" not in current or "case 107:" not in reference:
    raise AssertionError("ice release alias 107 is missing")

for token in ("m_willBeDead = 0", "m_vanishTime = 0", "m_isLieOnGround = 0",
              "m_isMotionFinished = 0", "m_isMotionFreezed = 0", "m_attackMotion = -1.0f",
              "m_shouldAddPickInfo = 1", "SetState(0)", "InvalidateBillboard()"):
    if token not in local_resurrection:
        raise AssertionError(f"local resurrection receiver case98 missing {token}")
for token in ("Trace(aAmResurrection)", "m_stateId = 0", "m_isLieOnGround = 0",
              "m_shouldAddPickInfo = 1", "m_waitingUseItemAck = 0", "SetState(this, 0)"):
    if token not in ref_resurrection:
        raise AssertionError(f"Ref resurrection receiver case98 missing {token}")
if "m_waitingUseItemAck" in local_resurrection:
    raise AssertionError("local packet layer, not actor message case98, owns waiting-item-ack reset")
if "m_willBeDead = 0" not in local_resurrection or "m_attackMotion = -1.0f" not in local_resurrection:
    raise AssertionError("local resurrection case98 extension resets are undocumented")

# The older executable reconstruction models these sprites as CBlowEffect and
# CNumEffect rather than the newer CMsgEffect. Their existence provides a
# lifecycle provenance check only; it does not establish rendered parity.
for signature in ("CBlowEffect::SendMsg", "CNumEffect::SendMsg"):
    if signature not in ref_npc:
        raise AssertionError(f"Ref/Npc.cpp lacks legacy effect handler {signature}")

print("Status and resurrection message98 receiver comparison passed (actor lifecycle only; rendered visual parity not established).")
