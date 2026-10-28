"""Compare effect171 VenomDust2 helper, assets and primitive routing."""
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


signature = "void __thiscall CRagEffect::VenomDust2(CRagEffect *this)"
compact = lambda value: re.sub(r"\s+", " ", value).strip()
if compact(body(REF, signature)) != compact(body(PE, signature)):
    raise SystemExit("primary Ref and canonical PE VenomDust2 helper bodies differ")
main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 171:")
end = main.index("          case 172:", start)
if "CRagEffect::VenomDust2(this);" not in main[start:end]:
    raise SystemExit("canonical OnProcess case171 no longer calls VenomDust2")
if "case 171: ApplyVenomDust2(); return true;" not in DISPATCH:
    raise SystemExit("local effect171 route is missing")

symbols = SymbolTable(ExeImage())
for name, va, path in (
    ("byte_71BC24", 0x71BC24, "이팩트\\particle3.spr"),
    ("byte_71BC0C", 0x71BC0C, "이팩트\\particle3.act"),
):
    resolved = symbols.resolve_name(name)
    if resolved is None or (resolved.va, resolved.text) != (va, path):
        raise SystemExit(f"{name} resolved as {resolved!r}, expected VA 0x{va:X} {path!r}")

local = body(SOURCE, "void CRagEffect::ApplyVenomDust2()")
ordered = (
    "if (m_stateCnt % 5 != 0) return;", "LaunchEffectPrim(PP_3DPARTICLE, vector3d{})",
    "prim->m_duration = 10;", "const int yaw = rand() % 360;", "MatrixAppendYRotation(",
    "MatrixIdentity(prim->m_matrix);", "MatrixAppendXRotation(prim->m_matrix, GetRadian(90));",
    "prim->m_speed = static_cast<float>(rand() % 20 + 20) / 80.0f;",
    "prim->m_accel =", "prim->m_size = 0.8f;", "prim->m_rollSpeed = 3.0f;",
    "prim->m_alpha = 70.0f;", "prim->m_alphaSpeed = prim->m_maxAlpha / 10.0f;",
    "particle3.spr", "particle3.act",
    "prim->m_numSegments = 1;", "InitReferenceSegments(prim);",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local VenomDust2 behavior missing or out of order: {token}")
    cursor = found + len(token)
if local.count("rand()") != 2:
    raise SystemExit("VenomDust2 must preserve exactly two ordered random draws")
if "m_master->GetEffectType() == 171" not in body(SOURCE, "bool CEffectPrim::UsesReferenceParticle() const"):
    raise SystemExit("effect171 is missing its source particle update/render selector")
print("effect171 parity: Ref/PE helper equality, caller/route, resources, selector and operation order passed")
