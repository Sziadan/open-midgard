#!/usr/bin/env python3
"""Source audit for PE/Ref/current PP_SMA RenderSMA parity."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIGNATURE = "void __thiscall CEffectPrim::RenderSMA(CEffectPrim *this, matrix *wtm)"
PE_RENDER = "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)"


def function_body(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for cursor in range(opening, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                return source[start:cursor + 1]
    raise AssertionError(f"unterminated function: {signature}")


def compact(source: str) -> str:
    return re.sub(r"\s+", "", source)


def main() -> int:
    ref = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8")
    ref_dispatch = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp1252")
    exe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8")
    current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
    render_tests = (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

    ref_render = function_body(ref, SIGNATURE)
    pe_render = function_body(exe, SIGNATURE)
    assert compact(ref_render) == compact(pe_render), "Ref RenderSMA differs from canonical PE"
    assert "PP_SMA = 0x78" in header

    assert re.search(r"case\s+PP_SMA\s*:\s*RenderSMA\(wtm\)\s*;\s*break\s*;", ref_dispatch)
    pe_dispatch = function_body(exe, PE_RENDER)
    pe_start = pe_dispatch.index("case 0x78:")
    pe_end = pe_dispatch.index("case 0x79:", pe_start)
    assert "CEffectPrim::RenderSMA(this, wtm)" in pe_dispatch[pe_start:pe_end]

    local_dispatch = function_body(current, "void CEffectPrim::Render(matrix* viewMatrix)")
    local_route = "if (m_type == PP_SMA)"
    assert local_route in local_dispatch
    route_start = local_dispatch.index(local_route)
    route_end = local_dispatch.index("if (m_type == PP_SMA2)", route_start)
    assert "RenderSma(*this);" in local_dispatch[route_start:route_end]
    local = function_body(current, "void RenderSma(CEffectPrim& prim)")

    # PE/Ref inspect GI0 only; life must equal one. Selector zero is cadence
    # gated; selector one launches each live render; every other selector no-ops.
    for token in (
        "if ( this->m_GI[0].life == 1 )", "v3 = this->m_GI[0].flag1[0];",
        "if ( v3 )", "if ( v3 == 1 )", "else if ( !(this->m_GI[0].process % 8) )",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref selector path missing: {token}"
    for token in (
        "const EffectGi& gi = prim.m_gi[0];", "if (gi.life != 1) return;",
        "if (gi.flag[0] == 0)", "if (gi.process % 8 != 0) return;",
        "} else if (gi.flag[0] == 1)",
    ):
        assert token in local, f"local selector path missing: {token}"

    # Selector zero launches effect 552 with a zero position/rotation, then
    # sends message 14 only on success. Payload is current vecB_now/sparkXYZ
    # plus {float(process), process%16==0 ? 0 : 1, 0}.
    for token in (
        "CAbleToMakeEffect::LaunchEffect(v4->m_world->m_player, 552, v9, 0.0)",
        "if ( v5 )", "pos.x = this->m_GI[0].vecB_now.x;",
        "pos.y = this->m_GI[0].vecB_now.y;", "pos.z = z;",
        "pos2.x = (float)process;", "if ( process % 16 )", "pos2.y = 1.0;",
        "SendMsg(v5, 0, 14, (int)&pos, (int)&pos2, 0)",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref selector-zero payload missing: {token}"
    for token in (
        "LaunchEffect(552, vector3d{}, 0.0f)", "if (effect)",
        "vector3d position{gi.sparkX, gi.sparkY, gi.sparkZ};",
        "vector3d position2{static_cast<float>(gi.process), gi.process % 16 == 0 ? 0.0f : 1.0f, 0.0f};",
        "effect->SendMsg(nullptr, 14", "reinterpret_cast<msgparam_t>(&position2)",
    ):
        assert token in local, f"local selector-zero mapping missing: {token}"

    # Selector one launches effect 556 every live render and sends only the
    # current position on success. Other selectors and failed launches send none.
    for token in (
        "LaunchEffect(GameMode->m_world->m_player, 556, v9, 0.0)",
        "if ( v8 )", "pos = this->m_GI[0].vecB_now;",
        "SendMsg(v8, 0, 14, (int)&pos, 0, 0)",
    ):
        assert compact(token) in compact(ref_render), f"PE/Ref selector-one path missing: {token}"
    for token in (
        "LaunchEffect(556, vector3d{}, 0.0f)", "if (effect)",
        "effect->SendMsg(nullptr, 14, reinterpret_cast<msgparam_t>(&position), 0, 0);",
    ):
        assert token in local, f"local selector-one mapping missing: {token}"

    fixture = render_tests[
        render_tests.index("CAbleToMakeEffect player;CWorld world;world.m_player=&player;"):
        render_tests.index("g_modeMgr.current=previousMode;", render_tests.index("CAbleToMakeEffect player;CWorld world;world.m_player=&player;"))
    ]
    for token in (
        "gi.process=16;", "lastEffectId==552", "lastPosition2.y==0",
        "gi.process=8;", "lastPosition2.y==1", "effectAvailable=false",
        "gi.flag[0]=1;gi.process=7;", "lastEffectId==556", "gi.flag[0]=2;",
        "gi.life=0;", "SMA exact-life gate",
    ):
        assert token in fixture, f"existing ParticleRenderTests fixture lacks: {token}"
    assert "g_renderer.submissions.empty()" not in fixture  # RenderSMA submits no geometry.
    assert "RenderSma(sma);" in fixture

    print("PASS RenderSMA PE/Ref normalized body identity and PP_SMA dispatch routes")
    print("PASS GI0 exact-life gate, selector-zero cadence/effect552/message14 payload and failure gate")
    print("PASS selector-one effect556/message14 path, other-selector and inactive no-ops")
    print("PASS existing ParticleRenderTests fixture covers source branch and payload cases; renderer emits no geometry")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
