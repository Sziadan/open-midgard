#!/usr/bin/env python3
"""Compare effect 522 Gravitation and its quake receiver with the PE and Ref."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.ro_exe_resolve import ExeImage, SymbolTable  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
REF2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="utf-8", errors="replace")
MODE_REF = (ROOT / "Ref/GameMode2.cpp").read_text(encoding="utf-8", errors="replace")
VIEW_REF = (ROOT / "Ref/View.cpp").read_text(encoding="utf-8", errors="replace")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
LIFECYCLE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8", errors="replace")
MODE_LOCAL = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8", errors="replace")
VIEW_LOCAL = (ROOT / "src/gamemode/View.cpp").read_text(encoding="utf-8", errors="replace")
VIEW_HEADER = (ROOT / "src/gamemode/View.h").read_text(encoding="utf-8", errors="replace")
DISPATCH = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
symbols = SymbolTable(ExeImage())


def require(ok: bool, message: str) -> None:
    if not ok:
        raise SystemExit(message)


def function_body(text: str, signature: str) -> str:
    matches = list(re.finditer(signature, text))
    require(bool(matches), f"missing source function: {signature}")
    match = matches[-1]
    start = text.find("{", match.end())
    depth = 0
    for pos in range(start, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:pos]
    raise SystemExit(f"unterminated source function: {signature}")


require("case 0x20A:\n      CRagEffect::Gravitation(this);" in PE
        and "case 0x20A:\n      CRagEffect::Gravitation(this);" in REF,
        "effect 522 PE/primary Ref caller must dispatch to Gravitation")
require("case 522: ApplyGravitation(); return true;" in DISPATCH,
        "effect 522 dispatch route changed")
for token in ("effect.ApplyGravitation();", "effect522 production Gravitation emits four lanes",
              "effect522 sends quake messages in order", "effect522 assigns two ordered stone textures",
              "effect522 preserves ordered PP_3DQUADHORN2 payloads",
              "effect522 nonzero state returns without duplicate primitives"):
    require(token in LIFECYCLE, f"effect522 Gravitation lifecycle fixture missing {token}")

pe = function_body(PE, r"void __thiscall CRagEffect::Gravitation\(CRagEffect \*this\)")
ref = function_body(REF2, r"void\s+CRagEffect\s*::\s*Gravitation\(\)")
local = function_body(LOCAL, r"void CRagEffect::ApplyGravitation\(\)")
for label, body in (("PE", pe), ("secondary Ref", ref)):
    require("m_stateCnt" in body and "m_duration" in body and "PP_3DQUADHORN2" in body,
            f"{label} Gravitation state gate/primitive setup missing")
    for token in ("m_deltaPos2", "m_size", "m_heightSize", "m_longitude", "m_latitude",
                  "m_matrix", "m_rotMat", "m_speed", "m_accel", "m_alpha", "m_count",
                  "m_ChangePoint", "m_ChangeSpeed", "m_ChangeAccel", "m_fadeOutCnt"):
        require(token in body, f"{label} Gravitation field {token} missing")
    loop_proven = ("v9 < 4" in body or "i<4" in body or "i < 4" in body)
    require(loop_proven, f"{label} Gravitation must create four ordered primitives")
require("rand() % 360" in pe and "rand();" in pe and "rand() % 50" in pe
        and "rand() % 40" in pe and pe.count("rand()") == 5,
        "PE Gravitation per-lane RNG call sequence changed")
require("random(360)" in ref and "random(4)" in ref and "random(50)" in ref
        and "random(40)" in ref,
        "secondary Ref Gravitation random sequence changed")
for token in ("if (m_stateCnt != 0)", "for (int index = 0; index < 4; ++index)",
              "PP_3DQUADHORN2", "m_duration", "m_pattern = 1", "rand() % 360",
              "(void)rand()", "rand() % 50", "rand() % 40", "m_deltaPos2 = {0.0f, 10.0f, 0.0f}",
              "m_heightSize = 18.0f", "m_speed = 1.0f", "m_count = 15", "m_changePoint = 12",
              "m_changeSpeed = -1.2f", "m_fadeOutCnt = prim->m_duration - 10"):
    require(token in local, f"local Gravitation missing {token}")
require(local.count("rand()") == 5, "local Gravitation must consume five random draws per lane")
require(local.index("SendMsg(135") < local.index("SendMsg(148") < local.index("for (int index")
        < local.index("PP_3DQUADHORN2"), "Gravitation message and primitive ordering changed")

for name, expected in (("aEffectStoneBmp", b"effect\\stone.bmp"),
                       ("aEffectIceTga", b"effect\\ice.tga")):
    resolved = symbols.resolve_name(name)
    require(resolved is not None and resolved.raw == expected,
            f"{name} executable resource bytes changed")
require("aEffectStoneBmp" in pe and "aEffectIceTga" in pe,
        "PE Gravitation texture ordering changed")
require("effect\\\\stone.bmp" in ref and "effect\\\\ice.tga" in ref,
        "secondary Ref Gravitation texture ordering changed")
require('index < 2 ? "effect\\\\stone.bmp" : "effect\\\\ice.tga"' in local,
        "local Gravitation must use two stone then two ice textures")

# Receiver parity: message 148 writes the three quake fields through pointers.
require("case 0x94u:" in MODE_REF and "CView::SetQuakeInfo" in MODE_REF,
        "Ref GameMode message 148 receiver missing")
require("void __thiscall CView::SetQuakeInfo(CView *this, float sideQuake, float frontQuake, float latitudeQuake)" in VIEW_REF,
        "Ref SetQuakeInfo implementation missing")
mode_case = re.search(r"case 148:\s*(.*?)\s*return 0;", MODE_LOCAL, re.S)
require(mode_case is not None and "SetQuakeInfo" in mode_case.group(1)
        and "reinterpret_cast<const float*>(wparam)" in mode_case.group(1)
        and "reinterpret_cast<const float*>(lparam)" in mode_case.group(1)
        and "reinterpret_cast<const float*>(extra)" in mode_case.group(1),
        "local GameMode must dereference all three message 148 float pointers")
view_set = function_body(VIEW_LOCAL, r"void CView::SetQuakeInfo\(float sideQuake, float frontQuake, float latitudeQuake\)")
require("m_sideQuake = sideQuake;" in view_set and "m_frontQuake = frontQuake;" in view_set
        and "m_latitudeQuake = latitudeQuake;" in view_set
        and "SetQuakeInfo(float sideQuake, float frontQuake, float latitudeQuake)" in VIEW_HEADER,
        "local SetQuakeInfo must assign the three quake amplitudes")
require("case 135:" in MODE_LOCAL and "case 148:" in MODE_LOCAL,
        "local GameMode must implement both Gravitation quake messages")
print("effect 522 Gravitation and quake receiver source checks passed")
