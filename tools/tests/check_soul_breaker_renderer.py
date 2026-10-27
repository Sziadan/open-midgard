from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]


def function_body(path: pathlib.Path, signature: str) -> str:
    text = path.read_text(encoding="latin-1")
    match = re.search(signature, text, re.M)
    if not match:
        raise AssertionError(f"function not found: {path}: {signature}")
    start = match.start()
    brace = text.find("{", match.start())
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unbalanced function: {path}: {signature}")


def ordered(text: str, fragments: list[str], label: str) -> None:
    cursor = 0
    for fragment in fragments:
        found = text.find(fragment, cursor)
        if found < 0:
            raise AssertionError(f"{label}: missing or out-of-order anchor {fragment!r}")
        cursor = found + len(fragment)


canonical = function_body(ROOT / "HighPriest.exe.c",
    r"^void __thiscall CEffectPrim::RenderSoulBreaker\([^\n]*\)\s*\{")
ref = function_body(ROOT / "Ref/RagEffect2.cpp",
    r"^void __thiscall CEffectPrim::RenderSoulBreaker\([^\n]*\)\s*\{")
compact = lambda value: re.sub(r"\s+", "", value)
assert compact(canonical) == compact(ref), "canonical EXE and Ref RenderSoulBreaker bodies differ"
assert canonical.count("CEffectPrim::RenderTeiRect(") == 1

dispatcher = function_body(ROOT / "HighPriest.exe.c",
    r"^void __thiscall CEffectPrim::Render\([^\n]*\)\s*\{")
for case in ("0x56", "0x5C"):
    start = dispatcher.index(f"case {case}:")
    assert "RenderSoulBreaker(this, wtm);" in dispatcher[start:start + 150]

source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
public = function_body(ROOT / "src/world/RagEffect.cpp", r"^void CEffectPrim::Render\(matrix\* viewMatrix\)")
guard = public.split("if ((!m_master", 1)[1].split("|| !viewMatrix", 1)[0]
for enum in ("PP_SOULBREAKER", "PP_SOULBREAKER2"):
    assert f"m_type != {enum}" in guard, f"masterless {enum} is rejected before its route"
    assert f"m_type == {enum}" in public, f"{enum} route is missing"
assert "RenderReferenceSoulBreaker(*this, *viewMatrix);" in public

renderer = function_body(ROOT / "src/world/RagEffect.cpp", r"^void RenderReferenceSoulBreaker\(")
ordered(renderer, [
    "for (const EffectGi& gi : prim.m_gi)",
    "if (gi.life != 1 || gi.alphaB <= 0) continue;",
    "int angle = static_cast<int>(gi.rotStart) - 45;",
    "angle += 90;",
    "if (angle >= 360) angle -= 360;",
    "g_cosTable[angle]",
    "g_sinTable[angle]",
    "gi.sparkX",
    "gi.sparkY",
    "gi.sparkZ",
    "SubmitReferenceSoulBreakerRect(rpCount, corners, prim, view, gi.alphaB);",
], "current band geometry")
submitter = function_body(ROOT / "src/world/RagEffect.cpp", r"^void SubmitReferenceSoulBreakerRect\(")
ordered(submitter, [
    "if (rpCount >= 510) return;",
    "const int triangles[2][3] = {{0, 1, 2}, {3, 2, 0}};",
    "(200u << 16) | (200u << 8) | 255u;",
    "CTexture* texture = prim.m_texture[0];",
    "face->destAlphaMode = D3DBLEND_INVSRCALPHA;",
    "ProjectReferenceEffectVertex(corners[corner], view, face->m_verts[vertex]);",
    "g_renderer.AddRP(face, static_cast<int>(prim.m_renderFlag));",
    "++rpCount;",
], "current triangle submitter")
assert "RenderReferenceSoulBreaker" in (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")
print("EXE/Ref RenderSoulBreaker bodies match after whitespace normalization.")
print("Both EXE dispatcher cases0x56/0x5C and current masterless routes are checked.")
print("Branch map passes: GI life/alpha gates, RotStart-45 angle sequence, one-time wrap, four corners, vecB_now translation, alpha/RGB, texture, UV/winding, inverse-alpha blend, AddRP flags.")
