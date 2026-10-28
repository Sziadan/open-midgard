"""Compare PE/Ref DecAgility and verify local effect38 audio/texture routes."""
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


signature = "void __thiscall CRagEffect::DecAgility(CRagEffect *this)"
if compact(fn(pe, signature)) != compact(fn(ref, signature)):
    raise SystemExit("DecAgility PE/primary Ref helper mismatch")
main = fn(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 38:"):main.index("case 39:")]
if "CRagEffect::DecAgility(this)" not in case:
    raise SystemExit("PE effect38 caller missing")
if "CRagEffect::DecAgility(this" in ref:
    raise SystemExit("Ref caller omission changed")
if "case 38: return ProcessEffectFrameLegacy();" not in dispatch:
    raise SystemExit("local effect38 legacy route missing")
legacy = fn(local, "bool CRagEffect::ProcessEffectFrameLegacy()")
if "m_type == 38" not in legacy or "SpawnDecAgility();" not in legacy:
    raise SystemExit("local DecAgility handler route missing")
handler = fn(local, "void CRagEffect::SpawnDecAgility()")
for token in ("m_stateCnt == 0", '"effect\\\\EF_DecAgility.wav"',
              '"effect\\\\slow.bmp"', '"effect\\\\ac_center2.tga"',
              "PP_2DTEXTURE", "PP_3DCROSSTEXTURE", "m_stateCnt % 2 != 0",
              "rand() % 360", "rand() % 7+2", "rand() % 60+30"):
    if token not in handler:
        raise SystemExit(f"DecAgility payload missing {token}")
cross = fn(local, "bool CEffectPrim::UsesReferenceCrossTexture() const")
screen = fn(local, "bool CEffectPrim::UsesReferenceScreenTexture() const")
if "m_master->GetEffectType() == 38" not in cross or "m_master->GetEffectType() == 38" not in screen:
    raise SystemExit("effect38 reference primitive family binding missing")
table = SymbolTable(ExeImage())
for symbol, expected in (("aEffectEfDecagi", b"effect\\EF_DecAgility.wav"),
                         ("aEffectSlowBmp", b"effect\\slow.bmp")):
    resolved = table.resolve_name(symbol)
    if resolved is None or resolved.raw != expected:
        raise SystemExit(f"canonical DecAgility asset bytes changed for {symbol}: {resolved!r}")
print("DecAgility38 parity: helper/caller evidence, local route/payload, renderer families and exact PE assets passed")
