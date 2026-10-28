"""Compare effect118 FireIvy's helper, dispatch, PE resources, and local fields."""
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
            if depth == 0: return text[opening:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


signature = "void __thiscall CRagEffect::FireIvy(CRagEffect *this)"
if re.sub(r"\s+", " ", body(REF, signature)).strip() != re.sub(r"\s+", " ", body(PE, signature)).strip():
    raise SystemExit("primary Ref and canonical PE FireIvy helper bodies differ")
main = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = main.index("          case 118:")
end = main.index("          case 119:", start)
if "CRagEffect::FireIvy(this);" not in main[start:end]:
    raise SystemExit("canonical OnProcess case118 no longer calls FireIvy")
if "case 118: return ProcessFireIvy118();" not in DISPATCH:
    raise SystemExit("local effect118 route is missing")

symbols = SymbolTable(ExeImage())
expected = {
    "aEffectWizardFi_1": (0x71BB6C, "effect\\wizard_fire_ivy.wav"),
    "aEffectFireIvyB": (0x71A7AC, "effect\\fire_ivy.bmp"),
}
for name, (va, path) in expected.items():
    resolved = symbols.resolve_name(name)
    if resolved is None or (resolved.va, resolved.text) != (va, path):
        raise SystemExit(f"{name} resolved as {resolved}, expected VA 0x{va:X} {path!r}")

local = body(SOURCE, "bool CRagEffect::ProcessFireIvy118()")
ordered = (
    "if (m_stateCnt != 0) return true;", "PlayWave(\"effect\\\\wizard_fire_ivy.wav\"",
    "LaunchEffectPrim(PP_3DTEXTURE", "prim->m_duration = m_duration;", "prim->m_pattern = 64;",
    "prim->m_pattern |= 0x100;", "prim->m_renderFlag |= 2;", "prim->m_widthSize = 3.5f;",
    "prim->m_heightSize = 3.5f;", "prim->m_speed3d.y =", "prim->m_deltaPos2.y = -5.0f;",
    '"effect\\\\fire_ivy.bmp"',
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0: raise SystemExit(f"local FireIvy behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local:
    raise SystemExit("FireIvy helper must not consume RNG")
print("effect118 parity: Ref/PE helper equality, case118 route, two PE resources, and local field order passed")
