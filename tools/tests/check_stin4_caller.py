from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def body(text: str, marker: str, next_marker: str) -> str:
    start = text.index(marker)
    end = text.index(next_marker, start + len(marker))
    return text[start:end]


def ordered(text: str, fragments: list[str]) -> bool:
    cursor = 0
    for fragment in fragments:
        found = text.find(fragment, cursor)
        if found < 0:
            return False
        cursor = found + len(fragment)
    return True


source_enum = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="latin1")
enum = body(source_enum, "EF_TEMP_OK,", "};")
enum_names = [x.strip() for x in re.split(r"[,\n]", enum) if x.strip()]
case_id = 610
for name in enum_names:
    if name == "EF_STIN4":
        break
    case_id += 1
else:
    raise AssertionError("EF_STIN4 missing from readable enum")

source_dispatch = body((ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin1"),
                       "EF_STIN4:", "EF_THROWITEM6:")
canonical = body((ROOT / "HighPriest.exe.c").read_text(encoding="latin1"),
                 "case 0x26D:", "case 0x26E:")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
current_dispatch = body(current, "if (m_type == 621) {", "if (m_type == 620)")
caller = body(current, "void CRagEffect::ApplyStin4()", "void CRagEffect::SpawnStin4(")
emitter = body(current, "void CRagEffect::SpawnStin4(", "void CRagEffect::ApplyBottomMagnus(")
source_emitter = body((ROOT / "Ref/dhxj/RagEffect2.cpp").read_text(encoding="latin1"),
                      "void\tCRagEffect :: STIN4(", "void\tCRagEffect :: SMA(")
canonical_emitter = body((ROOT / "HighPriest.exe.c").read_text(encoding="latin1"),
                          "void __thiscall CRagEffect::STIN4(CRagEffect *this, char *tName, char F1)\n{",
                          "void __thiscall CRagEffect::SMA(CRagEffect *this, char F1)\n{")
canonical_dispatch = canonical

assert case_id == 621, f"readable enum resolves EF_STIN4={case_id}, expected 621"
assert re.search(r"case\s+0x26D", (ROOT / "HighPriest.exe.c").read_text(encoding="latin1"))
assert ordered(source_dispatch, ["if (m_stateCnt == 0)", "m_pos = m_master->m_pos",
                                 'PlayWave("effect\\\\stin4.wav"',
                                 "for (int i=0;i<m_count;i++)", 'STIN4("effect\\\\wind.tga",i%2)'])
assert ordered(canonical_dispatch, ["if ( !this->m_stateCnt )", "this->m_pos = this->m_master->m_pos",
                                     "PlayWave(aEffect_21", "if ( this->m_count <= 0 )",
                                     "CRagEffect::STIN4(this, aEffect270, v35 % 2)",
                                     "while ( v35 < this->m_count )"])
assert ordered(current_dispatch, ["ApplyStin4();", "return true;"])
assert ordered(caller, ["if (m_stateCnt == 0)", 'PlayWave("effect\\\\stin4.wav"',
                       "for (int index = 0; index < m_count; ++index)",
                       'SpawnStin4("effect\\\\wind.tga", static_cast<char>(index % 2))'])
assert ordered(source_emitter, ["m_pos = m_master->m_pos", "if (m_stateCnt  == 0)",
                                "LaunchEffectPrim(PP_STIN4)", "RF_EFFECT", "m_duration",
                                "m_totalTexture = 1", "GetTexture(tName)", "m_size=0",
                                "m_deltaPos = m_deltaPos", "m_pos = m_pos",
                                "for (ec=0;ec<4;ec++)", "life = 1", "process= 0",
                                "random(11)*0.5f", "alphaB = 0", "vecB_now.y = m_pos.y-10.0f",
                                "RotStart=0", "rise_angle=ec", "random(21)*0.1f",
                                "flag1[0] = 0", "flag1[1] = 0", "if ( F1 == 0 )",
                                "else if ( F1 == 1)", "flag1[3] = 1"])
assert ordered(canonical_emitter, ["this->m_pos = this->m_master->m_pos", "if ( !this->m_stateCnt )",
                                   "LaunchEffectPrim(this, PP_STIN4", "m_renderFlag = 5",
                                   "m_duration = this->m_duration", "m_totalTexture = 1",
                                   "GetTexture(&g_texMgr, tName, 0)", "m_size = 0.0",
                                   "m_deltaPos = this->m_deltaPos", "m_pos = this->m_pos",
                                   "while ( v5 < 4 )"])
for branch in ["life = 1", "process = 0", "rand() % 11", "alphaB = 0",
               "vecB_now.y = this->m_pos.y - 10.0", "RotStart = 0", "rise_angle = ec",
               "rand() % 21", "height[1]", "flag1[0] = 0", "flag1[1] = 0",
               "if ( F1 )", "if ( F1 == 1 )", "flag1[2] = 0", "flag1[3] = 1"]:
    assert branch in canonical_emitter, f"canonical emitter branch missing: {branch}"
assert ordered(emitter, ["m_effectPos = master->m_pos", "if (m_stateCnt != 0) return",
                         "LaunchEffectPrim(PP_STIN4", "prim->m_renderFlag = 5",
                         "prim->m_duration = m_duration", "prim->m_totalTexture = 1",
                         "GetTexture(textureName", "prim->m_size = 0.0f",
                         "prim->m_deltaPos = m_deltaPos", "prim->m_pos = m_effectPos",
                         "lane < 4", "gi.life = 1", "gi.process = 0", "rand() % 11",
                         "gi.alphaB = 0", "gi.vecBPreX =", "gi.arcBaselineY =", "gi.vecBPreZ =", "gi.rotStart = 0",
                         "gi.riseAngle", "rand() % 21", "gi.flag[0] = 0",
                         "gi.flag[1] = 0", "if (variant == 0)", "else if (variant == 1)",
                         "gi.flag[3] = 1"])
assert emitter.count("rand() %") == 2, "two source RNG draws per lane must remain"
ctor_source = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="latin1")
ctor_current = body(current, "CRagEffect::CRagEffect()", "CRagEffect::~CRagEffect()")
assert "m_count = 1;" in body(ctor_source, "CRagEffect :: CRagEffect()", "CRagEffect :: ~CRagEffect()")
assert "this->m_count = 0;" in body((ROOT / "Ref/RagEffect.cpp").read_text(encoding="latin1"),
                                       "CRagEffect *__thiscall CRagEffect::CRagEffect(CRagEffect *this)\n{",
                                       "void __thiscall CRagEffect::~CRagEffect(CRagEffect *this)\n{")
assert "this->m_count = 0;" in body((ROOT / "HighPriest.exe.c").read_text(encoding="latin1"),
                                      "CRagEffect *__thiscall CRagEffect::CRagEffect(CRagEffect *this)\n{",
                                      "void __thiscall CRagEffect::~CRagEffect(CRagEffect *this)\n{")
assert "m_count(0)" in ctor_current

print("STIN4 source/canonical/current caller and emitter comparisons passed (case 621 / 0x26D; 8 RNG draws per primitive).")
