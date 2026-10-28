from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


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
receiver = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")

pe_dispatch = body(pe, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
ref_dispatch = body(ref, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
for label, source in (("PE", pe_dispatch), ("Ref", ref_dispatch)):
    begin = source.index("case 0x25D:")
    end = source.index("case 0x25E:", begin)
    case = source[begin:end]
    assert "if ( this->m_stateCnt )" in case and "return 1;" in case
    assert "this->m_master->CGameObject::SendMsg(this->m_master, 0, 109, 604, 0, 0);" in case

handler = body(local, "bool CRagEffect::ProcessEffect605()")
assert "m_stateCnt == 0" in handler and "m_master->SendMsg(nullptr, 109, 604, 0, 0)" in handler
assert "return true;" in handler and "LaunchEffect" not in handler and "PlayWave" not in handler
assert "case 605: return ProcessEffect605();" in routes
assert '605: "return ProcessEffect605();"' in generator
sendmsg = body(receiver, "void CAbleToMakeEffect::SendMsg(CGameObject* sender, int msg")
assert "case 109:" in sendmsg and "RemoveEffectById(static_cast<int>(par1))" in sendmsg
print("effect605 state gate, exact message tuple, receiver and route checks passed")
