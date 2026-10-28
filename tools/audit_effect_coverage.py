#!/usr/bin/env python3
"""Generate a ratcheted static inventory of Ref effect ids and src routes."""
from __future__ import annotations

import csv
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INDEX = ROOT / "docs/skill/generated/effect_handler_index.tsv"
COVERAGE = ROOT / "docs/skill/generated/EFFECT_COVERAGE.tsv"
AUDIT = ROOT / "docs/skill/generated/PARITY_AUDIT.txt"
PARITY = ROOT / "docs/skill/PARITY_LOG.md"
RAG = ROOT / "src/world/RagEffect.cpp"
STR_TABLE = ROOT / "src/world/RagEffectStrNameTable.generated.inc"
CLIP_TABLE = ROOT / "src/world/RagEffect.cpp"
RECLASSIFIED_UNPORTED = {537, 669}
DISPATCH_FILES = {
    "OnProcess": ROOT / "src/world/RagEffectOnProcessCases.generated.inc",
    "OnProcess2": ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc",
    "OnProcess3": ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc",
}


def body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        return ""
    opening = source.find("{", start + len(signature))
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    raise ValueError(f"unterminated body: {signature}")


def ids_from_body(text: str) -> set[int]:
    result = {int(value) for value in re.findall(r"\bm_type\s*==\s*(\d+)\b", text)}
    for first, last in re.findall(r"\bm_type\s*>=\s*(\d+)\s*&&\s*m_type\s*<=\s*(\d+)\b", text):
        result.update(range(int(first), int(last) + 1))
    for first, last in re.findall(r"\bm_type\s*<=\s*(\d+)\s*&&\s*m_type\s*>=\s*(\d+)\b", text):
        result.update(range(int(last), int(first) + 1))
    result.update(int(value) for value in re.findall(r"\bcase\s+(\d+)\s*:", text))
    return result


def route_sources(effect_id: int, process_text: str, process2_text: str, str_only_text: str, init_ids: set[int], clip_ids: set[int]) -> list[str]:
    found: list[str] = []
    if re.search(rf"\bm_type\s*==\s*{effect_id}\b|\bcase\s+{effect_id}\s*:", process_text) or any(
        first <= effect_id <= last for first, last in (
            (int(a), int(b)) for a, b in re.findall(r"\bm_type\s*>=\s*(\d+)\s*&&\s*m_type\s*<=\s*(\d+)", process_text)
        )
    ):
        found.append("ProcessEffectFrame")
    if re.search(rf"\bcase\s+{effect_id}\s*:", process2_text):
        found.append("OnProcess2")
    if re.search(rf"\bcase\s+{effect_id}\s*:", str_only_text):
        found.append("ProcessReferenceStrOnly")
    if effect_id in init_ids:
        found.append("Init(handler)")
    if effect_id in clip_ids:
        found.append("kProvenClipWaves")
    return found


def init_handler_ids(text: str) -> set[int]:
    init = body(text, "void CRagEffect::Init(")
    ids: set[int] = set()
    labels: list[int] = []
    group_start = 0
    for match in re.finditer(r"\bcase\s+(\d+)\s*:", init):
        between = init[group_start:match.start()]
        if "m_handler = Handler::" in between:
            handlers = re.findall(r"m_handler\s*=\s*Handler::(\w+)\s*;", between)
            if handlers and handlers[-1] != "None":
                ids.update(labels)
        if "break;" in between or "return" in between:
            labels = []
        labels.append(int(match.group(1)))
        group_start = match.end()
    tail = init[group_start:]
    handlers = re.findall(r"m_handler\s*=\s*Handler::(\w+)\s*;", tail)
    if handlers and handlers[-1] != "None":
        ids.update(labels)
    return ids


def launch_sources(ids: set[int]) -> dict[int, set[str]]:
    patterns = [
        re.compile(r"\bLaunchEffect\s*\(\s*(\d+)\b"),
        re.compile(r"\bLaunchOn\s*\([^,]*,\s*(\d+)\b"),
        re.compile(r"\bPlaceEffect\s*\([^,]*,\s*(\d+)\s*,"),
        re.compile(r"\b(?:effectId1|beginEffectId|m_effectId)\s*=\s*(\d+)\b"),
    ]
    candidates = [
        p for p in (ROOT / "src").rglob("*")
        if p.is_file() and p.suffix.lower() in {".cpp", ".h", ".inc"}
        if "build" not in p.parts and "tests" not in p.parts
    ]
    candidates += [ROOT / "src/session/SkillAttackEffectIdTable.generated.inc"]
    result: dict[int, set[str]] = defaultdict(set)
    for path in candidates:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rel = path.relative_to(ROOT).as_posix()
        if path.name == "SkillAttackEffectIdTable.generated.inc":
            for match in re.finditer(r"(?<!\d)(\d+)(?!\d)", text):
                effect_id = int(match.group(1))
                if effect_id in ids:
                    result[effect_id].add(rel)
            continue
        for pattern in patterns:
            for match in pattern.finditer(text):
                effect_id = int(match.group(1))
                if effect_id in ids:
                    result[effect_id].add(rel)
    return result


def parity_lines(rows: list[dict[str, str]], text: str) -> dict[int, str]:
    lines = text.splitlines()
    result: dict[int, str] = {}
    for row in rows:
        effect_id = int(row["case"])
        handler = row["handler"]
        needle = re.compile(rf"(?<!\d){effect_id}(?!\d)")
        case_list_hits = []
        for i, line in enumerate(lines, 1):
            match = re.search(r"Case list:\s*([\d, -]+)", line)
            if match:
                listed_ids = {int(value) for value in re.findall(r"\d+", match.group(1))}
                if effect_id in listed_ids:
                    case_list_hits.append(str(i))
        if case_list_hits:
            result[effect_id] = case_list_hits[-1]
            continue
        hits = [str(i) for i, line in enumerate(lines, 1) if handler in line or needle.search(line)]
        result[effect_id] = hits[0] if hits else ""
    return result


def main() -> int:
    rag = RAG.read_text(encoding="utf-8", errors="replace")
    parity_text = PARITY.read_text(encoding="utf-8", errors="replace")
    with INDEX.open(encoding="utf-8", newline="") as stream:
        index_rows = list(csv.DictReader(stream, delimiter="\t"))
    if len(index_rows) != 693:
        raise SystemExit(f"expected 693 Ref effect rows, found {len(index_rows)}")

    process_text = body(rag, "bool CRagEffect::ProcessEffectFrame()")
    process2_text = body(rag, "bool CRagEffect::OnProcess2()")
    str_only_text = body(rag, "std::optional<bool> CRagEffect::ProcessReferenceStrOnly()")
    init_ids = init_handler_ids(rag)
    dispatch_text = {tier: path.read_text(encoding="utf-8", errors="replace") for tier, path in DISPATCH_FILES.items()}
    explicit_dispatch = {
        int(effect_id): (tier, "UnportedEffect" in line, match.group(2))
        for tier, text in dispatch_text.items()
        for line in text.splitlines()
        if (match := re.match(r"case\s+(\d+):\s*(.*)", line))
        for effect_id in [match.group(1)]
    }
    process_ids = ids_from_body(process_text) - set(range(300, 374))
    process2_ids = ids_from_body(process2_text)
    explicit_ids = process_ids | process2_ids | ids_from_body(str_only_text) | init_ids | set(explicit_dispatch)
    clip_ids = {int(value) for value in re.findall(r"\{\s*(\d+)\s*,", body(rag, "const ProvenClipWave kProvenClipWaves[]"))}
    str_ids = {int(value) for value in re.findall(r"\{\s*(\d+)\s*,", STR_TABLE.read_text(encoding="utf-8", errors="replace"))}
    launched = launch_sources({int(row["case"]) for row in index_rows})

    prior_rows: dict[int, dict[str, str]] = {}
    if COVERAGE.exists():
        with COVERAGE.open(encoding="utf-8", newline="") as stream:
            prior_rows = {int(row["id"]): row for row in csv.DictReader(stream, delimiter="\t")}

    out: list[dict[str, str]] = []
    parity_map = parity_lines(index_rows, parity_text)
    for row in index_rows:
        effect_id = int(row["case"])
        handler = row["handler"]
        in_explicit = effect_id in explicit_ids
        if effect_id in explicit_dispatch and explicit_dispatch[effect_id][1]:
            status = "unported"
            route = f"{explicit_dispatch[effect_id][0]} -> UnportedEffect"
        elif in_explicit:
            has_parity = handler in parity_text or re.search(rf"(?<!\d){effect_id}(?!\d)", parity_text)
            # A dispatcher destination only proves routing. Keep the stronger
            # ported label for independently branch-compared work; the current
            # legacy wrappers still need their WP5 walk-throughs.
            status = "routed" if effect_id in explicit_dispatch and has_parity else ("ported" if has_parity else "unported")
            if effect_id in explicit_dispatch:
                tier, _, destination = explicit_dispatch[effect_id]
                route = f"{tier} -> {destination}"
            else:
                route = ";".join(route_sources(effect_id, process_text, process2_text, str_only_text, init_ids, clip_ids))
        elif effect_id in clip_ids:
            status, route = "clip-wave", "kProvenClipWaves"
        elif effect_id in str_ids:
            status, route = "str-default", "RagEffectStrNameTable.generated.inc -> Handler::EzStr"
        else:
            status, route = "unported", ""
        out.append({
            "id": str(effect_id), "tier": row["tier"], "refHandler": handler,
            "status": status, "srcRoute": route,
            "launchedBy": ";".join(sorted(launched.get(effect_id, set()))),
            # Keep the final TSV cell explicit; a blank final field serializes
            # as a trailing tab and is flagged by git diff --check.
            "parityLogLine": parity_map[effect_id] if status in {"ported", "routed"} and parity_map[effect_id] else "-",
        })

    counts = Counter(row["status"] for row in out)
    launched_unported = sum(1 for row in out if row["status"] == "unported" and row["launchedBy"])
    if prior_rows:
        old = Counter(row["status"] for row in prior_rows.values())
        permitted_unported = old["unported"] + old["str-default"] + old["clip-wave"]
        newly_reclassified = sum(
            1 for effect_id in RECLASSIFIED_UNPORTED
            if effect_id in prior_rows and prior_rows[effect_id]["status"] != "unported"
        )
        permitted_unported += newly_reclassified
        if counts["routed"] + counts["ported"] + newly_reclassified < old["routed"] + old["ported"] or counts["unported"] > permitted_unported:
            raise SystemExit(f"coverage regression: previous={dict(old)} current={dict(counts)}")

    COVERAGE.parent.mkdir(parents=True, exist_ok=True)
    with COVERAGE.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=["id", "tier", "refHandler", "status", "srcRoute", "launchedBy", "parityLogLine"], delimiter="\t")
        writer.writeheader()
        writer.writerows(out)

    audit_cmd = [sys.executable, str(ROOT / "tools/audit_skill_parity.py")]
    subprocess.run(audit_cmd, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    audit_text = AUDIT.read_text(encoding="utf-8")
    audit_text = audit_text.replace("Effect bodies/primitives: incomplete; see PROGRESS.md and PARITY_LOG.md.",
        f"Effect coverage: ported={counts['ported']}, routed={counts['routed']}, clip-wave={counts['clip-wave']}, str-default={counts['str-default']}, unported={counts['unported']}, launched-but-unported={launched_unported}; see EFFECT_COVERAGE.tsv.")
    AUDIT.write_text(audit_text, encoding="utf-8", newline="\n")
    print(f"explicit routes={len(explicit_ids)}; ported={counts['ported']} routed={counts['routed']} clip-wave={counts['clip-wave']} str-default={counts['str-default']} unported={counts['unported']}")
    print(f"launched but not ported={launched_unported}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
