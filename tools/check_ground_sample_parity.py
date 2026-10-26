from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0: return text[opening + 1:pos]
    raise AssertionError(f"unclosed body: {marker}")

def check(name, source_patterns, impl_patterns, source, impl):
    for pattern in source_patterns:
        assert re.search(pattern, source, re.S), f"source mismatch in {name}: {pattern}"
    for pattern in impl_patterns:
        assert re.search(pattern, impl, re.S), f"implementation mismatch in {name}: {pattern}"
    print(f"PASS {name}")

src_route = body(EXE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n{")
dst_route = body(IMPL, "void CEffectPrim::Render(matrix* viewMatrix)")
src = body(REF, "void __thiscall CEffectPrim::RenderGroundSample(CEffectPrim *this, matrix *wtm)\n{")
exe_src = body(EXE, "void __thiscall CEffectPrim::RenderGroundSample(CEffectPrim *this, matrix *wtm)\n{")
dst = body(IMPL, "void RenderGroundSample(CEffectPrim& prim, const matrix& viewMatrix)")

assert re.search(r"case 0x70:\s*CEffectPrim::RenderGroundSample\(this, wtm\)", src_route)
assert re.search(r"m_type == PP_GROUNDSAMPLE\)\s*\{\s*RenderGroundSample\(\*this, \*viewMatrix\)", dst_route)
print("PASS canonical 0x70 route")
assert re.sub(r"\s+", " ", src) == re.sub(r"\s+", " ", exe_src), "Ref and executable RenderGroundSample bodies differ"
print("PASS Ref/executable body equivalence")

check("four bands and life-only gate",
      [r"v37 = 4", r"\*\(\(_BYTE \*\)p_RotStart - 16\) == 1", r"p_RotStart \+= 90"],
      [r"bandIndex < 4", r"gi\.life != 1", r"if \(gi\.life != 1\) continue;"], src, dst)
assert not re.search(r"alphaB", src) and not re.search(r"gi\.alphaB", dst)
print("PASS no alpha gate")

check("duplicate game-mode lookup and server coordinate sequence",
      [r"GameMode = CModeMgr::GetGameMode\(&g_modeMgr\);\s*v5 = CModeMgr::GetGameMode\(&g_modeMgr\);", r"ground = v5->m_world->m_ground;", r"CGameMode::GetServerCoor\(GameMode, x, v2->m_pos\.z, &attrX, &attrY\)"],
      [r"gameMode = g_modeMgr\.GetCurrentGameMode\(\);\s*CGameMode\* worldMode = g_modeMgr\.GetCurrentGameMode\(\);", r"ground = worldMode->m_world->m_ground;", r"gameMode->GetServerCoor\(prim\.m_pos\.x, prim\.m_pos\.z, attrX, attrY\)"], src, dst)

check("source coordinate window and row-major 7x7 loops",
      [r"attrY -= 3;\s*attrX -= 3;", r"i = 0;[\s\S]*?while \( v38 < 7 \)", r"j = 0;[\s\S]*?while \( j < 7 \)", r"v13 = v10 \+ attrX", r"v16 = i \+ attrY"],
      [r"attrY -= 3;\s*attrX -= 3;", r"for \(int i = 0; i < 7; \+\+i\)[\s\S]*?for \(int j = 0; j < 7; \+\+j\)", r"attrX \+ j, attrY \+ i"], src, dst)

check("GI rotation sine/cosine table lookup",
      [r"v8 = g_sinTable\[v7\]", r"v9 = g_cosTable\[v7\]"],
      [r"sinRotation = g_sinTable\[gi\.rotStart\]", r"cosRotation = g_cosTable\[gi\.rotStart\]"], src, dst)

src_uv = [r"v36 = v46 \+ v44", r"v35 = v45 - v19", r"v34 = v20 \+ v46", r"v33 = v45 - v1", r"v44 = v43 \+ v44", r"v32 = v41 - v19", r"v42 = v43 \+ v42", r"v1 = v41 - v1", r"uv\[0\]\.x = v36", r"uv\[0\]\.y = v35", r"uv\[1\]\.x = v34", r"uv\[1\]\.y = v33", r"uv\[2\]\.x = v44", r"uv\[2\]\.y = v32", r"uv\[3\]\.x = v42", r"uv\[3\]\.y = v1"]
dst_uv = [r"row0 \* sinRotation \+ col0 \* cosRotation", r"row0 \* cosRotation - col0 \* sinRotation", r"row0 \* sinRotation \+ col1 \* cosRotation", r"row0 \* cosRotation - col1 \* sinRotation", r"row1 \* sinRotation \+ col0 \* cosRotation", r"row1 \* cosRotation - col0 \* sinRotation", r"row1 \* sinRotation \+ col1 \* cosRotation", r"row1 \* cosRotation - col1 \* sinRotation"]
check("all four rotated source UV corners and their order", src_uv, dst_uv, src, dst)

check("inner and expanded outer UV scales plus normalization",
      [r"v21 = v17->y \* \*\(\(float \*\)p_RotStart - 2\)", r"\(uv\[0\]\.x \+ 3\.5\) \* 0\.14285715", r"v24 = \*\(\(float \*\)p_RotStart - 2\) \+ 0\.1", r"\(uv\[3\]\.y \+ 3\.5\) \* 0\.14285715"],
      [r"point\.x \* gi\.maxHeight \+ 3\.5", r"point\.y \* gi\.maxHeight \+ 3\.5", r"outerScale = static_cast<double>\(gi\.maxHeight\) \+ 0\.1", r"outerScale \* rawUv\[corner\]\.x \+ 3\.5", r"outerScale \* rawUv\[corner\]\.y \+ 3\.5", r"0\.14285715"], src, dst)

check("two ordered texture fetches and two ground tile submissions",
      [r"Texture = CTexMgr::GetTexture\(&g_texMgr, aEffectMagicTar, 0\);\s*C3dGround::RenderGroundTile\(ground, wtm, v13, v16, 0xAA9999FF, Texture, uv\);", r"v29 = CTexMgr::GetTexture\(&g_texMgr, aEffectMagicTar, 0\);\s*C3dGround::RenderGroundTile\(ground, wtm, v13, v16, 0x559999FFu, v29, uv\);"],
      [r"texture = g_texMgr\.GetTexture\(\"effect\\\\magic_target\.tga\", false\);\s*ground->RenderGroundTile\(viewMatrix, attrX \+ j, attrY \+ i, 0xAA9999FFu, texture, uv\);", r"texture = g_texMgr\.GetTexture\(\"effect\\\\magic_target\.tga\", false\);\s*ground->RenderGroundTile\(viewMatrix, attrX \+ j, attrY \+ i, 0x559999FFu, texture, uv\);"], src, dst)

assert len(re.findall(r"C3dGround::RenderGroundTile\(", src)) == 2
assert len(re.findall(r"ground->RenderGroundTile\(", dst)) == 2
assert len(re.findall(r"g_texMgr\.GetTexture\(", dst)) == 2
print("PASS two draw sites and two texture lookups per cell; 7x7 * 2 = 98 tile calls per live band")
print("11 mechanical RenderGroundSample branch comparisons passed")








