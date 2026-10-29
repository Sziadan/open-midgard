#!/usr/bin/env python3
"""Compare effects436/437 HalfSphere and AttackEnergy callers and origin refresh."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_CALLS = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
TEST = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8", errors="replace")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


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


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


for name, sig in (
    ("HalfSphere", "void __thiscall CRagEffect::HalfSphere(CRagEffect *this, char *tName)"),
    ("AttackEnergy", "void __thiscall CRagEffect::AttackEnergy(CRagEffect *this, char *tName)"),
):
    pe_helper, ref_helper = function(PE, sig), function(REF, sig)
    require(compact(pe_helper) == compact(ref_helper), f"PE/primary-Ref {name} helper bodies diverged")
    require(pe_helper.index("this->m_pos = this->m_master->m_pos;") < pe_helper.index("if ( !this->m_stateCnt )"),
            f"{name} no longer refreshes parent origin before the state-zero gate")

for token in (
    "case 436:\n            CRagEffect::HalfSphere(this, aEffectWhite02B);",
    "case 437:\n            CRagEffect::HalfSphere(this, aEffectWhite02B);\n            CRagEffect::AttackEnergy(this, aEffectRingBlue);",
):
    require(token in PE, f"PE effect caller missing: {token.splitlines()[0]}")
require("CRagEffect::HalfSphere(this" not in REF_CALLS and "CRagEffect::AttackEnergy(this" not in REF_CALLS,
        "primary Ref unexpectedly gained inline effect436/437 caller")

for effect, route in (
    (436, 'case 436: ApplyHalfSphere("effect\\\\white02.bmp"); return true;'),
    (437, 'case 437: ApplyHalfSphere("effect\\\\white02.bmp"); ApplyAttackEnergy("effect\\\\ring_blue.tga"); return true;'),
):
    require(route in DISPATCH, f"local effect{effect} route changed")

symbols = SymbolTable(ExeImage())
for name, expected in (("aEffectWhite02B", b"effect\\white02.bmp"),
                       ("aEffectRingBlue", b"effect\\ring_blue.tga")):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected, f"PE asset bytes changed: {name}")

for signature, tokens in (
    ("void CRagEffect::ApplyHalfSphere(const char* textureName)",
     ("ResolveCurrentMaster()", "m_effectPos = origin", "m_cachedPos = origin", "m_stateCnt != 0",
      "kPrimTypeHalfSphere", "prim->m_renderFlag = 513", "gi.fullDisplayAngle = 180")),
    ("void CRagEffect::ApplyAttackEnergy(const char* textureName)",
     ("ResolveCurrentMaster()", "m_effectPos = origin", "m_cachedPos = origin", "m_stateCnt != 0",
      "kPrimTypeAttackEnergy", "prim->m_renderFlag = 513", "const short rises[4] = { 0, 45, 90, 135 }")),
):
    body = function(LOCAL, signature)
    require(body.index("m_effectPos = origin") < body.index("m_stateCnt != 0"),
            f"{signature} origin refresh moved after state gate")
    for token in tokens:
        require(token in body, f"{signature} missing {token}")

require('effect436 HalfSphere refreshes origin and emits source GI/resource profile' in TEST
        and 'effect436 HalfSphere refreshes moved master after spawn gate' in TEST
        and 'effect437 AttackEnergy refreshes origin and emits all four source GI bands' in TEST
        and 'effect437 AttackEnergy refreshes moved master after spawn gate' in TEST,
        "effects436/437 lifecycle coverage missing")
print("effects436/437 HalfSphere/AttackEnergy parity passed: helper bodies, local routes, assets and parent refresh")
