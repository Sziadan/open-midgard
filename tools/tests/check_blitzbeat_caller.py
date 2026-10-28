"""Compare BlitzBeat helper and verify effect115 ordering, emitters, RNG, and family routes."""
from __future__ import annotations
import re
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as resolver

def function(source: str, signature: str) -> str:
    start = source.rindex(signature); opening = source.index("{", start); depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{": depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0: return source[start:pos + 1]
    raise AssertionError(signature)
def compact(text: str) -> str: return re.sub(r"\s+", "", text)
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
sig = "void __thiscall CRagEffect::BlitzBeat(CRagEffect *this)"
if compact(function(pe, sig)) != compact(function(ref, sig)): raise SystemExit("Ref/PE BlitzBeat helper mismatch")
main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
case = main[main.index("case 115:"):main.index("case 116:")]
if "BlitzBeat(this)" not in case: raise SystemExit("canonical effect115 caller changed")
if "BlitzBeat(this" in ref: raise SystemExit("Ref caller omission changed; review PE-only caller evidence")
if "bool ProcessBlitzBeat115();" not in header or "case 115: return ProcessBlitzBeat115();" not in dispatch:
    raise SystemExit("effect115 route/declaration missing")
handler = function(local, "bool CRagEffect::ProcessBlitzBeat115()")
for token in ('"effect\\\\hunter_blitzbeat_1st.wav"', '"effect\\\\hunter_blitzbeat.wav"',
              "m_stateCnt == 5 || m_stateCnt == 30", "m_effectPos = master->m_pos", "m_stateCnt == 0",
              "g_blitzBeatEmissionCount = 0", "index < 10", "PP_3DCROSSTEXTURE", "rand() % 360",
              "rand() % 40 + 20", "effect\\\\ac_center2.tga", "m_stateCnt > 35", "m_stateCnt % 3 == 0",
              "g_blitzBeatEmissionCount < m_count", "rand() % 10", "rand() % 20", "PP_3DCYLINDER",
              "effect\\\\magic_green.tga", "++g_blitzBeatEmissionCount"):
    if token not in handler: raise SystemExit(f"BlitzBeat payload missing {token}")
if handler.count("rand()") != 5: raise SystemExit("expected 20 state-zero RNG draws plus 3 later draws")
for name, signature, token in (("cross-texture", "bool CEffectPrim::UsesReferenceCrossTexture() const", "115"),
                               ("cylinder", "bool CEffectPrim::UsesReferenceCylinder() const", "case 115")):
    family = function(local, signature)
    if token not in family: raise SystemExit(f"effect115 {name} reference family route missing")
for symbol, value in (("aEffectHunterBl_0", "effect\\\\hunter_blitzbeat_1st.wav"),
                      ("aEffectHunterBl_1", "effect\\\\hunter_blitzbeat.wav"),
                      ("aEffectAcCenter", "effect\\\\ac_center2.tga"),
                      ("aEffectMagicGre", "effect\\\\magic_green.tga")):
    if f'{symbol}[] = "{value}"' not in pe: raise SystemExit(f"canonical PE resource {symbol} changed")
print("BlitzBeat115 parity: Ref/PE helper, PE caller, route, wave/resource names, RNG/cadence, and two render families passed")
