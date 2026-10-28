#!/usr/bin/env python3
"""Check effect227's PE caller loop, helper resources, and local port."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage  # noqa: E402

rag = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
if "case 227: return ProcessIntimidate227();" not in dispatch:
    raise AssertionError("effect227 is not routed to Intimidate")
caller = re.search(r"case 227:(.*?)(?=case 228:)", pe, re.S)
if caller is None or "CRagEffect::INTIMIDATE(this)" not in caller.group(1) or not re.search(
    r"v61\s*=\s*20;.*?while\s*\(\s*v61\s*\)", caller.group(1), re.S
):
    raise AssertionError("PE effect227 caller no longer invokes INTIMIDATE twenty times")
helper = re.search(r"void __thiscall CRagEffect::INTIMIDATE\(.*?(?=//----- \(0062DF70)", pe, re.S)
if helper is None or "0062DD80" not in pe or "rand() % 30" not in helper.group(0):
    raise AssertionError("canonical PE Intimidate helper is missing")
image = ExeImage()
for va, expected in (
    (0x71A4A4, b"effect\\EF_Bash.wav"),
    (0x7141F0, b"effect\\rog_intimidate.wav"),
    (0x71C9B8, b"effect\\white01.bmp"),
):
    if image.read_cstring_at(va).raw != expected:
        raise AssertionError(f"Intimidate PE resource at {va:#x} changed")
start = rag.find("bool CRagEffect::ProcessIntimidate227()")
end = rag.find("// TEST_BODY_END intimidate227_handler", start)
if start < 0 or end < 0:
    raise AssertionError("Intimidate handler test boundary is missing")
body = rag[start:end]
for token in (
    "m_stateCnt == 0", "m_stateCnt == 35", "EF_Bash.wav", "rog_intimidate.wav",
    "call<20", "m_pos=master->m_pos", "PP_INTIMIDATE", "white01.bmp",
    "gi.process=-70-rand()%30", "gi.riseAngle=static_cast<short>(rand()%360)",
    "gi.fullDisplayAngle=static_cast<short>(rand()%360)",
    "gi.rotStart=static_cast<short>(rand()%360)", "gi.height[2]=static_cast<float>(rand()%360)",
):
    if token not in body:
        raise AssertionError(f"Intimidate handler is missing {token}")
print("effect227 source check passed: twenty-call PE behavior, two cues, four GI lanes, PE resources")
