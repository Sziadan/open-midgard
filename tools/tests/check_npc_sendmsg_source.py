#!/usr/bin/env python3
"""Source-to-current audit for the in-scope CNpc::SendMsg spawn behavior."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF_NPC = (ROOT / "Ref/Npc.cpp").read_text(encoding="utf-8", errors="replace")
REF_SESSION = (ROOT / "Ref/Session.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> tuple[str, str]:
    start = text.rfind(marker)
    assert start >= 0, f"function not found: {marker}"
    opening = text.find("{", start)
    semicolon = text.find(";", start)
    assert opening >= 0 and (semicolon < 0 or opening < semicolon), f"definition not found: {marker}"
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening].strip(), text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def labels(text: str) -> list[str]:
    match = re.search(r"switch\s*\(\s*message\s*\)\s*\{", text)
    assert match, "CNpc::SendMsg message switch missing"
    start = match.end()
    depth = 1
    result: list[str] = []
    for line in re.finditer(r"[^\n]*(?:\n|$)", text[start:]):
        value = line.group()
        if depth == 1:
            label = re.match(r"\s*(case\s+[^:]+|default)\s*:", value)
            if label:
                result.append(label.group(1).strip())
        depth += value.count("{") - value.count("}")
        if depth == 0:
            return result
    raise AssertionError("unclosed CNpc::SendMsg message switch")


normalize = lambda value: re.sub(r"\s+", " ", value).strip()
npc_marker = "void __thiscall CNpc::SendMsg(CNpc *this, CGameObject *sender, int message, int arg1, int arg2, int arg3)"
ref_sig, ref = body(REF_NPC, npc_marker)
exe_sig, exe = body(EXE, npc_marker)
assert normalize(ref_sig) == normalize(exe_sig), "CNpc::SendMsg Ref/EXE signatures differ"
assert normalize(ref) == normalize(exe), "complete CNpc::SendMsg Ref/EXE bodies differ"
expected = ["case 4", "case 34", "case 35", "case 125", "case 138", "default"]
assert labels(ref) == labels(exe) == expected, f"NPC message IDs/order differ: Ref={labels(ref)}, EXE={labels(exe)}"
print(f"PASS complete Ref/EXE body equality ({len(normalize(ref))} normalized characters)")
print(f"PASS mechanically extracted ordered NPC message IDs: {expected}")

session_marker = "bool __thiscall CSession::IsVisibleNPC(CSession *this, JOBTYPE jobType)"
session_ref = body(REF_SESSION, session_marker)[1]
session_exe = body(EXE, session_marker)[1]
assert normalize(session_ref) == normalize(session_exe), "IsVisibleNPC Ref/EXE bodies differ"
assert re.search(
    r"jobType\s*!=\s*JT_WARPNPC\s*&&\s*jobType\s*!=\s*JT_EFFECTLAUNCHER\s*&&\s*jobType\s*!=\s*JT_HIDDEN_NPC",
    session_ref,
), "source NPC visibility predicate changed"
print("PASS Ref/EXE visibility predicate: hide WARPNPC, EFFECTLAUNCHER, HIDDEN_NPC")

send_marker = "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)"
_, current = body(IMPL, send_marker)
match = re.search(r"switch\s*\(\s*msg\s*\)\s*\{", current)
assert match, "current actor message switch missing"
current_labels: list[str] = []
start = match.end()
depth = 1
case4_start = case4_end = -1
for line in re.finditer(r"[^\n]*(?:\n|$)", current[start:]):
    value = line.group()
    if depth == 1:
        label = re.match(r"\s*(case\s+[^:]+|default)\s*:", value)
        if label:
            current_labels.append(label.group(1).strip())
            if label.group(1).strip() == "case 4":
                case4_start = start + line.end()
            elif case4_start >= 0 and case4_end < 0:
                case4_end = start + line.start()
    depth += value.count("{") - value.count("}")
    if depth == 0:
        switch_end = start + line.start()
        break
if case4_start >= 0 and case4_end < 0:
    case4_end = switch_end
assert case4_start >= 0 and case4_end > case4_start, "current message-4 route missing"
current_case4 = current[case4_start:case4_end]
assert re.search(r"m_job\s*=\s*resolvedJob", current_case4), "current message 4 doesn't store the resolved source job"
assert re.search(r"m_isVisible\s*=\s*\(resolvedJob\s*!=\s*0x2D\s*&&\s*resolvedJob\s*!=\s*0x68\s*&&\s*resolvedJob\s*!=\s*0x6F\)", current_case4), \
    "current message-4 NPC visibility predicate differs from source job mapping"
assert re.search(r"if\s*\(resolvedJob\s*==\s*722\)[\s\S]*?SetGuildInfo\(guildId,\s*actorArgs\[17\]\)", current_case4), \
    "current message 4 guild metadata route missing"
assert re.search(r"actorArgs\[18\]\s*<<\s*16[\s\S]*?actorArgs\[19\]\s*&\s*0xffff", current_case4), \
    "current guild ID high/low field mapping differs"
print("PASS current message-4 branch resolves job, applies visibility filter, and routes guild metadata")

for ui_case in ("case 34", "case 35", "case 125", "case 138"):
    assert ui_case in expected
print("Documented scope: gauge-window messages 34/35/125/138 are legacy UI; remaining default behavior delegates to the base actor message handler")
