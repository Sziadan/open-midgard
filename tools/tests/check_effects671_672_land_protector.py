#!/usr/bin/env python3
"""Compare effects 671/672 Land Protector callers and PW=3 helper behavior."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
COVERAGE = (ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv").read_text(encoding="utf-8")
PARITY_LOG = (ROOT / "docs/skill/PARITY_LOG.md").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


cases = ((671, "0x29F", "aEffectHanmoon1", b"effect\\hanmoon1.tga", 3),
         (672, "0x2A0", "aEffectHanmoon2", b"effect\\hanmoon2.tga", 4))
for effect_id, hex_id, resource, raw, variant in cases:
    require(re.search(rf"case {hex_id}:\s*CRagEffect::Bottom_LandProtector\(this,\s*{resource},\s*3\);", PE),
            f"PE OnProcess3 effect {effect_id} caller mapping changed")
    require(re.search(rf"case {hex_id}:\s*CRagEffect::Bottom_LandProtector\(this,\s*{resource},\s*3\);", REF1),
            f"primary Ref OnProcess3 effect {effect_id} caller mapping changed")
    resolved = symbols.resolve_name(resource)
    require(resolved is not None and resolved.raw == raw,
            f"{resource} executable bytes do not resolve to {raw!r}")
    route = f"case {effect_id}: return ProcessLandProtector({variant});"
    require(route in DISPATCH and f'{effect_id}: "return ProcessLandProtector({variant});"' in GENERATOR,
            f"effect {effect_id} generated/runtime route mismatch")
    coverage = next((line.split("\t") for line in COVERAGE.splitlines()
                     if line.startswith(f"{effect_id}\t")), None)
    require(coverage is not None and coverage[3] == "routed",
            f"effect {effect_id} coverage row is not routed")
    parity_line = int(coverage[6])
    parity_lines = PARITY_LOG.splitlines()
    require(parity_line > 0 and parity_line <= len(parity_lines)
            and "Case list:" in parity_lines[parity_line - 1]
            and re.search(rf"(?<!\d){effect_id}(?!\d)", parity_lines[parity_line - 1]),
            f"effect {effect_id} coverage does not point to its case-list evidence")

pe_helper = re.search(r"void __thiscall CRagEffect::Bottom_LandProtector\([^)]*\)\s*\{(.*?)\n\}", PE, re.S)
ref_helper = re.search(r"void CRagEffect\s*::\s*Bottom_LandProtector\([^)]*\)\s*\{(.*?)\n\}", REF2, re.S)
require(pe_helper is not None and ref_helper is not None, "PE/secondary Ref shared helper missing")
for label, body in (("PE", pe_helper.group(1)), ("secondary Ref", ref_helper.group(1))):
    for token in ("m_master->m_pos", "m_stateCnt", "PP_LANDPROTECTOR",
                  "m_duration", "m_texture", "life = 1", "life = 0", "distance = 10.0",
                  "RotStart = 225", "alphaB = 250", "rise_angle", "height[0]", "height[4]"):
        require(token in body, f"{label} PW=3 helper lacks {token}")
    require("m_size = 111.0" in body if label == "PE" else "m_size=111" in body,
            f"{label} PW=3 size changed")
    require("m_renderFlag = 5" in body if label == "PE" else "RF_EFFECT" in body,
            f"{label} render flag changed")
    require(body.index("m_master->m_pos") < body.index("m_stateCnt") < body.index("PP_LANDPROTECTOR"),
            f"{label} helper master/state/emission order changed")
    require("rand() % 360" in body or "random(360)" in body,
            f"{label} helper random angle expression changed")

local_helper = re.search(r"bool CRagEffect::ProcessLandProtector\(int variant\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local_helper is not None, "local shared Land Protector helper missing")
body = local_helper.group(1)
ordered = ("CRenderObject* master = m_master", "m_effectPos = master->m_pos", "m_stateCnt != 0",
           "LaunchEffectPrim(PP_LANDPROTECTOR", "m_renderFlag = 5", "prim->m_duration = m_duration",
           '"effect\\\\hanmoon1.tga"', '"effect\\\\hanmoon2.tga"',
           "gi.life = 1", "lane = 1", "gi.distance =", "gi.rotStart =", "gi.alphaB =",
           "rand() % 360", "gi.height[0] = -2.0f", "gi.height[4]", "m_size =")
pos = -1
for token in ordered:
    current = body.find(token, pos + 1)
    require(current >= 0, f"local handler missing or reordered operation: {token}")
    pos = current
require("profile == 2 ? 12.0f : 10.0f" in body and "profile == 3 ? 225 : 0" in body
        and "profile == 3 ? 250 : 100" in body and "profile == 3 ? 111.0f : 5.0f" in body,
        "local PW=3 profile constants changed")
require("master == nullptr" not in body and "prim == nullptr" not in body,
        "local helper added guards absent from the executable helper")
require(not any(token in body for token in ("PlayWave", "TryPlay", "SendMsg")),
        "Land Protector added an unsupported side effect")
print("effects671_672_land_protector source parity passed: caller IDs 0x29F/0x2A0, exact PE assets, ordered PW=3 helper profile")
