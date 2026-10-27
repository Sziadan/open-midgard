from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
HEADER = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="latin-1")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DURATION = (ROOT / "src/world/RagEffectDurationTable.generated.inc").read_text(encoding="utf-8")


def body(text, start, end):
    stop = text.index(end)
    return text[text.rindex(start, 0, stop):stop]


def ordered(label, text, tokens):
    offset = 0
    for token in tokens:
        found = text.find(token, offset)
        if found < 0:
            raise SystemExit(f"FAIL: {label} missing/out of order: {token}")
        offset = found + len(token)


# Evaluate the Ref enum's explicit resets plus implicit values.
enum_start = HEADER.index("enum {")
enum_end = HEADER.index("};", enum_start)
enum = re.sub(r"/\*.*?\*/|//[^\n]*", "", HEADER[enum_start:enum_end], flags=re.S)
values = {}
next_value = 0
for item in enum[enum.index("{") + 1:].split(","):
    match = re.search(r"\b(EF_[A-Z0-9_]+)\b(?:\s*=\s*(\d+))?", item)
    if not match:
        continue
    name, explicit = match.groups()
    if explicit is not None:
        next_value = int(explicit)
    values[name] = next_value
    next_value += 1
if values.get("EF_TOPRANK") != 159:
    raise SystemExit(f"FAIL: Ref enum does not derive EF_TOPRANK=159 ({values.get('EF_TOPRANK')})")
if not re.search(r"9999\s*,\s*//\s*159\b", DURATION):
    raise SystemExit("FAIL: current generated duration table does not retain EF_TOPRANK lifetime9999")
if "EF_TOPRANK:" not in REF or "TopRank();" not in REF:
    raise SystemExit("FAIL: Ref caller lacks EF_TOPRANK route")
if "case 159:\n            CRagEffect::TopRank(this);" not in EXE:
    raise SystemExit("FAIL: canonical dispatcher lacks case159 TopRank route")
if "case 159:\n        // ProcessEffectFrame owns the source TopRank emitter for EF_TOPRANK.\n        m_handler = Handler::None;" not in IMPL:
    raise SystemExit("FAIL: current Init still selects an unrelated handler for ID159")
if "if (m_type == 159) {\n        SpawnTopRank();\n        return true;\n    }" not in IMPL:
    raise SystemExit("FAIL: current frame dispatcher lacks ID159 TopRank route")

ref_fn = body(REF, "void CRagEffect :: TopRank()", "void CRagEffect :: Party()")
exe_fn = body(EXE, "void __thiscall CRagEffect::TopRank(CRagEffect *this)", "//----- (00602C90)")
cur_fn = body(IMPL, "void CRagEffect::SpawnTopRank()", "// Ref/RagEffect.cpp:8826-8852")

ordered("Ref", ref_fn, [
    "m_pos = m_master->m_pos", "if (m_stateCnt == 9998)", "m_stateCnt = 0",
    "if (m_stateCnt == 0)", "LaunchEffectPrim(PP_3DTEXTURE)", "m_duration = 9999",
    "m_longitude = 0", "m_longSpeed = 4.5f", "m_roll = 90",
    "m_widthSize = 4.0f", "m_heightSize = 4.0f", "if (m_count != 11)",
    "m_red = 250", "m_blue = 0 + (10 - m_count) * 25", "m_green = 0 + (10 - m_count) * 25",
    "m_red = 0", "m_blue = 0", "m_green = 250", "GetTexture(\"effect\\\\LockOn128.tga\")",
    "GetHeight(m_pos.x, m_pos.z) - 2.0f",
])
ordered("canonical EXE", exe_fn, [
    "this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt == 9998 )", "this->m_stateCnt = 0",
    "if ( !this->m_stateCnt )", "LaunchEffectPrim(this, PP_3DTEXTURE, v3)",
    "m_duration = 9999", "m_longitude = 0.0", "m_longSpeed = 4.5", "m_roll = 90.0",
    "m_widthSize = 4.0", "m_heightSize = 4.0", "if ( this->m_count == 11 )",
    "m_red = 0", "m_blue = 0", "m_green = 250", "m_red = 250",
    "m_blue = 25 * (10 - this->m_count)", "m_green = 25 * (10 - this->m_count)",
    "aEffectLockon12", "GetHeight(GameMode->m_world->m_attr, this->m_pos.x, this->m_pos.z) - 2.0",
])
ordered("current", cur_fn, [
    "m_effectPos = m_master->m_pos", "if (m_stateCnt == 9998)", "m_stateCnt = 0",
    "if (m_stateCnt == 0)", "LaunchEffectPrim(PP_3DTEXTURE, vector3d{})", "prim->m_duration = 9999",
    "prim->m_longitude = 0.0f", "prim->m_longSpeed = 4.5f", "prim->m_roll = 90.0f",
    "prim->m_widthSize = 4.0f", "prim->m_heightSize = 4.0f", "if (m_count != 11)",
    "prim->m_red = 250", "prim->m_blue = (10 - m_count) * 25", "prim->m_green = (10 - m_count) * 25",
    "prim->m_red = 0", "prim->m_blue = 0", "prim->m_green = 250",
    'GetTexture("effect\\\\LockOn128.tga", false)',
    "GetHeight(\n        m_effectPos.x, m_effectPos.z) - 2.0f",
])

for label, fn in (("Ref", ref_fn), ("canonical EXE", exe_fn), ("current", cur_fn)):
    if re.search(r"\b(?:rand|random)\s*\(", fn):
        raise SystemExit(f"FAIL: {label} unexpectedly consumes RNG")

if 'char aEffectLockon12[] = "effect\\\\LockOn128.tga"' not in EXE:
    raise SystemExit("FAIL: canonical aEffectLockon12 does not resolve to LockOn128.tga")
print("PASS: EF_TOPRANK ID159/lifetime, dispatcher, reset/emission/color/texture/terrain branch order and no-RNG behavior match Ref/EXE/current")
