"""Compare EnergyDrain2/3 emitters and canonical caller behavior."""
from __future__ import annotations

import re
import sys
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
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
ref_callers = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

for name in ("EnergyDrain2", "EnergyDrain3"):
    signature = f"void __thiscall CRagEffect::{name}(CRagEffect *this)"
    if compact(function(pe, signature)) != compact(function(ref, signature)):
        raise SystemExit(f"Ref/PE helper mismatch: {name}")
if "case 378:" in ref_callers or "case 383:" in ref_callers:
    raise SystemExit("Ref caller omission changed; review canonical-only caller evidence")

main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
branch378 = main[main.index("case 378:"):main.index("case 379:")]
branch383 = main[main.index("case 383:"):main.index("case 384:")]
for token in ("EnergyDrain2(this)", "v108 >= 55", "v108 <= 65",
              "m_BodyLight & 1", "SetArgb(*p_m_master, -1, 100, 100, 255)"):
    if token not in branch378: raise SystemExit(f"effect378 branch missing {token}")
for token in ("EnergyDrain3(this)", "v109 >= 50", "v109 <= 80",
              "m_BodyLight & 8", "2 * (125 - this->m_stateCnt)", "250"):
    if token not in branch383: raise SystemExit(f"effect383 branch missing {token}")
if "case 378: return ProcessEnergyDrain2Effect();" not in dispatch:
    raise SystemExit("effect378 route missing")
if "case 383: return ProcessEnergyDrain3Effect();" not in dispatch:
    raise SystemExit("effect383 route missing")

sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver
exe = resolver.ExeImage()
if exe.read_cstring_at(0x71B200).raw != b"\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.spr":
    raise SystemExit("particle1 SPR bytes changed")
if exe.read_cstring_at(0x71B1E8).raw != b"\xC0\xCC\xC6\xD1\xC6\xAE\\particle1.act":
    raise SystemExit("particle1 ACT bytes changed")

for name, duration in (("EnergyDrain2", 60), ("EnergyDrain3", 60)):
    signature = f"void CRagEffect::Spawn{name}()"
    declaration = f"void Spawn{name}();"
    if declaration not in header: raise SystemExit(f"declaration missing: {name}")
    body = function(local, signature)
    for token in ("PP_3DPARTICLESPLINE", f"m_duration = {duration}", "roll < 180",
                  "MatrixAppendYRotation", "m_deltaPos3", "m_rotAxis", "m_numSegments = 7",
                  "InitReferenceSegments(prim)", "particle1.spr", "particle1.act"):
        if token not in body: raise SystemExit(f"{name} payload anchor missing: {token}")

energy2 = function(local, "void CRagEffect::SpawnEnergyDrain2()")
for token in ("m_deltaPos.x - m_effectPos.x", "m_deltaPos.z - m_effectPos.z", "-10.0f",
              "m_effectPos.x + m_effectPos.x - m_deltaPos.x", "m_effectPos.z + m_effectPos.z - m_deltaPos.z",
              "roll = -90", "roll += 90", "m_red = rand() % 81 + 160",
              "m_green = rand() % 81 + 160", "rand() % 6", "+ 1.5"):
    if token not in energy2: raise SystemExit(f"EnergyDrain2 setup anchor missing: {token}")
if energy2.count("rand() %") != 3:
    raise SystemExit("EnergyDrain2 must consume three RNG draws per primitive")

energy3 = function(local, "void CRagEffect::SpawnEnergyDrain3()")
for token in ("m_stateCnt > 4", "m_deltaPos2.x = 0.0f", "m_pos.x = -10.0f", "m_pos.y = 0.0f",
              "const int offsetZ = rand() % 7", "const int offsetX = rand() % 7",
              "m_red = rand() % 81 + 160", "m_green = 255", "m_blue = rand() % 81 + 160",
              "rand() % 6", "+ 1.0"):
    if token not in energy3: raise SystemExit(f"EnergyDrain3 setup anchor missing: {token}")
if energy3.count("rand() %") != 5:
    raise SystemExit("EnergyDrain3 must consume five RNG draws per primitive")

process2 = function(local, "bool CRagEffect::ProcessEnergyDrain2Effect()")
process3 = function(local, "bool CRagEffect::ProcessEnergyDrain3Effect()")
for token in ("SpawnEnergyDrain2()", "m_stateCnt >= 55", "m_stateCnt <= 65", "m_BodyLight & 1",
              "SetArgb(-1, 100, 100, 255)"):
    if token not in process2: raise SystemExit(f"effect378 local caller missing: {token}")
for token in ("SpawnEnergyDrain3()", "m_stateCnt >= 50", "m_stateCnt <= 80", "m_BodyLight & 8",
              "2 * (125 - m_stateCnt)", "SetArgb(-1, color, color, 250)"):
    if token not in process3: raise SystemExit(f"effect383 local caller missing: {token}")
print("EnergyDrain2/3 parity: Ref/PE helpers, PE caller behavior, resource bytes, GI state, RNG counts and routes passed")
