"""Compare MakeBlur/MakeBlur2 callers and CBlurPC snapshot lifecycle anchors."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.rindex(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(signature)


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
ref_effect = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
ref_effect2 = (ROOT / "Ref/RagEffect2.cpp").read_text(encoding="latin-1")
ref_npc = (ROOT / "Ref/Npc.cpp").read_text(encoding="latin-1")
local_effect = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
local_actor = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
local_header = (ROOT / "src/world/GameActor.h").read_text(encoding="utf-8")
effect_header = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")
dispatch = (ROOT / "src/world/RagEffectOnProcessCases.generated.inc").read_text(encoding="utf-8")
dispatch3 = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
world = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")

for name, source in (("MakeBlur", ref_effect), ("MakeBlur2", ref_effect2)):
    signature = f"void __thiscall CRagEffect::{name}(CRagEffect *this)"
    if compact(function(pe, signature)) != compact(function(source, signature)):
        raise SystemExit(f"Ref/PE helper mismatch: {name}")

for name, signature in (
    ("CBlurPC constructor", "CBlurPC *__thiscall CBlurPC::CBlurPC(CBlurPC *this, CPc *pc)"),
    ("CBlurPC process", "unsigned __int8 __thiscall CBlurPC::OnProcess(CBlurPC *this)"),
    ("CBlurPC render", "void __thiscall CBlurPC::Render(CBlurPC *this, matrix *ptm)"),
    ("CBlurPC message", "void __thiscall CBlurPC::SendMsg(CBlurPC *this, CGameObject *sender, int message, float arg1, int arg2, int arg3)"),
):
    if compact(function(pe, signature)) != compact(function(ref_npc, signature)):
        raise SystemExit(f"Ref/PE {name} mismatch")

pe_main = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess(CRagEffect *this)")
branch166 = pe_main[pe_main.index("case 166:"):pe_main.index("case 170:")]
if "MakeBlur(this)" not in branch166:
    raise SystemExit("canonical effect166 caller mismatch")
pe3 = function(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
branch617 = pe3[pe3.index("case 0x269:"):pe3.index("case 0x26A:")]
if "MakeBlur2(this)" not in branch617:
    raise SystemExit("canonical effect617 caller mismatch")

for type_id, method in ((166, "ProcessMakeBlur"), (617, "ProcessMakeBlur2")):
    case_table = dispatch if type_id == 166 else dispatch3
    if f"case {type_id}: return {method}();" not in case_table:
        raise SystemExit(f"effect{type_id} route missing")
    if f"bool {method}();" not in effect_header:
        raise SystemExit(f"effect{type_id} declaration missing")

make_blur = function(local_effect, "bool CRagEffect::ProcessMakeBlur()")
for token in ("master->m_bodyPalette", "master->m_oldColor.a", "m_stateCnt == 9998",
              "m_effectPos.x == -1.0f", "m_effectPos.x == -2.0f", "rand() % 14",
              "m_duration != 30", "m_stateCnt % 5", "AddBlurSnapshot"):
    if token not in make_blur:
        raise SystemExit(f"effect166 local branch missing {token}")
if make_blur.index("oldColorAlpha") > make_blur.index("m_effectPos.x == -1.0f"):
    raise SystemExit("effect166 palette/old-color branches are out of order")

make_blur2 = function(local_effect, "bool CRagEffect::ProcessMakeBlur2()")
for token in ("155.0f, 155.0f, 255.0f", "SetArgb(-1, 155, 155, 255)",
              "m_stateCnt == 9998", "m_stateCnt % 20", "AddBlurSnapshot(master, this, 155, 155, 255)"):
    if token not in make_blur2:
        raise SystemExit(f"effect617 local branch missing {token}")

constructor = function(local_actor, "CBlurPC::CBlurPC(CPc* source)")
for field in ("m_pos", "m_lastPixelRatio", "m_sprShift", "m_zoom", "m_baseAction", "m_curAction",
              "m_curMotion", "m_bodyPalette", "m_headPalette", "m_effectState", "m_headPaletteName",
              "m_imfName", "m_gid", "m_gdid", "m_sex", "m_job"):
    if f"{field} = source->{field}" not in constructor:
        raise SystemExit(f"CBlurPC snapshot field missing: {field}")

onprocess = function(local_actor, "u8 CBlurPC::OnProcess()")
if "m_u28.m_sprArgb < m_alphaDelta" not in onprocess or "m_u28.m_sprArgb -= m_alphaDelta" not in onprocess:
    raise SystemExit("CBlurPC fade lifecycle mismatch")
sendmsg = function(local_actor, "void CBlurPC::SendMsg(")
for token in ("case 0:", "m_alphaDelta = 0x08000000u", "case 1:", "case 2:", "case 3:", "CPc::SendMsg"):
    if token not in sendmsg:
        raise SystemExit(f"CBlurPC message branch missing {token}")
if "dynamic_cast<CBlurPC*>(object)" not in world or "blur->m_renderEligible" not in world:
    raise SystemExit("CBlurPC world snapshot render lane missing")

print("MakeBlur/MakeBlur2 parity: Ref/PE helpers, caller ids, color/cadence branches, snapshot fade/messages and world render lane passed")
