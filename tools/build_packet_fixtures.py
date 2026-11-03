import argparse, copy, hashlib, json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MANIFEST=ROOT/'docs/packets/evidence/PK02.02-contract-manifest.json'
OUT=ROOT/'docs/packets/evidence/PK02.03-fixtures'

# Values are synthetic. Every placement below is taken from the server parser
# registration or packed server structure recorded in the PK02.02 manifest.
def make(contract_id, fixture_id, stage, direction, profile, length, entries, rationale):
    buf=bytearray(length); fields=[]
    for name,offset,raw,value,basis in entries:
        if offset < 0 or offset+len(raw)>length: raise ValueError(name)
        buf[offset:offset+len(raw)]=raw
        fields.append({'name':name,'offset':offset,'width_bytes':len(raw),'expected_hex':raw.hex(),'decoded_value':value,'source_basis':basis})
    raw=bytes(buf)
    return {'fixture_id':fixture_id,'contract_id':contract_id,'provenance':rationale,'direction':direction,'stage':stage,'profile':profile,'expected_length':length,'hex_file':fixture_id+'.hex','sha256':hashlib.sha256(raw).hexdigest(),'synthetic_data':True,'fields':fields},raw

def fixtures():
    u16=lambda x:struct.pack('<H',x); u32=lambda x:struct.pack('<I',x); i16=lambda x:struct.pack('<h',x)
    return [
      make('PK01.P01.HANDSHAKE','P01-account-echo-synthetic','char','S2C',23,4,[('account_id',0,u32(0x11223344),'0x11223344','src/char/char_clif.cpp:850-853 raw account echo')],'No opcode; synthetic account ID, server echo field.'),
      make('PK01.P03.01F2','P03-guild-status-synthetic','map','S2C',23,20,[('PacketType',0,u16(0x01f2),'0x01F2','src/map/packets.hpp:868 opcode'),('aid',2,u32(0x11223344),'0x11223344','src/map/packets.hpp:870'),('cid',6,u32(0x55667788),'0x55667788','src/map/packets.hpp:871'),('status',10,u32(0xa1b2c3d4),'0xA1B2C3D4','src/map/packets.hpp:872'),('gender',14,u16(1),'1','src/map/packets.hpp:873'),('hairStyle',16,u16(12),'12','src/map/packets.hpp:874'),('hairColor',18,u16(3),'3','src/map/packets.hpp:875')],'Synthetic values packed using server PACKET_ZC_UPDATE_CHARSTAT field order and measured 20-byte size.'),
      make('PK01.P04.01A2-20080910','P04-pet-status-synthetic','map','S2C',23,35,[('PacketType',0,i16(0x01a2),'0x01A2','src/map/packets_struct.hpp:5442'),('szName',2,b'Mochi'+bytes(19),'Mochi; NUL padded to 24 bytes','src/map/packets_struct.hpp:5443 NAME_LENGTH=24'),('bModified',26,struct.pack('<b',1),'1','src/map/packets_struct.hpp:5444'),('nLevel',27,i16(25),'25','src/map/packets_struct.hpp:5445'),('nFullness',29,i16(80),'80','src/map/packets_struct.hpp:5446'),('nRelationship',31,i16(1000),'1000','src/map/packets_struct.hpp:5447'),('ITID',33,i16(0x1234),'0x1234','src/map/packets_struct.hpp:5448')],'Synthetic values packed using server PACKET_ZC_PROPERTY_PET fields; later job field is absent under PACKETVER=20080910.'),
      make('PK01.P05.0190','P05-action-0190-synthetic','map','C2S',22,19,[('opcode',0,u16(0x0190),'0x0190','src/map/clif_packetdb.hpp:881'),('target_id',5,u32(0x11223344),'0x11223344','final registration parser offset 5'),('action',18,bytes([7]),'7','final registration parser offset 18')],'Synthetic padded ActionRequest; parser-consumed positions are from final active registration.'),
      make('PK01.P05.007E','P05-skill-ground-text-synthetic','map','C2S',23,102,[('opcode',0,u16(0x007e),'0x007E','src/map/clif_packetdb.hpp:868'),('skill_level',5,u16(3),'3','final registration parser offset 5'),('skill_id',9,u16(0x0123),'0x0123','final registration parser offset 9'),('x',12,u16(100),'100','final registration parser offset 12'),('y',20,u16(200),'200','final registration parser offset 20'),('text',22,b'fixture'+bytes(73),'fixture; NUL padded to 80 bytes','final registration parser offset 22')],'Synthetic request payload; padding bytes are zero. Coordinates and text are synthetic, not live user data.'),
      make('PK01.P05.0437','P05-action-0437-synthetic','map','C2S',23,7,[('opcode',0,u16(0x0437),'0x0437','src/map/clif_packetdb.hpp:1149'),('target_id',2,u32(0x11223344),'0x11223344','final registration parser offset 2'),('action',6,bytes([7]),'7','final registration parser offset 6')],'Synthetic compact ActionRequest; parser-consumed positions are from final active registration.')]

def validate(manifest,doc,hex_map):
    contracts={c['contract_id']:c for c in manifest['contracts']}; seen=set()
    for f in doc['fixtures']:
        fid=f['fixture_id']
        if fid in seen: raise ValueError('duplicate fixture_id '+fid)
        seen.add(fid)
        if f['contract_id'] not in contracts: raise ValueError('unknown contract '+f['contract_id'])
        contract_fields={x['name']:x for x in contracts[f['contract_id']].get('fields',[])}
        for field in f['fields']:
            if field['name'] in contract_fields:
                source_field=contract_fields[field['name']]
                if field['offset']!=source_field['offset'] or field['width_bytes']!=source_field['width_bytes']:
                    raise ValueError('manifest offset/width mismatch '+fid+':'+field['name'])
        raw=hex_map[f['hex_file']]
        if len(raw)!=f['expected_length']: raise ValueError('wrong length '+fid)
        if hashlib.sha256(raw).hexdigest()!=f['sha256']: raise ValueError('wrong SHA '+fid)
        occupied=[]
        for field in f['fields']:
            a=field['offset']; n=field['width_bytes']; b=bytes.fromhex(field['expected_hex'])
            if n!=len(b) or a<0 or a+n>len(raw): raise ValueError('invalid bounds '+fid+':'+field['name'])
            if raw[a:a+n]!=b: raise ValueError('field slice mismatch '+fid+':'+field['name'])
            if any(i in occupied for i in range(a,a+n)): raise ValueError('overlapping fields '+fid)
            occupied.extend(range(a,a+n))
        if not f.get('synthetic_data'): raise ValueError('fixture not marked synthetic '+fid)
    if len(seen)!=len(doc['fixtures']): raise ValueError('duplicate IDs')
    return len(seen)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--self-test',action='store_true'); args=ap.parse_args()
    manifest=json.loads(MANIFEST.read_text(encoding='utf-8')); built=[make_data for make_data in fixtures()]
    records=[x[0] for x in built]; blobs={x[0]['hex_file']:x[1] for x in built}
    doc={'schema_version':1,'manifest_sha256':hashlib.sha256(MANIFEST.read_bytes()).hexdigest(),'fixtures':records}
    validate(manifest,doc,blobs)
    if args.self_test:
        bad=copy.deepcopy(doc); bad['fixtures'][0]['fields'][0]['expected_hex']='00'
        try: validate(manifest,bad,blobs)
        except (ValueError,KeyError): pass
        else: raise SystemExit('negative validator self-test unexpectedly passed')
    OUT.mkdir(parents=True,exist_ok=True)
    for name,data in blobs.items(): (OUT/name).write_text(data.hex()+'\n',encoding='ascii')
    (OUT/'PK02.03-fixtures.json').write_text(json.dumps(doc,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    saved=json.loads((OUT/'PK02.03-fixtures.json').read_text(encoding='utf-8'))
    saved_blobs={f['hex_file']:bytes.fromhex((OUT/f['hex_file']).read_text(encoding='ascii').strip()) for f in saved['fixtures']}
    print(f'PASS: {validate(manifest,saved,saved_blobs)} unique synthetic fixtures; manifest links, lengths, hashes, field slices and bounds valid'+('; negative validator self-test rejected corruption' if args.self_test else ''))
if __name__=='__main__': main()
