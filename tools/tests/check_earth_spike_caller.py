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
        assert match, f"{label}: missing/out-of-order branch item {pattern}"
        position += match.end()


header = (ROOT / "Ref/dhxj/RagEffect.h").read_text(encoding="cp949", errors="replace")
readable_dispatch = (ROOT / "Ref/dhxj/RagEffect.cpp").read_text(encoding="cp949", errors="replace")
canonical = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
current = (ROOT / "src/world/RagEffect.cpp").read_text(encoding="utf-8")
mode_source = (ROOT / "Ref/dhxj/GameMode2.cpp").read_text(encoding="cp949", errors="replace")
mode_current = (ROOT / "src/gamemode/GameMode.cpp").read_text(encoding="utf-8")
source = body(readable_dispatch, "void CRagEffect :: EarthSpike(char F1)")
exe = body(canonical, "void __thiscall CRagEffect::EarthSpike(CRagEffect *this, char *F1)")
implementation = body(current, "void CRagEffect::ApplyEarthSpike(char variant)")

earth_seed = re.search(r"EF_WARPZONE\s*,(?P<tail>.*?EF_EARTHSPIKE)", header, re.S)
kouenka_seed = re.search(r"EF_COOKING_FAIL\s*,\s*//\s*609(?P<tail>.*?EF_HYOUSENSOU)", header, re.S)
assert earth_seed and 61 + len(re.findall(r"\bEF_[A-Z0-9_]+\b", earth_seed.group("tail"))) == 79
assert kouenka_seed and 609 + len(re.findall(r"\bEF_[A-Z0-9_]+\b", kouenka_seed.group("tail"))) == 619
assert re.search(r"case\s+EF_EARTHSPIKE\s*:\s*EarthSpike\(\)", readable_dispatch)
assert re.search(r"case\s+EF_HYOUSENSOU\s*:\s*EarthSpike\(2\)", readable_dispatch)
assert re.search(r"case\s+79\s*:\s*CRagEffect::EarthSpike\(this,\s*0\)", canonical)
assert re.search(r"case\s+0x26B\s*:\s*CRagEffect::EarthSpike\(this,\s*\(char \*\)2\)", canonical)
assert re.search(r"if\s*\(m_type\s*==\s*79\s*\|\|\s*m_type\s*==\s*619\)\s*\{\s*ApplyEarthSpike\(m_type\s*==\s*79\s*\?\s*0\s*:\s*2\);", current)
mode_set_info = body(mode_source, "switch (messageId)")
mode_current_send = body(mode_current, "msgresult_t CGameMode::SendMsg(int msg,")
assert re.search(r"case\s+MM_SETQUAKEINFO:[\s\S]*?SetQuakeInfo\(\*v,\s*\*v1,\s*\*v2\)", mode_set_info)
quake_case = mode_current_send[mode_current_send.index("case 135:"):mode_current_send.index("case 179:")]
assert re.search(r"SetQuake\(true,\s*1,\s*1\.0f,\s*0\.24f,\s*0\.2f\)", quake_case)
assert not re.search(r"case\s+148\s*:", mode_current_send), "current MM_SETQUAKEINFO is not separately routed"

ordered(source, [
    r"if\s*\(m_stateCnt\s*==\s*0\)",
    r"if\s*\(F1\s*==\s*0\)", r"wizard_earthspike\.wav", r"stone\.bmp",
    r"else\s+if\s*\(F1\s*==\s*2\)", r"sprintf\(buf,\s*\"effect\\\\EF_IceArrow%d\.wav\",\s*random\(3\)\s*\+\s*1\)", r"ice\.tga",
    r"SendMsg\(MM_QUAKE,\s*1\)", r"side\s*=\s*1\.0f,\s*front\s*=\s*0\.24f,\s*latitude\s*=\s*0\.2f",
    r"SendMsg\(MM_SETQUAKEINFO", r"LaunchEffectPrim\(PP_3DQUADHORN\)", r"m_duration\s*=\s*240",
    r"m_pattern\s*=\s*PTQH_SPEEDLIMIT", r"m_pattern\s*\|=\s*PTQH_RETURNDOWN",
    r"angle\s*=\s*random\(360\)", r"length\s*=\s*\(float\)\(random\(4\)\s*\+\s*1\)\s*/\s*10\.0f",
    r"GetSin\(angle\)", r"GetCos\(angle\)", r"m_deltaPos2\.Set\(0,\s*10,\s*0\)",
    r"m_size\s*=\s*\(float\)\(random\(50\)\s*\+\s*300\)\s*/\s*100\.0f",
    r"m_longitude\s*=\s*random\(360\)", r"m_latitude\s*=\s*random\(20\)\s*\+\s*80",
    r"m_speed\s*=\s*1\.0f", r"m_accel\s*=\s*0\.01f", r"m_count\s*=\s*15",
    r"m_ChangePoint\s*=\s*12", r"m_ChangeSpeed\s*=\s*-1\.2f", r"GetTexture\(tName\)",
    r"for\s*\(int a\s*=\s*0;\s*a\s*<\s*6;\s*a\+\+\)",
    r"random\(\(a\s*\+\s*1\)\s*\*\s*60\)\s*\+\s*\(a\s*\*\s*60\)",
    r"if\s*\(degree\s*>\s*360\)\s*degree\s*=\s*360",
    r"LaunchEffectPrim\(PP_3DQUADHORN\)", r"m_pattern\s*\|=\s*PTQH_SPEEDLIMIT",
    r"angle\s*=\s*random\(360\)", r"length\s*=\s*\(float\)\(random\(4\)\s*\+\s*1\)\s*/\s*10\.0f",
    r"m_deltaPos2\.Set\(radius\s*\*\s*sn,\s*20,\s*-radius\s*\*\s*cs\)",
    r"m_size\s*=\s*\(float\)\(random\(100\)\s*\+\s*400\)\s*/\s*100\.0f",
    r"m_latitude\s*=\s*100", r"m_speed\s*=\s*1\.5f", r"m_accel\s*=\s*0\.0f", r"m_count\s*=\s*2",
    r"GetTexture\(tName\)",
], "readable EarthSpike")

ordered(exe, [
    r"if\s*\(\s*!this->m_stateCnt\s*\)", r"if\s*\(\s*\(_BYTE\)F1\s*\)",
    r"if\s*\(\s*\(_BYTE\)F1\s*==\s*2\s*\)", r"rand\(\)", r"EF_IceArrow%d\.wav", r"aEffectIceTga",
    r"aEffectWizardEa", r"aEffectStoneBmp", r"SendMsg\(GameMode,\s*135,\s*1",
    r"side\s*=\s*1\.0", r"front\s*=\s*0\.23999999", r"latitude\s*=\s*0\.2", r"SendMsg\(GameMode,\s*148",
    r"LaunchEffectPrim\(this,\s*PP_3DQUADHORN", r"m_duration\s*=\s*240", r"m_pattern\s*=\s*1", r"m_pattern\s*\|=\s*2u",
    r"rand\(\)\s*%\s*360", r"rand\(\)", r"rand\(\)\s*%\s*50\s*\+\s*300", r"rand\(\)\s*%\s*360", r"rand\(\)\s*%\s*20\s*\+\s*80",
    r"m_speed\s*=\s*1\.0", r"m_accel\s*=\s*0\.0099999998", r"m_count\s*=\s*15", r"m_ChangePoint\s*=\s*12",
    r"GetTexture\(&g_texMgr,\s*F1,\s*0\)", r"v10\s*=\s*60", r"rand\(\)\s*%\s*v10", r"v10\s*\+\s*v11\s*-\s*60",
    r"if\s*\(\s*v12\s*>\s*360\.0\s*\)", r"LaunchEffectPrim\(v26,\s*PP_3DQUADHORN", r"m_pattern\s*\|=\s*1u",
    r"rand\(\)\s*%\s*360", r"rand\(\)", r"rand\(\)\s*%\s*100\s*\+\s*400",
    r"m_latitude\s*=\s*100\.0", r"m_speed\s*=\s*1\.5", r"m_accel\s*=\s*0\.0", r"m_count\s*=\s*2",
    r"v10\s*\+=\s*60", r"while\s*\(\s*v10\s*<\s*420\s*\)",
], "canonical EarthSpike")

ordered(implementation, [
    r"if\s*\(m_stateCnt\s*!=\s*0", r"if\s*\(variant\s*==\s*0\)", r"stone\.bmp", r"wizard_earthspike\.wav",
    r"else\s*\{", r"EF_IceArrow%d\.wav", r"ice\.tga", r"mode->SendMsg\(135,\s*1",
    r"mode->SendMsg\(148", r"LaunchEffectPrim\(PP_3DQUADHORN", r"m_duration\s*=\s*240",
    r"m_pattern\s*=\s*1\s*\|\s*2", r"angle\s*=\s*rand\(\)%360", r"rand\(\)%4",
    r"m_size\s*=\s*static_cast<float>\(rand\(\)%50\+300\)\s*/\s*100\.0f",
    r"m_longitude\s*=\s*static_cast<float>\(rand\(\)%360\)", r"m_latitude\s*=\s*static_cast<float>\(rand\(\)%20\+80\)",
    r"m_count\s*=\s*15", r"m_changePoint\s*=\s*12", r"m_changeSpeed\s*=\s*-1\.2f", r"GetTexture\(textureName,\s*false\)",
    r"for\s*\(int index\s*=\s*0;\s*index\s*<\s*6;\s*\+\+index\)",
    r"rand\(\)%\(\(index\+1\)\*60\)\s*\+\s*index\*60", r"if\s*\(degree\s*>\s*360\)",
    r"LaunchEffectPrim\(PP_3DQUADHORN", r"m_pattern\s*\|=\s*1", r"childAngle\s*=\s*rand\(\)%360", r"rand\(\)%4",
    r"m_deltaPos2\s*=\s*\{radius\*childSin,\s*20\.0f,\s*-radius\*childCos\}",
    r"m_size\s*=\s*static_cast<float>\(rand\(\)%100\+400\)\s*/\s*100\.0f",
    r"m_latitude\s*=\s*100\.0f", r"m_speed\s*=\s*1\.5f", r"m_count\s*=\s*2", r"GetTexture\(textureName,\s*false\)",
], "current EarthSpike")

assert len(re.findall(r"rand\(\)", implementation)) == 10, "expected 10 random call sites including the audio variant and looped discarded draws"
readable_live_code = re.sub(r"//.*", "", source)
assert len(re.findall(r"random\(", readable_live_code)) == 10, "readable source random-call sites changed"
for asset in [
    "data/texture/effect/stone.bmp", "data/texture/effect/ice.tga",
    "data/wav/effect/wizard_earthspike.wav", "data/wav/effect/ef_icearrow1.wav",
    "data/wav/effect/ef_icearrow2.wav", "data/wav/effect/ef_icearrow3.wav",
]:
    assert (ROOT / "Ref/GRF-Content" / asset).exists(), f"missing exact source asset {asset}"
print("EarthSpike: readable IDs 79/619 match canonical 79/0x26B and current routes; mode branches, seven-primitive cadence, RNG order, audio/resources, and primary/child profiles compared. Current quake message 135 already sets the exact amplitudes repeated by Ref message 148.")
