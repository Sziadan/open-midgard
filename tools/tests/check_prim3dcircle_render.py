#!/usr/bin/env python3
"""Source map for the explicit-master PP_3DCIRCLE renderer subset."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
REF_DISPATCH = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252", errors="replace")
CURRENT = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")


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
    raise AssertionError(f"unclosed body: {marker}")


def check(label: str, condition: bool) -> None:
    assert condition, f"FAIL {label}"
    print(f"PASS {label}")


PE_SIG = "void __thiscall CEffectPrim::Render3DCircle(CEffectPrim *this, matrix *wtm)"
PE = PE[PE.index(PE_SIG + "; // idb") + 1:]
PE_RENDER = body(PE, PE_SIG)
REF_RENDER = body(REF, PE_SIG)
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)",
                   PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_RENDER = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceCircle() const")
LOCAL_CIRCLE = body(CURRENT, "void RenderReferenceCircle(")
LOCAL_GENERIC = body(CURRENT, "void RenderCircleLayer(")

check("canonical PE and primary Ref Render3DCircle bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
check("PE and primary Ref dispatch primitive id 0x0C to Render3DCircle",
      re.search(r"case\s+0xC\s*:\s*CEffectPrim::Render3DCircle\(this, wtm\)", PE_DISPATCH) is not None
      and re.search(r"case\s+PP_3DCIRCLE\s*:\s*Render3DCircle\(wtm\)", REF_DISPATCH) is not None)

expected_masters = {17, 31, 26, 35, 36, 42, 48, 61, 73, 170, 321}
actual_masters = {int(value) for value in re.findall(r"GetEffectType\(\)\s*==\s*(\d+)", LOCAL_PREDICATE)}
check("local source-selected subset uses the exact explicit master-effect ID set",
      actual_masters == expected_masters and "m_type == PP_3DCIRCLE" in LOCAL_PREDICATE
      and "m_master" in LOCAL_PREDICATE)
check("selected route uses RenderReferenceCircle; remaining PP_3DCIRCLE uses generic circle layers",
      "if (UsesReferenceCircle())" in LOCAL_RENDER
      and "RenderReferenceCircle(*this, *viewMatrix);" in LOCAL_RENDER
      and "case PP_3DCIRCLE:" in LOCAL_RENDER
      and "RenderCircleLayer(*this" in LOCAL_RENDER)

# Source starts at float angle zero, truncates every accumulated step to an int,
# tests the unrounded 360-arcAngle stop before the next iteration, and carries
# U across segments while wrapping at >=1.
for label, source, tokens in (
    ("PE", PE_RENDER, ("uInc = 0.0", "v4 = 360.0 - this->m_arcAngle",
       "v12 = (__int64)(v89 + this->m_arcAngle)", "v7 = (__int64)(v89 + this->m_arcAngle)",
       "while ( v65 <= temp )", "if ( sn1 >= 1.0 )", "uInc = 0.0")),
    ("local", LOCAL_CIRCLE, ("const float stop = 360.0f - prim.m_arcAngle", "float u = 0.0f",
       "static_cast<int>(static_cast<double>(angle) + prim.m_arcAngle)",
       "for (int angle = 0; angle <= stop;)", "u += 0.25f", "if (u >= 1.0f) u = 0.0f")),
):
    missing = [token for token in tokens if token not in source]
    check(f"{label} preserves integer angle steps, unrounded stop and U phase wrap", not missing)

check("local maps inner radius and outer radius, matrix transform, then segment position",
      "const float inner = segment.radius - prim.m_size" in LOCAL_CIRCLE
      and "place(segment.radius, angle)" in LOCAL_CIRCLE
      and "g_cosTable[degrees]" in LOCAL_CIRCLE and "g_sinTable[degrees]" in LOCAL_CIRCLE
      and "prim.m_matrix.m[1][0]" in LOCAL_CIRCLE and "prim.m_matrix.m[0][0]" in LOCAL_CIRCLE
      and "prim.m_matrix.m[3][0]" in LOCAL_CIRCLE
      and "return AddVec3(transformed, segment.pos)" in LOCAL_CIRCLE)
check("local copies per-segment ARGB, slot-zero texture and raw render flag to both source faces",
      "prim.m_texture[0]" in LOCAL_CIRCLE and "segment.argb" in LOCAL_CIRCLE
      and "static_cast<int>(prim.m_renderFlag)" in LOCAL_CIRCLE
      and "SubmitReferenceEffectTriangle(first" in LOCAL_CIRCLE
      and "SubmitReferenceEffectTriangle(second" in LOCAL_CIRCLE)

# At positive sub-degree arcs the original int step can stop progressing;
# local returns instead of hanging. Keep that edge explicit and out of parity.
check("positive sub-degree source step can stop progressing; local guard avoids that hang",
      "v12 = (__int64)(v89 + this->m_arcAngle)" in PE_RENDER
      and "if (next <= angle || next > 360) return; // Invalid arc, no replacement geometry." in LOCAL_CIRCLE
      and "invalid circle arc does not invent replacement geometry" in FIXTURE)

fixture_expectations = (
    "circle UV phase wraps after four wedges",
    "circle carries UV phase across segment boundaries instead of restarting it",
    "2008 circle truncates each fractional next angle and compares against unrounded stop",
    "circle reads endpoint table index three-sixty directly without wrapping to zero",
    "invalid circle arc does not invent replacement geometry",
    "circle emits two independent triangles for each arc and every segment",
)
check("existing circle fixture pins geometry, per-segment/color state, UV wrap and fractional angles",
      all(text in FIXTURE for text in fixture_expectations))
print("PP_3DCIRCLE explicit-master subset comparator passed; generic master route remains separate.")
