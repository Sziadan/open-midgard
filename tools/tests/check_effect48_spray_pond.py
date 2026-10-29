#!/usr/bin/env python3
"""Source and route comparison for effect 48 SprayPond."""
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


SIGNATURE = "void __thiscall CRagEffect::SprayPond(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnSprayPond()")
LOCAL_PROCESS = body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")

check("canonical PE and primary Ref SprayPond bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect 48 dispatches to SprayPond",
      re.search(r"case\s+48\s*:\s*CRagEffect::SprayPond\(this\)", PE_PROCESS) is not None)
check("local effect 48 dispatches to SpawnSprayPond",
      re.search(r"case\s+48\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH) is not None
      and re.search(r"if\s*\(m_type\s*==\s*48\)\s*\{\s*SpawnSprayPond\(\);\s*return\s+true;\s*\}", LOCAL_PROCESS) is not None)
check("PE emitter has state-zero water, ten-frame fade and twenty-frame circle gates in order",
      PE_HANDLER.index("if ( !this->m_stateCnt )")
      < PE_HANDLER.index("if ( !(this->m_stateCnt % 10) )")
      < PE_HANDLER.index("if ( !(this->m_stateCnt % 20) )"))
check("local emitter preserves the three cadence gates and matching order",
      LOCAL_HANDLER.index("if (m_stateCnt == 0)")
      < LOCAL_HANDLER.index("if (m_stateCnt % 10 == 0)")
      < LOCAL_HANDLER.index("if (m_stateCnt % 20 == 0)"))
check("local emitter launches two eight-panel texture groups and a final circle",
      LOCAL_HANDLER.count("LaunchEffectPrim(PP_3DTEXTURE,") == 2
      and LOCAL_HANDLER.count("LaunchEffectPrim(PP_3DCIRCLE,") == 1
      and LOCAL_HANDLER.count("degrees += 45") == 2)

for token in (
    'GetTexture("effect\\\\pond_water.bmp",false)',
    'GetTexture("effect\\\\alpha_down.tga",false)',
    'GetTexture("effect\\\\alpha_center.tga",false)',
    "prim->m_pattern = 2", "prim->m_duration = 130", "prim->m_duration = 30",
    "prim->m_radius = 13.0f", "prim->m_arcAngle = 10.0f",
    "prim->m_radiusSpeed = 0.25f", "InitReferenceSegments(prim)",
):
    check(f"local emitter retains source anchor: {token}", token in LOCAL_HANDLER)

for phrase in (
    "SprayPond zero emits eight water panels eight fades then ring without RNG or sound despite reduced effects",
    "SprayPond water retains source pattern dimensions duration alpha and constructor fade animation UV defaults",
    "SprayPond radial offsets use truncated twenty-two-degree phase with source radii and water-only minus-two height",
    "SprayPond launch motion and retained render matrices preserve source Y versus Z-then-Y rotations",
    "SprayPond texture case and grouped request order match exact source without sprite resources",
    "SprayPond frame ten emits only eight fading panels",
    "SprayPond frame twenty emits eight fades followed by ring without repeating water panels",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("SprayPond PE/Ref source, effect48 route and local lifecycle mapping passed.")
