"""Verify effect105's canonical no-op branch and shared frame-count behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
fixture = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")

function = pe.rindex("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
start = pe.index("          case 105:", function)
end = pe.index("          case 106:", start)
branch = pe[start:end]
expected = """          case 105:
          case 161:
          case 198:
          case 443:
          case 489:
          case 490:
            break;
"""
if branch != expected:
    raise SystemExit("canonical effect105 grouped inline branch changed")
for effect_id in (105, 161, 443, 489, 490):
    if f"case {effect_id}: return true;" not in dispatch:
        raise SystemExit(f"effect{effect_id} must preserve the canonical no-op frame")
if re.search(r"(?:void|bool)\s+CRagEffect::\w*105\w*\s*\(", ref):
    raise SystemExit("unexpected primary Ref helper appeared for inline effect105")
on_process = re.search(r"u8 CRagEffect::OnProcess\(\)\s*\{(.*?)\n\}", source, re.S)
if not on_process or "if (!ProcessEffectFrame())" not in on_process.group(1):
    raise SystemExit("effect105 must retain the shared OnProcess lifecycle")
if "++m_stateCnt;" not in on_process.group(1) or "return m_stateCnt < m_duration;" not in on_process.group(1):
    raise SystemExit("effect105 must advance and expire through the common duration tail")
for phrase in (
    "effect105 explicit no-op still advances one common lifecycle frame without visuals, audio or RNG",
    "effect105 common duration tail expires only after its second dispatched frame",
):
    if phrase not in fixture:
        raise SystemExit(f"effect105 lifecycle fixture missing {phrase!r}")
print("effect105 parity: canonical no-op branch and common frame-count tail passed")
