"""Compare SoulStrike helper and verify both effect15/451 routes and payload."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver


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


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::SoulStrike(CRagEffect *this, int F1)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE SoulStrike helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
for effect, variant in ((15, 0), (451, 1)):
    case = main[main.index(f"case {effect}:"):main.index(f"case {effect + 1}:")]
    if f"SoulStrike(this, {variant})" not in case:
        raise SystemExit(f"canonical effect{effect} caller changed")
if "SoulStrike(this" in ref:
    raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessSoulStrike15(int variant);" not in header:
    raise SystemExit("SoulStrike handler declaration missing")
for effect, variant in ((15, 0), (451, 1)):
    if f"case {effect}: return ProcessSoulStrike15({variant});" not in dispatch:
        raise SystemExit(f"effect{effect} route missing")

handler = function(local, "bool CRagEffect::ProcessSoulStrike15(int variant)")
for token in ("ProjectReferenceEffectVertex", "m_stateCnt + 5", "tick % 11", "tick / 11 > end",
              '"effect\\\\EF_SoulStrike.wav"', "PP_3DPARTICLESPLINE", "m_duration = 40",
              "m_deltaPos2 = {0.0f, -10.0f, 0.0f}", "std::atan2(dx, -dz)", "m_gravSpeed = 3.5f",
              "m_latiSpeed = -2.0f", "m_roll = static_cast<float>(roll)", "m_size = 2.75f",
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle5.spr"',
              '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\particle1.spr"',
              "m_numSegments = 12", "InitReferenceSegments(prim)"):
    if token not in handler: raise SystemExit(f"SoulStrike payload missing {token}")
if "rand()" in handler:
    raise SystemExit("SoulStrike source helper has no RNG draws")
if handler.index("m_stateCnt + 5") > handler.index('"effect\\\\EF_SoulStrike.wav"'):
    raise SystemExit("SoulStrike sound/timing order changed")

image = resolver.ExeImage()
resources = [image.read_cstring_at(address).raw for address in (0x71B200, 0x71B1E8, 0x71B364, 0x71B34C)]
expected = [b"\xc0\xcc\xc6\xd1\xc6\xae\\particle1.spr", b"\xc0\xcc\xc6\xd1\xc6\xae\\particle1.act",
            b"\xc0\xcc\xc6\xd1\xc6\xae\\particle5.spr", b"\xc0\xcc\xc6\xd1\xc6\xae\\particle5.act"]
if resources != expected:
    raise SystemExit(f"SoulStrike sprite/action bytes changed: {resources!r}")
print("SoulStrike15/451 parity: Ref/PE helper, callers, routes, cadence, no RNG, spline payload and exact assets passed")
