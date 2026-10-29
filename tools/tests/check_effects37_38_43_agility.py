#!/usr/bin/env python3
"""Source and route comparisons for effects 37/38/43 agility handlers."""
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


PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_PROCESS = body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")
LOCAL_INC = body(CURRENT, "void CRagEffect::SpawnIncAgility()")
LOCAL_DEC = body(CURRENT, "void CRagEffect::SpawnDecAgility()")

for method in ("IncAgility", "DecAgility", "IncAGIDEX"):
    signature = f"void __thiscall CRagEffect::{method}(CRagEffect *this)"
    pe_body = body(PE, signature, PE.index(signature + "; // idb") + 1)
    ref_body = body(REF, signature)
    check(f"canonical PE and primary Ref {method} bodies normalize identically",
          re.sub(r"\s+", " ", pe_body).strip() == re.sub(r"\s+", " ", ref_body).strip())

for effect_id, method in ((37, "IncAgility"), (38, "DecAgility"), (43, "IncAGIDEX")):
    check(f"PE effect{effect_id} dispatches to {method}",
          re.search(rf"case\s+{effect_id}\s*:\s*CRagEffect::{method}\(this\)", PE_PROCESS) is not None)
check("local effects37/43 share agility emitter and effect38 uses decrement emitter",
      re.search(r"if\s*\(m_type\s*==\s*38\)\s*\{\s*SpawnDecAgility\(\);\s*return\s+true;\s*\}", LOCAL_PROCESS) is not None
      and re.search(r"if\s*\(m_type\s*==\s*37\s*\|\|\s*m_type\s*==\s*43\)\s*\{\s*SpawnIncAgility\(\);\s*return\s+true;\s*\}", LOCAL_PROCESS) is not None)
check("local agility variants distinguish effect43 overlay/audio and effect37 particle life",
      "const bool dex = m_type == 43;" in LOCAL_INC
      and "m_gi[0].life = dex ? 0 : 99;" in LOCAL_INC)
check("local increment handler preserves overlay-before-cross cadence and RNG order",
      LOCAL_INC.index("if (m_stateCnt == 0)") < LOCAL_INC.index("LaunchEffectPrim(PP_2DTEXTURE,")
      < LOCAL_INC.index("if (m_stateCnt % 2 != 0) return;")
      < LOCAL_INC.index("LaunchEffectPrim(PP_3DCROSSTEXTURE,")
      < LOCAL_INC.index("rand() % 360") < LOCAL_INC.index("rand() % 7 + 2")
      < LOCAL_INC.index("rand() % 50 + 20") < LOCAL_INC.index("rand() % 60 + 30"))
check("local decrement handler preserves distinct slow overlay and downward cross profile",
      "effect\\\\slow.bmp" in LOCAL_DEC
      and "prim->m_deltaPos2.y -= 20.0f;" in LOCAL_DEC
      and "MatrixAppendXRotation(prim->m_matrix,GetRadian(-90));" in LOCAL_DEC
      and "prim->m_accel = 0.015f;" in LOCAL_DEC)

for phrase in (
    "Agility variants emit source overlay then four-draw cross trail despite reduced effects",
    "Agility variants play distinct exact wave from newly copied actor origin before trail RNG",
    "Agility variants project actor then copy world origin preserving z and parent duration",
    "Agility overlay preserves source unscaled dimensions drag alpha rise and fade",
    "Agility variants use exactly their reference overlay and cross texture without fallback candidates",
    "Agility cross draws radial angle then radius and retains original launch offset",
    "Agility variants preserve trail alpha profile and distinct life-ninety-nine versus zero flags",
    "Only IncAgility life-ninety-nine primitives bypass original no-frame-time gate",
    "DecAgility emits source overlay then three-draw cross trail despite reduced effects",
    "DecAgility plays exact actor-origin horizontal wave before trail RNG",
    "DecAgility cross replaces radial matrix with negative-X rotation and uses acceleration from zero speed",
    "DecAgility even frames emit independent three-draw trail with exact lower range offsets",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Agility PE/Ref handlers, effects37/38/43 routes and local lifecycle mapping passed.")
