import csv, hashlib, json, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SERVER=Path('C:/Users/sziad/source/repos/rathena-master')
BUILD=ROOT/'build-mingw-preset/packet-audit'
BASE=json.loads((ROOT/'docs/packets/source-baseline.json').read_text(encoding='utf-8'))
for rel, expected in BASE['server_source_sha256'].items():
    actual=hashlib.sha256((SERVER/rel).read_bytes()).hexdigest()
    if actual.lower()!=expected.lower(): raise SystemExit(f'source hash mismatch: {rel}: {actual}')
ASSIGN_PATH=ROOT/'docs/packets/evidence/PK01.02-server-registry-assignments.txt'
FINAL_PATH=ROOT/'docs/packets/evidence/PK01.02-server-registry-final.csv'
assign=[line.split('|') for line in ASSIGN_PATH.read_text(encoding='utf-8').splitlines()]
final=list(csv.DictReader(FINAL_PATH.open(encoding='utf-8')))

def fields(entries):
    out=[]
    for n,o,w,s,p,ln,g,m in entries:
        item={'name':n,'offset':o,'width_bytes':w,'signed':s,'endian':'little','source_path':p,'source_line':ln,'guard':g,'meaning':m,'semantic_type':'wire field; feature meaning not adjudicated here','units':'unresolved pending semantic contract','validity':'unresolved pending semantic contract'}
        if 'padding' in n: item.update({'semantic_type':'opaque padding bytes','validity':'padding byte convention unresolved'})
        if n in ('target_id',): item['consumer_signed']=True
        if n in ('x','y'): item['consumer_signed']=True
        out.append(item)
    return out
def registration(op):
    rows=[r for r in final if r['opcode'].lower()==op.lower()]
    if len(rows)!=1: raise SystemExit(f'expected one final registration for {op}')
    r=rows[0]
    return {'kind':'parser_registration','guard':'effective PACKETVER=20080910; source-order last assignment wins','source_path':'src/map/clif_packetdb.hpp','source_line':int(r['source_line']),'accepted_length':int(r['length']),'parser':r['parser'],'field_positions':[int(x) for x in r['field_positions'].split(',') if x and int(x)!=0], 'assignment_count':sum(a[0].lower()==op.lower() for a in assign)}
def c2s(opcode, length, pos, name, discovery, profile, layout_variant, entries, line):
    return {'contract_id':name,'discovery_parents':[discovery],'stage':'map','direction':'C2S','server_packetver':20080910,'client_profile':profile,'opcode':opcode,'layout_variant':layout_variant,'applicability_status':'approved_for_applicability_only','registration':registration(opcode[2:]),'fields':fields(entries),'probe':{'kind':'compiled_registry','accepted_length':length,'parser_positions':pos}}

contracts=[
 {'contract_id':'PK01.P01.HANDSHAKE','discovery_parents':['AUDIT-0x8482','AUDIT-0x8483'],'stage':'char','direction':'S2C','server_packetver':20080910,'client_profile':23,'opcode':None,'layout_variant':'raw four-byte account ID echo','applicability_status':'approved_for_applicability_only','registration':{'kind':'raw_handshake','source_path':'src/char/char_clif.cpp','source_line':850,'symbol':'chclif_parse_reqtoconnect','emitted_length':4},'fields':fields([('account_id',0,4,False,'src/char/char_clif.cpp',850,'raw handshake','echoed account identifier')]),'probe':{'kind':'source_field','result':'raw prefix, not opcode registration'}},
 {'contract_id':'PK01.P02.FRAMING','discovery_parents':[],'stage':'login/char/map connections','direction':'receive framing','server_packetver':20080910,'client_profile':23,'opcode':None,'layout_variant':'cross-stage receive state machine','applicability_status':'approved_for_applicability_boundary_only','registration':None,'fields':[],'probe':{'kind':'not_applicable','result':'state machine has no fixed byte layout'}},
 {'contract_id':'PK01.P03.01F2','discovery_parents':['AUDIT-0x01F2'],'stage':'map','direction':'S2C','server_packetver':20080910,'client_profile':23,'opcode':'0x01F2','layout_variant':'PACKET_ZC_UPDATE_CHARSTAT under defined(PACKETVER)','applicability_status':'approved_for_applicability_only','registration':{'kind':'emitter','source_path':'src/map/clif.cpp','source_line':8682,'symbol':'clif_guild_memberlogin_notice','emitted_length':20},'fields':fields([('PacketType',0,2,False,'src/map/packets.hpp',868,'defined(PACKETVER)','opcode'),('aid',2,4,False,'src/map/packets.hpp',870,'defined(PACKETVER)','account ID'),('cid',6,4,False,'src/map/packets.hpp',871,'defined(PACKETVER)','character ID'),('status',10,4,False,'src/map/packets.hpp',872,'defined(PACKETVER)','status'),('gender',14,2,False,'src/map/packets.hpp',873,'defined(PACKETVER)','gender'),('hairStyle',16,2,False,'src/map/packets.hpp',874,'defined(PACKETVER)','hair style'),('hairColor',18,2,False,'src/map/packets.hpp',875,'defined(PACKETVER)','hair color')]),'probe':{'kind':'compiled_sizeof_offsetof','record':'PACKET_ZC_UPDATE_CHARSTAT'}},
 {'contract_id':'PK01.P04.01A2-20080910','discovery_parents':['AUDIT-0x01A2'],'stage':'map','direction':'S2C','server_packetver':20080910,'client_profile':23,'opcode':'0x01A2','layout_variant':'35-byte PACKET_ZC_PROPERTY_PET','applicability_status':'approved_for_applicability_only','registration':{'kind':'emitter','source_path':'src/map/clif.cpp','source_line':8335,'symbol':'clif_send_petstatus','emitted_length':35},'fields':fields([('PacketType',0,2,True,'src/map/packets_struct.hpp',5442,'PACKETVER < 20081126','opcode'),('szName',2,24,None,'src/map/packets_struct.hpp',5443,'PACKETVER < 20081126','name bytes'),('bModified',26,1,True,'src/map/packets_struct.hpp',5444,'PACKETVER < 20081126','rename flag'),('nLevel',27,2,True,'src/map/packets_struct.hpp',5445,'PACKETVER < 20081126','level'),('nFullness',29,2,True,'src/map/packets_struct.hpp',5446,'PACKETVER < 20081126','fullness'),('nRelationship',31,2,True,'src/map/packets_struct.hpp',5447,'PACKETVER < 20081126','relationship'),('ITID',33,2,True,'src/map/packets_struct.hpp',5448,'PACKETVER < 20081126','accessory item ID')]),'probe':{'kind':'compiled_sizeof_offsetof','record':'PACKET_ZC_PROPERTY_PET'}},
 c2s('0x0190',19,[5,18],'PK01.P05.0190','AUDIT-0x0190',22,'19-byte padded ActionRequest',[('opcode',0,2,False,'src/map/clif_packetdb.hpp',881,'effective registration','opcode'),('padding_2_5',2,3,None,'src/map/clif_packetdb.hpp',881,'opaque padding','padding'),('target_id',5,4,False,'src/map/clif_packetdb.hpp',881,'effective registration','action target'),('padding_9_18',9,9,None,'src/map/clif_packetdb.hpp',881,'opaque padding','padding'),('action',18,1,False,'src/map/clif_packetdb.hpp',881,'effective registration','action kind')],881),
 c2s('0x007E',102,[5,9,12,20,22],'PK01.P05.007E','AUDIT-0x007E',23,'102-byte UseSkillToPosMoreInfo',[('opcode',0,2,False,'src/map/clif_packetdb.hpp',868,'effective registration','opcode'),('padding_2_5',2,3,None,'src/map/clif_packetdb.hpp',868,'opaque padding','padding'),('skill_level',5,2,False,'src/map/clif_packetdb.hpp',868,'effective registration','skill level'),('padding_7_9',7,2,None,'src/map/clif_packetdb.hpp',868,'opaque padding','padding'),('skill_id',9,2,False,'src/map/clif_packetdb.hpp',868,'effective registration','skill ID'),('padding_11_12',11,1,None,'src/map/clif_packetdb.hpp',868,'opaque padding','padding'),('x',12,2,False,'src/map/clif_packetdb.hpp',868,'effective registration','target X'),('padding_14_20',14,6,None,'src/map/clif_packetdb.hpp',868,'opaque padding','padding'),('y',20,2,False,'src/map/clif_packetdb.hpp',868,'effective registration','target Y'),('text',22,80,None,'src/map/clif_packetdb.hpp',868,'effective registration','skill text')],868),
 c2s('0x0437',7,[2,6],'PK01.P05.0437','AUDIT-0x0437',23,'7-byte ActionRequest',[('opcode',0,2,False,'src/map/clif_packetdb.hpp',1149,'PACKETVER >= 20080910','opcode'),('target_id',2,4,False,'src/map/clif_packetdb.hpp',1149,'effective registration','action target'),('action',6,1,False,'src/map/clif_packetdb.hpp',1149,'effective registration','action kind')],1149)
]
# compile and execute the selected-layout probe each time the manifest is generated.
compiler=Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
cpp=ROOT/'tools/packet_contract_layout_probe.cpp'; exe=BUILD/'p0_layout_probe.exe'; BUILD.mkdir(parents=True,exist_ok=True)
header_source=(SERVER/'src/map/packets.hpp').read_text(encoding='utf-8')
header=header_source.replace('#include \"map.hpp\"','#include <common/mmo.hpp>\n#define MESSAGE_SIZE 80\n#define TALKBOX_MESSAGE_SIZE 80\n#define MAX_ACHIEVEMENT_OBJECTIVES 10\n#define MAX_ITEM_RDM_OPT 5')
header=header.replace('#include \"packets_struct.hpp\"','#include \"'+(SERVER/'src/map/packets_struct.hpp').as_posix()+'\"')
if header==header_source or 'MAX_ITEM_RDM_OPT 5' not in header: raise SystemExit('server packet header substitutions failed')
(BUILD/'server_packets.hpp').write_text(header,encoding='utf-8')
inc=[SERVER/'src',SERVER/'3rdparty',SERVER/'3rdparty/libconfig']
cmd=[str(compiler),'-std=c++17','-I'+str(BUILD),*['-I'+str(x) for x in inc],str(cpp),'-o',str(exe)]
compile_result=subprocess.run(cmd,capture_output=True,text=True)
if compile_result.returncode: raise SystemExit(compile_result.stderr)
run_result=subprocess.run([str(exe)],capture_output=True,text=True)
if run_result.returncode: raise SystemExit(run_result.stderr)
probe=run_result.stdout.splitlines()
expected_defines=['PACKETVER=20080910','PACKETVER_SAK_NUM=20080910','PACKETVER_MAIN_NUM=20080910']
if probe[:3]!=expected_defines: raise SystemExit(f'effective packet macros differ: {probe[:3]}')
layout={}
for line in probe:
    m=re.match(r'(PACKET_\w+) sizeof=(\d+) (.*)',line)
    if m: layout[m.group(1)]={'sizeof':int(m.group(2)),'offsets':{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',m.group(3))}}
for c in contracts:
    name=c.get('probe',{}).get('record')
    if name:
        measured=layout.get(name)
        if measured is None: raise SystemExit(f'missing compiled layout {name}')
        if measured['sizeof'] != c['registration']['emitted_length']: raise SystemExit(f'compiled sizeof mismatch {name}')
        for field in c['fields']:
            if measured['offsets'].get(field['name']) != field['offset']: raise SystemExit('compiled offset mismatch '+name+'.'+field['name'])
        c['probe']['result']=measured
    elif c['direction']=='C2S':
        reg=c['registration']; fields_to_read=[f for f in c['fields'] if f['name'] not in ('opcode',) and not f['name'].startswith('padding_')]
        if reg['accepted_length'] != sum(f['width_bytes'] for f in c['fields']): raise SystemExit('length/field coverage mismatch '+c['contract_id'])
        if reg['field_positions'] != [f['offset'] for f in fields_to_read]: raise SystemExit('parser offsets mismatch '+c['contract_id']+': '+str(reg['field_positions'])+' vs '+str([f['offset'] for f in fields_to_read]))
version=subprocess.run([str(compiler),'--version'],capture_output=True,text=True).stdout.splitlines()[0]
manifest={'schema_version':1,'baseline':{'client_commit':BASE['client_head'],'server_root':BASE['server_root'],'server_revision':BASE['server_git_revision'],'server_snapshot_state':'unversioned; no .git metadata','effective_defines':{'PACKETVER':20080910,'PACKETVER_SAK_NUM':20080910,'PACKETVER_MAIN_NUM':'PACKETVER'},'deployed_server_provenance':'unknown'},'source_sha256':{k:v.lower() for k,v in sorted(BASE['server_source_sha256'].items())},'contracts':sorted(contracts,key=lambda x:x['contract_id']),'registry_probe':{'active_assignments':len(assign),'final_registrations':len(final),'assignment_file_sha256':hashlib.sha256(ASSIGN_PATH.read_bytes()).hexdigest(),'final_file_sha256':hashlib.sha256(FINAL_PATH.read_bytes()).hexdigest(),'generator':'tools/audit_packets.py','command':'python tools/audit_packets.py --server C:/Users/sziad/source/repos/rathena-master'},'layout_probe':{'source':'tools/packet_contract_layout_probe.cpp','source_sha256':hashlib.sha256(cpp.read_bytes()).hexdigest(),'generated_header_sha256':hashlib.sha256((BUILD/'server_packets.hpp').read_bytes()).hexdigest(),'header_source_sha256':hashlib.sha256((SERVER/'src/map/packets.hpp').read_bytes()).hexdigest(),'struct_header_source_sha256':hashlib.sha256((SERVER/'src/map/packets_struct.hpp').read_bytes()).hexdigest(),'command':' '.join(cmd),'compiler':version,'compile_exit_code':compile_result.returncode,'run_exit_code':run_result.returncode,'output':probe}}
manifest['generator']={'path':'tools/build_packet_contract_manifest.py','source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'command':'python tools/build_packet_contract_manifest.py'}
out=ROOT/'docs/packets/evidence/PK02.02-contract-manifest.json'
out.write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')


