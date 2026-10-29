#!/usr/bin/env python3
"""Source and route comparison for effect 46 SandWind."""
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


SIGNATURE = "void __thiscall CRagEffect::SandWind(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnSandWind()")

check("canonical PE and primary Ref SandWind bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 46 dispatches to SandWind",
      re.search(r"case\s+46\s*:\s*CRagEffect::SandWind\(this\)", PE_PROCESS) is not None)
check("local effect 46 dispatches through the legacy emitter",
      re.search(r"case\s+46\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH) is not None
      and re.search(r"if\s*\(m_type\s*==\s*46\)\s*\{\s*SpawnSandWind\(\);\s*return\s+true;\s*\}",
                    body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")) is not None)
check("source projection precedes its under-sixty and even-state emission gates",
      PE_HANDLER.index("CRenderer::ProjectVertex") < PE_HANDLER.index("if ( m_stateCnt < 60 )")
      < PE_HANDLER.index("m_stateCnt & 0x80000001")
      < PE_HANDLER.index("LaunchEffectPrim(this, PP_2DTEXTURE,"))
check("local projection refresh precedes the same emission gates",
      LOCAL_HANDLER.index("ProjectReferenceEffectVertex") < LOCAL_HANDLER.index("m_tlvertX =")
      < LOCAL_HANDLER.index("m_stateCnt >= 60") < LOCAL_HANDLER.index("m_stateCnt % 2"))
check("local emission keeps six ordered random draws and one PP_2DTEXTURE",
      LOCAL_HANDLER.index("m_duration - rand() % 100")
      < LOCAL_HANDLER.index("rand() % 300 + 260")
      < LOCAL_HANDLER.index("rand() % 15 + 130")
      < LOCAL_HANDLER.index("rand() % 640 - 500")
      < LOCAL_HANDLER.index("rand() % 50 - 240")
      < LOCAL_HANDLER.index("rand() % 10 + 10")
      and LOCAL_HANDLER.count("LaunchEffectPrim(PP_2DTEXTURE,") == 1)

for token in (
    "prim->m_pattern = 8", "prim->m_widthSize = 30.0f", "prim->m_heightSize = 30.0f",
    "prim->m_alpha = 128.0f", "prim->m_maxAlpha = 254.0f",
    "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 3",
    'GetTexture("effect\\\\sand1.bmp", false)', "prim->m_gi[0].life = 0",
):
    check(f"local SandWind retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "SandWind projects actor x/y without replacing effect origin or stored z",
    "SandWind emits reference screen texture with original random duration and fade arithmetic",
    "SandWind preserves six-draw duration speed heading x y interval order",
    "SandWind retains source dimensions alpha constructor color and texture animation defaults",
    "SandWind requests exact texture and emits no wave",
    "SandWind includes last even frame fifty-eight",
    "SandWind stops emission at sixty while continuing actor projection",
    "SandWind preserves negative random lifetime and integer division without invented clamp",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("SandWind PE/Ref source, effect46 route and local lifecycle mapping passed.")
