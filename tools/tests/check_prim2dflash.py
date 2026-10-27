#!/usr/bin/env python3
"""Branch-level comparison for canonical CEffectPrim::Prim2DFlash."""
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

    updater = function_body(current, "void UpdateReferenceFlash(CEffectPrim* prim)")
    ordered = (
        "prim->m_pos.x = anchor.x + prim->m_deltaPos2.x;",
        "prim->m_pos.y = anchor.y + prim->m_deltaPos2.y;",
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
        "prim->m_longitude = static_cast<float>(angularSpeed + prim->m_longitude);",
        "prim->m_heightSize += prim->m_heightSpeed;",
        "prim->m_arcAngle += prim->m_widthSpeed;",
        "ProcessReferenceParticleAlpha(prim);",
    )
    cursor = -1
    for token in ordered:
        cursor = updater.find(token, cursor + 1)
        assert cursor >= 0, f"current flash branch missing or out of order: {token}"

    processing = function_body(current, "bool CEffectPrim::ProcessPrimitiveFrame()")
    route = "if (m_type == PP_2DFLASH || m_type == PP_3DFLASH)"
    assert route in processing, "PP_2DFLASH lacks the canonical unconditional reference updater route"
    route_at = processing.index(route)
    update_at = processing.index("UpdateReferenceFlash(this);", route_at)
    assert "return true;" in processing[update_at : update_at + 100], "void flash handler must continue the shared primitive tick"

    tests = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")
    assert "m_type == PP_2DFLASH || m_type == PP_3DFLASH" in tests
    assert "PP_2DFLASH routes regardless of parent type" in tests
    print("Prim2DFlash comparison passed: EXE/Ref body, 0x03 route, ordered helper branches, and common tick")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
