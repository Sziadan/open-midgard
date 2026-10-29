#!/usr/bin/env python3
"""Source map for the explicit-master PP_3DCIRCLE renderer subset."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE_SOURCE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
PE = PE_SOURCE
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

expected_masters = {8, 9, 17, 31, 26, 35, 36, 42, 48, 61, 62, 73, 170, 321}
actual_masters = {int(value) for value in re.findall(r"GetEffectType\(\)\s*==\s*(\d+)", LOCAL_PREDICATE)}
check("local source-selected subset uses the exact explicit master-effect ID set",
      actual_masters == expected_masters and "m_type == PP_3DCIRCLE" in LOCAL_PREDICATE
      and "m_master" in LOCAL_PREDICATE)

# Every canonical PE circle launch is associated with one of these effect
# handlers. Keep this complete so a new caller cannot silently fall through to
# the generic circle renderer.
pe_circle_callers = (
    (8, "Warp", "void", 1),
    (9, "Enhance", "void", 1),
    (17, "MagnumBreak", "void", 2),
    (26, "IceArrow", "void", 1),
    (31, "FireArrow", "void", 1),
    (35, "ReadyPortal", "void", 1),
    (36, "Portal", "void", 1),
    (42, "Blessing", "void", 1),
    (48, "SprayPond", "void", 1),
    (61, "WarpZone", "void", 2),
    (62, "Sightrasher", "void", 1),
    (73, "BowlingBash", "void", 1),
    (170, "CartRevolution", "unsigned __int8", 1),
    (321, "WarpZone2", "void", 1),
)
caller_circle_count = 0
caller_segment_count = 0
caller_effect_ids = set()
for effect_id, name, return_type, expected_circle_calls in pe_circle_callers:
    signature = f"{return_type} __thiscall CRagEffect::{name}(CRagEffect *this"
    signature_at = PE_SOURCE.rindex(signature)
    caller_body = body(PE_SOURCE, signature, signature_at)
    circle_calls = len(re.findall(r"LaunchEffectPrim\(this,\s*PP_3DCIRCLE\b", caller_body))
    segment_calls = len(re.findall(r"CEffectPrim::InitSegment\(", caller_body))
    check(f"canonical effect {effect_id} {name} creates {expected_circle_calls} circles with initialized segment state",
          circle_calls == expected_circle_calls and segment_calls >= circle_calls)
    caller_circle_count += circle_calls
    caller_segment_count += segment_calls
    caller_effect_ids.add(effect_id)
check("complete canonical PE circle caller set matches the specialized master set",
      caller_effect_ids == expected_masters
      and caller_circle_count == 16 and caller_segment_count >= caller_circle_count)

ref_effects = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
for master, pe_signature, local_signature, segment_token in (
    (8, "void __thiscall CRagEffect::Warp(CRagEffect *this)", "void CRagEffect::SpawnWarp8()", "InitReferenceSegments"),
    (9, "void __thiscall CRagEffect::Enhance(CRagEffect *this)", "void CRagEffect::SpawnEnhance9()", "InitReferenceSegments"),
    (62, "void __thiscall CRagEffect::Sightrasher(CRagEffect *this)", "void CRagEffect::ApplySightrasher()", "m_numSegments = 1"),
):
    pe_caller = body(PE, pe_signature)
    ref_caller = body(ref_effects, pe_signature)
    local_caller = body(CURRENT, local_signature)
    check(f"effect {master} PE/Ref callers create a PP_3DCIRCLE with segment state",
          "PP_3DCIRCLE" in pe_caller and "PP_3DCIRCLE" in ref_caller
          and segment_token in local_caller)
    check(f"effect {master} local caller is included in source renderer master set",
          f"GetEffectType() == {master}" in LOCAL_PREDICATE)
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
