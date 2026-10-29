#!/usr/bin/env python3
"""Build the WP8 per-skill evidence matrix without claiming runtime parity."""
from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENUM_PATH = ROOT / "src/skill/SkillEnumIdTable.inc"
SERVER_ENUM_PATH = ROOT / "Ref/eAthena_src_2011/map/skill.h"
BEHAVIOUR_PATH = ROOT / "docs/skill/generated/skill_behaviour_index.tsv"
EXE_C_PATH = ROOT / "HighPriest.exe.c"
EXE_PATH = ROOT / "HighPriest.exe"
OUT_PATH = ROOT / "docs/skill/SKILL_MATRIX.md"


def extract_body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start)
    depth = 0
    for end in range(opening, len(text)):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1 : end]
    raise ValueError(f"unterminated function: {signature}")


def parse_server_enum(text: str) -> dict[int, str]:
    match = re.search(r"enum e_skill\s*\{(.*?)\n\};", text, re.S)
    if not match:
        raise ValueError("Ref/eAthena e_skill enum not found")
    result: dict[int, str] = {}
    next_id: int | None = None
    for line in match.group(1).splitlines():
        line = line.split("//", 1)[0].strip()
        if not line:
            continue
        for item in line.split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                name, value = (part.strip() for part in item.split("=", 1))
                next_id = int(value, 0)
            else:
                name = item
                next_id = 1 if next_id is None else next_id
            if next_id in result:
                raise ValueError(f"duplicate e_skill id {next_id}")
            result[next_id] = name
            next_id += 1
    return result


def parse_source_enum(text: str) -> dict[int, str]:
    return {int(skill_id): name for skill_id, name in re.findall(r'\{(\d+),\s*"([^"]+)"\}', text)}


def exe_named_tables(exe_c: str, exe_bytes: bytes) -> dict[int, str]:
    strings = {
        symbol: value
        for symbol, value in re.findall(r'char\s+(a\w+)\[\d+\]\s*=\s*"([A-Z0-9_-]+)";', exe_c)
        if value.encode("ascii") + b"\0" in exe_bytes
    }
    result: dict[int, str] = {}
    definitions = (
        ("guild", "void __cdecl SKILL_INFO::InitGuildTable()", "s_guildSkillIdNameList", 10000, 0),
        ("homunculus", "void __cdecl SKILL_INFO::InitHomunTable()", "s_homunSkillIdNameList", 8000, 0),
    )
    for label, signature, vector_name, base_id, id_offset in definitions:
        body = extract_body(exe_c, signature)
        # Ref confirms these exact EXE initialization lists and ordering.
        ref_text = (ROOT / "Ref/SkillInfo.cpp").read_text(encoding="utf-8", errors="replace")
        ref_body = extract_body(ref_text, signature)
        if re.sub(r"\s+", " ", body).strip() != re.sub(r"\s+", " ", ref_body).strip():
            raise ValueError(f"EXE/Ref {label} skill-name initialization differs")
        assignment = re.compile(
            rf"\(\(_DWORD \*\)SKILL_INFO::{re.escape(vector_name)}\._First \+ (\d+)\) = (\w+);"
        )
        for index, symbol in assignment.findall(body):
            index = int(index)
            name = strings.get(symbol)
            if name is None:
                raise ValueError(f"EXE string for {label} skill name {symbol} was not found")
            skill_id = base_id + index - id_offset
            result[skill_id] = name
        first = re.search(rf"\*SKILL_INFO::{re.escape(vector_name)}\._First = (\w+);", body)
        if first and id_offset == 0:
            name = strings.get(first.group(1))
            if name is None:
                raise ValueError(f"EXE string for {label} skill name {first.group(1)} was not found")
            result[base_id] = name
    return result


def markdown_cell(value: str) -> str:
    return value.replace("|", "\\|").replace("\n", "<br>")


def main() -> None:
    server_enum = parse_server_enum(SERVER_ENUM_PATH.read_text(encoding="utf-8", errors="replace"))
    source_enum = parse_source_enum(ENUM_PATH.read_text(encoding="utf-8", errors="replace"))
    if server_enum != source_enum:
        raise SystemExit("Ref/eAthena e_skill and current SkillEnumIdTable.inc differ")

    exe_c = EXE_C_PATH.read_text(encoding="utf-8", errors="replace")
    exe_bytes = EXE_PATH.read_bytes()
    extra_names = exe_named_tables(exe_c, exe_bytes)
    names = dict(source_enum)
    names.update(extra_names)
    if b"SKILL-UNKNOWN\0" not in exe_bytes:
        raise SystemExit("canonical EXE fallback skill name not found")

    union = set(source_enum)
    union.update(range(8001, 8047))
    union.update(range(8201, 8239))
    union.update(range(10000, 10015))
    if len(union) != 1045:
        raise SystemExit(f"unexpected WP8 ID union size: {len(union)}")

    evidence: dict[int, list[dict[str, str]]] = defaultdict(list)
    indexed_ids: set[int] = set()
    with BEHAVIOUR_PATH.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            skill_id = int(row["skillId"])
            if skill_id in union:
                indexed_ids.add(skill_id)
                if row["function"] != "catalog":
                    evidence[skill_id].append(row)
    missing_index_ids = sorted(union - indexed_ids - set(extra_names))
    unknown_name_ids = sorted(skill_id for skill_id in union if skill_id not in names)
    # Several IDs in the plan's homunculus range have no name row in the EXE;
    # the canonical client resolves those through its SKILL-UNKNOWN fallback.
    if any(skill_id < 8017 or skill_id >= 8047 for skill_id in unknown_name_ids):
        raise SystemExit(f"unexpected IDs without EXE/Ref names: {unknown_name_ids[:20]}")

    current_paths = {
        "CSession::GetSkillActionInfo": "src/session/SkillActionInfo.cpp",
        "CSession::GetSkillActionInfo2": "src/session/SkillActionInfo2.cpp",
        "CSession::InitSkillEffectIdTable": "src/session/Session.cpp",
        "CSession::LaunchSkillSpecialEffect": "src/session/Session.cpp",
        "CGameMode::Zc_Use_Skill": "src/gamemode/GameModePacket.cpp",
        "CGameMode::Zc_Notify_Groundskill": "src/gamemode/GameModePacket.cpp",
        "CGameActor::Am_Skill": "src/world/GameActorMsgHandler.cpp",
        "CGameActor::Am_Groundskill": "src/world/GameActorMsgHandler.cpp",
        "CSkill::SendMsg": "src/world/SkillUnit.cpp",
    }
    for function, relative_path in current_paths.items():
        if not (ROOT / relative_path).is_file():
            raise SystemExit(f"current implementation path missing for {function}: {relative_path}")

    columns = [
        "id", "name", "packets", "cast begin effect / motion", "caster effects",
        "target effects", "ground effects / units", "sounds", "Ref anchors", "src functions", "verified",
    ]
    output = [
        "# Per-skill verification matrix",
        "",
        "Generated from `generated/skill_behaviour_index.tsv`, the canonical EXE skill-name tables, and the WP8 ID ranges. This is an evidence tracker, not a parity pass.",
        "Effect, packet, and sound columns are source-traced only where row-specific evidence is recorded; unfilled rows remain explicitly untraced.",
        "",
        f"Rows: **{len(union)}** required skill IDs. Static behavior-index rows: **{sum(map(len, evidence.values()))}**. Rows are blocked until packet/effect/audio columns are traced and the plan-required RunningServer or replay log is attached.",
        "",
        "`SKILL-UNKNOWN (EXE fallback)` means the ID is in WP8's requested homunculus range but the canonical client has no name-table entry for it. Names outside the EXE tables use the Ref/eAthena enum label and are explicitly marked.",
        "",
        "| " + " | ".join(columns) + " |",
        "|" + "|".join(["---"] * len(columns)) + "|",
    ]

    for skill_id in sorted(union):
        name = names.get(skill_id)
        if name is None:
            name = "SKILL-UNKNOWN (EXE fallback)" if 8017 <= skill_id <= 8046 else f"ID_{skill_id} (no EXE/Ref name mapping)"
        elif skill_id in source_enum and name.encode("ascii") + b"\0" not in exe_bytes:
            name += " (Ref/eAthena label; no EXE name string)"

        records = evidence.get(skill_id, [])
        ref_anchors = []
        src_functions = []
        candidates: dict[str, set[str]] = defaultdict(set)
        for row in records:
            function = row["function"]
            source = row["source"]
            line = row["line"]
            case = row["case"]
            anchor = f"{source}:{line}" if line else source
            if case:
                anchor += f" (case {case})"
            if anchor:
                ref_anchors.append(f"{function}@{anchor}")
            if function in current_paths:
                src_functions.append(f"{function}->{current_paths[function]}")
            if function in ("CSession::GetSkillActionInfo", "CSession::GetSkillActionInfo2"):
                candidates["cast"].add(function)
            elif function in ("CSession::LaunchSkillSpecialEffect", "CGameActor::Am_Skill", "CGameMode::Zc_Use_Skill"):
                candidates["caster"].add(function)
            elif function == "CSession::InitSkillEffectIdTable":
                candidates["target"].add(function)
            elif function in ("CGameActor::Am_Groundskill", "CGameMode::Zc_Notify_Groundskill", "CSkill::SendMsg"):
                candidates["ground"].add(function)

        cast = "; ".join(sorted(candidates["cast"])) or "No Ref anchor in behavior index; source review pending"
        caster = "; ".join(sorted(candidates["caster"])) or "No Ref anchor in behavior index; source review pending"
        target = "; ".join(sorted(candidates["target"])) or "No Ref anchor in behavior index; source review pending"
        ground = "; ".join(sorted(candidates["ground"])) or "No Ref anchor in behavior index; source review pending"
        row = [
            str(skill_id),
            name,
            "Not traced per skill (WP8 server `clif_skill_*` walk pending)",
            cast,
            caster,
            target,
            ground,
            "Not joined to per-skill producers yet",
            "; ".join(sorted(set(ref_anchors))) or "No Ref behavior anchor in generated index",
            "; ".join(sorted(set(src_functions))) or "Current implementation route not mapped",
            "BLOCKED(no RunningServer/replay evidence captured)",
        ]
        if skill_id == 5:
            # First row with a complete source/replay receive-path trace. Keep
            # it blocked until a server capture demonstrates the producer.
            row[2] = "SM_BASH -> skill_castend_damage_id -> skill_attack -> clif_skill_damage; PACKETVER<3 emits 0x0114 (31 bytes); client receive replay: replay/bash5.tsv"
            row[3] = "GetSkillActionInfo: begin effect16/motion2; GetSkillActionInfo2: motion2"
            row[4] = "Action5 with a target returns through WBA message96 before caster effects; no Bash-specific caster effect on this replay path"
            row[5] = "Action5 WBA effectId1/effectId2=-1/-1; attack/target tables are -1/1, but the action5 fast path bypasses those table values"
            row[6] = "None for the targeted Bash damage path"
            row[7] = "_enemy_hit_normal2.wav in replay; generic message96 hit-wave lookup uses target weapon type -1, not a Bash-specific wave"
            row[8] = "; ".join([
                "skill_castend_damage_id@Ref/eAthena_src_2011/map/skill.c:2432 (function), 2474 (SM_BASH case)",
                "skill_attack damage emission@Ref/eAthena_src_2011/map/skill.c:1811",
                "clif_skill_damage@Ref/eAthena_src_2011/map/clif.c:4653",
                "CSession::GetSkillActionInfo@Ref/Session2.cpp:4086 (case 5)",
                "CSession::GetSkillActionInfo2@Ref/Session2.cpp:4762 (case 5)",
                "CSession::InitSkillEffectIdTable@Ref/Session2.cpp:6226, 6463 (case 5)",
                "CGameActor::Am_Skill@Ref/GameActorMsgHandler.cpp:883 (action 5)",
                "HandleSkillDamageNotify@Ref/GameModePacket.cpp:21262",
                "WBA hit-wave lookup@Ref/GameActor.cpp:0x55EA45",
            ])
            row[9] = "; ".join([
                "skill action tables->src/session/SkillActionInfo.cpp, src/session/SkillActionInfo2.cpp, src/session/Session.cpp",
                "CGameActor::Am_Skill->src/world/GameActorMsgHandler.cpp",
                "HandleSkillDamageNotify->src/gamemode/GameModePacket.cpp",
                "WBA delivery wave lookup->src/world/GameActor.cpp:6837-6839",
            ])
            row[10] = "BLOCKED(RunningServer Bash5 capture required; replay covers client receive path only)"
        output.append("| " + " | ".join(markdown_cell(value) for value in row) + " |")

    OUT_PATH.write_text("\n".join(output) + "\n", encoding="utf-8", newline="\n")
    print(f"Generated {OUT_PATH.relative_to(ROOT)}: {len(union)} rows; {len(evidence)} IDs have indexed Ref behavior anchors; {len(unknown_name_ids)} requested-range IDs use the EXE unknown-name fallback.")


if __name__ == "__main__":
    main()
