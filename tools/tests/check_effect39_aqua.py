#!/usr/bin/env python3
"""Source and route comparison for effect 39 Aqua."""
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


SIGNATURE = "void __thiscall CRagEffect::Aqua(CRagEffect *this)"
PE_HANDLER = body(PE, SIGNATURE, PE.index(SIGNATURE + "; // idb") + 1)
REF_HANDLER = body(REF, SIGNATURE)
PE_PROCESS_SIG = "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)"
PE_PROCESS = body(PE, PE_PROCESS_SIG, PE.index(PE_PROCESS_SIG + "; // idb") + 1)
LOCAL_HANDLER = body(CURRENT, "void CRagEffect::SpawnAqua()")

check("canonical PE and primary Ref Aqua bodies normalize identically",
      re.sub(r"\s+", " ", PE_HANDLER).strip() == re.sub(r"\s+", " ", REF_HANDLER).strip())
check("PE effect39 dispatches to Aqua",
      re.search(r"case\s+39\s*:\s*CRagEffect::Aqua\(this\)", PE_PROCESS) is not None)
check("local effect39 dispatches to SpawnAqua",
      re.search(r"case\s+39\s*:\s*return\s+ProcessEffectFrameLegacy\(\)", DISPATCH) is not None
      and re.search(r"if\s*\(m_type\s*==\s*39\)\s*\{\s*SpawnAqua\(\);\s*return\s+true;\s*\}",
                    body(CURRENT, "bool CRagEffect::ProcessEffectFrameLegacy()")) is not None)
check("local parent refresh precedes state-zero one-particle launch and sound",
      LOCAL_HANDLER.index("m_effectPos = master->m_pos")
      < LOCAL_HANDLER.index("if (m_stateCnt != 0) return;")
      < LOCAL_HANDLER.index("PlayWave(")
      < LOCAL_HANDLER.index("LaunchEffectPrim(PP_3DPARTICLE,"))
check("local particle retains parent duration, offset, animation, alpha and original resources",
      all(token in LOCAL_HANDLER for token in (
          "prim->m_duration = m_duration", "prim->m_deltaPos2.y = -20.0f",
          "prim->m_animSpeed = 2", "prim->m_repeatAnim = false",
          "prim->m_alpha = 254.0f", "prim->m_fadeOutCnt = -1",
          r"\xBC\xBA\xBC\xF6\xB6\xDF\xB1\xE2.spr",
          r"\xBC\xBA\xBC\xF6\xB6\xDF\xB1\xE2.act", "InitReferenceSegments(prim)")))

for phrase in (
    "Aqua emits one reference particle without RNG despite reduced effects",
    "Aqua copies actor origin before exact horizontal audio without overriding parent lifetime",
    "Aqua preserves source parent duration offset nonrepeating animation and constructor alpha size fade",
    "Aqua loads exact recovered CP949 sprite then action without guessed texture resources",
    "Aqua initializes one full segment at source launch origin before offset integration",
    "Aqua frame-zero update applies offset then advances source animation to frame one",
    "Aqua nonzero calls follow actor without repeating emission sound resources or RNG",
):
    check(f"lifecycle fixture covers {phrase}", phrase in FIXTURE)

print("Aqua PE/Ref source, effect39 route and local lifecycle mapping passed.")
