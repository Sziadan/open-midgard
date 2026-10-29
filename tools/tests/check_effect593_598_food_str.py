#!/usr/bin/env python3
"""Compare effects593-598 food STR routes, shared cue, and Init assets."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
CATALOG = (ROOT / "src/world/RagEffectCatalog.generated.inc").read_text(encoding="utf-8")
STR_TABLE = (ROOT / "src/world/RagEffectStrNameTable.generated.inc").read_text(encoding="utf-8")
EFFECTS = {
    593: ("food_str.str", "aFoodStrStr", 0x71A8C8),
    594: ("food_int.str", "aFoodIntStr", 0x71A874),
    595: ("food_vit.str", "aFoodVitStr", 0x71A864),
    596: ("food_agi.str", "aFoodAgiStr", 0x71A854),
    597: ("food_dex.str", "aFoodDexStr", 0x71A844),
    598: ("food_luk.str", "aFoodLukStr", 0x71A834),
}


def function_body(source: str, signature: str, last: bool = False) -> str:
    matches = list(re.finditer(signature, source))
    if not matches:
        raise AssertionError(f"missing {signature}")
    match = matches[-1] if last else matches[0]
    opening = source.find("{", match.end())
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:offset]
    raise AssertionError(f"unterminated {signature}")


pe_dispatch = function_body(PE, r"__thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)", last=True)
ref_dispatch = function_body(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    group = re.search(r"case\s+0x251:\s*case\s+0x252:\s*case\s+0x253:\s*case\s+0x254:\s*case\s+0x255:\s*case\s+0x256:(.*?)(?=case\s+0x257:)", dispatch, re.S)
    if group is None:
        raise AssertionError(f"{label} food cases593-598 are not grouped together")
    branch = group.group(1)
    ordered = ["if ( this->m_stateCnt )", "goto $L146677", "this->m_pos = this->m_master->m_pos", "goto LABEL_399"]
    offsets = [branch.find(token) for token in ordered]
    if any(offset < 0 for offset in offsets) or offsets != sorted(offsets):
        raise AssertionError(f"{label} food cases593-598 branch order or state gate changed")
    if any(token in branch for token in ("rand(", "SendMsg", "Effect_SPR")):
        raise AssertionError(f"{label} food cases593-598 have unexpected caller side effects")
    if not re.search(r"LABEL_399:\s*v\w+\s*=\s*&CModeMgr::GetGameMode\(&g_modeMgr\)->m_world->m_player->m_pos;\s*z\w+\s*=\s*this->m_pos.z\s*-\s*v\w+->z;\s*x\w+\s*=\s*this->m_pos.x\s*-\s*v\w+->x;\s*PlayWave\(aHealEffectWav,\s*x\w+,\s*0\.0,\s*z\w+,\s*250,\s*40,\s*1\.0\);", dispatch):
        raise AssertionError(f"{label} shared food state-zero wave coordinates or arguments changed")
    if not re.search(r"\$L146677:\s*result\s*=\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*if\s*\(\s*!result\s*\)\s*return result;\s*return 1", dispatch):
        raise AssertionError(f"{label} food shared STR continuation must propagate completion")

init_pe = function_body(PE, r"__thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)", last=True)
init_ref = function_body(REF, r"void __thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)")
image = ExeImage()
for effect_id, (filename, symbol, va) in EFFECTS.items():
    if effect_id == 593:
        pe_map = re.search(r"else if \( v6 == 593 \)\s*\{\s*sprintf\(StrName,\s*aFoodStrStr\);", init_pe)
        ref_map = re.search(r"else if \( v6 == 593 \)\s*\{\s*sprintf\(StrName,\s*aFoodStrStr\);", init_ref)
    else:
        pe_map = re.search(rf"case\s+{effect_id}:\s*sprintf\(StrName,\s*{symbol}\);\s*break;", init_pe)
        ref_map = re.search(rf"case\s+{effect_id}:\s*sprintf\(StrName,\s*{symbol}\);\s*break;", init_ref)
    if pe_map is None or ref_map is None:
        raise AssertionError(f"PE/Ref Init effect{effect_id} STR mapping changed")
    resolved = image.read_cstring_at(va)
    if resolved is None or resolved.raw != filename.encode("ascii"):
        raise AssertionError(f"PE STR bytes at VA {va:#x} no longer resolve to {filename}")
    for label, table in (("catalog", CATALOG), ("STR table", STR_TABLE)):
        if not re.search(rf'\{{\s*{effect_id},\s*"{re.escape(filename)}"', table):
            raise AssertionError(f"local {label} effect{effect_id} mapping changed")
    if not re.search(rf"case\s+{effect_id}:\s+return ProcessTier3StrEffect\(\);", DISPATCH):
        raise AssertionError(f"local effect{effect_id} dispatch changed")

local_tier3 = function_body(LOCAL, r"bool CRagEffect::ProcessTier3StrEffect\(\)")
if not re.search(r"case\s+593:\s*case\s+594:\s*case\s+595:\s*case\s+596:\s*case\s+597:\s*case\s+598:\s*waveFrame = 0;\s*wave = \"_heal_effect\.wav\";\s*break;", local_tier3):
    raise AssertionError("local food group must copy position and play _heal_effect.wav only at frame zero")
if not re.search(r"const bool active = ProcessEZ2STR\(0\.0f,\s*-80\.0f,\s*0\.0f\);\s*return propagateStrResult \? active : true;", local_tier3):
    raise AssertionError("local food group must propagate generic STR completion")

print("effects593-598 source check passed: six grouped cases, state-zero cue, propagated STR results, six PE-backed STR names")
