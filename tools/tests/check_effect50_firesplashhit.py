#!/usr/bin/env python3
"""Source and route comparison for effect 50 FireSplashHit."""
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


SIGNATURE = "void __thiscall CRagEffect::FireSplashHit(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnFireSplashHit()")
LOCAL_PROCESS = body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")

check("canonical PE and primary Ref FireSplashHit bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 50 dispatches to FireSplashHit",
      re.search(r"case\s+50\s*:\s*CRagEffect::FireSplashHit\(this\)", PE_PROCESS) is not None)
check("local effect 50 dispatches to SpawnFireSplashHit",
      re.search(r"case\s+50\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH) is not None
      and re.search(r"if\s*\(m_type\s*==\s*50\)\s*\{\s*SpawnFireSplashHit\(\);\s*return\s+true;\s*\}", LOCAL_PROCESS) is not None)
check("source handler projects then draws angle before state-zero primitive creation",
      PE_HANDLER.index("CRenderer::ProjectVertex") < PE_HANDLER.index("rand() % 360")
      < PE_HANDLER.index("if ( !m_stateCnt )")
      < PE_HANDLER.index("LaunchEffectPrim(this, PP_2DTEXTURE,"))
check("local valid-state path preserves projection, unconditional angle draw and state-zero launch order",
      LOCAL_HANDLER.index("ProjectReferenceEffectVertex") < LOCAL_HANDLER.index("rand() % 360")
      < LOCAL_HANDLER.index("if (m_stateCnt != 0) return;")
      < LOCAL_HANDLER.index("LaunchEffectPrim(PP_2DTEXTURE,"))
check("local projection guards unavailable master or view before the source-equivalent work",
      "if (!master || !mode || !mode->m_view) return;" in LOCAL_HANDLER)

for token in (
    "m_duration = 30", "m_deltaPos2.y = -20.0f", "m_widthSize = 10.0f",
    "m_heightSize = 10.0f", "m_widthSpeed = 5.6999998f", "m_heightSpeed = 5.6999998f",
    "m_rollSpeed = -18.0f", "m_fadeOutCnt = prim->m_duration-prim->m_duration/2",
    'GetTexture("effect\\\\FireRing.tga",false)', "m_gi[0].life = 0",
):
    check(f"local FireSplashHit retains source field: {token}", token in LOCAL_HANDLER)

for phrase in (
    "FireSplashHit emits one source screen texture and draws one roll despite reduced effects",
    "FireSplashHit projects actor x y while preserving projected z stored world origin and parent lifetime",
    "FireSplashHit retains source dimensions growth random roll angular drag and half-duration fade",
    "FireSplashHit restores constructor full alpha and repeat animation with source GI life zero",
    "FireSplashHit uses exact case-sensitive source texture with no sprite loads or wave",
    "FireSplashHit later calls discard required angle draw without repeat emission texture request or roll reset",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("FireSplashHit PE/Ref source, effect50 route and local lifecycle mapping passed.")
