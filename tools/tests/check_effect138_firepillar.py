"""Compare effect138 FirePillar helper, resource and primitive route."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SOURCE = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start + len(signature))
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


signature = "void __thiscall CRagEffect::FirePillarOn(CRagEffect *this)"
compact = lambda value: re.sub(r"\s+", " ", value).strip()
if compact(body(REF, signature)) != compact(body(PE, signature)):
    raise SystemExit("primary Ref and canonical PE FirePillarOn helper bodies differ")
main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 138:")
end = main.index("          case 140:", start)
if "CRagEffect::FirePillarOn(this);" not in main[start:end]:
    raise SystemExit("canonical OnProcess case138 no longer calls FirePillarOn")
if "case 138: return ProcessFirePillar138();" not in DISPATCH:
    raise SystemExit("local effect138 route is missing")

resource = SymbolTable(ExeImage()).resolve_name("aEffectMagicRed")
if resource is None or (resource.va, resource.text) != (0x71BBB8, "effect\\magic_red.tga"):
    raise SystemExit(f"aEffectMagicRed resolved as {resource}, expected VA 0x71BBB8")

local = body(SOURCE, "bool CRagEffect::ProcessFirePillar138()")
ordered = (
    "if (m_stateCnt != 0) return true;", "for (int i = 0; i < 3; ++i)",
    "LaunchEffectPrim(PP_3DCYLINDER, vector3d{})", "prim->m_duration = m_duration;",
    "prim->m_longSpeed = -3.0f;", "prim->m_outerSize =", "prim->m_innerSize =",
    "prim->m_heightSize =", "prim->m_rollSpeed = -0.5f;", "prim->m_alpha =",
    '"effect\\\\magic_red.tga"',
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local FirePillar behavior missing or out of order: {token}")
    cursor = found + len(token)
if "case 138:" not in body(SOURCE, "bool CEffectPrim::UsesReferenceCylinder() const"):
    raise SystemExit("effect138 is missing its source cylinder update/render selector")
if "rand(" in local or "PlayWave(" in local:
    raise SystemExit("FirePillar must not consume RNG or play audio")
print("effect138 parity: Ref/PE helper equality, case route, resource, cylinder selector and field order passed")
