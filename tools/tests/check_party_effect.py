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
if values.get("EF_PARTY") != 160:
    raise SystemExit(f"FAIL: Ref enum does not derive EF_PARTY=160 ({values.get('EF_PARTY')})")
if not re.search(r"9999\s*,\s*//\s*160\b", DURATION):
    raise SystemExit("FAIL: generated current duration table does not retain EF_PARTY lifetime9999")
if "EF_PARTY:" not in REF or "Party();" not in REF:
    raise SystemExit("FAIL: Ref caller lacks EF_PARTY route")
if "case 160:\n            CRagEffect::Party(this);" not in EXE:
    raise SystemExit("FAIL: canonical dispatcher lacks case160 Party route")
if "case 160:\n        // ProcessEffectFrame owns the source Party emitter for EF_PARTY.\n        m_handler = Handler::None;" not in IMPL:
    raise SystemExit("FAIL: current Init does not reserve ID160 for the direct source handler")
if "if (m_type == 160) {\n        SpawnParty();\n        return true;\n    }" not in IMPL:
    raise SystemExit("FAIL: current frame dispatcher lacks ID160 Party route")

ref_fn = body(REF, "void CRagEffect :: Party()", "void\tCRagEffect :: StatusState()")
exe_fn = body(EXE, "void __thiscall CRagEffect::Party(CRagEffect *this)", "//----- (00602E00)")
cur_fn = body(IMPL, "void CRagEffect::SpawnParty()", "// Ref/RagEffect.cpp:8826-8852")

for label, fn in (("Ref", ref_fn), ("canonical EXE", exe_fn), ("current", cur_fn)):
    if re.search(r"\b(?:rand|random)\s*\(", fn):
        raise SystemExit(f"FAIL: {label} unexpectedly consumes RNG")

ordered("Ref", ref_fn, [
    "m_pos = m_master->m_pos", "if (m_stateCnt == 9998)", "m_stateCnt = 0",
    "if (m_stateCnt == 0)", "LaunchEffectPrim(PP_3DTEXTURE)", "m_duration = 9999",
    "m_longitude = 0", "m_roll = 90", "m_widthSize = 4.0f", "m_heightSize = 4.0f",
    "m_red = 50", "m_blue = 253", "m_green = 50", "GetTexture(\"effect\\\\party.tga\")",
    "GetHeight(m_pos.x, m_pos.z) - 2.0f",
])
ordered("canonical EXE", exe_fn, [
    "this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt == 9998 )", "this->m_stateCnt = 0",
    "if ( !this->m_stateCnt )", "LaunchEffectPrim(this, PP_3DTEXTURE, v3)", "m_duration = 9999",
    "m_longitude = 0.0", "m_roll = 90.0", "m_widthSize = 4.0", "m_heightSize = 4.0",
    "m_red = 50", "m_blue = 253", "m_green = 50", "aEffectPartyTga",
    "GetHeight(GameMode->m_world->m_attr, this->m_pos.x, this->m_pos.z) - 2.0",
])
ordered("current", cur_fn, [
    "m_effectPos = m_master->m_pos", "if (m_stateCnt == 9998)", "m_stateCnt = 0",
    "if (m_stateCnt == 0)", "LaunchEffectPrim(PP_3DTEXTURE, vector3d{})", "prim->m_duration = 9999",
    "prim->m_longitude = 0.0f", "prim->m_roll = 90.0f", "prim->m_widthSize = 4.0f",
    "prim->m_heightSize = 4.0f", "prim->m_red = 50", "prim->m_blue = 253", "prim->m_green = 50",
    'GetTexture("effect\\\\party.tga", false)', "GetHeight(\n        m_effectPos.x, m_effectPos.z) - 2.0f",
])
if 'char aEffectPartyTga[] = "effect\\\\party.tga"' not in EXE:
    raise SystemExit("FAIL: canonical Party texture symbol does not resolve to exact effect\\party.tga")
print("PASS: EF_PARTY ID160/lifetime, dispatch and ordered reset/emission/payload/texture/terrain branches match Ref/EXE/current")
