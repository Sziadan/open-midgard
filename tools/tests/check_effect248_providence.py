"""Compare effect248 Providence caller, emitter, STR tail and local fixture."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
PRIMARY_DISPATCH = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
SRC = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
DISPATCH = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
FIXTURE = (ROOT / "tools/tests/EffectLifecycleTests.cpp").read_text(encoding="utf-8")


def body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start : pos + 1]
    raise AssertionError(f"unterminated body: {signature}")


def named_body(text: str, method: str) -> str:
    matches = list(re.finditer(rf"void\s+(?:__thiscall\s+)?CRagEffect\s*::\s*{method}\s*\(", text))
    if not matches:
        raise SystemExit(f"{method} method is missing")
    match = matches[-1]
    opening = text.index("{", match.start())
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[match.start() : pos + 1]
    raise AssertionError(f"unterminated method: {method}")


pe_process = body(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
if not re.search(r"case 248:\s*CRagEffect::Providence\(this, aEffectRingYell, 0\);\s*goto LABEL_424;", pe_process):
    raise SystemExit("PE effect248 caller mapping or shared-tail branch changed")
primary_process = body(PRIMARY_DISPATCH, "unsigned __int8 __thiscall CRagEffect::OnProcess2(CRagEffect *this)")
if re.search(r"case\s+248\s*:", primary_process) or "Providence(this" in primary_process:
    raise SystemExit("primary Ref unexpectedly gained an effect248 process caller; re-audit the recorded omission")
label = PE[PE.index("LABEL_424:", PE.index("CRagEffect::OnProcess(CRagEffect *this)")):]
label = label[:label.index("LABEL_425:")]
if "ProcessEZ2STR(this, 0.0, -80.0, 0.0)" not in label:
    raise SystemExit("PE effect248 shared tail no longer processes providence.str with offset (0,-80,0)")
if not re.search(r"case 248:\s*sprintf\(StrName, aProvidenceStr\);", PE):
    raise SystemExit("PE Init mapping no longer selects providence.str")
if 'char aProvidenceStr[15] = "providence.str";' not in PE:
    raise SystemExit("PE providence.str bytes/literal changed")

pe = named_body(PE, "Providence")
ref = named_body(REF, "Providence")
secondary = named_body((ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace"), "Providence")
if re.sub(r"\s+", "", pe) != re.sub(r"\s+", "", ref):
    raise SystemExit("PE and primary Ref Providence helper bodies no longer match after whitespace normalization")
for label, source in (("PE", pe), ("primary Ref", ref)):
    flat = re.sub(r"\s+", "", source.replace("this->", ""))
    ordered = ("m_pos=m_master->m_pos", "LaunchEffectPrim", "PP_MAPPILLAR",
               "m_renderFlag=5", "m_duration", "m_texture", "m_size=5.0",
               "process=200", "full_display_angle=360", "max_height=120.0", "RotStart=",
               "alphaB=40", "rise_angle=83-", "alphaT=1", "vecB_now.y=0")
    positions = [flat.find(token) for token in ordered]
    if (any(pos < 0 for pos in positions) or positions != sorted(positions)
            or "if(!m_stateCnt)" not in flat and "if(m_stateCnt==0)" not in flat
            or "if(!F1)" not in flat and "if(F1==0)" not in flat):
        raise SystemExit(f"{label} Providence helper changed source emission order/profile")
    if "0.5+6.0" not in flat and "0.5f+6.0f" not in flat:
        raise SystemExit(f"{label} Providence helper lost four-band rotation/distance profile")
secondary_flat = re.sub(r"\s+", "", secondary)
for marker in ("m_pos=m_master->m_pos", "m_stateCnt==0", "PP_MAPPILLAR", "m_duration",
               "m_size=5", "process=200", "full_display_angle=360", "max_height=120.0f",
               "RotStart=ec*90", "alphaB=40", "distance=6.0f+(ec*0.5f)",
               "rise_angle=83-ec", "alphaT=1", "flag1[i]=0"):
    if marker not in secondary_flat:
        raise SystemExit(f"secondary Ref Providence corroboration changed at {marker!r}")
if "m_GI[v5].height[20]" not in pe or "v6 = v5 * 180 + 964" not in ref or "i < 21" not in ref:
    raise SystemExit("Providence no longer clears each GI height/flag array")
if not re.search(r"case 248:\s*sprintf\(StrName, aProvidenceStr\);", PRIMARY_DISPATCH):
    raise SystemExit("primary Ref Init no longer maps effect248 to providence.str")

local = named_body(SRC, "ApplyProvidence")
for token in ("if (textureName == nullptr || m_stateCnt != 0)", "ResolveCurrentMaster()",
              "kPrimTypeMapPillar", "prim->m_renderFlag = 5;", "prim->m_duration = m_duration;",
              "prim->m_size = 5.0f;", "m_effectPos = origin;", "prim->m_pos = origin;", "gi.process = 200;",
              "gi.fullDisplayAngle = 360;", "gi.maxHeight = 120.0f;", "gi.rotStart",
              "gi.alphaB = 40;", "gi.alphaT = 1;", "gi.height.fill(0.0f);", "gi.flag.fill(0);"):
    if token not in local:
        raise SystemExit(f"local Providence profile changed at {token!r}")
if 'case 248: ApplyProvidence("effect\\\\ring_yellow.tga"); ProcessEZ2STR(); return FinishEffectFrame();' not in DISPATCH:
    raise SystemExit("local effect248 emitter/STR/termination order changed")
if 'ApplyProvidence("effect\\\\ring_yellow.tga");' not in SRC:
    raise SystemExit("local legacy dispatcher Providence profile is missing")

fixture_start = FIXTURE.rfind("CRenderObject providenceMaster")
if fixture_start < 0:
    raise SystemExit("Providence lifecycle fixture is missing")
fixture = FIXTURE[fixture_start:]
for phrase in ("Providence effect248 emits exact four-band MAPPILLAR profile",
               "Providence state1 follows moved master without repeating emission",
               "Providence exhausted STR completion is propagated after frame-zero emission"):
    if phrase not in fixture:
        raise SystemExit(f"Providence lifecycle assertion missing: {phrase}")

print("effect248 PE/primary-Ref Providence helper, STR route and local lifecycle evidence passed")
