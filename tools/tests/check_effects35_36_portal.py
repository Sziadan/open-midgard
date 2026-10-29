"""Compare ReadyPortal and Portal PE/Ref handlers with the local effect routes."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str, start: int | None = None) -> str:
    begin = text.index(signature, start or 0)
    opening = text.index("{", begin)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[begin:pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


def check(label: str, condition: bool) -> None:
    if not condition:
        raise SystemExit(f"FAIL {label}")
    print(f"PASS {label}")


pe_process_sig = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
pe_process = body(PE, pe_process_sig, PE.index(pe_process_sig + "; // idb") + 1)

for effect_id, name, local_name, anchors, fixture_phrases in (
    (35, "ReadyPortal", "ApplyReadyPortal", ("m_stateCnt % 14", "EF_ReadyPortal.wav", "PP_3DCIRCLE", "m_duration = 30", "m_radiusSpeed = 1.0"), (
        "ReadyPortal thirty-five emits one canonical reference circle on zero",
        "ReadyPortal repeats both circle and sound on frame fourteen",
        "ReadyPortal begins source current-alpha fade exactly at frame fifteen",
    )),
    (36, "Portal", "ApplyPortal", ("m_stateCnt % 14", "PP_3DCYLINDER", "m_longSpeed = 4.0", "m_longSpeed = 5.0", "PP_3DCIRCLE", "m_duration = 30"), (
        "original Portal emits two cylinders then one circle at zero",
        "Portal repeats only circle at fourteen without cylinders",
        "Portal preserves recurring cadence with no wave or RNG",
    )),
):
    pe_sig = f"void __thiscall CRagEffect::{name}(CRagEffect *this)"
    pe_start = PE.index(pe_sig + "; // idb") + 1
    pe_handler = body(PE, pe_sig, pe_start)
    ref_handler = body(REF, pe_sig)
    local_handler = body(SRC, f"void CRagEffect::{local_name}()")
    dispatch_case = re.search(rf"case\s+{effect_id}\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH)
    route = re.search(rf"if\s*\(m_type\s*==\s*{effect_id}\)\s*\{{\s*{local_name}\(\);\s*return\s+true;\s*\}}", SRC)
    check(f"effect{effect_id} canonical PE and primary Ref bodies normalize identically",
          re.sub(r"\s+", " ", pe_handler).strip() == re.sub(r"\s+", " ", ref_handler).strip())
    check(f"PE effect{effect_id} OnProcess route calls {name}",
          re.search(rf"case\s+{effect_id}\s*:\s*CRagEffect::{name}\(this\)", pe_process) is not None)
    check(f"local effect{effect_id} generated dispatcher and legacy route reach {local_name}",
          dispatch_case is not None and route is not None)
    check(f"effect{effect_id} local emitter retains source branch anchors",
          all(token in local_handler for token in anchors))
    check(f"effect{effect_id} lifecycle fixture covers cadence and emitted payload",
          all(phrase in FIXTURE for phrase in fixture_phrases))

print("ReadyPortal and Portal PE/Ref source, local routes and lifecycle mapping passed.")
