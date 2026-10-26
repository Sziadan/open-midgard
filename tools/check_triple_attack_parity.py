#!/usr/bin/env python3
"""Mechanically compare RenderTripleAttack source branches to the production port."""
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


src = body(EXE, "void __thiscall CEffectPrim::RenderTripleAttack(CEffectPrim *this, float wtm)\n{")
disp = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n{")
impl = body(IMPL, "void RenderTripleAttack(CEffectPrim& prim,")
submit = body(IMPL, "void SubmitTripleAttackRect(")
render = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")
tei = body(EXE, "void __thiscall CEffectPrim::RenderTeiRect(\n        CEffectPrim *this,\n        int *rpCnt,\n        __int16 alpha,")


def check(label: str, source: str, source_pattern: str, target: str, target_pattern: str) -> None:
    assert re.search(source_pattern, source, re.S), f"reference mismatch: {label}"
    assert re.search(target_pattern, target, re.S), f"implementation mismatch: {label}"
    print(f"PASS {label}")


check("canonical dispatch and pointer-bit ABI", disp,
      r"case 0x45:\s*CEffectPrim::RenderTripleAttack\(this, \*\(float \*\)&wtm\)", render,
      r"m_type == PP_TRIPLEATTACK\)\s*\{\s*RenderTripleAttack\(\*this, \*viewMatrix\)")
check("float-shaped reference ABI recovers matrix pointer", src,
      r"v2 = \(matrix \*\)LODWORD\(wtm\)", impl, r"rpCount = 0")
check("four bands and life/alpha gate", src,
      r"v22 = 4;[\s\S]*?\*\(\(_BYTE \*\)p_z - 140\) == 1 && \*\(\(__int16 \*\)p_z - 69\) > 0[\s\S]*?p_z \+= 45",
      impl, r"bandIndex < 4[\s\S]*?gi\.life != 1 \|\| gi\.alphaB <= 0")
check("two passes, six table points and five emitted segments per pass", src,
      r"j = 0;[\s\S]*?v23 = 0;[\s\S]*?v23 \+= 288;[\s\S]*?v23 <= 1440[\s\S]*?if \( v5 >= 2 \)",
      impl, r"pass < 2[\s\S]*?sample <= 5[\s\S]*?sample > 0")
check("orientation index uses signed short addition and remainder", src,
      r"4 \* \(\(__int16\)\(\*\(\(_WORD \*\)p_z - 62\) \+ 90\) % 360\)",
      impl, r"static_cast<short>\(static_cast<int>\(gi\.rotStart\) \+ 90\) % 360")
check("second-pass mode radius branches", src,
      r"if \( v5 == 1 \)[\s\S]*?if \( \*\(\(_DWORD \*\)p_z - 26\) == 1077936128 \)\s*v9 = v8 \+ v8;\s*else\s*v9 = v8 \+ 0\.25",
      impl, r"pass == 1\) radius = mode == 3\.0f \? radius \+ radius : radius \+ 0\.25f")
check("six table samples and bottom/top endpoint equations", src,
      r"v10 = wtm \* \*\(float \*\)\(v7 \+ 7525444\);\s*Rx = v10 \* \*\(float \*\)\(v6 \+ 7525444\);\s*Rz = v10 \* \*\(float \*\)\(v6 \+ 7522556\);[\s\S]*?v11 = wtm \* \*\(float \*\)\(v7 \+ 7522556\) - \*\(p_z - 30\)",
      impl, r"sample \* 72[\s\S]*?static_cast<double>\(radius\) \* circleCos \* g_cosTable\[orientation\][\s\S]*?static_cast<double>\(radius\) \* circleCos \* g_sinTable\[orientation\][\s\S]*?static_cast<double>\(radius\) \* circleSin - gi\.height\[0\][\s\S]*?gi\.sparkX[\s\S]*?gi\.vecTNowX")

source_calls = re.findall(r"CEffectPrim::RenderTeiRect\(\s*this,\s*&rpCnt,([\s\S]*?)\);", src)
assert len(source_calls) == 1, f"expected one shared source TeiRect call site, found {len(source_calls)}"
assert all(re.search(r"\n\s*v15,\s*\n\s*0,\s*\n\s*1,\s*\n\s*0xFFu,", call) for call in source_calls), "both source passes must use PW=v15,TN=0,F1=1"
assert re.search(r"v15 = 0;", src) and re.search(r"v15 = 1;", src), "both source passes must select their PW branch"
assert "if ( !(_BYTE)F1 )" in tei and "vec1.x = vec1.x + this->m_pos.x" in tei, "F1=1 must suppress parent-position translation"
assert re.search(r"m_texture\[v29\][\s\S]*?m_texture\[F1 / 4\]", tei), "source triangle texture selectors changed"
assert re.search(r"if \( \*rpCnt < 510 \)", tei), "source face counter cap missing"
assert re.search(r"if \(rpCount >= 510\) return;[\s\S]*?rpCount \+= 2;", submit), "port must preserve rectangle cap and two-face counter"
assert re.search(r"texture = prim\.m_texture\.empty\(\) \? nullptr : prim\.m_texture\[0\]", impl), "port must bind source texture index 0"
assert not re.search(r"prim\.m_pos|translation", submit), "F1=1 suppresses source parent translation"
print("PASS both source submission sites use TN0/F1=1; same texture; no parent translation; 510 cap")

check("pass colors and PW branches", src,
      r"if \( \*\(\(_DWORD \*\)p_z - 26\) == 0x40000000 \)[\s\S]*?v17 = -1;[\s\S]*?v17 = -74;\s*v16 = 89[\s\S]*?v17 = -56",
      impl, r"mode == 2\.0f[\s\S]*?mode == 3\.0f[\s\S]*?pass == 1[\s\S]*?pass == 0")
expected = {
    "(255,255,55)": r"unsigned char red = 255, green = 255, blue = 55",
    "(255,89,182)": r"red = 255; green = 89; blue = 182",
    "(255,255,200)": r"pass == 1\)\s*\{\s*blue = 200",
}
assert re.search(r"v20 = PW == 0;[\s\S]*?if \( v20 \)\s*this->m_rp\[\*rpCnt\]\.destAlphaMode = 2;", tei)
assert re.search(r"else\s*this->m_rp\[\*rpCnt\]\.destAlphaMode = 6;", tei)
assert "D3DBLEND_INVSRCALPHA : D3DBLEND_ONE" in submit
assert all(re.search(pattern, impl) for pattern in expected.values()), "RGB case mapping missing"
print("PASS both RGB mode branches, PW0=>ONE and PW1=>inverse-source-alpha")

assert re.search(r"vec1,\s*v13,\s*v14,\s*vec3,\s*v2,\s*v15,\s*0,\s*1,\s*0xFFu,\s*v16,\s*v17,\s*0\.0,\s*1\.0,\s*0\.0,\s*1\.0", src)
assert re.search(r"\{ bottomPrevious, bottomCurrent, topCurrent \}[\s\S]*?\{ topPrevious, topCurrent, bottomPrevious \}", submit)
assert re.search(r"\{ \{ 0\.0f, 1\.0f \}, \{ 1\.0f, 1\.0f \}, \{ 1\.0f, 0\.0f \} \}[\s\S]*?\{ \{ 0\.0f, 0\.0f \}, \{ 1\.0f, 0\.0f \}, \{ 0\.0f, 1\.0f \} \}", submit)
print("PASS source quad vertex order and fixed UV orientation")
print("11 mechanical RenderTripleAttack branch comparisons passed")
