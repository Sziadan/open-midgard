from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def body(text: str, signature: str) -> str:
    start = text.rindex(signature)
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise AssertionError(f"unterminated {signature}")


def ordered(text: str, patterns: list[str], label: str) -> None:
    position = 0
    for pattern in patterns:
        match = re.search(pattern, text[position:], re.S)
        assert match, f"{label}: expected ordered branch item {pattern}"
        position = position + match.end()


readable = (ROOT / "Ref/dhxj/RagEffectPrim.cpp").read_text(encoding="cp949", errors="replace")
canonical_text = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
current_text = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
ref = body(readable, "void\tCEffectPrim :: ProcessAlpha()")
exe = body(canonical_text, "void __thiscall CEffectPrim::ProcessAlpha(CEffectPrim *this)")
current = body(current_text, "void ProcessReferenceParticleAlpha(CEffectPrim* prim)")

# Branch ledger: each source operation is paired with its current spelling.
ordered(ref, [
    r"if\s*\(m_AlphaShow\)",
    r"m_chCnt\s*<\s*0\s*\|\|\s*m_chCnt\s*>\s*m_chPoint\.size\(\)\s*-\s*1",
    r"m_stateCnt\s*==\s*m_chPoint\[m_chCnt\]",
    r"m_alpha\s*=\s*m_chVal1\[m_chCnt\]",
    r"m_maxAlpha\s*=\s*m_chVal2\[m_chCnt\]",
    r"m_chCnt\+\+",
    r"if\s*\(m_pattern\s*&\s*PT_TWINKLE\)",
    r"if\s*\(m_alphaSpeed\s*<\s*0\)",
    r"m_alpha\s*<=\s*m_minAlpha",
    r"m_alpha\s*>=\s*m_maxAlpha",
    r"m_isDisappear\s*==\s*FALSE",
    r"m_duration\s*-\s*m_stateCnt",
    r"m_master->m_duration\s*-\s*m_master->m_stateCnt\s*\+\s*1",
    r"m_isDisappear\s*=\s*TRUE",
    r"m_pattern\s*&=\s*~PT_TWINKLE",
    r"m_alphaSpeed\s*=\s*-\(m_alpha\s*/\s*\(m_master->m_duration\s*-\s*m_master->m_stateCnt\)\)",
    r"m_stateCnt\s*==\s*m_fadeOutCnt",
    r"m_alphaSpeed\s*=\s*-\(m_alpha\s*/\s*\(m_duration\s*-\s*m_stateCnt\)\)\s*\*\s*m_alphaDelta",
    r"m_alpha\s*\+=\s*m_alphaSpeed",
    r"m_alpha\s*=\s*\(m_alpha\s*<\s*m_minAlpha\).*?m_alpha\s*=\s*\(m_alpha\s*>\s*m_maxAlpha\)",
    r"PACK32BIT8888\(\(int\)m_alpha,\s*m_red,\s*m_green,\s*m_blue\)",
    r"return",
    r"else\s+if\s*\(m_chSpeed\)",
    r"m_chCnt\s*<\s*0\s*\|\|\s*m_chCnt\s*>\s*m_chPoint\.size\(\)\s*-\s*1",
    r"m_stateCnt\s*==\s*m_chPoint\[m_chCnt\]",
    r"m_alphaSpeed\s*=\s*m_chVal1\[m_chCnt\]",
    r"m_chCnt\+\+",
    r"m_pattern\s*&\s*PT_TWINKLE",
    r"m_isDisappear\s*==\s*false",
    r"m_stateCnt\s*==\s*m_fadeOutCnt",
    r"m_alpha\s*\+=\s*m_alphaSpeed",
    r"PACK32BIT8888\(\(int\)m_alpha,\s*m_red,\s*m_green,\s*m_blue\)",
], "readable ProcessAlpha")

ordered(exe, [
    r"if\s*\(\s*!this->m_AlphaShow\s*\)",
    r"if\s*\(\s*this->m_chSpeed\s*\)",
    r"m_stateCnt\s*==\s*this->m_chPoint\._First\[m_chCnt\]",
    r"this->m_alphaSpeed\s*=\s*v24",
    r"m_pattern\s*&\s*0x80u",
    r"if\s*\(\s*!this->m_isDisappear\s*\)",
    r"this->m_pattern\s*=\s*m_pattern",
    r"v30\s*==\s*this->m_fadeOutCnt",
    r"this->m_alphaSpeed\s*=\s*-\(this->m_alpha\s*/\s*\(double\)\(this->m_duration\s*-\s*v30\)\s*\*\s*this->m_alphaDelta\)",
    r"this->m_alpha\s*=\s*m_minAlpha",
    r"goto\s+LABEL_42",
    r"goto\s+LABEL_43",
    r"v2\s*=\s*this->m_chCnt",
    r"this->m_chCnt\s*=\s*0",
    r"this->m_alpha\s*=\s*this->m_chVal1\._First",
    r"this->m_maxAlpha\s*=\s*v8",
    r"this->m_alphaSpeed\s*=\s*this->m_alphaSpeed\s*\*\s*-1\.0",
    r"this->m_isDisappear\s*=\s*1",
    r"this->m_alphaSpeed\s*=\s*-\(this->m_alpha\s*/\s*\(double\)\(v13->m_duration\s*-\s*v13->m_stateCnt\)\)",
    r"v5\s*==\s*this->m_fadeOutCnt",
    r"this->m_alphaSpeed\s*=\s*-\(this->m_alpha\s*/\s*\(double\)v14\s*\*\s*this->m_alphaDelta\)",
    r"this->m_alpha\s*=\s*m_minAlpha",
    r"this->m_argb\s*=\s*\(unsigned __int8\)this->m_blue",
], "canonical ProcessAlpha")

ordered(current, [
    r"if\s*\(prim->m_AlphaShow\)",
    r"prim->m_chCnt\s*<\s*0\s*\|\|\s*prim->m_chCnt\s*>\s*prim->m_chPoint\.size\(\)\s*-\s*1",
    r"static_cast<double>\(prim->m_stateCnt\)\s*==\s*prim->m_chPoint\[prim->m_chCnt\]",
    r"prim->m_alpha\s*=\s*prim->m_chVal1\[prim->m_chCnt\]",
    r"prim->m_maxAlpha\s*=\s*prim->m_chVal2\[prim->m_chCnt\]",
    r"\+\+prim->m_chCnt",
    r"else\s+if\s*\(prim->m_chSpeed\)",
    r"prim->m_chCnt\s*<\s*0\s*\|\|\s*prim->m_chCnt\s*>\s*prim->m_chPoint\.size\(\)\s*-\s*1",
    r"static_cast<double>\(prim->m_stateCnt\)\s*==\s*prim->m_chPoint\[prim->m_chCnt\]",
    r"prim->m_alphaSpeed\s*=\s*prim->m_chVal1\[prim->m_chCnt\]",
    r"\+\+prim->m_chCnt",
    r"prim->m_pattern\s*&\s*0x80",
    r"prim->m_isDisappear",
    r"prim->m_stateCnt\s*==\s*prim->m_fadeOutCnt",
    r"prim->m_alpha\s*\+=\s*prim->m_alphaSpeed",
    r"prim->m_alpha\s*<\s*prim->m_minAlpha",
    r"prim->m_alpha\s*>\s*prim->m_maxAlpha",
    r"prim->m_argb\s*=\s*PackColor\(static_cast<u8>\(static_cast<long long>\(prim->m_alpha\)\),\s*RGB\(static_cast<u8>\(prim->m_red\),\s*static_cast<u8>\(prim->m_green\),\s*static_cast<u8>\(prim->m_blue\)\)\)",
], "current ProcessReferenceParticleAlpha")

assert "m_red, m_green, m_blue" in ref and "this->m_red" in exe
assert "prim->m_tintColor" not in current
print("ProcessAlpha branch order, key cursor rules, twinkle/fade sequence, clamps, and independent RGB packing match readable Ref and canonical EXE.")
