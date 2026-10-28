"""Compare effect165 Banjjakii helper, PE assets and particle selector."""
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


signature = "void __thiscall CRagEffect::Banjjakii(CRagEffect *this)"
compact = lambda value: re.sub(r"\s+", " ", value).strip()
if compact(body(REF, signature)) != compact(body(PE, signature)):
    raise SystemExit("primary Ref and canonical PE Banjjakii helper bodies differ")
main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 165:")
end = main.index("          case 166:", start)
if "CRagEffect::Banjjakii(this);" not in main[start:end]:
    raise SystemExit("canonical OnProcess case165 no longer calls Banjjakii")
if "case 165: return ProcessBanjjakii165();" not in DISPATCH:
    raise SystemExit("local effect165 route is missing")

symbols = SymbolTable(ExeImage())
for symbol, va, path in (
    ("byte_71B4C0", 0x71B4C0, "이팩트\\크리스마스.spr"),
    ("byte_71B4A8", 0x71B4A8, "이팩트\\크리스마스.act"),
):
    resolved = symbols.resolve_name(symbol)
    if resolved is None or (resolved.va, resolved.text) != (va, path):
        raise SystemExit(f"{symbol} resolved as {resolved!r}, expected VA 0x{va:X} {path!r}")

local = body(SOURCE, "bool CRagEffect::ProcessBanjjakii165()")
ordered = (
    "m_effectPos = m_master->m_pos;", "if (m_stateCnt != 0) return true;",
    "LaunchEffectPrim(PP_3DPARTICLE, vector3d{})", "prim->m_pattern |= 0x200;",
    "prim->m_duration = 125;", "prim->m_deltaPos2.y = -10.0f;",
    "if (m_param[0] <= 0.0f) m_param[0] = 1.0f;",
    "if (m_param[1] < 1.0f) m_param[1] = 1.0f;",
    "prim->m_animSpeed = static_cast<int>(m_param[1]);", "prim->SetParticleResources(",
    "const int draw = rand() % 10;", "prim->m_size = draw % 3 == 1 ? 0.01f : 1.0f;",
    "prim->m_spriteAction = draw / 2;", "prim->m_curMotion = 0;",
    "prim->m_numSegments = 1;", "InitReferenceSegments(prim);",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Banjjakii behavior missing or out of order: {token}")
    cursor = found + len(token)
if local.count("rand()") != 1:
    raise SystemExit("Banjjakii must consume exactly one random draw on its spawn frame")
if "m_master->GetEffectType() == 165" not in body(SOURCE, "bool CEffectPrim::UsesReferenceParticle() const"):
    raise SystemExit("effect165 is missing its source particle update/render selector")
print("effect165 parity: Ref/PE helper equality, caller/route, PE assets, particle selector and operation order passed")
