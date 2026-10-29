#!/usr/bin/env python3
"""Compare effect493 EnchantPoison2 caller, particle cadence and resources."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402

PE = (ROOT / "HighPriest.exe.c").read_text(encoding="latin-1")
REF = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin-1")
LOCAL = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ROUTES = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
HEADER = (ROOT / "src/world/RagEffect.h").read_text(encoding="utf-8")


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
                return source[opening + 1:pos]
    raise AssertionError(f"unterminated function {signature}")


def compact(text: str) -> str:
    return re.sub(r"\s+", "", text)


pe_helper = function(PE, "void __thiscall CRagEffect::EnchantPoison2(CRagEffect *this)")
ref_helper = function(REF, "void __thiscall CRagEffect::EnchantPoison2(CRagEffect *this)")
if compact(pe_helper) != compact(ref_helper):
    raise AssertionError("PE/primary-Ref EnchantPoison2 helper bodies differ")

pe_onprocess3 = function(PE, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_onprocess3 = function(REF, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
def effect493_branch(body: str) -> str:
    start = body.index("case 0x1ED:")
    end = body.index("case 0x1EE:", start)
    return body[start:end]


pe_branch = effect493_branch(pe_onprocess3)
ref_branch = effect493_branch(ref_onprocess3)
if compact(pe_branch) != compact(ref_branch):
    raise AssertionError("PE/primary-Ref effect493 caller branches differ")
for token in ("EnchantPoison2(this)", "m_stateCnt >= 81", "m_stateCnt & 0x80000001",
              "m_BodyLight & 1", "SetArgb(this->m_master, -1, 255, 0, 255)"):
    if token not in pe_branch:
        raise AssertionError(f"effect493 PE caller behavior missing {token}")

if "case 493: ProcessEnchantPoison2(); return true;" not in ROUTES:
    raise AssertionError("local effect493 route changed")
if "void ProcessEnchantPoison2();" not in HEADER or "void SpawnEnchantPoison2();" not in HEADER:
    raise AssertionError("local effect493 function declarations missing")
spawn = function(LOCAL, "void CRagEffect::SpawnEnchantPoison2()")
for token in ("m_stateCnt == 0", "m_stateCnt == 5", "m_stateCnt == 11", "m_stateCnt == 18",
              "m_stateCnt == 26", "m_stateCnt == 40", '"effect\\\\assasin_enchantpoison.wav"',
              "m_effectPos.x - listener.x", "m_effectPos.z - listener.z", "m_stateCnt % 3 == 0",
              "LaunchEffectPrim(PP_3DPARTICLE, vector3d{})", "ConfigureEnchantPoisonParticle(prim, 50, 20, 20)"):
    if token not in spawn:
        raise AssertionError(f"local EnchantPoison2 emitter behavior missing {token}")

process = function(LOCAL, "void CRagEffect::ProcessEnchantPoison2()")
for token in ("SpawnEnchantPoison2()", "m_stateCnt >= 81", "ResolveCurrentMaster()",
              "m_stateCnt % 2 != 0", "master->SetArgb(-1, 255, 255, 255)",
              "master->m_BodyLight & 1", "master->SetArgb(-1, 255, 0, 255)"):
    if token not in process:
        raise AssertionError(f"local EnchantPoison2 frame/tint behavior missing {token}")

image = ExeImage(ROOT / "HighPriest.exe")
for va, expected in ((0x71BC58, b"effect\\assasin_enchantpoison.wav"),
                     (0x71BC24, b"\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.spr"),
                     (0x71BC0C, b"\xC0\xCC\xC6\xD1\xC6\xAE\\particle3.act")):
    actual = image.read_cstring_at(va)
    if actual is None or actual.raw != expected:
        raise AssertionError(f"effect493 resource differs at PE VA 0x{va:X}")

print("effect493 EnchantPoison2 parity passed: PE/Ref helper and caller, local cadence/tint and exact resources")
