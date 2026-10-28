#!/usr/bin/env python3
"""Compare the four canonical Bash3D callers with their local wrapper."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


exe_c = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
exe_c = exe_c[exe_c.index("unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)\n{"):]
rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
dispatcher = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
method_match = re.search(r"bool CRagEffect::ProcessBash3DEffect\(\)\s*\{(.*?)\n\}", rag, re.S)
require(method_match is not None, "ProcessBash3DEffect is missing")
method = method_match.group(1)
switch_match = re.search(r"switch \(m_type\)\s*\{([\s\S]*?)\n\s*default:", method)
require(switch_match is not None, "ProcessBash3DEffect caller switch is missing")
switch = switch_match.group(1)

expected = {
    377: ("aEffectMon_1", 5, 8, 2, "effect\\mon_맹룡과강.wav", "0x711E1C"),
    386: ("aEffectHunterDe", 0, 12, 3, "effect\\hunter_detecting.wav", "0x71A428"),
    399: ("aEffect_83", 0, 6, 4, "effect\\헤드 크러쉬.wav", "0x71A45C"),
    400: ("aEffect_98", 22, 6, 5, "effect\\비트 조인트.wav", "0x71A444"),
}
image = ExeImage()
for effect_id, (symbol, wave_state, count, variant, wave, va) in expected.items():
    case_match = re.search(rf"case {effect_id}:([\s\S]*?)(?=\n\s*case |\n\s*default:)", exe_c[exe_c.find("case 377:"):], re.M)
    require(case_match is not None, f"canonical case {effect_id} missing")
    source_case = case_match.group(1)
    require(symbol in source_case, f"canonical case {effect_id} wave symbol changed")
    require(f"this->m_stateCnt == {wave_state}" in source_case if wave_state else "!this->m_stateCnt" in source_case,
            f"canonical case {effect_id} wave gate changed")
    require("CRagEffect::BASH3D(this, aEffectAlphaCen" in source_case,
            f"canonical case {effect_id} Bash3D call missing")
    require(re.search(rf"for \( \w+ = 0; \w+ < {count}; \+\+\w+ \)[\s\S]{{0,100}}BASH3D\(this, aEffectAlphaCen, \w+, {variant}\)", source_case),
            f"canonical case {effect_id} loop count/F2 changed")
    resolved = image.read_cstring_at(int(va, 16))
    require(resolved is not None and resolved.text == wave,
            f"PE wave at {va} did not resolve to {wave!r}")

    route = re.search(rf"^case\s+{effect_id}:\s*(.*)$", dispatcher, re.M)
    require(route is not None and "ProcessBash3DEffect" in route.group(1),
            f"effect {effect_id} dispatcher route missing")
    source_wave = wave.replace("\\", "\\\\")
    local_case_match = re.search(rf"case {effect_id}:([\s\S]*?)(?=\n\s*case |$)", switch)
    require(local_case_match is not None, f"effect {effect_id} implementation branch missing")
    local_case = local_case_match.group(1)
    require(f'wave = "{source_wave}"' in local_case, f"effect {effect_id} implementation wave changed")
    require(f"waveState = {wave_state}" in local_case if wave_state else "waveState = 0" not in local_case,
            f"effect {effect_id} implementation wave frame changed")
    require(f"giCount = {count}" in local_case and f"variant = {variant}" in local_case,
            f"effect {effect_id} count or Bash3D F2 changed")

require('"effect\\\\alpha_center.tga"' in method, "Bash3D texture changed")
require("m_stateCnt >= 5 && m_stateCnt <= 35" in method and "m_BodyLight & 1" in method,
        "effect 377 body-light interval changed")
require("m_stateCnt >= 20 && m_stateCnt <= 50" in method and "m_BodyLight & 8" in method,
        "effects 399/400 body-light interval changed")
require("master->SetArgb(-1, 255, 255, 200)" in method and "master->SetArgb(-1, 255, 255, 255)" in method,
        "effects 399/400 tint changed")
print("Bash3D caller source/PE/current branch checks passed (4 callers, 18 assertions)")
