#!/usr/bin/env python3
"""Source map for the legacy segment-sprite renderer and generic particle routing."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
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


SIG = "void __thiscall CEffectPrim::Render3DParticle(CEffectPrim *this, matrix *wtm)"
PE_RENDER = body(PE, SIG, PE.index("void __thiscall CEffectPrim::Render3DParticle(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_RENDER = body(REF, SIG)
PE_DISPATCH = body(PE, "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)", PE.index("void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm); // idb") + 1)
REF_DISPATCH = body(REF_DISPATCH, "void\tCEffectPrim :: Render(matrix &wtm/*view matrix*/)")
LOCAL_DISPATCH = body(CURRENT, "void CEffectPrim::Render(matrix* viewMatrix)")
LOCAL_PREDICATE = body(CURRENT, "bool CEffectPrim::UsesReferenceParticle() const")
LOCAL_RENDER = body(CURRENT, "void RenderParticleSprites(")
LOCAL_SUBMIT = body(CURRENT, "void SubmitParticleSprite(")
LOCAL_GROUPED = body(CURRENT, "bool SubmitAnimatedSpriteParticle(")

check("canonical PE and primary Ref Render3DParticle bodies normalize identically",
      re.sub(r"\s+", " ", PE_RENDER).strip() == re.sub(r"\s+", " ", REF_RENDER).strip())
pe_route = re.search(r"((?:case\s+(?:0x[\da-fA-F]+|\d+)\s*:\s*)+)\s*CEffectPrim::Render3DParticle\(this, wtm\)", PE_DISPATCH)
pe_ids = {int(v, 0) for v in re.findall(r"case\s+(0x[\da-fA-F]+|\d+)\s*:", pe_route.group(1))} if pe_route else set()
check("PE dispatch maps PP_3DPARTICLE, GRAVITY and SPLINE (0x05/06/08) into the particle renderer",
      {5, 6, 8}.issubset(pe_ids))
ref_route = re.search(r"((?:case\s+PP_[A-Z0-9_]+\s*:\s*)+)\s*Render3DParticle\(wtm\)", REF_DISPATCH)
ref_ids = set(re.findall(r"case\s+(PP_[A-Z0-9_]+)\s*:", ref_route.group(1))) if ref_route else set()
check("primary Ref dispatch maps PP_3DPARTICLE, GRAVITY and SPLINE into Render3DParticle",
      {"PP_3DPARTICLE", "PP_3DPARTICLEGRAVITY", "PP_3DPARTICLESPLINE"}.issubset(ref_ids))

# Inventory each canonical launch inside its enclosing decompiled method. The
# PE has no missing-resource rendering fallback: these callers assign both
# resources and initialize segment state before the particle is rendered.
particle_call = re.compile(r"LaunchEffectPrim\([^;\n]*?\b(PP_3DPARTICLE(?:GRAVITY|SPLINE)?)\b")
definition = re.compile(r"(?m)^.*__thiscall\s+((?:CEffectPrim|CRagEffect)::\w+)\([^\n]*\)\s*$")
definitions = []
for match in definition.finditer(PE):
    brace = match.end()
    while brace < len(PE) and PE[brace].isspace():
        brace += 1
    if brace >= len(PE) or PE[brace] != "{":
        continue
    contents = body(PE, match.group(0), match.start())
    definitions.append((match.start(), match.start() + len(contents), match.group(1), contents))

pe_launch_counts = {"PP_3DPARTICLE": 0, "PP_3DPARTICLEGRAVITY": 0, "PP_3DPARTICLESPLINE": 0}
pe_particle_callers = set()
for launch in particle_call.finditer(PE):
    pe_launch_counts[launch.group(1)] += 1
    enclosing = [item for item in definitions if item[0] < launch.start() < item[1]]
    check(f"canonical {launch.group(1)} launch belongs to a source function", bool(enclosing))
    if not enclosing:
        continue
    _, _, name, caller_body = max(enclosing, key=lambda item: item[0])
    pe_particle_callers.add(name)
    check(f"canonical {name} assigns both particle resources and segment state",
          "SetSprName(" in caller_body and "SetActName(" in caller_body and "InitSegment(" in caller_body)
check("canonical PE particle inventory is complete: 37 standard, 5 gravity, 6 spline launches in 39 functions",
      pe_launch_counts == {"PP_3DPARTICLE": 37, "PP_3DPARTICLEGRAVITY": 5, "PP_3DPARTICLESPLINE": 6}
      and len(pe_particle_callers) == 39)
check("PE particle renderer has resource lookup and segment-count gate, with no local approximation fallback",
      "CRenderObject::GetSprRes(this)" in PE_RENDER
      and "CRenderObject::GetActRes(v2)" in PE_RENDER
      and "if ( m_numSegment > 0 )" in PE_RENDER
      and "SubmitAnimatedSpriteParticle" not in PE_RENDER
      and "SubmitBillboard" not in PE_RENDER)

# The PE renderer takes its current action/motion once for every segment and
# draws each clip from that motion. Local UsesReferenceParticle is the existing
# source-selected subset; the general grouped helper is a different pipeline.
expected_master_ids = {0, 2, 3, 7, 10, 14, 18, 20, 21, 22, 24, 31, 33, 39,
                       42, 44, 45, 47, 53, 61, 71, 80, 81, 86, 98, 100, 117,
                       125, 165, 171, 180, 184, 212, 321, 373, 493, 601, 622}
actual_master_ids = {int(v) for v in re.findall(r"m_master->GetEffectType\(\)\s*==\s*(\d+)", LOCAL_PREDICATE)}
check("local particle render gates use the documented master-effect ID union and call segment renderer",
      expected_master_ids == actual_master_ids
      and "RenderReferenceParticlePrimitive(*this, *viewMatrix);" in LOCAL_DISPATCH
      and "case PP_3DPARTICLE:" in LOCAL_DISPATCH and "case PP_3DPARTICLEGRAVITY:" in LOCAL_DISPATCH
      and "case PP_3DPARTICLESPLINE:" in LOCAL_DISPATCH)
canonical_route = re.search(
    r"const bool hasCanonicalParticleResources\s*=\s*(.*?);\s*if\s*\(UsesReferenceParticle\(\)\s*\|\|\s*hasCanonicalParticleResources\)",
    LOCAL_DISPATCH, re.S)
check("all three generic particle types use canonical segments when both locked resources and segment state exist",
      canonical_route is not None
      and all(token in canonical_route.group(1) for token in (
          "m_type == PP_3DPARTICLE", "m_type == PP_3DPARTICLEGRAVITY",
          "m_type == PP_3DPARTICLESPLINE", "m_particleSprRes", "m_particleActRes", "m_numSegments > 0"))
      and "RenderReferenceParticlePrimitive(*this, *viewMatrix);" in LOCAL_DISPATCH)
check("legacy local renderer selects each segment position/size/color and current action/motion clips",
      all(token in LOCAL_RENDER for token in ("prim.m_segments[static_cast<size_t>(j)]", "segment.pos",
          "segment.size", "actions.GetMotion(prim.m_spriteAction, prim.m_curMotion)",
          "motion->sprClips", "sprites.GetSprite(clip.clipType, clip.sprIndex)")))
check("projection and pixel ratio reproduce ProjectVertex/GetPixelRatio inputs",
      "g_renderer.m_hpc" in LOCAL_RENDER and "g_renderer.m_vpc" in LOCAL_RENDER
      and "g_renderer.m_hpc * 0.14285715" in LOCAL_RENDER
      and "GetPixelRatio(g_renderer, oow)" in PE_RENDER)
check("clip bounds and flipped/unflipped screen rejection follow the source branches",
      "g_renderer.m_height > top && bottom > 0.0f" in LOCAL_RENDER
      and "if (clip.flags & 1)" in LOCAL_RENDER
      and "right >= g_renderer.m_width || left <= 0.0" in LOCAL_RENDER
      and "left >= g_renderer.m_width || right <= 0.0f" in LOCAL_RENDER
      and "Clip->flags & 1" in PE_RENDER)
check("pattern 0x200 selects clip ARGB; otherwise segment ARGB and clip angle plus roll are used",
      "(prim.m_pattern & 0x200) != 0" in LOCAL_RENDER
      and "segment.argb" in LOCAL_RENDER and "static_cast<double>(clip.angle) + prim.m_roll" in LOCAL_RENDER
      and "m_lastTlvertY & 0x200" in PE_RENDER
      and "(double)Clip->angle + *(float *)&v36[2].m_BodyAniFrame" in PE_RENDER)
check("sprite submission maps UV, sprite texture/palette, blend flag, depth and missing-upload behavior",
      "ResolveParticleTexture(*sprite.image, palette)" in LOCAL_SUBMIT
      and "renderFlag == 5 ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA" in LOCAL_SUBMIT
      and "vertex.tu = (i & 1) ? u1 : 0.0f" in LOCAL_SUBMIT
      and "if (!texture) return;" in LOCAL_SUBMIT
      and "RenderSprite(v36, &spr, sprRes->m_pal, v21);" in PE_RENDER)
check("focused fixture covers per-segment sprite selection, clipping, color, UV, blend and missing frames",
      all(phrase in FIXTURE for phrase in (
          "every segment submits its motion independently", "history segment retains its own position, size and color",
          "clip whose bottom is above screen is culled", "flipped offscreen horizontal clip",
          "ordinary clips use segment ARGB", "pattern 0x200 selects clip ARGB exactly",
          "padded texture UVs use update dimensions", "flag exactly five uses additive source-alpha blending",
          "missing clip sprite is skipped without billboard substitution")))

# Missing resource/segment state retains the old approximation as a fallback;
# it is not treated as canonical parity.
check("local grouped approximation runs only when canonical retained resources or segment state are missing",
      "GetBakedEffectSpriteFrame(" in LOCAL_GROUPED and "SubmitScreenQuadPivot(anchor" in LOCAL_GROUPED
      and "if (!SubmitAnimatedSpriteParticle(*this, base, viewMatrix, color, destBlend, renderFlags))" in LOCAL_DISPATCH
      and "SubmitBillboard(pos," in LOCAL_DISPATCH
      and "m_particleSprRes && m_particleActRes && m_numSegments > 0" in LOCAL_DISPATCH)
print("PP_3DPARTICLE canonical callers provide resources and segments; missing-resource local degradation remains outside PE parity.")
