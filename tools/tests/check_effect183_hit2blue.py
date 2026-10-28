"""Verify effect183 Hit2 helper, caller, resources and local parity."""
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


signature = "void __thiscall CRagEffect::Hit2(CRagEffect *this, int option)"
compact = lambda value: re.sub(r"\s+", " ", value).strip()
if compact(body(REF, signature)) != compact(body(PE, signature)):
    raise SystemExit("primary Ref and canonical PE Hit2 helper bodies differ")

main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 183:")
end = main.index("          case 185:", start)
caller = main[start:end]
for token in ("CRagEffect::Hit2(this, 1);", "v151->CMode::SendMsg(v151, 135, 1, 0, 0);", "goto LABEL_424;"):
    if token not in caller:
        raise SystemExit(f"canonical effect183 caller missing {token}")
if any(f"m_stateCnt == {frame}" not in caller for frame in (3, 7, 9)):
    raise SystemExit("canonical effect183 caller changed message frames")
if "case 183: return ProcessHit2Blue();" not in DISPATCH:
    raise SystemExit("local effect183 route is missing")

symbols = SymbolTable(ExeImage())
for name, va, path in (
    ("aEffectEfHit2Wa", 0x719BC8, "effect\\EF_hit2.wav"),
    ("aEffectLens1Tga", 0x71A490, "effect\\lens1.tga"),
    ("aEffectLens2Tga", 0x71A3A4, "effect\\lens2.tga"),
):
    resolved = symbols.resolve_name(name)
    if resolved is None or (resolved.va, resolved.text) != (va, path):
        raise SystemExit(f"{name} resolved as {resolved!r}, expected VA 0x{va:X} {path!r}")

local = body(SOURCE, "void CRagEffect::SpawnHit2(int option)")
ordered = (
    "if (m_stateCnt != 0) return;", "PlayWave(\"effect\\\\EF_hit2.wav\"", "for (int i = 0; i < 8; ++i)",
    "LaunchEffectPrim(PP_2DTEXTURE", "prim->m_pattern = 1;", "rand() % 20 + 10",
    "rand() % 30", "rand() % 45 + 5", "rand() % 15 + 5", "rand() % 20 + 20",
    "rand() % 5", "prim->m_tintColor = RGB(120,120,255);", "prim->m_alphaSpeed = static_cast<float>(prim->m_maxAlpha * 0.125);",
    "i % 2 == 0 ? \"effect\\\\lens1.tga\" : \"effect\\\\lens2.tga\"",
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0:
        raise SystemExit(f"local Hit2 behavior missing or out of order: {token}")
    cursor = found + len(token)
if local.count("rand()") != 6:
    raise SystemExit("Hit2 must preserve six ordered random draws per particle")
if "m_master->GetEffectType() == 183" not in body(SOURCE, "bool CEffectPrim::UsesReferenceScreenTexture() const"):
    raise SystemExit("effect183 is missing its source screen-texture selector")
processor = body(SOURCE, "bool CRagEffect::ProcessHit2Blue()")
if "if (m_stateCnt == 0) SpawnHit2(1);" not in processor or "ProcessEZ2STR(0.0f,-80.0f,0.0f)" not in processor:
    raise SystemExit("local Hit2Blue caller changed emission/completion behavior")
print("effect183 parity: Ref/PE helper equality, caller/route, resources, selector and local operation order passed")
