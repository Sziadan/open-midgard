#!/usr/bin/env python3
"""Compare the effect651 Map_Particle sequence against the canonical PE."""
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
MISMATCHES = (ROOT / "docs/skill/MISMATCHES.md").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def body(text: str, signature: str) -> str:
    matches = list(re.finditer(signature, text))
    require(bool(matches), f"missing source function: {signature}")
    start = text.find("{", matches[-1].end())
    depth = 0
    for pos in range(start, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:pos]
    raise SystemExit(f"unterminated function: {signature}")


pe_caller = body(PE, r"__thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
ref_caller = body(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
for label, caller in (("PE", pe_caller), ("primary Ref", ref_caller)):
    start = caller.index("case 0x28B:")
    end = caller.index("return 1;", start)
    branch = caller[start:end]
    require(branch.index("Map_MagicZone(this, aEffectMjin2Tga, 2)")
            < branch.index("Map_MagicZone(this, aEffectMjinTga, 3)")
            < branch.index("for ( k = 2; k < 21; k += 2 )")
            < branch.index("Map_Particle(this, aEffectMpa3Tga, k, 0)"),
            f"{label} effect651 caller order changed")
    require("k < 21" in branch and "k += 2" in branch,
            f"{label} effect651 particle caller range changed")
require("case 651: return ProcessEffect651();" in DISPATCH
        and '651: "return ProcessEffect651();"' in GENERATOR,
        "effect651 generated route changed")

magic = body(PE, r"void __thiscall CRagEffect::Map_MagicZone\([^)]*\)")
for token in ("this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt )",
              "PP_MAPMAGICZONE", "m_renderFlag = 5", "m_duration = this->m_duration",
              "m_size = 110.0", "m_GI[0].distance = 40.0", "m_GI[0].RotStart = 0",
              "m_GI[0].alphaB = 200", "m_GI[0].height[4] = (float)F1"):
    require(token in magic, f"PE Map_MagicZone lacks {token}")

particle = body(PE, r"void __thiscall CRagEffect::Map_Particle\([^)]*\)")
particle_ops = ["this->m_pos = this->m_master->m_pos", "if ( !this->m_stateCnt )",
                "PP_MAPPARTICLE", "m_renderFlag = 5", "m_duration = this->m_duration",
                "F1a = -(double)(2 * F1)", "m_size = 0.0"]
positions = [particle.find(item) for item in particle_ops]
require(all(pos >= 0 for pos in positions) and positions == sorted(positions),
        "PE Map_Particle setup/gate order changed")
rand_draws = list(re.finditer(r"rand\(\)\s*%\s*360", particle))
require(len(rand_draws) == 8, f"PE Map_Particle expects 8 rand draws/lane, found {len(rand_draws)}")
ordered_fields = ["height[0]", "height[1]", "height[2]", "height[3]", "vecB_pre.y",
                  "height[4]", "vecB_now.y", "height[5]", "vecT_pre.y", "height[6]",
                  "vecT_now.y", "height[7]", "flag1[0]", "flag1[1]"]
field_positions = [particle.find(field) for field in ordered_fields]
require(all(pos >= 0 for pos in field_positions) and field_positions == sorted(field_positions),
        "PE Map_Particle GI write order changed")
require("F2 == 2" in particle and "flag1[1] = 0" in particle,
        "PE Map_Particle F2 flag branch changed")
require("RF_EFFECT_OM" in REF2 and "Map_Particle" in REF2,
        "secondary Ref render-mode discrepancy evidence missing")
require("effect651" in MISMATCHES and "render flag5" in MISMATCHES,
        "PE/secondary Ref render-mode mismatch must be documented")

for name, raw, va in (("aEffectMjin2Tga", b"effect\\mjin2.tga", 0x719E00),
                      ("aEffectMjinTga", b"effect\\mjin.tga", 0x719DF0),
                      ("aEffectMpa3Tga", b"effect\\mpa3.tga", 0x719E14)):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == raw and resolved.va == va,
            f"executable resource {name} changed")

local = body(LOCAL, r"bool CRagEffect::ProcessEffect651\(\)")
for token in ("ResolveCurrentMaster()", "m_effectPos = origin", "m_cachedPos = origin",
              "PP_MAPMAGICZONE", "PP_MAPPARTICLE", '"effect\\\\mjin2.tga"',
              '"effect\\\\mjin.tga"', '"effect\\\\mpa3.tga"',
              "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
              "gi.distance = 40.0f", "variant == 2 ? 0 : 358",
              "variant == 2 ? 200 : 50", "variant == 2 ? -0.5f : -0.40000001f",
              "gi.height[4] = static_cast<float>(variant)",
              "gi.maxHeight = 0.15000001f", "gi.distance = 1.0f",
              "gi.flag[0] = 1", "gi.flag[1] = 0"):
    require(token in local, f"local ProcessEffect651 lacks {token}")
require("for (int variant = 2; variant < 21; variant += 2)" in local
        and "static_cast<float>(-(2 * variant))" in local,
        "local effect651 variant range or baseline differs from PE")
local_draws = list(re.finditer(r"std::rand\(\)\s*%\s*360", local))
require(len(local_draws) == 8, f"local Map_Particle must draw 8 values/lane, found {len(local_draws)}")
require(10 * 4 * len(local_draws) == 320,
        "effect651 must consume exactly 320 state-zero random draws across its ten particles")
local_particle = local[local.index("LaunchEffectPrim(PP_MAPPARTICLE"):]
local_fields = ["gi.height[0]", "gi.height[1]", "gi.height[2]", "gi.height[3]",
                "gi.arcBaselineY = baselineY", "gi.height[4]", "gi.sparkY = baselineY",
                "gi.height[5]", "gi.vecTPreY = baselineY", "gi.height[6]",
                "gi.vecTNowY = baselineY", "gi.height[7]", "gi.flag[0]", "gi.flag[1]"]
local_positions = [local_particle.find(field) for field in local_fields]
require(all(pos >= 0 for pos in local_positions) and local_positions == sorted(local_positions),
        "local Map_Particle GI write order differs from PE")
require(local.count("refreshPosition();") == 2
        and local.count("if (m_stateCnt != 0)") == 2
        and local.index('spawnMagicZone("effect\\\\mjin2.tga", 2)')
        < local.index('spawnMagicZone("effect\\\\mjin.tga", 3)')
        < local.index("for (int variant = 2; variant < 21; variant += 2)")
        < local.index("LaunchEffectPrim(PP_MAPPARTICLE"),
        "local effect651 must refresh/gate each helper and emit in source order")
require("rand()" not in local.replace("std::rand()", "")
        and "PlayWave" not in local and "SendMsg" not in local,
        "effect651 must have only the eight PE RNG draws per lane and no audio/messages")
print("effect651 caller, Map_MagicZone/Map_Particle, route, resources, order and mismatch checks passed")
