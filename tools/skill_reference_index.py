"""Structural indexes for the decompiled 2008 skill/effect dispatchers.

Offsets always refer to the original source. Literals and comments are masked
before counting braces so paths, format strings and IDA comments cannot become
syntax. Skip-frame switches and nested switches are not effect dispatch cases.
"""
from __future__ import annotations

from dataclasses import dataclass
import re
from pathlib import Path


def mask_literals(text: str) -> str:
    token = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\r\n])*"|\'(?:\\.|[^\'\\\r\n])*\'')
    return token.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), text)


def closing_brace(masked: str, opening: int) -> int:
    depth = 1
    for match in re.finditer(r"[{}]", masked[opening + 1:]):
        depth += 1 if match.group() == "{" else -1
        if depth == 0:
            return opening + 1 + match.start()
    raise ValueError("Unclosed source block")


@dataclass
class SourceFunction:
    text: str
    masked: str
    first_line: int


def read_function(path: Path, name: str) -> SourceFunction:
    text = path.read_text(encoding="latin-1")
    masked = mask_literals(text)
    signature = re.search(r"// Original Signature:[^\n]*\b" + re.escape(name) + r"\(", text)
    pattern = re.compile(r"^[^\n]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\n\{", re.M)
    definition = pattern.search(masked, signature.end() if signature else 0)
    if not definition:
        raise ValueError(f"Definition of {name} not found in {path}")
    opening = masked.index("{", definition.start())
    end = closing_brace(masked, opening) + 1
    return SourceFunction(text[definition.start():end], masked[definition.start():end],
        text.count("\n", 0, definition.start()) + 1)


@dataclass
class DispatchCase:
    id: int
    line: int
    handlers: tuple[str, ...]


def switch_case_locations(function: SourceFunction):
    """Yield (numeric label, switch expression, absolute line) for every case."""
    code = function.masked
    switches = []
    for match in re.finditer(r"\bswitch\s*\(([^;{}]*?)\)\s*\{", code):
        opening = code.index("{", match.start())
        switches.append((opening, closing_brace(code, opening), match.group(1).strip()))
    for match in re.finditer(r"\bcase\s+(0x[0-9a-fA-F]+|\d+)\s*:", code):
        enclosing = [item for item in switches if item[0] < match.start() < item[1]]
        if enclosing:
            expression = max(enclosing)[2]
            token = match.group(1)
            yield int(token, 16 if token.startswith("0x") else 10), expression, function.first_line + code.count("\n", 0, match.start())


def dispatch_cases(function: SourceFunction, expression: str) -> list[DispatchCase]:
    code = function.masked
    switch = re.search(r"\bswitch\s*\(\s*" + re.escape(expression) + r"\s*\)\s*\{", code)
    if not switch:
        raise ValueError(f"Dispatch switch {expression} not found")
    opening = code.index("{", switch.start())
    end = closing_brace(code, opening)
    depth = 1
    cases = []
    for token in re.finditer(r"[{}]|\bcase\s+(0x[0-9a-fA-F]+|\d+)\s*:", code[opening + 1:end]):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
        elif depth == 1:
            cases.append((int(token.group(1), 0) if token.group(1).startswith("0x") else int(token.group(1)),
                opening + 1 + token.start(), opening + 1 + token.end()))
    labels = {m.group(1): m.end() for m in re.finditer(r"^\s*([\w$]+):", code, re.M)
        if m.group(1) not in ("default", "case")}
    event = re.compile(r"CRagEffect::(\w+)\s*\(|\bgoto\s+([\w$]+)\s*;|\bbreak\s*;|\breturn\b[^;]*;")
    result = []
    for case_id, case_start, body_start in cases:
        base_indent = len(code[code.rfind("\n", 0, case_start) + 1:case_start]) + 2
        for line in code[body_start:end].splitlines():
            statement = line.strip()
            if not statement or statement.startswith(("case ", "default:")) or re.fullmatch(r"[\w$]+:", statement):
                continue
            base_indent = len(line) - len(line.lstrip())
            break
        handlers = []
        visited = set()

        def walk(start: int) -> None:
            if start in visited or not opening < start < end:
                return
            visited.add(start)
            for match in event.finditer(code, start, end):
                if match.group(1):
                    if match.group(1) not in handlers:
                        handlers.append(match.group(1))
                    continue
                line_start = code.rfind("\n", 0, match.start()) + 1
                indent = len(code[line_start:match.start()])
                if match.group(2):
                    target = labels.get(match.group(2))
                    if target is not None:
                        walk(target)
                # Indented transfers inside a condition do not terminate the
                # other path. The decompiler indents single-statement bodies.
                if indent <= base_indent:
                    return

        walk(body_start)
        result.append(DispatchCase(case_id, function.first_line + code.count("\n", 0, case_start), tuple(handlers)))
    return result
