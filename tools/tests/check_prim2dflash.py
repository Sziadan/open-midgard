#!/usr/bin/env python3
"""Branch-level comparisons for canonical flash primitive updates and rendering."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


def main() -> int:
    ref = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8")
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    dispatcher = (ROOT / "docs/skill/generated/primitive_onprocess_dispatch.tsv").read_text(encoding="utf-8")
    header = (ROOT / "HighPriest.exe.h").read_text(encoding="utf-8")

    signature = "void __thiscall CEffectPrim::Prim2DFlash(CEffectPrim *this)"
    ref_body = function_body(ref, signature)
    exe_body = function_body(exe, signature)
    assert compact(ref_body) == compact(exe_body), "Ref Prim2DFlash body differs from canonical executable decompilation"

    ref_dispatch = function_body(ref, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
    exe_dispatch = function_body(exe, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)")
    for label, body in (("Ref", ref_dispatch), ("EXE", exe_dispatch)):
        case = body[body.index("case 3:") : body.index("case 4:", body.index("case 3:"))]
        assert "Prim2DFlash(this)" in case, f"{label} OnProcess case 3 is not routed to Prim2DFlash"
        assert "goto $L145961" in case, f"{label} case 3 does not continue to the common tick"
    assert "0x03\tPP_2DFLASH\tPrim2DFlash\tcontinues" in dispatcher

    # PP_3DFLASH has an updater and renderer, but the canonical PE effect
    # constructors do not launch it. Local lifecycle tests may still build
    # the primitive directly to verify its source behavior.
    launch_calls = re.findall(r"CRagEffect::LaunchEffectPrim\(this,\s*([^,]+),", exe)
    assert "PP_3DFLASH" not in launch_calls, "canonical PE unexpectedly constructs PP_3DFLASH"
    assert not any(re.fullmatch(r"(?:19|0x13)", argument.strip()) for argument in launch_calls), \
        "canonical PE unexpectedly constructs primitive id 0x13 numerically"
    assert re.search(r"PP_3DFLASH\s*=\s*0x13", header), "canonical primitive enum no longer maps PP_3DFLASH to 0x13"
    launch_factory = function_body(exe, "CEffectPrim *__thiscall CRagEffect::LaunchEffectPrim(")
    assert "CEffectPrim::Init(v5, this, effectPrimId, fDeltaPos)" in launch_factory, \
        "LaunchEffectPrim no longer initializes the primitive from its requested type"

    updater = function_body(current, "void UpdateReferenceFlash(CEffectPrim* prim)")
    anchor3_start = updater.index("if (is3dFlash)")
    anchor3_end = updater.index("} else {", anchor3_start)
    anchor3_branch = updater[anchor3_start:anchor3_end]
    anchor2_branch = updater[anchor3_end:updater.index("if ((prim->m_pattern & 1)")]
    assert anchor2_branch.index("prim->m_pos.x =") < anchor2_branch.index("prim->m_pos.y =")
    assert anchor3_branch.index("prim->m_pos.y =") < anchor3_branch.index("prim->m_pos.x =")
    ordered = (
        "(prim->m_pattern & 1) != 0 && prim->m_heightSize <= 0.0f",
        "prim->m_heightSpeed = 0.0f;",
        "prim->m_heightAccel = 0.0f;",
        "prim->m_stateCnt == prim->m_changePoint",
        "prim->m_heightSpeed = prim->m_changeHeightSpeed;",
        "prim->m_longSpeed = prim->m_changeSpeed;",
        "prim->m_longAccel = prim->m_changeAccel;",
        "if (prim->m_heightSize < 0.0f)",
        "static_cast<double>(prim->m_heightSpeed) * -1.0",
        "prim->m_heightSize = 0.0f;",
        "prim->m_heightSpeed = static_cast<float>(reflectedSpeed);",
        "static_cast<double>(prim->m_longAccel) + prim->m_longSpeed",
        "prim->m_heightSpeed += prim->m_heightAccel;",
        "ProcessReferenceParticleAlpha(prim);",
    )
    cursor = -1
    for token in ordered:
        cursor = updater.find(token, cursor + 1)
        assert cursor >= 0, f"current flash branch missing or out of order: {token}"
    integration3_start = updater.index("if (is3dFlash)", updater.index("prim->m_heightSpeed +="))
    integration3_end = updater.index("} else {", integration3_start)
    integration3_branch = updater[integration3_start:integration3_end]
    integration2_branch = updater[integration3_end:]
    for branch, sequence, label in (
        (integration2_branch, ("prim->m_longitude =", "prim->m_heightSize +=", "prim->m_arcAngle +="), "2D"),
        (integration3_branch, ("prim->m_arcAngle +=", "prim->m_longitude =", "prim->m_heightSize +="), "3D"),
    ):
        branch_cursor = -1
        for token in sequence:
            branch_cursor = branch.find(token, branch_cursor + 1)
            assert branch_cursor >= 0, f"current {label} flash integration order differs from source: {token}"

    flash3_signature = "void __thiscall CEffectPrim::Prim3DFlash(CEffectPrim *this)"
    ref_flash3 = function_body(ref, flash3_signature)
    exe_flash3 = function_body(exe, flash3_signature)
    assert compact(ref_flash3) == compact(exe_flash3), "Ref Prim3DFlash body differs from canonical executable decompilation"
    ref_onprocess_case = ref_dispatch[ref_dispatch.index("case 0x13:"):ref_dispatch.index("case 0x14:", ref_dispatch.index("case 0x13:"))]
    exe_onprocess_case = exe_dispatch[exe_dispatch.index("case 0x13:"):exe_dispatch.index("case 0x14:", exe_dispatch.index("case 0x13:"))]
    assert "Prim3DFlash(this)" in ref_onprocess_case and "goto $L145961" in ref_onprocess_case
    assert "Prim3DFlash(this)" in exe_onprocess_case and "goto $L145961" in exe_onprocess_case

    processing = function_body(current, "bool CEffectPrim::ProcessPrimitiveFrame()")
    route = "if (m_type == PP_2DFLASH || m_type == PP_3DFLASH)"
    assert route in processing, "PP_2DFLASH lacks the canonical unconditional reference updater route"
    route_at = processing.index(route)
    update_at = processing.index("UpdateReferenceFlash(this);", route_at)
    assert "return true;" in processing[update_at : update_at + 100], "void flash handler must continue the shared primitive tick"

    tests = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")
    assert "m_type == PP_2DFLASH || m_type == PP_3DFLASH" in tests
    assert "PP_2DFLASH routes regardless of parent type" in tests

    render_signature = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"
    exe_render = function_body(exe, render_signature)
    flash2_render_signature = "void __thiscall CEffectPrim::Render2DFlash(CEffectPrim *this)"
    ref_render2d = function_body(ref, flash2_render_signature)
    exe_render2d = function_body(exe, flash2_render_signature)
    assert compact(ref_render2d) == compact(exe_render2d), "Ref Render2DFlash body differs from canonical executable"
    visible_switch = exe_render[exe_render.index("if ( this->m_isVisible )") :]
    flash_case_start = visible_switch.index("case 3:")
    flash_case_end = visible_switch.index("case 4:", flash_case_start)
    flash_case = visible_switch[flash_case_start:flash_case_end]
    assert "Render2DFlash(this)" in flash_case

    flash3_signature = "void __thiscall CEffectPrim::Render3DFlash(CEffectPrim *this, matrix *wtm)"
    ref_render3d = function_body(ref, flash3_signature)
    exe_render3d = function_body(exe, flash3_signature)
    assert compact(ref_render3d) == compact(exe_render3d), "Ref Render3DFlash body differs from canonical executable"
    flash3_case_start = visible_switch.index("case 0x13:")
    flash3_case_end = visible_switch.index("case 0x14:", flash3_case_start)
    flash3_case = visible_switch[flash3_case_start:flash3_case_end]
    assert "Render3DFlash(this, wtm)" in flash3_case

    render_signature_current = "void CEffectPrim::Render(matrix* viewMatrix)"
    current_render = function_body(current, render_signature_current)
    flash_route = "if (m_type == PP_2DFLASH || m_type == PP_3DFLASH)"
    assert flash_route in current_render, "current renderer does not route both flash types to the reference helper"
    route_start = current_render.index(flash_route)
    route_end = current_render.index("if (m_type == PP_STIN", route_start)
    assert "RenderReferenceFlash(*this);" in current_render[route_start:route_end]
    assert current_render.index(flash_route) < current_render.index("switch (m_type)")

    render_helper = function_body(current, "void RenderReferenceFlash(const CEffectPrim& prim)")
    geometry = (
        "static_cast<double>(prim.m_longitude)-prim.m_arcAngle",
        "static_cast<double>(prim.m_arcAngle)+prim.m_longitude",
        "prim.m_pos.x",
        "static_cast<double>(cosine1)*prim.m_heightSize+prim.m_pos.x",
        "prim.m_pos.y-static_cast<double>(sine1)*prim.m_heightSize",
        "prim.m_pos.y-sine2*prim.m_heightSize",
    )
    cursor = -1
    for token in geometry:
        cursor = render_helper.find(token, cursor + 1)
        assert cursor >= 0, f"current flash renderer missing or reorders source geometry: {token}"
    for token in (
        "{{0.5f,0},{1,1},{0,1}}",
        "vertex.z = 0.000001f; vertex.oow = 0.99999899f",
        "vertex.color = prim.m_argb",
        "vertex.tu = uv[i][0]; vertex.tv = uv[i][1]",
        "face->tex = texture; face->mtPreset = 0; face->cullMode = D3DCULL_NONE",
        "face->srcAlphaMode = D3DBLEND_SRCALPHA; face->destAlphaMode = D3DBLEND_INVSRCALPHA",
        "AddReferenceEffectTriangle(face,1)",
    ):
        assert token in render_helper, f"current flash renderer missing source field/submission: {token}"

    renderer = (ROOT / "src/render/Renderer.cpp").read_text(encoding="utf-8")
    reference_submit = function_body(renderer, "void CRenderer::AddReferenceEffectTriangle(RPFace* face, int renderFlag)")
    alpha_route = reference_submit[reference_submit.index("if ((renderFlag & 1) != 0)") :]
    assert "m_rpAlphaList.push_back({face->verts[0].oow, face})" in alpha_route
    assert "m_rpAlphaNoDepthList.push_back({average, face})" in alpha_route
    assert "m_rpAlphaDepthList.push_back({average, face})" in alpha_route

    print("Flash primitive comparison passed: EXE/Ref updater and both renderer bodies, EXE render cases, current routes, ordered geometry, alpha blend state, submission flag routing, and common tick")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
