#!/usr/bin/env python3
"""Source-to-current branch audit for CGameActor::SetAttrState."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
SESSION_REF = (ROOT / "Ref/Session.cpp").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActorMsgHandler.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"function not found: {marker}"
    opening = text.find("{", start)
    semicolon = text.find(";", start)
    assert opening >= 0 and (semicolon < 0 or opening < semicolon), f"definition not found: {marker}"
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def switch_labels(text: str, pattern: str) -> list[str]:
    match = re.search(pattern, text)
    assert match, f"switch missing: {pattern}"
    start = match.end()
    depth = 1
    result: list[str] = []
    for line in re.finditer(r"[^\n]*(?:\n|$)", text[start:]):
        value = line.group()
        if depth == 1:
            label = re.match(r"\s*(case\s+[^:]+|default)\s*:", value)
            if label:
                result.append(label.group(1).strip())
        depth += value.count("{") - value.count("}")
        if depth == 0:
            return result
    raise AssertionError("unclosed switch")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
marker = "void __thiscall CGameActor::SetAttrState("
ref_sig, ref = body(REF, marker)
exe_sig, exe = body(EXE, marker)
assert normalize(ref_sig) == normalize(exe_sig), "SetAttrState Ref/EXE signatures differ"
assert normalize(ref) == normalize(exe), "complete SetAttrState Ref/EXE bodies differ"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref))} normalized characters)")

old_states = ["case 1", "case 2", "case 3", "case 4", "default"]
new_states = ["case 1", "case 2", "case 3", "case 4", "case 6", "default"]
assert switch_labels(ref, r"switch\s*\(\s*oldBodyState\s*\)\s*\{") == old_states
assert switch_labels(exe, r"switch\s*\(\s*oldBodyState\s*\)\s*\{") == old_states
assert switch_labels(ref, r"switch\s*\(\s*this->m_bodyState\s*\)\s*\{") == new_states
assert switch_labels(exe, r"switch\s*\(\s*this->m_bodyState\s*\)\s*\{") == new_states
print(f"PASS mechanically extracted ordered body-state labels: old={old_states}, new={new_states}")

session_marker = "BOOL __thiscall CSession::IsMasterAid(CSession *this, int showLevel)"
master_ref = body(SESSION_REF, session_marker)[1]
master_exe = body(EXE, session_marker)[1]
assert normalize(master_ref) == normalize(master_exe), "IsMasterAid Ref/EXE bodies differ"
print("PASS Ref/EXE IsMasterAid dependency equality")

delete_marker = "void __thiscall CGameActor::Delete99LevelEffects(CGameActor *this)"
delete_ref = body(REF, delete_marker)[1]
delete_exe = body(EXE, delete_marker)[1]
assert normalize(delete_ref) == normalize(delete_exe), "Delete99LevelEffects Ref/EXE bodies differ"
assert len(re.findall(r"CGameObject::SendMsg\(this,\s*0,\s*109,\s*(?:200|397|398|362|201|202)", delete_ref)) == 7 \
    and "v2->CGameObject::SendMsg)(0, 109, 200" in delete_ref, \
    "source 99-effect cleanup message inventory changed"
print("PASS Ref/EXE 99-level cleanup helper equality and source message inventory")

impl_marker = "void CGameActor::SetAttrState(int bodyState, int effectState, int healthState, bool changedNow)"
_, current = body(IMPL, impl_marker)
required = {
    "old state capture and incoming state updates": r"const int oldEffectState\s*=\s*m_effectState[\s\S]*?const int oldBodyState\s*=\s*m_bodyState[\s\S]*?m_bodyState\s*=\s*bodyState[\s\S]*?m_effectState\s*=\s*effectState[\s\S]*?m_healthState\s*=\s*healthState",
    "sight and Ruwach enter sounds": r"BitChanged\(effectState,\s*oldEffectState,\s*kSightBit\)[\s\S]*?PlayStateWave\(\*this,\s*\"effect\\\\EF_Sight\.wav\"\)[\s\S]*?BitChanged\(effectState,\s*oldEffectState,\s*kRuwachBit\)[\s\S]*?PlayStateWave\(\*this,\s*\"effect\\\\EF_Sight\.wav\"\)",
    "99-level deletion resets marker and removes exact source effect sequence": r"delete99LevelEffects\(\);",
    "burrow transition deletes stale aura effects and handles enter/exit": r"if\s*\(BitChanged\(effectState,\s*oldEffectState,\s*kBurrowBit\)\)\s*\{\s*delete99LevelEffects\(\);[\s\S]*?SendMsg\(this,\s*63[\s\S]*?LaunchEffect\(16[\s\S]*?m_isVisibleBody\s*=\s*0[\s\S]*?isMasterAid\(7\)[\s\S]*?SendMsg\(this,\s*109,\s*159[\s\S]*?SendMsg\(this,\s*35[\s\S]*?m_isVisibleBody\s*=\s*1[\s\S]*?SendMsg\(this,\s*51[\s\S]*?SendMsg\(this,\s*101[\s\S]*?m_is99[\s\S]*?SendMsg\(this,\s*126",
    "hiding transition deletes stale aura and respects master visibility exception": r"if\s*\(BitChanged\(effectState,\s*oldEffectState,\s*kHidingBit\)\)\s*\{\s*delete99LevelEffects\(\);[\s\S]*?m_isVisibleBody\s*=\s*0[\s\S]*?m_isVisible\s*=\s*0[\s\S]*?isMasterAid\(7\)[\s\S]*?SendMsg\(this,\s*109,\s*159[\s\S]*?m_isVisibleBody\s*=\s*1[\s\S]*?m_isVisible\s*=\s*1[\s\S]*?m_is99[\s\S]*?SendMsg\(this,\s*126",
    "cart bird and chicken/star effect transitions": r"BitChanged\(effectState,\s*oldEffectState,\s*kCartBits\)[\s\S]*?SendMsg\(this,\s*52[\s\S]*?SendMsg\(this,\s*51[\s\S]*?BitChanged\(effectState,\s*oldEffectState,\s*kBirdBit\)[\s\S]*?101\s*:\s*102[\s\S]*?BitChanged\(effectState,\s*oldEffectState,\s*kChickenBit\)[\s\S]*?105\s*:\s*106",
    "special hiding applies master level 8 exception and effect cleanup": r"if\s*\(BitChanged\(effectState,\s*oldEffectState,\s*kCloakBit\)\)\s*\{\s*delete99LevelEffects\(\);[\s\S]*?isMasterAid\(8\)[\s\S]*?m_isVisibleBody\s*=\s*1[\s\S]*?m_isVisible\s*=\s*1[\s\S]*?m_isVisibleBody\s*=\s*0[\s\S]*?m_isVisible\s*=\s*0[\s\S]*?SendMsg\(this,\s*109,\s*159[\s\S]*?m_is99[\s\S]*?SendMsg\(this,\s*126",
    "old/new body state ordered branches and status messages": r"if\s*\(oldBodyState\s*!=\s*m_bodyState\)[\s\S]*?switch\s*\(oldBodyState\)[\s\S]*?case 1:[\s\S]*?_stone_explosion\.wav[\s\S]*?case 2:[\s\S]*?SendMsg\(this,\s*59[\s\S]*?SendMsg\(this,\s*62[\s\S]*?_frozen_explosion\.wav[\s\S]*?case 3:[\s\S]*?SendMsg\(this,\s*72[\s\S]*?case 4:[\s\S]*?SendMsg\(this,\s*70[\s\S]*?switch\s*\(m_bodyState\)[\s\S]*?case 1:[\s\S]*?TintActor\(\*this,\s*64,\s*64,\s*64\)[\s\S]*?case 2:[\s\S]*?SendMsg\(this,\s*58[\s\S]*?TintActor\(\*this,\s*0,\s*128,\s*255\)[\s\S]*?case 3:[\s\S]*?SendMsg\(this,\s*71[\s\S]*?case 4:[\s\S]*?SendMsg\(this,\s*69[\s\S]*?case 6:[\s\S]*?TintActor\(\*this,\s*128,\s*128,\s*128\)",
    "poison curse silence stun confusion blind effects and messages": r"HasBit\(m_healthState,\s*0x80\)[\s\S]*?LaunchEffect\(335[\s\S]*?SendMsg\(this,\s*109,\s*335[\s\S]*?HasBit\(m_healthState,\s*0x02\)[\s\S]*?SendMsg\(this,\s*67[\s\S]*?SendMsg\(this,\s*68[\s\S]*?HasBit\(m_healthState,\s*0x04\)[\s\S]*?SendMsg\(this,\s*73[\s\S]*?SendMsg\(this,\s*74[\s\S]*?0x20\)[\s\S]*?75\s*:\s*76[\s\S]*?0x08[\s\S]*?_confusion\.wav[\s\S]*?0x10[\s\S]*?LaunchEffect\(334[\s\S]*?SendMsg\(this,\s*109,\s*334",
    "common freeze and tint cleanup": r"if\s*\(m_bodyState\s*==\s*0\)\s*\{\s*m_isMotionFreezed\s*=\s*0[\s\S]*?\(m_healthState\s*&\s*0x83\)\s*==\s*0[\s\S]*?ClearActorTint\(\*this\)",
}
for description, pattern in required.items():
    assert re.search(pattern, current), f"current SetAttrState mismatch: {description}"
print(f"PASS current ordered branch comparison: {len(required)} source-derived behavior groups")

assert re.search(r"PlayWave\(waveName,\s*actor\.m_pos\.x\s*-\s*listener\.x,\s*0\.0f,\s*actor\.m_pos\.z\s*-\s*listener\.z", IMPL), \
    "current status sound position differs from source horizontal-only offset"
assert re.search(r"for\s*\(const int effectId\s*:\s*\{\s*200,\s*397,\s*398,\s*362,\s*362,\s*201,\s*202,\s*202\s*\}\)[\s\S]*?SendMsg\(this,\s*109,\s*effectId", current), \
    "current Delete99LevelEffects effect order/duplicates differ"
assert "m_u28.m_sprColor.u.s.a" not in IMPL[IMPL.find("void TintActor("):IMPL.find("void ClearActorTint(")], \
    "tint setter must preserve existing sprite alpha"
assert "m_u28.m_sprColor.u.s.a" not in IMPL[IMPL.find("void ClearActorTint("):IMPL.find("} // namespace", IMPL.find("void ClearActorTint("))], \
    "tint clear must preserve existing sprite alpha"
assert current.count("isMasterAid(7)") == 2 and current.count("isMasterAid(8)") == 1
assert current.count("SendMsg(this, 126, 0, 0, 0)") == 3, "all three reveal transitions must restore level-99 effect"
assert re.search(r"IsGravityAid\(aid\)\s*&&\s*showLevel\s*==\s*7[\s\S]*?g_serviceType\s*!=\s*ServiceKorea[\s\S]*?aid\s*>\s*0x2C8C83u[\s\S]*?aid\s*==\s*2919558u[\s\S]*?aid\s*==\s*2919555u[\s\S]*?aid\s*==\s*100025u[\s\S]*?aid\s*==\s*234073u", current), \
    "current inline master-aid predicate differs from Ref CSession::IsMasterAid"
assert current.count("delete99LevelEffects();") == 3, "all three source transition edges must clear 99-level effects"
print("PASS source-specific wave position, exact 99-effect cleanup, RGB-only tint, master-aid gates, and three reveal effects")

print("Documented architecture scope: legacy emblem/word windows and the source-only self-hide/blur tail are not part of modern actor/UI presentation")
