"""Compare PE/Ref Detecting and verify local effect119 route and texture family."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage, SymbolTable

pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")


def fn(text: str, sig: str) -> str:
    start = text.rindex(sig)
    opening = text.index("{", start)
    depth = 0
    for i in range(opening, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0: return text[start:i + 1]
    raise AssertionError(sig)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


signature = "void __thiscall CRagEffect::Detecting(CRagEffect *this)"
if compact(fn(pe, signature)) != compact(fn(ref, signature)):
    raise SystemExit("Detecting PE/primary Ref helper mismatch")
main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case_starts = [match.start() for match in re.finditer(r"case 119:", main)]
case_start = next((start for start in case_starts if "CRagEffect::Detecting(this)" in main[start:start + 250]), None)
if case_start is None:
    raise SystemExit("PE effect119 caller missing")
if "CRagEffect::Detecting(this" in ref:
    raise SystemExit("primary Ref caller unexpectedly invokes Detecting")
if '119: "SpawnDetecting(); return true;"' not in (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8"):
    raise SystemExit("effect119 generator route missing")
if "case 119: SpawnDetecting(); return true;" not in dispatch:
    raise SystemExit("generated effect119 route missing")
handler = fn(local, "void CRagEffect::SpawnDetecting()")
for token in ("m_stateCnt != 0", '"effect\\\\hunter_detecting.wav"',
              "m_effectPos.x - listener.x", "m_effectPos.z - listener.z", "PP_3DTEXTURE",
              "prim->m_duration = 57", "prim->m_widthSpeed = 1.5f",
              "prim->m_heightSpeed = 1.5f", "prim->m_widthAccel = prim->m_widthSpeed * 0.071428575f",
              "prim->m_heightAccel = prim->m_heightSpeed * 0.071428575f",
              "prim->m_deltaPos2.y = -7.0f", "prim->m_deltaPos2.y = -5.0f",
              "prim->m_speed3d.y = -0.5f", "prim->m_roll = 90.0f", "prim->m_changePoint = 17",
              "prim->m_fadeOutCnt = 40", '"effect\\\\fashasha.tga"'):
    if token not in handler:
        raise SystemExit(f"local Detecting behavior missing {token}")
family = fn(local, "bool CEffectPrim::UsesReferenceTexture() const")
if "m_master->GetEffectType() == 119" not in family:
    raise SystemExit("effect119 missing from reference texture family")
table = SymbolTable(ExeImage())
for symbol, expected in (("aEffectHunterDe", b"effect\\hunter_detecting.wav"),
                         ("aEffectFashasha", b"effect\\fashasha.tga")):
    resolved = table.resolve_name(symbol)
    if resolved is None or resolved.raw != expected:
        raise SystemExit(f"canonical Detecting resource changed for {symbol}: {resolved!r}")
print("Detecting119 parity: full helper/caller evidence, local route/payload, exact PE resources and texture family passed")
