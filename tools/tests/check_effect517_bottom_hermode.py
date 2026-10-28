#!/usr/bin/env python3
"""Compare effect517 Bottom_Hermode caller and ordered emitter profile."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF1 = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def body(source: str, signature: str) -> str | None:
    match = re.search(signature, source)
    if not match:
        return None
    opening = source.find("{", match.end())
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    return None


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


for source, label in ((PE, "PE"), (REF1, "primary Ref")):
    require(re.search(r"case 0x205:\s*CRagEffect::Bottom_Hermode\(this, aEffectWhite02B\);", source)
            is not None, f"{label} effect517 caller changed")
symbol = SymbolTable(ExeImage()).resolve_name("aEffectWhite02B")
require(symbol is not None and (symbol.va, symbol.raw) == (0x713CC8, b"effect\\white02.bmp"),
        "effect517 bitmap symbol changed")
pe = body(PE, r"void\s+__thiscall\s+CRagEffect::Bottom_Hermode\([^)]*\)(?=\s*\{)")
ref = body(REF2, r"void\s+CRagEffect\s*::\s*Bottom_Hermode\s*\([^)]*\)")
local = body(LOCAL, r"bool\s+CRagEffect::ProcessBottomHermode517\s*\(\)")
require(pe is not None and ref is not None and local is not None, "PE/Ref/local Bottom_Hermode helper missing")
for token in ("this->m_pos=this->m_master->m_pos", "if(!this->m_stateCnt)",
              "LaunchEffectPrim(this,PP_HERMODE", "m_renderFlag=5", "m_size=5.0",
              "m_GI[0].distance=1.6", "m_GI[0].rise_angle=rand()%360",
              "m_GI[0].RotStart=rand()%360", "m_GI[0].flag1[0]=rand()%31"):
    require(token in compact(pe), f"PE Bottom_Hermode profile changed: missing {token}")
for token in ("m_effectPos = master->m_pos", "if (m_stateCnt != 0)",
              "LaunchEffectPrim(PP_HERMODE", "prim->m_renderFlag = 5", "prim->m_duration = m_duration",
              'GetTexture("effect\\\\white02.bmp", false)', "prim->m_size = 5.0f",
              "prim->m_gi[0].distance = 1.6f"):
    require(token in local, f"local Bottom_Hermode payload changed: missing {token}")
draws = [local.find("rand() % 360"), local.find("rand() % 360", local.find("rand() % 360") + 1),
         local.find("rand() % 31")]
require(all(draw >= 0 for draw in draws) and draws == sorted(draws),
        "local Bottom_Hermode random draws no longer follow rise, rotation, flag order")
require("case 517: return ProcessBottomHermode517();" in DISPATCH
        and '517: "return ProcessBottomHermode517();"' in GENERATOR,
        "effect517 generated route mismatch")
print("effect517 Bottom_Hermode source comparison passed; exact asset and RNG order agree")
