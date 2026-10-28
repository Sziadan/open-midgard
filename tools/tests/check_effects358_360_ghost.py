#!/usr/bin/env python3
"""Compare Ghost effects 358-360 against primary sources and PE resources."""
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
DISPATCH = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
MISMATCHES = (ROOT / "docs/skill/MISMATCHES.md").read_text(encoding="utf-8")
PARITY_LOG = (ROOT / "docs/skill/PARITY_LOG.md").read_text(encoding="utf-8")
COVERAGE = (ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


for effect_id, case_id, variant in ((358, "0x166", 0), (359, "0x167", 1), (360, "0x168", 2)):
    route = f"case {case_id}:\n      CRagEffect::Ghost(this, {variant});"
    require(route in PE and route in REF, f"effect {effect_id} PE/primary Ref caller changed")
    generated = f"case {effect_id}: return ProcessGhost({variant});"
    require(generated in DISPATCH and f'{effect_id}: "return ProcessGhost({variant});"' in GENERATOR,
            f"effect {effect_id} runtime/generator route changed")
    coverage = next((line.split("\t") for line in COVERAGE.splitlines()
                     if line.startswith(f"{effect_id}\t")), None)
    require(coverage is not None and coverage[3] == "routed",
            f"effect {effect_id} coverage row is not routed")
    parity_lines = PARITY_LOG.splitlines()
    parity_line = int(coverage[6])
    require(0 < parity_line <= len(parity_lines)
            and "Case list:" in parity_lines[parity_line - 1]
            and re.search(rf"(?<!\d){effect_id}(?!\d)", parity_lines[parity_line - 1]),
            f"effect {effect_id} coverage does not point to its exact case-list evidence")

pe_helper = re.search(r"void __thiscall CRagEffect::Ghost\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void\s+CRagEffect\s*::\s*Ghost\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(pe_helper is not None and ref_helper is not None, "PE/secondary Ref Ghost helper missing")
for label, body in (("PE", pe_helper.group(1)), ("secondary Ref", ref_helper.group(1))):
    for token in ("m_stateCnt < 10", "PP_GHOST", "m_duration", "m_size",
                  "flag1[0]", "height[1]", "flag1[2]", "distance", "max_height",
                  "m_alpha", "m_animSpeed", "InitSegment"):
        require(token in body, f"{label} Ghost helper lacks {token}")
    require(body.index("m_master->m_pos") < body.index("m_stateCnt < 10") < body.index("PP_GHOST"),
            f"{label} Ghost position/gate/launch order changed")

require("byte_71C01C" in pe_helper.group(1) and "byte_71C00C" in pe_helper.group(1)
        and "byte_71BFFC" in pe_helper.group(1) and "byte_71BFEC" in pe_helper.group(1),
        "canonical PE Ghost resource symbol mapping changed")
assets = (("byte_71C01C", b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC0\xAF\xB7\xC9.spr"),
         ("byte_71C00C", b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xC0\xAF\xB7\xC9.act"),
         ("byte_71BFFC", b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xB9\xDA\xC1\xE3.spr"),
         ("byte_71BFEC", b"\xC0\xCC\xC6\xD1\xC6\xAE\\\xB9\xDA\xC1\xE3.act"))
for name, expected in assets:
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected, f"{name} PE bytes changed")

local = re.search(r"bool CRagEffect::ProcessGhost\(int variant\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local is not None, "local ProcessGhost handler missing")
body = local.group(1)
for token in ("m_effectPos = master->m_pos", "m_stateCnt < 10", "LaunchEffectPrim(PP_GHOST",
              "gi.process = 0", "gi.life = 1", "prim->m_size = 1.5f", "gi.sparkX = m_effectPos.x",
              "gi.flag[0] = 2", "gi.flag[0] = 1", "gi.flag[2]", "gi.distance = 16.0f",
              "gi.maxHeight = 4.0f", "prim->m_alpha = 0.0f", "prim->m_animSpeed = 2",
              "prim->m_renderFlag |= 0x200", "m_spriteMotionBase = 16", "m_repeatAnim = false",
              "m_numSegments = 1", "InitReferenceSegments(prim)", "lane = 1"):
    require(token in body, f"local Ghost helper lacks {token}")
require(body.index("m_effectPos = master->m_pos") < body.index("m_stateCnt < 10")
        < body.index("LaunchEffectPrim(PP_GHOST"), "local Ghost position/gate/launch order changed")
require('"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xC0\\xAF\\xB7\\xC9.spr"' in body
        and '"\\xC0\\xCC\\xC6\\xD1\\xC6\\xAE\\\\\\xB9\\xDA\\xC1\\xE3.spr"' in body,
        "local Ghost asset paths do not match the canonical PE bytes")
require("misc\\\\ghost.spr" in REF2 and "misc\\\\bat.spr" in REF2
        and "ghost effects 358" in MISMATCHES.lower(),
        "secondary Ref asset mismatch is not documented")
print("effects 358-360 Ghost source and asset checks passed")
