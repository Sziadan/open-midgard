#!/usr/bin/env python3
"""Compare queued-hit processing with the canonical Ref and executable."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


ref_marker = "void __thiscall CGameActor::ProcessWillBeAttacked(CGameActor *this)\n{"
impl_marker = "void CGameActor::ProcessWillBeAttacked()\n{"
ref = body(REF, ref_marker)
exe = body(EXE, ref_marker)
impl = body(IMPL, impl_marker)

assert re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "Ref and executable ProcessWillBeAttacked bodies differ"
print("PASS Ref/executable ProcessWillBeAttacked body equality")

# Source control flow: one time snapshot, ordered full-list walk, strict-future
# skip (thus equality is due), null sender + pointer to the node payload,
# dispatch before erasure, then continue at the saved successor.
assert len(re.findall(r"timeGetTime\s*\(\)", ref)) == 1
assert re.search(r"curTime < Next->_Value\.time", ref)
assert re.search(r"SendMsg\(this, 0, Next->_Value\.message, \(int\)&Next->_Value, 0, 0\);", ref)
assert ref.index("SendMsg(this, 0,") < ref.index("Next->_Prev->_Next = Next->_Next;")
assert re.search(r"v4 = Next->_Next;[\s\S]*?Next = v4;", ref)
print("PASS source deadline, iteration order, sender, payload identity, and erase order")

# Current branch mapping must preserve those semantics without a stack copy.
assert len(re.findall(r"timeGetTime\s*\(\)", impl)) == 1
assert re.search(r"if \(now < it->time\)\s*\{\s*\+\+it;\s*continue;\s*\}", impl)
dispatch = re.search(r"SendMsg\(nullptr,\s*it->message,\s*reinterpret_cast<msgparam_t>\(&\*it\),\s*0,\s*0\);", impl)
erase = re.search(r"it\s*=\s*m_willBeAttackedList\.erase\(it\);", impl)
assert dispatch and erase and dispatch.start() < erase.start(), "current due-item dispatch must precede erasure of that same list node"
assert "const WBA hit = *it;" not in impl
assert "ApplyQueuedHitReaction" not in IMPL
print("PASS current due/future handling, null sender, in-place WBA payload, and erase order")
