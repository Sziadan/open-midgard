from pathlib import Path
import ast
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin-1")
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
if values.get("EF_SAKURA") != 163:
    raise SystemExit(f"FAIL: Ref enum does not derive EF_SAKURA=163 ({values.get('EF_SAKURA')})")
if not re.search(r"99999999\s*,\s*//\s*163\b", DURATION):
    raise SystemExit("FAIL: current duration table does not retain EF_SAKURA lifetime")
if not re.search(r"case\s+EF_SAKURA:\s*Sakura\(\)", REF):
    raise SystemExit("FAIL: readable RagEffect caller lacks EF_SAKURA route")
if "case 163:\n            CRagEffect::Sakura(this);" not in EXE:
    raise SystemExit("FAIL: canonical dispatcher lacks case163 Sakura route")
if "case 163:\n        // ProcessEffectFrame owns the source Sakura emitter for EF_SAKURA.\n        m_handler = Handler::None;" not in IMPL:
    raise SystemExit("FAIL: current Init does not select the direct Sakura path")
if "if (m_type == 163) {\n        SpawnSakura();\n        return true;\n    }" not in IMPL:
    raise SystemExit("FAIL: current frame dispatcher lacks ID163 Sakura route")

ref_fn = body(REF2, "void\tCRagEffect :: Sakura()", "void\tCRagEffect :: Kouenka()")
exe_fn = body(EXE, "void __thiscall CRagEffect::Sakura(CRagEffect *this)", "//----- (0060A860)")
cur_fn = body(IMPL, "void CRagEffect::SpawnSakura()", "// Ref/RagEffect.cpp:8826-8852")

ordered("Ref", ref_fn, [
    "m_pos = m_master->m_pos", "if (m_stateCnt == 99999998)", "m_stateCnt = 0",
    "m_duration = 99999999", "if ( m_stateCnt%2 == 0 && m_stateCnt < 300 )",
    "LaunchEffectPrim(PP_SAKURA)", "m_duration = m_duration", "m_GI[ec].life = 1",
    "random(2)+2", "random(6) + 22", "height[0]=random(360)", "height[1]=random(360)",
    "height[2]=random(360)", "height[3]=random(360)", "vecB_pre.x=m_pos.x+random(300)-150",
    "vecB_pre.y=m_pos.y-200", "vecB_pre.z=m_pos.z+random(300)-150", "flag1[0]=0",
    "m_alpha = 200", "sakura01.spr", "sakura01.act", "SetAction(random(3), 36, MT_LOOP)",
    "InitSegment()", "for (ec=1;ec<4;ec++)", "m_GI[ec].life = 0",
])
ordered("canonical EXE", exe_fn, [
    "this->m_pos = this->m_master->m_pos", "if ( this->m_stateCnt == 99999998 )",
    "this->m_stateCnt = 0", "this->m_duration = 99999999", "if ( !(this->m_stateCnt % 2) && this->m_stateCnt < 300 )",
    "LaunchEffectPrim(this, PP_SAKURA, v5)", "m_duration = this->m_duration", "m_GI[0].life = 1",
    "rand() % 2", "rand() % 6 + 22", "rand() % 360", "rand() % 360", "rand() % 360", "rand() % 360",
    "rand() % 300", "vecB_pre.y = this->m_pos.y - 200.0", "v6 = rand() % 300",
    "vecB_pre.z = (double)v6 + this->m_pos.z - 150.0", "flag1[0] = 0", "m_alpha = 200.0",
    "byte_71BFB8", "byte_71BFA4", "v3 = rand()", "v3 % 3", "36, 0", "InitSegment(g_prim)",
    "for ( i = 180; i < 720;", "i += 180",
])
ordered("current", cur_fn, [
    "m_effectPos = m_master->m_pos", "if (m_stateCnt == 99999998)", "m_stateCnt = 0",
    "m_duration = 99999999", "if (m_stateCnt % 2 == 0 && m_stateCnt < 300)",
    "LaunchEffectPrim(PP_SAKURA, vector3d{})", "prim->m_duration = m_duration", "gi.life = 1",
    "rand() % 2 + 2", "rand() % 6 + 22", "rand() % 360", "rand() % 360", "rand() % 360", "rand() % 360",
    "rand() % 300", "m_effectPos.x - 150.0", "gi.arcBaselineY = m_effectPos.y - 200.0f",
    "rand() % 300", "m_effectPos.z - 150.0", "gi.flag[0] = 0", "prim->m_alpha = 200.0f",
    "sakura01.spr", "sakura01.act", "prim->m_spriteAction = rand() % 3", "prim->m_spriteMotionBase = 36",
    "prim->m_spriteRepeat = true", "prim->m_curMotion = 0", "InitReferenceSegments(prim)",
    "for (int index = 1; index < 4; ++index)", "life = 0",
])

for label, fn, pattern in (("Ref", ref_fn, r"\brandom\s*\("),
                           ("canonical EXE", exe_fn, r"\brand\s*\("),
                           ("current", cur_fn, r"\brand\s*\(")):
    draws = len(re.findall(pattern, fn))
    if draws != 9:
        raise SystemExit(f"FAIL: {label} has {draws} RNG call sites per emission, expected nine")

ordered("Ref RNG/position order", ref_fn, [
    "random(2)", "random(6)", "random(360)", "random(360)", "random(360)", "random(360)",
    "random(300)", "m_pos.y-200", "random(300)", "SetAction(random(3)",
])
ordered("canonical RNG/position order", exe_fn, [
    "rand() % 2", "rand() % 6 + 22", "rand() % 360", "rand() % 360", "rand() % 360", "rand() % 360",
    "rand() % 300", "m_pos.y - 200.0", "v6 = rand() % 300", "v3 = rand()", "v3 % 3",
])
ordered("current RNG/position order", cur_fn, [
    "rand() % 2", "rand() % 6", "rand() % 360", "rand() % 360", "rand() % 360", "rand() % 360",
    "rand() % 300", "m_effectPos.y - 200.0f", "rand() % 300", "rand() % 3",
])

# Resolve canonical sprite/action pointers through the PE and compare literal bytes.
data = (ROOT / "HighPriest.exe").read_bytes()
pe = struct.unpack_from("<I", data, 0x3C)[0]
coff = pe + 4
section_count = struct.unpack_from("<H", data, coff + 2)[0]
optional_size = struct.unpack_from("<H", data, coff + 16)[0]
optional = coff + 20
image_base = struct.unpack_from("<I", data, optional + 28)[0]
sections = optional + optional_size


def c_string(va):
    rva = va - image_base
    for index in range(section_count):
        offset = sections + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, offset + 8)
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            pos = raw_pointer + rva - virtual_address
            return data[pos:data.index(b"\0", pos)]
    raise SystemExit(f"FAIL: canonical VA {va:#x} outside PE sections")


resources = re.findall(r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)', cur_fn)
if (len(resources) != 1
        or ast.literal_eval(resources[0][0]).encode("latin-1") != c_string(0x71BFB8)
        or ast.literal_eval(resources[0][1]).encode("latin-1") != c_string(0x71BFA4)):
    raise SystemExit("FAIL: Sakura resources differ from canonical PE symbols 0x71BFB8/0x71BFA4")
if "UpdateReferenceSakura(this); // Source case 0x28 ignores PrimSakura's return." not in IMPL:
    raise SystemExit("FAIL: PP_SAKURA does not dispatch to the source updater")

print("PASS: EF_SAKURA ID163/lifetime, reset/cadence, nine-draw branch payload, action/resource setup and PP_SAKURA route match")
