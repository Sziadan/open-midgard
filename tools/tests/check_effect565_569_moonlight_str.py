#!/usr/bin/env python3
"""Compare effects565-569 callers, timed cues, and Init STR assets."""
from __future__ import annotations

import ast
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
    565: ("moonlight_1.str", "aMoonlight1Str", 0x71A984, "effect\\h_moonlight_1.wav", 0x71959C, "aEffectHMoonlig"),
    566: ("moonlight_2.str", "aMoonlight2Str", 0x71A974, "effect\\h_moonlight_2.wav", 0x719580, "aEffectHMoonlig_0"),
    567: ("moonlight_3.str", "aMoonlight3Str", 0x71A964, "effect\\h_moonlight_3.wav", 0x719564, "aEffectHMoonlig_1"),
    568: ("h_levelup.str", "aHLevelupStr", 0x71A954, "levelup.wav", 0x713168, "aLevelupWav"),
    569: ("defense.str", "aDefenseStr", 0x71A8D8, None, None, None),
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
image = ExeImage()
for label, dispatch in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    for effect_id, (filename, _, str_va, wave, wave_va, wave_symbol) in EFFECTS.items():
        case_id = f"0x{effect_id:X}"
        if wave is not None:
            next_case = f"0x{effect_id + 1:X}"
            branch = re.search(rf"case\s+{case_id}:(.*?)(?=case\s+{next_case}:)", dispatch, re.S)
            if branch is None:
                raise AssertionError(f"{label} effect{effect_id} caller branch missing")
            text = branch.group(1)
            ordered = ["if ( !this->m_stateCnt )", "this->m_pos = this->m_master->m_pos",
                       "m_world->m_player->m_pos", f"PlayWave({wave_symbol}", "0.0, z", "250, 40, 1.0", "goto $L146677"]
            offsets = [text.find(token) for token in ordered]
            if any(offset < 0 for offset in offsets) or offsets != sorted(offsets):
                raise AssertionError(f"{label} effect{effect_id} frame-zero copy, cue, and STR order changed")
            wave_bytes = image.read_cstring_at(wave_va)
            if wave_bytes is None or wave_bytes.raw != wave.encode("ascii"):
                raise AssertionError(f"PE wave bytes at VA {wave_va:#x} differ from {wave}")
        else:
            if not re.search(r"case\s+0x239:\s*case\s+0x25B:\s*goto\s+\$L146677", dispatch):
                raise AssertionError(f"{label} effect569 must silently join the shared STR tail")

        init_signature = r"__thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)"
        init = function_body(PE if label == "PE" else REF, init_signature, last=label == "PE")
        if not re.search(rf"case\s+{effect_id}:\s*sprintf\(StrName,\s*{EFFECTS[effect_id][1]}\);\s*break;", init):
            raise AssertionError(f"{label} Init effect{effect_id} STR mapping changed")
        str_bytes = image.read_cstring_at(str_va)
        if str_bytes is None or str_bytes.raw != filename.encode("ascii"):
            raise AssertionError(f"PE STR bytes at VA {str_va:#x} differ from {filename}")
        for table_name, table in (("catalog", CATALOG), ("STR table", STR_TABLE)):
            if not re.search(rf'\{{\s*{effect_id},\s*"{re.escape(filename)}"', table):
                raise AssertionError(f"local {table_name} effect{effect_id} mapping changed")
        if not re.search(rf"case\s+{effect_id}:\s+return ProcessTier3StrEffect\(\);", DISPATCH):
            raise AssertionError(f"local effect{effect_id} dispatch changed")

if not re.search(r"\$L146677:\s*result\s*=\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*if\s*\(\s*!result\s*\)\s*return result;\s*return 1", pe_dispatch):
    raise AssertionError("PE shared STR tail must propagate completion")
if not re.search(r"\$L146677:\s*result\s*=\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*if\s*\(\s*!result\s*\)\s*return result;\s*return 1", ref_dispatch):
    raise AssertionError("Ref shared STR tail must propagate completion")

local_tier3 = function_body(LOCAL, r"bool CRagEffect::ProcessTier3StrEffect\(\)")
for effect_id, (filename, _, _, wave, _, _) in EFFECTS.items():
    if wave is None:
        expected = r"case\s+569:\s*case\s+603:\s*break;"
        if not re.search(expected, local_tier3):
            raise AssertionError(f"local effect{effect_id} event/STR mapping changed")
    else:
        route = re.search(rf"case\s+{effect_id}:\s*waveFrame = 0;\s*wave = (\"(?:\\.|[^\"\\])*\");\s*break;", local_tier3)
        if route is None or ast.literal_eval(route.group(1)) != wave:
            raise AssertionError(f"local effect{effect_id} event/STR mapping changed")
if not re.search(r"const bool active = ProcessEZ2STR\(0\.0f,\s*-80\.0f,\s*0\.0f\);\s*return propagateStrResult \? active : true;", local_tier3):
    raise AssertionError("local moonlight/defense cases must propagate STR completion")

print("effects565-569 source check passed: four frame-zero cues, silent effect569, shared STR result and five PE-backed STR names")
