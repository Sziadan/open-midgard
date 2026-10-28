"""Check PortalWind effect 389 helper and caller parity anchors."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
dhxj = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")

signature = "void __thiscall CRagEffect::PortalWind(CRagEffect *this, char F1)"
if compact(function(pe, signature)) != compact(function(ref, signature)):
    raise SystemExit("Ref/PE PortalWind helper mismatch")

main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
branch = main[main.index("case 389:"):main.index("case 390:")]
for token in ("PortalWind(this, 0)", "PlayWave(aEffect_66", "v15 >= 5", "v15 <= 25",
              "m_BodyLight & 1", "SetArgb(*p_m_master, -1, 220, 250, 220)"):
    if token not in branch:
        raise SystemExit(f"canonical effect389 branch missing {token}")

fallback_start = dhxj.index("case\tEF_PORTAL4")
fallback_end = dhxj.index("case\tEF_PORTAL5", fallback_start)
fallback = dhxj[fallback_start:fallback_end]
for token in ("PortalWind();", 'PlayWave("effect\\\\windwalk.wav"',
              "m_stateCnt >= 5", "m_stateCnt <= 25", "SetArgb(-1, 220, 250, 220)"):
    if token not in fallback:
        raise SystemExit(f"dhxj EF_PORTAL4 fallback evidence missing {token}")

if "bool ProcessPortalWind389();" not in header:
    raise SystemExit("effect389 process declaration missing")
if "case 389: return ProcessPortalWind389();" not in dispatch:
    raise SystemExit("effect389 route missing")
process = function(local, "bool CRagEffect::ProcessPortalWind389()")
for token in ('ApplyPortalWind(0)', '"effect\\\\windwalk.wav"', "m_stateCnt >= 5",
              "m_stateCnt <= 25", "m_BodyLight & 1", "SetArgb(-1, 220, 250, 220)"):
    if token not in process:
        raise SystemExit(f"local effect389 behavior missing {token}")

helper = function(local, "void CRagEffect::ApplyPortalWind(int variant)")
for token in ("m_effectPos = origin", "m_stateCnt != 0", "kPrimTypeWind", "m_renderFlag = 5",
              '"effect\\\\cloud11.tga"', "rand() % 101", "rand() % 21 + 80",
              "gi.height.fill(1.0f)"):
    if token not in helper:
        raise SystemExit(f"local PortalWind helper anchor missing {token}")
if helper.count("rand() %") != 3:
    raise SystemExit("PortalWind variants must preserve their distance draws and shared rise draw")
variant2 = helper[helper.index("} else if (variant == 2)"):helper.index("} else if (variant == 3)")]
variant3 = helper[helper.index("} else if (variant == 3)"):helper.index("} else {", helper.index("} else if (variant == 3)"))]
if "rand()" in variant2 or "rand()" in variant3:
    raise SystemExit("PortalWind variants 2/3 must not consume distance RNG draws")

if "char aEffect_66[] = \"effect\\\\\"" not in pe:
    raise SystemExit("canonical aEffect_66 data inspection changed")
print("PortalWind389 parity: Ref/PE helper, canonical caller, dhxj fallback wave, route, GI and RNG checks passed")
