"""Compare WarpZone effect61 in PE, primary Ref and the local lifecycle route."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    begin = text.rindex(signature)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin : pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


pe_helper = body(PE, "void __thiscall CRagEffect::WarpZone(CRagEffect *this)")
ref_helper = body(REF, "void __thiscall CRagEffect::WarpZone(CRagEffect *this)")
if compact(pe_helper) != compact(ref_helper):
    raise SystemExit("canonical PE and primary Ref WarpZone helper bodies differ")

pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 61:\s*CRagEffect::WarpZone\(this\);\s*break;", pe_process):
    raise SystemExit("PE effect61 OnProcess route no longer calls WarpZone")
if "case 61: return ProcessEffectFrameLegacy();" not in DISPATCH:
    raise SystemExit("local generated effect61 route is missing")
local_process = body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()")
if not re.search(r"if\s*\(m_type\s*==\s*61\)\s*\{\s*ApplyWarpZone\(\);\s*return\s+true;", local_process):
    raise SystemExit("local effect61 legacy route no longer calls ApplyWarpZone")

local_helper = body(SRC, "void CRagEffect::ApplyWarpZone()")
ordered = (
    "if (m_stateCnt == 145) m_stateCnt = 0;",
    "if (m_stateCnt == 0)",
    "prim->m_duration = 158;",
    "if (m_stateCnt % 28 == 0 && m_stateCnt < 117)",
    'g_texMgr.GetTexture("effect\\\\ring_blue.tga", false)',
    "if (m_stateCnt % 10 == 0)",
    "rand() % 200 / 100",
    "rand() % 360",
    "rand(); // Original discarded draw before fixed angular speed.",
    "rand() % 10 / 10",
)
cursor = 0
for marker in ordered:
    found = local_helper.find(marker, cursor)
    if found < 0:
        raise SystemExit(f"local WarpZone field/order marker changed: {marker}")
    cursor = found + len(marker)

fixture = body(FIXTURE, "int main()")
start = fixture.index('"original WarpZone emits disk then shrinking ring then orbit at zero"')
end = fixture.index("rngValues.clear();rngCursor=0;", start)
warp_fixture = fixture[start:end]
for phrase in (
    "consumes four RNG draws including discarded angular draw and integer-divided size draw",
    "uses exact sprite then action resources",
    "emits only shrinking ring at twenty-eight",
    "emits last shrinking ring at one-twelve",
    "stops shrinking rings after source threshold while orbit cadence continues",
    "resets exactly at one-forty-five before zero-frame emission",
    "does not replace equality reset with greater-than clamp",
):
    if phrase not in warp_fixture:
        raise SystemExit(f"existing WarpZone lifecycle coverage missing {phrase!r}")

print("effect61 PE/Ref helper, local route/order and lifecycle coverage passed")
