#!/usr/bin/env python3
"""Check Shield Charge effect246's zero-offset STR-only behavior."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
ref2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 246: return ProcessShieldCharge246();" not in dispatch:
    raise AssertionError("effect246 is not routed to Shield Charge")
if "aShieldChargeSt" not in ref or "CRagEffect::ShieldCharge" not in ref2 or "0x60d6f0" not in ref2.lower():
    raise AssertionError("Ref Shield Charge init/helper evidence is missing")
if "case 246:" not in pe or "CRagEffect::ShieldCharge(this)" not in pe:
    raise AssertionError("PE effect246 route/helper call is missing")
if ExeImage().read_cstring_at(0x71AA14).raw != b"shield_charge.str":
    raise AssertionError("Shield Charge PE STR name changed")
start = rag.find("bool CRagEffect::ProcessShieldCharge246()")
end = rag.find("// TEST_BODY_END shieldcharge246_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Shield Charge handler test boundary is missing")
body = rag[start:end]
if "ProcessEZ2STR(0.0f,0.0f,0.0f)" not in body:
    raise AssertionError("Shield Charge must use zero STR offsets and propagate completion")
print("effect246 source check passed: PE/Ref route, shield_charge.str bytes, zero-offset STR behavior")
