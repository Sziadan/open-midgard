#!/usr/bin/env python3
"""Compare effect650's composite map magic zone with PE and Ref sources."""
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


caller_pe = body(PE, r"__thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
caller_ref = body(REF, r"unsigned __int8 __thiscall CRagEffect::OnProcess3\(CRagEffect \*this\)")
expected = ("CRagEffect::Map_MagicZone(this, aEffectMjin2Tga, 0);",
            "CRagEffect::Map_MagicZone(this, aEffectMjinTga, 1);",
            "CRagEffect::Map_Pika(this, aEffectPikapika);",
            "CRagEffect::Map_Aura(this, aEffectRingBlue);")
for label, caller in (("PE", caller_pe), ("primary Ref", caller_ref)):
    start = caller.index("case 0x28A:")
    end = caller.index("return 1;", start)
    branch = caller[start:end]
    positions = [branch.find(token) for token in expected]
    require(all(pos >= 0 for pos in positions) and positions == sorted(positions),
            f"{label} effect650 helper call order changed")
require("case 650: return ProcessEffect650();" in DISPATCH
        and '650: "return ProcessEffect650();"' in GENERATOR,
        "effect650 generated route changed")

magic = body(PE, r"void __thiscall CRagEffect::Map_MagicZone\([^)]*\)")
for token in ("this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt )", "PP_MAPMAGICZONE",
              "m_renderFlag = 5", "m_duration = this->m_duration", "m_size = 110.0",
              "m_GI[0].life = 1", "m_GI[1].life = 0", "m_GI[3].life = 0",
              "m_GI[0].distance = 30.0", "m_GI[0].RotStart = 358", "m_GI[0].alphaB = 200",
              "m_GI[0].height[4] = (float)F1"):
    require(token in magic, f"PE Map_MagicZone lacks {token}")
pika = body(PE, r"void __thiscall CRagEffect::Map_Pika\([^)]*\)")
for token in ("this->m_pos = this->m_master->m_pos", "if ( !this->m_stateCnt )", "PP_3DAURA",
              "m_duration = this->m_duration", "m_size = 4.0", "m_GI[0].life = 1",
              "m_GI[1].life = 1", "m_GI[2].life = 0", "m_GI[3].life = 0",
              "m_GI[0].distance = 46.0", "m_GI[1].rise_angle = 180", "m_GI[0].alphaB = 25",
              "m_GI[0].height[0] = -1.0"):
    require(token in pika, f"PE Map_Pika lacks {token}")
aura = body(PE, r"void __thiscall CRagEffect::Map_Aura\([^)]*\)")
for token in ("this->m_pos = this->m_master->m_pos", "if ( !this->m_stateCnt )", "PP_3DCASTING",
              "m_renderFlag |= 1u", "m_renderFlag |= 5u", "m_duration = this->m_duration",
              "m_size = 0.0", "m_GI[0].full_display_angle = 315", "m_GI[0].max_height = 15.0",
              "m_GI[1].max_height = 13.0", "m_GI[2].max_height = 11.0",
              "m_GI[0].distance = 12.9", "m_GI[1].distance = 13.1", "m_GI[2].distance = 13.3",
              "m_GI[0].alphaT = 1", "m_GI[3].life = 0"):
    require(token in aura, f"PE Map_Aura lacks {token}")
require("Map_Aura(char *tName, float size)" in REF2
        and "hardcoded distances12.9/13.1/13.3" in MISMATCHES,
        "secondary Map_Aura variant mismatch must stay documented")

for name, raw, va in (("aEffectMjin2Tga", b"effect\\mjin2.tga", 0x719E00),
                      ("aEffectMjinTga", b"effect\\mjin.tga", 0x719DF0),
                      ("aEffectPikapika", b"effect\\pikapika2.bmp", 0x719DD8),
                      ("aEffectRingBlue", b"effect\\ring_blue.tga", 0x71943C)):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == raw and resolved.va == va,
            f"executable resource {name} changed")

local = body(LOCAL, r"bool CRagEffect::ProcessEffect650\(\)")
for token in ("ResolveCurrentMaster()", "m_effectPos = origin", "m_cachedPos = origin",
              "if (m_stateCnt != 0)", "PP_MAPMAGICZONE", "PP_3DAURA", "PP_3DCASTING",
              r'"effect\\mjin2.tga"', r'"effect\\mjin.tga"', r'"effect\\pikapika2.bmp"',
              r'"effect\\ring_blue.tga"', "prim->m_duration = m_duration", "prim->m_size = 110.0f",
              "gi.distance = 30.0f", "gi.rotStart = variant == 0 ? 0 : 358",
              "gi.height[4] = static_cast<float>(variant)", "gi.riseAngle = static_cast<short>(lane * 180)",
              "distances{12.9f, 13.1f, 13.3f}", "maxHeights{15.0f, 13.0f, 11.0f}",
              "riseAngles{55, 50, 45}"):
    require(token in local, f"local ProcessEffect650 lacks {token}")
require(local.count("refreshPosition();") == 3
        and local.count('spawnMagicZone("effect\\\\mjin') == 2
        and local.index('spawnMagicZone("effect\\\\mjin2.tga", 0)')
        < local.index('spawnMagicZone("effect\\\\mjin.tga", 1)')
        < local.index("LaunchEffectPrim(PP_3DAURA")
        < local.index("LaunchEffectPrim(PP_3DCASTING"),
        "local effect650 must refresh four times and emit prims in source order")
require("rand()" not in local and "PlayWave" not in local and "SendMsg" not in local,
        "effect650 has no RNG, audio or messages")
print("effect650 Map_MagicZone source and resource checks passed")
