"""Compare effect116 WaterBall's helper, dispatch, resource strings, and fields."""
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


def function_body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start + len(signature))
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0: return text[opening:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


signature = "void __thiscall CRagEffect::WaterBall(CRagEffect *this)"
ref_body = re.sub(r"\s+", " ", function_body(REF, signature)).strip()
pe_body = re.sub(r"\s+", " ", function_body(PE, signature)).strip()
if ref_body != pe_body:
    raise SystemExit("primary Ref and canonical PE WaterBall helper bodies differ")
onprocess = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case_start = onprocess.index("          case 116:")
case_end = onprocess.index("          case 117:", case_start)
if "CRagEffect::WaterBall(this);" not in onprocess[case_start:case_end]:
    raise SystemExit("canonical OnProcess case116 no longer calls WaterBall")
if "case 116: return ProcessWaterBall116();" not in DISPATCH:
    raise SystemExit("local effect116 dispatcher route is missing")

symbols = SymbolTable(ExeImage())
expected = {
    "aEffectWizardWa": (0x71BB18, "effect\\wizard_waterball_chulung.wav"),
    "aEffectWaterOut": (0x71BB00, "effect\\water_out_a.bmp"),
    "aEffectWaterOut_0": (0x71BAE8, "effect\\water_out_b.bmp"),
    "aEffectWaterOut_1": (0x71BAD0, "effect\\water_out_c.bmp"),
}
for name, (va, path) in expected.items():
    resolved = symbols.resolve_name(name)
    if resolved is None or (resolved.va, resolved.text) != (va, path):
        raise SystemExit(f"{name} resolved as {resolved}, expected VA 0x{va:X} {path!r}")

local = function_body(SOURCE, "bool CRagEffect::ProcessWaterBall116()")
ordered = (
    "if (m_stateCnt != 0) return true;", "PlayWave(\"effect\\\\wizard_waterball_chulung.wav\"",
    "LaunchEffectPrim(PP_3DTEXTURE", "prim->m_duration = m_duration;", "prim->m_pattern = 64;",
    "prim->m_pattern |= 0x100;", "prim->m_renderFlag |= 2;", "prim->m_repeatAnim = true;",
    "prim->m_widthSize = 3.5f;", "prim->m_heightSize = 3.5f;", "prim->m_speed3d.y =",
    "prim->m_deltaPos2.y = -5.0f;", "prim->m_animSpeed = 5;", "prim->m_totalTexture = 3;",
    '"effect\\\\water_out_a.bmp"', '"effect\\\\water_out_b.bmp"', '"effect\\\\water_out_c.bmp"',
)
cursor = 0
for token in ordered:
    found = local.find(token, cursor)
    if found < 0: raise SystemExit(f"local WaterBall behavior missing or out of order: {token}")
    cursor = found + len(token)
if "rand(" in local:
    raise SystemExit("WaterBall must not consume RNG")
print("effect116 parity: Ref/PE helper equality, case116 route, four PE resource VAs, and local field order passed")
