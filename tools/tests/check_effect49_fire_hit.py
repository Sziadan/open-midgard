"""Compare effect49 inline FireHit PE branch with Ref/local handler."""
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


process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case49 = process[process.index("case 49:") : process.index("case 50:")]
ordered_pe = (
    "if ( !this->m_stateCnt )", "&CModeMgr::GetGameMode(&g_modeMgr)->m_world->m_player->m_pos",
    "this->m_pos.z - v140->z", "this->m_pos.x - v140->x",
    "CSession::GetAttrWaveName(&g_session, 3)", "PlayWave(AttrWaveName, xcu, 0.0, zcu, 250, 40, 1.0)",
    "goto LABEL_424;",
)
cursor = 0
for token in ordered_pe:
    found = case49.find(token, cursor)
    if found < 0:
        raise SystemExit(f"canonical PE effect49 branch order missing: {token}")
    cursor = found + len(token)
if "result = CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0);" not in process:
    raise SystemExit("canonical PE effect49 shared STR tail/offset missing")

ref = body(REF, "bool __thiscall CRagEffect::FireHit(CRagEffect *this)")
local = body(SRC, "bool CRagEffect::ProcessFireHit()")
ref_order = (
    "if ( !this->m_stateCnt )", "CSession::GetAttrWaveName(&g_session, 3)",
    "PlayWave(AttrWaveName, x, 0.0, z, 250, 40, 1.0)",
    "return CRagEffect::ProcessEZ2STR(this, 0.0, -80.0, 0.0) != 0",
)
local_order = (
    "if (m_stateCnt == 0)", "g_session.GetAttrWaveName(3)", "PlayWave(wave,x,0.0f,z,250,40,1.0f)",
    "return ProcessEZ2STR(0.0f,-80.0f,0.0f) != 0",
)
for source_name, handler, ordered in (("primary Ref", ref, ref_order), ("local", local, local_order)):
    cursor = 0
    for token in ordered:
        compact = re.sub(r"\s+", "", handler)
        compact_token = re.sub(r"\s+", "", token)
        found = compact.find(compact_token, cursor)
        if found < 0:
            raise SystemExit(f"{source_name} FireHit operation order missing: {token}")
        cursor = found + len(compact_token)
if "case 49: return ProcessFireHit();" not in DISPATCH:
    raise SystemExit("local generated effect49 route is missing")
if "m_type == 49" not in body(SRC, "bool CRagEffect::ProcessEffectFrameLegacy()"):
    raise SystemExit("local FireHit handler no longer participates in legacy processing")

for phrase in (
    "FireHit frame zero selects sound with one draw before propagating missing STR despite reduced effects",
    "FireHit uses original first fire attribute wave and plays after selector RNG",
    "FireHit uses stored-origin horizontal listener deltas and original wave parameters",
    "FireHit missing STR leaves screen fields unchanged without an extra projection",
    "FireHit later missing STR returns false without repeating selector RNG or audio",
    "FireHit normal mode selects one sound and continues active STR on zero frame",
    "FireHit production selector returns exact second fire wave for odd draw",
    "FireHit active STR uses source minus-eighty vertical offset at actor projection",
    "FireHit later active STR avoids repeated audio and RNG",
    "FireHit STR follows moving actor while preserving stored effect origin and parent lifetime",
    "FireHit propagates exhausted STR without extra audio or RNG",
    "FireHit helper adds no primitives or extra resource loads",
    "FireHit preserves original effect-level no-frame-time gate exception",
):
    if phrase not in FIXTURE:
        raise SystemExit(f"FireHit lifecycle fixture missing {phrase!r}")

print("effect49 FireHit inline PE branch, Ref/local handler, route and lifecycle coverage passed")
