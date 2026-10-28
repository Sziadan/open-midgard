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

for label, source in (("PE", pe), ("Ref", ref)):
    dispatch = body(source, "unsigned __int8 __thiscall CRagEffect::OnProcess3(CRagEffect *this)")
    start = dispatch.index("case 0x233:")
    end = dispatch.index("case 0x234:", start)
    case = dispatch[start:end]
    assert "v97 < 100" in case and "v97 % 5" in case
    assert "LABEL_346" in case
    shared = dispatch[dispatch.index("LABEL_346:"):dispatch.index("case 0x29B:")]
    assert "SendMsg(v98, 135, 0, 0, 0)" in shared, f"{label} shared label message missing"

handler = body(local, "bool CRagEffect::ProcessEffect563()")
assert "m_stateCnt < 100 && m_stateCnt % 5 == 0" in handler
assert "SendMsg(135, 0, 0, 0)" in handler
assert "case 563: return ProcessEffect563();" in routes
assert '563: "return ProcessEffect563();"' in generator
print("effect563 periodic state guard, shared message target and local route checks passed")
