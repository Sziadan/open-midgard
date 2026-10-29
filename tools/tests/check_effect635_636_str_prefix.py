#!/usr/bin/env python3
"""Compare recoverable effect635/636 frame-zero audio and STR behavior."""
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
    for effect_id, case_id, symbol in ((635, "0x27B", "aEffect_91"), (636, "0x27C", "aEffect_40")):
        following = "0x27C" if effect_id == 635 else "0x27D"
        branch = re.search(rf"case\s+{case_id}:(.*?)(?=case\s+{following}:)", dispatch, re.S)
        if branch is None:
            raise AssertionError(f"{label} effect{effect_id} branch is missing")
        text = branch.group(1)
        ordered = ["if ( !this->m_stateCnt )", "this->m_pos = this->m_master->m_pos",
                   "m_world->m_player->m_pos", f"PlayWave({symbol}", "0.0, z", "250, 40, 1.0", "goto $L146219"]
        positions = [text.find(token) for token in ordered]
        if any(position < 0 for position in positions) or positions != sorted(positions):
            raise AssertionError(f"{label} effect{effect_id} frame-zero copy/wave/shared-tail order changed")
        if "rand(" in text or "SendMsg" in text:
            raise AssertionError(f"{label} effect{effect_id} must not consume RNG or send a message")

for label, init_source in (("PE", PE), ("Ref", REF)):
    init = function_body(init_source, r"__thiscall CRagEffect::Init\(CRagEffect \*this, CRenderObject \*master, int effectId, vector3d fDeltaPos\)", last=label == "PE")
    for effect_id, asset_symbol in ((635, "aFireDragonStr"), (636, "aIcyStr")):
        if not re.search(rf"case\s+{effect_id}:\s*sprintf\(StrName,\s*{asset_symbol}\)", init):
            raise AssertionError(f"{label} Init effect{effect_id} STR mapping changed")

image = ExeImage()
for va, expected in ((0x6F1FF0, b"effect\\"), (0x71A8A8, b"fire dragon.str"), (0x71A884, b"icy.str")):
    resolved = image.read_cstring_at(va)
    if resolved is None or resolved.raw != expected:
        raise AssertionError(f"PE bytes at VA {va:#x} do not match {expected!r}")
for effect_id, filename in ((635, "fire dragon.str"), (636, "icy.str")):
    for label, table in (("catalog", CATALOG), ("STR table", STR_TABLE)):
        if not re.search(rf'\{{\s*{effect_id},\s*"{re.escape(filename)}"', table):
            raise AssertionError(f"local {label} effect{effect_id} mapping changed")
    if not re.search(rf"case\s+{effect_id}:\s+return ProcessTier3StrEffect\(\);", DISPATCH):
        raise AssertionError(f"local effect{effect_id} dispatch changed")

local_tier3 = function_body(LOCAL, r"bool CRagEffect::ProcessTier3StrEffect\(\)")
for effect_id in (635, 636):
    if not re.search(rf"case\s+{effect_id}:\s*waveFrame = 0;\s*wave = \"effect\\\\\";\s*propagateStrResult = false;\s*break;", local_tier3):
        raise AssertionError(f"local effect{effect_id} must preserve only the proven prefix and ignore STR completion")
if not re.search(r"\$L146219:\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*return 1", PE):
    raise AssertionError("PE shared STR tail must ignore completion and return true")
if not re.search(r"\$L146219:\s*CRagEffect::ProcessEZ2STR\(this,\s*0\.0,\s*-80\.0,\s*0\.0\);\s*return 1", REF):
    raise AssertionError("Ref shared STR tail must ignore completion and return true")

print("effects635/636 source check passed: frame-zero cues, STR continuation and filenames verified; WAV suffixes remain blocked")
