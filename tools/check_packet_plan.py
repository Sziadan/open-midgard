"""Validate planning coverage/checkpoints; does not certify packet implementations."""
import csv,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; D=ROOT/'docs/packets'
def load(p): return json.loads(p.read_text(encoding='utf-8'))
def require(ok,msg):
    if not ok: raise SystemExit('FAIL: '+msg)
pkgs=load(D/'work-packages.json'); ids={p['id'] for p in pkgs}
require(len(ids)==len(pkgs),'duplicate package IDs')
steps={s['id']:s for p in pkgs for s in p['steps']}
require(len(steps)==sum(len(p['steps']) for p in pkgs),'duplicate step IDs')
for p in pkgs:
    require(p['tier'] in ('S','M','L') and p['acceptance'] and p['steps'],'missing card requirements '+p['id'])
    require(all(x in ids for x in p['dependencies']),'unknown dependency '+p['id'])
    require(p['steps'][0]['tier']=='S','missing strong contract gate '+p['id'])
    require(all(s['action'] and s['tier'] in ('S','M','L') for s in p['steps']),'incomplete step '+p['id'])
visiting=set(); seen=set(); byid={p['id']:p for p in pkgs}
def visit(k):
    require(k not in visiting,'dependency cycle '+k)
    if k in seen:return
    visiting.add(k)
    for dep in byid[k]['dependencies']:visit(dep)
    visiting.remove(k);seen.add(k)
for k in ids:visit(k)
rows=load(ROOT/'docs/packet-audit/inventory.json')['rows']
cov=list(csv.DictReader((D/'coverage.csv').open(encoding='utf-8',newline='')))
require(len(cov)==len(rows),'coverage row count differs from audit')
require({r['discovery_id'] for r in cov}=={'AUDIT-'+r['opcode'] for r in rows},'missing/extra discovery IDs')
require(len({r['discovery_id'] for r in cov})==len(cov),'duplicate coverage IDs')
for r in cov:
    require(r['primary_package'] in ids,'invalid primary owner')
    require(all(x in ids for x in r['secondary_packages'].split(';') if x),'invalid secondary owner')
    require(bool(r['assignment_basis']),'missing ownership basis')
    if r['classification']!='UNREVIEWED':
        require(bool(r['canonical_contracts']),'adjudicated row lacks canonical contract/proof '+r['discovery_id'])
f=load(ROOT/'docs/packet-audit/findings.json'); fo=load(D/'finding-ownership.json')
require(set(fo)=={x['id'] for x in f},'headline finding ownership incomplete')
require(all(v['primary'] in ids and all(x in ids for x in v['secondary']) for v in fo.values()),'bad finding owner')
io=load(D/'ignored-ownership.json')
ignored={r['opcode'] for r in rows if r.get('client_handler')=='HandleIgnorePacket' and r['stage']=='map'}
require(set(io)==ignored,'ignored registration ownership differs from actual handlers: '+str(set(io)^ignored))
require(all(x in ids for x in io.values()),'invalid ignored owner')
state=load(D/'state.json');require(set(state['steps'])==set(steps),'state steps out of sync')
allowed={'NOT_STARTED','IN_PROGRESS','BLOCKED','READY_TO_COMMIT','COMPLETED'}
for k,v in state['steps'].items():
    require(v['status'] in allowed,'bad step state '+k)
    if v['status']=='COMPLETED':
        require(bool(v['evidence']) and bool(v['commit_subject']),'missing completion evidence '+k)
        for ep in v['evidence']: require((ROOT/ep).is_file(),'missing evidence '+ep)
        subjects=subprocess.check_output(['git','log','--format=%s'],cwd=ROOT,text=True).splitlines()
        require(v['commit_subject'] in subjects,'completion commit absent '+k)
require(state['active_step'] is None or state['active_step'] in steps,'invalid active step')
for name in ('STATUS.md','PROGRESS.md','HANDOFF.md','BLOCKED.md','CONTRACT_TEMPLATE.md','EVIDENCE_TEMPLATE.md','WORK_ITEM_TEMPLATE.md','WORK_PACKAGES.md','source-baseline.json'):
    require((D/name).is_file() and (D/name).stat().st_size>100,'missing required deliverable '+name)
cards=(D/'WORK_PACKAGES.md').read_text(encoding='utf-8')
require(all('**'+k+' [' in cards for k in steps),'generated cards stale/missing steps')
require(cards.count('Checkpoint: evidence + tracker + separate commit + user progress update.')==len(steps),'missing per-step checkpoint')
# Local relative Markdown links in the stable plan must resolve.
import re
plan=(ROOT/'docs/PACKET_IMPLEMENTATION_PLAN.md').read_text(encoding='utf-8')
for link in re.findall(r'\]\(([^)]+)\)',plan):
    if '://' not in link: require((ROOT/'docs'/link.split('#')[0]).exists(),'broken plan link '+link)
print(f'PASS: {len(pkgs)} packages, {len(steps)} numbered steps, {len(cov)} discovery owners, {len(fo)} findings, {len(io)} ignored registrations; DAG/tracker/checkpoints/links valid.')
print('Planning consistency only; no runtime parity or build certification.')
