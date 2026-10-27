from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def function_body(text: str, signature: str) -> str:
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise AssertionError(f"unterminated function {signature}")


def ordered(body: str, patterns: list[str], label: str) -> None:
    cursor = 0
    for pattern in patterns:
        match = re.search(pattern, body[cursor:], re.S)
        assert match, f"{label}: missing/out-of-order branch token {pattern}"
        cursor = cursor + match.end()


source = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
dispatcher = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp949", errors="replace")
canonical = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
implementation = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

assert "EF_KOUENKA" in (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
enum_text = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
seed = re.search(r"EF_COOKING_FAIL\s*,\s*//\s*609(?P<tail>.*?EF_KOUENKA)", enum_text, re.S)
assert seed, "could not locate the readable effect enum seed for Kouenka"
enum_ids = re.findall(r"\bEF_[A-Z0-9_]+\b", seed.group("tail"))
assert 609 + len(enum_ids) == 618, f"readable enum resolves Kouenka to {609 + len(enum_ids)}, not current ID 618"
assert re.search(r"case\s+0x26A\s*:", (ROOT / "Ref/RagEffect.cpp").read_text(encoding="cp949", errors="replace"))
assert re.search(r"case\s+0x26A\s*:", canonical)
assert re.search(r"if\s*\(m_type\s*==\s*618\)\s*\{\s*ApplyKouenka\(\);", implementation)

kouenka = function_body(source, "void\tCRagEffect :: Kouenka()")
ordered(kouenka, [
    r"m_pos\s*=\s*m_master->m_pos",
    r"m_stateCnt\s*>=\s*12\s*&&\s*m_stateCnt\s*<=\s*15",
    r"random\(3\)\s*\+\s*1",
    r"m_stateCnt\s*<\s*20",
    r"LaunchEffectPrim\(PP_KOUENKA\)",
    r"random\(3\)\s*\+\s*2",
    r"random\(6\)\s*\+\s*22",
    r"ta\s*=\s*random\(360\)",
    r"height\[0\]\s*=\s*random\(360\)",
    r"height\[1\]\s*=\s*random\(360\)",
    r"height\[2\]\s*=\s*random\(360\)",
    r"height\[3\]\s*=\s*random\(360\)",
    r"SetSprName\(\"misc\\\\sakura01\.spr\"\)",
    r"SetActName\(\"misc\\\\sakura01\.act\"\)",
    r"SetAction\(random\(3\),\s*36,\s*MT_LOOP\)",
    r"InitSegment\(\)",
], "readable Kouenka")

current = function_body(implementation, "void CRagEffect::ApplyKouenka()")
ordered(current, [
    r"m_effectPos\s*=\s*master->m_pos",
    r"m_stateCnt\s*>=\s*12\s*&&\s*m_stateCnt\s*<=\s*15",
    r"rand\(\)\s*%\s*3\s*\+\s*1",
    r"m_stateCnt\s*<\s*20",
    r"LaunchEffectPrim\(PP_KOUENKA",
    r"rand\(\)\s*%\s*3\s*\+\s*2",
    r"rand\(\)\s*%\s*6\s*\+\s*22",
    r"startAngle\s*=\s*rand\(\)\s*%\s*360",
    r"height\[static_cast<size_t>\(index\)\]\s*=\s*static_cast<float>\(rand\(\)\s*%\s*360\)",
    r"SetParticleResources\(\"misc\\\\sakura01\.spr\",\s*\"misc\\\\sakura01\.act\"\)",
    r"m_spriteAction\s*=\s*rand\(\)\s*%\s*3",
    r"m_numSegments\s*=\s*1",
    r"InitReferenceSegments\(prim\)",
], "current Kouenka")

dispatch = function_body(dispatcher, "void\tCRagEffect :: ProcessEffectFrame()") if "void\tCRagEffect :: ProcessEffectFrame()" in dispatcher else dispatcher
ordered(dispatch, [r"Kouenka\(\)", r"for\s*\(int i=0;i<2;i\+\+\)", r"BASH3D\(\"effect\\\\alpha_center\.tga\",i,2\)"], "readable effect dispatcher")
canonical_kouenka = canonical[canonical.index("case 0x26A:"):canonical.index("case 0x26B:")]
ordered(canonical_kouenka, [r"CRagEffect::Kouenka\(this\)", r"CRagEffect::BASH3D\(this,.*?,\s*i,\s*2\)"], "canonical effect dispatcher")

assert len(re.findall(r"rand\(\)", current)) == 6, "caller should have one audio, five petal call sites (one in a four-iteration loop)"
assert current.count('SpawnBash3D("effect\\\\alpha_center.tga"') == 4, "both BASH3D call sites must cover valid and missing-master paths"
print("EF_KOUENKA: source/canonical/current IDs, ordered branches, 8 petal RNG draws, and 2 BASH3D calls match")
