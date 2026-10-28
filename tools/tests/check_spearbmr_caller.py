"""Compare SpearBmr effect 80 between canonical PE, primary Ref and local route."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def body(text: str, start: str, end: str) -> str:
    a = text.index(start)
    b = text.index(end, a + len(start))
    return text[a:b]


ref_body = body(ref, "void __thiscall CRagEffect::SpearBmr(CRagEffect *this)", "void __thiscall CRagEffect::Sphere(")
pe_body = body(exe, "void __thiscall CRagEffect::SpearBmr(CRagEffect *this)\n{", "//----- (005FE8B0)")
local = body(source, "void CRagEffect::SpawnSpearBmr()", "// TEST_BODY_END spear_bmr_handler")

canonical = ["m_stateCnt == 4", "m_stateCnt == 8", "m_stateCnt == 12", "PlayWave", "PP_3DPARTICLE",
             "m_duration = 15", "m_pattern |= 0x120", "m_deltaPos2.y = -15.0", "m_speed", "m_size = 1.0",
             "m_animSpeed = 1", "m_alpha = 180.0", "m_alpha = 130.0", "m_alpha = 80.0", "SetAction",
             "SetSprName", "SetActName", "InitSegment"]
for label, text in (("primary Ref", ref_body), ("canonical PE", pe_body)):
    for token in canonical:
        if token not in text:
            raise SystemExit(f"{label} SpearBmr body missing canonical behavior {token!r}")
if "case 80:\n            CRagEffect::SpearBmr(this);" not in exe:
    raise SystemExit("canonical PE OnProcess case80 does not invoke SpearBmr")
if "SpearBmr(this" in ref:
    raise SystemExit("primary Ref OnProcess unexpectedly calls the SpearBmr helper")
if "void SpawnSpearBmr();" not in header or "case 80: SpawnSpearBmr(); return true;" not in dispatch:
    raise SystemExit("local SpearBmr declaration or effect80 route is missing")
if "void SetAction(int action, int motion, int type)" not in header or "m_effectAction = action" not in header:
    raise SystemExit("local SpearBmr action request state is missing")

ordered = ["m_stateCnt != 0 && m_stateCnt != 4 && m_stateCnt != 8 && m_stateCnt != 12", "PlayWave(",
           '"effect\\\\EF_FireBall.wav"', "LaunchEffectPrim(PP_3DPARTICLE", "m_duration = 15", "m_pattern |= 0x120",
           "m_deltaPos2 = {0.0f, -15.0f, 0.0f}", "std::atan2", "m_longitude = -prim->m_roty", "m_speed =",
           "m_size = 1.0f", "m_animSpeed = 1", "m_alpha = 180.0f", "m_alpha = 130.0f", "m_alpha = 80.0f",
           "GetCameraLongitude()", "SetAction(", "SetParticleResources(", "m_numSegments = 1", "InitReferenceSegments(prim)"]
cursor = 0
for token in ordered:
    position = local.find(token, cursor)
    if position < 0:
        raise SystemExit(f"local SpearBmr payload is missing/out of order: {token}")
    cursor = position + len(token)

family = body(source, "bool CEffectPrim::UsesReferenceParticle()", "// TEST_BODY_END reference_particle_render_family")
if "m_master->GetEffectType() == 80" not in family:
    raise SystemExit("SpearBmr is missing its PP_3DPARTICLE reference update family")

symbols = SymbolTable(ExeImage(ROOT / "HighPriest.exe"))
wave = symbols.resolve_name("aEffectEfFireba")
sprite = symbols.resolve_name("byte_71B894")
action = symbols.resolve_name("byte_71B884")
if wave is None or wave.raw != b"effect\\EF_FireBall.wav":
    raise SystemExit(f"SpearBmr PE wave bytes changed: {wave!r}")
asset_prefix = b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC3\xA2"
if sprite is None or sprite.raw != asset_prefix + b".spr" or action is None or action.raw != asset_prefix + b".act":
    raise SystemExit(f"SpearBmr PE sprite/action bytes changed: {sprite!r}, {action!r}")
if '"effect\\\\EF_FireBall.wav"' not in local or '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xC3\\xA2.spr"' not in local or '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xC3\\xA2.act"' not in local:
    raise SystemExit("local SpearBmr sound or CP949 resource literals differ from the executable")

print("SpearBmr80 parity: PE/Ref helper, PE caller, pulse payload, wave/assets, route and reference family passed")
