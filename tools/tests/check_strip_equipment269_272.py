#!/usr/bin/env python3
"""Check Strip Weapon/Shield/Armor/Helm effects269-272 shared PE behavior."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
for effect_id in range(269, 273):
    if f"case {effect_id}: return ProcessStripEquipment269To272();" not in dispatch:
        raise AssertionError(f"effect{effect_id} is not routed to the shared Strip handler")
for name in ("aStripWeaponStr", "aStripShieldStr", "aStripArmorStr", "aStripHelmStr"):
    if name not in ref:
        raise AssertionError(f"Ref initialization is missing {name}")
if ExeImage().read_cstring_at(0x6F1FF0).raw != b"effect\\":
    raise AssertionError("Strip equipment PE wave prefix changed")
onprocess = pe[pe.find("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)\n{") :]
branch = onprocess[onprocess.find("case 269:") : onprocess.find("case 273:")]
for token in (
    "case 270:", "case 271:", "case 272:", "m_stateCnt == 6", "m_pos = (*p_m_master)->m_pos",
    "PlayWave(aEffect_70", "250, 40, 1.0", "goto LABEL_424",
):
    if token not in branch:
        raise AssertionError(f"shared PE Strip branch is missing {token}")
start = rag.find("bool CRagEffect::ProcessStripEquipment269To272()")
end = rag.find("// TEST_BODY_END strip_equipment269_272_handler", start)
if start < 0 or end < 0:
    raise AssertionError("local Strip equipment handler boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 6", "m_pos=master->m_pos", "effect\\\\", "ProcessEZ2STR(0.0f,-80.0f,0.0f)",
):
    if token not in body:
        raise AssertionError(f"local Strip equipment handler is missing {token}")
print("effects269-272 source check passed: shared state6 behavior and exact available wave prefix")
