from __future__ import annotations

import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve


def bodies(path: pathlib.Path, signature: str) -> list[str]:
    text = path.read_text(encoding="latin-1")
    result = []
    for match in re.finditer(signature, text, re.M):
        start = match.start()
        brace = text.find("{", match.start())
        depth = 0
        for end in range(brace, len(text)):
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
                if depth == 0:
                    result.append(text[start:end + 1])
                    break
    return result


def body(path: pathlib.Path, signature: str) -> str:
    found = bodies(path, signature)
    assert found, f"function not found: {path}: {signature}"
    return found[0]


compact = lambda value: re.sub(r"\s+", "", value)
signature = r"^void __thiscall CEffectPrim::RenderPeongUp\([^\n]*\)\s*\{"
exe_body = body(ROOT / "HighPriest.exe.c", signature)
ref_bodies = bodies(ROOT / "Ref/RagEffect2.cpp", signature)
assert len(ref_bodies) == 2, f"expected two duplicate Ref bodies, got {len(ref_bodies)}"
assert all(compact(exe_body) == compact(candidate) for candidate in ref_bodies), \
    "canonical EXE and duplicate Ref RenderPeongUp bodies differ"
assert exe_body.count("CEffectPrim::RenderTeiRect(") == 2

image = ro_exe_resolve.ExeImage()
data = next(section for section in image.sections if section.name == ".data")
text = next(section for section in image.sections if section.name == ".text")
text_bytes = image.data[text.raw:text.raw + text.raw_size]
globals_ = list(dict.fromkeys(re.findall(r"\b(flt_[0-9A-Fa-f]+)\b", exe_body)))
assert len(globals_) == 8, f"expected eight orientation globals, got {globals_}"
for name in globals_:
    va = int(name.removeprefix("flt_"), 16)
    assert data.va <= va < data.va + data.virtual_size
    offset = image.offset_of(va)
    if offset is None:
        assert va >= data.va + data.raw_size, f"{name}: zero-fill address is outside the tail"
        raw = bytes(4)
    else:
        raw = image.data[offset:offset + 4]
    assert raw == bytes(4), f"{name}: canonical EXE value is not zero"
    refs = [m.start() for m in re.finditer(re.escape(va.to_bytes(4, "little")), text_bytes)]
    assert refs, f"{name}: no machine-code references"
    memory_loads = []
    for pos in refs:
        if text_bytes[pos - 2:pos] == b"\xD9\x05":
            memory_loads.append(pos)
        elif name == "flt_72CDE8" and text_bytes[pos:pos + 4] == b"\xE8" + va.to_bytes(4, "little")[1:]:
            continue  # call opcode plus relative displacement, not an absolute operand
        else:
            raise AssertionError(f"{name}: unexpected absolute reference could mutate the constant")
    assert memory_loads, f"{name}: no x87 FLD load"

dispatcher = body(ROOT / "HighPriest.exe.c", r"^void __thiscall CEffectPrim::Render\([^\n]*\)\s*\{")
assert "if ( this->m_isVisible )" in dispatcher[:300]
case = dispatcher[dispatcher.index("case 0x60:"):]
assert "CEffectPrim::RenderPeongUp(this, wtm);" in case[:140]
symbol_table = ro_exe_resolve.SymbolTable(image)
texture = symbol_table.resolve_name("aEffectWhitelig")
assert texture is not None and texture.text == "effect\\whitelight.tga"
exe_text = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
assert exe_text.count("CRagEffect::PeongUp(this, aEffectWhitelig,") >= 3

source_path = ROOT / "src/world/RagEffect.cpp"
public = body(source_path, r"^void CEffectPrim::Render\(")
assert "m_type != PP_PEONGUP" in public.split("if ((!m_master", 1)[1].split("|| !viewMatrix", 1)[0]
assert "if (m_type == PP_PEONGUP)" in public
assert "RenderReferencePeongUp(*this, *viewMatrix);" in public
renderer = body(source_path, r"^void RenderReferencePeongUp\(")
for token in (
    "for (const EffectGi& gi : prim.m_gi)", "if (gi.life != 1) continue;",
    "constexpr float orientation[4][2]", "const float radius = gi.height[0]",
    "IntimidateTowardScreen(corners,prim.m_gi[0].alphaT,prim.m_gi[1].alphaT)",
    "gi.vecBPreX", "gi.arcBaselineY", "gi.vecBPreZ", "if (corners[0].y > 0.0f) continue;",
    "if (gi.flag[0]==2) color=105", "else if (gi.flag[0]==3) color=55",
    "gi.alphaB,color,color,blue",
):
    assert compact(token) in compact(renderer), f"implementation branch missing {token!r}"
assert "RenderReferencePeongUp" in (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

print("Canonical EXE and both duplicate Ref RenderPeongUp bodies match; two submission sites are mapped.")
print("All eight orientation globals are executable zero values with load-only .text references.")
print("Dispatcher case0x60, current route/masterless eligibility, life/visibility/color branches are checked.")
print("Executable caller symbol aEffectWhitelig resolves to effect\\whitelight.tga.")
