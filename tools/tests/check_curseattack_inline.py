"""Verify inline PE effect196 payload, helper/caller distinction and texture family."""
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


signature = "void __thiscall CRagEffect::CurseAttack(CRagEffect *this)"
if re.sub(r"\s+", "", fn(pe, signature)) != re.sub(r"\s+", "", fn(ref, signature)):
    raise SystemExit("CurseAttack PE/primary Ref helper mismatch")
for source, label in ((pe, "PE"), (ref, "Ref")):
    if "CRagEffect::CurseAttack(this" in source:
        raise SystemExit(f"{label} unexpectedly invokes orphaned CurseAttack helper")
main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case_starts = [match.start() for match in re.finditer(r"case 196:", main)]
case_start = next((start for start in case_starts if "PP_3DTEXTURE" in main[start:start + 1200]), None)
if case_start is None:
    raise SystemExit("PE effect196 inline texture caller missing")
case_end = main.find("case 197:", case_start)
case = main[case_start:case_end if case_end >= 0 else case_start + 1200]
for token in ("if ( !this->m_stateCnt )", "LaunchEffectPrim(this, PP_3DTEXTURE",
              "g_prim->m_duration = 9999", "g_prim->m_longitude = 0.0",
              "g_prim->m_longSpeed = 4.5", "g_prim->m_deltaPos2.y = g_prim->m_deltaPos2.y - 30.0",
              "g_prim->m_widthSize = 4.0", "g_prim->m_heightSize = 4.0",
              "CTexMgr::GetTexture(&g_texMgr, aEffectCurseBmp, 0)"):
    if token not in case:
        raise SystemExit(f"inline PE effect196 payload missing {token}")
if "case 196: ApplyCurseAttack(); return true;" not in dispatch:
    raise SystemExit("local effect196 route missing")
handler = fn(local, "void CRagEffect::ApplyCurseAttack()")
for token in ("m_stateCnt != 0", "PP_3DTEXTURE", "prim->m_duration = 9999",
              "prim->m_longSpeed = 4.5f", "prim->m_deltaPos2.y -= 30.0f",
              "prim->m_widthSize = 4.0f", "prim->m_heightSize = 4.0f",
              '"effect\\\\curse.bmp"'):
    if token not in handler:
        raise SystemExit(f"local CurseAttack payload missing {token}")
family = fn(local, "bool CEffectPrim::UsesReferenceTexture() const")
if "m_master->GetEffectType() == 196" not in family:
    raise SystemExit("effect196 missing from reference texture family")
texture = SymbolTable(ExeImage()).resolve_name("aEffectCurseBmp")
if texture is None or texture.raw != b"effect\\curse.bmp":
    raise SystemExit(f"canonical CurseAttack texture bytes changed: {texture!r}")
print("CurseAttack196 parity: orphan helper, inline PE caller, local route/payload, exact texture bytes and reference texture family passed")
