"""Check NpcStop helper, canonical callers, resources, and generated routes."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch_main = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
dispatch3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")

ref_helper = function(ref, "void __thiscall CRagEffect::NpcStop(CRagEffect *this, char F1)")
pe_helper = function(pe, "void __thiscall CRagEffect::NpcStop(CRagEffect *this, char F1)")
if compact(ref_helper) != compact(pe_helper):
    raise SystemExit("Ref/PE NpcStop helper mismatch")

pe_calls = re.findall(r"CRagEffect::NpcStop\(this,\s*(\d+)\)", pe)
if pe_calls != ["2", "0"]:
    raise SystemExit(f"canonical NpcStop callers changed: {pe_calls}")

for text in (local, header):
    if "SpawnNpcStop" not in text or "ProcessNpcStopEffects" not in text:
        raise SystemExit("local NpcStop declarations/implementation missing")
spawn = function(local, "void CRagEffect::SpawnNpcStop(int variant)")
for token in (
    "PP_BOTTOMSPR", "prim->m_renderFlag |= 0x200", "gi.flag[4] = 1",
    "gi.arcBaselineY = m_effectPos.y - (variant == 2 ? 0.0f : 7.0f)",
    '"\\xBE\\xC6\\xC0\\xCC\\xC5\\xDB\\\\cconfine.spr"',
    '"\\xBE\\xC6\\xC0\\xCC\\xC5\\xDB\\\\\\xBD\\xBA\\xC5\\xDC.spr"',
    "prim->m_pattern = 2048", "prim->m_repeatAnim = false", "InitReferenceSegments(prim)",
):
    if token not in spawn:
        raise SystemExit(f"NpcStop setup anchor missing: {token}")
if "case 453: return ProcessNpcStopEffects();" not in dispatch_main:
    raise SystemExit("effect 453 route missing")
if "case 604: return ProcessNpcStopEffects();" not in dispatch3:
    raise SystemExit("effect 604 route missing")
index = (ROOT / "docs/skill/generated/effect_handler_index.tsv").read_text(encoding="utf-8")
if "OnProcess\tHighPriest.exe.c\t453\tNpcStop\t" not in index or "OnProcess3\tRef/RagEffect.cpp\t604\tNpcStop\t" not in index:
    raise SystemExit("effect handler index lost canonical NpcStop callers")
if re.search(r"\t\d+\t[^\n]*NpcStop", index.replace("OnProcess\tHighPriest.exe.c\t453\tNpcStop", "").replace("OnProcess3\tRef/RagEffect.cpp\t604\tNpcStop", "")):
    raise SystemExit("NpcStop was attributed to a non-caller effect")
print("NpcStop parity: helper, direct callers, variants, resources and routes passed")
