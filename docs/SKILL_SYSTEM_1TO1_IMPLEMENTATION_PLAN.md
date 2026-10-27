# Skill System 1:1 Implementation Plan

Status: IN PROGRESS — partial implementation; see `docs/skill/PROGRESS.md` and `docs/skill/PARITY_LOG.md`. Full acceptance has not passed.
Scope: gameplay-relevant skill and spell logic, state, effects, and animations from the original 2008 HighPriest client. Preserve these behaviours in `src/`; do not recreate the original UI windows, layout, input, or rendering 1:1.
Audience: implementation agents of any capability level. Every work package below is written so it can be executed in isolation, with exact reference anchors, exact output locations, and a pass/fail checklist.

---

## 0. Non-negotiable rules (read before every task)

These rules apply to every work package in this document. A task is not finished if any of them is violated.

### 0.1 In-scope gameplay logic is transcribed, not interpreted

1. The decompiled original client under `Ref/` is the specification. You port its control flow, its numeric constants, its table contents, its call order and its timing. You do not "improve" it, "simplify" it, or reproduce "the general idea".
2. Every numeric constant (effect id, wave name, motion type, state id, delay in ms, frame count, colour, offset, factor such as `0.0023148148`) is copied from `Ref` verbatim. Never invent a value. Never round. Never pick "a similar effect".
3. If `Ref` has a `switch` with N cases, the port has N cases with the same ids and the same bodies. If `Ref` has a table with N entries, the port has N entries with the same values.
4. If `Ref` does something you do not understand, port it anyway and add a one-line comment `// Ref: <file>:<line>` explaining where it came from. Do not drop it.
5. A port that handles "the common skills" and falls back to a generic path for the rest is NOT acceptable. The default path is acceptable only for skill ids that take the default path in the canonical executable and corroborating source as well.
6. UI presentation is not a 1:1 acceptance target. Existing/new UI systems may expose skill data or invoke equivalent actions, but do not port legacy window classes, pixel layout, art composition, controls, or input flows unless a specific gameplay behavior depends on them. UI-only branches are recorded as out of scope; they do not block skill/effect parity.

### 0.2 No stubs, no placeholders, no "close enough"

1. No `TODO`, `FIXME`, `// not implemented`, `return;` bodies, empty `case` labels that `Ref` fills, or "fallback" branches that `Ref` does not have.
2. No guessed asset names. Resolve exact `.wav`/`.str`/`.rsm`/`.spr` names from `HighPriest.exe` bytes first with WP0.1; use `Ref` to corroborate the recovered literal and call site. If the executable and `Ref` cannot resolve a name, the task is BLOCKED: record the exact symbol (e.g. `aEffectT_24`) in `docs/skill/BLOCKED.md` with the call site, and stop. Use `dhxj` only as a documented last resort after both higher-priority sources fail. Do not substitute "a similar sound".
3. No candidate-path lists ("try `ef_firebolt.wav`, then `firebolt.wav`, then ..."). One exact path per call site, as `Ref` has it.
4. Existing "close enough" code in `src/` that the work package replaces must be deleted, not left alongside the port. (A list of the known offenders is in section 3.)
5. The existing Lua bridge override (`g_buabridge.GetSkillEffectInfoBySkillId`) in the skill pipeline is not part of the 2008 client and must be removed from the skill code paths (WP3).

### 0.3 Source precedence

| Question | Authority | Corroboration / fallback |
|---|---|---|
| What does the client do visually / audibly / animation-wise? | `HighPriest.exe` (the executable's code and data bytes; use the matching root `HighPriest.exe.c` decompilation to inspect control flow) | `Ref/*.cpp` is the next proof/check. Consult `Ref/dhxj/*` only when the executable and `Ref` leave a specific detail unresolved, and record that fallback in `MISMATCHES.md`. |
| Packet opcodes, lengths, field layouts, which packet the server actually sends | `PACKET_VERSION_ALIGNMENT.md`, then `Ref/eAthena_src_2011/map/clif.c` and `Ref/RunningServer/packet_db.txt` | `Ref/GameModePacket.cpp` for how the client consumes the fields |
| Asset existence / exact file name bytes | `HighPriest.exe` bytes via `tools/ro_exe_resolve.py` / WP0.1 | `Ref` source and `Ref/GRF-Content/data/...` corroborate the literal/use and existence; `dhxj` is last-resort evidence only if the executable and `Ref` cannot recover it. |
| Skill ids / unit ids the server can emit | `Ref/eAthena_src_2011/map/skill.h` (`enum e_skill`, `enum skill_unit_id`) and `Ref/eAthena_src_2011/db/*skill_db.txt` | `src/skill/SkillEnumIdTable.inc` |

If the executable, `Ref`, and `dhxj` disagree, follow the executable, use `Ref` as the second-source proof/check, and consult `dhxj` only for unresolved details. Write disagreements or last-resort fallbacks into `docs/skill/MISMATCHES.md` (file, line, values, which source you followed, and why). Known mismatches are pre-listed in section 9.

Packet version: the rebuilt client targets `packet_ver 23` (2008-09-10aSakexe). `Ref` is the same client family, but do not assume opcode layouts from `Ref` without confirming them against `clif.c` for the layout and `packet_db.txt` `packet_ver: 23` block for the length.


### 0.4 UI boundary

This plan covers skills, spells, gameplay state, effects, and animations. It does not require the modern UI to reproduce the 2008 client’s window classes, rendering, layout, controls, or input behavior. Keep source-faithful skill data models or receiver sketches as optional groundwork when they do not interfere with the existing Modern Qt UI. UI-only effects such as opening a legacy list window are documented, but they are not gameplay-parity blockers. If a UI branch sends a gameplay command (for example, using a selected skill), preserve that command semantics through the current UI architecture without recreating the source window.

### 0.5 Media rule

Never open, render, or play a media file to "see what it looks like". Inspect bytes/headers only (`.str` header, `.wav` RIFF header, `.spr/.act` headers). Correctness is established by matching `Ref`, not by eyeballing.

### 0.6 Build and verification commands

```
cmake --preset vs2022-x64 -DRO_ENABLE_DEV_DEPLOY=ON
cmake --build --preset build-release-x64
```

Any throwaway probe script lives outside the repository (e.g. `%TEMP%\ro_probe_*.py`, run with `.venv/Scripts/python.exe`). Do not commit probe scripts. Generators that produce committed `*.generated.inc` files live in `tools/` and are committed.

### 0.7 Deliverable per task

Every work package produces, in addition to code:

* A parity note appended to `docs/skill/PARITY_LOG.md`: for each ported function, `Ref file:line-range -> src file:function`, plus the list of every `case`/table entry ported (ids only), and the list of anything BLOCKED.
* A clean build with the commands above.
* The checklist in the work package ticked with evidence (log lines, table diff output, etc.).

### 0.8 Finish and compare one function before starting another

Port functions serially. Do not begin editing or porting another function until the current function has passed a source-to-implementation comparison and that comparison is recorded. A successful build is necessary where required, but it does not establish parity: both versions can compile while implementing different behavior.

For every function, compare its complete control flow and behavior against its reference before marking it complete. For switches, mechanically extract the reference case IDs and implementation case IDs, normalize their representation, and compare the complete ordered lists (and counts). Record the extraction commands or script, both ID lists, and the comparison result. Do not rely on visual spot checks or a manually copied list.

Case-label equality is only a structural check. For complex functions, also produce a branch-level comparison: enumerate each conditional and switch branch, its guards, operation order, calls, state/resource mutations, return or termination behavior, and relevant constants; map each reference branch to its implementation branch and mark it matched or explain a source-supported discrepancy. Mechanically compare tables and other finite dispatch data as well. If the source cannot be parsed reliably, improve the extractor or document the bounded manual method and its evidence; do not treat compilation as a substitute.

The function is not complete, and work on the next function must not start, while any case, branch, table entry, call-order rule, or behavior remains unaccounted for. Add the comparison evidence to the function's parity-log entry. If a mismatch cannot be resolved from the reference, record it in docs/skill/MISMATCHES.md or docs/skill/BLOCKED.md as appropriate and keep the function open.

---

## 1. How the original client does skills (reference architecture)

Read this once. Every work package refers back to these stages.

```
 server packet
   │
   ▼
 CGameMode::Zc_*            Ref/GameModePacket.cpp         (stage A: decode + route)
   │  SendMsg(actor, AM_x, &struct)
   ▼
 CGameActor::SendMsg        Ref/GameActor.cpp:2609          (stage B: actor message dispatch)
   ├─ 11  → Am_Action        Ref/GameActorMsgHandler.cpp:52
   ├─ 37  → Am_Skill         Ref/GameActorMsgHandler.cpp:748
   ├─ 38  → Am_Groundskill   Ref/GameActorMsgHandler.cpp:336
   └─ 27  → enqueue WBA      (will-be-attacked queue)
   │
   ▼
 CGameActor::ProcessWillBeAttacked   Ref/GameActor.cpp:8039  (stage C: timed hit delivery)
   │  SendMsg(this, wba.message(26/39/93/94/96/97/111/130/133), &wba)
   ▼
 CGameActor::SendMsg case 26/39/93/94/96/97/111/130/133   Ref/GameActor.cpp:3254, 3908-4147
   │  hit wave (PlayWave), LaunchEffect(effectId1), LaunchEffect(effectId2),
   │  damage numbers (msg 88/89/92/95), SetState(4) hit reaction, motion speed
   ▼
 CAbleToMakeEffect::LaunchEffect(effectId)  →  CRagEffect::Init   Ref/RagEffect.cpp:12839
   │  m_duration = durationTable[effectId]; pick .str or hard-coded handler
   ▼
 CRagEffect::OnProcess      HighPriest.exe.c:419284 (main, ids 0-299 & 374-490)
 CRagEffect::OnProcess2     Ref/RagEffect.cpp:10606 (ids 300-373)
 CRagEffect::OnProcess3     Ref/RagEffect.cpp:11107 (ids 491-709)
   │  per-id handler method (≈470 methods, Ref/RagEffect.cpp + Ref/RagEffect2.cpp)
   ▼
 CEffectPrim (26 primitive kinds)   Ref/RagEffectPrim.cpp
```

Ground units are separate objects:

```
 Zc_Skill_Entry / Zc_Skill_Entry2  →  CWorld::MakeSkill  →  CSkill / CGraffiSkill   Ref/Skill.cpp
   SendMsg(45, SKILLSTANDENTRYSTRUCT)  init: visibility, party check, unit-job → effect id, trap model
   SendMsg(47, job)                    change unit type
   SendMsg(119)                        reveal trap (Zc_Skill_Update)
   SendMsg(37, SKILLSTRUCT)            unit "attacks" (Zc_Notify_Skill with src = unit AID)
   CSkill::OnProcess                   loop effects per unit job (Ref/Skill.cpp:1290-1698)
 Zc_Skill_Disappear  →  CWorld::RemoveSkill
```

Casting:

```
 Zc_useskill_ack (0x013E, Ref/GameModePacket.cpp:33840)  →  CSession::GetSkillActionInfo2 → SetState(motionType), msg 85 (begin spell effect), msg 82 (gauge), msg 81 (target), msg 86 (target lock effect)
 Zc_Dispel (0x01B9)        →  cancel: msg 83, msg 85 with -1 / msg 87
```

Sound:

```
 PlayWave(name, dx, dy, dz, volumeMaxDist=250, volumeMinDist=40, vFactor=1.0)   Ref/Wave.cpp:289
   dx/dz are (effect pos − player pos); loads via CResMgr::Get(name); PlayDigital or Play3DDigital by g_soundMode
```

Key tables in `CSession` (Ref/Session.cpp, Ref/Session2.cpp):

| Function | Ref anchor | Purpose |
|---|---|---|
| `GetSkillActionInfo(skillId,&beginEffectId,&motionType)` | Session2.cpp:4070 (212 cases) | cast begin effect + motion per skill; default 16 / 7 |
| `GetSkillActionInfo2(...)` | Session2.cpp:4739 (164 cases) | variant used by `Zc_useskill_ack` (property/job dependent) |
| `InitSkillEffectIdTable()` | Session2.cpp:6188 | `m_skillAttackEffectIdTable[1039]` |
| `GetAttackEffectIdBySkillId` / `GetAttackedEffectIdBySkillId` | Session.cpp:6925 / 6938 | effect launched on attacker / on target |
| `LaunchSkillSpecialEffect(skillId, level, sActor, tActor)` | Session2.cpp:6673 | extra effects + waves per skill |
| `GetMonsterSkillInfo` | Session (see grep) | monster action index for skill |
| `InitJobHitWaveName` / `InitWeaponHitWaveName` / `InitWeaponSwingWaveName` | Session.cpp:18190 / 24433 / 24554 | hit/swing wave tables |
| `GetJobHitWaveName` / `GetWeaponHitWaveName` / `GetWeaponSwingWaveName` / `GetAttrWaveName` | Session.cpp:5683 / 5994 / 6033 / 5369 | lookups |

---

## 2. Verified facts you will need (do not re-derive, do not contradict)

### 2.1 `HighPriest.exe` layout (for resolving truncated IDA strings and tables)

Image base `0x400000`. Sections (VA, raw file offset):

| Section | VA | Raw offset |
|---|---|---|
| `.text` | `0x401000` | `0x1000` |
| `.rdata` | `0x6CC000` | `0x2CC000` |
| `.data` | `0x6F1000` | `0x2F1000` |
| `.rsrc` | `0x7B8000` | `0x32D000` |

`file_offset = raw + (VA − sectionVA)`. All literal strings are cp949 (EUC-KR). Decode with `cp949`.

Already resolved by this method (use these, do not re-guess):

| Symbol / VA | Decoded value | Used by |
|---|---|---|
| `byte_71A89C` | `트랙킹.str` | `CRagEffect::Init` |
| `byte_71A88C` | `불스아이.str` | `CRagEffect::Init` |
| `byte_71A90C` | `소울번.str` | `CRagEffect::Init` |
| `byte_71A8FC` | `사람효과.str` | `CRagEffect::Init` |
| `byte_71A930` | `찹쌀떡.str` | `CRagEffect::Init` |
| `byte_71AB6C` / `71AB5C` / `71AB4C` / `71AB3C` | `빨간포션.str` / `주홍포션.str` / `노란포션.str` / `하얀포션.str` | `CRagEffect::Init` (potion effects) |
| `byte_724FBC` / `724FA8` / `724F94` | `외부소품\트랩02.rsm` / `트랩01.rsm` / `트랩03.rsm` | `CSkill::GetTrapModelName` |
| `byte_724F7C` / `724F64` / `724F4C` / `724F34` | `외부소품\트랩03_6.rsm` / `03_4` / `03_5` / `03_2` | `CSkill::GetTrapModelName` |
| `byte_724F20` / `724F0C` | `외부소품\트랩04.rsm` / `트랩05.rsm` | `CSkill::GetTrapModelName` |
| `byte_725018` / `724FFC` | `몬스터\mandragora_atk.spr` / `.act` | `CSkill` (Mandragora unit) |

All of these exist in `Ref/GRF-Content` (the `.str` files are in `data/texture/effect/`, 234 `.str` files total; the directory listing shows the cp949 names as Latin-1 mojibake, e.g. `Æ®·¢Å·.str` = `트랙킹.str`). The resource loader must open them by their cp949 byte name.

### 2.2 `durationTable`

IDA labels it `int durationTable[22]` at VA `0x7182AC` ("guessed type"). The real table is 710 `int32` entries (effect ids 0..709). Verified values read from the exe:

* `[0..5]=50`, `[6]=100`, `[7]=50`, `[8]=80`, `[9]=80`, `[10]=100`, `[11]=80`, `[12]=40`, `[13]=9999`, `[14]=50`, `[15]=100`, `[16]=40`, `[17]=30`, `[18]=50`, `[19]=50`, `[20]=100`, `[21]=100`, `[22]=270`, `[23]=999`
* `[158]=999`, `[312]=100`, `[313]=100`, `[325]=100`, `[337]=200`, `[371]=200`
* `[700]=36000`, `[701..703]=99999999`, `[704]=40`, `[705]=1200`, `[706]=1500`, `[707]=1800`, `[708]=3600`, `[709]=9999`
* Exactly three zero entries below 710: ids `161`, `184`, `198`.

The current `src/world/RagEffect.cpp` hard-codes different durations (e.g. id 12 → 56, id 158 → 72). Those are wrong and are replaced by the generated table (WP0/WP5). Individual handlers may overwrite `m_duration` after `Init`; port those overwrites where `Ref` has them.

### 2.3 `CRagEffect::OnProcess` dispatch (HighPriest.exe.c:419284)

* Returns 0 immediately if `m_master == nullptr`.
* Skip-frame gate: for the id set `{25,37,49,51,52,53,147,152,228,254,312,313,320,325,328,362,397,398,418,436,437,438,441,454,467..473,488,489,490,503,504,506,523..536,590,603,629,678,679,680,...}` (port the exact list from the two `switch` blocks at 419640/419656 and 421955/421982) the gate `if (g_isSkipFrame > 0 && m_master->m_BeZero == 1) { m_BodyLight = 0; m_BeZero = 0; }` is applied before dispatch.
* Routing: `if (type in 300..373) OnProcess2(); else if (type >= 491) OnProcess3(); else switch(type)` with 416 case labels (nominal ids 0..299 and 374..490, excluding 19 and 243, plus 709). Unknown id → `ErrorMsg("Invalid Effect Id %d")` and return 0.
* After dispatch: `++m_stateCnt; return m_stateCnt < m_duration;`
* `OnProcess2` (Ref/RagEffect.cpp:10606): 73 cases, ids 300..373.
* `OnProcess3` (Ref/RagEffect.cpp:11107): 204 cases, ids 491..704.
* `CRagEffect::Init` (Ref/RagEffect.cpp:12839): 164 cases mapping ids to `.str` names (including random variants `FireHit%d.str`, `Pong%d.str`, `WindHit%d.str`, `Firewall%d.str` and `m_isMinEffect` variants), then `m_duration = durationTable[m_type]`.
* Handler methods: 152 in `Ref/RagEffect.cpp`, 334 in `Ref/RagEffect2.cpp` (names listed in section 10.3). `CEffectPrim` (Ref/RagEffectPrim.cpp): 26 `Prim*` initialisers, 17 `Render*` methods, `OnProcess`, `ProcessAlpha`, `AnimMotion`, `AnimTexture`, `InitSegment`, `ShiftSegment`, `SendMsg`.

### 2.4 Packet layouts (from `Ref/eAthena_src_2011/map/clif.c`, confirmed against `Ref/GameModePacket.cpp`)

| Opcode | Name | Len (packet_ver 23) | Layout (byte offsets) | clif.c | Ref client handler |
|---|---|---|---|---|---|
| `0x0114` | ZC_NOTIFY_SKILL | 31 | skill.W@2 src.L@4 dst.L@8 tick.L@12 sdelay.L@16 ddelay.L@20 damage.W@24 level.W@26 div.W@28 type.B@30 | 4669 (only sent when `PACKETVER < 3`) | `Zc_Notify_Skill` 21265 |
| `0x01DE` | ZC_NOTIFY_SKILL2 | 33 | same, damage.L@24 level.W@28 div.W@30 type.B@32 | 4700 (what RunningServer sends) | `Zc_Notify_Skill2` 21358 |
| `0x0115` | ZC_NOTIFY_SKILL_POSITION | 35 | … x.W@24 y.W@26 damage.W@28 level.W@30 div.W@32 type.B@34 | 4740 (**commented out in eAthena 2011; never sent**) | `Zc_Notify_Skill_Position` 21482 |
| `0x011A` | ZC_USE_SKILL | 15 | skill.W@2 lv/heal.W@4 (`min(heal,INT16_MAX)`) dst.L@6 src.L@10 result.B@14 | 4798 | `Zc_Use_Skill` 27959 |
| `0x0117` | ZC_NOTIFY_GROUNDSKILL | 18 | skill.W@2 src.L@4 level.W@8 x.W@10 y.W@12 tick.L@14 | 4831 | `Zc_Notify_Groundskill` 18135 |
| `0x011F` | ZC_SKILL_ENTRY | 16 | aid.L@2 creator.L@6 x.W@10 y.W@12 unitId.B@14 visible.B@15 | 4857 | `Zc_Skill_Entry` 25375 |
| `0x01C9` | ZC_SKILL_ENTRY2 | 97 | … unitId.B@14 visible.B@15 graffiti.B@16 text@17 (80) | 4857 (graffiti only) | `Zc_Skill_Entry2` 25499 |
| `0x0120` | ZC_SKILL_DISAPPEAR | 6 | aid.L@2 | 4244 | `Zc_Skill_Disappear` 25365 |
| `0x01AC` | ZC_SKILL_UPDATE | 6 | aid.L@2 | 4259 | `Zc_Skill_Update` 25684 |
| `0x013E` | ZC_USESKILL_ACK | 24 | src.L@2 dst.L@6 x.W@10 y.W@12 skill.W@14 property.L@16 casttime.L@20 | 4530 | `Zc_useskill_ack` 33840 (dispatch `case 0x13E` at 30062) |
| `0x01B9` | ZC_DISPEL | 6 | aid.L@2 | 4562 | `Zc_Dispel` 10622 |
| `0x0110` | ZC_ACK_TOUSESKILL | 10 | skill.W@2 num.L@4 result.B@8 cause.B@9 | 4597 | `Zc_Ack_Touseskill` 7006 |
| `0x008A` / `0x02E1` | ZC_NOTIFY_ACT / 2 | 29 / 33 | ACTSTRUCT | 3999 | `Zc_Notify_Act` 16724, `Zc_Notify_Actentry` 16871 |
| `0x0119` / `0x0229` | ZC_STATE_CHANGE / 3 | 13 / 15 | aid.L@2 body.W@6 health.W@8 effect.W@10 (+ .L@10 for 0x0229) pk.B | 3225 / 3241 | `Zc_State_Change` 26879 |
| `0x0196` / `0x043F` | ZC_MSG_STATE_CHANGE / 2 | 9 / 25 | index.W@2 aid.L@4 state.B@8 (+ remain.L val1..3.L) | 5134 | `Zc_Msg_State_Change` 15260 |
| `0x01D0`/`0x01E1` | spirits | 8 / 8 | aid.L@2 num.W@6 | 6804 | `Zc_Spirits` 26130 / `Zc_Spirits2` 26215; both gameplay packet handlers are ported and branch-compared (see parity log) |
| `0x01CF` | devotion | 28 | devoter AID.L@2, five devotee AIDs at `+6..+22`, max distance.W@26 | 6767 | `Zc_DevotionList` 10555; effect handler ported and branch-compared (max distance is unused by the handler) |
| `0x01CD` | autospell | 30 | seven 32-bit skill IDs at `+2,+6,...,+26` | 461 | `Zc_AutoSpellList` 8618 (UI-only; excluded from gameplay acceptance) |
| `0x01D1` | bladestop | 14 | | 6831 | `Zc_BladeStop` 8805 |
| `0x01D2` | combo delay | 10 | | 6813 | `Zc_ComboDelay` 10255 |
| `0x0215` | ZC_SKILLMSG | 6 | msg id.L@2 | 8398 (`Ref/eAthena_src_2011/map/clif.c`) | `Zc_SKILLMSG` 25035 |
| `0x01F3` | ZC_NOTIFY_EFFECT2 | 10 | aid.L@2 effectId.L@6 | 8012 | `Zc_Notify_Effect2` 17110 |
| `0x0284` | ZC_NOTIFY_EFFECT3 | 14 | aid.L@2 effectId.L@6 num.L@10 | 8048 | `Zc_Notify_Effect3` 17131 |
| `0x019B` | ZC_NOTIFY_EFFECT | 10 | aid.L@2 type.L@6 | 3196 | `Zc_Notify_Effect` 17050 |
| `0x01D3` | ZC_SOUND | 35 | name 24B@2 act.B@26 term.L@27 aid.L@31 | 7976 | `Zc_Sound` 25976 |
| `0x0191` | ZC_TALKBOX_CHATCONTENTS | 86 | | 7686 | `Zc_talkbox_chat_contents` 33736 |
| `0x0148` | ZC_RESURRECTION | 8 | | 5336 | `Zc_resurrection` 33654 |
| `0x013D` | ZC_RECOVERY | 6 | | | `Zc_recovery` 33505 |
| `0x043D` | ZC_SKILL_POSTDELAY | 8 | skill.W@2 tick.L@4 | 4633 | Later protocol addition (2008-11-12+); absent from the canonical 2008 `HighPriest.exe` dispatcher and excluded from this plan's required target-opcode inventory. |
| `0x010F`/`0x010E`/`0x0111`/`0x0235`/`0x029D` | skill lists / updates | -1 / 11 / 39 / -1 / -1 | | 4389 / 4476 / 4425 / 14765 | `Zc_Skillinfo_List` 25700 etc. |
| `0x0162` | ZC_GUILD_SKILLINFO | variable (`packet_db`: -1) | skill records, 37 bytes each from `+6`; points at `+4` | 7320 | `Zc_GuildSkillInfo` 11880 (dispatch 354); packet/state port stores records and points. Legacy invalidation/message-23 receiver behavior is UI-only; the tested model is optional groundwork and the modern UI may consume the state through its own architecture. |
| `0x0145` | ZC_SHOW_IMAGE | 19 | name[16]@`+2`, type.B@`+18` (`255` → `-1`) | — | `Zc_show_image` 33679 (dispatch 325; UI-only, excluded from gameplay acceptance) |
| `0x01B4` | ZC_CHANGE_GUILD | 12 | aid.L@`+2`, guild id.L@`+6`, emblem version.sW@`+10` | — | `Zc_ChangeGuild` 9948 (dispatch 436) |

Client-send (packet_ver 23 block, `packet_db.txt` line 1252 ff.): `0x0438,10,useskilltoid,2:4:6`; `0x0113` is **not** useskilltoid in ver 23 (it is in the old block, line 191). `0x0116,10,useskilltopos,2:4:6:8`; `0x0190,90,useskilltoposinfo,2:4:6:8:10`; `0x011B,20,useskillmap,2:4`; `0x0112,4,skillup,2`. Confirm each against `PACKET_VERSION_ALIGNMENT.md` before use (the document is authoritative for the send side).

### 2.5 Actor state ids and motion factors (Ref)

* State ids: `0` idle, `1` walk, `2` attack, `3` dead, `4` hit reaction, `5`/`6` (Am_Action actions 1/2: pickup / sit), `7` skill (one-shot), `8` casting (no SetAction), `13` casting loop, `55` asura attack (`Ref/GameActor.cpp:3184`). Force-animation variants are driven by `SetForceAnimation` / `m_isForceState*`.
* `SetModifyFactorOfmotionSpeed(attackMT)`: factor = `attackMT * 0.0023148148`; `attackMT == 0 → 1440` (`Zc_Notify_Act`). `Zc_Notify_Act_Position` uses `attackMT=432, attackedMT=288`.
* Attacked speed factor = `attackedMT * 0.0034722222`.
* Hit delivery time inside `WBA` = `now + m_attackMotion * m_motionSpeed * 24` (+ `doubleAttackTerm` spacing for multi-hits; +192 for jobs 1016/1420; +912 for 1285/1830; +408 for 1286/1829/1287) — port from `Am_Action`/`Am_Skill` exactly.
* `Ref` `WBA` fields: `time, targetGid, message, damage, effectId1, effectId2, effectLevel, tCellX, tCellY, attackedSpeedFactor`. The `src` `WBA` (`src/world/GameActor.h:55`) has different fields and must be replaced (WP2).

---

## 3. Current state of `src/` (what exists, what is wrong)

| Area | Current state | Verdict |
|---|---|---|
| `src/session/SkillActionInfo.cpp`, `SkillActionInfo2.cpp` | Ports of `GetSkillActionInfo`/`GetSkillActionInfo2` | Keep; re-verify case-by-case in WP1 |
| `src/session/Session.cpp:2456-2520` | `GetAttrWaveName`, `GetJobHitWaveName`, `GetWeaponHitWaveName`, `InitJobHitWaveName`, `InitWeaponHitWaveName` | Exist; verify table contents against Ref in WP1. `InitWeaponSwingWaveName`/`GetWeaponSwingWaveName` missing |
| `src/session/Session.*` | No `InitSkillEffectIdTable`, `GetAttackEffectIdBySkillId`, `GetAttackedEffectIdBySkillId`, `LaunchSkillSpecialEffect`, `GetMonsterSkillInfo`, `IsSkillNotDelayed` | Missing → WP1 |
| `src/world/GameActor.h` `WBA` | `gid,time,message,attackedMotionTime,damage,damageColor,damageKind,damageDestX/Z,waveName[128]` | Not Ref-shaped → replace in WP2 |
| `src/world/GameActor.cpp` `CGameActor::SendMsg` | The mechanical inventory now finds 75 current direct message IDs, including every in-scope Ref ID; UI-only 7/32/33/54/55 are excluded and lifecycle message 48 is handled by effect destruction. Branch-level source comparisons are recorded per checkpoint in `docs/skill/PARITY_LOG.md`; inventory coverage alone does not establish complete function parity, and missing launched `CRagEffect` handlers remain WP5 work. | Route inventory complete; continue branch-level WP2 audits and retain WP5 visual-handler backlog |
| `src/world/GameActor.cpp:4932` `ProcessWillBeAttacked` | Calls a local `ApplyQueuedHitReaction` instead of `SendMsg(wba.message)` | Replace → WP2 |
| `src/world/GameActor.cpp` | No `Am_Action`, `Am_Skill`, `Am_Groundskill` | Missing → WP2 |
| `src/gamemode/GameModePacket.cpp:5246-5670` | `HandleSkillCastAck`, `HandleSkillDamageNotify`, `HandleSkillDamagePositionNotify`, `HandleGroundSkillNotify`, `HandleSkillNoDamageNotify`, `HandleSkillUnitSet` built from invented helpers (`ResolveGroundSkillEffectId`, `PlayGroundSkillImpactSound` with guessed wav names, `ResolveUseSkillPacketInfo`, `EmitSkillImpactEffects`, `QueueSkillMultiHitPresentations`, `IsBoltSkillId`, `ResolveDisplayedSkillHitCount`, `MaybeStartGrimtoothSourcePresentation`, grimtooth `-30000` sentinel suppression, skill 24 `beginEffectId = -1` hack, Lua override, `ApplyPendingLocalHideSkillFallback`) | All of this is "close enough" logic. Delete and replace with 1:1 ports → WP3 |
| `src/gamemode/GameModePacket.cpp` registrations | `0x0120`, `0x02C9` → `HandleIgnorePacket`; `0x01AC`, `0x01C9` graffiti path, `0x0115` not fully handled | Fix → WP3 |
| `src/world/GameActor.h` `CSkill` | 6 fields, no behaviour; no `CGraffiSkill`, no `CGroundScope`; `CWorld::MakeSkill/GetSkillByAID/RemoveSkill/RemoveSkillbyJT` not ported | Missing → WP4 |
| `src/world/RagEffect.cpp:3934` `CRagEffect::Init` | ~45 ids handled with hand-picked durations; everything else goes to `ResolveStrName` (generated catalog) or nothing | Replace with full `Init` port + `durationTable` → WP5 |
| `src/world/RagEffect.h` `Handler` enum | 41 handlers vs ≈470 in Ref | Port all → WP5 |
| `src/world/RagEffect.h` `EFFECTPRIMID` | 129 explicit ids from `0x00` through `0x80`; names and numeric values mechanically match `HighPriest.exe.h`. Older `Ref/dhxj/RagEffect.h` has four additional stale ids. `CEffectPrim` behavior is still incomplete. | Keep canonical enum; finish remaining primitive methods → WP5 |
| `src/world/RagEffectCatalog.generated.inc` + `tools/generate_rageffect_catalog.py` | Effect id → `.str` mapping with a hand-written `MANUAL_TOKEN_MAP` for `byte_*` symbols (guessed) | Regenerate from exe bytes; no manual map → WP0 |
| Sound | `CAudio::PlaySound/PlaySound3D`; skill sounds triggered from `PlayEzStrAssociatedStartupSound` and candidate-path lists; 259 wav/effect references in `RagEffect.cpp` | Replace with `PlayWave` port and exact per-call-site names → WP6 |
| `src/skill/Skill.cpp` | Skill metadata only (names, descriptions, SP) | Keep; not behaviour |
| `src/world/MsgEffect.cpp` | Damage numbers, attached sprite effects (cart, bird, status icons) | Keep; verify msg ids used by WP2 |

---

## 4. Work package overview and order

```
WP0  Tooling: exe resolvers + generated tables            (no dependencies)
WP1  CSession skill tables                                 (needs WP0 output)
WP2  Actor message model (SendMsg/Am_*/WBA/SetState)       (needs WP1)
WP3  Packet handlers (all ZC_* skill packets + sends)      (needs WP2)
WP4  Ground units (CSkill/CGraffiSkill/CGroundScope/World) (needs WP2, WP5-core)
WP5  CRagEffect + CEffectPrim full port                    (needs WP0; parallelisable in batches)
WP6  Sound (PlayWave + every wave call site)               (needs WP0; runs alongside WP2-WP5)
WP7  Skill-adjacent packets/state visuals                  (needs WP2, WP3)
WP8  Per-skill verification matrix                         (needs all)
WP9  Final acceptance                                      (needs all)
```

WP5 is by far the largest and is split into batches (5A–5H). Different agents can take different batches in parallel once WP5-core (5A) is merged.

---

## WP0 — Tooling and generated reference tables

Goal: make every constant the other work packages need available as verified, generated data so that no agent ever has to guess.

### WP0.1 `tools/ro_exe_resolve.py` (new, committed)

Input: `HighPriest.exe`, `HighPriest.exe.c`.
Functions:

1. `va_to_offset(va)` using the section table in 2.1 (parse the PE header; do not hard-code offsets).
2. `read_cstring(va) -> str` (cp949 decode, stop at NUL).
3. `read_int32_table(va, count) -> list[int]`.
4. `resolve_symbol(name)`: for `byte_XXXXXX` / `dword_XXXXXX` / `unk_XXXXXX` symbols, parse the hex VA from the name and read it. For `aXxx` string symbols, find `char aXxx[] = "..."; // idb` in `HighPriest.exe.c`; if the literal ends in a cut-off prefix (`effect\\T_`, `effect\\`, `effect\\mon_`, `_` …) mark it `TRUNCATED` and fall through to 5.
5. Call-site resolution for truncated wave names: scan `.text` for `E8 rel32` calls whose target is `PlayWave` (`0x43A970`). For each call, walk backwards over the 7-argument push sequence (`68 00 00 80 3F` vFactor 1.0, `6A 28` minDist 40, `68 FA 00 00 00` maxDist 250, three pushes for z/y/x which may be `51`/`52`/`50`/`FF 75 xx`/`FF B5 xx xx xx xx`/`D9 1C 24`-style float pushes, then `68 <imm32>` = string VA). Record `(call VA, string VA, decoded string)`. Map each call VA to the enclosing function using the `// Decompiled from address: 0x......` headers in `Ref/*.cpp` and `HighPriest.exe.c` (functions are contiguous; the enclosing function is the greatest header address ≤ call VA). Output `docs/skill/generated/playwave_callsites.tsv` with columns `callVA, function, stringVA, string, ok|UNRESOLVED`.
6. Repeat 5 for other sound entry points if used by skill code: `PlayDigital` (Ref/Sound.cpp:195), `Play3DDigital` (Ref/Sound.cpp:142), and any `CSoundMaker` usage in `Ref/Npc.cpp`.

Validation (must all pass before WP0 is accepted):

* `resolve_symbol("byte_71A89C") == "트랙킹.str"` and all rows in table 2.1 reproduce.
* `read_int32_table(0x7182AC, 710)` reproduces every value listed in 2.2.
* Every `aEffect*` symbol referenced by `PlayWave(` in `Ref/*.cpp` (collect with `rg -o "PlayWave\(\s*(a\w+|byte_\w+|&byte_\w+)" Ref --max-depth 1 --glob "*.cpp"`) resolves to a string that exists under `Ref/GRF-Content/data/wav/` (case-insensitive, cp949 byte compare). Print the count resolved / unresolved. Unresolved → `docs/skill/BLOCKED.md`, never a guess.
* All 26 `effect\T_…` strings at file offsets `0x311D1C–0x31A6D0` appear in the output and each is tied to at least one call site.

### WP0.2 Generated tables (new files, committed, produced by `tools/ro_exe_resolve.py` and `tools/generate_skill_tables.py`)

| Output file | Content | Source |
|---|---|---|
| `src/world/RagEffectDurationTable.generated.inc` | `static const int kRagEffectDurationTable[710] = {...};` | exe VA `0x7182AC` |
| `src/world/RagEffectStrNameTable.generated.inc` | effect id → `.str` name (incl. `%d` random variants and `isMinEffect` variant) | `CRagEffect::Init` switch, Ref/RagEffect.cpp:12839, with `byte_*` resolved via WP0.1 |
| `src/session/SkillAttackEffectIdTable.generated.inc` | the 1039-entry `m_skillAttackEffectIdTable` | `InitSkillEffectIdTable`, Ref/Session2.cpp:6188 (transcribe every explicit assignment; default value as in Ref) |
| `src/session/JobHitWaveName.generated.inc`, `WeaponHitWaveName.generated.inc`, `WeaponSwingWaveName.generated.inc` | wave tables | Ref/Session.cpp:18190 / 24433 / 24554 with symbols resolved |
| `docs/skill/generated/playwave_callsites.tsv` | see WP0.1 | |
| `docs/skill/generated/effect_handler_index.tsv` | effect id → (OnProcess tier, handler method name, Ref file:line) for all 693 dispatch case labels (416 + 73 + 204; skip-frame switches excluded) | HighPriest.exe.c:419284, Ref/RagEffect.cpp:10606, 11107 |
| `docs/skill/generated/skill_behaviour_index.tsv` | skill id → every Ref location that mentions it (`GetSkillActionInfo` case, `GetSkillActionInfo2` case, `Zc_Use_Skill` case, `Am_Skill` special case, `Am_Groundskill` case, `LaunchSkillSpecialEffect` case, `m_skillAttackEffectIdTable` entry, `Zc_Notify_Groundskill` case, `CSkill::SendMsg` case) | grep over the anchors listed in section 1 |

Rules for the generators: no manual override maps. If a value cannot be derived mechanically, emit it into `BLOCKED.md`, not into the table. Replace `tools/generate_rageffect_catalog.py` and delete `MANUAL_TOKEN_MAP`.

### WP0.3 Acceptance

* All generated files regenerate byte-identically from a clean run.
* `docs/skill/generated/skill_behaviour_index.tsv` contains every skill id in `src/skill/SkillEnumIdTable.inc` and every id in eAthena `enum e_skill` up to the highest id the Ref client has a case for, plus 8001–8046 (homunculus) and 8201–8238 (mercenary) and 10000–10014 (guild).
* Build still succeeds (generated includes are not yet consumed except where trivially wired).

---

## WP1 — `CSession` skill tables (exact ports)

Files: `src/session/Session.h`, `src/session/Session.cpp`, `src/session/SkillActionInfo*.cpp`, plus generated includes from WP0.

Tasks (each is a function-level 1:1 port; keep Ref parameter order and return semantics):

| # | Function | Ref anchor | Notes |
|---|---|---|---|
| 1.1 | `GetSkillActionInfo(int skillId, int& beginEffectId, int& motionType)` | Ref/Session2.cpp:4070 | Already ported. Diff every one of the 212 cases against Ref; fix any divergence. Default must be `beginEffectId=16, motionType=7`. |
| 1.2 | `GetSkillActionInfo2(...)` | Ref/Session2.cpp:4739 | Same for 164 cases, including job/property-dependent branches. |
| 1.3 | `InitSkillEffectIdTable()` | Ref/Session2.cpp:6188 | Fill `m_skillAttackEffectIdTable[1039]` from the generated include; call from the same place in session init as Ref. |
| 1.4 | `GetAttackEffectIdBySkillId(int)` / `GetAttackedEffectIdBySkillId(int)` | Ref/Session.cpp:6925 / 6938 | Bounds behaviour exactly as Ref. |
| 1.5 | `LaunchSkillSpecialEffect(int skillId, int level, CGameActor* s, CGameActor* t)` | Ref/Session2.cpp:6673 | Every case (419, 421, 467, 468, 469, 480, 485, 490, 501, 502, 503, 507, 512, 530, 534, 537, 540, 542, 544, 653, 654, 659, 661, 665, 666, 667, 668, 669, 672–676, 1004, 8009, 8012 and all others present). Waves via `PlayWave` (WP6) with names from WP0. |
| 1.6 | `GetMonsterSkillInfo` | locate with `rg -n "GetMonsterSkillInfo" Ref --max-depth 1 --glob "*.cpp"` | Port the table and lookup. |
| 1.7 | `IsSkillNotDelayed` and any other predicate used by `Am_Skill` | locate in Ref/Session*.cpp | Port. |
| 1.8 | `InitJobHitWaveName`, `InitWeaponHitWaveName`, `InitWeaponSwingWaveName`, `GetJobHitWaveName`, `GetWeaponHitWaveName`, `GetWeaponSwingWaveName`, `GetAttrWaveName` | Ref/Session.cpp:18190, 24433, 24554, 5683, 5994, 6033, 5369 | Compare current `Session.cpp:2456-2520` content to Ref entry by entry; add the swing table. |
| 1.9 | `GetWeaponType` | Ref/Session.cpp (grep) | Needed by hit-wave selection in `SendMsg` case 96/97 path (`Ref/GameActor.cpp:4020-4046`). |

Acceptance: a throwaway script (outside repo) that dumps each table from `src` and from the Ref transcript shows zero diffs; parity note lists all case ids.

---

## WP2 — Actor message model

Files: `src/world/GameActor.h`, `src/world/GameActor.cpp`, new `src/world/GameActorMsgHandler.cpp`, `src/world/World.cpp` (lookups), `src/world/Pc.cpp`/`Player.cpp`/`Npc.cpp` if the class split exists in `src` (otherwise the corresponding sections of `GameActor.cpp`).

### WP2.1 Structures (exact field sets and semantics)

* `struct WBA` → replace with Ref layout: `u32 time; u32 targetGid; int message; int damage; int effectId1; int effectId2; int effectLevel; int tCellX; int tCellY; float attackedSpeedFactor;` (confirm order/types from the `WBA` usage in `Ref/GameActorMsgHandler.cpp` and `Ref/GameActor.cpp:3920-3942`, where `v8[1]`=targetGid, `v8[3]`=damage, `v8[4]/v8[5]`=tCellX/tCellY, `v8[6]`=effectId1, `v8[7]`=effectId2, `v8[8]`=attackedSpeedFactor(float), `v8[9]`=effectLevel/extra). Document the final field order with the Ref indices in a comment.
* `struct ACTSTRUCT`, `struct SKILLSTRUCT`, `struct SKILLSTANDENTRYSTRUCT`, `struct GROUNDSKILLSTRUCT` as built in `Ref/GameModePacket.cpp` `Zc_Notify_Act` (16727), `Zc_Notify_Skill` (21265), `Zc_Skill_Entry` (25375), `Zc_Notify_Groundskill` (18135).
* Add `enum ActorMsg` with the numeric ids used by Ref (keep the numbers; names are for readability only). Minimum set: 0, 3, 4, 5, 6, 7, 11, 12, 14, 19, 20, 21, 26, 27, 28, 29, 32–38, 39, 43, 44, 45, 47, 48, 50–59, 61–63, 67–76, 78–83, 85–89, 92–99, 101, 102, 105–111, 117–119, 122, 123, 126–136, 139 (the full list is the `case` list of `Ref/GameActor.cpp:2609-7133`; port the whole list).

### WP2.2 `CGameActor::SendMsg` (Ref/GameActor.cpp:2609–7133)

Port every case. The skill-critical ones, with their Ref lines:

* `11` → `Am_Action` (3163)
* `12` → death check (3166)
* `26/39/93/94/111/130/133` and `96/97` → shared attacked-handler block (3254–3261, 3908–4147). Port verbatim: single-colour flash selection (effectId2 198 → `-16503900`, 184 → `-11513776`), `m_damageDestX/Z` from `tCellX/tCellY` or own pos, target job / weapon lookup, miss/lucky/perfect-dodge handling (`89`, `95`, `127`), hit wave selection order (96/97 → `GetWeaponHitWaveName(-1)`; PC job → `GetJobHitWaveName`; else attacker weapon type via `GetWeaponType` when > 1000, job 12/4013/4035 special), `LaunchEffect(effectId1)` + `SendMsg(44,1)`, `effectId2 == 122` → rotate `m_roty += 90`, `effectId2 == 216/217` → `SendMsg(14, &attackerPos)`, damage numbers by message (93 → colour `-256` kind 16 + msg 92; 130/133 → kind 22; 111 → kinds 21 and 22 with colours `-1`/`-171`; default kind 14), `SetState(4)` rules and `m_loopCountOfmotionFinish`/`m_motionSpeed` from `attackedSpeedFactor`, `m_willBeDead` → msg 12.
* `27` → enqueue `WBA` (3262): `if (arg2 == 0.0) …; wba->attackedSpeedFactor = arg2; wba->message = 111;` then push to `m_willBeAttackedList` (port exactly).
* `37` → `Am_Skill` (3349); `38` → `Am_Groundskill` (3352).
* `43` → `SetAttrState(body, effect, health, true)` + `m_pkState` (3355).
* `108` → multi-hit re-attack: `SetState(2)`, `m_attackMotion = GetAttackMotion()`, `RegisterPos`, timing (4220 ff.).
* `131`, `134` (4871, 4896) → counter/asura related.
* `85/86/87` (begin-spell / magic target effects): compare current `src` implementation with Ref and remove the `src`-only "refresh instead of relaunch" special-casing unless Ref has it.
* `44`, `80`, `14`, `50`, `53` are messages sent *to* `CRagEffect`/`CMsgEffect`; make sure `CRagEffect::SendMsg` (Ref/RagEffect.cpp:12695) and `CMsgEffect::SendMsg` implement them.

### WP2.3 `Am_Action` (Ref/GameActorMsgHandler.cpp:52–329)

Progress (2026-10-27): implementation and branch-level Ref/EXE/current comparator are complete. The comparator covers the ordered action labels, target lookup including skill units, counter effect sender, action-to-WBA message map, delay bonuses, WBA fields, and multi-hit/residual-damage timing. See `docs/skill/PARITY_LOG.md`; no live packet-replay acceptance is claimed.

Port verbatim: actions 0/4/8/9/10/11 attack path (when `m_isCounter`, launch effect 131 and attach it to the actor), `SetModifyFactorOfmotionSpeed`, `SetState(2)`, `CalcDir`, `WBA` with message 26/39/93/94 by action type, multi-hit spacing with `doubleAttackTerm`, `leftDamage`, action 1 → `SetState(5)`, 2 → `SetState(6)`, 3 → `SetState(0)`, job-specific delay additions (1016/1420 +192; 1285/1830 +912; 1286/1829/1287 +408).

### WP2.4 `Am_Skill` (Ref/GameActorMsgHandler.cpp:748–1321)

Progress (2026-10-27): implementation and branch-level Ref/EXE/current comparison are complete for current routes. The comparator covers target lookup, action/message map, attack effect table defaults/exceptions, skills 5/62 message 131, WBA values, post-effects, and skill 136 RNG ordering. See `docs/skill/PARITY_LOG.md`; no live packet-replay acceptance is claimed.

Port verbatim, including: action 5 → WBA message 96 to target unless skill ∈ {47, 11, 413, 273}; `GetSkillActionInfo` + `SetState(motionType)` unless skill ∈ {143, 263, 272, 273, 271, 371}; `IsSkillNotDelayed` → `LaunchEffect(beginEffectId)` + `SendMsg(44, count)`; `LaunchSkillSpecialEffect`; attack effect via `GetAttackEffectIdBySkillId` + `SendMsg(14, &tPos)`; attacked effect `186 → 187..190` by level; the `effectId1` table (7/17/19 → 49, 14/15 → 51, 20 → 52, 54 → 152 if damage, 90/91 → 147, 190 → 180, 467/468 → 549; default 0, with source-specific ranges/IDs → −1); message selection 39/97/93/96; skill 77 +500 ms; 5/62/413/419/421 → msg 131, 415/417 +300 ms msg 131; 379/271 → msg 93; 273 → effect 704; post-effects for 263/271/266/267/272/273/370/371/372/413; multi-hit loop (136 → msg 108 + effect 122; 272 → effect 122; 372/394 → msg 108; 413–421 → msg 134).

### WP2.5 `Am_Groundskill` (Ref/GameActorMsgHandler.cpp:336–606)

Status: implemented; source comparison complete. The Ref and canonical EXE bodies match; `tools/tests/check_groundskill_source.py` checks the source routes, current route mapping, branch ordering, special effects, wave calls, the 478 thresholds, source-equivalent `IsThisPC` job predicate, coordinate setup, and `duration+10`. Skill 83 assigns `motionCount - 1` even when the count is zero; the port now does the same when an action resource exists. A null action resource remains a defensive guard. x64 Release builds; source comparator passes. Authorized OldRO deployment matches the build hash. No live packet/visual acceptance is claimed. See `docs/skill/PARITY_LOG.md` for evidence.

Port the per-skill `PlaceEffect(pos, effectId, duration+10)` table verbatim (21→30, 25→141, 69/694→91, 70→83, 79→113, 83→92 + force action 88 last motion, 85→90, 87→74, 89→89, 91→142, 92→95, 110→102, 111/459→98, 115→69, 130→119 + bird msg 29, 140→124, 264 → `IsThisPC` → effect 166 + `SendMsg(19, x, y, 15)` + `SetAction(base+80, 6, 1)` freeze, 285→225, 286→236, 287→237, 288→238, 478→497/498/499 by level + `assulter_attack.wav`, 483→223 + `EF_FireWall.wav`, 484 → `wizard_earthspike.wav`, 491 → `LaunchEffect(58)`, 521/538 → waves) and everything else in that range.

### WP2.6 `ProcessWillBeAttacked` (Ref/GameActor.cpp:8039)

Status: implemented and source-compared. `tools/tests/check_process_wba_source.py` checks full Ref/EXE body equality and branch order. The port samples time once, dispatches due entries in list order with a null sender and the exact list-node `WBA` address, then erases each entry; future entries remain queued. `ApplyQueuedHitReaction` is absent. See `docs/skill/PARITY_LOG.md`.

Reference behavior: for each entry with `curTime >= time`, dispatch `CGameObject::SendMsg(sender=null, wba.message, &wba)` and then erase.

### WP2.7 `SetState` / `ProcessState` for all actor classes

Progress (2026-10-27): read-only source audit found canonical Ref/EXE equality for the reviewed functions. Current parity is incomplete: base `CGameActor::SetState` has only states `[0,1,2,3,4,5,7,8,9,13]` versus source `[0,1,2,3,4,5,7,9,11,12,13,16–28,30–42]`, and several shared state semantics differ. `CGameActor::ProcessState` also lacks source states and has timer/movement differences. `CPc::ProcessState`, `CPlayer`, and `CNpc` overrides are absent; current `CPc::SetState` does not cover the source branch set. `ProcessEffectState`, `MakePartyEffect`, and `MakeSpecialAttackEffect` have no current definitions. Detailed findings are in `docs/skill/PARITY_LOG.md` and `docs/skill/MISMATCHES.md`. Work must proceed one complete function at a time with branch-level source/current gates.

* `CGameActor::SetState` Ref/GameActor.cpp:5491, `ProcessState` 1631, `ProcessEffectState` 7978, `MakeSpecialAttackEffect` 7561, `MakePartyEffect` 7447.
* `CPc::SetState` Ref/Pc.cpp:5678, `ProcessState` 2382.
* `CPlayer::SetState` Ref/Player.cpp:523, `ProcessState` 87.
* `CNpc::SetState` Ref/Npc.cpp:1544.
* Confirm that states 2, 4, 5, 6, 7, 8, 13, 55 and the force-animation paths (`SetForceAnimation`, `SetForceAnimSpeed`, `m_isForceState/2/3`) behave as Ref, including `m_attackMotion`, `m_motionSpeed`, `m_loopCountOfmotionFinish`, `m_isMotionFreezed`, `m_isMotionFinished`.
* `CGameActor::SetAttrState` (Ref/GameActor.cpp:8077–8667): status visuals and waves (`ef_sight.wav` for sight/ruwach, stone/frozen explosion waves, `_stonecurse.wav`, `stun.wav`, `poison.wav` + effect 335, `curse.wav`, `silence.wav`, `confusion.wav`, `blind.wav` + effect 334, msgs 58/59/67–76, effect 16 on burrow, effect 159 removal on hide). Port in full; this is where many "skill target effects" actually come from.

Acceptance: a debug log (behind the existing `DbgLog`) printing `msg id, skill id, effect ids, wave name, delivery tick` for every WBA; for Bash (5), Double Strafe (46), Fire Bolt (19), Heal (28), Sonic Blow (136), Asura Strike (271), Grimtooth (137), Storm Gust (89), Sanctuary (70) the log must match a manual walk-through of the Ref code (write the walk-through into the parity note).

---

## WP3 — Packet handlers (stage A)

File: `src/gamemode/GameModePacket.cpp` (and `src/network/MapSendProfile.cpp`, `src/network/GronPacket.cpp` for ids/lengths).

### WP3.1 Delete the invented layer

Remove: `ResolveGroundSkillEffectId`, `PlayGroundSkillImpactSound`, `ResolveUseSkillPacketInfo`, `UseSkillPacketInfo`, `EmitSkillImpactEffect(s)`, `QueueSkillMultiHitPresentations`, `IsBoltSkillId`, `ResolveDisplayedSkillHitCount`, `MaybeStartGrimtoothSourcePresentation`, grimtooth sentinel suppression, `ResolveHealVisualEffectId` (heal thresholds are handled in `Zc_Use_Skill` case 28 per Ref: `<200 → 312`, `<2000 → 313`, else `325`, and effect 7 — port there), `ResolveSkillVisibleEffectId`, `ApplyPendingLocalHideSkillFallback`, `StartSkillSourceAnimation`, `StartDirectedSkillBeginEffect`, `StartSkillStateAnimation`, `PopulateCombatHitWaveName`, `SpawnWorldSkillEffect` (replace with `PlaceEffect`/`CWorld` equivalents as Ref uses), the Lua `GetSkillEffectInfoBySkillId` override, the `skillId == 24` begin-effect hack, and the `frozenCast` (57/62) special-case unless Ref has it.

### WP3.2 Port each `Zc_*` 1:1

| Opcode(s) | Port of | Must include |
|---|---|---|
| `0x0114`, `0x01DE` | `Zc_Notify_Skill` 21265 / `Zc_Notify_Skill2` 21358 | Build `SKILLSTRUCT`; `SendMsg(37)` to the source actor **or to the `CSkill` unit if `srcGID` is a unit AID** (`CWorld::GetSkillByAID`); monster source → `GetMonsterSkillInfo` → `SetAction(8*act-8, 1, loop)`; else `CMode::SendMsg(99, skillId, actor, level)`; `0x01DE` extras: skill 141 with level `0xFFFF` → `LaunchEffect(129)` on target; skill 662 → `LaunchEffect(372)` on target. Damage is `int16` for `0x0114`, `int32` for `0x01DE`. |
| `0x0115` | `Zc_Notify_Skill_Position` 21482 | Full port even though eAthena 2011 never sends it (record in MISMATCHES). |
| `0x011A` | `Zc_Use_Skill` 27959–30133 | Entire function: skills 249/8220 → `SetState(2)`; non-monster → `SendMsg(99)`; monster → `GetMonsterSkillInfo`; unless skill ∈ {263, 272, 273, 371} → `GetSkillActionInfo` → `SetState(motionType or 7)`; facing rules (12, 18, 21, 70, 79, 83, 85, 89, 110 via `GetClientCoor(HIWORD/LOWORD(tgt))` + `CalcDir`; 124/131 face the `CSkill`; 244 closes homunculus windows; else face target actor); then the complete per-skill `switch` (820 `case` labels in the function body) with every `LaunchEffect`, `SendMsg(109, id)`, `PlayWave`, `SetForceAnimation`, heal numbers (`SendMsg(88, amount, colour, 114)`; skill 9 colour `-16776961`; 28 and 8001 thresholds), screen shake (`GameMode SendMsg(135)`), etc. Split the switch across several agents by id range if needed, but the function must end up complete. |
| `0x0117` | `Zc_Notify_Groundskill` 18135 | `SendMsg(38)`; skill 47 (`0x2F`) arrow effects via `MakeArrowEffectPos` with the 8 offsets; 362/369 (`0x16A`/`0x171`) waves. |
| `0x011F` | `Zc_Skill_Entry` 25375 | `m_visibleTrapList` toggle; create `CSkill` only if `job >= 0x2D`; `SendMsg(45, &SKILLSTANDENTRYSTRUCT)`. |
| `0x01C9` | `Zc_Skill_Entry2` 25499 | graffiti flag → `CGraffiSkill`, `SendMsg(22)` + `SendMsg(117, text, x, y)`. |
| `0x0120` | `Zc_Skill_Disappear` 25365 | `CWorld::RemoveSkill(aid)` (currently ignored!). |
| `0x01AC` | `Zc_Skill_Update` 25684 | unit jobs 139–153 → `SendMsg(119)` (reveal trap). |
| `0x013E` | `Zc_useskill_ack` 33840 | Port Ref's handler verbatim, using `GetSkillActionInfo2`. |
| `0x01B9` | `Zc_Dispel` 10625 | cast cancel path. |
| `0x0110` | `Zc_Ack_Touseskill` | failure messages (MsgStringTable ids per clif.c comment). |
| `0x008A`, `0x02E1`, `0x008D`? | `Zc_Notify_Act` 16727, `Zc_Notify_Act_Position` 16833 | `attackMT 0 → 1440`; position variant 432/288; `SendMsg(11)`. (Needed because counter/auto-attack visuals share the WBA path.) |
| `0x0119`, `0x0229` | `Zc_State_Change` 14217 / 15263 | `SendMsg(43)` → `SetAttrState`. |
| `0x0196`, `0x043F` | `Zc_Msg_State_Change` | status icons + `CSession` effect-state list used by `IsExistEffectState(19/104)` in the hit path. |
| `0x019B`, `0x01F3`, `0x0284` | `Zc_Notify_Effect/2/3` 17050 / 17110 / 17131 | `0x0284` now also emits message 88 (type 114) with effect-specific color, launches the requested effect, and updates self HP/SP for ids 331/332 while preserving source RNG consumption. |
| `0x01D3` | `Zc_Sound` 25976 | server-triggered waves (used by some skill scripts). |
| `0x01D0`, `0x01E1` | `Zc_Spirits/2` 26133 / 26218 | spirit spheres (Monk/Champion/Gunslinger). |
| `0x01CF` | `Zc_DevotionList` 10555 | devotion link visuals. |
| `0x01CD` | `Zc_AutoSpellList` 8618 | UI-only auto-spell list. Its packet and receiver model are retained as groundwork; factory, rendering, scrolling, selection, and close-window fidelity are outside this plan’s acceptance scope. |
| `0x01D1`, `0x01D2` | bladestop / combo delay | |
| `0x0148`, `0x013D` | `Zc_resurrection` 33654, `Zc_recovery` 33505 | |
| `0x0191` | `Zc_talkbox_chat_contents` 33736 | resolves the skill AID and forwards message 7 with the raw packet payload at +6; the receiver now displays nonempty text for five seconds as a red skill speech bubble through the Qt overlay. |
| `0x0145` | `Zc_show_image` 33679 | sends message 113 with a fixed 16-byte name and type; message 113 stores the name/type and applies the source `.bmp` suffix rule. The EXE contains `DrawIllustImage` at VA `0x56C740`, but it has no call site/xref and is absent from the EXE `OnRun` render path; `Ref/dhxj`'s call is later-source behavior and is excluded. Do not add image display to this target port. |
| `0x01B4` | `Zc_ChangeGuild` 9948 | resolves actor AID and calls `SetGuildInfo(guildId, emblemVersion)`; actor category/version gates and guild/emblem state are ported. Emblem-file lookup and missing-resource notification remain open. |
| skill list family | `Zc_Skillinfo_List` 25700, `Zc_Skillinfo_Update` 25841, `Zc_Add_Skill` 8267, `Zc_GuildSkillInfo` 11880, homun/mer skill lists | keep existing if already exact; verify. |

Register every opcode in the dispatch table; nothing skill-related may remain on `HandleIgnorePacket`.

### WP3.3 Client-send side

Per `PACKET_VERSION_ALIGNMENT.md` and `packet_db.txt` ver 23: implement `0x0190 useskilltoposinfo` (90 bytes: skill lv.W@2, skill id.W@4, x.W@6, y.W@8, text 80B@10) used by Warp Portal name / Graffiti text; confirm `0x0438 useskilltoid`, `0x0116 useskilltopos`, `0x011B useskillmap`, `0x0112 skillup`. Use `CGameMode::DrawUseSkillLevelNumber` (Ref/GameMode.cpp:9839) for the level-number display on use.

### WP3.4 Acceptance

* Hex-dump based unit check (temporary, outside repo): feed captured byte buffers for each opcode into the handler and assert which actor messages / effect ids / wave names result; compare to a manual Ref walk-through.
* Live check against RunningServer for: single-target melee skill, bolt skill, heal, buff, ground AoE with units (Sanctuary, Fire Wall, Storm Gust, Meteor Storm, Lord of Vermilion), trap set/reveal/remove, Warp Portal (incl. 0x0190), Graffiti, dispel/cast cancel.

---

## WP4 — Ground units

Files: new `src/world/Skill.cpp`/`Skill.h` (world objects; do not confuse with `src/skill/` metadata), `src/world/World.cpp`.

| # | Port | Ref anchor |
|---|---|---|
| 4.1 | `CSkill` constructor/destructor/`OnInit`/`OnExit` | Ref/Skill.cpp:339, 457, 4091, 4063 |
| 4.2 | `CSkill::SendMsg` all messages: 7 (balloon), 22, 37 (unit attack → WBA with `effectId1` by skill 18→49, 79→152, 85→52, 89→51, 428–430→583), 45 (init: visibility + party check, `EF_FireWall.wav` for job 127, effect ids 106/139/99/108/107 for trap jobs, 189 random `aEffect_54` — resolve that symbol via WP0), 47 (set job; 128 → `EF_Portal.wav`; 140 → launch `m_effectId`), 117, 119 | Ref/Skill.cpp:2925–3543 |
| 4.3 | `CSkill::OnProcess` loop effects per unit job: 126→315, 127→25 (3 s lifetime), 128→317/36, 129→316, 130→91, 131→319, 132→318, 135/197→138, 136→97, 141/195/196→74, 142→95, 146→171, 154..157→239..242, 158..175→278..294 (166→277), 177→302, 178→341, 179→370, 180→374, 182→405, 183→404, 184→522, 185→517, 187→620, 188→631, 189..194→634,638..642, 199→674, 200→671, 201→672; `m_LoopEffect->m_duration += 21` | Ref/Skill.cpp:1287–1698 |
| 4.4 | `CSkill::Render`, `SetRenderInfo`, `GetTrapModelName` (names from 2.1), `ShowModel` (C3dActor, animSpeed 0.3333, frame 3, light dir (0.5, 0.7, 0.5)), `HideModel` | Ref/Skill.cpp:2875, 3547, 83, 154, 131 |
| 4.5 | `CGraffiSkill` (ctor, `OnInit`, `OnExit`, `OnProcess`, `Render`, `SendMsg`) | Ref/Skill.cpp:274, 2040, 2026, 652, 2162, 2271 |
| 4.6 | `CGroundScope` (`GetScopeSize`, ctor, `OnProcess`, `Render`, `SendMsg`) — the ground targeting scope shown while choosing a cell | Ref/Skill.cpp:4, 296, 662, 2307, 2445 |
| 4.7 | `CWorld::MakeSkill`, `GetSkillByAID`, `RemoveSkill`, `RemoveSkillbyJT`, skill list ownership, pick info | Ref/GameWorld.cpp:848, 825, 665, 721 |
| 4.8 | Unit-id coverage check: every `UNT_*` value in `Ref/eAthena_src_2011/map/skill.h:1428-1548` that is ≤ the highest unit id the Ref client handles (201) must map to the Ref behaviour; ids the Ref client does not know (e.g. 3rd-job units) go to the Ref default path, and the list of those ids is written to `MISMATCHES.md`. |

Acceptance: Fire Wall, Safety Wall, Pneuma, Sanctuary, Magnus Exorcismus, Warp Portal (waiting/active), Ice Wall, Quagmire, all Hunter traps (set hidden, reveal via `0x01AC`, trigger, remove), Bard/Dancer songs, Graffiti, Volcano/Deluge/Violent Gale/Land Protector, Basilica, Gospel, Demonstration, Spider Web, Gravitation, Ninja units (Suiton, Kaen, Tatamigaeshi), Ground Drift — each spawns the Ref loop effect id, model and sound, and is removed on `0x0120`.

---

## WP5 — `CRagEffect` and `CEffectPrim` full port

Files: `src/world/RagEffect.h`, `src/world/RagEffect.cpp` (split into `RagEffect.cpp`, `RagEffect2.cpp`, `RagEffectPrim.cpp` mirroring Ref), generated includes from WP0.

### 5A — Core (must land first)

1. `CRagEffect::Init(CRenderObject*, int effectId, vector3d delta)` — Ref/RagEffect.cpp:12839: all 164 `.str` cases from `RagEffectStrNameTable.generated.inc`, random variants with the same `rand()` modulus as Ref, `m_isMinEffect` variants, then `m_duration = kRagEffectDurationTable[m_type]`. Remove all hand-written durations from the current `Init`.
2. `CRagEffect::OnProcess` dispatcher exactly as 2.3 (skip-frame gate list, tier routing, `Invalid Effect Id` error, `m_stateCnt` increment and return value). Until a handler batch is merged, its case must call the real handler — do **not** merge the dispatcher with missing cases pointing to a no-op. Practical rule: 5A lands together with at least 5B, and each later batch adds its cases to the switch in the same commit as the handlers.
3. `CRagEffect::SendMsg` (Ref/RagEffect.cpp:12695): msgs 14 (target pos), 44 (count), 80 (duration), 50/53, and any others present.
4. `CRagEffect::Render` (12656), `RenderAniClip` (9723), `InitEZ2STRFrame` (9663), `LoadEzEffect`, `ProcessEZ2STR`, `LaunchEffectPrim` (9937).
5. `CEffectPrim` — Use `HighPriest.exe.h` as the canonical enum authority (129 values, `0x00`–`0x80`); `Ref/RagEffectPrim.h` has no `EFFECTPRIMID`, and older `Ref/dhxj/RagEffect.h` has four stale ids. The EXE `Init` body is at `HighPriest.exe.c:471542`; the Ref entry annotated as `Init` near `Ref/RagEffectPrim.cpp:6385` is actually `InitSegment`. Mechanically compare every primitive function before editing. Port all `Prim*` initialisers (`Prim2DCircle, Prim2DFlash, Prim2DTexture, Prim2DTextureOrbit, Prim3DCircle, Prim3DCylinder, Prim3DCylinderHalf, Prim3DFlash, Prim3DLine, Prim3DParticle, Prim3DParticleGravity, Prim3DParticleNoMaster, Prim3DParticleOrbit, Prim3DParticleSpline, Prim3DQuadHorn, Prim3DQuadHorn2, Prim3DQuadSquare, Prim3DSphere, Prim3DSpiral, Prim3DTexture, Prim3DTextureNoMaster`), `OnProcess`, `ProcessAlpha`, `AnimMotion`, `AnimTexture`, `InitSegment`, `ShiftSegment`, `SendMsg`, and all `Render*` (`Render2DCircle, Render2DFlash, Render2DQuad, Render2DTexture, Render3DCircle, Render3DCrossTexture, Render3DCylinder, Render3DFlash, Render3DLine, Render3DParticle, Render3DQuadHorn, Render3DQuadSquare, Render3DSegment, Render3DSphere, Render3DTexture, Render3DTextureBillboard`).
6. `CAbleToMakeEffect::LaunchEffect`, `PlaceEffect` (world-position effects), `CAbleToMakeEffect::SendMsg` (Ref/GameActor.cpp:2327) and the effect-list ownership rules (`m_effectList`, msg 109 removal by id, `Delete99LevelEffects`).

### 5B–5H — Handler batches

Each batch: port every handler method for the listed effect ids (method bodies from `Ref/RagEffect.cpp` / `Ref/RagEffect2.cpp`, located by name), add the `case` labels to the dispatcher, resolve every `PlayWave`/texture/`.str` literal via WP0 output, and append to the parity log. Handler method names are in section 10.3; which id calls which method is in `docs/skill/generated/effect_handler_index.tsv`.

| Batch | Effect ids | Count of case labels | Examples |
|---|---|---|---|
| 5B | 0–99 | from index | Hit1–Hit6, Entry/Exit/Warp, Enhance, Coin, Endure, Beginspell*, Bash, MagnumBreak, FireBall/FireWall/FireHit, ColdHit, Lightning, ThunderStorm, Heal, Blessing, IncAgility, Sight, StoneCurse, FrostDiver, SoulStrike, NapalmBeat, … |
| 5C | 100–199 | | SonicBlow(+Hit), GrimTooth, Pierce, Quagmire, BowlingBash, SpearBmr, traps (SkidTrap, BlastMine, Claymore, Flasher, Freezing…), Magnus, Sanctuary, Gloria, Kyrie, Suffragium, Aspersio, Benedictio, LexAeterna/Divina, TurnUndead, … |
| 5D | 200–299 | | Potion_*, Steal, Cloaking, EnergyDrain, CartRevolution, Torch, Snow, weather, Portal/ReadyPortal/WarpZone, … |
| 5E | 300–373 (`OnProcess2`) | 73 | CHEMICALPROTECTION, PokJuk_Sound, Demonstration, CHEMICAL2, TELEPORTATION2, ForestLight, ThrowItem, FirstAid, PARTICLE_UP, Heal/Heal2/Exit2/GlassWall2/ReadyPortal2/Portal2/Bottom_Magnus/WarpZone2/Foot, … |
| 5F | 374–490 | | Tracking (`트랙킹.str`), Bull's Eye, Soul Burn, Asura, Ninja/Gunslinger/Taekwon/Soul Linker/Star Gladiator effects, … |
| 5G | 491–600 (`OnProcess3` part 1) | | 3rd-era 2008 effects: Assaulter, Stormy Kick, Dust, Flying Side Kick, Acid Demonstration, … |
| 5H | 601–709 (`OnProcess3` part 2 + main-switch tail 506/523–536/590/603/629/678–680/700–709) | | status auras (601 Sight state), FlowerCasting, Test (709), long-duration loop effects 700–708 |

Per-batch acceptance: every id in the batch range that appears in `effect_handler_index.tsv` has a `case` and a ported method; `rg -n "TODO|FIXME|not implemented" src/world/RagEffect*.cpp` is empty; the build passes; launching each id via the dev console (`/effect <id>` or equivalent existing debug hook) does not hit `Invalid Effect Id` and does not crash.

### 5I — Textures and resources

Every `aEffect*` texture literal used by handlers (e.g. `aEffectRingBlue`, `aEffectAlphaDow`, `aEffectCloud11T`, `aEffectFootLBTg`) must be resolved via WP0 to its full path under `data/texture/effect/` and verified to exist in `Ref/GRF-Content`. Texture literals that are truncated are BLOCKED items, same as waves.

---

## WP6 — Sound

Files: `src/audio/*`, new `src/audio/Wave.cpp` (`PlayWave`), plus every call site touched by WP1–WP5.

1. Implement `PlayWave(const char* name, float x, float y, float z, int volumeMaxDist, int volumeMinDist, float vFactor)` with the Ref semantics (Ref/Wave.cpp:289): no-op unless `g_soundMode && g_isSoundOn`; load through the resource manager; `g_soundMode == 1 → PlayDigital`, `== 2 → Play3DDigital` (Ref/Sound.cpp:195 / 142 — port their distance/volume math, not an approximation). The `x/y/z` arguments are *relative to the player* (`effectPos − playerPos`), exactly as every Ref call site computes them.
2. Replace every current `CAudio::PlaySound/PlaySound3D` skill-related call and every `PlayEzStrAssociatedStartupSound` / candidate-path lookup with the exact `PlayWave` call from the corresponding Ref line. Default parameters are `250, 40, 1.0` unless the Ref call site says otherwise.
3. Hit sounds: wave name chosen in the attacked-handler block (WP2.2) via `GetWeaponHitWaveName(-1)` / `GetJobHitWaveName(job)` / `GetWeaponHitWaveName(GetWeaponType(weapon))`; swing sounds via `GetWeaponSwingWaveName` where `Ref` (`CPc::ProcessState`/`SetState`) plays them; elemental hit via `GetAttrWaveName`.
4. Status sounds in `SetAttrState` (WP2.7) and unit sounds in `CSkill` (WP4) use the same `PlayWave`.
5. `Zc_Sound` (0x01D3) server-triggered sounds.
6. Verification: `docs/skill/generated/playwave_callsites.tsv` rows ↔ `src` call sites: produce a diff (outside repo) listing each Ref call site (function + string) and the `src` function that now contains it. Zero Ref call sites may be missing from `src` in functions that are in scope (all skill/effect/actor/packet functions listed in this plan).

Wave files are under `Ref/GRF-Content/data/wav/` (1523 files; `data/wav/effect/` has 292, of which 100 have Korean or `T_`/`mon_` names). The resource loader must resolve cp949 names.

---

## WP7 — Skill-adjacent visuals

These are triggered by skill use but arrive through other packets; the plan is incomplete without them.

1. Status change icons/effects (`Zc_Msg_State_Change` → `CSession` effect-state list + `CMsgEffect` icons; `Zc_State_Change` → `SetAttrState`): all `SI_*`/effect-state ids the Ref client handles (`IsEffectState*` family in Ref). The Ref client uses the effect-state bits for Sight (22/601 aura), Ruwach, Hiding, Cloaking, Chase Walk, Energy Coat, Endure, Two-Hand Quicken, Spear Quicken, One-Hand Quicken, Max Power/Overthrust, Auto Guard, Reflect Shield, Devotion, Kyrie, Magnificat, Gloria, Angelus, Blessing, Inc AGI/Dec AGI, Aspersio, Suffragium, Impositio Manus, Lex Aeterna, Safety Wall/Pneuma standing, Freeze/Stone/Stun/Sleep/Poison/Curse/Silence/Confusion/Blind, Berserk, etc. Port `CGameActor::ProcessEffectState` (Ref/GameActor.cpp:7978) and `CMsgEffect` state handlers (Ref/Npc.cpp `CBlowEffect/CNumEffect/CSoundMaker`, `Ref/GameActor.cpp` msg 58/59/67–76 handlers).
2. Spirit spheres (`Zc_Spirits/2`) → `CMsgEffect` sphere ring; Devotion link; Blade Stop pose; Combo delay (Monk combo input window); Autospell window; Resurrection effect; Recovery numbers; Talkie Box text.
3. Level-99 aura and party/guild effects interplay (`MakePartyEffect`, `Delete99LevelEffects`) because several skill paths call them.
4. Homunculus/mercenary skills: ids 8001–8046 and 8201–8238 flow through the same `Zc_Use_Skill`/`Zc_Notify_Skill` switches; make sure the ported switches include their cases (e.g. 8001 heal thresholds, 8009, 8012 in `LaunchSkillSpecialEffect`).

---

## WP8 — Per-skill verification matrix

Produce `docs/skill/SKILL_MATRIX.md` (generated from `skill_behaviour_index.tsv` + manual columns). One row per skill id in the union of: `src/skill/SkillEnumIdTable.inc`, eAthena `enum e_skill` (1–~1040 as present in the 2011 db), 8001–8046, 8201–8238, 10000–10014. Columns:

| Column | Meaning |
|---|---|
| id, name | |
| packets | which of 0x013E / 0x011A / 0x01DE / 0x0117 / 0x011F / 0x0120 the server emits for it (from `Ref/eAthena_src_2011/map/skill.c` `clif_skill_*` calls for that skill) |
| cast begin effect / motion | from `GetSkillActionInfo(2)` |
| caster effects | ids launched on caster (`Zc_Use_Skill`, `Am_Skill`, `LaunchSkillSpecialEffect`) |
| target effects | ids launched on target (`GetAttackedEffectIdBySkillId`, `effectId1/2` in WBA, `Zc_Use_Skill`) |
| ground effects / units | `Am_Groundskill` effect, unit ids and their `CSkill` loop effects |
| sounds | every wave name from all of the above |
| Ref anchors | file:line list |
| src functions | where each is implemented |
| verified | `yes` + date + tester, or `BLOCKED(reason)` |

Rules: a row may only be marked `verified` when (a) every Ref anchor in the row has a `src` counterpart, (b) the skill was triggered against RunningServer (or, for skills the server cannot emit, via replayed packet bytes) and the debug log shows the exact effect ids and wave names from the row. "Looks right" is not a criterion. Rows that the Ref client itself handles via the default path are marked `default-path (Ref)` in the effect columns — that is a valid 1:1 result, not a stub.

---

## WP9 — Final acceptance

All of the following must be true:

1. `rg -n "TODO|FIXME|stub|placeholder|close enough|not implemented" src/world src/gamemode src/session src/audio` → no hits related to skills/effects/sound.
2. `docs/skill/BLOCKED.md` is empty, or every remaining entry has been explicitly accepted by the project owner (with the exe evidence attached showing the string is genuinely unrecoverable).
3. `docs/skill/MISMATCHES.md` lists every place where `Ref` ≠ eAthena/RunningServer ≠ `PACKET_VERSION_ALIGNMENT.md`, and for each, which source was followed.
4. `docs/skill/SKILL_MATRIX.md`: every row `verified` or `default-path (Ref)`-verified.
5. Effect coverage: for each of the 693 effect dispatch `case` labels in the three Ref dispatchers, `src` has a case and a non-empty ported handler (script outside repo diffing the case id sets).
6. Packet coverage: every opcode in 2.4 is registered and handled by a 1:1 port; none remains on `HandleIgnorePacket`.
7. Build passes with `cmake --preset vs2022-x64 -DRO_ENABLE_DEV_DEPLOY=ON` + `cmake --build --preset build-release-x64`, no new warnings in the touched files.
8. Live session against RunningServer: run the WP3.4 and WP4 acceptance scenarios plus 20 skills chosen at random from the matrix; the debug log matches the matrix rows.

---

## 9. Known source mismatches (pre-filled for `docs/skill/MISMATCHES.md`)

1. `ZC_NOTIFY_SKILL_POSITION (0x0115)`: Ref client handles it; eAthena 2011 `clif_skill_damage2` is commented out, so RunningServer never sends it. Port it anyway (client parity); verify by packet replay only.
2. `ZC_NOTIFY_SKILL (0x0114)` vs `ZC_NOTIFY_SKILL2 (0x01DE)`: eAthena sends `0x01DE` for `PACKETVER >= 3`; the client handles both. Both are ported; live testing exercises `0x01DE`.
3. `useskilltoid`: `0x0113` in the legacy block vs `0x0438` for `packet_ver 23`. Follow `PACKET_VERSION_ALIGNMENT.md` (`0x0438`).
4. Unit ids above 201 (`UNT_*` for 3rd jobs in eAthena 2011) are unknown to the 2008 client; they take the Ref default path. List them.
5. Skill ids above the Ref client's tables (3rd-job skills 2001+ present in eAthena 2011) likewise take the Ref default path in `GetSkillActionInfo` (16/7) and the attack-effect table default. List them; do not invent visuals.
6. `durationTable`: IDA shows `[22]`; the real table has 710 entries (section 2.2). Follow the exe bytes.
7. IDA string symbols with `_N` suffixes are **not** in address order (tested: 18 consistent vs 41 mismatched groups). Never resolve a truncated literal by suffix ordering; use WP0.1 call-site resolution.
8. The current `src` `WBA`, `durationTable` values, effect-id → handler coverage and candidate-path sounds are not Ref behaviour; they are replaced, not reconciled.

---

## 10. Appendices

### 10.1 Reference anchor index (one place to look things up)

| Topic | File:line |
|---|---|
| `Zc_Notify_Skill` / `2` / `_Position` | Ref/GameModePacket.cpp:21265 / 21358 / 21482 |
| `Zc_Use_Skill` | Ref/GameModePacket.cpp:27959–30133 |
| `Zc_Notify_Groundskill` | Ref/GameModePacket.cpp:18135 |
| `Zc_Skill_Entry` / `Entry2` / `Disappear` / `Update` | Ref/GameModePacket.cpp:25375 / 25499 / 25365 / 25684 |
| `Zc_Notify_Act` / `_Position` | Ref/GameModePacket.cpp:16727 / 16833 |
| `Zc_Add_Skill`, `Zc_AutoSpellList`, `Zc_DevotionList`, `Zc_Dispel`, `Zc_GuildSkillInfo`, `Zc_Msg_State_Change`, `Zc_Notify_Effect/2/3`, `Zc_Skillinfo_List`, `Zc_Skillinfo_Update`, `Zc_Sound`, `Zc_Spirits/2`, `Zc_StarSkill`, `Zc_Status_Change_Ack`, `Zc_autorun_skill`, `Zc_recovery`, `Zc_resurrection`, `Zc_talkbox_chat_contents` | 8267, 8618, 10555, 10625, 11880, 14217/15263, 17050/17110/17131, 25700, 25841, 25976, 26133/26218, 26592, 26970/27090/27210, 31684, 33505, 33654, 33736 |
| `CGameMode::DrawUseSkillLevelNumber` | Ref/GameMode.cpp:9839 |
| `CGameActor::SendMsg` | Ref/GameActor.cpp:2609 (case index at 3163 ff.) |
| `Am_Action` / `Am_Groundskill` / `Am_Skill` | Ref/GameActorMsgHandler.cpp:52 / 336 / 748 |
| `ProcessWillBeAttacked`, `SetAttrState`, `ProcessState`, `SetState`, `MakePartyEffect`, `MakeSpecialAttackEffect`, `ProcessEffectState`, `ProcessSound` | Ref/GameActor.cpp:8039, 8077, 1631, 5491, 7447, 7561, 7978, 9036 |
| `CAbleToMakeEffect::SendMsg`, `CRenderObject::SendMsg`, `CalcDir` | Ref/GameActor.cpp:2327, 7133, 8831 |
| `CPc::SetState` / `ProcessState`; `CPlayer::SetState` / `ProcessState`; `CNpc::SetState` / `SendMsg` | Ref/Pc.cpp:5678 / 2382; Ref/Player.cpp:523 / 87; Ref/Npc.cpp:1544 / 1303 |
| `CSession` tables | Ref/Session2.cpp:4070, 4739, 6188, 6673; Ref/Session.cpp:5369, 5683, 5994, 6033, 6925, 6938, 18190, 24433, 24554 |
| `CSkill` family | Ref/Skill.cpp (method index in WP4) |
| `CWorld` skill list | Ref/GameWorld.cpp:665, 721, 825, 848 |
| `CRagEffect::Init` / `OnProcess2` / `OnProcess3` / `Render` / `SendMsg` / `LaunchEffectPrim` / `InitEZ2STRFrame` / `RenderAniClip` | Ref/RagEffect.cpp:12839 / 10606 / 11107 / 12656 / 12695 / 9937 / 9663 / 9723 |
| `CRagEffect::OnProcess` (main) | HighPriest.exe.c:419284 |
| `CEffectPrim` | Ref/RagEffectPrim.cpp (type switch at 5737), Ref/RagEffectPrim.h |
| `PlayWave`, `PlayDigital`, `Play3DDigital` | Ref/Wave.cpp:289, Ref/Sound.cpp:195, 142 |
| Packet builders | Ref/eAthena_src_2011/map/clif.c (table 2.4) |
| Unit ids | Ref/eAthena_src_2011/map/skill.h:1428–1548 |
| Packet lengths ver 23 | Ref/RunningServer/packet_db.txt:1252 ff. |

### 10.2 Actor message glossary (numbers are the contract; names are mnemonic)

0 move-path, 3/4/5/6/7 spawn/appear variants, 11 `Am_Action`, 12 die-if-pending, 14 effect target position, 19 move-to-cell, 20 launch effect 7, 21 emotion, 22 (CSkill) graffiti init, 26 attacked (normal), 27 enqueue WBA, 28 die, 29 bird effect toggle, 32/33 chat title, 34/35 current-only PC gauge UI extensions (not canonical top-level `CGameActor::SendMsg` cases), 36 effect 68, 37 `Am_Skill`, 38 `Am_Groundskill`, 39 attacked (no reaction), 43 `SetAttrState`, 44 (to CRagEffect) count, 45 (CSkill) init, 47 (CSkill) set job, 48 clear msg effects, 50 (to CMsgEffect) attach, 51/52 cart on/off, 53 remove msg effect, 58/59 freeze on/off, 63 hide-change hook, 67/68 curse on/off, 69/70 sleep on/off, 71/72 stun on/off, 73/74 silence on/off, 75/76 (health bit 0x20) on/off, 80 (to CRagEffect) duration, 81 target gid, 82/83 recharge gauge on/off, 85 begin-spell effect, 86/87 magic-target effect on/off, 88 damage number, 89 miss, 92 critical, 93 attacked critical, 94 attacked lucky, 95 lucky dodge, 96/97 unit attacked, 98 resurrection, 99 vanish, 101/102 bird on/off, 105/106 chicken on/off, 108 multi-hit re-attack, 109 remove effect by id, 111 attacked (delayed WBA), 117 (CSkill) graffiti text, 119 (CSkill) reveal, 126 level-99 aura, 127 perfect dodge text, 130/133 attacked variants (kind 22 numbers), 131 counter/asura, 132, 134 combo-hit, 135 actor cursor effect, 136 remove cursor effect, 139 ice-effect forwarding. Confirm each against the `case` body before relying on the mnemonic.

### 10.3 `CRagEffect` handler method names in Ref (port all)

From `Ref/RagEffect.cpp` (152): Angelus, Aqua, Aspersio, AutoCounter, Banjjakii, Barrier, Bash, BeginSpell, BeginSpell2–7, Benedictio, BlastMine, BlastMineBomb, Blessing, BlitzBeat, BloodDrain, Bottom, Bottom2, BowlingBash, CartRevolution, ChangeCold, ChangeDark, ChangeEarth, ChangeFire, ChangeFlame, ChangeHoly, ChangePoison, ChangeWind, Claymore, Cloaking, Coin, ColdHit, Concentration, Cone, CrashEarth, Cure, CurseAttack, DarkBreath, DecAgility, Detecting, Detoxication, DragonSmoke, EarthSpike, EnchantPoison, EnchantPoison2, Endure, EnergyDrain, Enhance, Entry, Exit, EZ2STROnlyEffect, FireArrow, FireBall, FireFly, FireHit, FireIvy, FirePillar, FirePillarBomb, FirePillarOn, FireSplashHit, FireWall, Flasher, Freeze, Freezed, Freezing, FrostDiver, FrostDiver2, GlassWall, Gloria, GrimTooth, GrimToothAtk, HasteUp, HealSP, HeavensDrive, Hit1–Hit6, HitDark, IceArrow, IceWall, Impositio, IncAGIDEX, IncAgility, Invenom, Kyrie, LexAeterna, LexDivina, LightBolt, LockOn, Lord, Magnificat, MagnumBreak, Magnus, MakeBlur, MaxPower, MeteorStorm, Mvp, NapalmBeat, OverThrust, Party, Perfection, Pierce, PoisonHit, PoisonReact2, Portal, Potion_, Potion_Berserk, Potion_Con, PotionPillar, Provoke, Quagmire, ReadyPortal, Recovery, RefineFail, RefineOk, RemoveTrap, RepairWeapon, Resurrection, Revive, Ruwach, Sanctuary, SandWind, ShockWave, Sight, Sight2, Sightrasher, Signum, SkidTrap, SlowPoison, Smoke, Snow, SonicBlow, SonicBlowHit, SoulStrike, SpearBmr, Sphere, Splasher, SprayPond, SpringTrap, StatusState, Steal, StoneCurse, StormGust, Suffragium, Suicide, SuiExplosion, SummonSlave, Teleportation, Test, TetraVoltex, ThunderStorm, ThunderStorm2, TopRank, Torch, TurnUndead, TwoHandQuicken, VenomDust, VenomDust2, Warp, WarpZone, WaterBall, WaterBall2, Yufitel, YufitelHit.

From `Ref/RagEffect2.cpp` (334 methods): enumerate with `rg -n "^// Original Signature: .*CRagEffect::\w+" Ref/RagEffect2.cpp -o` and include the full list in `docs/skill/generated/effect_handler_index.tsv` (WP0). Examples seen in `OnProcess2`: CHEMICALPROTECTION, PokJuk_Sound, Demonstration, CHEMICAL2, TELEPORTATION2, ForestLight, ThrowItem, FirstAid, PARTICLE_UP, Heal, Heal2, Exit2, GlassWall2, ReadyPortal2, Portal2, Bottom_Magnus, WarpZone2, Foot, FlowerCasting.

### 10.4 Agent working protocol for a single function port

1. Start from the function address and code/data bytes in `HighPriest.exe`; inspect its matching `HighPriest.exe.c` body and IDA local-variable preamble (which can expose struct field indices like `v8[6]`). Then read the corresponding `Ref/*.cpp` body as the second-source proof/check. Use `Ref/dhxj/*` only for a detail the executable and `Ref` leave unresolved, and record that fallback.
2. Write the `src` function to match the executable's parameters and order of operations. Keep every branch and constant; use `Ref` to validate the interpretation.
3. For every string symbol (`aXxx`, `byte_XXXXXX`): look it up in `docs/skill/generated/playwave_callsites.tsv` / the generated tables / `tools/ro_exe_resolve.py`. If unresolved → `BLOCKED.md`, stop that branch (leave the exact call with a compile-time `static_assert`-style marker is **not** allowed; instead do not merge until resolved or owner-accepted).
4. For every `SendMsg(x, id, ...)`: confirm the receiving class implements message `id` (grep `case id:` in the receiver's `SendMsg`). If not, port that case too (it is in scope).
5. For every `LaunchEffect(id)`/`PlaceEffect(..., id, ...)`: confirm `id` has a dispatcher case and handler (WP5). If not, add it to the WP5 batch backlog in `PARITY_LOG.md`; the packet/actor code still launches the id.
6. Before starting another function, mechanically compare this function's reference and implementation. Extract and compare complete ordered case-ID lists for every switch, plus table or finite dispatch entries. For complex functions, create a branch-by-branch mapping of guards, operations, calls, mutations, constants, and exits. Resolve every mismatch or record why the function remains open. Save the comparison commands/script, output, and branch mapping with the parity evidence.
7. Build and run the relevant focused checks. Trigger the behaviour (live or by packet replay) when available and safe; capture the debug log. Compare the result with the Ref walk-through. Build success alone does not pass the source comparison.
8. Append the parity note with the comparison evidence. Only then mark the function complete and proceed to the next function.
