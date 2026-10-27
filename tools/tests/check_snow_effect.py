from pathlib import Path
import ast
import re
import struct

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
if values.get("EF_SNOW") != 162:
    raise SystemExit(f"FAIL: Ref enum does not derive EF_SNOW=162 ({values.get('EF_SNOW')})")
if not re.search(r"99999999\s*,\s*//\s*162\b", DURATION):
    raise SystemExit("FAIL: generated current duration table does not retain EF_SNOW lifetime")
if not re.search(r"case\s+EF_SNOW:\s*#if !defined\(INSTANT_EVENT_SERVER\)\s*Snow\(\)", REF):
    raise SystemExit("FAIL: Ref caller lacks EF_SNOW route")
if "case 162:\n            CRagEffect::Snow(this);" not in EXE:
    raise SystemExit("FAIL: canonical dispatcher lacks case162 Snow route")
if "case 162:\n        // ProcessEffectFrame owns the source Snow emitter for EF_SNOW.\n        m_handler = Handler::None;" not in IMPL:
    raise SystemExit("FAIL: current Init lacks direct Snow handler selection")
if "if (m_type == 162) {\n        SpawnSnow();\n        return true;\n    }" not in IMPL:
    raise SystemExit("FAIL: current ProcessEffectFrame lacks ID162 Snow route")

ref_fn = body(REF, "void\tCRagEffect :: Snow()", "void\tCRagEffect :: Coin()")
exe_fn = body(EXE, "void __thiscall CRagEffect::Snow(CRagEffect *this)", "//----- (005F7770)")
cur_fn = body(IMPL, "void CRagEffect::SpawnSnow()", "// Ref/RagEffect.cpp:8826-8852")

ordered("Ref", ref_fn, [
    "CWorld* world = g_modeMgr.GetGameMode()->GetWorld()",
    "if (m_stateCnt == 0)", "m_pos = pPos",
    "if( GetDuration ()-GetRagStateCnt()>160 )", "for( int i=0; i<2 ; i++ )",
    "LaunchEffectPrim(PP_3DPARTICLE_NOMASTER)", "m_orgPos = m_pos", "m_duration = 320",
    "MakeYRotation(GetRadian(random(360)))", "m_deltaPos2.MatrixMult",
    "random(300) - 300", "MakeXRotation(GetRadian(90))", "m_speed = -0.5f", "m_accel = 0.0f",
    "m_size = (float)(random(15) + 25) / 50", "m_rollSpeed = 0.0f", "m_alpha = 0",
    "m_alphaSpeed = g_prim->m_maxAlpha / 10.0f", "m_fadeOutCnt = g_prim->m_duration - (g_prim->m_duration / 5)",
    "ef_snow.spr", "ef_snow.act", "InitSegment()",
])
ordered("canonical EXE", exe_fn, [
    "this->m_pos = world->m_player->m_pos", "this->m_duration - this->m_stateCnt > 160",
    "v10 = 2", "LaunchEffectPrim(this, PP_3DPARTICLE_NOMASTER, angle[0])",
    "m_orgPos = this->m_pos", "m_duration = 320", "v2 = rand()", "v2 % 360",
    "MakeYRotation", "v3 = rand() % 300", "GetHeight(world->m_attr, this->m_pos.x, this->m_pos.z) - 100.0",
    "v5 = (double)(v3 - 300)", "m_deltaPos2.x =", "p_m_deltaPos2->y =", "p_m_deltaPos2->z =",
    "GetRadian(90)", "MakeXRotation", "m_speed = -0.5", "m_accel = 0.0",
    "rand() % 15 + 25", "m_rollSpeed = 0.0", "m_alpha = 0.0", "m_alphaSpeed = g_prim->m_maxAlpha * 0.1",
    "m_fadeOutCnt = g_prim->m_duration - g_prim->m_duration / 5", "byte_71B4EC", "byte_71B4D8",
    "InitSegment(g_prim)", "--v10", "while ( v10 )",
])
ordered("current", cur_fn, [
    "m_effectPos = world->m_player->m_pos", "if (m_duration - m_stateCnt > 160)",
    "for (int index = 0; index < 2; ++index)",
    "LaunchEffectPrim(PP_3DPARTICLE_NOMASTER, vector3d{})", "prim->m_orgPos = m_effectPos",
    "prim->m_duration = 320", "MatrixAppendYRotation(prim->m_matrix, GetRadian(rand() % 360))",
    "const int zOffset = rand() % 300 - 300", "GetHeight(m_effectPos.x, m_effectPos.z) - 100.0f",
    "prim->m_deltaPos2 =", "MatrixAppendXRotation(prim->m_matrix, GetRadian(90))",
    "prim->m_speed = -0.5f", "prim->m_accel = 0.0f", "rand() % 15 + 25",
    "prim->m_rollSpeed = 0.0f", "prim->m_alpha = 0.0f", "prim->m_alphaSpeed = prim->m_maxAlpha / 10.0f",
    "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5",
    "ef_snow.spr", "ef_snow.act", "prim->m_numSegments = 1", "InitReferenceSegments(prim)",
])

for label, fn in (("Ref", ref_fn), ("canonical EXE", exe_fn), ("current", cur_fn)):
    if label == "canonical EXE":
        has_angle = "v2 = rand()" in fn and "v2 % 360" in fn
    elif label == "Ref":
        has_angle = bool(re.search(r"\b(?:random|rand)\s*\(\s*360\s*\)", fn))
    else:
        has_angle = "rand() % 360" in fn
    if not has_angle:
        raise SystemExit(f"FAIL: {label} lacks angle RNG draw")
    if label == "canonical EXE":
        has_offset = "v3 = rand() % 300" in fn
    elif label == "Ref":
        has_offset = bool(re.search(r"\b(?:random|rand)\s*\(\s*300\s*\)", fn))
    else:
        has_offset = "rand() % 300" in fn
    if not has_offset:
        raise SystemExit(f"FAIL: {label} lacks offset RNG draw")
    if label == "canonical EXE":
        has_size = "rand() % 15 + 25" in fn
    elif label == "Ref":
        has_size = bool(re.search(r"\b(?:random|rand)\s*\(\s*15\s*\)", fn))
    else:
        has_size = "rand() % 15 + 25" in fn
    if not has_size:
        raise SystemExit(f"FAIL: {label} lacks size RNG draw")
if not re.search(r"for\s*\(\s*int i\s*=\s*0\s*;\s*i\s*<\s*2", ref_fn):
    raise SystemExit("FAIL: Ref child loop no longer emits two particles")
if not re.search(r"for\s*\(\s*int index\s*=\s*0\s*;\s*index\s*<\s*2", cur_fn):
    raise SystemExit("FAIL: current child loop no longer emits two particles")

# The Ref C++ embeds rand(300) in a function-call argument whose evaluation
# order is not fixed by the language; the canonical machine output orders the
# draw before terrain sampling. The implementation follows that EXE order.
ordered("current source-defined RNG/terrain order", cur_fn, [
    "rand() % 360", "rand() % 300 - 300", "GetHeight(m_effectPos.x, m_effectPos.z)", "rand() % 15 + 25",
])
for label, fn, pattern in (("Ref", ref_fn, r"\brandom\s*\("),
                          ("canonical EXE", exe_fn, r"\brand\s*\("),
                          ("current", cur_fn, r"\brand\s*\(")):
    draws = len(re.findall(pattern, fn))
    if draws != 3:
        raise SystemExit(f"FAIL: {label} has {draws} RNG call sites inside the two-child loop, expected three")
ordered("canonical transformed offset", exe_fn, [
    "m_deltaPos2.x = v5 * g_prim->m_matrix.v31 + v4 * g_prim->m_matrix.v21 + g_prim->m_matrix.v41",
    "p_m_deltaPos2->y = v5 * p_m_matrix->v32 + v4 * p_m_matrix->v22 + p_m_matrix->v42",
    "p_m_deltaPos2->z = v5 * p_m_matrix->v33 + v4 * p_m_matrix->v23 + p_m_matrix->v43",
])
ordered("current transformed offset", cur_fn, [
    "zOffset * prim->m_matrix.m[2][0] + yOffset * prim->m_matrix.m[1][0] + prim->m_matrix.m[3][0]",
    "zOffset * prim->m_matrix.m[2][1] + yOffset * prim->m_matrix.m[1][1] + prim->m_matrix.m[3][1]",
    "zOffset * prim->m_matrix.m[2][2] + yOffset * prim->m_matrix.m[1][2] + prim->m_matrix.m[3][2]",
])

# Resolve the two canonical opaque names through the PE image and compare the
# current byte strings directly.
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
    raise SystemExit(f"FAIL: canonical VA {va:#x} is outside PE sections")


requested = re.findall(r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)', cur_fn)
if (len(requested) != 1
        or ast.literal_eval(requested[0][0]).encode("latin-1") != c_string(0x71B4EC)
        or ast.literal_eval(requested[0][1]).encode("latin-1") != c_string(0x71B4D8)):
    raise SystemExit("FAIL: Snow sprite/action bytes differ from canonical PE symbols 0x71B4EC/0x71B4D8")
if "if (m_type == PP_3DPARTICLE_NOMASTER) {\n        return UpdateReferenceNoMasterParticle(this);\n    }" not in IMPL:
    raise SystemExit("FAIL: PP_3DPARTICLE_NOMASTER does not use its reference updater")

print("PASS: EF_SNOW ID162/lifetime, dispatcher, two-child/three-RNG loop, EXE RNG-terrain order, payload, no-master route and PE assets match")
