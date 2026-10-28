"""Compare LockOn effect 60 between the canonical PE, primary Ref and local route."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def between(text: str, start: str, end: str) -> str:
    first = text.index(start)
    last = text.index(end, first + len(start))
    return text[first:last]


ref = between(ref_text, "void __thiscall CRagEffect::LockOn(CRagEffect *this)", "void __thiscall CRagEffect::MagnumBreak")
pe = between(exe_text, "void __thiscall CRagEffect::LockOn(CRagEffect *this)\n{", "//----- (005FBA00)")
local = between(source, "void CRagEffect::SpawnLockOn()", "void CRagEffect::SpawnDarkBreath()")

for label, text in (("Ref", ref), ("PE", pe)):
    for field in ("m_longitude = 0.0", "m_longitude = 45.0", "m_longSpeed = 4.5", "m_roll = 90.0",
                  "m_widthSize = 30.0", "m_heightSize = 30.0", "m_widthSpeed = -1.0", "m_heightSpeed = -1.0",
                  "m_ChangePoint = 20", "m_RGBCycleDelay = 20", "m_red = 250", "m_green = 150", "m_blue = 150",
                  "m_greenSpeed = -15", "m_blueSpeed = -15", "m_stateCnt", "GetHeight"):
        if field not in text:
            raise SystemExit(f"{label} LockOn body is missing {field}")
    if text.count("LaunchEffectPrim") != 2:
        raise SystemExit(f"{label} LockOn must launch two texture primitives")

if "case 60:\n            CRagEffect::LockOn(this);" not in exe_text:
    raise SystemExit("canonical PE OnProcess case60 does not invoke LockOn")
if "void SpawnLockOn();" not in header or "case 60: SpawnLockOn(); return true;" not in dispatch:
    raise SystemExit("local LockOn declaration or effect60 route is missing")
ordered = ["ResolveCurrentMaster()", "m_effectPos = master->m_pos", "if (m_stateCnt == 0)",
           "for (const float longitude : {0.0f, 45.0f})", "LaunchEffectPrim(PP_3DTEXTURE", "m_duration = m_duration",
           "m_pattern |= 0x800", "m_longitude = longitude", "m_longSpeed = 4.5f", "m_roll = 90.0f",
           "m_widthSize = 30.0f", "m_heightSize = 30.0f", "m_widthSpeed = -1.0f", "m_heightSpeed = -1.0f",
           "m_changePoint = 20", "m_rgbCycleDelay = 20", "m_red = 250", "m_green = 150", "m_blue = 150",
           "m_greenSpeed = -15.0f", "m_blueSpeed = -15.0f", '"effect\\\\LockOn128.tga"',
           "m_effectPos.y = ResolveGroundHeight(m_effectPos) - 2.0f"]
cursor = 0
for token in ordered:
    position = local.find(token, cursor)
    if position < 0:
        raise SystemExit(f"local LockOn payload is missing or out of order: {token}")
    cursor = position + len(token)

family = between(source, "bool CEffectPrim::UsesReferenceTexture()", "bool CEffectPrim::UsesReferenceCylinder()")
if "m_master->GetEffectType() == 60" not in family:
    raise SystemExit("LockOn is missing its PP_3DTEXTURE reference renderer family")

resolved = SymbolTable(ExeImage(ROOT / "HighPriest.exe")).resolve_name("aEffectLockon12")
if resolved is None or resolved.raw != b"effect\\LockOn128.tga":
    raise SystemExit(f"canonical LockOn texture bytes changed: {resolved!r}")
if '"effect\\\\LockOn128.tga"' not in local:
    raise SystemExit("local LockOn texture path differs from the canonical PE bytes")

print("LockOn60 parity: PE/Ref caller and payload, local route, terrain anchor, texture bytes and renderer family passed")
