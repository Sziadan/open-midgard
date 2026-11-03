"""Generate planning views and provisional discovery ownership; never reset progress.
Run from any directory. Review routing after every audit refresh.
"""
import argparse,csv,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/packets'
def read(p): return json.loads(p.read_text(encoding='utf-8'))
def generate():
    packages=read(OUT/'work-packages.json'); audit=read(ROOT/'docs/packet-audit/inventory.json')
    findings=read(ROOT/'docs/packet-audit/findings.json'); owners=read(OUT/'finding-ownership.json'); ignored=read(OUT/'ignored-ownership.json')
    exact={}
    for f in findings:
        for op in f['opcodes']: exact.setdefault(op,owners[f['id']]['primary'])
    exact.update(ignored)
    seeds={'PK25':'0144 0189','PK26':'011E 018C 020E 0253','PK27':'019E 01A0 01A3 01A4 01A6 01AA','PK28':'022E 022F 0230 0235 0239','PK29':'029B 02A2','PK30':'01E6 01EA 01F6 0205 0216','PK32':'023F 0240 0241 0242 0243 0244 0245 0247 0248 0249 024A 0255 0257 0273 0274','PK33':'0298 0299 02BB','PK35':'0188 018F 018D 01FC 01FE 025A','PK39':'010A 010B 010C 010D 014A 014B 01AB 0219 021A 021B 021C 0224 0226 0236 0238 02C1','PK40':'0250 0252 0256 025D 025F 0287 0289','PK41':'02DE 02DF 02E0','PK42':'02CB 02CC 02CD 02CE','PK21':'02F0 02F1 02F2','PK50':'02B1 02B2 02B3 02B4 02B5 02B7'}
    for pid,ops in seeds.items():
        for op in ops.split(): exact.setdefault('0x'+op,pid)
    rules=[('PK44',r'CLAN|ACHIEV|ROULETTE|ATTENDANCE|DRESSROOM|REPUTATION|PINCODE|CHAR.*RENAME|DELETE.*CHAR'),('PK43',r'BUYINGSTORE|BUYING_STORE|SEARCHSTORE|SEARCH_STORE|BOOKING'),('PK50',r'QUEST|MISSION'),('PK41',r'BATTLEGROUND|BATTLEGROUND|BG_'),('PK42',r'INSTANCE'),('PK40',r'AUCTION|CASH|BANK'),('PK32',r'MAIL'),('PK28',r'HOMUN'),('PK29',r'MERCENARY|ELEMENTAL'),('PK27',r'PET'),('PK30',r'ADOPT|MARRIAGE|DIVORCE|WEDDING'),('PK12',r'GUILD'),('PK15',r'PARTY'),('PK16',r'FRIEND|WHISPER|IGNORE'),('PK34',r'INVENTORY.*LIST|STORAGE.*LIST'),('PK37',r'TRADE|VEND|SHOP|CART'),('PK38',r'ITEM|EQUIP|CARD|IDENTIF'),('PK23',r'SKILL|DAMAGE|CAST'),('PK24',r'STATUS|PARAM|EXP|HP|SP'),('PK25',r'NPC|DIALOG|SCRIPT'),('PK39',r'RANK|MVP|PVP|MANNER'),('PK31',r'CHATROOM|CHAT_ROOM|CHATROOM'),('PK45',r'SPAWN|MOVE|ACTOR|WALK|CHARACTION')]
    rows=[]
    for r in audit['rows']:
        op=r['opcode']; hint=' '.join(r.get('names',[])+[r.get('server_parser',''),r.get('client_handler','')]+[str(e) for e in r.get('server_emitters',[])]).upper()
        if op in exact: pid=exact[op]; why='Explicit finding, ignored-registration or family seed owner; source verification still required'
        elif r['stage']!='map': pid='PK44'; why='Connection-stage applicability and handshake review'
        else:
            hit=next(((p,pattern) for p,pattern in rules if re.search(pattern,hint)),None)
            if hit: pid=hit[0]; why='Provisional source-symbol family routing; manually confirm canonical identity'
            elif int(op,16) in range(0xD5,0xE4): pid='PK31'; why='Legacy chat-room seed range; confirm active contract'
            elif r.get('client_handler') and r.get('client_handler')!='HandleIgnorePacket': pid='PK45'; why='Existing handler semantic review; not certified implemented'
            else: pid='PK47'; why='Unresolved direction/version/family: strong-agent source adjudication required'
        secondary=['PK01','PK02','PK47','PK48']
        if r['direction'] in ('C2S','both/reused') or r.get('server_parser') or r.get('outbound_evidence'): secondary.append('PK46')
        if r.get('client_handler'): secondary.append('PK45')
        rows.append({'discovery_id':'AUDIT-'+op,'opcode':op,'stage_hint':r['stage'],'direction_hint':r['direction'],'primary_package':pid,'secondary_packages':';'.join(x for x in secondary if x!=pid),'assignment_basis':why,'classification':'UNREVIEWED','canonical_contracts':''})
    # Retain manually adjudicated ownership/classifications by discovery ID.
    path=OUT/'coverage.csv'
    if path.exists():
        previous={r['discovery_id']:r for r in csv.DictReader(path.open(encoding='utf-8',newline=''))}
        for r in rows:
            old=previous.get(r['discovery_id'])
            if old and old.get('classification')!='UNREVIEWED':
                for k in ('primary_package','secondary_packages','assignment_basis','classification','canonical_contracts'): r[k]=old[k]
    with path.open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    lines=['# Bounded packet work packages','','Read ../PACKET_IMPLEMENTATION_PLAN.md first. Every numbered step below requires its own commit and progress update. All steps start NOT_STARTED; generated cards are specifications, not current status. IDs are stable; appended PK50 is scheduled by dependencies, not numeric order.','','Common source anchors for every card: rAthena src/map/clif_packetdb.hpp, packets.hpp, packets_struct.hpp, clif.cpp and the parser/emitter call tree; Ref/GameModePacket.cpp, packet.h and corresponding Session/actor/UI methods. Exact matching audit symbols/opcodes are listed in coverage.csv and inventory.json. Before implementation, write exact symbol/guard/field evidence into a contract. If a card is too broad, enumerate permanent child IDs before coding; each child gets a separate commit.','']
    for p in packages:
        lines += ['## '+p['id']+' — '+p['title'],'','Tier: **'+p['tier']+'**. Dependencies: '+(', '.join(p['dependencies']) or 'none')+'.', '', '### Bounded steps','']
        for s in p['steps']: lines+=['- **'+s['id']+' ['+s['tier']+']** '+s['action']+' **Checkpoint: evidence + tracker + separate commit + user progress update.**']
        lines+=['','### Required acceptance and traps','',p['acceptance'],'','### Discovery ownership','',str(sum(r['primary_package']==p['id'] for r in rows))+' provisional primary rows. Also review secondary assignments; zero primary rows does not waive a cross-cutting package. All applicable packets found while tracing the family must be added, even if absent from the seed list.']
        fs=[f for f in findings if owners[f['id']]['primary']==p['id'] or p['id'] in owners[f['id']]['secondary']]
        for f in fs:
            lines+=['','Finding '+f['id']+': '+f['title']+'.', '']
            for e in f['evidence']: lines+=['- Source anchor: `'+e['path']+'`:'+str(e.get('line',''))+'; search `'+str(e.get('needle',''))+'`. Recheck current line/guard.']
        lines+=['']
    (OUT/'WORK_PACKAGES.md').write_text('\n'.join(lines).rstrip()+'\n',encoding='utf-8')
    print(f'Generated {len(packages)} cards and {len(rows)} provisional discovery assignments; progress unchanged.')
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--refresh-coverage',action='store_true',help='Refresh discovery assignments and task cards without resetting state'); parser.parse_args(); generate()
