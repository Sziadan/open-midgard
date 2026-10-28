#!/usr/bin/env python3
"""Check Repair Weapon effect101 route and PE-backed cue."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 101: return ProcessRepairWeapon101();" not in dispatch:
    raise AssertionError("effect101 is not routed to Repair Weapon")
if "CRagEffect::RepairWeapon" not in ref or "0x606480" not in ref.lower():
    raise AssertionError("Ref Repair Weapon helper/canonical PE anchor is missing")
if ExeImage().read_cstring_at(0x71BD2C).raw != b"effect\\black_weapon_repair_a.wav":
    raise AssertionError("Repair Weapon PE cue changed")
start = rag.find("bool CRagEffect::ProcessRepairWeapon101()")
end = rag.find("// TEST_BODY_END repairweapon101_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Repair Weapon handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 20", "m_stateCnt == 55", "black_weapon_repair_a.wav",
    "ProcessEZ2STR(0.0f, -80.0f, 0.0f)",
):
    if token not in body:
        raise AssertionError(f"Repair Weapon handler is missing {token}")
print("effect101 source check passed: state20/55 cue, PE resource bytes, shared STR continuation")
