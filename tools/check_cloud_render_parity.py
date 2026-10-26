#!/usr/bin/env python3
"""Mechanical RenderCloud dispatch and branch comparison."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_PRIM = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")


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


def case_ids(block: str) -> list[str]:
    opening = block.index("{")
    depth = 0
    for pos in range(opening, len(block)):
        if block[pos] == "{": depth += 1
        elif block[pos] == "}":
            depth -= 1
            if depth == 0:
                content = block[opening + 1:pos]
                break
    result=[]
    depth=0
    for line in content.splitlines():
        if depth == 0:
            match=re.match(r"\s*case\s+([^:]+):",line)
            if match: result.append(match.group(1).strip())
        depth += line.count("{") - line.count("}")
    return result


ref=body(REF,"void __thiscall CEffectPrim::RenderCloud(CEffectPrim *this, matrix *wtm)")
cloud_style=body(IMPL,"CloudRenderStyle CloudStyle(")
cloud_corners=body(IMPL,"void CloudCorners(")
cloud_render=body(IMPL,"void RenderReferenceCloud(")
route=body(IMPL,"bool IsCloudRendererType(")

ref_pattern=case_ids(ref[ref.index("switch ( v16 )"):])
impl_pattern=case_ids(cloud_style[cloud_style.index("switch (gi.flag[2])"):])
assert ref_pattern == ["0","1","2","3","4","7","8","9","10","11","12","13","14","15","16","17"], ref_pattern
assert impl_pattern == ref_pattern, (ref_pattern,impl_pattern)
print("PASS mechanically extracted outer pattern cases:", ",".join(impl_pattern))

ref_nested=case_ids(ref[ref.index("switch ( v17 )"):])
impl_nested=case_ids(cloud_style[cloud_style.index("switch (gi.flag[3])"):])
assert ref_nested == ["1","2","3","4","5","6"]
assert impl_nested == ref_nested, (ref_nested,impl_nested)
print("PASS mechanically extracted nested pattern-9 cases:", ",".join(impl_nested))

dispatch_start=EXE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)\n")
dispatch_call=EXE.index("CEffectPrim::RenderCloud(this, wtm);",dispatch_start)
source_dispatch=re.findall(r"case (0x[0-9A-F]+):\s*$",EXE[dispatch_start:dispatch_call],re.M)[-13:]
expected=["0x21","0x2A","0x2D","0x2F","0x31","0x35","0x3A","0x3B","0x57","0x61","0x74","0x7B","0x7F"]
assert source_dispatch==expected,source_dispatch
enum_map={name:value for name,value in re.findall(r"\b(PP_[A-Z0-9_]+)\s*=\s*(0x[0-9A-F]+)",HEADER)}
impl_names=re.findall(r"case (PP_[A-Z0-9_]+):",route)
impl_dispatch=[enum_map[name] for name in impl_names]
assert impl_dispatch==source_dispatch,(source_dispatch,impl_dispatch)
print("PASS mechanically extracted 13 source dispatch IDs:",",".join(source_dispatch))
impl_render=body(IMPL,"void CEffectPrim::Render(matrix* viewMatrix)")
assert re.search(r"if \(IsCloudRendererType\(m_type\)\)\s*\{\s*RenderReferenceCloud\(\*this, \*viewMatrix\);\s*return;",impl_render)
print("PASS shared renderer route is applied before other primitive-specific renderers")

checks={
    "four GI bands":(r"flag1 \+= 180;.*?--v33;.*?while \( v33 \);",r"band<4"),
    "life and positive-alpha gate":(r"\*\(flag1 - 104\) == 1 && \*\(\(__int16 \*\)flag1 - 51\) > 0",r"gi\.life != 1 \|\| gi\.alphaB <= 0"),
    "source phase reset and sine pulse":(r">= 360\.0 \|\| .*?< 0\.0.*?g_sinTable\[\(__int64\).*?\* 0\.050000001",r"height\[8\] >= 0\.0f && gi\.height\[8\] < 360\.0f.*?g_sinTable\[phase\].*?0\.050000001"),
    "quarter-turn radius ring":(r"Angle \+ 90.*?v8.*?Angle \+ 90",r"fullDisplayAngle \+ 90.*?angle \+= 90"),
    "height-10 fixed x offset":(r"v15 == 1092616192.*?vec1\.x = vec1\.x \+ 4\.0",r"height\[9\] == 10\.0f.*?corner[s]?\[i\]\.x.*?\+ 4\.0"),
    "TowardScreen uses shared GI rotations":(r"Tei_TowardScreen\(this,.*?m_GI\[0\]\.alphaT, this->m_GI\[1\]\.alphaT",r"IntimidateTowardScreen\(corners,prim\.m_gi\[0\]\.alphaT,prim\.m_gi\[1\]\.alphaT"),
    "source anchor added to all corners":(r"vec1\.x = vec1\.x \+ \*\(\(float \*\)flag1 \+ 7\).*?vec4\.z = vec4\.z \+ \*\(\(float \*\)flag1 \+ 9\)",r"corner\.x=.*?gi\.sparkX.*?corner\.y=.*?gi\.sparkY.*?corner\.z=.*?gi\.sparkZ"),
    "pattern selector is GI0 across bands":(r"v16 = this->m_GI\[0\]\.flag1\[2\];.*?v17 = this->m_GI\[0\]\.flag1\[3\]",r"CloudStyle\(prim\.m_gi\[0\]\)"),
    "blend flag maps to ONE/inverse alpha":(r"\*flag1,\s*1,",r"gi\.flag\[0\]\s*!=\s*0.*?D3DBLEND_INVSRCALPHA.*?D3DBLEND_ONE"),
    "world-space overload keeps source primitive translation disabled":(r"wtm,\s*0,\s*\*flag1,\s*1,",r"SubmitCloudRect\(corners,viewMatrix"),
    "fixed U and source frame-V UVs":(r"0\.047619049",r"0\.047619049"),
}
for name,(rp,ip) in checks.items():
    source_body=REF_PRIM if name=="fixed U and source frame-V UVs" else ref
    assert re.search(rp,source_body,re.S),f"reference missing {name}"
    assert re.search(ip,cloud_corners+cloud_render+body(IMPL,"void SubmitCloudRect("),re.S),f"implementation missing {name}"
    print("PASS",name)

cloud_submit=body(IMPL,"void SubmitCloudRect(")
assert re.search(r"verts\[0\]\.tu = 1\.0;.*?verts\[0\]\.tv = v19",REF_PRIM,re.S)
assert re.search(r"verts\[1\]\.tu = 1\.0;.*?verts\[1\]\.tv = Tx2",REF_PRIM,re.S)
assert re.search(r"verts\[2\]\.tu = 0\.0;.*?verts\[2\]\.tv = Tx2",REF_PRIM,re.S)
assert "{ { 1.0f, frame0 }, { 1.0f, frame1 }, { 0.0f, frame1 } }" in cloud_submit
assert "{ { 0.0f, frame0 }, { 0.0f, frame1 }, { 1.0f, frame0 } }" in cloud_submit
print("PASS exact two-triangle frame-V UV layout")

# Branch colors are compared as source channel triples against the literal ARGB table.
colors={0:"FFFFFF",1:"9696FA",2:"8282FA",3:"C8C864",4:"FFFFFF",7:"FFFFFF",8:"87FF87",
        10:"FAFA96",11:"DCFADC",12:"FCAB8F",13:"FA6464",14:"5E0000",15:"FFC8FF",16:"000000",17:"FFB4B4"}
for value,rgb in colors.items():
    assert f"case {value}: return {{0x00{rgb}u" in cloud_style, f"style color missing pattern {value}"
assert all(f"0x00{rgb}u" in cloud_style for rgb in ("FF8787","FFCD87","FFFF87","FFFFFF","8787FF"))
assert len(ref_pattern)==16 and len(ref_nested)==6
branch_colors=[
    (r"case 2:.*?v26 = -6;\s*v23 = -126;\s*v20 = -126",r"case 2: return \{0x008282FAu"),
    (r"case 3:.*?v26 = 100;\s*v23 = -56;\s*v20 = -56",r"case 3: return \{0x00C8C864u"),
    (r"case 8:.*?v26 = -121;\s*v23 = -1;\s*v20 = -121",r"case 8: return \{0x0087FF87u"),
    (r"case 1:\s*v27 = -121;\s*v24 = -121;\s*v21 = -1",r"case 1: return \{0x00FF8787u"),
    (r"case 2:\s*v26 = -121;\s*v23 = -51;.*?v20 = -1",r"case 2: return \{0x00FFCD87u"),
    (r"case 3:\s*v26 = -121;\s*v23 = -1;\s*v20 = -1",r"case 3: return \{0x00FFFF87u"),
    (r"case 4:.*?v27 = -1;\s*v24 = -1;\s*v21 = -1",r"case 4: return \{0x00FFFFFFu"),
    (r"case 5:\s*v26 = -1;\s*v23 = -121;\s*v20 = -121",r"case 5: return \{0x008787FFu"),
    (r"case 6:\s*CEffectPrim::RenderTeiRect",r"case 6: return \{0x00FFFFFFu"),
]
for source_pattern,impl_pattern in branch_colors:
    assert re.search(source_pattern,ref,re.S),f"source color branch missing {source_pattern}"
    assert re.search(impl_pattern,cloud_style),f"implementation color branch missing {impl_pattern}"
print("PASS all RGB branch literals and 16 outer / 6 nested branches")
print("36 mechanical RenderCloud source/implementation comparisons passed")
