#!/usr/bin/env python3
"""Canonical source, dispatch and effect-route checks for PP_3DTEXTURE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
TESTS = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")


def body(text: str, marker: str, start: int = 0) -> str:
    index = text.index(marker, start)
    opening = text.index("{", index)
    depth = 0
    for cursor in range(opening, len(text)):
        if text[cursor] == "{":
            depth += 1
        elif text[cursor] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:cursor]
    raise AssertionError(f"unclosed function body: {marker}")


def compact(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


SIG = "void __thiscall CEffectPrim::Render3DTexture(CEffectPrim *this, matrix *wtm)"
PE_RENDER = body(PE, SIG, PE.index("//----- (00646770)"))
REF_RENDER = body(REF, SIG)
LOCAL_RENDER = body(CURRENT, "void RenderReferenceTexture(const CEffectPrim& prim, const matrix& view)")
SUBMIT = body(CURRENT, "void SubmitReferenceEffectTriangle(")
PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceTexture() const")
LOCAL_DISPATCH = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)", PE.index("//----- (00649220)"))
REF_DISPATCH_BODY = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")

check("canonical PE and primary Ref Render3DTexture bodies normalize identically",
      compact(PE_RENDER) == compact(REF_RENDER))
check("PE and Ref primitive dispatch route PP_3DTEXTURE directly to Render3DTexture",
      re.search(r"case\s+0xF\s*:\s*CEffectPrim::Render3DTexture\(this, wtm\)", PE_DISPATCH) is not None
      and re.search(r"case\s+PP_3DTEXTURE\s*:\s*Render3DTexture\(wtm\)", REF_DISPATCH_BODY) is not None)
check("local selected routes enter the legacy texture renderer ahead of generic billboard fallback",
      "if (UsesReferenceTexture())" in LOCAL_DISPATCH
      and "RenderReferenceTexture(*this,*viewMatrix);" in LOCAL_DISPATCH
      and "case PP_3DTEXTURE:" in LOCAL_DISPATCH
      and "SubmitBillboard(pos," in LOCAL_DISPATCH)

# Every PE effect launcher which dispatches into OnProcess is mapped to the
# local reference renderer gate. Orphan helpers have no PE callsite and are
# intentionally not treated as live launch routes.
direct_callers = {
    13: "GlassWall", 48: "SprayPond", 60: "LockOn", 82: "TurnUndead",
    93: "Yufitel", 94: "YufitelHit", 99: "Flasher", 114: "Bottom",
    116: "WaterBall", 118: "FireIvy", 119: "Detecting", 137: "Bottom2",
    159: "TopRank", 160: "Party", 196: "inline", 452: "YufitelHit",
}
pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)",
                  PE.index("unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this); // idb") + 1)
for effect_id, method in direct_callers.items():
    if method == "inline":
        routed = re.search(r"case\s+196\s*:[\s\S]{0,600}LaunchEffectPrim\(this, PP_3DTEXTURE,", pe_process) is not None
    else:
        routed = re.search(rf"case\s+{effect_id}\s*:\s*(?:result\s*=\s*)?CRagEffect::{method}\(this(?:,\s*\d+)*\)", pe_process) is not None
    check(f"PE OnProcess effect {effect_id} launches through {method}", routed)

# Pin every canonical PE launch site, including shared helpers and defined
# helpers which have no OnProcess caller. This prevents the route predicate
# from silently omitting a newly discovered source caller.
pe_launch_counts = {
    "OnProcess": 1, "GlassWall": 2, "SprayPond": 2, "LockOn": 2,
    "TurnUndead": 1, "Yufitel": 2, "YufitelHit": 2, "Flasher": 1,
    "Bottom": 2, "Bottom2": 2, "WaterBall": 1, "FireIvy": 1,
    "Detecting": 1, "TopRank": 1, "Party": 1, "TetraVoltex": 1,
    "CurseAttack": 1,
}
check("canonical PE PP_3DTEXTURE launch total is 24",
      len(re.findall(r"LaunchEffectPrim\(this,\s*PP_3DTEXTURE,", PE)) == 24)
for method, expected_count in pe_launch_counts.items():
    definition = re.search(
        rf"(?m)^(?:[\w]+\s+)+__thiscall CRagEffect::{method}\(CRagEffect \*this[^\n]*\)\n\{{", PE)
    check(f"PE {method} definition exists for the caller inventory", definition is not None)
    method_body = body(PE, definition.group(0).splitlines()[0], definition.start())
    actual_count = len(re.findall(r"LaunchEffectPrim\(this,\s*PP_3DTEXTURE,", method_body))
    check(f"PE {method} has exactly {expected_count} PP_3DTEXTURE launch site(s)",
          actual_count == expected_count)

local_effects = {int(value) for value in re.findall(r"m_master->GetEffectType\(\)\s*==\s*(\d+)", PREDICATE)}
check("local texture route covers exactly all 16 dispatched PE launch effects",
      local_effects == set(direct_callers))
for orphan in ("TetraVoltex", "CurseAttack"):
    calls = re.findall(rf"CRagEffect::{orphan}\(this\)", PE)
    check(f"PE {orphan} has no invocation callsite and remains outside the route gate", not calls)
check("live texture route master set equals the 16 canonical dispatched effects",
      local_effects == set(direct_callers))

check("source ordinary and pattern 0x80/0x8 branches map to local lower/upper corner coordinates",
      "if ( (m_pattern & 0x80u) == 0 )" in PE_RENDER
      and "if ( (m_pattern & 8) != 0 )" in PE_RENDER
      and "const float lowerY = (prim.m_pattern & 0x80) != 0 ? -2.0f * h : -h;" in LOCAL_RENDER
      and "const float upperY = (prim.m_pattern & 0x8) != 0 ? 0.0f" in LOCAL_RENDER)
check("all four corners use source width/height axes, render matrix and world translation",
      "const vector3d local[4] = {{0,lowerY,w},{0,lowerY,-w},{0,upperY,w},{0,upperY,-w}};" in LOCAL_RENDER
      and "static_cast<double>(local[i].z)*prim.m_rotMat.m[2][axis]" in LOCAL_RENDER
      and "static_cast<double>(local[i].y)*prim.m_rotMat.m[1][axis]" in LOCAL_RENDER
      and "prim.m_rotMat.m[3][axis]" in LOCAL_RENDER
      and "AddVec3(vector3d{rotated[0],rotated[1],rotated[2]},prim.m_pos)" in LOCAL_RENDER)
check("two source triangle orders and full UV coordinates preserve m_v1/m_v2",
      "const vector3d first[3] = {points[0],points[1],points[2]}, second[3] = {points[3],points[2],points[1]};" in LOCAL_RENDER
      and "{{0,prim.m_v1},{1,prim.m_v1},{0,prim.m_v2}}" in LOCAL_RENDER
      and "{{1,prim.m_v2},{0,prim.m_v2},{1,prim.m_v1}}" in LOCAL_RENDER)
check("both triangles use current-motion texture, stored ARGB and raw source renderFlag",
      "prim.m_texture[static_cast<size_t>(prim.m_curMotion)]" in LOCAL_RENDER
      and "prim.m_argb" in LOCAL_RENDER
      and "static_cast<int>(prim.m_renderFlag)" in LOCAL_RENDER
      and "face->srcAlphaMode = (renderFlag & 2) != 0 ? D3DBLEND_ONE : D3DBLEND_SRCALPHA;" in SUBMIT
      and "face->destAlphaMode = (renderFlag & 6) != 0 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA;" in SUBMIT
      and "g_renderer.AddReferenceEffectTriangle(face, renderFlag);" in SUBMIT)

for phrase in (
    "3D texture emits two triangles for one projected quad",
    "3D texture ordinary pattern preserves source quad corners and both triangle orders",
    "3D texture preserves full source UV orientation and v1/v2 values",
    "3D texture selects current texture and stored color and forwards render flags",
    "3D texture pattern 0x80 extends lower edge to minus twice height and pins upper edge",
    "3D texture pattern 0x8 collapses upper edge to the origin plane",
    "3D texture combined pattern bits preserve lower extension and upper collapse",
    "3D texture applies retained render matrix before world translation and view projection",
    "3D texture forwards source render flags to triangle blend and queue selection",
    "3D texture missing texture emits no substitute geometry",
):
    check(f"geometry/flags fixture covers {phrase}", phrase in TESTS)

print("PP_3DTEXTURE renderer comparator passed.")
