#!/usr/bin/env python3
"""Bind message88 recovery numbers to the local consumer and PE/Ref provenance."""
import re
import sys
from pathlib import Path

current_path, ref_actor_path, ref_helper_path, pe_path, recovery_fixture_path = map(Path, sys.argv[1:6])
current = current_path.read_text(encoding="utf-8", errors="replace")
ref_actor = ref_actor_path.read_text(encoding="utf-8", errors="replace")
ref_helper = ref_helper_path.read_text(encoding="utf-8", errors="replace")
pe = pe_path.read_text(encoding="utf-8", errors="replace")
recovery_fixture = recovery_fixture_path.read_text(encoding="utf-8", errors="replace")


def body(source: str, start: str, end: str) -> str:
    first = source.find(start)
    if first < 0:
        raise AssertionError(f"missing source marker: {start}")
    last = source.find(end, first + len(start))
    if last < 0:
        raise AssertionError(f"missing end marker {end} after {start}")
    return source[first:last]


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


local_sendmsg_start = current.find("void CGameActor::SendMsg(")
if local_sendmsg_start < 0:
    raise AssertionError("local CGameActor::SendMsg receiver is missing")
local_sendmsg = current[local_sendmsg_start:]
local_case = body(local_sendmsg, "    case 88: {", "    case 89:")
for token in (
    "CGameActor* sourceActor = dynamic_cast<CGameActor*>(src)",
    "MakeDamageNumber(*this, sourceActor, static_cast<int>(par1),",
    "static_cast<u32>(par2), static_cast<int>(par3)",
):
    if token not in local_case:
        raise AssertionError(f"local message88 receiver dropped/reordered token: {token}")

make_number = body(current, "void MakeDamageNumber(CGameActor& actor", "bool FindActivePathSegment(")
for token in (
    "ClampDamageNumber(value)",
    "if (kind == 22)",
    "actor.DeleteTotalNumber(kind)",
    "SpawnDamageNumberEffect(actor, sourceActor, clampedValue, colorArgb, kind)",
):
    if token not in make_number:
        raise AssertionError(f"local MakeDamageNumber missing consumer operation: {token}")

spawn_number = body(current, "CMsgEffect* SpawnDamageNumberEffect(", "void SpawnMissEffect(")
for token in (
    "effect->m_colorArgb = colorArgb",
    "effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), kind, 0)",
    "64,\n        reinterpret_cast<msgparam_t>(&actor.m_pos),\n        value,",
    "effect->SendMsg(&actor, 50, 0, 0, 0)",
    "g_world.m_gameObjectList.push_back(effect)",
    "actor.m_msgEffectList.push_back(effect)",
):
    if token not in spawn_number:
        raise AssertionError(f"local damage-number spawn sink missing operation: {token}")

local_clamp = body(current, "int ClampDamageNumber(int value)", "int CountDamageDigits(")
for token in ("if (value < 0)", "return 0", "if (value > 999999)", "return 999999"):
    if token not in local_clamp:
        raise AssertionError(f"local damage-number clamp missing {token}")

ref_sendmsg_start = ref_actor.find("void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
if ref_sendmsg_start < 0:
    raise AssertionError("Ref CGameActor::SendMsg receiver is missing")
ref_sendmsg = ref_actor[ref_sendmsg_start:]
ref_case = body(ref_sendmsg, "    case 88:", "    case 89:")
pe_sendmsg_start = pe.find("void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{")
if pe_sendmsg_start < 0:
    raise AssertionError("canonical PE CGameActor::SendMsg receiver is missing")
pe_sendmsg = pe[pe_sendmsg_start:]
pe_case = body(pe_sendmsg, "    case 88:", "    case 89:")
for label, case in (("Ref", ref_case), ("PE", pe_case)):
    if "CGameActor::Am_Make_Number(this" not in case:
        raise AssertionError(f"{label} message88 does not route to Am_Make_Number")
    if not re.search(r"arg1\s*,\s*LODWORD\(arg2\)\s*,\s*arg3", case):
        raise AssertionError(f"{label} message88 argument order differs")

number_signature = "void __thiscall CGameActor::Am_Make_Number(\n        CGameActor *this,"
ref_number = body(ref_helper, number_signature, "void __thiscall CGameActor::Am_Skill(")
ref_number_flat = compact(ref_number)
for token in map(compact, (
    "if (arg1 <= 999999)", "if (arg1 < 0)", "v6 = 0", "v6 = 999999",
    "eachNum[0] = v6 % 10", "if (arg3 == 22)",
    "v10->m_sprArgb = arg2", "CGameObject::SendMsg(v10, 0, 64",
)):
    if token not in ref_number_flat:
        raise AssertionError(f"PE/Ref Am_Make_Number provenance missing {token}")

pe_number = body(pe, number_signature, "\n// 56855B:")
pe_number_start = pe.find(number_signature)
if pe_number_start < 0 or "00568430" not in pe[max(0, pe_number_start - 180):pe_number_start]:
    raise AssertionError("canonical PE Am_Make_Number anchor 0x568430 is missing")
ref_number_start = ref_helper.find(number_signature)
if ref_number_start < 0 or "0x568430" not in ref_helper[max(0, ref_number_start - 220):ref_number_start]:
    raise AssertionError("Ref Am_Make_Number executable anchor 0x568430 is missing")
pe_number_flat = compact(pe_number)
for token in map(compact, (
    "if (arg1 <= 999999)", "if (arg1 < 0)", "eachNum[0] = v6 % 10",
    "if (arg3 == 22)", "v10->m_sprArgb = arg2", "CGameObject::SendMsg(v10, 0, 64",
)):
    if token not in pe_number_flat:
        raise AssertionError(f"canonical PE Am_Make_Number provenance missing {token}")

for token in (
    "kRecoveryNumberKind=114", "kRecoveryHpNumberColor=0xFF00FF00u",
    "kRecoverySpNumberColor=0xFF0000FFu", "player.messages[0].a==12",
    "player.messages[0].a==9", "player.messages[0].c==114",
):
    if compact(token) not in compact(recovery_fixture):
        raise AssertionError(f"recovery packet fixture does not bind message88 recovery tuple: {token}")

print("Recovery message88 receiver provenance passed: local clamp/consumer bindings, packet tuples, and PE/Ref dispatch/helper anchors; digit/font rendering excluded.")
