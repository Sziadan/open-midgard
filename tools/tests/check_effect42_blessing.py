#!/usr/bin/env python3
"""Source and route comparison for effect 42 Blessing."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, start: int = 0) -> str:
    index = text.index(marker, start)
    opening = text.index("{", index)
    depth = 0
    for cursor in range(opening, len(text)):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:cursor]
    raise AssertionError(f"unclosed body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


SIGNATURE = "void __thiscall CRagEffect::Blessing(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnBlessing()")

check("canonical PE and primary Ref Blessing bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 42 dispatches to Blessing",
      re.search(r"case\s+42\s*:\s*CRagEffect::Blessing\(this\)", PE_PROCESS) is not None)
check("local effect 42 dispatches to SpawnBlessing",
      re.search(r"case\s+42\s*:\s*SpawnBlessing\(\);\s*return\s+true;", DISPATCH) is not None)
check("state-zero source and local launch order is circle, burst and recurring particle",
      PE_HANDLER.index("LaunchEffectPrim(this, PP_3DCIRCLE,")
      < PE_HANDLER.index("LaunchEffectPrim(this, PP_3DPARTICLE,")
      < PE_HANDLER.index("LaunchEffectPrim(this, PP_3DPARTICLE,", PE_HANDLER.index("LaunchEffectPrim(this, PP_3DPARTICLE,") + 1)
      and LOCAL_HANDLER.index("LaunchEffectPrim(PP_3DCIRCLE,")
      < LOCAL_HANDLER.index("LaunchEffectPrim(PP_3DPARTICLE,")
      < LOCAL_HANDLER.index("LaunchEffectPrim(PP_3DPARTICLE,", LOCAL_HANDLER.index("LaunchEffectPrim(PP_3DPARTICLE,") + 1))
check("state-zero blessing sound occurs before particle RNG and launch work",
      PE_HANDLER.index("PlayWave(") < PE_HANDLER.index("LaunchEffectPrim(this, PP_3DCIRCLE,")
      < PE_HANDLER.index("rand()")
      and LOCAL_HANDLER.index("PlayWave(") < LOCAL_HANDLER.index("rand() % 360"))

for token in (
    '"effect\\\\EF_Blessing.wav"', "PP_3DCIRCLE", "RGB(32,176,232)",
    '"effect\\\\alpha_down.tga"', "m_stateCnt % 3 == 0 && m_stateCnt < 10",
    "particle6.spr", "particle6.act", "m_stateCnt % 4 == 0",
    "rand() % 7 + 2", "rand() % 20 + 20", "prim->m_pattern |= 0x80",
):
    check(f"local Blessing retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "Blessing frame zero emits circle, burst and recurring particle with three RNG draws",
    "Blessing preserves circle/burst/spark launch order",
    "Blessing sound uses horizontal listener delta and precedes particle RNG",
    "Blessing circle retains source flags, orientation, radius and tint",
    "Blessing circle initializes reference alpha and segment profile",
    "Blessing burst stores source offset and looping constructor profile",
    "Blessing preserves burst sprite-first and recurring action-first load order with exact assets",
    "Blessing recurring particle retains twinkle, fade, size and animation profile",
    "Blessing angle/radius/speed RNG draws reach source profile boundaries",
    "Blessing preserves independent zero/three/four-frame emission gates",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Blessing PE/Ref source, effect42 route and local lifecycle mapping passed.")
