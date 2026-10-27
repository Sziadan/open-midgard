from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
ref_h = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
ref = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp949", errors="replace")
ref2 = (ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="cp949", errors="replace")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="cp949", errors="replace")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")


def require(ok, message):
    if not ok:
        raise AssertionError(message)


def between(text, start, end):
    a = text.index(start)
    b = text.index(end, a + len(start))
    return text[a:b]


enum_body = between(ref_h, "enum {", "};")
tokens = re.findall(r"\bEF_[A-Z0-9_]+\b", re.sub(r"//.*", "", enum_body))
anchor = tokens.index("EF_TAROTCARD2")
base = 524 - anchor
ids = {name: base + tokens.index(name) for name in ("EF_CHOOKGI", "EF_CHOOKGI2", "EF_CHOOKGI3")}
require(ids == {"EF_CHOOKGI": 228, "EF_CHOOKGI2": 504, "EF_CHOOKGI3": 629}, f"enum values derived from EF_TAROTCARD2=524: {ids}")
print(f"enum token-position comparison: {ids}")

read_dispatch = between(ref, "case\tEF_CHOOKGI3:", "case\tEF_BASH3D5:")
read_main = between(ref, "case\tEF_CHOOKGI:", "case\tEF_LINELINK:")
exe_case0 = re.search(r"case 228:\s*CRagEffect::CHOOKGI\(this, 0\);", exe)
exe_case1 = re.search(r"case 0x1F8:\s*CRagEffect::CHOOKGI\(this, 1\);", exe)
exe_case3 = re.search(r"case 0x275:\s*CRagEffect::CHOOKGI\(this, 3\);", exe)
require("CHOOKGI(3)" in read_dispatch and "CHOOKGI(1)" in between(ref, "case\tEF_CHOOKGI2:", "case\tEF_SLIM:"), "readable variant calls")
require(re.search(r"case\s+EF_CHOOKGI:\s*CHOOKGI\(\);", ref) is not None and exe_case0 is not None, "variant-zero caller mapping")
require(exe_case1 is not None and exe_case3 is not None, "canonical variants one and three")
require(all(x in src for x in ("m_type == 228", "m_type == 504", "m_type == 629", "SpawnChookgi(m_type == 228 ? 0 : (m_type == 504 ? 1 : 3))")),
        "current caller ID-to-variant routing")
print("dispatcher comparison: Ref/canonical/current IDs 228->0, 504->1, 629->3")

read = between(ref2, "void\tCRagEffect :: CHOOKGI(char F1)", "bool\tCEffectPrim :: PrimCHOOKGI()")
canonical = between(exe, "void __thiscall CRagEffect::CHOOKGI(CRagEffect *this, char F1)\n{", "//----- (006390A0)")
current = between(src, "void CRagEffect::SpawnChookgi(char variant)", "void CRagEffect::ApplyBottomMagnus(")
for label, body in (("readable", read), ("canonical", canonical), ("current", current)):
    require("m_stateCnt == 0" in body or "!this->m_stateCnt" in body or "m_stateCnt != 0" in body, f"{label} state-zero gate")
    require("m_deltaPos.x" in body, f"{label} count source")
    require("> 5" in body or "> 5.0" in body, f"{label} five-child cap")
    require("PP_CHOOKGI" in body and "m_duration" in body, f"{label} primitive type/duration")
    require("thunder_center.bmp" in body or "aEffectThunderC" in body, f"{label} exact texture")

require("m_pos = m_master->m_pos" in read and "this->m_pos = this->m_master->m_pos" in canonical
        and "m_effectPos = master->m_pos" in current, "per-call parent-origin refresh")
require("maxnum = m_deltaPos.x" in read and "for (i=0;i<maxnum;i++)" in read
        and "maxCount = static_cast<int>(m_deltaPos.x)" in current
        and "index < maxCount" in current, "source truncation and child loop")
require("(int)x > 0" in canonical and "while ( v4 < maxnum )" in canonical, "canonical positive-count loop")

require(read.index("F1 == 1") < read.index("F1 == 3") < read.index("else\n\t\t\t{"), "readable variant branch order")
require(current.index("variant == 1") < current.index("variant == 3") < current.index("} else {"), "current variant branch order")
require("m_deltaPos.x <= 5" in read and "m_deltaPos.x > 5" in read and "m_deltaPos.x - i >= 6" in read,
        "readable variant-three spacing and distance guards")
require("m_deltaPos.x > 5.0" in canonical and "m_deltaPos.x - (double)i >= 6.0" in canonical,
        "canonical variant-three spacing and distance guards")
require("m_deltaPos.x <= 5.0f" in current and "m_deltaPos.x > 5.0f" in current
        and "m_deltaPos.x - static_cast<float>(index) >= 6.0f" in current,
        "current variant-three spacing and distance guards")

for label, body, rng in (("readable", read, r"random\(360\)"), ("canonical", canonical, r"rand\(\)\s*%\s*360"), ("current", current, r"rand\(\)\s*%\s*360")):
    require(len(re.findall(rng, body)) == 7, f"{label} has seven ordered per-child random sites")
require(read.index("height[5]") < read.index("F1 == 1") < read.index("height[8]"), "readable random draws bracket variant branch")
require(canonical.index("height[5]") < canonical.index("F1 == 1") < canonical.index("height[8]"), "canonical random draws bracket variant branch")
require(current.index("height[5]") < current.index("variant == 1") < current.index("height[8]"), "current random draws bracket variant branch")
require("alphaB=0" in read and "process=0" in read and "flag1[0]=1" in read
        and "alphaB = 0" in current and "process = 0" in current and "flag[0] = 1" in current,
        "common alpha/process and variant-three flag initialization")
require("height[7]=-14.0f" in read and "distance = 1.3f" in current and "height[10] = 1.0f" in current,
        "variant-one exact fields")
require("height[7]=-15.0f" in read and "distance = 2.0f" in current and "height[10] = 0.0f" in current,
        "default exact fields")
require("height[6]=i*72" in read and "height[7]=-15.0f" in read and "flag1[0]=0" in read
        and "height[6] = static_cast<float>(index * 72)" in current and "gi.flag[0] = 0" in current,
        "default angle, vertical offset and flag")
require("height[6]=i*72" in read and "height[7]=-14.0f" in read and "height[10]=1" in read
        and "gi.height[6] = static_cast<float>(index * 72)" in current and "gi.flag[0] = 0" in current,
        "variant-one angle, vertical offset and flag")
require(re.search(r"height\[6\]\s*=\s*i\s*\*\s*360\s*/\s*m_deltaPos\.x", read)
        and "index * 360 / m_deltaPos.x" in current
        and "height[7]=-15.0f" in read and "flag1[0]=1" in read and "gi.flag[0] = 1" in current,
        "variant-three angle cadence, offset and flag")
print("emitter branch comparison: state gate/count cap, branch order, mode guards, fields, texture, seven RNG sites and common tail matched")
print("CHOOKGI source-to-implementation comparison passed")




