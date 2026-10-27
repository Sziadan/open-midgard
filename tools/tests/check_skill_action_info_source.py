#!/usr/bin/env python3
"""Compare GetSkillActionInfo against Ref and the canonical EXE decompilation."""

from __future__ import annotations

import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def extract_body(path: Path, signature: str) -> str:
    source = path.read_text(encoding="utf-8", errors="replace")
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"missing function signature {signature!r} in {path}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"missing function body in {path}")

    depth = 0
    quote: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    index = opening
    while index < len(source):
        char = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
        elif block_comment:
            if char == "*" and following == "/":
                block_comment = False
                index += 1
        elif quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif char == "/" and following == "/":
            line_comment = True
            index += 1
        elif char == "/" and following == "*":
            block_comment = True
            index += 1
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening : index + 1]
        index += 1
    raise AssertionError(f"unterminated function body in {path}")


def code_only(source: str) -> str:
    """Remove comments while preserving quoted text, then normalize whitespace."""
    out: list[str] = []
    index = 0
    quote: str | None = None
    escaped = False
    while index < len(source):
        char = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if quote:
            out.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            index += 1
        elif char in ('"', "'"):
            quote = char
            out.append(char)
            index += 1
        elif char == "/" and following == "/":
            index += 2
            while index < len(source) and source[index] != "\n":
                index += 1
        elif char == "/" and following == "*":
            index += 2
            while index + 1 < len(source) and source[index : index + 2] != "*/":
                index += 1
            index += 2
        else:
            out.append(char)
            index += 1
    return re.sub(r"\s+", " ", "".join(out)).strip()


def switch_bodies(source: str) -> list[str]:
    switches: list[str] = []
    for match in re.finditer(r"\bswitch\s*\([^)]*\)\s*\{", source):
        opening = source.find("{", match.start())
        depth = 0
        for index in range(opening, len(source)):
            if source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth == 0:
                    switches.append(source[opening + 1 : index])
                    break
    return switches


def switch_cases(source: str) -> list[list[int]]:
    return [
        [int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", switch)]
        for switch in switch_bodies(source)
    ]


def guards(source: str) -> list[str]:
    # Conditions in this method are simple skillId comparisons/ranges.
    return [re.sub(r"\s+", "", condition) for condition in re.findall(r"\bif\s*\(([^()]*)\)", source)]


def assignments(source: str) -> list[tuple[str, str]]:
    return re.findall(r"\*\s*(beginEffectId|motionType)\s*=\s*(-?\d+)\s*;", source)


def main() -> None:
    ref = extract_body(ROOT / "Ref/Session2.cpp", "GetSkillActionInfo(")
    exe = extract_body(ROOT / "HighPriest.exe.c", "GetSkillActionInfo(")
    current = extract_body(ROOT / "src/session/SkillActionInfo.cpp", "void GetSkillActionInfo(")
    ref_code, exe_code, current_code = map(code_only, (ref, exe, current))

    digest = hashlib.sha256(ref_code.encode()).hexdigest()
    assert ref_code == exe_code, "Ref and canonical EXE function bodies differ"
    assert digest == "708b66e9a21789d5d30310724e9dab326a7dd3b3b840eb565f187142686dd145", digest

    case_sets = [switch_cases(code) for code in (ref_code, exe_code, current_code)]
    assert case_sets[0] == case_sets[1], "Ref/EXE ordered switch case IDs differ"
    assert case_sets[0] == case_sets[2], "current ordered switch case IDs differ from Ref"
    assert len(case_sets[0]) == 6 and sum(map(len, case_sets[0])) == 398
    assert len({case for group in case_sets[0] for case in group}) == 398

    # Compare behavior-bearing assignments within each corresponding switch,
    # preserving order so a matching case inventory cannot hide branch drift.
    switch_assignment_sets = [
        [assignments(switch) for switch in switch_bodies(code)]
        for code in (ref_code, exe_code, current_code)
    ]
    assert switch_assignment_sets[0] == switch_assignment_sets[1], "Ref/EXE switch branches differ"
    assert switch_assignment_sets[0] == switch_assignment_sets[2], "current switch branch assignments differ"

    guard_sets = [guards(code) for code in (ref_code, exe_code, current_code)]
    assert guard_sets[0] == guard_sets[1], "Ref/EXE guard conditions differ"
    assert guard_sets[0] == guard_sets[2], "current guard conditions differ from Ref"
    assert len(guard_sets[0]) == 18

    assignment_sets = [assignments(code) for code in (ref_code, exe_code, current_code)]
    assert assignment_sets[0] == assignment_sets[1], "Ref/EXE output assignments differ"
    assert assignment_sets[0][:2] == [("beginEffectId", "16"), ("motionType", "7")]
    assert sorted(assignment_sets[0]) == sorted(assignment_sets[2]), "current output assignments differ"

    # Ref expresses skill 1004 inline with a return. Current source uses a
    # goto to this final assignment label; the sole no-op label jump preserves
    # the same pair of outputs and there are no statements after the label.
    assert len(re.findall(r"\bgoto\s+\$L153920\s*;", current_code)) == 2
    assert re.search(
        r"\$L153920\s*:\s*\*\s*beginEffectId\s*=\s*-1\s*;\s*"
        r"\*\s*motionType\s*=\s*12\s*;\s*}", current_code
    ), "skill 1004 final output label is not the terminal (-1, 12) route"

    print(
        "GetSkillActionInfo source gate passed: Ref/EXE body SHA-256 "
        f"{digest}; 6 switches / 398 ordered cases, 18 guards, and all "
        "per-switch ordered assignments and output inventory match. Skill 1004 "
        "goto/return normalization verified."
    )


if __name__ == "__main__":
    main()
