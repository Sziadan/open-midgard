#!/usr/bin/env python3
"""Gate and adapt the Ref GetSkillActionInfo2 body for differential testing."""

from __future__ import annotations

import argparse
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
    depth = 0
    quote = None
    escaped = line_comment = block_comment = False
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


def normalize(source: str) -> str:
    result: list[str] = []
    index = 0
    quote = None
    escaped = False
    while index < len(source):
        char = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if quote:
            result.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            index += 1
        elif char in ('"', "'"):
            quote = char
            result.append(char)
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
            result.append(char)
            index += 1
    return re.sub(r"\s+", " ", "".join(result)).strip()


def switch_bodies(source: str) -> list[str]:
    result: list[str] = []
    for match in re.finditer(r"\bswitch\s*\([^)]*\)\s*\{", source):
        opening = source.find("{", match.start())
        depth = 0
        for index in range(opening, len(source)):
            if source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth == 0:
                    result.append(source[opening + 1 : index])
                    break
    return result


def cases(source: str) -> list[list[int]]:
    return [
        [int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", body)]
        for body in switch_bodies(source)
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--emit-oracle", type=Path)
    args = parser.parse_args()

    ref = extract_body(ROOT / "Ref/Session2.cpp", "GetSkillActionInfo2(")
    exe = extract_body(ROOT / "HighPriest.exe.c", "CSession::GetSkillActionInfo2(")
    current = extract_body(ROOT / "src/session/SkillActionInfo2.cpp", "void GetSkillActionInfo2(")
    ref_code, exe_code, current_code = map(normalize, (ref, exe, current))
    digest = hashlib.sha256(ref_code.encode()).hexdigest()
    assert ref_code == exe_code, "Ref and canonical EXE bodies differ"
    assert digest == "fbf2638d044fd0e868187e09b8e2baa3fc8218e999638226442755a66362f827", digest

    ref_switches, current_switches = cases(ref_code), cases(current_code)
    assert len(ref_switches) == 8 and len(current_switches) == 7
    assert ref_switches[:7] == current_switches, "ordered skill case IDs differ"
    ref_property = ref_switches[-1]
    assert ref_property == [1, 2, 3, 4, 5, 6]
    property_helper = extract_body(ROOT / "src/session/SkillActionInfo2.cpp", "static void ApplyBeginEffect12Property(")
    helper_cases = [int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", property_helper)]
    assert helper_cases == ref_property
    for prop, effect in zip(helper_cases, [54, 56, 55, 57, 59, 58]):
        assert re.search(rf"case\s+{prop}\s*:\s*beginEffectId\s*=\s*{effect}\s*;", property_helper)

    if args.emit_oracle:
        body = re.sub(r"\$([A-Za-z_]\w*)", r"\1", ref_code)
        args.emit_oracle.parent.mkdir(parents=True, exist_ok=True)
        args.emit_oracle.write_text(
            "#include <cstdint>\n"
            "#define LOBYTE(x) (*reinterpret_cast<unsigned char*>(&(x)))\n"
            "void RefGetSkillActionInfo2(int skillId, int* beginEffectId, int* motionType, int property, int sjob) "
            + body
            + "\n",
            encoding="utf-8",
        )

    print(
        "GetSkillActionInfo2 source inventory passed: Ref/EXE SHA-256 "
        f"{digest}; 7 skill switches / {sum(map(len, ref_switches[:7]))} ordered labels; "
        "property cases 1..6 match the current helper. Full parity is determined by the differential runtime check."
    )


if __name__ == "__main__":
    main()
