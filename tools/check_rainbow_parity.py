#!/usr/bin/env python3
"""Mechanically compare RenderRainbow control flow, branches, geometry and submission."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")


source = body(EXE, "void __thiscall CEffectPrim::RenderRainbow(CEffectPrim *this, matrix *wtm)\n{")
dispatcher = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n{")
impl = body(IMPL, "void RenderRainbow(CEffectPrim& prim,")
submit = body(IMPL, "void SubmitRainbowRect(")
render_dispatch = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")
tei = body(EXE, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        int TexPart,")


def check(name: str, src: str, src_re: str, dst: str, dst_re: str) -> None:
    assert re.search(src_re, src, re.S), f"reference branch mismatch: {name}"
    assert re.search(dst_re, dst, re.S), f"implementation branch mismatch: {name}"
    print(f"PASS {name}")


check("canonical 0x5D route", dispatcher,
      r"case 0x5D:\s*CEffectPrim::RenderRainbow\(this, wtm\)", render_dispatch,
      r"m_type == PP_RAINBOW\)\s*\{\s*RenderRainbow\(\*this, \*viewMatrix\)")
check("four bands and life/alpha gate", source,
      r"v27 = 4;[\s\S]*?\*\(\(_BYTE \*\)p_alphaB - 2\) != 1 \|\| \*p_alphaB <= 0[\s\S]*?p_alphaB \+= 90",
      impl, r"bandIndex < 4[\s\S]*?gi\.life != 1 \|\| gi\.alphaB <= 0")
check("seven passes and negative-span branch", source,
      r"rcolor = 0;[\s\S]*?p_alphaB\[2\] < 0[\s\S]*?while \( v4 < 7 \)",
      impl, r"pass < 7[\s\S]*?gi\.fullDisplayAngle < 0")
check("angle increment and segment saturation", source,
      r"BasicAngle = p_alphaB\[2\] / 20[\s\S]*?if \( \+\+v5 >= 21 \)\s*v5 = 20[\s\S]*?count \+= BasicAngle[\s\S]*?count <= p_alphaB\[2\]",
      impl, r"static_cast<int>\(gi\.fullDisplayAngle\) / 20[\s\S]*?if \(\+\+segment >= 21\) segment = 20[\s\S]*?count \+= basicAngle[\s\S]*?count <= gi\.fullDisplayAngle")
check("radius and both radial scales", source,
      r"1\.0 - \(double\)rcolor \* 0\.050000001[\s\S]*?1\.0 - \(double\)\(v4 \+ 1\) \* 0\.050000001[\s\S]*?\*\(float \*\)\(p_alphaB \+ 63\)",
      impl, r"outerScale = 1\.0 - static_cast<double>\(pass\) \* 0\.050000001[\s\S]*?innerScale = 1\.0 - static_cast<double>\(pass \+ 1\) \* 0\.050000001[\s\S]*?radius = gi\.distance")
check("single-subtraction table angle wraps", source,
      r"v6 = count \+ p_alphaB\[7\];[\s\S]*?if \( v6 >= 360 \)\s*v6 -= 360[\s\S]*?v9 = BasicAngle \+ count \+ p_alphaB\[7\][\s\S]*?if \( v9 >= 360 \)\s*v9 -= 360",
      impl, r"count \+ gi\.rotStart\) >= 360 \? count \+ gi\.rotStart - 360 : count \+ gi\.rotStart[\s\S]*?angle1Raw >= 360 \? angle1Raw - 360 : angle1Raw")
check("orientation angle is used directly for sine and cosine", source,
      r"g_sinTable\[p_alphaB\[62\]\][\s\S]*?g_cosTable\[p_alphaB\[62\]\]",
      impl, r"g_cosTable\[gi\.riseAngle\][\s\S]*?g_sinTable\[gi\.riseAngle\]")
source_equations = [
    r"vec1\.x = vec1a \* v10", r"vec1\.z = v11 \* sn1", r"vec1\.y = v8 \* v26 \+ v8 \* v26",
    r"vec4\.x = v12 \* v10", r"vec4\.z = dist \* sn1", r"vec4\.y = dista \* v25 \+ dista \* v25",
    r"vector3d\.x = v15 \* g_cosTable\[p_alphaB\[62\]\]", r"vector3d\.z = v15 \* v16",
    r"vector3d\.y = distb \* v25 \+ distb \* v25",
    r"vec2\.x = vec2a \* g_cosTable\[p_alphaB\[62\]\]", r"vec2\.z = v17 \* v16",
    r"vec2\.y = v14 \* v26 \+ v14 \* v26",
]
impl_equations = [
    r"nextOuter = \{ static_cast<float>\(cos1 \* radius \* outerScale \* orientCos\), static_cast<float>\(2\.0 \* sin1 \* radius \* outerScale\), static_cast<float>\(cos1 \* radius \* outerScale \* orientSin\) \}",
    r"nextInner = \{ static_cast<float>\(cos1 \* radius \* innerScale \* orientCos\), static_cast<float>\(2\.0 \* sin1 \* radius \* innerScale\), static_cast<float>\(cos1 \* radius \* innerScale \* orientSin\) \}",
    r"currentInner = \{ static_cast<float>\(cos0 \* radius \* innerScale \* orientCos\), static_cast<float>\(2\.0 \* sin0 \* radius \* innerScale\), static_cast<float>\(cos0 \* radius \* innerScale \* orientSin\) \}",
    r"currentOuter = \{ static_cast<float>\(cos0 \* radius \* outerScale \* orientCos\), static_cast<float>\(2\.0 \* sin0 \* radius \* outerScale\), static_cast<float>\(cos0 \* radius \* outerScale \* orientSin\) \}",
]
assert all(re.search(pattern, source) for pattern in source_equations), "source corner equation missing"
assert all(re.search(pattern, impl) for pattern in impl_equations), "implementation corner equation missing"
print("PASS four source corner equations and corresponding implementation vectors")

src_cases = re.findall(r"case\s+(\d+)\s*:", source[source.index("switch ( rcolor )"):source.index("default:", source.index("switch ( rcolor )"))])
dst_switch = impl[impl.index("switch (pass)"):impl.index("const unsigned int color", impl.index("switch (pass)"))]
dst_cases = re.findall(r"case\s+(\d+)\s*:", dst_switch)
assert src_cases == list(map(str, range(7))), f"reference color cases differ: {src_cases}"
assert dst_cases == src_cases, f"implementation color cases differ: {dst_cases}"
source_switch = source[source.index("switch ( rcolor )"):source.index("default:", source.index("switch ( rcolor )"))]
for case, snippet in {
    0: r"case 0:[\s\S]*?v21 = 0;\s*goto LABEL_25;",
    1: r"case 1:[\s\S]*?v21 = 0;[\s\S]*?v20 = 126;[\s\S]*?goto LABEL_26;",
    2: r"case 2:[\s\S]*?v21 = 0;[\s\S]*?v20 = -1;[\s\S]*?goto LABEL_26;",
    3: r"case 3:[\s\S]*?v21 = 0;[\s\S]*?v20 = -1;[\s\S]*?v19 = 0;",
    4: r"case 4:[\s\S]*?v21 = -1;[\s\S]*?v20 = 0;[\s\S]*?v19 = 0;",
    5: r"case 5:[\s\S]*?v21 = -56;[\s\S]*?v20 = 50;[\s\S]*?v19 = 115;",
    6: r"case 6:[\s\S]*?v21 = -1;",
}.items():
    assert re.search(snippet, source_switch), f"source color case {case} assignments differ"
expected_impl = {
    (255, 0, 0): r"case 0: break;",
    (255, 126, 0): r"case 1: red = 255; green = 126; break;",
    (255, 255, 0): r"case 2: red = 255; green = 255; break;",
    (0, 255, 0): r"case 3: red = 0; green = 255; break;",
    (0, 0, 255): r"case 4: red = 0; blue = 255; break;",
    (115, 50, 200): r"case 5: red = 115; green = 50; blue = 200; break;",
    (255, 0, 255): r"case 6: red = 255; blue = 255; break;",
}
# Resolve the source's shared LABEL_25/LABEL_26 assignments: r=255 in LABEL_26,
# g=0 and b=0 in LABEL_25; -1 bytes wrap to 255.
assert re.search(r"LABEL_25:\s*v20 = 0;[\s\S]*?LABEL_26:\s*v19 = 255;", source)
for rgb, pattern in expected_impl.items():
    assert re.search(pattern, dst_switch), f"implementation RGB branch missing for {rgb}"
assert re.search(r"unsigned char red = 255, green = 0, blue = 0;", impl)
print(f"PASS seven ordered color branches and source-derived RGB mapping: {src_cases}")

check("alpha, PW=0, selected texture and RenderTeiRect call", source,
      r"CEffectPrim::RenderTeiRect\([\s\S]*?\n\s*v5,\n\s*v18,\n\s*v18,[\s\S]*?\n\s*0,\n\s*\*\(float \*\)&v19,\n\s*v20,\n\s*v21\);",
      impl, r"texture = prim\.m_texture\.empty\(\) \? nullptr : prim\.m_texture\[0\][\s\S]*?SubmitRainbowRect\(rpCount,[\s\S]*?texture, color, frame0, frame1, static_cast<int>\(prim\.m_renderFlag\)\)")
assert re.search(r"static_cast<unsigned short>\(gi\.alphaB\)\) << 24", impl), "alphaB must be forwarded unchanged"
assert re.search(r"if \( \*rpCnt < 510 \)", tei), "source must retain its 510-face bound"
assert re.search(r"if \(rpCount >= 510\) return;", submit), "implementation must retain source face bound"
assert re.search(r"rpCount \+= 2;", submit), "implementation must advance the face counter twice"
assert re.search(r"if \( PW \).*?destAlphaMode = 6;.*?else.*?destAlphaMode = 2;", tei, re.S)
assert "face->destAlphaMode = D3DBLEND_ONE;" in submit
assert re.search(r"face->tex = texture;", submit), "implementation must submit the selected texture"
assert not re.search(r"if \(!texture|s_dummy_texture", submit), "source does not filter null/dummy texture values"
assert re.search(r"TexPart \* 0\.047619049[\s\S]*?TexPart \+ 1\).*?0\.047619049", tei, re.S)
assert re.search(r"verts\[0\]\.tu = 1\.0;[\s\S]*?verts\[0\]\.tv = v19[\s\S]*?verts\[1\]\.tv = Tx2[\s\S]*?verts\[2\]\.tu = 0\.0", tei, re.S)
assert re.search(r"frame0 = .*segment.*0\.047619049[\s\S]*?frame1 = .*segment \+ 1.*0\.047619049", impl)
assert re.search(r"\{ \{ 1\.0f, frame0 \}, \{ 1\.0f, frame1 \}, \{ 0\.0f, frame1 \} \}[\s\S]*?\{ \{ 0\.0f, frame0 \}, \{ 0\.0f, frame1 \}, \{ 1\.0f, frame0 \} \}", submit)
print("PASS TexPart-derived UV frames, single texture, unchanged alpha, PW0=>ONE and render flags")
assert re.search(r"CRenderer::ProjectVertex\(g_renderer,.*?&vert\);", tei, re.S)
assert re.search(r"ProjectReferenceEffectVertex\(positions\[triangle\]\[vertex\], viewMatrix, face->m_verts\[vertex\]\);", submit)
print("PASS unconditional source vertex projection and matching reference projection adapter")
print("12 mechanical RenderRainbow branch comparisons passed")
