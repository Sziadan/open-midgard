"""Curate the static inventory into an actionable report and searchable canvas.

Run audit_packets.py first. This only writes audit artifacts, never client code.
"""
from pathlib import Path
from collections import Counter, defaultdict
import csv
import json

ROOT = Path(__file__).resolve().parents[1]
SERVER = Path('C:/Users/sziad/source/repos/rathena-master')
OUT = ROOT / 'docs/packet-audit'

def evidence(file, needle, server=False):
    path = (SERVER if server else ROOT) / file
    text = path.read_text(encoding='utf-8', errors='replace')
    at = text.find(needle)
    assert at >= 0, (file, needle)
    return {'path': path.as_posix(), 'line': text.count('\n', 0, at) + 1, 'needle': needle}

def finding(id, priority, title, ops, issue, expected, refs, confidence='Confirmed source defect'):
    return dict(id=id, priority=priority, title=title, opcodes=ops.split(), issue=issue,
                expected=expected, evidence=refs, confidence=confidence)

def main():
    data = json.loads((OUT / 'inventory.json').read_text(encoding='utf-8'))
    F = finding
    E = evidence
    findings = [
        F('P01','P0','Account-ID handshake is represented as fake opcodes','0x8482 0x8483',
          'The character server sends a raw four-byte account ID before normal packets. The client instead special-cases 0x8482/0x8483 as four-byte packets and ignores them. Other account IDs can be mistaken for real packet headers; this is not a general handshake implementation.',
          'Consume and validate the raw account-ID prefix in the correct connection state, including fragmented reads, before starting ordinary framing.',
          [E('src/network/GronPacket.cpp','SetPacketSize(0x8482'),E('src/gamemode/LoginMode.cpp','case 0x8482'),E('src/char/char_clif.cpp','// send back account_id',True)]),
        F('P02','P0','Unknown packet recovery can destroy stream alignment','',
          'RecvPacket drops two bytes for an unknown opcode and scans the remaining payload as headers. Missing implementations therefore affect unrelated later packets, rather than only losing one feature.',
          'Implement correct framing for supported packets and use explicit connection-state/error recovery when a boundary cannot be determined. Never pretend a payload scan is successful decoding.',
          [E('src/network/Connection.cpp','if (knownSize == 0)')]),
        F('P03','P0','Guild member status is still unframed and unhandled','0x01F2',
          'rAthena sends a 20-byte ZC_UPDATE_CHARSTAT on guild member login/logout. It is absent from the client size table and router. The newly added 0x016D/0x01EB handlers do not replace it.',
          'Decode account/character/status/appearance fields, update the guild roster and visible-member/minimap state, and consume the exact 2008 layout.',
          [E('src/map/clif.cpp','void clif_guild_memberlogin_notice',True),E('src/network/GronPacket.cpp','void FillPacketSizeTable')]),
        F('P04','P0','Pet status length is wrong for the configured server','0x01A2',
          'The client frames 37 bytes. The compiled 20080910 server struct is 35 bytes; its extra job word is enabled only from 20081126. The client also has no pet-status handler. This can consume two bytes of the next packet.',
          'Use the profile-correct 35-byte layout and implement pet state/UI, with different lengths only under the appropriate newer profile.',
          [E('src/network/GronPacket.cpp','SetPacketSize(0x01A2, 37)'),E('src/map/packets_struct.hpp','struct PACKET_ZC_PROPERTY_PET',True)]),
        F('P05','P0','Ground-skill text request uses the wrong active contract','0x0190 0x007E',
          'The client emits PACKET_CZ_USESKILLTOPOSINFO as opcode 0x0190, 90 bytes. The evaluated server database ends with 0x0190 as a 19-byte ActionRequest; UseSkillToPosMoreInfo is 0x007E, 102 bytes with padding/offsets. Sending the current request can corrupt the server receive stream.',
          'Build this request from the actual 20080910 server opcode, length and field offsets; test the real request against the server parser rather than only the old Ref layout.',
          [E('src/network/Packet.h','struct PACKET_CZ_USESKILLTOPOSINFO'),E('src/network/MapSendProfile.cpp','bool BuildActiveUseSkillToPosInfoPacket'),E('src/map/clif_packetdb.hpp','parseable_packet(0x007e,102',True)]),
        F('P06','P2','Duplicate registration hides an advertised inventory handler','0x01EF',
          'Actual final registration replaces HandleEquipInventoryList with commerce cart handling. The receive profile advertises 0x01EF as timed equipment inventory while commerce interprets it as a normal cart list. For the active 2008 server the inventory opcode is 0x02D0, so this is primarily a conflicting compatibility claim, not proof that live inventory currently breaks.',
          'Make aliases profile-specific and ensure one unambiguous layout and owner per opcode. Remove unsupported compatibility claims or provide a tested route.',
          [E('src/network/MapSendProfile.cpp','profile.equipInventoryListTimed = 0x01EF'),E('src/gamemode/GameModePacket.cpp','for(u16 id:{0x0122')], 'Confirmed registration/profile conflict; active impact is version-dependent'),
        F('P07','P1','Party positions are discarded','0x0107',
          'A real server-generated party minimap position packet reaches an empty handler. The minimap has m_partyPosList rendering support, but this notification does not feed it.',
          'Update/remove party markers by account ID and honor the server removal sentinel and map changes.',
          [E('src/gamemode/GameModePacket.cpp','router.Register(0x0107'),E('src/map/clif.cpp','void clif_party_xy(',True)]),
        F('P08','P1','Map cell and map-property changes are discarded','0x0192 0x0199 0x01D6 0x02E7',
          'These registrations all use HandleIgnorePacket. Dynamic map cell changes and map mode/restriction/property updates cannot be represented by consuming their bytes.',
          'Apply cell flags/appearance and the profile-specific map-property fields to world state, restrictions and indicators; verify each family against Ref and active rAthena senders.',
          [E('src/gamemode/GameModePacket.cpp','router.Register(0x0192'),E('src/map/clif.cpp','void clif_changemapcell',True)]),
        F('P09','P1','NPC class-change notification is discarded','0x01B0',
          'ZC_NPCSPRITE_CHANGE is registered to the empty handler. Handling other sprite-change opcodes does not implement this wire contract.',
          'Decode the target and class/view change, rebuild the correct sprite or model, and invalidate render/picking caches.',
          [E('src/gamemode/GameModePacket.cpp','router.Register(0x01B0'),E('src/map/clif.cpp','void clif_class_change',True)]),
        F('P10','P1','Storage password flow is a message/log stub','0x023A 0x023B 0x023C',
          'The request handler explicitly says the flow is not implemented; the result handler only logs result/error count. No complete prompt/reply/retry state machine exists.',
          'Provide the proper password UI and protocol response, handle success/failure/retry counts, and resume storage only when authorized.',
          [E('src/gamemode/GameModePacket.cpp','void HandleStoragePasswordRequest'),E('src/gamemode/GameModePacket.cpp','void HandleStoragePasswordResult')]),
        F('P11','P1','Equipment/privacy and party-invite settings are discarded','0x02C8 0x02C9 0x02D8 0x02D9 0x02DA',
          'The server configuration and party invitation-state notifications use empty handlers. Request/acknowledgement state is not reflected in client preferences and privacy controls.',
          'Implement the supported 2008 settings fields, related commands/UI requests, and server acknowledgements without assuming a local toggle succeeded.',
          [E('src/gamemode/GameModePacket.cpp','router.Register(0x02D9'),E('src/map/clif.cpp','void clif_parse_configuration',True)]),
        F('P12','P1','Server-directed disconnect is ignored','0x02D5',
          'The client registers this to an empty handler. Ref dispatches it to Zc_Isvr_Disconnect. The currently configured rAthena sender was not established by the scan, so event reachability is a separate question from the missing implementation.',
          'Implement the appropriate disconnect/transition and UI/network cleanup for the supported family.',
          [E('src/gamemode/GameModePacket.cpp','router.Register(0x02D5'),E('Ref/GameModePacket.cpp','CGameMode::Zc_Isvr_Disconnect')], 'Confirmed client stub; server reachability needs testing'),
        F('P13','P1','Show-EFST notification is only partially implemented','0x028A',
          'The handler implements level-99 aura and the PC Assumptio bit, but not the other Ref GameModeShowEFST cases. A registered status/effect packet is therefore not full visual support.',
          'Implement the remaining supported PC/NPC effect flags and removal/lifetime semantics, with visual tests for each effect family.',
          [E('src/gamemode/GameModePacket.cpp','void HandleNpcShowEfstUpdate'),E('Ref/GameModePacket.cpp','int showEFST)')], 'Confirmed partial implementation'),
        F('P14','P2','Extended status values are parsed only as icon timing','0x043F',
          'HandlePacket043F reads status, actor, flag and remaining time, but ignores the remaining value fields. Those fields can carry status-specific behavior. This newer receive family is supported by client code but is not the configured 2008 status packet.',
          'Associate value fields with the relevant status semantics before calling the extended packet implemented.',
          [E('src/gamemode/GameModePacket.cpp','void HandlePacket043F')], 'Confirmed unused fields; per-status impact needs review'),
        F('P15','P1','Guild rank changes do not refresh local authority','0x0156 0x0160 0x0166 0x0174',
          'Guild packets update WindowState and the local rank title, while localGuild.permissions remains sourced from the belong packet. A changed rank or rank mode can leave invite/expel controls and guards stale until another refresh/login.',
          'Recompute local permissions/title from the current member position after both member-position and position-definition changes.',
          [E('src/gamemode/GameModePacket.cpp','void HandleGuildWindowPacket'),E('src/qtui/QtCommerce.cpp','m_localGuild.permissions&1')], 'Confirmed state propagation gap'),
        F('P16','P2','Guild menu permissions are stored but not applied','0x014E',
          'The menuFlags value is stored by the decoder, but the Qt adapter does not expose it and the guild UI always offers its six tabs.',
          'Apply the server menu capability mask to tabs/actions while preserving server authority.',
          [E('src/core/GuildState.h','if(op==0x14e)'),E('src/qtui/QtUiStateAdapter.cpp','const auto& guild=g_session.m_localGuild;const auto& info='),E('src/qtui/qml/RoGuild.qml','model: ["Overview"')], 'Confirmed partial implementation'),
        F('P17','P1','Guild receive support is not complete guild functionality','0x015B 0x015D 0x0170 0x017E 0x0180 0x0183',
          'The guild window implements notices, ranks, skills, emblem upload and leaving, but has no corresponding actions for expulsion, disbanding, initiating an alliance/opposition, removing a relation, or guild-chat sending.',
          'Add permission-gated actions, proper confirmation where needed, request builders and acknowledgement-driven updates.',
          [E('src/qtui/QtUiStateAdapter.cpp','QObject::connect(m_state,&QtUiState::guildActionRequested'),E('src/map/clif.cpp','void clif_parse_GuildMessage',True)], 'Confirmed feature/request gaps from UI and send-path review'),
        F('P18','P1','Group chat transport and filtering are incomplete','0x0108 0x0109 0x017E 0x017F',
          'Incoming party chat exists, but outbound chat paths provide global and whisper sends rather than party/guild requests. Guild messages are currently recorded with the party channel, so guild and party filtering cannot be independent.',
          'Implement distinct party/guild send modes and channels, with the server-specific request headers and separate filtering/history behavior.',
          [E('src/gamemode/GameMode.cpp','bool SendGlobalChatMessage'),E('src/gamemode/GameModePacket.cpp','if(op==0x17f)'),E('src/ui/UINewChatWnd.cpp','kChatFilterLabels')], 'Confirmed partial group-chat feature'),
        F('P19','P1','Legacy mail is compose/send only','0x023F 0x0240 0x0241 0x0242 0x0243 0x0244 0x0245 0x0247 0x024A 0x0255 0x0257 0x0273 0x0274',
          'The recent implementation handles composing/sending and its result, but not inbox listing, reading, attachments, deletion, new-mail notification or return handling. Most of their incoming packets are unframed.',
          'Implement the full legacy-mail state machine and UI, rather than treating a working send acknowledgement as complete mail support.',
          [E('src/gamemode/Commerce.cpp','if(c.action=="mail")'),E('src/map/clif.cpp','void clif_Mail_refreshinbox',True)], 'Confirmed subsystem gap'),
        F('P20','P2','Snapshot parsers accept partial record arrays','0x00A3 0x00A4 0x00A5 0x00A6 0x0201 0x02D0 0x02D1 0x02E8 0x02EA',
          'Several inventory/storage/friend list handlers loop over complete records without first rejecting a trailing partial record. Some clear the previous snapshot first, so a malformed list can replace good state with a truncated list. Commerce and GuildState already reject partial arrays more consistently.',
          'Validate the complete payload and decode into temporary state before replacing snapshots; add malformed/empty/fragmentation cases.',
          [E('src/gamemode/GameModePacket.cpp','void HandleNormalInventoryList'),E('src/gamemode/GameModePacket.cpp','void HandleEquipInventoryList'),E('src/gamemode/GameModePacket.cpp','void HandleFriendsList')], 'Confirmed validation gap; malformed input required'),
        F('P21','P1','Server trade distance failure notifies only one peer','0x00E7 0x01F5',
          'In trade_tradeack, the acceptance-distance failure sends TRADE_ACK_TOOFAR only to the accepting client and clears both partners. The requester gets no failure packet. Client preflight/expiry guards mitigate this but do not repair that server behavior.',
          'For a server-side fix, send the failure/cancellation to both clients before clearing partners, and test different GM permissions and movement between request/accept.',
          [E('src/map/trade.cpp','void trade_tradeack',True),E('src/gamemode/Commerce.cpp','void UpdateTradeContext')], 'Confirmed server behavior defect; client mitigation exists'),
    ]
    ignored = {
        '0x0100': 'Server defines this as C2S LeaveParty. Resolve/remove the bogus receive registration; do not invent an incoming message.',
        '0x0107': 'Party minimap position/update/removal.', '0x0192': 'Dynamic map-cell change.',
        '0x0199': 'Map property/mode state.', '0x01B0': 'NPC sprite/class change.',
        '0x01D6': 'Map type/property restrictions.', '0x01F8': 'Ref arms the adoption skill; implement adoption flow and verify server version.',
        '0x0283': 'Map connection account-ID confirmation: decode/validate identity.',
        '0x02B1': 'Quest list snapshot.', '0x02B2': 'Quest mission/objective information.',
        '0x02C9': 'Party invitation refusal setting.', '0x02D3': 'Bind-on-equip notification.',
        '0x02D5': 'Inter-server disconnect/transition.', '0x02D9': 'Server configuration result/state.',
        '0x02DA': 'Equipment-sharing/config notification.', '0x02DD': 'Battleground team/emblem identity.',
        '0x02E7': 'Extended map-property state.', '0x0814': 'Buying-store board/list entry; newer family.',
        '0x0816': 'Buying-store board removal; newer family.'}

    by_op = defaultdict(list)
    for item in findings:
        for op in item['opcodes']:
            by_op[op].append(item)
    for row in data['rows']:
        row['review'] = ' | '.join(f"{f['id']} {f['title']}" for f in by_op[row['opcode']])
        if row['opcode'] in ignored:
            row['ignored_behavior_required'] = ignored[row['opcode']]
        if by_op[row['opcode']]:
            row['manual_findings'] = [f['id'] for f in by_op[row['opcode']]]
    (OUT / 'inventory.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    (OUT / 'findings.json').write_text(json.dumps(findings, indent=2), encoding='utf-8')
    fields = ['opcode','stage','direction','names','server_size','client_size','client_handler','client_handler_line','server_parser','server_field_positions','server_emitters','status','reachability','review']
    with (OUT / 'inventory.csv').open('w', newline='', encoding='utf-8-sig') as f:
        writer=csv.DictWriter(f,fields);writer.writeheader()
        for row in data['rows']:
            writer.writerow({k:'; '.join(x['function'] if isinstance(x,dict) else x for x in row.get(k, [])) if isinstance(row.get(k),list) else row.get(k,'') for k in fields})

    rows=data['rows']
    missing=[r for r in rows if 'missing receive handler' in r['status']]
    framed_missing=[r for r in rows if 'missing framing' in r['status']]
    strong=[r for r in missing if r['server_emitters'] and r['server_size'] is not None]
    ignored_rows=[r for r in rows if 'ignored/stubbed' in r['status']]
    outgoing=[r for r in rows if r['server_parser']]
    source_missing=[r for r in framed_missing if r['server_emitters'] and r['server_size'] is not None]
    lines=['# Packet implementation audit', '',
      'Snapshot: 2026-11-03. Client default `packetver23`; rAthena custom `PACKETVER 20080910`, `PACKETVER_SAK_NUM 20080910`.', '',
      'This is a client-facing login/character/map protocol audit, not a request to implement inter-server login/char/map traffic in the client. No gameplay code or server code was changed during this audit.', '',
      'The audit evaluates rAthena preprocessor guards, compiles its actual registry/packed wire sizes, executes a stand-alone probe of the client\'s actual final registration order and packet-size implementation, and reviews the resulting gaps against source. This is a static audit, not a claim that every handled packet has passed a live replay.', '',
      '## What the counts mean', '',
      f'- {len(rows)} opcode rows in the combined configured registry/client/declaration inventory.',
      f'- {data["counts"]["server_registry"]} final map registry entries from {data["counts"]["server_registry_assignments"]} assignments; last assignment wins.',
      f'- {data["counts"]["client_table"]} client size-table/router entries; {data["counts"]["handler_count"]} named map handler functions indexed for review.',
      f'- {len(findings)} manually reviewed findings, with source evidence and recommended behavior.',
      f'- {len(ignored_rows)} explicit empty map handlers. Every one is listed as unfinished or a direction/version issue requiring resolution.',
      f'- {len(missing)} incoming declaration/reference candidates have no handler; {len(strong)} of those also have active sender/reference evidence and a known wire length.',
      f'- {len(framed_missing)} incoming candidates have no client framing; {len(source_missing)} have sender/reference evidence plus a known layout.',
      f'- {len(outgoing)} server-accepted request opcode rows are included in the outbound inventory. Literal/profile constructor evidence is not proof of a working UI-to-server feature.', '',
      '**Do not read these numbers as counts of guaranteed live failures.** The server database retains historical aliases, some declarations are unavailable under 2008, and some newer sender functions are compiled even when their call sites require a later feature. The inventory preserves that reachability uncertainty. Registered handlers are marked “semantics not certified,” never “complete.”', '',
      '## Highest-priority implementation and correctness findings', '']
    for f in findings:
        lines += [f"### {f['id']} · {f['priority']} · {f['title']}", '',
                  'Packets: '+(', '.join('`'+x+'`' for x in f['opcodes']) or 'connection framing / handshake state')+'.', '',
                  f"Evidence level: {f['confidence']}.", '',f['issue'],'', '**Required handling:** '+f['expected'],'',
                  'Sources: '+', '.join(f"[{Path(e['path']).name}:{e['line']}]({e['path']}:{e['line']})" for e in f['evidence'])+'.', '']
    lines += ['## Every explicit empty receive registration', '']
    for r in ignored_rows:
        lines += [f"- **{r['opcode']}** — {ignored.get(r['opcode'],'Unfinished; establish the proper direction and version before implementing.')} Client length: {r['client_size']}; server: {r['server_size']}." ]
    lines += ['', '## Complete missing incoming-handler candidate list', '',
      'Each row below is unimplemented at dispatch, but not every row is proven reachable during ordinary 2008 play. A known size and sender are stated separately. Names are the server symbols; senders identify the behavior to implement. Historical/unused rows without an established incoming direction remain in the machine inventory rather than being mislabeled as missing game features.', '']
    for r in missing:
        name=', '.join(r['names']) or 'legacy numeric opcode'
        senders=', '.join(dict.fromkeys(x['function'] for x in r['server_emitters'])) or 'no sender established by scan'
        lines += [f"- **{r['opcode']} · {name}** — server size {r['server_size']}; client size {r['client_size']}; {senders}. {r['reachability']}." ]
    lines += ['', '## Complete accepted-request inventory', '',
      'This records every active map-server request registration. “Evidence found” points to a constructor/profile use in inventory.json; it is not a semantics certification. “Not established” is a review queue, not an automatic declaration that the request is absent. A legacy opcode may be an alias of an implemented request under another profile.', '']
    for r in outgoing:
        proof='constructor/profile reference found' if r['outbound_evidence'] else 'outbound path not established by scan'
        lines += [f"- **{r['opcode']} · {r['server_parser']}** — {r['server_size']} bytes; positions `{r['server_field_positions']}`; {proof}." ]
    lines += ['', '## Remaining registered-handler review queue', '',
      'Every registered opcode is retained in inventory.csv/json with its handler and source line. Their existence is only a dispatch-coverage fact. Handler/helper semantics, UI state transitions, lifecycle/removal, error paths, ordering, reconnect and actor bootstrap still require per-family review or wire replay. Examples already confirmed partial are storage passwords, show-EFST, guild permissions, group chat, mail and record-array validation above.', '',
      '## Suggested order', '',
      '1. Correct handshake/framing defects and outbound layout mismatches; replay concatenated and fragmented packet streams.',
      '2. Implement currently discarded map, party, NPC, configuration and disconnect state notifications.',
      '3. Complete guild permissions/notifications and group chat, storage passwords, NPC progress/crafting/repair responses, and quest state.',
      '4. Finish pet/homunculus/mercenary, chat rooms, inbox/attachments, rental/binding, battleground/instance, cash-shop and auction packet families as actual features.',
      '5. Bring later compatibility families into explicitly selected version profiles; avoid one global mixture of unrelated wire formats.', '',
      '## Reproduction and files', '',
      '- `python tools/audit_packets.py` regenerates the compiled/static inventory.',
      '- `python tools/render_packet_audit.py` reapplies the reviewed findings and generates this report and the canvas source.',
      '- `inventory.json` retains sender and constructor evidence, source hashes, uncertainty and final handler registration.',
      '- `inventory.csv` is the filterable full inventory; `findings.json` is the reviewed implementation backlog.',
      '- `historical-catalog.csv` catalogs all numeric definitions found across server versions; inactive entries are not counted as missing 2008 features.',
      '- Intermediate probes and preprocessed server files are in the ignored build directory `build-mingw-preset/packet-audit/`.', '']
    (OUT / 'README.md').write_text('\n'.join(lines),encoding='utf-8')
    write_canvas(data,findings,dict(union=len(rows),missing=len(missing),ignored=len(ignored_rows),findings=len(findings)))
    print(json.dumps({'reviewed_findings':len(findings),'missing_handler_candidates':len(missing),'known_sender_and_layout_missing':len(strong),'unframed_sender_and_layout':len(source_missing),'ignored':len(ignored_rows)}))

def write_canvas(data,findings,counts):
    # Only the public SDK is imported. Data is fully embedded, with no network.
    payload=[{k:r.get(k) for k in ('opcode','stage','direction','names','server_size','client_size','client_handler','server_parser','server_emitters','status','reachability','review')} for r in data['rows']]
    content='''import {useState,useMemo,useHostTheme,Stack,Row,H1,H2,Text,Button,Pill,Table,Stat,Callout,CollapsibleSection} from "cursor/canvas";
const rows=__ROWS__;
const findings=__FINDINGS__;
const counts=__COUNTS__;
export default function PacketAudit(){
 const theme=useHostTheme();const [query,setQuery]=useState("");const [filter,setFilter]=useState("Unfinished");const [page,setPage]=useState(0);
 const options=["Unfinished","All packets","Missing framing","Missing handler","Ignored / stubbed","Reviewed findings","Registered / unverified","Server requests"];
 const filtered=useMemo(()=>rows.filter(r=>{
   const blob=[r.opcode,...r.names,r.client_handler,r.server_parser,...r.server_emitters.map(e=>e.function),r.review,...r.status].join(" ").toLowerCase();
   if(query&&!blob.includes(query.toLowerCase()))return false;
   if(filter==="All packets")return true;
   if(filter==="Missing framing")return r.status.includes("missing framing");
   if(filter==="Missing handler")return r.status.includes("missing receive handler");
   if(filter==="Ignored / stubbed")return r.status.includes("ignored/stubbed");
   if(filter==="Reviewed findings")return !!r.review;
   if(filter==="Registered / unverified")return !!r.client_handler;
   if(filter==="Server requests")return !!r.server_parser;
   return !!r.review||r.status.includes("missing framing")||r.status.includes("missing receive handler")||r.status.includes("ignored/stubbed");
 }),[query,filter]);
 const totalPages=Math.max(1,Math.ceil(filtered.length/60));const current=Math.min(page,totalPages-1);const visible=filtered.slice(current*60,(current+1)*60);
 return <Stack gap={20} style={{padding:24,color:theme.text.primary,background:theme.bg.editor}}>
  <H1>Ragnarok packet implementation audit</H1>
  <Text>2026-11-03 · Client packetver23 · rAthena PACKETVER 20080910 · Login / character / map protocol</Text>
  <Row gap={28}><Stat label="Combined opcode inventory" value={String(counts.union)}/><Stat label="Reviewed findings" value={String(counts.findings)}/><Stat label="Explicit empty handlers" value={String(counts.ignored)}/></Row>
  <Callout tone="warning">A registration or consumed payload is not an implementation. Missing-handler counts are static candidates, not guaranteed live failures. Version, direction, sender reachability and wire-layout evidence are retained below.</Callout>
  <H2>Highest-priority findings</H2>
  <Stack gap={8}>{findings.map(f=><CollapsibleSection key={f.id} title={f.id+" · "+f.title} defaultOpen={f.priority==="P0"}>
   <Stack gap={8}><Row gap={8}><Pill>{f.priority}</Pill><Text>{f.opcodes.join(" · ")||"Connection state / framing"}</Text></Row><Text>{f.issue}</Text><Text weight="medium">Required: {f.expected}</Text><Text>{f.confidence}</Text><div>{f.evidence.map((e,i)=><div key={i}><a href={e.path+":"+e.line} style={{color:theme.text.link}}>{e.path.split("/").slice(-2).join("/")+":"+e.line}</a></div>)}</div></Stack>
  </CollapsibleSection>)}</Stack>
  <H2>Full inventory and implementation queue</H2>
  <Row gap={10}><input aria-label="Search packets" placeholder="Search opcode, feature, handler or finding…" value={query} onChange={e=>{setQuery(e.target.value);setPage(0)}} style={{width:360,padding:8,color:theme.text.primary,background:theme.fill.tertiary,border:"1px solid "+theme.stroke.primary}}/><select aria-label="Coverage filter" value={filter} onChange={e=>{setFilter(e.target.value);setPage(0)}} style={{padding:8,color:theme.text.primary,background:theme.fill.tertiary,border:"1px solid "+theme.stroke.primary}}>{options.map(x=><option key={x}>{x}</option>)}</select><Text>{filtered.length} rows</Text></Row>
  {visible.length>0?<Table headers={["Opcode / direction","Server definition / behavior","Wire bytes · server / client","Client handler","Audit status"]} rows={visible.map(r=>[
   <div><strong>{r.opcode}</strong><div>{r.stage} · {r.direction}</div></div>,
   <div>{r.names.join(", ")||r.server_emitters.map(e=>e.function).join(", ")||r.server_parser||"Legacy / unresolved definition"}<div style={{color:theme.text.secondary}}>{r.reachability}</div></div>,
   String(r.server_size??"?")+" / "+String(r.client_size),
   r.client_handler||"No handler established",
   <div>{r.status.join("; ")}<div style={{color:theme.accent.primary}}>{r.review}</div></div>
  ])} stickyHeader striped/>:null}
  <Row gap={12}><Button disabled={current===0} onClick={()=>setPage(current-1)}>Previous</Button><Text>Page {current+1} / {totalPages}</Text><Button disabled={current+1>=totalPages} onClick={()=>setPage(current+1)}>Next</Button></Row>
  <Text>Source: local compiled packet registries and packed structures, preprocessed server client interfaces, and reviewed client source. Later protocol declarations and historical aliases are not automatically counted as live 2008 behavior. Existing handlers remain unverified until their semantics and lifecycle are tested.</Text>
 </Stack>;
}
'''
    content=content.replace('__ROWS__',json.dumps(payload)).replace('__FINDINGS__',json.dumps(findings)).replace('__COUNTS__',json.dumps(counts))
    (OUT/'packet-audit.canvas.tsx').write_text(content,encoding='utf-8')

if __name__=='__main__':main()
