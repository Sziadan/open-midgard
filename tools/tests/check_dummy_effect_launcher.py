#!/usr/bin/env python3
"""Compare PlaceEffect and its dummy launcher lifecycle to the canonical EXE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
MODE = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


ref_place = body(REF, "void __thiscall CGameMode::PlaceEffect(CGameMode *this, vector3d *pos, int effectId, int lastTime, int F1)\n{")
exe_place = body(EXE, "void __thiscall CGameMode::PlaceEffect(CGameMode *this, vector3d *pos, int effectId, int lastTime, int F1)\n{")
assert re.sub(r"\s+", " ", ref_place) == re.sub(r"\s+", " ", exe_place), "Ref and executable PlaceEffect bodies differ"
print("PASS Ref/executable PlaceEffect body equality")

assert re.search(r"operator new\(0x11Cu\)[\s\S]*?CAbleToMakeEffect::CAbleToMakeEffect\(v6\)[\s\S]*?CDummyEffectLauncher::`vftable'[\s\S]*?SendMsg\)[\s\S]*?m_world->m_gameObjectList", exe_place)
assert re.search(r"new CDummyEffectLauncher\(\);\s*launcher->Initialize\(pos, effectId, lastTime, extra\);\s*m_world->m_gameObjectList\.push_back\(launcher\);", MODE)
print("PASS launcher construction, init-before-enqueue order, and world ownership")

exe_process = body(EXE, "bool __thiscall CDummyEffectLauncher::OnProcess(CDummyEffectLauncher *this)\n{")
impl_process = body(ACTOR, "u8 CDummyEffectLauncher::OnProcess()\n{")
assert re.search(r"\(timeGetTime\(\) - this->m_stateStartTick\) \* 0\.041666668 <= \(double\)this->m_lastCnt", exe_process)
assert re.search(r"static_cast<double>\(timeGetTime\(\) - m_stateStartTick\) \* 0\.041666668;\s*return elapsedFrames <= static_cast<double>\(m_lastCnt\);", impl_process)
print("PASS elapsed-milliseconds to 24 Hz frame lifetime and inclusive expiry check")

exe_send = body(EXE, "void __thiscall CDummyEffectLauncher::SendMsg(\n        CDummyEffectLauncher *this,")
impl_init = body(ACTOR, "void CDummyEffectLauncher::Initialize(const vector3d& pos, int effectId, int lastTime, int extra)\n{")
impl_send = body(ACTOR, "void CDummyEffectLauncher::SendMsg(CGameObject* sender, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
for pattern in (
    r"if \( message == 14 \)",
    r"this->m_pos = \*arg1;\s*this->m_isVisible = 0;[\s\S]*?this->m_stateStartTick = timeGetTime\(\);",
    r"LaunchEffect\(&this->CAbleToMakeEffect, arg2, v9, 0\.0\)",
    r"if \( arg3 \)\s*\{\s*this->m_lastCnt = arg3;",
    r"else\s*\{\s*this->m_lastCnt = 300;",
    r"if \( arg3 == 333 \)[\s\S]*?pos\.x = \(float\)arg4;[\s\S]*?SendMsg\(v8, 0, 14, \(int\)&pos, 0, 0\)",
    r"CAbleToMakeEffect::SendMsg\(&this->CAbleToMakeEffect, sender, message",
):
    assert re.search(pattern, exe_send), f"canonical launcher branch missing: {pattern}"
for pattern in (
    r"m_pos = pos;\s*m_isVisible = 0;\s*m_stateStartTick = timeGetTime\(\);",
    r"CRagEffect\* effect = LaunchEffect\(effectId, zero, 0\.0f\);",
    r"m_lastCnt = lastTime != 0 \? lastTime : 300;",
    r"if \(lastTime == 333 && effect\)[\s\S]*?effect->SendMsg\(effect, 14, reinterpret_cast<msgparam_t>\(&offset\), 0, 0\);",
):
    assert re.search(pattern, impl_init), f"implementation launcher branch missing: {pattern}"
assert re.search(r"if \(msg == 14\)[\s\S]*?Initialize\(\*pos, static_cast<int>\(par2\), static_cast<int>\(par3\), 0\);[\s\S]*?CAbleToMakeEffect::SendMsg\(sender, msg, par1, par2, par3\);", impl_send)
print("PASS message-14 state, default/explicit lifetime, special 333 offset, and base forwarding")
