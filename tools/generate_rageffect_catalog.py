from __future__ import annotations

import pathlib
import re
from dataclasses import dataclass


ROOT = pathlib.Path(__file__).resolve().parents[1]
REF_CPP = ROOT / "Ref" / "RagEffect.cpp"
EFFECT_DIR = ROOT / "Ref" / "GRF-Content" / "data" / "texture" / "effect"
OUT_FILE = ROOT / "src" / "world" / "RagEffectCatalog.generated.inc"


DIRECT_RE = re.compile(r'sprintf\(StrName,\s*(a[A-Za-z0-9_]+|byte_[A-Za-z0-9_]+)\);')
VARIANT_INLINE_RE = re.compile(
    r'sprintf\(StrName,\s*"([A-Za-z0-9_]+)%d\.str",\s*([A-Za-z0-9_]+)\s*%\s*([0-9]+)\s*\+\s*([0-9]+)\);'
)
VARIANT_TRACKED_RE = re.compile(
    r'sprintf\(StrName,\s*"([A-Za-z0-9_]+)%d\.str",\s*([A-Za-z0-9_]+)\s*\+\s*([0-9]+)\);'
)
RAND_MOD_RE = re.compile(r'([A-Za-z0-9_]+)\s*=\s*rand\(\)\s*%\s*([0-9]+);')
CASE_RE = re.compile(r"case\s+(\d+):")
LABEL_RE = re.compile(r"^\$([A-Za-z0-9_]+):")
GOTO_RE = re.compile(r"goto\s+\$([A-Za-z0-9_]+);")


@dataclass
class Mapping:
    mode: str
    token: str | None = None
    variant_prefix: str | None = None
    variant_first: int = 1
    variant_count: int = 0
    min_token: str | None = None


MANUAL_TOKEN_MAP = {
    "aMagnificatMinS": "magnificat_min.str",
    "aResurrectionMi": "resurrection_min.str",
    "aLexaeternaMinS": "lexaeterna_min.str",
    "aSuffragiumMinS": "suffragium_min.str",
    "aWeaponperfecti": "weaponperfection_min.str",
    "aWeaponperfecti_0": "weaponperfection.str",
    "abyte_71A89C": "magicpower.str",
    "abyte_71A88C": "magical.str",
    "abyte_71A90C": "allow.str",
    "abyte_71A8FC": "allow2.str",
    "abyte_71A930": "moonstar.str",
    "abyte_71AB6C": "fruit.str",
    "abyte_71AB5C": "fruit_.str",
    "abyte_71AB4C": "deffender.str",
    "abyte_71AB3C": "keeping.str",
    "abyte_71A8E8": "cartrevolution.str",
    "abyte_71A8D4": "cartrevolution2.str",
    "abyte_71A8BC": "lightning.str",
    "abyte_71A8AC": "thunderstorm.str",
}


def normalize_token(token: str) -> str:
    token = token.strip()
    if token.startswith("a"):
        token = token[1:]
    token = re.sub(r"(Str|St|MinS|Mi|S)$", "", token)
    token = token.replace("_0", "").replace("_1", "").replace("_2", "").replace("_3", "")
    return re.sub(r"[^a-z0-9]+", "", token.lower())


def normalize_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", pathlib.Path(name).stem.lower())


def build_asset_index() -> dict[str, list[str]]:
    index: dict[str, list[str]] = {}
    for path in sorted(EFFECT_DIR.glob("*.str")):
        key = normalize_name(path.name)
        index.setdefault(key, []).append(path.name.lower())
    return index


def choose_asset(token: str, asset_index: dict[str, list[str]]) -> str | None:
    manual = MANUAL_TOKEN_MAP.get(token)
    if manual:
        return manual

    key = normalize_token(token)
    if not key:
        return None

    exact = asset_index.get(key)
    if exact:
        return exact[0]

    candidates: list[str] = []
    for asset_key, names in asset_index.items():
        if asset_key == key or asset_key.startswith(key) or key.startswith(asset_key) or key in asset_key or asset_key in key:
            candidates.extend(names)
    if not candidates:
        return None

    candidates = sorted(set(candidates), key=lambda name: (abs(len(normalize_name(name)) - len(key)), len(name), name))
    return candidates[0]


def parse_mappings() -> tuple[dict[str, Mapping], dict[int, Mapping]]:
    lines = REF_CPP.read_text(encoding="latin-1").splitlines()
    start_index = next(i for i, line in enumerate(lines) if "this->m_duration = durationTable[m_type];" in line)
    end_index = next(i for i, line in enumerate(lines[start_index:], start_index) if "if ( strlen(StrName) )" in line)
    lines = lines[start_index:end_index]
    label_map: dict[str, Mapping] = {}
    case_map: dict[int, Mapping] = {}

    current_cases: list[int] = []
    current_label: str | None = None
    pending_min = False
    pending_else = False
    pending_min_token: str | None = None
    rand_mods: dict[str, int] = {}

    def flush_mapping(mapping: Mapping) -> None:
        nonlocal current_cases, current_label, pending_min, pending_else, pending_min_token
        if current_label:
            label_map[current_label] = mapping
        for case_id in current_cases:
            case_map[case_id] = mapping
        current_cases.clear()
        pending_min = False
        pending_else = False
        pending_min_token = None

    for raw_line in lines:
        line = raw_line.strip()
        label_match = LABEL_RE.match(line)
        if label_match:
            current_label = label_match.group(1)
            current_cases.clear()

        case_match = CASE_RE.match(line)
        if case_match:
            current_cases.append(int(case_match.group(1)))
            continue

        rand_mod_match = RAND_MOD_RE.search(line)
        if rand_mod_match:
            rand_mods[rand_mod_match.group(1)] = int(rand_mod_match.group(2))
            continue

        if line in {"break;", "default:", "}"}:
            if line == "break;":
                current_cases.clear()
            continue

        if line.startswith("if ( g_session.m_isMinEffect )"):
            pending_min = True
            pending_else = False
            pending_min_token = None
            continue

        if line == "else":
            pending_else = True
            continue

        goto_match = GOTO_RE.search(line)
        if goto_match:
            mapping = label_map.get(goto_match.group(1))
            if mapping:
                for case_id in current_cases:
                    case_map[case_id] = mapping
                current_cases.clear()
            continue

        variant_match = VARIANT_INLINE_RE.search(line)
        if variant_match:
            mapping = Mapping(
                mode="variant",
                variant_prefix=variant_match.group(1),
                variant_first=int(variant_match.group(4)),
                variant_count=int(variant_match.group(3)),
            )
            flush_mapping(mapping)
            continue

        variant_match = VARIANT_TRACKED_RE.search(line)
        if variant_match:
            count = rand_mods.get(variant_match.group(2))
            if count:
                mapping = Mapping(
                    mode="variant",
                    variant_prefix=variant_match.group(1),
                    variant_first=int(variant_match.group(3)),
                    variant_count=count,
                )
                flush_mapping(mapping)
            continue

        direct_match = DIRECT_RE.search(line)
        if direct_match:
            token = direct_match.group(1)
            if pending_min and not pending_else:
                pending_min_token = token
                continue

            if pending_else and pending_min_token:
                mapping = Mapping(mode="direct", token=token, min_token=pending_min_token)
                flush_mapping(mapping)
                continue

            mapping = Mapping(mode="direct", token=token)
            flush_mapping(mapping)

    return label_map, case_map


def main() -> None:
    asset_index = build_asset_index()
    _, case_map = parse_mappings()

    resolved_lines: list[str] = []
    unresolved: list[str] = []
    for effect_id in sorted(case_map):
        mapping = case_map[effect_id]
        if mapping.mode == "variant":
            resolved_lines.append(
                "    { %d, nullptr, nullptr, \"%s\", %d, %d }," %
                (effect_id, mapping.variant_prefix, mapping.variant_first, mapping.variant_count)
            )
            continue

        if not mapping.token:
            unresolved.append(f"{effect_id}: missing token")
            continue

        direct_name = choose_asset(mapping.token, asset_index)
        min_name = choose_asset(mapping.min_token, asset_index) if mapping.min_token else None
        if not direct_name:
            unresolved.append(f"{effect_id}: {mapping.token}")
            continue

        direct_literal = f"\"{direct_name}\""
        min_literal = f"\"{min_name}\"" if min_name else "nullptr"
        resolved_lines.append(
            f"    {{ {effect_id}, {direct_literal}, {min_literal}, nullptr, 0, 0 }},"
        )

    header = [
        "// Generated by tools/generate_rageffect_catalog.py",
        "// Do not edit by hand.",
        "",
        "static constexpr RagEffectCatalogEntry kRagEffectCatalog[] = {",
        *resolved_lines,
        "};",
        "",
        "static constexpr const char* kRagEffectCatalogUnresolved[] = {",
    ]
    if unresolved:
        header.extend(f"    \"{entry}\"," for entry in unresolved)
    else:
        header.append("    nullptr,")
    header.extend([
        "};",
        "",
    ])

    OUT_FILE.write_text("\n".join(header), encoding="utf-8", newline="\n")
    print(f"wrote {OUT_FILE}")
    print(f"resolved: {len(resolved_lines)}")
    print(f"unresolved: {len(unresolved)}")
    if unresolved:
        for entry in unresolved[:50]:
            print(entry)


if __name__ == "__main__":
    main()
