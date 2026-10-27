from pathlib import Path
import ast
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin-1")
HEADER = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="latin-1")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
IMPL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


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


# Derive the effect ID from the actual enum sequence, excluding comments.
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
if values.get("EF_TEST") != 2028:
    raise SystemExit(f"FAIL: expected the Ref EF_TEST enum value 2028, got {values.get('EF_TEST')}")

if "EF_TEST:" not in REF or "Test();" not in REF:
    raise SystemExit("FAIL: Ref caller lacks EF_TEST route")
if "case 709:\n            CRagEffect::Test(this);" not in EXE:
    raise SystemExit("FAIL: canonical dispatcher lacks case709 Test route")
if "if (m_type == 709) {\n        SpawnTestEffect();\n        return true;\n    }" not in IMPL:
    raise SystemExit("FAIL: current dispatcher lacks ID709 route")

ref_fn = body(REF, "void\tCRagEffect :: Test()", "bool CRagEffect::Angelus()")
exe_fn = body(EXE, "void __thiscall CRagEffect::Test(CRagEffect *this)", "//----- (00604F00)")
cur_fn = body(IMPL, "void CRagEffect::SpawnTestEffect()", "// Ref/RagEffect.cpp:8826-8852")

ordered("Ref", ref_fn, [
    "if (m_stateCnt % 5 == 0)", "for(a = 0; a < r; a++)",
    "LaunchEffectPrim(PP_3DPARTICLE)", "m_duration = 10",
    "MakeYRotation(GetRadian(random(360)))", "MakeXRotation(GetRadian(90))",
    "m_speed = (float)(random(20) + 20) / 80", "m_accel = -(g_prim->m_speed / g_prim->m_duration) / 2.0f",
    "m_size = (float)(80) / 100", "m_rollSpeed = 3.0f", "m_alpha = 70",
    "m_alphaSpeed = g_prim->m_maxAlpha / 10.0f", "m_fadeOutCnt = g_prim->m_duration - (g_prim->m_duration / 5)",
    "particle3.spr", "particle3.act", "InitSegment()",
    "m_pos.y = g_modeMgr.GetGameMode()->GetWorld()->m_attr->GetHeight(m_pos.x, m_pos.z) - 4.5f",
])
ordered("canonical EXE", exe_fn, [
    "if ( !(this->m_stateCnt % 5) )", "LaunchEffectPrim(this, PP_3DPARTICLE, angle)",
    "m_duration = 10", "v2 = rand()", "v2 % 360", "MakeYRotation",
    "GetRadian(90)", "MakeXRotation", "rand() % 20 + 20", "m_accel =",
    "m_size = 0.80000001", "m_rollSpeed = 3.0", "m_alpha = 70.0",
    "m_alphaSpeed = g_prim->m_maxAlpha * 0.1", "m_fadeOutCnt = g_prim->m_duration - g_prim->m_duration / 5",
    "byte_71BC24", "byte_71BC0C", "InitSegment(g_prim)",
    "this->m_pos.y = C3dAttr::GetHeight",
])
ordered("current", cur_fn, [
    "if (m_stateCnt % 5 == 0)", "LaunchEffectPrim(PP_3DPARTICLE, vector3d{})",
    "prim->m_duration = 10", "MatrixAppendYRotation(prim->m_matrix, GetRadian(rand() % 360))",
    "MatrixAppendXRotation(prim->m_matrix, GetRadian(90))", "rand() % 20 + 20",
    "prim->m_accel = -(prim->m_speed / prim->m_duration) / 2.0f",
    "prim->m_size = static_cast<float>(80) / 100.0f", "prim->m_rollSpeed = 3.0f",
    "prim->m_alpha = 70.0f", "prim->m_alphaSpeed = prim->m_maxAlpha / 10.0f",
    "prim->m_fadeOutCnt = prim->m_duration - prim->m_duration / 5",
    "particle3.spr", "particle3.act", "InitReferenceSegments(prim)",
    "m_effectPos.y = mode->m_world->m_attr->GetHeight(m_effectPos.x, m_effectPos.z) - 4.5f",
])

for label, fn in (("Ref", ref_fn), ("canonical EXE", exe_fn), ("current", cur_fn)):
    draws = len(re.findall(r"\b(?:rand|random)\s*\(", fn))
    if draws != 2:
        raise SystemExit(f"FAIL: {label} has {draws} RNG calls, expected two")

# Resolve the opaque canonical symbols through the executable's PE sections.
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
        off = sections + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, off + 8)
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            pos = raw_pointer + rva - virtual_address
            return data[pos:data.index(b"\0", pos)]
    raise SystemExit(f"FAIL: canonical VA {va:#x} outside PE sections")


resources = re.findall(
    r'prim->SetParticleResources\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")\)', cur_fn
)
if (len(resources) != 1
        or ast.literal_eval(resources[0][0]).encode("latin-1") != c_string(0x71BC24)
        or ast.literal_eval(resources[0][1]).encode("latin-1") != c_string(0x71BC0C)):
    raise SystemExit("FAIL: EF_TEST particle3 paths differ from canonical PE asset bytes")

print("PASS: EF_TEST Ref enum=2028 vs EXE/current dispatch=709 discrepancy; branch order, two RNG draws, emit-before-ground-update order and PE assets match")
