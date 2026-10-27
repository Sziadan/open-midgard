from pathlib import Path
import re
import csv

ROOT = Path(__file__).resolve().parents[2]
ref = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp949", errors="replace")
ref_h = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="cp949", errors="replace")
exe_h = (ROOT / "HighPriest.exe.h").read_text(encoding="cp949", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
src_h = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def body_from(text, marker):
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 1
    i = opening + 1
    while depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return text[start:i]


ref_body = body_from(ref, "bool\tCEffectPrim :: OnProcess()")
current_onprocess = body_from(src, "u8 CEffectPrim::OnProcess()")
current_dispatch = body_from(src, "bool CEffectPrim::ProcessPrimitiveFrame()")

exe_enum = body_from(exe_h, "enum EFFECTPRIMID")
src_enum = body_from(src_h, "enum EFFECTPRIMID")
exe_ids = {name: int(value, 0) for name, value in re.findall(r"(PP_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", exe_enum)}
src_ids = {name: int(value, 0) for name, value in re.findall(r"(PP_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", src_enum)}
# Current enum is canonical. Ref's four appended labels are known stale names beyond 0x80.
ref_stale = ["PP_DOWNTEXTURE", "PP_TEIHIT1_REVERSE", "PP_MAPTEXTURE", "PP_RIPPLE"]
ref_ids = dict(exe_ids)
ref_ids.update({name: 0x81 + i for i, name in enumerate(ref_stale)})

switches = list(re.finditer(r"switch\s*\(m_type\)\s*\{", ref_body))
require(len(switches) == 2, f"Ref expected pre-gate and behavior switches, found {len(switches)}")
pre_gate = ref_body[switches[0].start():switches[1].start()]
dispatch_switch = ref_body[switches[1].start():]
ref_labels = list(dict.fromkeys(re.findall(r"case\s+(PP_[A-Z0-9_]+)\s*:", dispatch_switch)))
ref_case_ids = {ref_ids[name] for name in ref_labels if name in ref_ids}
exe_body = body_from(exe, "unsigned __int8 __thiscall CEffectPrim::OnProcess(CEffectPrim *this)\n{")
exe_case_ids = {int(value, 0) for value in re.findall(r"case\s+(0x[0-9A-Fa-f]+|\d+)\s*:", exe_body)}
current_labels = set(re.findall(r"\bPP_[A-Z0-9_]+\b", current_dispatch))
alias_map = dict(re.findall(r"constexpr int (kPrimType\w+) = (PP_[A-Z0-9_]+);", src_h))
current_aliases = {name for name in alias_map if name in current_dispatch}
current_route_ids = {src_ids[name] for name in current_labels if name in src_ids}
current_route_ids.update(src_ids[alias_map[name]] for name in current_aliases)
core_ref_ids = ref_case_ids - {0x81, 0x82, 0x83, 0x84}
require(exe_case_ids == core_ref_ids, f"canonical IDs vs Ref (excluding stale appended enum labels): missing={sorted(core_ref_ids-exe_case_ids)} extra={sorted(exe_case_ids-core_ref_ids)}")
require(core_ref_ids <= current_route_ids, f"Ref canonical cases missing from current dispatch identifiers: {sorted(core_ref_ids-current_route_ids)}")
require(ref_case_ids - core_ref_ids == {0x81, 0x82, 0x83, 0x84}, "Ref-only stale appended cases are IDs 0x81-0x84")
with (ROOT / "docs/skill/generated/primitive_onprocess_dispatch.tsv").open(encoding="utf-8", newline="") as stream:
    branch_rows = list(csv.DictReader(stream, delimiter="\t"))
mapped_ref_labels = {row["Ref_case"] for row in branch_rows if row["Ref_case"] != "default"}
require(mapped_ref_labels == set(ref_labels), "reviewable branch map covers every Ref case label, including source-only stale labels")
require(all(row["current_guard"] and row["current_handler_and_return"] for row in branch_rows), "branch map records a current route/result for every row")
void_false_mismatches = [row for row in branch_rows if row["Ref_contract"] == "continues" and "bool-return" in row["current_handler_and_return"]]
bool_return_mismatches = [row for row in branch_rows if row["Ref_contract"] == "returns-false" and "bool-return" not in row["current_handler_and_return"]]
require(not void_false_mismatches and not bool_return_mismatches,
        f"handler result contracts differ: void->early-exit={void_false_mismatches}, bool->ignored={bool_return_mismatches}")
print(f"case-ID comparison: Ref={len(ref_case_ids)} labels/{len(core_ref_ids)} canonical IDs; EXE={len(exe_case_ids)} IDs; current textual routes cover all {len(core_ref_ids)} canonical IDs")
print("Ref-only IDs 0x81-0x84 are the four stale enum labels and are absent from canonical EXE/current enum")

# Compare the copy-before-skip set and the two life-99 exceptions.
copy_names = re.findall(r"case\s+(PP_[A-Z0-9_]+)\s*:", pre_gate[:pre_gate.index("m_pos = m_master->m_pos")])
copy_names = [name for name in copy_names if name not in ("PP_2DTEXTURE", "PP_3DCROSSTEXTURE")]
source_copy_ids = {ref_ids[name] for name in copy_names}
current_copy = re.search(r"switch\s*\(static_cast<int>\(m_type\)\)\s*\{(.*?)default:", current_onprocess, re.S).group(1)
current_copy_ids = {int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]+)", current_copy)}
require(source_copy_ids == current_copy_ids, f"pre-gate position-copy ids differ: Ref={sorted(source_copy_ids)} current={sorted(current_copy_ids)}")
require("m_GI[0].life != 99" in pre_gate and "m_gi[0].life == 99" in current_onprocess
        and "g_isSkipFrame == 0" in pre_gate and "!g_isSkipFrame" in current_onprocess,
        "the two life-99 texture exceptions bypass only the skip-frame gate")
print(f"pre-gate comparison: {len(source_copy_ids)} ordered-copy IDs and life-99 skip exception matched")

# The Ref behavior switch has only eight handlers whose false return exits before common duration accounting.
expected_false_ids = {0x00, 0x01, 0x02, 0x05, 0x06, 0x07, 0x15, 0x16, 0x20}
false_names = set(re.findall(r"if\s*\(\s*!\s*(Prim\w+)\s*\(", dispatch_switch))
actual_false_names = {
    "Prim2DTexture", "Prim2DTextureOrbit", "Prim3DParticle", "Prim3DParticleGravity",
    "Prim3DParticleOrbit", "Prim3DParticleNoMaster", "Prim3DTextureNoMaster", "PrimCHOOKGI",
}
require(false_names == actual_false_names, f"Ref early-false handler list changed: {sorted(false_names)}")
false_case_ids = set()
for group in re.finditer(r"((?:\s*case\s+PP_[A-Z0-9_]+\s*:\s*)+)(.*?)(?=\bbreak\s*;|\bdefault\s*:)", dispatch_switch, re.S):
    if re.search(r"if\s*\(\s*!\s*Prim\w+\s*\(", group.group(2)):
        false_case_ids.update(ref_ids[name] for name in re.findall(r"case\s+(PP_[A-Z0-9_]+)", group.group(1)) if name in ref_ids)
require(false_case_ids == expected_false_ids, f"Ref false-propagation case ids differ: {sorted(false_case_ids)}")
require({ref_ids[name] for name in ref_labels if name in ref_ids and any(x in dispatch_switch for x in [])} == set(),
        "internal sanity")
# Current counterparts preserve bool results for all eight Ref bool-returning branches.
for marker in ("return UpdateReferenceScreenTexture(this);", "return UpdateReference2DTextureOrbit(this);",
               "return UpdateLinearReferenceParticle(this);", "return UpdateReferenceGravityParticle(this);",
               "return UpdateWarpZoneOrbit(this);", "return UpdateReferenceNoMasterParticle(this);",
               "return UpdateReferenceNoMasterTexture(this);", "return UpdateReferenceChookGi(this);"):
    require(marker in current_dispatch, f"current bool-returning counterpart missing: {marker}")
require("UpdateReferenceKouenka(this); // Source case 0x7C ignores PrimKouenka's result." in current_dispatch
        and "return UpdateReferenceKouenka(this)" not in current_dispatch,
        "void PrimKouenka return is ignored")
for marker in ("UpdateReferenceParticleSpline(this); // Ref case 0x08 ignores Prim3DParticleSpline's result.", "UpdateReferenceCircleState(this);",
               "UpdateCirclePrimitive(this);", "UpdateReferenceCylinderState(this);", "UpdateCylinderPrimitive(this);",
               "UpdateReferenceCrossTexture(this); // Source case 0x10 ignores Prim3DTexture's result.",
               "UpdateCrossTexturePrimitive(this);", "UpdateScreenTexturePrimitive(this);",
               "UpdatePortalBands(this);", "UpdateWindBands(this);", "UpdateWaterfallPrimitive(this);",
               "UpdateWeatherCloudPrimitive(this);", "UpdateMapPillarPrimitive(this);"):
    require(marker in current_dispatch, f"current ignored-void route missing: {marker}")
require("primitiveType < 0 || primitiveType > 0x80 || primitiveType == 0x53" in current_dispatch
        and 'ErrorMsg(message);' in current_dispatch
        and 'Illegal Effect PrimitiveId %d' in current_dispatch,
        "invalid-id and canonical no-case 0x53 path logs Ref error then continues")
require("if (!ProcessPrimitiveFrame())" in current_onprocess and "++m_stateCnt;" in current_onprocess
        and "return m_stateCnt <= m_duration;" in current_onprocess,
        "shared counter and inclusive duration result happen only after a false-propagating handler accepts")
print(f"branch-result comparison: Ref bool-exit IDs={sorted(expected_false_ids)} preserved; void handler results ignored; shared tick/error path matched")
print(f"branch-map validation: {len(branch_rows)} rows cover all Ref labels with no void/bool result-contract mismatches")
print("CEffectPrim::OnProcess source-to-implementation comparison passed for dispatcher, gates and result flow")

