from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from ro_exe_resolve import ExeImage  # noqa: E402


def body(source: str, signature: str) -> str:
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
    raise AssertionError(f"unterminated function: {signature}")


pe = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
ref = (ROOT / "Ref/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
local = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8", errors="replace")
routes = (ROOT / "src/world/RagEffectOnProcess3Cases.generated.inc").read_text(encoding="utf-8")
generator = (ROOT / "tools/generate_effect_dispatch.py").read_text(encoding="utf-8")

for label, source in (("PE", pe), ("Ref", ref)):
    dispatch = body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    for effect_id, case_id, color in ((652, "0x28C", "-65536"), (653, "0x28D", "-65536"), (654, "0x28E", "-65281")):
        begin = dispatch.index(f"case {case_id}:")
        end = dispatch.index("case ", begin + 5)
        case = dispatch[begin:end]
        assert "if ( this->m_stateCnt )" in case and "return 1;" in case
        assert f"SendMsg(this->m_master, 0, 88, 1, {color}, 114)" in case, f"{label} effect{effect_id} send tuple mismatch"
        if effect_id == 652:
            assert "PlayWave(aEffectEfHit2Wa" in case and "this->m_pos = this->m_master->m_pos" in case

handler = body(local, "bool CRagEffect::ProcessDamageNumberEffects()")
assert "m_stateCnt != 0" in handler and 'm_type == 652' in handler and 'effect\\\\EF_hit2.wav' in handler
assert "m_effectPos = m_master->m_pos" in handler and "m_cachedPos = m_effectPos" in handler
assert "m_master->SendMsg(nullptr, 88, 1, color, 114)" in handler
assert "m_type == 654 ? -65281 : -65536" in handler
for effect_id in (652, 653, 654):
    assert f"case {effect_id}: return ProcessDamageNumberEffects();" in routes
    assert f'{effect_id}: "return ProcessDamageNumberEffects();"' in generator
receiver = body((ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8", errors="replace"),
                "void CGameActor::SendMsg(CGameObject* src, int msg")
assert "case 88:" in receiver and "MakeDamageNumber(*this, sourceActor, static_cast<int>(par1), static_cast<u32>(par2), static_cast<int>(par3))" in receiver
wave = ExeImage().read_cstring_at(0x719BC8)
assert wave is not None and wave.raw == b"effect\\EF_hit2.wav"
print("effects652-654 state gates, exact wave/message tuples, receiver and routes passed")
