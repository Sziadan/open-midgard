#!/usr/bin/env python3
"""Compare the canonical EXE status-icon table with the checked-in port."""
from __future__ import annotations

import csv
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

EXPECTED_SHA256 = "9ABC36C88376387D85C878C0C879D9AF8EF66EB1F5152638085DC6995E3F4742"
EXE = ROOT / "HighPriest.exe"
TSV = ROOT / "tools/tests/status_state_exe_map.tsv"
GENERATED = ROOT / "src/world/StatusStateIconMap.generated.inc"


def cpp_literal(value: str) -> str:
    raw = value.encode("cp949")
    chunks: list[str] = []
    ascii_chunk: list[str] = []
    for byte in raw:
        if byte < 128:
            ascii_chunk.append(chr(byte))
        else:
            if ascii_chunk:
                chunks.append('"' + "".join(ascii_chunk).replace("\\", "\\\\").replace('"', '\\"') + '"')
                ascii_chunk.clear()
            chunks.append(f'"\\{byte:03o}"')
    if ascii_chunk or not chunks:
        chunks.append('"' + "".join(ascii_chunk).replace("\\", "\\\\").replace('"', '\\"') + '"')
    return " ".join(chunks)


def expected_include(rows: list[dict[str, str]]) -> str:
    lines = [
        "// Generated from HighPriest.exe GetStateFileName return strings. Do not edit by hand.",
        "static const StatusStateIconEntry kStatusStateIconEntries[] = {",
    ]
    for row in rows:
        special = "nullptr" if not row["special_path"] else cpp_literal(row["special_path"])
        lines.append(
            f"    {{{row['priority']}, {row['status']}, {cpp_literal(row['standard_path'])}, {special}}},"
        )
    lines.append("};")
    return "\n".join(lines) + "\n"


def canonical_explicit_cases(session_source: str) -> set[tuple[int, int]]:
    start = session_source.index("char *__thiscall CSession::GetStateFileName")
    next_function = session_source.index("\n// ================================================", start + 10)
    function = session_source[start:next_function]
    switch = re.search(r"switch\s*\(\s*priority\s*\)\s*\{", function)
    assert switch, "canonical GetStateFileName priority switch missing"
    open_brace = function.index("{", switch.start())
    depth = 1
    cursor = open_brace + 1
    while depth:
        depth += (function[cursor] == "{") - (function[cursor] == "}")
        cursor += 1
    body = function[open_brace + 1 : cursor - 1]

    blocks: dict[int, str] = {}
    depth = 0
    active_priority: int | None = None
    block_start = 0
    token_pattern = re.compile(r"[{}]|case\s+(\d+)\s*:")
    for token in token_pattern.finditer(body):
        if token.group(0) == "{":
            depth += 1
        elif token.group(0) == "}":
            depth -= 1
        elif depth == 0 and token.group(1) and int(token.group(1)) in range(4):
            if active_priority is not None:
                blocks[active_priority] = body[block_start : token.start()]
            active_priority = int(token.group(1))
            block_start = token.end()
    assert active_priority is not None
    blocks[active_priority] = body[block_start:]
    return {
        (priority, int(case))
        for priority, block in blocks.items()
        for case in re.findall(r"case\s+(\d+)\s*:", block)
    }


def main() -> int:
    exe_bytes = EXE.read_bytes()
    actual_hash = hashlib.sha256(exe_bytes).hexdigest().upper()
    assert actual_hash == EXPECTED_SHA256, f"unexpected reference executable hash: {actual_hash}"
    rows = list(csv.DictReader(TSV.open(encoding="utf-8"), delimiter="\t"))
    pairs = [(int(row["priority"]), int(row["status"])) for row in rows]
    assert len(rows) == 192, f"expected 192 effective pairs, found {len(rows)}"
    assert len(set(pairs)) == 192, "duplicate priority/status pair in map"
    session_source = (ROOT / "Ref/Session2.cpp").read_text(encoding="cp949", errors="replace")
    explicit_pairs = canonical_explicit_cases(session_source)
    conditional_pairs = {(0, 243), (3, 41), (3, 42), (3, 61), (3, 64), (3, 289)}
    assert len(explicit_pairs) == 186, f"expected 186 explicit canonical cases, found {len(explicit_pairs)}"
    assert set(pairs) - conditional_pairs == explicit_pairs, "PE inventory and canonical Ref case labels differ"
    assert conditional_pairs <= set(pairs), "conditional/range-only PE cases missing from inventory"

    image = ExeImage(EXE)
    for row in rows:
        for va_column, path_column in (("standard_va", "standard_path"), ("special_va", "special_path")):
            va = row[va_column]
            if not va:
                assert not row[path_column]
                continue
            resolved = image.read_cstring_at(int(va, 16))
            assert resolved is not None, f"unreadable EXE path at {va}"
            assert resolved.text == row[path_column], (
                f"EXE string mismatch at {va}: {resolved.text!r} != {row[path_column]!r}"
            )

    generated = GENERATED.read_text(encoding="utf-8")
    assert generated == expected_include(rows), "generated C++ map differs from PE-address crosswalk"

    source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
    decomp = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8")
    primitive_decomp = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8")
    caller_start = source.index("void CRagEffect::SpawnStatusState()")
    caller = source[caller_start : source.index("void CRagEffect::SendMsg(", caller_start)]
    canonical = decomp[decomp.index("void __thiscall CRagEffect::StatusState"):]
    canonical = canonical[: canonical.index("\n}") + 2]
    required_source = (
        "if ( this->m_stateCnt == 9998 )",
        "this->m_count = 0;",
        "v2->CGameObject::SendMsg(this, 0, 25, 100, 0, 0);",
        "if ( !this->m_stateCnt )",
        "if ( 35 * v3 + 150 < height )",
        "g_renderer->m_width + x - 32",
        "y + 150",
        "if ( cnt < 99 )",
    )
    for token in required_source:
        assert token in canonical, f"canonical status-state branch missing: {token}"
    for token in (
        "if (m_stateCnt == 9998)",
        "SendMsg(nullptr, 25, 100, 0, 0)",
        "if (m_stateCnt != 0)",
        "for (int priority = 0; priority < 4; ++priority)",
        "icon.statusType == 42 ? 12 : icon.statusType",
        "35 * visibleCount + 150 < g_renderer.m_height - 50",
        "g_renderer.m_width + x - 32",
        "y + 150",
        "drawList.size() < 99",
    ):
        assert token in caller, f"current status-state branch missing: {token}"
    assert "void SpawnStatusState();" in header
    assert "if (m_type == 164)" in source
    assert "if (m_type == PP_2DTEXTURE && (m_pattern & 4) != 0)" in source
    assert "RenderReferenceScreenTexture(*this);" in source

    render_start = primitive_decomp.index("void __thiscall CEffectPrim::Render2DTexture")
    render_ref = primitive_decomp[render_start:]
    render_ref = render_ref[: render_ref.index("\n}") + 2]
    renderer = source[source.index("void RenderReferenceScreenTexture("):]
    renderer = renderer[: renderer.index("\n}\n", renderer.index("void RenderReferenceScreenTexture(")) + 2]
    for token in (
        "(this->m_pattern & 1) != 0",
        "v5 = m_heightSize * -2.0",
        "v5 = -m_heightSize",
        "sinDeg = GetSin((__int64)this->m_roll)",
        "Cos = GetCos((__int64)this->m_roll)",
        "rx = v27 - v31",
        "rx_4 = v7 - v31",
        "rx_8 = v27 - v30",
        "rx_12 = v26 - v30",
        "this->m_rp[0].verts[0].x = rx_8 + this->m_pos.x - 0.5",
        "this->m_rp[0].verts[1].x = rx + this->m_pos.x - 0.5",
        "this->m_rp[0].verts[2].tu = 1.0",
        "this->m_rp[1].verts[2].x = rx_4 + this->m_pos.x - 0.5",
        "CRenderer::AddRP(g_renderer, &this->m_rp[1], 1)",
    ):
        assert token in render_ref, f"canonical screen-texture branch missing: {token}"
    for token in (
        "(prim.m_pattern & 1) != 0",
        "static_cast<double>(h) * -2.0",
        "-static_cast<double>(h)",
        "GetSin(static_cast<int>(prim.m_roll))",
        "GetCos(static_cast<int>(prim.m_roll))",
        "const float localX[4]",
        "const double localY[4]",
        "const int order[2][3] = {{0,1,2},{2,1,3}}",
        "const float uv[4][2] = {{0,1},{0,0},{1,1},{1,0}}",
        "vertex.z = 0.000001f; vertex.oow = 0.99999899f",
        "g_renderer.AddReferenceEffectTriangle(face, 1)",
    ):
        assert token in renderer, f"current screen-texture branch missing: {token}"

    print(f"status-state EXE/source/implementation comparison passed: {len(rows)} pairs, {len(explicit_pairs)} explicit cases, {len(conditional_pairs)} conditional/range pairs, {sum(bool(r['special_path']) for r in rows)} job-specific pairs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
