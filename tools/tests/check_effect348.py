#!/usr/bin/env python3
"""Source parity checks for effect 348's EffectTextureSet + LightSphere fanout."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
REF_PRIM = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess2Cases.generated.inc").read_text(encoding="utf-8")
GENERATOR = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function_body(source: str, signature: str) -> str:
    starts = [match.start() for match in re.finditer(re.escape(signature), source)]
    require(bool(starts), f"missing function signature: {signature}")
    # PE decompilations often have a prototype before the definition. The
    # canonical Ref also repeats this function name for multiple dispatchers.
    definitions = []
    for start in starts:
        opening = source.find("{", start + len(signature))
        if opening < 0:
            continue
        semicolon = source.find(";", start + len(signature), opening)
        if semicolon >= 0:
            continue
        definitions.append((start, opening))
    require(bool(definitions), f"missing function body: {signature}")
    start, opening = definitions[-1]
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise SystemExit(f"unterminated function body: {signature}")


def ordered(body: str, tokens: tuple[str, ...], label: str) -> None:
    position = -1
    for token in tokens:
        current = body.find(token, position + 1)
        require(current >= 0, f"{label} missing or reordered operation: {token}")
        position = current


# Effect 348 is 0x15C. The primary Ref and executable both issue the same
# EffectTextureSet call followed by exactly 80 LightSphere helper calls.
pe_case = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
ref_case = function_body(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
for label, body in (("PE", pe_case), ("Ref", ref_case)):
    case = re.search(r"case 0x15C:(.*?)(?=\n\s*case |\n\s*default:|\Z)", body, re.S)
    require(case is not None, f"{label} OnProcess2 case 0x15C missing")
    case_body = case.group(1)
    require(re.search(r"EffectTextureSet\(this,\s*aEffectThunderC,\s*0\)", case_body),
            f"{label} effect 348 EffectTextureSet arguments changed")
    require(re.search(r"v\d+\s*=\s*80;\s*do\s*\{\s*CRagEffect::LightSphere\(this,\s*aEffectWhite02B\);\s*--v\d+;\s*\}\s*while\s*\(\s*v\d+\s*\);", case_body, re.S),
            f"{label} effect 348 must call LightSphere(aEffectWhite02B) 80 times")
    require("return 1;" in case_body, f"{label} effect 348 must return true")
    ordered(case_body, ("EffectTextureSet(this, aEffectThunderC, 0)",
                        "= 80;", "LightSphere(this, aEffectWhite02B)", "return 1;"), label)

for name, expected in (("aEffectThunderC", b"effect\\thunder_center.bmp"),
                       ("aEffectWhite02B", b"effect\\white02.bmp")):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected,
            f"{name} executable resource bytes changed; expected {expected!r}")

# Compare the canonical PE and Ref spawn helper bodies.
pe_spawn = function_body(PE, "void __thiscall CRagEffect::LightSphere(CRagEffect *this, char *tName)")
ref_spawn = function_body(REF2, "void __thiscall CRagEffect::LightSphere(CRagEffect *this, char *tName)")
pe_spawn = pe_spawn.replace("this->", "")
ref_spawn = ref_spawn.replace("this->", "")
for label, body in (("PE", pe_spawn), ("Ref", ref_spawn)):
    for token in ("m_pos = m_master->m_pos", "m_stateCnt", "PP_LIGHTSPHERE",
                  "m_renderFlag", "m_duration", "m_totalTexture", "m_texture",
                  "life = 1", "process = 0", "alphaB = 40", "distance",
                  "height[3]", "rise_angle", "RotStart", "full_display_angle",
                  "max_height", "height[0]", "height[1]", "height[2]"):
        require(token in body, f"{label} LightSphere helper lacks {token}")
    require(re.search(r"distance\s*=\s*1(?:\.0)?", body)
            and re.search(r"height\[3\]\s*=\s*10(?:\.0)?", body),
            f"{label} LightSphere baseline fields changed")
    for pattern, field in ((r"m_renderFlag\s*=\s*5", "render flag"),
                           (r"m_duration\s*=\s*m_duration", "inherited duration"),
                           (r"m_totalTexture\s*=\s*2", "two-texture metadata"),
                           (r"process\s*=\s*0", "GI process"),
                           (r"alphaB\s*=\s*40", "GI alpha"),
                           (r"height\[3\]\s*=\s*10(?:\.0)?", "GI height[3]")):
        require(re.search(pattern, body), f"{label} LightSphere {field} changed")
    require(re.search(r"life\s*=\s*1", body), f"{label} LightSphere lane life changed")
    require("tName" in body and "GetTexture" in body,
            f"{label} LightSphere texture slot 0 is not assigned from tName")
    require(body.count("m_pos = m_master->m_pos") == 2,
            f"{label} LightSphere must refresh master position before and inside state-zero gate")
    require("flag" not in body, f"{label} LightSphere unexpectedly writes GI flags")
    require("m_totalTexture = 2" in body if label == "PE" else "m_totalTexture = 2" in body,
            f"{label} LightSphere texture count changed")
    require(body.count("rand()") == 5,
            f"{label} LightSphere must consume five RNG draws per GI lane")
    require(body.index("m_pos = m_master->m_pos") < body.index("m_stateCnt")
            < body.index("PP_LIGHTSPHERE"), f"{label} LightSphere position/gate/spawn order changed")

ordered(pe_spawn,
        ("rise_angle = rand() % 360", "RotStart = rand() % 360",
         "full_display_angle = rand() % 360", "rand() % 2", "rand() & 0x80000001",
         "height[0] = 105.0", "height[1] = 105.0", "height[2] = 255.0",
         "height[0] = 55.0", "height[1] = 55.0", "height[2] = 225.0"),
        "PE LightSphere random/palette branch")
require(re.search(r"while\s*\(\s*v3\s*<\s*4\s*\)", pe_spawn),
        "PE LightSphere must initialize all four GI lanes")
require(re.search(r"while\s*\(\s*v3\s*<\s*4\s*\)", ref_spawn),
        "Ref LightSphere must initialize all four GI lanes")

# Current local primitive updater is expected to preserve the already-ported
# motion and position refresh; common CEffectPrim::OnProcess owns duration.
local_update = function_body(LOCAL, "void UpdateReferenceLightSphere(CEffectPrim* prim)")
ordered(local_update,
        ("prim->m_pos = prim->m_master->EffectPosition()",
         "if (static_cast<signed char>(gi.life) <= 0) continue",
         "++gi.process", "gi.distance *= 1.02f", "gi.process > 500",
         "--gi.alphaB", "gi.fullDisplayAngle = static_cast<short>(angle)"),
        "local LightSphere primitive update")
require(re.search(r"if \(m_type == PP_LIGHTSPHERE\)\s*\{\s*UpdateReferenceLightSphere\(this\);.*?return true;\s*\}", LOCAL, re.S),
        "local PP_LIGHTSPHERE dispatch/common lifecycle path changed")
require("++m_stateCnt;\n    return m_stateCnt <= m_duration;" in LOCAL,
        "local CEffectPrim duration lifecycle changed")

# Runtime implementation and generated dispatcher must share the explicit
# effect route and the caller must retain the 80-spawn loop.
local_handler = function_body(LOCAL, "bool CRagEffect::ProcessEffect348()")
ordered(local_handler,
        ('ApplyEffectTexture("effect\\\\thunder_center.bmp", 0)',
         "for (int i = 0; i < 80; ++i)",
         'ApplyLightSphere("effect\\\\white02.bmp")', "return true;"),
        "local effect 348 handler")
require("case 348: return ProcessEffect348();" in DISPATCH,
        "OnProcess2 effect 348 generated route missing")
require(re.search(r"(?:^|\n)\s*348:\s*\"return ProcessEffect348\(\);\"", GENERATOR),
        "effect 348 generator route missing")

# PE effect 381 has the same emitter sequence in the main OnProcess switch.
# The primary Ref source omits direct case 381; its effect348 caller plus the
# shared Ref/PE helpers above corroborate the sequence, while PE is the direct
# caller authority for routing effect381.
pe_main = function_body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
pe_381 = re.search(r"case 381:(.*?)(?=\n\s*case |\n\s*default:|\Z)", pe_main, re.S)
require(pe_381 is not None, "PE OnProcess case 381 missing")
ordered(pe_381.group(1), ("EffectTextureSet(this, aEffectThunderC, 0)", "= 80;",
                           "LightSphere(this, aEffectWhite02B)", "while ( v106 )", "break;"),
        "PE effect 381 caller")
require("case 381:" not in REF and "case 0x17D:" not in REF,
        "primary Ref caller now has a direct effect381 case; review evidence note")
require("case 381: return ProcessEffect348();" in
        (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8"),
        "local OnProcess effect 381 must reuse the effect 348 emitter")
require(re.search(r"(?:^|\n)\s*381:\s*\"return ProcessEffect348\(\);\"", GENERATOR),
        "effect 381 generator route missing")

local_texture = function_body(LOCAL, "void CRagEffect::ApplyEffectTexture(const char* textureName, int variant)")
variant_zero = re.search(r"else if \(variant == 0\)\s*\{(.*?)\n\s*\}", local_texture, re.S)
require(variant_zero is not None, "local EffectTextureSet variant0 branch missing")
ordered(variant_zero.group(1), ("gi.distance = 5.0f", "gi.arcBaselineY = origin.y - 10.0f",
                                "gi.flag[2] = 2", "gi.height[10] = 0.0f",
                                "gi.fullDisplayAngle = 135"),
        "local EffectTextureSet variant0 payload")
require("gi.rotStart = static_cast<short>(rand() % 360)" in local_texture,
        "local EffectTextureSet must retain its common RotStart RNG draw")

texture_helper = function_body(LOCAL, "void CRagEffect::ApplyEffectTexture(const char* textureName, int variant)")
variant0 = re.search(r"else if \(variant == 0\)\s*\{(.*?)\n\s*\} else if \(variant == 100\)",
                     texture_helper, re.S)
require(variant0 is not None, "local EffectTextureSet variant 0 branch missing")
for token in ("gi.distance = 5.0f", "gi.arcBaselineY = origin.y - 10.0f",
              "gi.flag[2] = 2", "gi.height[10] = 0.0f", "gi.fullDisplayAngle = 135"):
    require(token in variant0.group(1), f"local EffectTextureSet variant 0 lacks {token}")
require("rand()" not in variant0.group(1),
        "EffectTextureSet variant 0 must not consume a branch-specific RNG draw")

local_spawn = function_body(LOCAL, "void CRagEffect::ApplyLightSphere(const char* textureName)")
ordered(local_spawn,
        ("m_stateCnt != 0", "LaunchEffectPrim(PP_LIGHTSPHERE", "m_renderFlag = 5",
         "prim->m_duration = m_duration", "m_totalTexture = 2", "m_texture.resize(2, nullptr)",
         "GetTexture(textureName", "for (EffectGi& gi : prim->m_gi)",
         "gi.life = 1", "gi.process = 0", "gi.alphaB = 40", "gi.distance = 1.0f",
         "gi.height[3] = 10.0f", "gi.riseAngle", "gi.rotStart",
         "gi.fullDisplayAngle", "rand() % 2", "rand() & 0x80000001",
         "gi.height[0] = 105.0f", "gi.height[1] = 105.0f", "gi.height[2] = 255.0f",
         "gi.height[0] = 55.0f", "gi.height[1] = 55.0f", "gi.height[2] = 225.0f"),
        "local LightSphere spawn helper")
require(local_spawn.count("rand()") == 5,
        "local LightSphere helper must consume five RNG draws per lane (20 per prim)")
require("prim->m_texture[0] = g_texMgr.GetTexture(textureName, false)" in local_spawn
        and "prim->m_texture[1]" not in local_spawn
        and "prim->m_renderFlag = 5" in local_spawn
        and "prim->m_totalTexture = 2" in local_spawn,
        "local LightSphere two-texture metadata / initialized slot 0 changed")
require("gi.maxHeight = static_cast<float>(rand() % 2 + 1)" in local_spawn
        and "(colorSelector & 1) == 0" in local_spawn,
        "local LightSphere random max height / PE tint selector changed")
require("flag" not in local_spawn,
        "local LightSphere helper added unsupported GI flag writes")
require("for (int i = 0; i < 80; ++i)" in local_handler,
        "effect 348 must issue 80 LightSphere spawn attempts (1600 RNG draws total)")

# The primitive render pass is routed separately from its OnProcess update.
# Canonical PE and Ref use the same four-lane, two-rectangle mesh; the older
# Ref dispatcher spelling is retained in Ref/dhxj/RagEffectPrim.cpp.
pe_render = function_body(PE,
    "void __thiscall CEffectPrim::RenderLightSphere(CEffectPrim *this, matrix *wtm)")
ref_render = function_body(REF2,
    "void __thiscall CEffectPrim::RenderLightSphere(CEffectPrim *this, matrix *wtm)")
for label, body in (("PE", pe_render), ("Ref", ref_render)):
    require(body.count("RenderTeiRect(") == 2,
            f"{label} LightSphere renderer must emit the two cross-plane RenderTeiRect calls")
    require(re.search(r"if\s*\(\s*\*\(\(_BYTE\s*\*\)p_distance\s*-\s*128\)\s*==\s*1\s*&&\s*\*\(\(__int16\s*\*\)p_distance\s*-\s*63\)\s*>\s*0\s*\)", body),
            f"{label} renderer must require live lane and positive alpha")
    for token in ("p_distance = &this->m_GI[0].distance", "v13 = 4",
                  "p_distance += 45", "g_sinTable[*((__int16 *)p_distance - 61)] * *p_distance + *p_distance",
                  "v3 * g_cosTable[*((__int16 *)p_distance - 1)]",
                  "vector3d.x = vec2 * g_cosTable[*((__int16 *)p_distance - 56)]",
                  "vector3d.z = v4 * g_sinTable[*((__int16 *)p_distance - 56)]",
                  "vector3d.y = v3 * g_sinTable[*((__int16 *)p_distance - 1)] - *(p_distance - 24)",
                  "v5.x = 0.5", "v7.x = -0.5", "v8.x = 0.0", "v6.x = 0.0",
                  "HIDWORD(vec1_4a) = 1056964608",
                  "HIDWORD(vec4_4) = -1090519040"):
        require(token in body, f"{label} LightSphere renderer lacks source geometry operation: {token}")
    require(re.search(r"while\s*\(\s*v13\s*\)", body),
            f"{label} renderer must visit all four GI lanes")
    require(body.count("HIDWORD(vec1_4a) = 1056964608") == 1
            and body.count("HIDWORD(vec4_4) = -1090519040") == 1,
            f"{label} renderer cross-plane half-widths changed")
    require("*((__int16 *)p_distance - 63)" in body
            and "(__int64)*(p_distance - 27)" in body
            and "(__int64)*(p_distance - 26)" in body
            and "(__int64)*(p_distance - 25)" in body,
            f"{label} renderer alpha/RGB channel sources changed")

pe_render_dispatch = function_body(PE,
    "void __thiscall CEffectPrim::Render(CEffectPrim *this, matrix *wtm)")
require(re.search(r"case 0x51:\s*CEffectPrim::RenderLightSphere\(this,\s*wtm\);", pe_render_dispatch),
        "PE CEffectPrim::Render 0x51 route changed")
ref_render_dispatch = function_body(REF_PRIM, "CEffectPrim :: Render(matrix &wtm")
require(re.search(r"case\s+PP_LIGHTSPHERE:\s*RenderLightSphere\(wtm\);", ref_render_dispatch),
        "Ref CEffectPrim::Render PP_LIGHTSPHERE route changed")

local_render = function_body(LOCAL, "void CEffectPrim::Render(matrix* viewMatrix)")
local_sphere_route = re.search(
    r"if\s*\(m_type\s*==\s*PP_LIGHTSPHERE\)\s*\{(.*?)\}", local_render, re.S)
require(local_sphere_route is not None
        and "RenderReferenceLightSphere(*this, *viewMatrix);" in local_sphere_route.group(1)
        and "return;" in local_sphere_route.group(1),
        "local CEffectPrim::Render must route PP_LIGHTSPHERE directly to its renderer")
require(local_render.index("PP_LIGHTSPHERE") < local_render.index("default:"),
        "local PP_LIGHTSPHERE render route must precede the generic default")

local_render_body = function_body(LOCAL,
    "void RenderReferenceLightSphere(const CEffectPrim& prim, const matrix& viewMatrix)")
ordered(local_render_body,
        ("prim.m_texture[0]", "gi.life != 1 || gi.alphaB <= 0",
         "GetSin(gi.fullDisplayAngle) * gi.distance + gi.distance",
         "radius * GetCos(gi.riseAngle)",
         "radial * GetCos(gi.rotStart)",
         "radius * GetSin(gi.riseAngle) - gi.height[3]",
         "radial * GetSin(gi.rotStart)",
         "static_cast<unsigned char>(gi.height[0])", "static_cast<unsigned char>(gi.height[1])",
         "static_cast<unsigned char>(gi.height[2])", "SubmitWorldTeiRect(",
         "vector3d{ 0.5f, bottomY, 0.0f }", "AddVec3(prim.m_pos, center)",
         "AddVec3(prim.m_pos, center)", "vector3d{ -0.5f, bottomY, 0.0f }",
         "D3DBLEND_ONE", "SubmitWorldTeiRect(", "vector3d{ 0.0f, bottomY, 0.5f }",
         "AddVec3(prim.m_pos, center)", "AddVec3(prim.m_pos, center)",
         "vector3d{ 0.0f, bottomY, -0.5f }", "D3DBLEND_ONE"),
        "local LightSphere render geometry")
require(local_render_body.count("SubmitWorldTeiRect(") == 2
        and local_render_body.count("AddVec3(prim.m_pos, center)") == 4,
        "local LightSphere must submit two prim-position-translated rectangles around each center")
require(local_render_body.count("viewMatrix, texture, color, 0.0f, 1.0f, 1.0f, 0.0f,") == 2
        and local_render_body.count("D3DBLEND_ONE, 0.0f, static_cast<int>(prim.m_renderFlag)") == 2,
        "local LightSphere UV order, additive blend, alpha sort key or render flag changed")
require("&CTexMgr::s_dummy_texture" in local_render_body,
        "local LightSphere must suppress a missing/dummy texture as the renderer does")

print("effect348 source parity passed: PE/Ref 0x15C caller, exact texture bytes, 80 LightSphere spawns, four lanes and five RNG draws per lane")
