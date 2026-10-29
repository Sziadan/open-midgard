#!/usr/bin/env python3
"""Compare effect617 MakeBlur2's tint, state reset and periodic snapshot path."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


pe_call = re.search(r"case 0x269:\s*CRagEffect::MakeBlur2\(this\);", PE)
ref_call = re.search(r"case 0x269:\s*CRagEffect::MakeBlur2\(this\);", REF)
require(pe_call is not None and ref_call is not None,
        "PE/primary-Ref OnProcess3 effect617 MakeBlur2 caller changed")
require("case 617: return ProcessMakeBlur2();" in DISPATCH
        and re.search(r"617:\s*\"return ProcessMakeBlur2\(\);\"", GENERATOR),
        "effect617 generated local route changed")


def helper(source: str) -> str:
    match = re.search(r"void __thiscall CRagEffect::MakeBlur2\([^)]*\)\s*\{(.*?)\n\}", source, re.S)
    require(match is not None, "PE/secondary-Ref MakeBlur2 helper missing")
    return match.group(1)


pe_helper = helper(PE)
ref_helper = helper(REF2)
require(re.sub(r"\s+", "", pe_helper) == re.sub(r"\s+", "", ref_helper),
        "PE and secondary-Ref MakeBlur2 helper bodies diverged")
for label, body in (("PE", pe_helper), ("secondary Ref", ref_helper)):
    ordered = ("this->m_pos.x = 155.0", "this->m_pos.z = 255.0", "this->m_pos.y = 155.0",
               "SetArgb(m_master, -1, 155, 155, 255)", "m_stateCnt == 9998",
               "m_stateCnt = 0", "m_stateCnt % 20", "CBlurPC::CBlurPC",
               "this->m_pos.x", "this->m_pos.y", "this->m_pos.z", "m_gameObjectList")
    cursor = -1
    for token in ordered:
        index = body.find(token, cursor + 1)
        require(index >= 0, f"{label} MakeBlur2 missing or reordering {token}")
        cursor = index

local = re.search(r"bool CRagEffect::ProcessMakeBlur2\(\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local is not None, "local ProcessMakeBlur2 implementation missing")
body = local.group(1)
ordered = ("ResolveCurrentMaster()", "m_effectPos = vector3d{ 155.0f, 155.0f, 255.0f }",
           "master->SetArgb(-1, 155, 155, 255)", "m_stateCnt == 9998", "m_stateCnt = 0",
           "m_stateCnt % 20", "AddBlurSnapshot(master, this, 155, 155, 255)")
cursor = -1
for token in ordered:
    index = body.find(token, cursor + 1)
    require(index >= 0, f"local MakeBlur2 missing or reordering {token}")
    cursor = index
snapshot = re.search(r"CBlurPC\* AddBlurSnapshot\([^)]*\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(snapshot is not None and "new CBlurPC(pc)" in snapshot.group(1)
        and "blur->SendMsg(sender, 0, red, green, blue)" in snapshot.group(1)
        and "m_gameObjectList.push_back(blur)" in snapshot.group(1),
        "local blur snapshot construction/message/world insertion changed")
print("effect617 MakeBlur2 parity passed: PE/Ref helper, local route, tint/reset and 20-state snapshots")
