#!/usr/bin/env python3
"""Compare Portal3 effects 561/562 to executable and Ref sources."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
COVERAGE = (ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv").read_text(encoding="utf-8")
PARITY = (ROOT / "docs/skill/PARITY_LOG.md").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


for effect_id, variant, case_id, ref_line in ((561, 0, "0x231", 11829), (562, 1, "0x232", 11832)):
    caller = f"case {case_id}:\n      CRagEffect::Portal3(this, {variant});"
    require(caller in PE and caller in REF,
            f"effect {effect_id} PE/primary Ref caller mapping changed near Ref line {ref_line}")
    require(f"case {effect_id}: return ProcessPortal3({variant});" in DISPATCH
            and f'{effect_id}: "return ProcessPortal3({variant});"' in GENERATOR,
            f"effect {effect_id} generated route changed")
    row = next((line.split("\t") for line in COVERAGE.splitlines()
                if line.startswith(f"{effect_id}\t")), None)
    require(row is not None and row[3] == "routed", f"effect {effect_id} is not routed")
    lines = PARITY.splitlines()
    evidence_line = int(row[6])
    require(0 < evidence_line <= len(lines) and "Case list:" in lines[evidence_line - 1]
            and re.search(rf"(?<!\d){effect_id}(?!\d)", lines[evidence_line - 1]),
            f"effect {effect_id} coverage does not point to exact case-list evidence")

pe = re.search(r"void __thiscall CRagEffect::Portal3\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref2 = re.search(r"void\s+CRagEffect\s*::\s*Portal3\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(pe is not None and ref2 is not None, "PE/secondary Ref Portal3 helper missing")
pe_body, ref_body = pe.group(1), ref2.group(1)
for label, body in (("PE", pe_body), ("secondary Ref", ref_body)):
    for token in ("PP_HEAL", "m_stateCnt == 2", "PP_WIND", "m_renderFlag", "m_duration",
                  "max_height", "RotStart", "alphaB", "alphaT", "distance", "rise_angle"):
        require(token in body, f"{label} Portal3 helper lacks {token}")
    state_zero = body.find("m_stateCnt == 0") if "m_stateCnt == 0" in body else body.find("!this->m_stateCnt")
    state_two = body.find("m_stateCnt == 2") if "m_stateCnt == 2" in body else body.find("this->m_stateCnt == 2")
    require(state_zero >= 0 and state_two >= 0 and state_zero < body.index("PP_HEAL")
            < state_two < body.index("PP_WIND"),
            f"{label} Portal3 launch gates/order changed")
require(pe_body.count("rand() % 360") == 3, "canonical Portal3 must consume three state-zero RNG draws")
require("1069547520" in pe_body and "1.5f" in ref_body,
        "Portal3 wind height source immediate/reference value changed")

for name, expected in (("aEffectMagicVio", b"effect\\magic_violet.tga"),
                       ("aEffectRingRedT", b"effect\\ring_red.tga"),
                       ("aEffectCloud11T", b"effect\\cloud11.tga")):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected,
            f"{name} executable resource bytes changed")

local = re.search(r"bool CRagEffect::ProcessPortal3\(int variant\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local is not None, "local ProcessPortal3 handler missing")
body = local.group(1)
for token in ("if (m_stateCnt == 0)", "PP_HEAL", r"effect\\ring_red.tga",
              r"effect\\magic_violet.tga", "rand() % 360", "gi.sparkY = 0.0f",
              "gi.height.fill(0.0f)", "gi.flag.fill(3)", "heal->m_gi[3].life = 0",
              "if (m_stateCnt == 2)", "PP_WIND", r"effect\\cloud11.tga",
              "gi.height.fill(1.5f)", "gi.flag.fill(0)"):
    require(token in body, f"local Portal3 handler lacks {token}")
state_zero = body.index("m_stateCnt == 0")
state_two = body.index("m_stateCnt == 2")
require(body.count("rand() % 360") == 1 and state_zero < body.index("PP_HEAL")
        < state_two < body.index("PP_WIND"),
        "local Portal3 must preserve three lane draws and independent launch states")
print("effects 561/562 Portal3 source and asset checks passed")
