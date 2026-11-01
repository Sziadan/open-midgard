"""Check shipped coverage, critical prerequisites, and exporter import semantics."""
import importlib.util
import json
from pathlib import Path
import tempfile

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('exporter', root / 'tools/export_skill_tree.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)
catalog = json.loads((root / 'src/qtui/data/skill_tree_catalog.json').read_text())
assert catalog['schema'] == 1 and catalog['ruleset'] == 're'
assert len(catalog['sourceHash']) == 64
jobs = {job['id']: job for job in catalog['jobs']}
assert len(jobs) == 175 and sum(len(j['skills']) for j in jobs.values()) == 8122
assert any(job['name'] == 'Arch Mage' for job in jobs.values())  # Future job retained.
acolyte = {s['id']: s for s in jobs[4]['skills']}
assert acolyte[28]['cap'] == 10
assert acolyte[29]['requires'] == [{'id': 28, 'level': 3}]
assert acolyte[30]['requires'] == [{'id': 29, 'level': 1}]
assert 30 not in {s['id'] for s in jobs[6]['skills']}  # Thief cannot see Acolyte skills.
novice = {s['id']: s for s in jobs[0]['skills']}
assert novice[410]['symbol'] == 'WE_CALLBABY' and novice[410]['grantedOnly']
assert not novice[1]['grantedOnly']
priest = {s['id']: s for s in jobs[8]['skills']}
assert priest[1]['accessGroup'] == 0
assert priest[28]['accessGroup'] == 4
assert priest[74]['symbol'] == 'PR_MAGNIFICAT' and priest[74]['accessGroup'] == 8
for job in jobs.values():
    grants = {s['id'] for s in job['skills'] if s['grantedOnly']}
    # Filtering ungranted nodes must not break an ordinary skill's graph.
    for skill in job['skills']:
        if not skill['grantedOnly']:
            assert not any(req['id'] in grants for req in skill['requires'])

with tempfile.TemporaryDirectory() as temporary:
    server = Path(temporary)
    sources = {
        'src/common/mmo.hpp': 'enum e_job { JOB_NOVICE=0, JOB_ACOLYTE=4 };',
        'src/map/script_constants.hpp': '',
        'db/re/skill_db.yml': '  - Id: 28\n    Name: AL_HEAL\n    Description: Heal\n    TargetType: Support\n    Flags:\n      IsQuest: true\n      IsWedding: true\n',
        'db/import/skill_db.yml': '  - Id: 28\n    Description: Custom Heal\n    Flags:\n      IsQuest: false\n',
        'db/re/skill_tree.yml': '  - Job: Novice\n    Tree:\n      - Name: AL_HEAL\n        MaxLevel: 3\n  - Job: Acolyte\n    Inherit:\n      Novice: true\n',
        'db/import/skill_tree.yml': '  - Job: Acolyte\n    Tree:\n      - Name: AL_HEAL\n        MaxLevel: 7\n',
    }
    for filename, contents in sources.items():
        path = server / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
    exported = exporter.export(server, 're')
    skill = exported['jobs'][1]['skills'][0]
    assert skill['cap'] == 7 and skill['name'] == 'Custom Heal'
    assert not skill['quest'] and not skill['passive']
    assert skill['grantedOnly']  # Partial metadata override preserves the flag.
    tree = server / 'db/import/skill_tree.yml'
    tree.write_text(tree.read_text() + '        UnknownField: 1\n')
    try:
        exporter.export(server, 're')
    except ValueError as error:
        assert 'Unsupported tree syntax' in str(error)
    else:
        raise AssertionError('Unknown schema field was silently ignored')
print('Skill tree catalog and exporter checks passed')
