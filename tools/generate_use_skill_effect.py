"""Translate Ref CGameMode::Zc_Use_Skill's result-byte effect tree into C++."""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
REF = ROOT / "Ref" / "GameModePacket.cpp"
TSV = ROOT / "docs" / "skill" / "generated" / "playwave_callsites.tsv"
OUT = ROOT / "src" / "gamemode" / "UseSkillEffect.generated.inc"

START = 28155
END = 30132


def cpp_string(text: str) -> str:
    raw = text.replace("/", "\\").encode("cp949")
    pieces = []
    for byte in raw:
        if byte == 0x5C:
            pieces.append("\\\\")
        elif byte == 0x22:
            pieces.append('\\"')
        elif 32 <= byte < 127:
            pieces.append(chr(byte))
        else:
            pieces.append(f"\\x{byte:02X}")
    literal = '"' + "".join(pieces) + '"'
    return re.sub(r"(\\x[0-9A-Fa-f]{2})(?=[0-9A-Fa-f])", r'\1""', literal)


def wave_names() -> list[str]:
    names = []
    for line in TSV.read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if len(parts) >= 5 and parts[1] == "CGameMode::Zc_Use_Skill" and parts[4] == "ok":
            names.append(cpp_string(parts[3]))
    return names


def actor_expr(token: str) -> str:
    token = token.strip()
    if token == "m_player":
        return "source"
    if token == "GameActorByAID":
        return "target"
    if token == "v64->m_world->m_player":
        return "(mode.m_world != nullptr ? mode.m_world->m_player : nullptr)"
    return token


def translate_args(text: str) -> str:
    text = text.replace("*(__int16 *)(v2 + 4)", "rawLevel")
    text = text.replace("*(unsigned __int16 *)(v2 + 2)", "skillId")
    text = text.replace("*(_WORD *)(v2 + 2)", "skillId")
    text = text.replace("*(_DWORD *)(v2 + 10)", "static_cast<int>(sourceGid)")
    text = text.replace("*(_DWORD *)(v2 + 6)", "static_cast<int>(targetGid)")
    text = text.replace("(int)i", "i")
    text = re.sub(r"\bbuf\b", "clampedLevel", text)
    text = text.replace("m_player", "source")
    text = text.replace("GameActorByAID", "target")
    text = re.sub(r"(\d+)u\b", r"\1", text)
    return text


def main() -> None:
    lines = REF.read_text(encoding="utf-8", errors="replace").splitlines()
    body = lines[START - 1:END]
    waves = wave_names()
    wave_index = 0
    effect_vars = []
    for line in body:
        match = re.search(r"(\w+)\s*=\s*CAbleToMakeEffect::LaunchEffect", line)
        if match:
            effect_vars.append(match.group(1))
    effect_vars = list(dict.fromkeys(effect_vars))

    out = []
    unported = []
    index = 0
    while index < len(body):
        raw = body[index]
        line_no = START + index
        stripped = raw.strip()
        # DeliverUseSkill checks the result byte before calling this tree.
        if stripped == "if ( *(_BYTE *)(v2 + 14) )":
            index += 1
            continue
        indent = "    " + ("    " * ((len(raw) - len(raw.lstrip(" ")) ) // 2))

        def emit(text: str) -> None:
            out.append(indent + text)

        if stripped == "" or stripped in {
            "v60 = 0.0;",
            "beginEffectId = 0;",
            "HIDWORD(beginEffectId) = 0;",
            "z.x = 0.0;",
            "z.y = 0.0;",
            "z.z = 0.0;",
        } or stripped.startswith("memset("):
            index += 1
            continue
        if stripped.startswith(("v60 =", "*(float *)&beginEffectId", "beginEffectId =",
                                "HIDWORD(beginEffectId)", "p_m_pos =", "v44 =", "v27 =")):
            index += 1
            continue
        if "__vftable" in stripped or stripped.startswith("m_gid =") or "->m_gid" in stripped:
            index += 1
            continue
        if stripped.startswith("m_world =") or stripped.startswith("x = GameActorByAID->m_pos") or stripped.startswith("z.x = GameActorByAID->m_pos"):
            index += 1
            continue
        if "CModeMgr::GetGameMode" in stripped:
            index += 1
            continue
        if stripped.startswith("pos."):
            index += 1
            continue
        if "HIDWORD(beginEffectId)" in stripped and "SendMsg" in stripped:
            emit("if (source != nullptr) {")
            emit("    source->SendMsg(source, 82, 400 * rawLevel, 0, 0);")
            emit("}")
            index += 1
            continue

        if stripped.startswith("for ( i = (const char *)(buf + 466);"):
            emit("i = clampedLevel + 466;")
            emit("while (EffectExists(source, i)) {")
            emit("    source->SendMsg(source, 109, i, 0, 0);")
            emit("}")
            while index < len(body) and body[index].strip() != "}":
                index += 1
            index += 1
            continue

        if stripped.startswith("CAbleToMakeEffect::LaunchEffect") or "= CAbleToMakeEffect::LaunchEffect" in stripped:
            match = re.search(
                r"(?:(\w+)\s*=\s*)?CAbleToMakeEffect::LaunchEffect\((.+)\);",
                stripped,
            )
            args = [part.strip() for part in match.group(2).split(",")]
            who = actor_expr(args[0])
            effect_id = translate_args(args[1])
            if match.group(1):
                emit(f"{match.group(1)} = LaunchOn({who}, {effect_id});")
            else:
                emit(f"LaunchOn({who}, {effect_id});")
            index += 1
            continue

        if "PlayWave(" in stripped:
            match = re.search(r"PlayWave\(\w+,\s*([^,]+),\s*([^,]+),\s*([^,]+),", stripped)
            x_arg, y_arg, z_arg = match.group(1).strip(), match.group(2).strip(), match.group(3).strip()
            name = waves[wave_index]
            wave_index += 1
            if x_arg == "x" and z_arg == "z.x":
                emit(f"PlayUseSkillWaveAt(target != nullptr ? target->m_pos : vector3d{{}}, {name});")
            else:
                emit(f"PlayUseSkillWave({name}, {x_arg}, {y_arg}, {z_arg});")
            index += 1
            continue

        if "SetForceAnimation(" in stripped:
            match = re.search(r"SetForceAnimation\(m_player,\s*(.+)\)", stripped)
            emit(f"source->SetForceAnimation({translate_args(match.group(1))});")
            index += 1
            continue
        if "SetForceAnimSpeed(" in stripped:
            match = re.search(r"SetForceAnimSpeed\(m_player,\s*(.+)\)", stripped)
            emit(f"source->m_forceAnimSpeed = static_cast<u8>({translate_args(match.group(1))});")
            index += 1
            continue
        if "SetState(" in stripped:
            match = re.search(r"SetState\(m_player,\s*(\d+)\)", stripped)
            emit(f"source->SetState({match.group(1)});")
            index += 1
            continue
        if "GetSkillByAID" in stripped:
            match = re.match(r"(\w+)\s*=", stripped)
            emit(f"{match.group(1)} = mode.m_world != nullptr ? mode.m_world->GetSkillByAID(targetGid) : nullptr;")
            index += 1
            continue
        if "GetJob(" in stripped:
            emit("v18 = target != nullptr ? target->m_job : 0;")
            index += 1
            continue
        if "SendMsg(" in stripped:
            if "CMode::SendMsg" in stripped:
                emit("mode.SendMsg(135, 0, 0, 0);")
                index += 1
                continue
            match = re.search(r"SendMsg\((\w+),\s*0,\s*([^,]+),\s*(.+)\)\s*;", stripped)
            if not match:
                unported.append((line_no, stripped))
                emit(f"/* unported {line_no}: {stripped} */")
                index += 1
                continue
            obj = actor_expr(match.group(1))
            message = translate_args(match.group(2))
            rest = [part.strip() for part in translate_args(match.group(3)).rsplit(",", 2)]
            if message == "14":
                if "&source->m_pos" in stripped or "&m_player->m_pos" in stripped:
                    emit(f"if ({obj} != nullptr && source != nullptr) {{")
                    emit(f"    {obj}->SendMsg({obj}, 14, reinterpret_cast<msgparam_t>(&source->m_pos), 0, 0);")
                    emit("}")
                else:
                    emit(f"if ({obj} != nullptr && target != nullptr) {{")
                    emit(f"    {obj}->SendMsg({obj}, 14, reinterpret_cast<msgparam_t>(&target->m_pos), 0, 0);")
                    emit("}")
            else:
                args = ", ".join(rest).replace(
                    "(int)&v15->m_pos",
                    "reinterpret_cast<msgparam_t>(&v15->m_pos)",
                )
                emit(f"if ({obj} != nullptr) {{")
                emit(f"    {obj}->SendMsg({obj}, {message}, {args});")
                emit("}")
            index += 1
            continue

        text = stripped
        if text.startswith("$L") and text.endswith(":"):
            text = "L_" + text[2:]
        text = text.replace("goto $L", "goto L_")
        text = text.replace("*(unsigned __int16 *)(v2 + 2)", "skillId")
        text = text.replace("(unsigned __int16)", "(unsigned short)")
        text = text.replace("*(_WORD *)(v2 + 2)", "skillId")
        text = text.replace("*(__int16 *)(v2 + 4)", "rawLevel")
        text = text.replace("*(_WORD *)(v2 + 4)", "static_cast<unsigned short>(rawLevel)")
        text = text.replace("*(_DWORD *)(v2 + 10)", "sourceGid")
        text = text.replace("*(_DWORD *)(v2 + 6)", "targetGid")
        text = re.sub(r"\bbuf\b", "clampedLevel", text)
        text = text.replace("CAbleToMakeEffect::IsEffectExist(m_player,", "EffectExists(source,")
        text = text.replace("CSession::IsThisPC(&g_session, v18)",
                            "(v18 > 4000 && v18 < 6000 || static_cast<unsigned>(v18) < 28u)")
        text = text.replace("v64->m_world->m_player", "LOCAL_PLAYER")
        text = text.replace("m_player", "source")
        text = text.replace("LOCAL_PLAYER", "(mode.m_world != nullptr ? mode.m_world->m_player : nullptr)")
        text = text.replace("GameActorByAID", "target")
        text = re.sub(r"\bif \( source \)", "if (source != nullptr)", text)
        text = re.sub(r"\bif \( target \)", "if (target != nullptr)", text)
        text = text.replace("if ( source && target )", "if (source != nullptr && target != nullptr)")
        text = text.replace("else if ( source )", "else if (source != nullptr)")
        text = text.replace("else if ( target )", "else if (target != nullptr)")
        text = text.replace("if ( !target )", "if (target == nullptr)")
        text = text.replace("&& target )", "&& target != nullptr)")
        text = text.replace("( source )", "(source != nullptr)")
        if text.startswith("v13 ="):
            text = "v13 = rawLevel;"
        if text.startswith("v14 ="):
            text = "v14 = skillId;"
        if text.startswith("buf =") or text.startswith("clampedLevel = v13") or text.startswith("clampedLevel = 10") or text.startswith("clampedLevel = 1"):
            pass
        if re.search(r"\bv2\b|\bv60\b|beginEffectId|HIDWORD|LODWORD|CWorld::|CModeMgr::", text):
            unported.append((line_no, stripped))
            emit(f"/* unported {line_no}: {stripped} */")
        else:
            emit(text)
        index += 1

    if wave_index != len(waves):
        raise SystemExit(f"wave count {wave_index} != {len(waves)}")
    if unported:
        raise SystemExit(f"Ref translation has unported statements: {unported}")

    header = [
        "void ApplyUseSkillEffects(CGameMode& mode, CGameActor* source, CGameActor* target, int skillId, int rawLevel, u32 sourceGid, u32 targetGid)",
        "{",
        "    int clampedLevel = 0;",
        "    int i = 0;",
        "    int v13 = 0;",
        "    int v14 = 0;",
        "    int v18 = 0;",
        "    int v48 = 0;",
        "    int v49 = 0;",
        "    int v50 = 0;",
        "    int v51 = 0;",
        "    int v52 = 0;",
        "    int v53 = 0;",
        "    int v54 = 0;",
        "    int v57 = 0;",
        "    CAbleToMakeEffect* v15 = nullptr;",
        "    CAbleToMakeEffect* v17 = nullptr;",
    ]
    for name in effect_vars:
        header.append(f"    CRagEffect* {name} = nullptr;")
    header.append("    CRagEffect* v26 = nullptr;")
    header.append("    CRagEffect* v32 = nullptr;")
    header.append("    CMsgEffect* m_birdEffect = nullptr;")
    header.append("")
    footer = ["}", ""]
    used_labels = set(re.findall(r"\bgoto\s+(\w+)\s*;", "\n".join(out)))
    out = [line for line in out if not (re.fullmatch(r"\s*(\w+):\s*", line)
        and re.fullmatch(r"\s*(\w+):\s*", line).group(1) not in used_labels)]
    OUT.write_text("\n".join(header + out + footer) + "\n", encoding="utf-8")
    print(f"wrote {OUT} ({len(out)} lines), unported {len(unported)}")
    for line_no, text in unported:
        print(f"{line_no}: {text}")


if __name__ == "__main__":
    main()
