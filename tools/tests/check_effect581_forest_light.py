#!/usr/bin/env python3
"""Compare effect581 ForestLight variant11 against the PE and primary Ref."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF_CALLER = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
REF_HELPER = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTES = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function: {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


def branch(body: str) -> str:
    start = body.index("case 0x245:")
    return body[start:body.index("case 0x246:", start)]


pe_process = function(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_process = function(REF_CALLER, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
pe_branch, ref_branch = branch(pe_process), branch(ref_process)
if compact(pe_branch) != compact(ref_branch):
    raise AssertionError("PE and primary-Ref effect581 ForestLight caller differ")
if "ForestLight(this, aEffectCloud11T, 11)" not in pe_branch:
    raise AssertionError("effect581 PE variant11 caller changed")
pe_helper = function(PE, "void __thiscall CRagEffect::ForestLight(CRagEffect *this, char *tName, char F1)")
ref_helper = function(REF_HELPER, "void __thiscall CRagEffect::ForestLight(CRagEffect *this, char *tName, char F1)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE and primary-Ref ForestLight helper bodies differ")

if 'case 581: ApplyForestLight("effect\\\\cloud11.tga", 11); return true;' not in ROUTES:
    raise AssertionError("local effect581 variant11 generated route changed")
local_helper = function(LOCAL, "void CRagEffect::ApplyForestLight(const char* textureName, int variant)")
for token in ("m_stateCnt != 0", "kPrimTypeForest", "prim->m_renderFlag = 5",
              "prim->m_duration = m_duration", "prim->m_texture.push_back(g_texMgr.GetTexture(textureName, false))",
              "m_deltaPos.y -= 300.0f", "m_deltaPos.x -= 70.0f", "m_deltaPos.z -= 70.0f",
              "band < 4", "gi.life = 1", "gi.process = band == 3 ? 180 : 0",
              "variant == 11", "gi.alphaB = 0", "gi.maxHeight = static_cast<float>(2 * band) + 4.0f",
              "gi.height[0] = gi.maxHeight", "gi.rotStart = static_cast<short>(25 * band)",
              "gi.flag[0] = static_cast<u8>(variant)"):
    if token not in local_helper:
        raise AssertionError(f"local ForestLight variant11 payload missing {token}")

if "if (m_type == static_cast<EFFECTPRIMID>(kPrimTypeForest))" not in LOCAL or "RenderForest(*this, *viewMatrix);" not in LOCAL:
    raise AssertionError("local PP_FOREST renderer dispatch missing")
renderer = function(LOCAL, "void RenderForest(const CEffectPrim& prim, const matrix& viewMatrix)")
for token in ("gi.life != 1", "gi.alphaB <= 0", "step <= 5", "SubmitWorldTeiRect", "D3DBLEND_ONE"):
    if token not in renderer:
        raise AssertionError(f"local PP_FOREST render path missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
if not any(item.raw == b"effect\\cloud11.tga" for item in image.iter_cstrings()):
    raise AssertionError("PE effect581 ForestLight texture bytes missing")

print("effect581 ForestLight parity passed: PE/Ref caller/helper, local variant11 payload, PP_FOREST renderer and texture")
