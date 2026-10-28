"""Verify effect198 preserves the canonical empty case's shared frame flow."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
main = pe[pe.rindex(signature):]
start = main.index("case 105:")
end = main.index("case 106:", start)
group = main[start:end]
expected_cases = ["case 105:", "case 161:", "case 198:", "case 443:", "case 489:", "case 490:"]
cursor = 0
for case in expected_cases:
    cursor = group.find(case, cursor)
    if cursor < 0:
        raise SystemExit(f"canonical PE no-op group missing {case}")
    cursor += len(case)
if "break;" not in group or "CRagEffect::" in group or "PlayWave(" in group or "rand(" in group:
    raise SystemExit("canonical PE effect198 group is no longer an inline no-op")
if "case 198:" in ref:
    raise SystemExit("primary Ref now contains a case198 body; review the source discrepancy")
if "case 198: return true;" not in dispatch:
    raise SystemExit("effect198 must continue through common frame processing")
if "case 198: return UnportedEffect();" in dispatch:
    raise SystemExit("effect198 still terminates through UnportedEffect")
frame = source[source.rindex("bool CRagEffect::ProcessEffectFrame()") : source.rindex("bool CRagEffect::UnportedEffect()")]
if "#include \"RagEffectOnProcessCases.generated.inc\"" not in frame or "return *result" not in frame:
    raise SystemExit("effect198 route is not part of normal frame dispatch")

print("effect198 parity: canonical inline no-op keeps shared frame flow without effects")
