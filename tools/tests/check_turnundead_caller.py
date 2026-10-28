"""Compare TurnUndead effect 82 with the canonical PE and primary Ref."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def body(text: str, start: str, end: str) -> str:
    first = text.index(start)
    last = text.index(end, first + len(start))
    return text[first:last]


ref = body(ref_text, "void __thiscall CRagEffect::TurnUndead(CRagEffect *this)", "void __thiscall CRagEffect::VenomDust2(")
pe = body(exe_text, "void __thiscall CRagEffect::TurnUndead(CRagEffect *this)\n{", "// 5FD8FF")
local = body(source, "void CRagEffect::SpawnTurnUndead()", "// TEST_BODY_END turn_undead_handler")

canonical = ["m_stateCnt == 37", "PlayWave", "ProjectVertex", "PP_2DCIRCLE", "m_radius = 370.0",
             "PP_2DFLASH", "m_duration = 20", "m_speed3d.x", "m_speed3d.y", "rand() % 360",
             "rand() % 60 + 10", "rand() % 25 + 5", "rand() % 30 + 350", "rand() % 30 + 150",
             "m_alpha = 150.0", "m_stateCnt == 50", "PP_3DTEXTURE", "m_duration = 30", "m_roll = 90.0",
             "m_ChangePoint = 15", "aEffectRingLine"]
for label, text in (("primary Ref", ref), ("canonical PE", pe)):
    for token in canonical:
        if token not in text:
            raise SystemExit(f"{label} TurnUndead body missing {token!r}")
    if len(re.findall(r"\brand\s*\(", text)) != 7:
        raise SystemExit(f"{label} TurnUndead helper must preserve seven syntactic RNG draws")
if "case 82:\n            CRagEffect::TurnUndead(this);" not in exe_text:
    raise SystemExit("canonical PE OnProcess case82 does not invoke TurnUndead")
if "TurnUndead(this" in ref_text:
    raise SystemExit("primary Ref caller unexpectedly invokes the TurnUndead helper")
if "void SpawnTurnUndead();" not in header or "case 82: SpawnTurnUndead(); return true;" not in dispatch:
    raise SystemExit("local TurnUndead declaration or effect82 route is missing")

ordered = ["ResolveCurrentMaster()", "m_stateCnt == 37", '"effect\\\\EF_Bash.wav"',
           "ProjectReferenceEffectVertex(master->m_pos", "if (m_stateCnt == 0)",
           "LaunchEffectPrim(PP_2DCIRCLE", '"effect\\\\alpha_down.tga"',
           "for (int index = 0; index < 5; ++index)", "LaunchEffectPrim(PP_2DFLASH", "rand() % 360",
           "rand() % 60 + 10", "rand() % 25 + 5", "rand() % 30 + 350", "rand() % 30 + 150",
           '"EFFECT\\\\alpha_center.tga"', "m_effectPos = master->m_pos", "if (m_stateCnt == 50)",
           "LaunchEffectPrim(PP_3DTEXTURE", "m_duration = 30", "InitReferenceSegments(ring)",
           '"effect\\\\ring_liner.tga"']
cursor = 0
for token in ordered:
    position = local.find(token, cursor)
    if position < 0:
        raise SystemExit(f"local TurnUndead payload is missing/out of order: {token}")
    cursor = position + len(token)
if len(re.findall(r"\brand\s*\(", local)) != 5:
    raise SystemExit("local TurnUndead must preserve five ordered RNG draws per state-zero flash")

families = {
    "UsesReferenceScreenCircle": ("bool CEffectPrim::UsesReferenceScreenCircle()", "bool CEffectPrim::UsesReferenceFlash()"),
    "UsesReferenceFlash": ("bool CEffectPrim::UsesReferenceFlash()", "bool CEffectPrim::UsesReferenceQuadHorn()"),
    "UsesReferenceTexture": ("bool CEffectPrim::UsesReferenceTexture()", "bool CEffectPrim::UsesReferenceCylinder()"),
}
for method, (start, end) in families.items():
    if "m_master->GetEffectType() == 82" not in body(source, start, end):
        raise SystemExit(f"TurnUndead is missing the {method} renderer family")

symbols = SymbolTable(ExeImage(ROOT / "HighPriest.exe"))
expected = {
    "aEffectEfBashWa": b"effect\\EF_Bash.wav",
    "aEffectAlphaDow": b"effect\\alpha_down.tga",
    "aEffectAlphaCen_0": b"EFFECT\\alpha_center.tga",
    "aEffectRingLine": b"effect\\ring_liner.tga",
}
for name, value in expected.items():
    resolved = symbols.resolve_name(name)
    if resolved is None or resolved.raw != value:
        raise SystemExit(f"TurnUndead PE asset {name} differs from {value!r}: {resolved!r}")
for value in ('"effect\\\\EF_Bash.wav"', '"effect\\\\alpha_down.tga"',
              '"EFFECT\\\\alpha_center.tga"', '"effect\\\\ring_liner.tga"'):
    if value not in local:
        raise SystemExit(f"local TurnUndead resource literal missing: {value}")

print("TurnUndead82 parity: PE/Ref helper, caller, pulse assets, projected position, payloads, route and renderer families passed")
