from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]


def body(path: pathlib.Path, signature: str) -> str:
    text = path.read_text(encoding="latin-1")
    match = re.search(signature, text, re.M)
    assert match, f"missing function: {path}: {signature}"
    start = match.start()
    brace = text.find("{", start)
    depth = 0
    for end in range(brace, len(text)):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
            if depth == 0:
                return text[start:end + 1]
    raise AssertionError(f"unbalanced function: {path}: {signature}")


canonical = body(ROOT / "HighPriest.exe.c", r"^void __thiscall CEffectPrim::RenderRectUp2\([^\n]*\)\s*\{")
reference = body(ROOT / "Ref/RagEffect2.cpp", r"^void __thiscall CEffectPrim::RenderRectUp2\([^\n]*\)\s*\{")
compact = lambda value: re.sub(r"\s+", "", value)
assert compact(canonical) == compact(reference), "canonical EXE and Ref RenderRectUp2 bodies differ"
assert canonical.count("CEffectPrim::RenderTeiRect(") == 11, "EXE RenderTeiRect callsite inventory changed"

dispatcher = body(ROOT / "HighPriest.exe.c", r"^void __thiscall CEffectPrim::Render\([^\n]*\)\s*\{")
route = dispatcher[dispatcher.index("case 0x5A:"):]
assert "CEffectPrim::RenderRectUp2(this, wtm);" in route[:140]

current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
public = body(ROOT / "src/world/RagEffect.cpp", r"^void CEffectPrim::Render\(")
assert "if (m_type == PP_RECT_UP2)" in public
assert "RenderReferenceRectUp2(*this, *viewMatrix);" in public
renderer = body(ROOT / "src/world/RagEffect.cpp", r"^void RenderReferenceRectUp2\(")
for expected in (
    "for (int lane = 0; lane < 2; ++lane)",
    "gi.life != 1 || gi.alphaB <= 0",
    "SubmitReferenceRectUp2Rect(rpCount,rect,prim,view,gi.alphaB,200,250,200)",
    "for (int lane = 2; lane < 4; ++lane)",
    "const float d = gi.distance, span = gi.height[3]",
    "if (d < w*2.0f)", "else if (d < w*4.0f)", "else if (d < w*6.0f)",
    "if(left>0)", "bpre.z=depth-d", "bnow.z=bpre.z-span", "tnow.x=bnow.x-left",
    "bnow.x=w-(d-w*2.0f)", "tnow.x=bnow.x-span", "tpre.z=tnow.z+left",
    "tnow.z=-depth+(d-w*4.0f)", "tpre.z=tnow.z+span", "bpre.x=tpre.x+left",
    "tpre.x=-w+(d-w*6.0f)", "bpre.x=tpre.x+span", "bnow.z=bpre.z-left",
    "draw(bpre,bnow,bnowLocal,bpreLocal)", "draw(bnow,tnow,tnowLocal,bnowLocal)",
    "draw(tnow,tpre,tpreLocal,tnowLocal)", "draw(tpre,bpre,bpreLocal,tpreLocal)",
    "draw(bnow,tnow,tnowLocal,bnowLocal)",
    "SubmitReferenceRectUp2Rect(rpCount,rect,prim,view,gi.alphaB,150,250,150)",
):
    assert compact(expected) in compact(renderer), f"current branch map missing {expected!r}"
assert "RenderReferenceRectUp2" in (ROOT / "tools/tests/ParticleRenderTests.cpp").read_text(encoding="utf-8")

print("Canonical EXE and Ref RenderRectUp2 bodies match; all 11 rectangle callsites are accounted for.")
print("Dispatcher case0x5A and current PP_RECT_UP2 route are present.")
print("Current branch map covers both two-lane passes, strict 2w/4w/6w intervals, and overflow submissions.")
