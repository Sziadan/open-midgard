from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import ro_exe_resolve


def all_bodies(path: pathlib.Path, signature: str) -> list[str]:
    text = path.read_text(encoding="latin-1")
    result = []
    for match in re.finditer(signature, text, re.M):
        start = match.start()
        brace = text.find("{", start)
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
    found = all_bodies(path, signature)
    assert found, f"function not found: {path}: {signature}"
    return found[0]


compact = lambda value: re.sub(r"\s+", "", value)
exe_signature = r"^void __thiscall CEffectPrim::RenderPeong\([^\n]*\)\s*\{"
canonical = body(ROOT / "HighPriest.exe.c", exe_signature)
assert canonical.count("CEffectPrim::RenderTeiRect(") == 1
assert "LaunchEffect" not in canonical and "GetGameActorByAID" not in canonical

# Ref's banner says RenderPeong, but the actual C definition is RenderPeongUp.
ref_path = ROOT / "Ref/RagEffect2.cpp"
ref_bodies = all_bodies(ref_path,
    r"^void __thiscall CEffectPrim::RenderPeongUp\([^\n]*\)\s*\{")
assert len(ref_bodies) == 2 and compact(ref_bodies[0]) == compact(ref_bodies[1])
ref_text = ref_path.read_text(encoding="latin-1")
misattributed_banner = re.search(
    r"Original Signature: private: void __thiscall CEffectPrim::RenderPeong\([^\n]*\).*?\n[^\n]*\nvoid __thiscall CEffectPrim::RenderPeongUp",
    ref_text, re.S)
assert misattributed_banner, "expected Ref RenderPeong banner / RenderPeongUp definition mismatch"
assert compact(canonical) != compact(ref_bodies[0]), "misattributed Ref body unexpectedly matches canonical Peong"

dhxj = body(ROOT / "Ref/dhxj/RagEffect2.cpp",
    r"^void\s+CEffectPrim\s*::\s*RenderPeong\([^\n]*\)\s*\{")
assert re.search(r"Angle\s*=\s*45\s*;", dhxj)
assert "Rx=m_GI[ec].distance" in compact(dhxj)

image = ro_exe_resolve.ExeImage()
data = next(section for section in image.sections if section.name == ".data")
text = next(section for section in image.sections if section.name == ".text")
text_bytes = image.data[text.raw:text.raw + text.raw_size]
globals_ = list(dict.fromkeys(re.findall(r"\b(flt_[0-9A-Fa-f]+)\b", canonical)))
assert len(globals_) == 8, f"expected eight orientation globals, got {globals_}"
for name in globals_:
    va = int(name.removeprefix("flt_"), 16)
    assert data.va <= va < data.va + data.virtual_size
    offset = image.offset_of(va)
    if offset is None:
        assert va >= data.va + data.raw_size
        raw = bytes(4)
    else:
        raw = image.data[offset:offset + 4]
    assert raw == bytes(4), f"canonical executable value for {name} is not zero"
    refs = [m.start() for m in re.finditer(re.escape(va.to_bytes(4, "little")), text_bytes)]
    assert refs
    loads = []
    for pos in refs:
        if text_bytes[pos - 2:pos] == b"\xD9\x05":
            loads.append(pos)
        elif name == "flt_72CDE8" and text_bytes[pos:pos + 4] == b"\xE8" + va.to_bytes(4, "little")[1:]:
            continue  # opcode + rel32 coincidence, not an absolute operand
        else:
            raise AssertionError(f"{name} has an unexpected absolute reference")
    assert loads

dispatcher = body(ROOT / "HighPriest.exe.c",
    r"^void __thiscall CEffectPrim::Render\([^\n]*\)\s*\{")
assert "case 0x5E:" in dispatcher
assert "CEffectPrim::RenderPeong(this, wtm);" in dispatcher.split("case 0x5E:", 1)[1][:140]

symbol_table = ro_exe_resolve.SymbolTable(image)
expected_assets = {
    "aEffectPeong1Tg": "effect\\peong1.tga",
    "aEffectPeong2Tg": "effect\\peong2.tga",
    "aEffectPeong3Tg": "effect\\peong3.tga",
}
for symbol, path in expected_assets.items():
    resolved = symbol_table.resolve_name(symbol)
    assert resolved is not None and resolved.text == path, f"{symbol} did not resolve from EXE bytes"
    assert f"CRagEffect::Peong(this, {symbol}," in (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")

source_path = ROOT / "src/world/RagEffect.cpp"
current = source_path.read_text(encoding="utf-8")
public = body(source_path, r"^void CEffectPrim::Render\(")
assert "m_type != PP_PEONG" in public.split("if ((!m_master", 1)[1].split("|| !viewMatrix", 1)[0]
assert "if (m_type == PP_PEONG)" in public
assert "RenderReferencePeong(*this, *viewMatrix);" in public
renderer = body(source_path, r"^void RenderReferencePeong\(")
for token in (
    "for (const EffectGi& gi : prim.m_gi)", "if (gi.life!=1) continue;",
    "orientation[corner][0]*gi.distance", "orientation[corner][1]*gi.distance",
    "IntimidateTowardScreen(corners,prim.m_gi[0].alphaT,prim.m_gi[1].alphaT)",
    "gi.vecBPreX", "gi.arcBaselineY", "gi.vecBPreZ",
    "SubmitReferenceRectUp2Rect(rpCount,corners,prim,view,gi.alphaB,255,255,255,true)",
):
    assert compact(token) in compact(renderer), f"current branch map missing {token!r}"
assert "RenderReferencePeong" in (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

print("Canonical EXE RenderPeong and case0x5E route are mapped; current PP_PEONG route/masterless path is present.")
print("Ref's RenderPeong banner is proven misattributed; dhxj is used only to interpret the canonical body.")
print("Eight orientation globals are executable zeros with load-only references; all three emitter strings resolve from EXE bytes.")
