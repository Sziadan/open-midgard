"""Generate the skill/effect tables required by docs/SKILL_SYSTEM_1TO1_IMPLEMENTATION_PLAN.md.

Run from the repo root:

    .venv/Scripts/python.exe tools/generate_skill_tables.py

Every string comes from HighPriest.exe (via tools/ro_exe_resolve.py) or from an
explicit numeric assignment in Ref/. Unresolved symbols are written to
docs/skill/BLOCKED.md and are not replaced with a guessed asset.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from skill_reference_index import read_function, dispatch_cases, switch_case_locations

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ro_exe_resolve import (  # noqa: E402
    DECOMPILE_PATH,
    ROOT,
    ExeImage,
    SymbolTable,
    align_truncated_symbols,
    cpp_string_literal,
    load_function_spans,
    resolve_playwave_calls,
)


DURATION_VA = 0x7182AC
DURATION_COUNT = 710
SKILL_EFFECT_COUNT = 1039

REF = ROOT / "Ref"
OUT_DOCS = ROOT / "docs" / "skill" / "generated"
BLOCKED_PATH = ROOT / "docs" / "skill" / "BLOCKED.md"

CASE_RE = re.compile(r"case\s+(0x[0-9A-Fa-f]+|\d+)\s*:")
DIRECT_RE = re.compile(r"sprintf\(StrName,\s*(a[A-Za-z0-9_]+|byte_[A-Za-z0-9_]+)\);")
VARIANT_INLINE_RE = re.compile(
    r'sprintf\(StrName,\s*"([A-Za-z0-9_]+)%d\.str",\s*[A-Za-z0-9_]+\s*%\s*([0-9]+)\s*\+\s*([0-9]+)\);'
)
VARIANT_TRACKED_RE = re.compile(
    r'sprintf\(StrName,\s*"([A-Za-z0-9_]+)%d\.str",\s*([A-Za-z0-9_]+)\s*\+\s*([0-9]+)\);'
)
RAND_MOD_RE = re.compile(r"([A-Za-z0-9_]+)\s*=\s*rand\(\)\s*%\s*([0-9]+);")
LABEL_RE = re.compile(r"^\$([A-Za-z0-9_]+):")
GOTO_RE = re.compile(r"goto\s+\$([A-Za-z0-9_]+);")
EQUAL_ID_RE = re.compile(r"if \( v6 == (\d+) \)")
EFFECT_ASSIGN_RE = re.compile(
    r"m_skillAttackEffectIdTable\._First\[(\d+)\] = (-?\d+);"
)
ATTACKED_EFFECT_ASSIGN_RE = re.compile(
    r"m_skillAttackedEffectIdTable\._First\[(\d+)\] = (-?\d+);"
)
WAVE_ASSIGN_RE = re.compile(
    r"assign\([^;\n]*_First(?: \+ (\d+))?, (a[A-Za-z0-9_]+|byte_[A-Za-z0-9_]+),"
)
WAVE_INDEX_RE = re.compile(r"(\w+) = [^;\n]*_First \+ (\d+);")
WAVE_INDEX0_RE = re.compile(r"(\w+) = [^;\n]*_First;")
WAVE_COPY_RE = re.compile(r"qmemcpy\((\w+)->_Ptr, (a[A-Za-z0-9_]+|byte_[A-Za-z0-9_]+),")
WAVE_SIZE_RE = re.compile(r"_First \+ (\d+),")
HANDLER_RE = re.compile(r"CRagEffect::([A-Za-z0-9_]+)\(")
SKILL_ENUM_RE = re.compile(r"\{(\d+), \"([A-Z0-9_]+)\"\}")


def function_body(path: Path, qualname: str) -> str:
    lines = path.read_text(encoding="latin-1").splitlines()
    start = None
    for index, line in enumerate(lines):
        if f"{qualname}(" in line and "Original Signature" in line:
            start = index
            break
    if start is None:
        raise SystemExit(f"did not find {qualname} in {path}")
    end = len(lines)
    for index in range(start + 1, len(lines)):
        if lines[index].startswith("// Original Signature:"):
            end = index
            break
    return "\n".join(lines[start:end])


def parse_effect_str_cases(body: str) -> tuple[dict[int, dict], list[str]]:
    start = body.find("this->m_duration = durationTable[m_type];")
    end = body.find("if ( strlen(StrName) )", start)
    if start < 0 or end < 0:
        raise SystemExit("CRagEffect::Init str switch markers were not found")
    lines = body[start:end].splitlines()
    label_map: dict[str, dict] = {}
    case_map: dict[int, dict] = {}
    current: list[int] = []
    current_label: str | None = None
    pending_min = False
    pending_else = False
    pending_min_token: str | None = None
    rand_mods: dict[str, int] = {}
    pending_gotos: list[tuple[list[int], str]] = []
    notes: list[str] = []

    def flush(mapping: dict) -> None:
        nonlocal pending_min, pending_else, pending_min_token, current_label
        if current_label:
            label_map[current_label] = mapping
        for case_id in current:
            case_map[case_id] = mapping
        current.clear()
        current_label = None
        pending_min = False
        pending_else = False
        pending_min_token = None

    for raw in lines:
        line = raw.strip()
        label = LABEL_RE.match(line)
        if label:
            # The label belongs to the case or equality test above it.
            current_label = label.group(1)
            continue
        case = CASE_RE.match(line)
        if case:
            token = case.group(1)
            current.append(int(token, 16) if token.lower().startswith("0x") else int(token))
            continue
        equal_id = EQUAL_ID_RE.search(line)
        if equal_id:
            current = [int(equal_id.group(1))]
            continue
        rand_mod = RAND_MOD_RE.search(line)
        if rand_mod:
            rand_mods[rand_mod.group(1)] = int(rand_mod.group(2))
            continue
        if line == "break;":
            current.clear()
            current_label = None
            continue
        if "m_isMinEffect" in line:
            pending_min = True
            pending_else = False
            pending_min_token = None
            continue
        if line == "else":
            pending_else = True
            continue
        goto = GOTO_RE.search(line)
        if goto:
            mapping = label_map.get(goto.group(1))
            if mapping:
                for case_id in current:
                    case_map[case_id] = mapping
            else:
                pending_gotos.append((list(current), goto.group(1)))
            current.clear()
            current_label = None
            continue
        variant = VARIANT_INLINE_RE.search(line)
        if variant:
            flush({
                "mode": "variant",
                "prefix": variant.group(1),
                "first": int(variant.group(3)),
                "count": int(variant.group(2)),
            })
            continue
        variant = VARIANT_TRACKED_RE.search(line)
        if variant:
            count = rand_mods.get(variant.group(2))
            if not count:
                notes.append(f"variant without a visible rand modulus: {line}")
                continue
            flush({
                "mode": "variant",
                "prefix": variant.group(1),
                "first": int(variant.group(3)),
                "count": count,
            })
            continue
        direct = DIRECT_RE.search(line)
        if not direct:
            continue
        token = direct.group(1)
        if pending_min and not pending_else:
            pending_min_token = token
            continue
        if pending_else and pending_min_token:
            flush({"mode": "direct", "token": token, "min_token": pending_min_token})
            continue
        flush({"mode": "direct", "token": token})
    for case_ids, label_name in pending_gotos:
        mapping = label_map.get(label_name)
        if mapping is None:
            notes.append(f"goto ${label_name} has no str assignment for cases {case_ids}")
            continue
        for case_id in case_ids:
            case_map[case_id] = mapping
    return case_map, notes


def parse_wave_table(body: str) -> tuple[int, dict[int, str], str | None]:
    size = 0
    for match in WAVE_SIZE_RE.finditer(body):
        size = max(size, int(match.group(1)))
    default_match = re.search(
        r"assign\(&_X, (a[A-Za-z0-9_]+|byte_[A-Za-z0-9_]+),",
        body,
    )
    default = default_match.group(1) if default_match else None
    values: dict[int, str] = {}
    index_by_var: dict[str, int] = {}
    for line in body.splitlines():
        assign = WAVE_ASSIGN_RE.search(line)
        if assign:
            values[int(assign.group(1) or "0")] = assign.group(2)
        indexed = WAVE_INDEX_RE.search(line)
        if indexed:
            index_by_var[indexed.group(1)] = int(indexed.group(2))
        indexed0 = WAVE_INDEX0_RE.search(line)
        if indexed0:
            index_by_var[indexed0.group(1)] = 0
        copied = WAVE_COPY_RE.search(line)
        if copied and copied.group(1) in index_by_var:
            values[index_by_var[copied.group(1)]] = copied.group(2)
    if size == 0 and values:
        size = max(values) + 1
    return size, values, default


def emit_indexed_string_table(
    path: Path,
    array_name: str,
    size: int,
    values: dict[int, str],
    default_symbol: str | None,
    symbols: SymbolTable,
    blocked: list[str],
    overrides: dict[str, object] | None = None,
) -> None:
    overrides = overrides or {}

    def text_of(symbol: str | None) -> str:
        if symbol is None:
            return "nullptr"
        resolved = overrides.get(symbol) or symbols.resolve_name(symbol)
        if resolved is None:
            blocked.append(f"{path.name} {array_name}: symbol {symbol} did not resolve")
            return "nullptr"
        return cpp_string_literal(resolved.raw)

    default_literal = text_of(default_symbol)
    lines = [
        "// Generated by tools/generate_skill_tables.py",
        "// Do not edit by hand. Strings are cp949 bytes from HighPriest.exe.",
        f"static constexpr int {array_name}Count = {size};",
        f"static const char* const {array_name}Default = {default_literal};",
        f"static const char* const {array_name}[{size}] = {{",
    ]
    for index in range(size):
        symbol = values.get(index, default_symbol)
        lines.append(f"    {text_of(symbol)},")
    lines.append("};")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def write_duration(image: ExeImage) -> list[int]:
    table = image.read_i32_table(DURATION_VA, DURATION_COUNT)
    lines = [
        "// Generated by tools/generate_skill_tables.py",
        "// Do not edit by hand. int32 table at HighPriest.exe VA 0x7182AC.",
        f"static constexpr int kRagEffectDurationCount = {DURATION_COUNT};",
        "static constexpr int kRagEffectDurationTable[kRagEffectDurationCount] = {",
    ]
    for index, value in enumerate(table):
        lines.append(f"    {value}, // {index}")
    lines.append("};")
    lines.append("")
    out = ROOT / "src" / "world" / "RagEffectDurationTable.generated.inc"
    out.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    return table


def write_str_tables(symbols: SymbolTable, blocked: list[str]) -> int:
    body = function_body(REF / "RagEffect.cpp", "CRagEffect::Init")
    case_map, notes = parse_effect_str_cases(body)
    for note in notes:
        blocked.append("CRagEffect::Init " + note)

    str_lines = [
        "// Generated by tools/generate_skill_tables.py",
        "// Do not edit by hand. Effect id -> .str name from CRagEffect::Init.",
        "struct RagEffectStrName {",
        "    int effectId;",
        "    const char* strName;",
        "    const char* minStrName;",
        "    const char* variantPrefix;",
        "    int variantFirst;",
        "    int variantCount;",
        "};",
        "static constexpr RagEffectStrName kRagEffectStrNames[] = {",
    ]
    catalog_lines = [
        "// Generated by tools/generate_skill_tables.py",
        "// Do not edit by hand.",
        "static constexpr RagEffectCatalogEntry kRagEffectCatalog[] = {",
    ]
    unresolved: list[str] = []
    for effect_id in sorted(case_map):
        mapping = case_map[effect_id]
        if mapping["mode"] == "variant":
            row = (
                f'    {{ {effect_id}, nullptr, nullptr, "{mapping["prefix"]}", '
                f'{mapping["first"]}, {mapping["count"]} }},'
            )
            str_lines.append(row)
            catalog_lines.append(row)
            continue
        resolved = symbols.resolve_name(mapping["token"])
        if resolved is None:
            unresolved.append(f"{effect_id}: {mapping['token']}")
            blocked.append(
                f"CRagEffect::Init effect {effect_id} symbol {mapping['token']} did not resolve"
            )
            continue
        min_literal = "nullptr"
        min_token = mapping.get("min_token")
        if min_token:
            min_resolved = symbols.resolve_name(min_token)
            if min_resolved is None:
                blocked.append(
                    f"CRagEffect::Init effect {effect_id} min symbol {min_token} did not resolve"
                )
            else:
                min_literal = cpp_string_literal(min_resolved.raw)
        row = (
            f"    {{ {effect_id}, {cpp_string_literal(resolved.raw)}, {min_literal}, "
            "nullptr, 0, 0 },"
        )
        str_lines.append(row)
        catalog_lines.append(row)
    str_lines.extend(["};", ""])
    catalog_lines.append("};")
    catalog_lines.append("")
    catalog_lines.append("static constexpr const char* kRagEffectCatalogUnresolved[] = {")
    if unresolved:
        catalog_lines.extend(f'    "{entry}",' for entry in unresolved)
    else:
        catalog_lines.append("    nullptr,")
    catalog_lines.extend(["};", ""])
    (ROOT / "src" / "world" / "RagEffectStrNameTable.generated.inc").write_text(
        "\n".join(str_lines), encoding="utf-8", newline="\n"
    )
    (ROOT / "src" / "world" / "RagEffectCatalog.generated.inc").write_text(
        "\n".join(catalog_lines), encoding="utf-8", newline="\n"
    )
    return len(case_map)


def write_effect_id_array(name: str, values: list[int]) -> list[str]:
    lines = [
        f"static constexpr int {name}[kSkillAttackEffectIdCount] = {{",
    ]
    lines.extend(f"    {value}," for value in values)
    lines.append("};")
    return lines


def write_attack_effect_table() -> None:
    body = function_body(REF / "Session2.cpp", "CSession::InitSkillEffectIdTable")
    attack = [-1] * SKILL_EFFECT_COUNT
    attacked = [-1] * SKILL_EFFECT_COUNT
    for index_text, value_text in EFFECT_ASSIGN_RE.findall(body):
        index = int(index_text)
        if index < SKILL_EFFECT_COUNT:
            attack[index] = int(value_text)
    for index_text, value_text in ATTACKED_EFFECT_ASSIGN_RE.findall(body):
        index = int(index_text)
        if index < SKILL_EFFECT_COUNT:
            attacked[index] = int(value_text)
    # `*table._First = N` is index 0. The decompiler emits it that way.
    for table_name, dest in (
        ("m_skillAttackEffectIdTable", attack),
        ("m_skillAttackedEffectIdTable", attacked),
    ):
        star = re.search(
            rf"\*this->{table_name}\._First = (-?\d+);",
            body,
        )
        if star:
            dest[0] = int(star.group(1))
    lines = [
        "// Generated by tools/generate_skill_tables.py",
        "// Do not edit by hand. CSession::InitSkillEffectIdTable.",
        "// Attack table default is -1. Attacked table is filled with -1, then overwritten.",
        f"static constexpr int kSkillAttackEffectIdCount = {SKILL_EFFECT_COUNT};",
    ]
    lines.extend(write_effect_id_array("kSkillAttackEffectIdTable", attack))
    lines.append("")
    lines.extend(write_effect_id_array("kSkillAttackedEffectIdTable", attacked))
    lines.append("")
    (ROOT / "src" / "session" / "SkillAttackEffectIdTable.generated.inc").write_text(
        "\n".join(lines), encoding="utf-8", newline="\n"
    )


def write_wave_tables(image: ExeImage, symbols: SymbolTable, spans, blocked: list[str]) -> None:
    specs = [
        ("CSession::InitJobHitWaveName", "kJobHitWaveName", ROOT / "src" / "session" / "JobHitWaveName.generated.inc"),
        ("CSession::InitWeaponHitWaveName", "kWeaponHitWaveName", ROOT / "src" / "session" / "WeaponHitWaveName.generated.inc"),
        ("CSession::InitWeaponSwingWaveName", "kWeaponSwingWaveName", ROOT / "src" / "session" / "WeaponSwingWaveName.generated.inc"),
    ]
    session = REF / "Session.cpp"
    addresses = sorted({span.va for span in spans})
    for qualname, array_name, path in specs:
        body = function_body(session, qualname)
        size, values, default = parse_wave_table(body)
        start = next((span.va for span in spans if span.name == qualname), None)
        overrides = {}
        if start is not None:
            later = [va for va in addresses if va > start]
            end = later[0] if later else start + 0x2000
            overrides = align_truncated_symbols(body, start, end, image, symbols)
        emit_indexed_string_table(path, array_name, size, values, default, symbols, blocked, overrides)


def write_playwave(image: ExeImage, symbols: SymbolTable, spans, blocked: list[str]) -> tuple[int, int]:
    calls = resolve_playwave_calls(image, symbols, spans)
    rows = ["callVA\tfunction\tstringVA\tstring\tstatus"]
    unresolved = 0
    for call in calls:
        if call.status != "ok":
            unresolved += 1
        string_va = "" if call.string_va is None else f"0x{call.string_va:X}"
        rows.append(
            f"0x{call.call_va:X}\t{call.function}\t{string_va}\t{call.string or ''}\t{call.status}"
        )
    (OUT_DOCS / "playwave_callsites.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    runtime_ports = {}
    for path in (ROOT / "src").rglob("*.cpp"):
        for line in path.read_text(encoding="utf-8").splitlines():
            if "// Ref runtime-wave:" in line:
                for address in re.findall(r"0x[0-9A-Fa-f]+", line):
                    runtime_ports[int(address, 16)] = path.relative_to(ROOT).as_posix()
    runtime_rows = ["callVA\tfunction\tsrcAnchor\tstatus"]
    for call in calls:
        if call.status != "ok":
            anchor = runtime_ports.get(call.call_va, "")
            status = "port-anchor-present (runtime parity unverified)" if anchor else "producer port pending"
            runtime_rows.append(f"0x{call.call_va:X}\t{call.function}\t{anchor}\t{status}")
            if not anchor:
                blocked.append(
                    f"PlayWave at 0x{call.call_va:X} in {call.function or '(no function span)'} "
                    "uses a runtime name; trace and port its producer (not an unrecoverable literal)"
                )
    (OUT_DOCS / "runtime_wave_callsites.tsv").write_text("\n".join(runtime_rows) + "\n", encoding="utf-8", newline="\n")
    return len(calls), unresolved


def write_effect_handler_index() -> int:
    rows = ["tier\tsource\tcase\thandler\tsourceLine"]
    for tier, path, expression in (
        ("OnProcess", DECOMPILE_PATH, "v6"),
        ("OnProcess2", REF / "RagEffect.cpp", "this->m_type"),
        ("OnProcess3", REF / "RagEffect.cpp", "this->m_type"),
    ):
        function = read_function(path, f"CRagEffect::{tier}")
        cases = dispatch_cases(function, expression)
        for case in cases:
            handlers = ",".join(case.handlers) or "inline"
            rows.append(f"{tier}\t{path.relative_to(ROOT).as_posix()}\t{case.id}\t{handlers}\t{case.line}")
    (OUT_DOCS / "effect_handler_index.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    return len(rows) - 1


def write_skill_index() -> int:
    sources = [
        (REF / "Session2.cpp", "CSession::GetSkillActionInfo"),
        (REF / "Session2.cpp", "CSession::GetSkillActionInfo2"),
        (REF / "Session2.cpp", "CSession::LaunchSkillSpecialEffect"),
        (REF / "Session2.cpp", "CSession::InitSkillEffectIdTable"),
        (REF / "GameActorMsgHandler.cpp", "CGameActor::Am_Skill"),
        (REF / "GameActorMsgHandler.cpp", "CGameActor::Am_Groundskill"),
        (REF / "GameModePacket.cpp", "CGameMode::Zc_Use_Skill"),
        (REF / "GameModePacket.cpp", "CGameMode::Zc_Notify_Groundskill"),
        (REF / "Skill.cpp", "CSkill::SendMsg"),
    ]
    enum_text = (ROOT / "src" / "skill" / "SkillEnumIdTable.inc").read_text(encoding="utf-8")
    names = {int(item[0]): item[1] for item in SKILL_ENUM_RE.findall(enum_text)}
    # Only switch expressions that denote a skill id count. A level, action,
    # actor message, or ground-unit job is not a skill id.
    expressions = {
        "CSession::GetSkillActionInfo": {"skillId"},
        "CSession::GetSkillActionInfo2": {"skillId"},
        "CSession::LaunchSkillSpecialEffect": {"skillId"},
        "CGameActor::Am_Skill": {"skillId", "v10", "v36"},
        "CGameActor::Am_Groundskill": {"v7"},
        "CGameMode::Zc_Use_Skill": {"*(_WORD *)(v2 + 2)"},
        "CGameMode::Zc_Notify_Groundskill": {"*(_WORD *)(LODWORD(v2) + 2)"},
        "CSkill::SendMsg": {"skillId"},
    }
    # The enum contains the server's e_skill values (including third jobs).
    # Reserved ranges requested by the plan also need explicit inventory rows.
    inventory = set(names) | set(range(8001, 8047)) | set(range(8201, 8239)) | set(range(10000, 10015))
    rows = ["skillId\tname\tfunction\tcase\tline\tsource"]
    for skill_id in sorted(inventory):
        rows.append(f"{skill_id}\t{names.get(skill_id, '')}\tcatalog\t\t\tsrc/skill/SkillEnumIdTable.inc")
    for path, qualname in sources:
        function = read_function(path, qualname)
        source = path.relative_to(ROOT).as_posix()
        if qualname.endswith("InitSkillEffectIdTable"):
            for number, line in enumerate(function.text.splitlines(), function.first_line):
                assign = EFFECT_ASSIGN_RE.search(line) or ATTACKED_EFFECT_ASSIGN_RE.search(line)
                if assign:
                    skill_id = int(assign.group(1))
                    rows.append(f"{skill_id}\t{names.get(skill_id, '')}\t{qualname}\tassign\t{number}\t{source}")
            continue
        for skill_id, expression, number in switch_case_locations(function):
            if expression in expressions.get(qualname, set()):
                rows.append(f"{skill_id}\t{names.get(skill_id, '')}\t{qualname}\t{skill_id}\t{number}\t{source}")
    (OUT_DOCS / "skill_behaviour_index.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    return len(rows) - 1


def write_blocked(blocked: list[str]) -> None:
    unique = list(dict.fromkeys(blocked))
    begin, end = "<!-- generated:begin -->", "<!-- generated:end -->"
    lines = [begin]
    if unique:
        lines.extend(f"- {item}" for item in unique)
    else:
        lines.append("None.")
    lines.append(end)
    generated = "\n".join(lines)
    existing = BLOCKED_PATH.read_text(encoding="utf-8") if BLOCKED_PATH.exists() else ""
    if begin in existing and end in existing:
        start, stop = existing.index(begin), existing.index(end) + len(end)
        document = existing[:start] + generated + existing[stop:]
    else:
        document = ("# Blocked skill-system items\n\n"
            "Runtime-name entries require a producer port; they do not establish that an asset name is unrecoverable.\n"
            "Source anchors already ported are listed separately in `generated/runtime_wave_callsites.tsv`; none is a live-verification claim.\n\n"
            + generated + "\n\n## Manually reviewed dependencies\n\nNone recorded.\n")
    BLOCKED_PATH.write_text(document, encoding="utf-8", newline="\n")


def validate(image: ExeImage, symbols: SymbolTable, duration: list[int]) -> list[str]:
    errors: list[str] = []
    tracking = symbols.resolve_name("byte_71A89C")
    if tracking is None or tracking.text != "트랙킹.str":
        errors.append(f"byte_71A89C resolved to {tracking!r}, expected 트랙킹.str")
    expected = {
        0: 50, 6: 100, 12: 40, 13: 9999, 22: 270, 23: 999,
        158: 999, 312: 100, 337: 200, 371: 200, 700: 36000, 709: 9999,
    }
    for index, value in expected.items():
        if duration[index] != value:
            errors.append(f"duration[{index}] = {duration[index]}, expected {value}")
    firewall = symbols.resolve_name("aEffectEfFirewa")
    if firewall is None or firewall.text != "effect\\EF_FireWall.wav":
        errors.append(f"aEffectEfFirewa resolved to {firewall!r}")
    return errors


def main() -> None:
    OUT_DOCS.mkdir(parents=True, exist_ok=True)
    image = ExeImage()
    symbols = SymbolTable(image)
    spans = load_function_spans()
    blocked: list[str] = []
    duration = write_duration(image)
    str_cases = write_str_tables(symbols, blocked)
    write_attack_effect_table()
    write_wave_tables(image, symbols, spans, blocked)
    call_count, unresolved_calls = write_playwave(image, symbols, spans, blocked)
    handler_rows = write_effect_handler_index()
    skill_rows = write_skill_index()
    errors = validate(image, symbols, duration)
    write_blocked(blocked)
    report = [
        f"duration entries: {len(duration)}",
        f"str cases parsed: {str_cases}",
        f"playwave calls: {call_count} unresolved: {unresolved_calls}",
        f"effect handler rows: {handler_rows}",
        f"skill behaviour rows: {skill_rows}",
        f"blocked: {len(blocked)}",
        f"validation errors: {len(errors)}",
    ]
    report.extend(errors)
    text = "\n".join(report) + "\n"
    (OUT_DOCS / "WP0_VALIDATION.txt").write_text(text, encoding="utf-8", newline="\n")
    print(text, end="")
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
