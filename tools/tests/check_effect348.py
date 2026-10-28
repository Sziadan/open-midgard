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

print("effect348 source parity passed: PE/Ref 0x15C caller, exact texture bytes, 80 LightSphere spawns, four lanes and five RNG draws per lane")
