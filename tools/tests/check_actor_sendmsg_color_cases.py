#!/usr/bin/env python3
"""Compare actor effect/color messages 127-129 to the canonical sources."""
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
    raise AssertionError(f"unclosed function: {marker}")


def branch(function: str, case_id: int) -> str:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch, "message switch missing"
    offset = switch.end()
    depth = 1
    labels = []
    for match in re.finditer(r"[^\n]*(?:\n|$)", function[offset:]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label:
                labels.append((int(label.group(1)), offset + match.start(), offset + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0:
            break
    for index, (label, _, end_label) in enumerate(labels):
        if label == case_id:
            end = labels[index + 1][1] if index + 1 < len(labels) else len(function)
            return function[end_label:end]
    raise AssertionError(f"case {case_id} missing")


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref = body(REF, marker)
exe = body(EXE, marker)
impl = body(IMPL, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
assert re.sub(r"\s+", " ", ref) == re.sub(r"\s+", " ", exe), "Ref and executable SendMsg bodies differ"
print("PASS full Ref/executable SendMsg body equality")

assert re.search(r"CAbleToMakeEffect::LaunchEffect\(this, 336, z, 0\.0\);", branch(exe, 127))
assert re.search(r"LaunchEffect\(336, vector3d\{\}, 0\.0f\);", branch(impl, 127))
print("PASS message 127 launches effect 336")

source128 = branch(exe, 128)
impl128 = branch(impl, 128)
for pattern in (
    r"this->m_isSprArgbFixed = 1;",
    r"if \( !this->m_oldColor\.a && !this->m_oldColor\.r && !this->m_oldColor\.g && !this->m_oldColor\.b \)",
    r"m_oldColor\.a = HIBYTE\(this->m_sprArgb\);[\s\S]*?m_oldColor\.r = BYTE2\(this->m_sprArgb\);[\s\S]*?m_oldColor\.g = BYTE1\(this->m_sprArgb\);[\s\S]*?m_oldColor\.b = this->m_sprColor\.b;",
    r"this->m_curColor\.a = -1;[\s\S]*?this->m_curColor\.r = v207;[\s\S]*?this->m_curColor\.g = v208;[\s\S]*?this->m_curColor\.b = v209;",
    r"this->m_sprArgb = v209 \| \(\(v208 \| \(\(\*\(_DWORD \*\)&v207 \| 0xFFFFFF00\) << 8\)\) << 8\);",
):
    assert re.search(pattern, source128), f"source message 128 behavior missing: {pattern}"
for pattern in (
    r"m_isSprArgbFixed = 1;",
    r"if \(!m_oldColor\.a && !m_oldColor\.r && !m_oldColor\.g && !m_oldColor\.b\)",
    r"m_oldColor\.a = static_cast<u8>\(\(m_u28\.m_sprArgb >> 24\) & 0xff\);[\s\S]*?m_oldColor\.b = static_cast<u8>\(m_u28\.m_sprArgb & 0xff\);",
    r"m_curColor = ACTOR_COLOR\{ 255, red, green, blue \};",
    r"m_u28\.m_sprArgb = \(0xff000000u \| \(static_cast<u32>\(red\) << 16\)[\s\S]*?static_cast<u32>\(blue\)\);",
):
    assert re.search(pattern, impl128), f"implementation message 128 behavior missing: {pattern}"
print("PASS message 128 saved old ARGB once, set current color, and packed new ARGB")

source129 = branch(exe, 129)
impl129 = branch(impl, 129)
assert re.search(r"this->m_oldColor\.a = 0;\s*this->m_oldColor\.r = 0;\s*this->m_oldColor\.g = 0;\s*this->m_oldColor\.b = 0;\s*this->m_isSprArgbFixed = 0;", source129)
assert re.search(r"m_oldColor = ACTOR_COLOR\{ 0, 0, 0, 0 \};\s*m_isSprArgbFixed = 0;", impl129)
print("PASS message 129 clears saved color and disables fixed sprite tint")
