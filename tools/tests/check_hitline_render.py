#!/usr/bin/env python3
"""Mechanical Ref/EXE/current comparison for PP_HITLINE rendering."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function: {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


def ordered_tokens(source: str, tokens: tuple[str, ...], label: str) -> None:
    cursor = -1
    for token in tokens:
        cursor = source.find(token, cursor + 1)
        assert cursor >= 0, f"{label} omits or reorders source operation: {token}"


def main() -> int:
    ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8")
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")

    signature = "void __thiscall CEffectPrim::RenderHitLine(CEffectPrim *this, matrix *wtm)"
    ref_render = function_body(ref, signature)
    exe_render = function_body(exe, signature)
    assert compact(ref_render) == compact(exe_render), "Ref RenderHitLine differs from canonical executable"
    assert "PP_HITLINE = 0x46" in header

    render_signature = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    exe_dispatch = function_body(exe, render_signature)
    route_start = exe_dispatch.index("case 0x46:")
    route_end = exe_dispatch.index("case 0x47:", route_start)
    assert "RenderHitLine(this, wtm)" in exe_dispatch[route_start:route_end]

    current_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    current_route = "if (m_type == PP_HITLINE)"
    assert current_route in current_dispatch, "PP_HITLINE route is missing"
    route_start = current_dispatch.index(current_route)
    route_end = current_dispatch.index("if (m_type == PP_HITLINE3)", route_start)
    assert "RenderHitLine(*this, *viewMatrix);" in current_dispatch[route_start:route_end]
    assert route_start < current_dispatch.index("switch (m_type)")

    helper = function_body(current, "void RenderHitLine(const CEffectPrim& prim, const matrix& viewMatrix)")
    ordered_tokens(helper, (
        "if (gi.life != 1 || gi.alphaB <= 0 || gi.process <= 0) continue;",
        "for (int segment = 0; segment < 20; ++segment)",
        "const int historyCount = gi.flag[static_cast<size_t>(historyIndex)];",
        "distance += static_cast<double>(gi.distance) * 2.0 * decay;",
        "decay *= 0.69999999;",
        "if (height > 0.0f)",
        "height) * 0.011111111",
        "* static_cast<double>(gi.distance) * decay;",
        "* static_cast<double>(gi.distance) * -1.5 * decay;",
        "GetReferenceAngle(offsetA.x - offsetB.x, offsetA.z - offsetB.z) + 90",
        "angle %= 360;",
        "const vector3d pointA{offsetA.x + gi.sparkX, offsetA.y + gi.sparkY, offsetA.z + gi.sparkZ};",
        "const vector3d pointB{offsetB.x + gi.sparkX, offsetB.y + gi.sparkY, offsetB.z + gi.sparkZ};",
        "const int alpha = static_cast<int>(gi.alphaB) - segment * 5;",
        "gi.alphaT == 2",
        "static_cast<unsigned char>(segment * 7 + 60)",
        "static_cast<unsigned char>(segment * 7 + 10)",
        "for (int ring = 0; ring <= 3; ++ring)",
        "const int ringAngle = ring * 120;",
        "const float radial = gi.maxHeight * g_cosTable[ringAngle];",
        "const float vertical = gi.maxHeight * g_sinTable[ringAngle];",
        "const float offsetX = radial * g_cosTable[angle];",
        "const float offsetZ = radial * g_sinTable[angle];",
        "const vector3d currentA{pointA.x + offsetX, pointA.y + vertical, pointA.z + offsetZ};",
        "const vector3d currentB{pointB.x + offsetX, pointB.y + vertical, pointB.z + offsetZ};",
        "if (ring > 0)",
        "SubmitWorldTeiRect(previousA, currentA, currentB, previousB, viewMatrix,",
        "texture, color, 0.0f, 1.0f, 1.0f, 0.0f, D3DBLEND_ONE, 0.0f,",
        "previousA = currentA;",
        "previousB = currentB;",
    ), "current RenderHitLine")

    assert "gi.alphaT == 2\n                ? static_cast<unsigned char>(segment * 7 + 60) : 200;" in helper
    assert "F1=1 in RenderTeiRect means spark position" in current and "is already absolute" in current
    assert "prim.m_pos" not in helper, "absolute HitLine vertices were translated a second time"
    print("RenderHitLine comparison passed: Ref/EXE body, PP_HITLINE dispatch, history/ring geometry, tint and rectangle submission")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
