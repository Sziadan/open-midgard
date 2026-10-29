#!/usr/bin/env python3
"""Enforce plan rule 0.9: no narrative status in the plan or in STATUS.md.

Fails when:
  * the plan contains a date-stamped status line ("... (2026-10-29): ...") or a
    "checkpoint" heading; progress text belongs in docs/skill/PROGRESS.md;
  * the plan has any heading after the appendix section 10.x (appended
    checkpoint sections);
  * docs/skill/STATUS.md contains anything but the single status table;
  * a STATUS.md cell exceeds the cell budget (the column is "measured number",
    not prose; long evidence lists belong in PROGRESS.md / generated TSVs).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "docs/SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md"
STATUS = ROOT / "docs/skill/STATUS.md"

DATE_STAMP = re.compile(r"\(20\d\d-\d\d-\d\d\)")
CHECKPOINT_HEADING = re.compile(r"^#{1,6}\s.*\b(checkpoint|progress|update)\b", re.IGNORECASE)
APPENDIX_HEADING = re.compile(r"^### 10\.\d")
STATUS_CELL_BUDGET = 400
PLAN_CELL_BUDGET = 1200
STATUS_REQUIRED_ROWS = ["WP0", "WP1", "WP2", "WP3", "WP4", "WP5", "WP6", "WP7", "WP8", "WP9", "WP10"]


def check_plan(errors: list[str]) -> None:
    lines = PLAN.read_text(encoding="utf-8").splitlines()
    seen_appendix = False
    for number, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("#"):
            if APPENDIX_HEADING.match(stripped):
                seen_appendix = True
            elif seen_appendix and stripped.startswith("## "):
                errors.append(f"{PLAN.name}:{number}: section appended after the appendices: {stripped[:80]}")
            if CHECKPOINT_HEADING.match(stripped) and not stripped.startswith("## WP10 "):
                errors.append(f"{PLAN.name}:{number}: status heading in plan: {stripped[:80]}")
            continue
        if stripped.startswith("|"):
            for index, cell in enumerate(stripped.strip("|").split("|")):
                if len(cell.strip()) > PLAN_CELL_BUDGET:
                    errors.append(
                        f"{PLAN.name}:{number}: table cell {index + 1} is {len(cell.strip())} chars (budget {PLAN_CELL_BUDGET}); "
                        "findings stay findings, evidence goes to PROGRESS.md")
            continue
        if DATE_STAMP.search(stripped):
            errors.append(f"{PLAN.name}:{number}: date-stamped status text in plan: {stripped[:80]}")


def check_status(errors: list[str]) -> None:
    lines = STATUS.read_text(encoding="utf-8").splitlines()
    rows = []
    for number, line in enumerate(lines, 1):
        if line.strip() == "":
            continue
        if not line.startswith("|"):
            errors.append(f"{STATUS.name}:{number}: text outside the status table: {line.strip()[:80]}")
            continue
        rows.append((number, line))
    for number, line in rows[2:]:
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 5:
            errors.append(f"{STATUS.name}:{number}: expected 5 cells, found {len(cells)}")
        for index, cell in enumerate(cells):
            if len(cell) > STATUS_CELL_BUDGET:
                errors.append(
                    f"{STATUS.name}:{number}: cell {index + 1} is {len(cell)} chars (budget {STATUS_CELL_BUDGET}); "
                    "move evidence lists to PROGRESS.md or a generated TSV")
    body = "\n".join(line for _, line in rows)
    for wp in STATUS_REQUIRED_ROWS:
        if not re.search(rf"^\|\s*{wp}\b", body, re.MULTILINE):
            errors.append(f"{STATUS.name}: missing row for {wp}")


def main() -> int:
    errors: list[str] = []
    check_plan(errors)
    check_status(errors)
    if errors:
        print("\n".join(errors))
        print(f"{len(errors)} doc hygiene error(s)")
        return 1
    print("doc hygiene OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
