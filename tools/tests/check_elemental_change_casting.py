import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ref_cpp = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp1252")
ref_h = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp1252")
exe = (ROOT / "HighPriest.exe.c").read_text(encoding="cp1252")
src = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")

expected = {
    172: ("ChangeDark", "effect\\ring_black.tga", "aEffectRingBlac"),
    173: ("ChangeFire", "effect\\ring_red.tga", "aEffectRingRedT"),
    174: ("ChangeCold", "effect\\ring_blue.tga", "aEffectRingBlue"),
    175: ("ChangeWind", "effect\\ring_yellow.tga", "aEffectRingYell"),
    176: ("ChangeFlame", "effect\\ring_jadu.tga", "aEffectRingJadu"),
    177: ("ChangeEarth", "effect\\ring_brown.tga", "aEffectRingBrow"),
    178: ("ChangeHoly", "effect\\ring_white.tga", "aEffectRingWhit"),
    179: ("ChangePoison", "effect\\ring_purple.tga", "aEffectRingPurp"),
}

# Read the enum sequence directly instead of assuming source declaration order.
enum_tail = ref_h[ref_h.index("EF_CHANGEDARK"):ref_h.index("EF_HITDARK")]
ref_enum = re.findall(r"EF_CHANGE[A-Z]+|EF_CHAINGEHOLY", enum_tail)
assert ref_enum == ["EF_CHANGEDARK", "EF_CHANGEFIRE", "EF_CHANGECOLD", "EF_CHANGEWIND", "EF_CHANGEFLAME", "EF_CHANGEEARTH", "EF_CHAINGEHOLY", "EF_CHANGEPOISON"]
assert list(expected) == list(range(172, 180))


def function(text, marker):
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{": depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(f"unterminated {marker}")


def ref_call(name):
    body = function(ref_cpp, f"CRagEffect :: {name}()")
    assert "if (m_stateCnt == 0)" in body, f"{name} missing zero-state gate"
    match = re.search(r'BeginCasting\("([^"]+)",\s*3\)', body)
    assert match, f"{name} missing option-three BeginCasting"
    return match.group(1).replace("\\\\", "\\")

ref_map = {effect_id: (ref_call(name), 3) for effect_id, (name, _, _) in expected.items()}
assert ref_map == {effect_id: (texture, 3) for effect_id, (_, texture, _) in expected.items()}

# Mechanically read canonical case labels and each BeginCasting resource symbol.
exe_start = exe.index("          case 172:", exe.index("CRagEffect::VenomDust2(this)"))
exe_end = exe.index("          case 180:", exe_start)
exe_group = exe[exe_start:exe_end]
exe_labels = [int(x) for x in re.findall(r"case (17[2-9]):", exe_group)]
assert exe_labels == list(range(172, 180))
exe_map = {}
for match in re.finditer(r"case (17[2-9]):(.*?)(?=case 17[2-9]:|$)", exe_group, re.S):
    effect_id = int(match.group(1))
    body = match.group(2)
    call = re.search(r"BeginCasting\(this,\s*(a\w+),\s*(\d+)\)", body)
    gate = re.search(r"if \( !this->m_stateCnt \)", body)
    assert gate and call, f"canonical case {effect_id} missing zero-state BeginCasting path"
    exe_map[effect_id] = (call.group(1), int(call.group(2)))
assert exe_map == {effect_id: (symbol, 3) for effect_id, (_, _, symbol) in expected.items()}

impl = function(src, "void CRagEffect::ApplyElementalChangeCasting()")
impl_labels = [int(x) for x in re.findall(r"case (17[2-9]):", impl)]
assert impl_labels == list(range(172, 180))
impl_map = {}
for match in re.finditer(r'case (17[2-9]):\s*ApplyBeginCasting\("([^"]+)",\s*(\d+),\s*(true|false)\)', impl):
    impl_map[int(match.group(1))] = (match.group(2).replace("\\\\", "\\"), int(match.group(3)), match.group(4))
assert impl_map == {effect_id: (texture, option, "false") for effect_id, (texture, option) in ref_map.items()}
assert "if (m_stateCnt != 0) return;" in impl
route = function(src, "bool CRagEffect::ProcessEffectFrame()")
assert "const bool beginCast = dispatchHandler && (m_type >= 172 && m_type <= 179);" in route
assert "} else if (beginCast) {\n        ApplyElementalChangeCasting();" in route
assert "PlayWave" not in impl and "rand()" not in impl and "SendMsg" not in impl
print("PASS: effect IDs 172-179 mechanically match Ref enum/wrappers, canonical guarded cases, and current zero-state texture/option routes")



