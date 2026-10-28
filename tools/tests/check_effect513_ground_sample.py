#!/usr/bin/env python3
"""Compare effect513 GROUNDSAMPLE caller and state-zero emitter to PE/Ref."""
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
    require(re.search(r"case 0x201:\s*CRagEffect::GROUNDSAMPLE\(this\);", source) is not None,
            f"{label} effect513 caller changed")
symbol = SymbolTable(ExeImage()).resolve_name("aEffectMagicTar")
require(symbol is not None and (symbol.va, symbol.raw) == (0x71C9F8, b"effect\\magic_target.tga"),
        "GroundSample texture symbol changed")

pe = body(PE, r"void\s+__thiscall\s+CRagEffect::GROUNDSAMPLE\([^)]*\)(?=\s*\{)")
ref = body(REF2, r"void\s+CRagEffect\s*::\s*GROUNDSAMPLE\s*\([^)]*\)")
local = body(LOCAL, r"void\s+CRagEffect::ProcessGroundSample\s*\(\)")
require(pe is not None and ref is not None and local is not None, "PE/Ref/local GroundSample helper missing")
for token in ("this->m_pos=this->m_master->m_pos", "if(!this->m_stateCnt)",
              "LaunchEffectPrim(this,PP_GROUNDSAMPLE", "m_renderFlag=5", "m_duration=this->m_duration",
              "GetTexture(&g_texMgr,aEffectMagicTar,0)", "i<720", "m_GI[0].life=1",
              "m_GI[0].RotStart=0"):
    require(token in compact(pe), f"PE GroundSample emitter changed: missing {token}")
for token in ("m_effectPos = m_master->m_pos", "if (m_stateCnt != 0)",
              "LaunchEffectPrim(PP_GROUNDSAMPLE", "prim->m_renderFlag = 5",
              "prim->m_duration = m_duration", 'GetTexture("effect\\\\magic_target.tga", false)',
              "prim->m_gi[0].life = 1", "prim->m_gi[0].rotStart = 0"):
    require(token in local, f"local GroundSample emitter changed: missing {token}")
require("m_argb = 0x80FFFFFF" in ref and "m_argb" not in pe and "m_argb" not in local,
        "GroundSample alpha discrepancy changed; PE leaves primitive ARGB default, Ref assigns half-alpha")
route = "case 513: ProcessGroundSample(); return true;"
require(route in DISPATCH and '513: "ProcessGroundSample(); return true;"' in GENERATOR,
        "effect513 generated route mismatch")
print("effect513 GroundSample source comparison passed; PE default ARGB is preserved")
