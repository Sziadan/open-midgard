from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve as exe_resolver


def body(path: pathlib.Path, signature: str) -> str:
    text = path.read_text(encoding="latin-1")
    match = re.search(signature, text, re.M)
    if not match:
        raise AssertionError(f"function not found: {path}: {signature}")
    start = match.start()
    brace = text.find("{", match.start())
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"unbalanced function: {path}: {signature}")


def ordered(text: str, fragments: list[str], label: str) -> None:
    cursor = 0
    for fragment in fragments:
        found = text.find(fragment, cursor)
        if found < 0:
            raise AssertionError(f"{label}: missing or out-of-order anchor {fragment!r}")
        cursor = found + len(fragment)


canonical = body(ROOT / "HighPriest.exe.c", r"^void __thiscall CEffectPrim::RenderMapParticle\([^\n]*\)\s*\{")
ref = body(ROOT / "Ref/RagEffect2.cpp", r"^void __thiscall CEffectPrim::RenderMapParticle\([^\n]*\)\s*\{")
normalize = lambda value: re.sub(r"\s+", "", value)
assert normalize(canonical) == normalize(ref), "canonical EXE decomp and Ref RenderMapParticle differ"
assert canonical.count("CEffectPrim::RenderTeiRect(") == 4, "canonical must contain four ordered band submissions"

exe = exe_resolver.ExeImage()
data = next(section for section in exe.sections if section.name == ".data")
text_section = next(section for section in exe.sections if section.name == ".text")
text_bytes = exe.data[text_section.raw:text_section.raw + text_section.raw_size]
globals_ = re.findall(r"\b(flt_[0-9A-Fa-f]+)\b", canonical)
globals_ = list(dict.fromkeys(globals_))
assert len(globals_) == 8, f"expected eight orientation globals, found {globals_}"
for name in globals_:
    va = int(name.removeprefix("flt_"), 16)
    assert data.va <= va < data.va + data.virtual_size, f"{name} is outside .data"
    offset = exe.offset_of(va)
    if offset is None:
        assert va >= data.va + data.raw_size, f"{name} unexpectedly lacks a file offset"
    else:
        assert exe.data[offset:offset + 4] == b"\0\0\0\0", f"{name} has nonzero executable bytes"
    address_bytes = va.to_bytes(4, "little")
    refs = [match.start() for match in re.finditer(re.escape(address_bytes), text_bytes)]
    assert refs, f"{name} has no code references"
    memory_refs = []
    for pos in refs:
        if text_bytes[pos - 2:pos] == b"\xD9\x05":
            memory_refs.append(pos)
            continue
        # 0x72CDE8 also occurs as E8 CD 72 00, a call-opcode plus rel32
        # displacement, rather than as an absolute address operand.
        if text_bytes[pos:pos + 4] == b"\xE8" + address_bytes[1:]:
            continue
        raise AssertionError(f"{name} has an unexpected absolute reference that may initialize or mutate it")
    assert memory_refs, f"{name} has no FLD load"
readable = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
dhxj = body(ROOT / "Ref/dhxj/RagEffect2.cpp", r"^void\s+CEffectPrim\s*::\s*RenderMapParticle\([^\n]*\)\s*\{")
assert re.search(r"\bAngle\s*=\s*45\s*;", dhxj), "dhxj Angle=45 fallback is missing"
assert "RenderMapParticle" in readable

source = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
renderer = body(ROOT / "src/world/RagEffect.cpp", r"^void RenderReferenceMapParticle\(")
submitter = body(ROOT / "src/world/RagEffect.cpp", r"^void SubmitReferenceMapParticleRect\(")
assert "m_type == PP_MAPPARTICLE" in source and "RenderReferenceMapParticle(*this, *viewMatrix);" in source
assert "m_type != PP_MAPPARTICLE" in source[source.find("void CEffectPrim::Render(matrix* viewMatrix)"):]
ordered(renderer, [
    "for (const EffectGi& gi : prim.m_gi)",
    "if (gi.life != 1) continue;",
    "constexpr float orientation[4][2]",
    "orientation[corner][0]",
    "orientation[corner][1]",
    "IntimidateTowardScreen(corners, prim.m_gi[0].alphaT, prim.m_gi[1].alphaT);",
    "{gi.vecBPreX, gi.arcBaselineY, gi.vecBPreZ}",
    "{gi.sparkX, gi.sparkY, gi.sparkZ}",
    "{gi.vecTPreX, gi.vecTPreY, gi.vecTPreZ}",
    "{gi.vecTNowX, gi.vecTNowY, gi.vecTNowZ}",
    "int alpha = modeTwo ? 150 : 130;",
    "translated[0].y < -1.0f",
    "if (alpha < 0) alpha = 0;",
    "if (translated[0].y <= 0.0f)",
    "SubmitReferenceMapParticleRect(rpCount, translated, prim, view, alpha, modeTwo);",
], "current renderer branch map")
ordered(submitter, [
    "if (rpCount >= 510) return;",
    "const int triangles[2][3] = {{0, 1, 2}, {3, 2, 0}};",
    "CTexture* texture = prim.m_texture[0];",
    "face->destAlphaMode = D3DBLEND_ONE;",
    "ProjectReferenceEffectVertex(position, view, face->m_verts[vertex]);",
    "g_renderer.AddRP(face, static_cast<int>(prim.m_renderFlag));",
    "++rpCount;",
], "current triangle submitter")
assert "RenderReferenceMapParticle" in (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")
print("EXE/Ref RenderMapParticle bodies match after whitespace normalization.")
print("Branch map passes: four GI life gates, executable-zero orientation, four ordered bands, alpha/fade/clamp/cutoff, mode colors, additive two-triangle submissions.")
print("All eight orientation globals are zero in the image/zero-fill; every absolute .text operand is FLD (with one E8 rel32 byte coincidence excluded). dhxj's Angle=45 conflicts; implementation follows the EXE.")
