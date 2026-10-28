"""Compare effect403's inline PE behavior with the explicit local port."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(f"unterminated function {signature}")


pe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_text = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
skill = (ROOT / "src/gamemode/UseSkillEffect.generated.inc").read_text(encoding="utf-8")

pe = function(pe_text, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case_start = pe.index("case 403:")
case_end = pe.index("case 404:", case_start)
case = pe[case_start:case_end]
for token in ("m_stateCnt == 25", "this->m_pos = (*p_m_master)->m_pos", "PlayWave(aEffectSwordman",
              "m_stateCnt > 0x3Cu", "LABEL_218:"):
    if token not in case:
        raise SystemExit(f"canonical PE effect403 inline case missing {token!r}")
label_start = pe.index("LABEL_218:", case_start)
label_end = pe.index("case 404:", label_start)
random_tail = pe[label_start:label_end]
if len(re.findall(r"\brand\s*\(", random_tail)) != 3:
    raise SystemExit("canonical effect403 tint must consume three random draws")
for token in ("v47 = rand() % 255", "g = rand() % 255", "v48 = rand()",
              "SetArgb(*p_m_master, -1, v48 % 255, g, v47)"):
    if token not in random_tail:
        raise SystemExit(f"canonical PE effect403 tint order changed: {token}")
if "case 403:" in ref_text:
    raise SystemExit("primary Ref unexpectedly contains an effect403 case")
if not re.search(r"case 0x192:[\s\S]{0,800}?LaunchOn\(target,\s*403\)", skill):
    raise SystemExit("skill 402 target caller no longer launches effect403")

local = function(source, "bool CRagEffect::ProcessEffect403()")
if "bool ProcessEffect403();" not in header or "case 403: return ProcessEffect403();" not in dispatch:
    raise SystemExit("local effect403 declaration or generated route missing")
ordered = (
    "if (m_stateCnt == 25)", "m_effectPos = master->m_pos", '"effect\\\\swordman_provoke.wav"',
    "if (m_stateCnt > 60)", "const int blue = rand() % 255", "const int green = rand() % 255",
    "const int red = rand() % 255", "SetArgb(-1, red, green, blue)",
)
cursor = 0
for token in ordered:
    pos = local.find(token, cursor)
    if pos < 0:
        raise SystemExit(f"local effect403 behavior missing or out of order: {token}")
    cursor = pos + len(token)
if len(re.findall(r"\brand\s*\(", local)) != 3:
    raise SystemExit("local effect403 must preserve exactly three tint draws")

wave = SymbolTable(ExeImage(ROOT / "HighPriest.exe")).resolve_name("aEffectSwordman")
if wave is None or wave.va != 0x71A5E4 or wave.raw != b"effect\\swordman_provoke.wav":
    raise SystemExit(f"effect403's PE audio resource changed: {wave!r}")
if '"effect\\\\swordman_provoke.wav"' not in local:
    raise SystemExit("local effect403 PlayWave argument differs from canonical resource")
print("effect403 parity: caller, inline routing, exact wave, state25 audio, inclusive RNG tint passed")
