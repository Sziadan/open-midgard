#!/usr/bin/env python3
"""Compare WaterFall effect IDs 349-356 to PE/Ref callers and resources."""
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
PARITY_LOG = (ROOT / "docs/skill/PARITY_LOG.md").read_text(encoding="utf-8")
COVERAGE = (ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


variants = ((349, "0x15D", 0, 0, 0), (350, "0x15E", 1, 0, 0),
            (351, "0x15F", 0, 1, 0), (352, "0x160", 1, 1, 0),
            (353, "0x161", 0, 0, 1), (354, "0x162", 1, 0, 1),
            (355, "0x163", 0, 1, 1), (356, "0x164", 1, 1, 1))


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


def extract_function(source: str, pattern: str) -> str | None:
    matches = list(re.finditer(pattern, source))
    if not matches:
        return None
    start = matches[-1].start()
    opening = source.find("{", matches[-1].end())
    if opening < 0:
        return None
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return None


for effect_id, case_id, axis, compact_mode, texture_set in variants:
    particle_count = 40 if compact_mode == 0 else 20
    caller = (f"case{case_id}:CRagEffect::WaterFall(this,&Buf,{axis},{compact_mode},{texture_set});"
              f"v{effect_id-344}={particle_count};do{{CRagEffect::WaterFallParticle(this,{axis},{compact_mode});"
              f"--v{effect_id-344};}}while(v{effect_id-344});return1;")
    # The decompiler's temporary local number differs by effect; compare the exact call/order/count skeleton.
    for label, source in (("PE", PE), ("primary Ref", REF)):
        starts = [m for m in re.finditer(rf"case\s+{case_id}:", source)
                  if "CRagEffect::WaterFall(this" in source[m.start():m.start() + 180]
                  and "WaterFallParticle(this" in source[m.start():m.start() + 500]]
        require(bool(starts), f"{label} caller for effect {effect_id} missing")
        start = starts[0].start()
        next_case = re.search(r"case\s+0x", source[start + 1:])
        body = compact(source[start:start + 1 + next_case.start()] if next_case else source[start:])
        expected = compact(caller)
        require(body.startswith(expected), f"{label} effect {effect_id} WaterFall/particle tuple, count, or order changed")
    generated = f"case {effect_id}: return ProcessWaterfallEffects({axis}, {compact_mode}, {texture_set});"
    require(generated in DISPATCH
            and f'{effect_id}: "return ProcessWaterfallEffects({axis}, {compact_mode}, {texture_set});"' in GENERATOR,
            f"effect {effect_id} runtime/generator route changed")
    coverage = next((line.split("\t") for line in COVERAGE.splitlines()
                     if line.startswith(f"{effect_id}\t")), None)
    require(coverage is not None and coverage[3] == "routed", f"effect {effect_id} coverage is not routed")
    parity_lines = PARITY_LOG.splitlines()
    line_no = int(coverage[6])
    require(0 < line_no <= len(parity_lines) and "Case list:" in parity_lines[line_no - 1]
            and re.search(rf"(?<!\d){effect_id}(?!\d)", parity_lines[line_no - 1]),
            f"effect {effect_id} coverage does not point at its exact case-list evidence")

waterfall = extract_function(PE, r"void __thiscall CRagEffect::WaterFall\(")
waterfall_ref = extract_function(REF2, r"void\s+CRagEffect\s*::\s*WaterFall\(")
particle = extract_function(PE, r"void __thiscall CRagEffect::WaterFallParticle\(")
particle_ref = extract_function(REF2, r"void\s+CRagEffect\s*::\s*WaterFallParticle\(")
require(all(x is not None for x in (waterfall, waterfall_ref, particle, particle_ref)),
        "PE/secondary Ref shared WaterFall helper(s) missing")
for label, body in (("PE WaterFall", waterfall), ("Ref WaterFall", waterfall_ref)):
    for token in ("m_stateCnt", "PP_WATERFALL", "m_duration", "m_totalTexture",
                  "alphaB", "flag1[0]", "distance"):
        require(token in body, f"{label} lacks {token}")
require("aEffectWaterfal" in waterfall and "waterfall11.tga" in waterfall_ref
        and "waterfall31.tga" in waterfall_ref and "height" in waterfall_ref
        and "max_height" in waterfall_ref and "m_longAccel" in waterfall
        and "v8 -= 13" in waterfall and "v7 + 36" in waterfall and "v7 + 18" in waterfall,
        "WaterFall texture-set/height selection changed")
for label, body in (("PE WaterFallParticle", particle), ("Ref WaterFallParticle", particle_ref)):
    for token in ("m_stateCnt", "m_master->m_pos", "PP_WATERFALLPARTICLE", "rand", "m_size", "max_height", "flag1[11]", "flag1[12]",
                  "vecB_pre", "vecB_now", "vecT_pre", "vecT_now", "flag1[0]"):
        require(token in body, f"{label} lacks {token}")
    require(body.index("m_stateCnt") < body.index("m_master->m_pos") < body.index("PP_WATERFALLPARTICLE"),
            f"{label} state/position/emission order changed")
require("aEffectWaterp1T" in particle and "aEffectFreezeAS" in particle
        and "waterp1.tga" in particle_ref and "freeze_a_small.bmp" in particle_ref,
        "WaterFallParticle texture chooser changed")

assets = (("aEffectWaterfal", b"effect\\waterfall11.tga"),
         ("aEffectWaterfal_0", b"effect\\waterfall12.tga"),
         ("aEffectWaterfal_1", b"effect\\waterfall13.tga"),
         ("aEffectWaterfal_2", b"effect\\waterfall31.tga"),
         ("aEffectWaterfal_3", b"effect\\waterfall32.tga"),
         ("aEffectWaterfal_4", b"effect\\waterfall33.tga"),
         ("aEffectWaterp1T", b"effect\\waterp1.tga"),
         ("aEffectFreezeAS", b"effect\\freeze_a_small.bmp"))
for name, expected in assets:
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected, f"{name} executable bytes changed")

local = re.search(r"bool CRagEffect::ProcessWaterfallEffects\(int axisMode, int compactMode, int textureSet\)\s*\{(.*?)\n\}", LOCAL, re.S)
require(local is not None, "local ProcessWaterfallEffects body missing")
body = local.group(1)
for token in ("m_stateCnt == 0", "m_effectPos = m_master->m_pos", "LaunchEffectPrim(PP_WATERFALL",
              "m_renderFlag = 5", "m_duration = m_duration", "waterfall11.tga", "waterfall31.tga",
              "alphaB = 80", "80 - bandIndex * 13", "bandIndex - 1", "particleCount = compactMode == 0 ? 40 : 20",
              "rand() % 10 <= 0", "waterp1.tga", "freeze_a_small.bmp", "m_size = 4.0f",
              "0.050000001f", "gi.flag[11]", "gi.flag[12]", "rand() % 45 - 22", "rand() % 23 - 11",
              "gi.flag[0] = 2"):
    require(token in body, f"local waterfall handler lacks {token}")
print("effects 349-356 WaterFall source and asset checks passed")
