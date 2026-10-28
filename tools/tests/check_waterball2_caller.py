"""Compare effect117 WaterBall2 with canonical PE/primary-Ref helper evidence."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::WaterBall2(CRagEffect *this)"
pe = function(pe_text, signature)
ref = function(ref_text, signature)
if compact(pe) != compact(ref):
    raise SystemExit("canonical PE and primary Ref WaterBall2 helper bodies differ")
pe_main = function(pe_text, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case117 = pe_main[pe_main.index("case 117:"):pe_main.index("case 118:")]
if "WaterBall2(this)" not in case117:
    raise SystemExit("canonical PE effect117 caller no longer invokes WaterBall2")
if "CRagEffect::WaterBall2(this)" in ref_text:
    raise SystemExit("primary Ref OnProcess now invokes WaterBall2; review caller parity")

local = function(local_text, "void CRagEffect::SpawnWaterBall2()")
if "void SpawnWaterBall2();" not in header or "case 117: SpawnWaterBall2(); return true;" not in dispatch:
    raise SystemExit("local effect117 declaration or generated route is missing")
ordered = (
    "if (m_stateCnt != 0)", "PlayWave(", '"effect\\\\wizard_waterball_chulung.wav"',
    "LaunchEffectPrim(PP_3DPARTICLESPLINE", "SetParticleResources(", "m_duration = 40",
    "m_deltaPos2 = {0.0f, -10.0f, 0.0f}", "std::sqrt", "std::atan2", "m_longitude = -prim->m_roty",
    "m_speed =", "m_gravSpeed = 3.5f", "m_gravAccel =", "m_latiSpeed = -2.0f",
    "m_latiAccel =", "m_roll = 0.0f", "MatrixAppendYRotation", "m_deltaPos3 =",
    "m_rotAxis =", "m_size = 0.75f", "m_numSegments = 12", "InitReferenceSegments(prim)",
)
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    if pos < 0:
        raise SystemExit(f"local WaterBall2 payload missing or out of order: {token}")
    cursor = pos + len(token)
if re.search(r"\brand\s*\(", local):
    raise SystemExit("WaterBall2 canonical source has no random draws")

family = function(local_text, "bool CEffectPrim::UsesReferenceParticle() const")
if "PP_3DPARTICLESPLINE && m_master && m_master->GetEffectType() == 117" not in family:
    raise SystemExit("effect117 PP_3DPARTICLESPLINE reference family binding is missing")

symbols = SymbolTable(ExeImage(ROOT / "HighPriest.exe"))
expected = {
    "aEffectWizardWa": (0x71BB18, b"effect\\wizard_waterball_chulung.wav"),
    "byte_71BB54": (0x71BB54, b"\xC0\xCC\xC6\xD1\xC6\xAE\\Waterball.spr"),
    "byte_71BB3C": (0x71BB3C, b"\xC0\xCC\xC6\xD1\xC6\xAE\\Waterball.act"),
}
for name, (address, raw) in expected.items():
    resolved = symbols.resolve_name(name)
    if resolved is None or resolved.va != address or resolved.raw != raw:
        raise SystemExit(f"WaterBall2 PE asset {name} changed: {resolved!r}")
if '"effect\\\\wizard_waterball_chulung.wav"' not in local_text:
    raise SystemExit("local WaterBall2 wave literal differs from the executable")
if '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\Waterball.spr"' not in local_text:
    raise SystemExit("local WaterBall2 sprite literal differs from the executable CP949 bytes")
if '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\Waterball.act"' not in local_text:
    raise SystemExit("local WaterBall2 action literal differs from the executable CP949 bytes")

print("WaterBall2 117 parity: PE/Ref helper, caller, route, spline payload, exact assets and particle family passed")
