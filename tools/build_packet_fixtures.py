import argparse, copy, hashlib, json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MANIFEST=ROOT/'docs/packets/evidence/PK02.02-contract-manifest.json'
OUT=ROOT/'docs/packets/evidence/PK02.03-fixtures'
def E(n,o,b,v,s): return (n,o,b,v,s)
def make(cid,fid,stage,direction,profile,length,entries,why):
    buf=bytearray(length); fs=[]
    for n,o,b,v,basis in entries:
        if o<0 or o+len(b)>length: raise ValueError(n)
        buf[o:o+len(b)]=b
        fs.append({'name':n,'offset':o,'width_bytes':len(b),'expected_hex':b.hex(),'decoded_value':v,'source_basis':basis})
    raw=bytes(buf)
    return {'fixture_id':fid,'contract_id':cid,'provenance':why,'direction':direction,'stage':stage,'profile':profile,'expected_length':length,'hex_file':fid+'.hex','sha256':hashlib.sha256(raw).hexdigest(),'synthetic_data':True,'fields':fs},raw

def build():
    u16=lambda x:struct.pack('<H',x); u32=lambda x:struct.pack('<I',x); i16=lambda x:struct.pack('<h',x)
    z=lambda n:bytes(n)
    return [
      make('PK01.P01.HANDSHAKE','P01-account-echo-synthetic','char','S2C',23,4,[E('account_id',0,u32(0x11223344),'0x11223344','src/char/char_clif.cpp:850-853')],'Synthetic raw account echo; no opcode.'),
      make('PK01.P03.01F2','P03-guild-status-synthetic','map','S2C',23,20,[E('PacketType',0,u16(0x01f2),'0x01F2','src/map/packets.hpp:868'),E('aid',2,u32(0x11223344),'0x11223344','src/map/packets.hpp:870'),E('cid',6,u32(0x55667788),'0x55667788','src/map/packets.hpp:871'),E('status',10,u32(0xa1b2c3d4),'0xA1B2C3D4 synthetic raw marker; valid status semantics not asserted','src/map/packets.hpp:872'),E('gender',14,u16(1),'1 synthetic','src/map/packets.hpp:873'),E('hairStyle',16,u16(12),'12 synthetic','src/map/packets.hpp:874'),E('hairColor',18,u16(3),'3 synthetic','src/map/packets.hpp:875')],'Synthetic packed layout sample; status value is a raw marker and is not asserted valid for online/offline state.'),
      make('PK01.P04.01A2-20080910','P04-pet-status-synthetic','map','S2C',23,35,[E('PacketType',0,i16(0x01a2),'0x01A2','src/map/packets_struct.hpp:5442'),E('szName',2,b'Mochi'+z(19),'Mochi; NUL padded to 24 bytes','src/map/packets_struct.hpp:5443'),E('bModified',26,struct.pack('<b',1),'1 synthetic','src/map/packets_struct.hpp:5444'),E('nLevel',27,i16(25),'25 synthetic','src/map/packets_struct.hpp:5445'),E('nFullness',29,i16(80),'80 synthetic','src/map/packets_struct.hpp:5446'),E('nRelationship',31,i16(1000),'1000 synthetic','src/map/packets_struct.hpp:5447'),E('ITID',33,i16(0x1234),'0x1234 synthetic','src/map/packets_struct.hpp:5448')],'Synthetic 35-byte packed layout; the later job field is absent for PACKETVER=20080910.'),
      make('PK01.P05.0190','P05-action-0190-synthetic','map','C2S',22,19,[E('opcode',0,u16(0x0190),'0x0190','src/map/clif_packetdb.hpp:881'),E('padding_2_5',2,z(3),'zero-filled synthetic padding','src/map/clif_packetdb.hpp:881 accepted length'),E('target_id',5,u32(0x11223344),'0x11223344','src/map/clif_packetdb.hpp:881 parser offset 5'),E('padding_9_18',9,z(9),'zero-filled synthetic padding','src/map/clif_packetdb.hpp:881 accepted length'),E('action',18,bytes([7]),'7 synthetic','src/map/clif_packetdb.hpp:881 parser offset 18')],'Synthetic padded ActionRequest; target ID and action use final parser offsets, padding is a chosen zero pattern.'),
      make('PK01.P05.007E','P05-skill-ground-text-synthetic','map','C2S',23,102,[E('opcode',0,u16(0x007e),'0x007E','src/map/clif_packetdb.hpp:868'),E('padding_2_5',2,z(3),'zero-filled synthetic padding','src/map/clif_packetdb.hpp:868 accepted length'),E('skill_level',5,u16(3),'3 synthetic','src/map/clif_packetdb.hpp:868 parser offset 5'),E('padding_7_9',7,z(2),'zero-filled synthetic padding','src/map/clif_packetdb.hpp:868 accepted length'),E('skill_id',9,u16(0x0123),'0x0123 synthetic','src/map/clif_packetdb.hpp:868 parser offset 9'),E('padding_11_12',11,z(1),'zero-filled synthetic padding','src/map/clif_packetdb.hpp:868 accepted length'),E('x',12,u16(100),'100 wire bits; consumer interprets int16','src/map/clif_packetdb.hpp:868 parser offset 12'),E('padding_14_20',14,z(6),'zero-filled synthetic padding','src/map/clif_packetdb.hpp:868 accepted length'),E('y',20,u16(200),'200 wire bits; consumer interprets int16','src/map/clif_packetdb.hpp:868 parser offset 20'),E('text',22,b'fixture'+z(73),'fixture; NUL padded to 80 bytes','src/map/clif_packetdb.hpp:868 parser offset 22')],'Synthetic request; padding is zero by fixture choice. Text is a short ASCII example, not a general encoding claim.'),
      make('PK01.P05.0437','P05-action-0437-synthetic','map','C2S',23,7,[E('opcode',0,u16(0x0437),'0x0437','src/map/clif_packetdb.hpp:1149'),E('target_id',2,u32(0x11223344),'0x11223344 synthetic','src/map/clif_packetdb.hpp:1149 parser offset 2'),E('action',6,bytes([7]),'7 synthetic','src/map/clif_packetdb.hpp:1149 parser offset 6')],'Synthetic compact ActionRequest; parser-consumed fields use final active offsets.')]

def validate(manifest,doc,hex_map,manifest_bytes):
    if doc.get('manifest_sha256')!=hashlib.sha256(manifest_bytes).hexdigest(): raise ValueError('stale manifest hash')
    contracts={c['contract_id']:c for c in manifest['contracts']}; seen=set(); covered_contracts=set()
    if doc.get('schema_version')!=1: raise ValueError('unsupported fixture schema')
    for f in doc.get('fixtures',[]):
        fid=f['fixture_id']
        if fid in seen: raise ValueError('duplicate fixture_id '+fid)
        seen.add(fid)
        if f['contract_id'] not in contracts: raise ValueError('unknown contract '+f['contract_id'])
        c=contracts[f['contract_id']]
        covered_contracts.add(f['contract_id'])
        if (f['stage'],f['direction'],f['profile'])!=(c['stage'],c['direction'],c['client_profile']): raise ValueError('stage/direction/profile mismatch '+fid)
        if not f.get('synthetic_data'): raise ValueError('fixture not marked synthetic '+fid)
        if 'client serializer' in f['provenance'].lower() or 'client struct' in f['provenance'].lower(): raise ValueError('client-derived provenance '+fid)
        declared=c.get('fields',[]); actual=f.get('fields',[])
        if [(x['name'],x['offset'],x['width_bytes']) for x in actual] != [(x['name'],x['offset'],x['width_bytes']) for x in declared]: raise ValueError('required fields/offsets do not match manifest '+fid)
        length=f['expected_length']; reg=c.get('registration') or {}
        expected=reg.get('accepted_length',reg.get('emitted_length'))
        if expected is not None and length!=expected: raise ValueError('contract length mismatch '+fid)
        raw=hex_map[f['hex_file']]
        if len(raw)!=length: raise ValueError('wrong length '+fid)
        if hashlib.sha256(raw).hexdigest()!=f['sha256']: raise ValueError('wrong SHA '+fid)
        occupied=[]
        for field in actual:
            a=field['offset']; n=field['width_bytes']; b=bytes.fromhex(field['expected_hex'])
            if n!=len(b) or a<0 or a+n>len(raw): raise ValueError('invalid bounds '+fid+':'+field['name'])
            if raw[a:a+n]!=b: raise ValueError('field slice mismatch '+fid+':'+field['name'])
            if any(i in occupied for i in range(a,a+n)): raise ValueError('overlap '+fid)
            occupied.extend(range(a,a+n))
            if not field.get('source_basis') or 'client' in field['source_basis'].lower(): raise ValueError('unsupported field provenance '+fid)
        if len(occupied)!=length: raise ValueError('field map has gap '+fid)
        op=c.get('opcode')
        if op is not None:
            opfield=next((x for x in actual if x['name'] in ('opcode','PacketType')),None)
            if opfield is None or int.from_bytes(bytes.fromhex(opfield['expected_hex']),'little')!=int(op,16): raise ValueError('opcode mismatch '+fid)
    required={'PK01.P01.HANDSHAKE','PK01.P03.01F2','PK01.P04.01A2-20080910','PK01.P05.0190','PK01.P05.007E','PK01.P05.0437'}
    if covered_contracts!=required: raise ValueError('fixture suite coverage mismatch')
    return len(seen)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--self-test',action='store_true'); args=ap.parse_args()
    manifest_bytes=MANIFEST.read_bytes(); manifest=json.loads(manifest_bytes)
    built=build(); records=[x[0] for x in built]; blobs={x[0]['hex_file']:x[1] for x in built}
    doc={'schema_version':1,'manifest_sha256':hashlib.sha256(manifest_bytes).hexdigest(),'fixtures':records}
    validate(manifest,doc,blobs,manifest_bytes)
    if args.self_test:
        corruptions=[]
        x=copy.deepcopy(doc);x['manifest_sha256']='0'*64;corruptions.append(x)
        x=copy.deepcopy(doc);x['fixtures'][0]['direction']='C2S';corruptions.append(x)
        x=copy.deepcopy(doc);x['fixtures'][0]['fields'].pop();corruptions.append(x)
        x=copy.deepcopy(doc);x['fixtures'][-1]['expected_length']=8;corruptions.append(x)
        x=copy.deepcopy(doc);x['fixtures'][-1]['fields'][0]['expected_hex']='3804';corruptions.append(x)
        x=copy.deepcopy(doc);x['fixtures'].pop();corruptions.append(x)
        x=copy.deepcopy(doc);x['fixtures']=[];corruptions.append(x)
        for bad in corruptions:
            try: validate(manifest,bad,blobs,manifest_bytes)
            except (ValueError,KeyError): continue
            raise SystemExit('negative validator self-test unexpectedly passed')
    OUT.mkdir(parents=True,exist_ok=True)
    for name,data in blobs.items(): (OUT/name).write_text(data.hex()+'\n',encoding='ascii')
    (OUT/'PK02.03-fixtures.json').write_text(json.dumps(doc,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    saved=json.loads((OUT/'PK02.03-fixtures.json').read_text(encoding='utf-8'))
    saved_blobs={f['hex_file']:bytes.fromhex((OUT/f['hex_file']).read_text(encoding='ascii').strip()) for f in saved['fixtures']}
    count=validate(manifest,saved,saved_blobs,manifest_bytes)
    print(f'PASS: {count} fixtures; manifest, contract metadata, complete fields, lengths, hashes, slices, provenance and opcodes valid'+('; seven negative mutations rejected' if args.self_test else ''))
if __name__=='__main__': main()
