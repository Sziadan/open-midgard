#!/usr/bin/env python3
"""Gate actor tint lifecycle and billboard color against Ref and canonical EXE."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
REF_ACTOR = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
REF_PC = (ROOT / "Ref/Pc.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
CURRENT_ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
CURRENT_WORLD = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")
TINT_HEADER = (ROOT / "src/world/ActorTint.h").read_text(encoding="utf-8")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"definition missing: {marker}"
    opening = text.find("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
modify_marker = "void __thiscall CRenderObject::ModifyArgb(CRenderObject *this, int a, int r, int g, int b)"
ref_modify_sig, ref_modify = body(REF_ACTOR, modify_marker)
exe_modify_sig, exe_modify = body(EXE, modify_marker)
assert normalize(ref_modify_sig) == normalize(exe_modify_sig) == normalize(modify_marker)
normalized_modify = normalize(ref_modify)
assert normalized_modify == normalize(exe_modify), "complete Ref/EXE ModifyArgb bodies differ"
assert len(normalized_modify) == 653
assert hashlib.sha256(normalized_modify.encode()).hexdigest() == \
    "af491fcaaf4c55b6ebdfe6fdb6af1699ad3e3efccf4ec77963c6a09c1244578c"
assert "if ( !this->m_isSprArgbFixed )" in ref_modify
for field in ("m_sprColor.a", "m_sprColor.r", "m_sprColor.g", "m_sprColor.b"):
    assert field in ref_modify
assert "r + this->m_sprColor.r" in ref_modify
assert "g + this->m_sprColor.g" in ref_modify
assert "b + this->m_sprColor.b" in ref_modify
assert "a + this->m_sprColor.a" in ref_modify
assert "if ( v8 >= 255 )" in ref_modify

pc_marker = "unsigned __int8 __thiscall CPc::OnProcess(CPc *this)"
ref_pc_body = body(REF_PC, pc_marker)[1]
exe_pc_body = body(EXE, pc_marker)[1]
assert normalize(ref_pc_body) == normalize(exe_pc_body), "complete Ref/EXE CPc::OnProcess bodies differ"
assert "this->m_isSingleColor && timeGetTime() > this->m_singleColorStartTick + 100" in ref_pc_body
assert "this->m_colorOfSingleColor = -65536" in ref_pc_body
assert "if ( this->m_sprArgb != -1 )" in ref_pc_body
assert "CRenderObject::ModifyArgb(this, 8, 8, 8, 8)" in ref_pc_body
assert ref_pc_body.index("m_colorOfSingleColor = -65536") < ref_pc_body.index("ModifyArgb(this, 8, 8, 8, 8)")

# Current actor state processing folds the PC OnProcess tint lifecycle into
# ProcessState. Channel advancement is guarded by the union's non-sentinel
# ARGB and ModifyArgb's !fixed condition; timeout uses strict > 100 ms.
_, process_state = body(CURRENT_ACTOR, "u8 CGameActor::ProcessState()")
timeout_guard = "ro::actor::IsSingleColorExpired(m_isSingleColor != 0,"
assert timeout_guard in process_state
timeout_pos = process_state.index(timeout_guard)
assert "m_singleColorStartTick, [] { return timeGetTime(); }" in process_state
assert process_state.index("ProcessWillBeAttacked();") < timeout_pos
assert process_state.index("m_colorOfSingleColor = 0xFFFF0000u;", timeout_pos) > timeout_pos
assert process_state.index("m_isSingleColor = 0;", timeout_pos) > timeout_pos
assert "if (m_u28.m_sprArgb != 0xFFFFFFFFu)" in process_state
assert "ro::actor::AdvanceTint(m_u28.m_sprColor.u.s.a" in process_state

advance_marker = "inline void AdvanceTint(std::uint8_t& alpha"
advance = TINT_HEADER[TINT_HEADER.index(advance_marker):]
assert "if (fixed)" in advance
assert advance.index("if (fixed)") < advance.index("alpha = AdvanceTintChannel")
for channel in ("alpha", "red", "green", "blue"):
    assert f"{channel} = AdvanceTintChannel({channel}, 8)" in advance
assert "(std::max)(0, (std::min)(255, advanced))" in TINT_HEADER
assert "tickProvider() > startTick + 100u" in TINT_HEADER

_, billboard = body(CURRENT_WORLD, "bool RenderCachedBillboard(const CWorld::BillboardScreenEntry& entry)")
assert "ro::actor::ResolveBillboardColor(alpha," in billboard
for token in ("m_u28.m_sprColor.u.s.a", "m_u28.m_sprColor.u.s.r",
              "m_u28.m_sprColor.u.s.g", "m_u28.m_sprColor.u.s.b",
              "m_isSingleColor != 0", "m_colorOfSingleColor"):
    assert token in billboard
assert "verts[index].color = billboardColor" in billboard
resolve_marker = "inline std::uint32_t ResolveBillboardColor("
resolve = TINT_HEADER[TINT_HEADER.index(resolve_marker):]
assert "visibilityAlpha * tintAlpha / 255u" in resolve
assert "singleColor && singleColorSentinel != 0xFFFF0000u" in resolve
assert "? 0x00FF0000u" in resolve
print("PASS Ref/EXE ModifyArgb and CPc tint branches; current strict timeout, +8/clamp/fixed rules, and billboard tint/visibility composition")
