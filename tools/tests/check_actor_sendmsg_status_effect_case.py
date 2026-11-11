#!/usr/bin/env python3
"""Compare actor status/effect message 110 branch-by-branch to Ref and the EXE."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
REF = (ROOT / "Ref/GameActor.cpp").read_text(encoding="utf-8", errors="replace")
EXE = (ROOT / "HighPriest.exe.c").read_text(encoding="utf-8", errors="replace")
REF_PRIM = (ROOT / "Ref/RagEffectPrim.cpp").read_text(encoding="utf-8", errors="replace")
EXE_PRIM = EXE
ACTOR = (ROOT / "src/world/GameActor.cpp").read_text(encoding="utf-8")
WORLD = (ROOT / "src/world/World.cpp").read_text(encoding="utf-8")
WORLD_H = (ROOT / "src/world/World.h").read_text(encoding="utf-8")


def body(text: str, marker: str) -> str:
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for pos in range(opening, len(text)):
        if text[pos] == "{": depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0: return text[opening + 1:pos]
    raise AssertionError(f"unclosed function: {marker}")


def norm(value: str) -> str:
    return re.sub(r"\s+", " ", value)


def actor_case(function: str, case_id: int) -> str:
    switch = re.search(r"switch\s*\(\s*(?:message|msg)\s*\)\s*\{", function)
    assert switch
    offset, depth, labels = switch.end(), 1, []
    for match in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
        line = match.group(0)
        if depth == 1:
            label = re.match(r"\s*case\s+(\d+)\s*:", line)
            if label: labels.append((int(label.group(1)), switch.end() + match.start(), switch.end() + match.end()))
        depth += line.count("{") - line.count("}")
        if depth == 0: break
    for ix, (label, _, end) in enumerate(labels):
        if label == case_id:
            return function[end:(labels[ix + 1][1] if ix + 1 < len(labels) else len(function))]
    raise AssertionError(f"actor message {case_id} missing")


def switch_labels(function: str, expression: str) -> set[int]:
    result = set()
    matches = list(re.finditer(rf"switch\s*\(\s*{re.escape(expression)}\s*\)\s*\{{", function))
    assert matches, f"switch({expression}) missing"
    for switch in matches:
        depth = 1
        for match in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
            line = match.group(0)
            if depth == 1:
                label = re.match(r"\s*case\s+(?:0x([\da-fA-F]+)|(\d+))\s*:", line)
                if label: result.add(int(label.group(1), 16) if label.group(1) else int(label.group(2)))
            depth += line.count("{") - line.count("}")
            if depth == 0: break
    return result


def switch_branch(function: str, expression: str, case_id: int) -> str:
    matches = list(re.finditer(rf"switch\s*\(\s*{re.escape(expression)}\s*\)\s*\{{", function))
    for switch in matches:
        offset, depth, labels = switch.end(), 1, []
        for match in re.finditer(r"[^\n]*(?:\n|$)", function[switch.end():]):
            line = match.group(0)
            if depth == 1:
                label = re.match(r"\s*case\s+(?:0x([\da-fA-F]+)|(\d+))\s*:", line)
                if label:
                    value = int(label.group(1), 16) if label.group(1) else int(label.group(2))
                    labels.append((value, switch.end() + match.start(), switch.end() + match.start() + label.end()))
            depth += line.count("{") - line.count("}")
            if depth == 0: break
        for ix, (label, _, end) in enumerate(labels):
            if label == case_id:
                return function[end:(labels[ix + 1][1] if ix + 1 < len(labels) else len(function))]
    raise AssertionError(f"case {case_id} missing in switch({expression})")


marker = "void __thiscall CGameActor::SendMsg(CGameActor *this, int sender, int message, int arg1, float arg2, int arg3)\n{"
ref_send, exe_send = body(REF, marker), body(EXE, marker)
assert norm(ref_send) == norm(exe_send), "complete Ref/EXE CGameActor::SendMsg bodies differ"
src_send = body(ACTOR, "void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)\n{")
ref110, exe110 = actor_case(ref_send, 110), actor_case(exe_send, 110)
assert norm(ref110) == norm(exe110), "message 110 Ref/EXE branches differ"
src110 = actor_case(src_send, 110)
source_ids = switch_labels(ref110, "arg1")
current_ids = switch_labels(src110, "skillStatus")
assert source_ids | {149} == current_ids, f"message110 subcase IDs differ: source-only={sorted((source_ids | {149}) - current_ids)}, current-only={sorted(current_ids - (source_ids | {149}))}"

# Each source skill/status arm is tied to the implementation's concrete effect transition.
source_routes = {
    2: [166], 25: [166], 31: [166], 68: [424], 86: [203, 166], 87: [254], 95: [453],
    97: [655, 656], 103: [367, 474], 105: [369], 107: [368], 110: [375], 113: [382],
    121: [395, 396], 122: [396], 123: [394], 133: [442, 443], 134: [426, 442], 149: [424],
    152: [441, 474, 487], 153: [419], 159: [546], 160: [], 161: [166],
    165: [418, 544], 166: [418, 544], 167: [418, 544], 188: [166], 198: [601],
    200: [604], 201: [604], 207: [617], 232: [418, 544], 235: [254], 263: [586],
    277: [456], 278: [456], 279: [456], 280: [456], 281: [456],
}
source_aliases = {2: 161, 25: 188, 87: 235, 165: 232, 166: 232, 167: 232, 200: 201}
current_aliases = {
    2: 123, 25: 123, 31: 123, 86: 123, 87: 123, 97: 123, 107: 123, 113: 123, 122: 123,
    68: 149, 153: 263, 161: 263, 188: 263, 207: 263, 235: 263,
    165: 232, 166: 232, 167: 232, 200: 201,
    277: 281, 278: 281, 279: 281, 280: 281,
}
claim_routes = {
    2: [166], 25: [166], 31: [166], 86: [203, 166], 87: [254], 97: [655],
    103: [474], 107: [368], 113: [382], 121: [396], 122: [396], 123: [394],
    153: [419], 161: [166], 165: [418, 544], 166: [418, 544], 167: [418, 544],
    188: [166], 207: [617], 232: [418, 544], 235: [254], 263: [586],
}
claim_side_effects = {103: [367], 121: [395]}
showefst_body = body(ACTOR, "void CGameActor::RefreshShowEfstVisuals(")
showefst = norm(showefst_body)
showefst_claim_specs = (
    ((2, 161), "case 2: case 161: request(166, 0xffff, 130); break;", (166,)),
    ((25, 188), "case 25: case 188: request(166, 0xffff, 128); break;", (166,)),
    ((31,), "case 31: request(166, 0xffff, 169); break;", (166,)),
    ((86,), "case 86: request(203); request(166, 0xfffe, 128); break;", (203, 166)),
    ((87, 235), "case 87: case 235: request(254); break;", (254,)),
    ((97,), "case 97: request(655); break;", (655,)),
    ((103,), "case 103: request(474); break;", (474,)),
    ((107,), "case 107: request(368); break;", (368,)),
    ((113,), "case 113: request(382); break;", (382,)),
    ((121, 122), "case 121: case 122: request(396); break;", (396,)),
    ((123,), "case 123: request(394); break;", (394,)),
    ((153,), "case 153: request(419); break;", (419,)),
    ((165, 166, 167, 232), "case 165: case 166: case 167: case 232: request(418); request(544); break;", (418, 544)),
    ((207,), "case 207: request(617, 0xffff, 130); break;", (617,)),
    ((263,), "case 263: request(586); break;", (586,)),
)
showefst_claims = {}
for status_ids, group, effect_ids in showefst_claim_specs:
    assert group in showefst, f"RefreshShowEfstVisuals claim branch changed: {group}"
    for status in status_ids:
        showefst_claims[status] = (group, list(effect_ids))
assert set(claim_routes) == set(showefst_claims), "status-110 claim coverage differs from RefreshShowEfstVisuals"
assert all(claim_routes[status] == effect_ids for status, (_, effect_ids) in showefst_claims.items()), \
    "status-110 claim effect mapping differs from RefreshShowEfstVisuals"
assert "m_showEfstIconClaims.rbegin()" in showefst and "switch (*it)" in showefst
assert "wanted->marker == instance.marker && wanted->option == instance.option" in showefst
assert "if (hasAttached(wanted.id)) continue;" in showefst, "snapshot/icon reconciliation may duplicate another producer's effect"
assert "effect && effect->GetEffectType() == id" in showefst, "effect guard must search attached effects"
assert "!hidden && !hasAttached(418)" in showefst, "snapshot companion 544 must observe the primary 418 start guard"
assert "if (id == 420 && nonPc) LaunchEffect(540);" in showefst
assert "if (id == 655 && (nonPc || status655Ending)) LaunchEffect(656);" in showefst
claims_impl = norm(src110)
assert "claims.erase(std::remove(claims.begin(), claims.end(), skillStatus), claims.end()); if (enabled) claims.push_back(skillStatus); RefreshShowEfstVisuals(!enabled && skillStatus == 97);" in claims_impl
assert "effect != owned && effect->GetEffectType() == id" in src110, "message 110 may remove a ShowEFST-owned instance by ID"
assert set(source_routes) | {29, 34, 264, 266} == source_ids | {149}
for skill, expected in source_routes.items():
    implementation_branch = norm(switch_branch(src110, "skillStatus", current_aliases.get(skill, skill)))
    if skill in claim_routes:
        assert "updateVisualClaim();" in implementation_branch, f"message110 subcase {skill}: missing shared effect claim"
        assert all(str(effect) in implementation_branch for effect in claim_side_effects.get(skill, [])), \
            f"message110 subcase {skill}: distinct direct side effect omitted"
    else:
        assert all(str(effect) in implementation_branch for effect in expected), f"message110 subcase {skill}: implementation branch omitted effect id(s) {expected}"
    source_id = source_aliases.get(skill, skill)
    if skill not in (68, 149, 160, 152, 134, 133, 121, 97, 207, 277, 278, 279, 280, 281, 159):
        source_branch = norm(switch_branch(ref110, "arg1", source_id))
        assert all(str(effect) in source_branch for effect in expected), f"source subcase {source_id} lacks effect id(s) {expected}"

assert "m_isTrickDead = 0" in src110 and "m_isLieOnGround = 0" in src110 and "SetAction(64, 4, 1)" in src110
assert "removeUnclaimed(375); RefreshShowEfstVisuals(); return;" in norm(switch_branch(src110, "skillStatus", 110))
assert "if (!enabled) { removeUnclaimed(424); RefreshShowEfstVisuals(); }" in norm(switch_branch(src110, "skillStatus", current_aliases[68]))
assert "if (enabled) launch(367);" in norm(switch_branch(src110, "skillStatus", 103))
assert "removeOne(395);" in norm(switch_branch(src110, "skillStatus", 121))
assert "SetNightMode(enabled)" in src110 and "void CWorld::SetNightMode(bool enabled)" in WORLD
night_marker = "void __thiscall CWeather::Process(CWeather *this)\n{"
ref_night, exe_night = body(REF_PRIM, night_marker), body(EXE_PRIM, night_marker)
assert norm(ref_night) == norm(exe_night), "Ref/EXE CWeather::Process bodies differ"
assert "0.0049999999" in norm(exe_night) and "if ( x > 0.5 )" in norm(exe_night) and "if ( this->m_Diffuse.y > 0.5 )" in norm(exe_night)
night_impl = norm(body(WORLD, "void CWorld::UpdateNightLighting()\n{"))
assert "m_bgDiffuseCol.x > 0.5f" in night_impl and "m_bgDiffuseCol.y > 0.5f" in night_impl
assert "m_bgDiffuseCol.x < m_bgMaxDiffuseCol.x" in night_impl and "m_bgDiffuseCol.y < m_bgMaxDiffuseCol.y" in night_impl
assert "m_bgEnvDiffuseCol.x = 1.0f - (1.0f - m_bgAmbientCol.x) * (1.0f - m_bgDiffuseCol.x)" in night_impl
assert "m_bgMaxDiffuseCol" in WORLD_H and "m_bgEnvDiffuseCol" in WORLD_H
assert "m_masterActor" not in src110  # no accidental message 110 effect-owner routing
print(f"message 110: complete Ref/EXE body and branch match; {len(source_ids)} nested IDs and current effect/state/night routes verified")
