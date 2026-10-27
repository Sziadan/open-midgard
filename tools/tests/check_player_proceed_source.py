#!/usr/bin/env python3
"""Mechanically compare CPlayer::ProcessProceed in Ref and canonical EXE output."""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = {
    "Ref": (ROOT / "Ref/Player.cpp", "void __thiscall CPlayer::ProcessProceed(CPlayer *this)"),
    "EXE": (ROOT / "HighPriest.exe.c", "void __thiscall CPlayer::ProcessProceed(CPlayer *this)"),
}
EXPECTED_CASES = [
    "PT_ATTACK", "PT_ATTACK_2", "PT_PICKUPITEM", "PT_SKILL",
    "PT_GROUNDSKILL", "PT_TOUCH_SKILL",
]


def function_body(path: Path, signature: str) -> str:
    text = path.read_text(encoding="utf-8", errors="replace")
    start = None
    for match in re.finditer(re.escape(signature), text):
        candidate = match.end()
        while candidate < len(text) and text[candidate].isspace():
            candidate += 1
        if candidate < len(text) and text[candidate] == "{":
            start = candidate
            break
    if start is None:
        raise AssertionError(f"function definition not found in {path}")
    opening = start
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening : pos + 1]
    raise AssertionError(f"unclosed function body in {path}")


def normalize(body: str) -> str:
    body = re.sub(r"//[^\n]*", "", body)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return re.sub(r"\s+", " ", body).strip()


bodies = {name: function_body(path, sig) for name, (path, sig) in SOURCES.items()}
normalized = {name: normalize(body) for name, body in bodies.items()}
for name, body in normalized.items():
    print(f"{name}: {SOURCES[name][0].relative_to(ROOT)} body_chars={len(body)} sha256={hashlib.sha256(body.encode()).hexdigest()}")

ref, exe = normalized["Ref"], normalized["EXE"]
assert ref == exe, "normalized Ref and canonical EXE function bodies differ"

cases_by_source = {}
for name, body in normalized.items():
    cases_by_source[name] = re.findall(r"\bcase\s+(PT_[A-Z0-9_]+)\s*:", body)
    print(f"{name} case_ids={' -> '.join(cases_by_source[name])}")
    assert cases_by_source[name] == EXPECTED_CASES

branches = re.split(r"\bcase\s+(PT_[A-Z0-9_]+)\s*:", ref)
branch_text = {branches[i]: branches[i + 1] for i in range(1, len(branches) - 1, 2)}
print("branch_body_chars=" + ", ".join(f"{case}:{len(branch_text[case])}" for case in EXPECTED_CASES))
print("Ref and canonical EXE normalized bodies and ordered case IDs match.")
