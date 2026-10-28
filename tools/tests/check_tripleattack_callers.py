"""Compare TripleAttack2/3 emitters and callers against Ref and canonical PE."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(ok: bool, message: str) -> None:
    if not ok: raise SystemExit(message)


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
ref_calls = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
pe_onprocess = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
head = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signatures = (
    "void __thiscall CRagEffect::TRIPLEATTACK2(CRagEffect *this)",
    "void __thiscall CRagEffect::TRIPLEATTACK3(CRagEffect *this)",
)
for sig in signatures:
    require(compact(function(ref, sig)) == compact(function(pe, sig)), f"Ref/PE helper mismatch: {sig}")
require("TRIPLEATTACK2(this" not in ref_calls and "TRIPLEATTACK3(this" not in ref_calls,
        "Ref caller omission changed; review source evidence")

triple_sig = "void __thiscall CRagEffect::TRIPLEATTACK(CRagEffect *this)"
require(compact(function(ref, triple_sig)) == compact(function(pe, triple_sig)),
        "Ref/PE effect329 TRIPLEATTACK helper mismatch")

for effect, name, variant, last_state in ((388, "TRIPLEATTACK2", 2, 3), (393, "TRIPLEATTACK3", 3, 8)):
    require(f"case {effect}: return ProcessTripleAttackEffects();" in dispatch, f"route {effect} missing")
    start = pe_onprocess.index(f"case {effect}:")
    end = pe_onprocess.index(f"case {effect + 1}:", start)
    branch = pe_onprocess[start:end]
    call = f"CRagEffect::{name}(this);"
    require(call in branch, f"PE helper call missing for {effect}")
    require("m_stateCnt == 30" in branch and branch.index("PlayWave(") < branch.index(call),
            f"PE state-30 wave/call ordering mismatch for {effect}")

require("bool CRagEffect::ProcessTripleAttackEffects()" in src and "SpawnTripleAttack(int variant)" in head,
        "local implementation/declaration missing")
dispatch2 = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
require("case 329: return ProcessTripleAttackEffects();" in dispatch2, "effect329 route missing")
pe_onprocess2 = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
ref_onprocess2 = function(ref_calls, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
for source, label in ((pe_onprocess2, "PE"), (ref_onprocess2, "Ref")):
    start = source.index("case 0x149:")
    end = source.index("case 0x14A:", start)
    require("CRagEffect::TRIPLEATTACK(this);" in source[start:end] and "return 1;" in source[start:end],
            f"{label} effect329 case0x149 caller changed")
spawn = function(src, "void CRagEffect::SpawnTripleAttack(int variant)")
for token in (
    "m_stateCnt > lastState", "lastState = variant == 1 ? 1 : (variant == 2 ? 3 : 8)", "PP_TRIPLEATTACK",
    '"effect\\\\alpha_center.tga"', "prim->m_deltaPos = m_deltaPos", "gi.process = variant == 2 ? -45 - rand() % 20 : -30 - rand() % 110",
    "rand() % 100", "gi.maxHeight = 0.050000001f", "rand() % 11",
    "static_cast<double>(radialRandom) * 0.60000002 + planarDistance",
    "static_cast<double>(radialRandom) * 0.30000001 + planarDistance",
    "gi.height[4] = static_cast<float>(variant)",
): require(token in spawn, f"emitter anchor missing: {token}")
for token in (
    "variant == 1 ? 1", 'variant == 1 ? "effect\\\\cloud11.tga"',
    "variant == 1) {", "gi.process = -(rand() % 20)",
    "* 0.059999999 - 3.0", "gi.maxHeight = 0.25f",
    "* 0.039999999 + 8.0", "static_cast<double>(rand() % 100) * 0.2 + 20.0",
): require(token in spawn, f"effect329 emitter anchor missing: {token}")
proc = function(src, "bool CRagEffect::ProcessTripleAttackEffects()")
for token in ('"effect\\\\"', "m_stateCnt == 30", "SpawnTripleAttack(m_type == 388 ? 2 : 3)"):
    require(token in proc, f"caller behavior missing: {token}")
for token in ('m_type == 329', 'm_stateCnt == 10 || m_stateCnt == 20 || m_stateCnt == 30',
              '"effect\\\\EF_hit4.wav"', '"effect\\\\EF_hit2.wav"',
              "SpawnTripleAttack(1)"):
    require(token in proc, f"effect329 caller behavior missing: {token}")

image = ExeImage()
for address, expected in ((0x7192FC, b"effect\\alpha_center.tga"), (0x6F1FF0, b"effect\\")):
    value = image.read_cstring_at(address)
    require(value is not None and value.raw == expected, f"PE resource mismatch at {address:#x}")
require('aEffect_51[] = "effect\\\\";' in pe and 'aEffect_57[] = "effect\\\\";' in pe,
        "wave symbol bytes differ from exact effect\\ prefix")
for address, expected in ((0x7194E0, "effect\\cloud11.tga"),
                          (0x719BC8, "effect\\EF_hit2.wav"),
                          (0x71A2F4, "effect\\EF_hit4.wav")):
    value = image.read_cstring_at(address)
    require(value is not None and value.text == expected, f"PE effect329 resource mismatch at {address:#x}")

print("TripleAttack caller parity: effect329 and effects388/393 helpers, routes, states, RNG, exact assets and waves passed")
