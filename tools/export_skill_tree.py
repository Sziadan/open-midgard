"""Export rAthena skill trees without a runtime dependency on the server checkout.

The reader intentionally handles the documented skill_tree.yml schema (not general
YAML). Unknown tree fields fail export rather than silently changing requirements.
"""
import argparse
import copy
import hashlib
import json
import re
import subprocess
from pathlib import Path


def trees(path):
    result = {}
    job = skill = requirement = None
    section = ''
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        line = line.split('#', 1)[0].rstrip()
        if not line.strip():
            continue
        match = re.match(r'^  - Job: (\w+)$', line)
        if match:
            job = result.setdefault(match[1], {'parents': {}, 'skills': {}})
            skill = requirement = None
            continue
        if job is None:
            continue
        if line.startswith('    Inherit:'):
            section = 'inherit'
        elif line.startswith('    Tree:'):
            section = 'tree'
        elif section == 'inherit' and (match := re.match(r'^      (\w+): (true|false)$', line)):
            job['parents'][match[1]] = match[2] == 'true'
        elif (match := re.match(r'^      - Name: (\w+)$', line)):
            skill = job['skills'].setdefault(match[1], {})
            requirement = None
        elif skill is not None and (match := re.match(r'^        (MaxLevel|BaseLevel|JobLevel|Exclude): (\w+)$', line)):
            skill[match[1]] = match[2] == 'true' if match[1] == 'Exclude' else int(match[2])
        elif line == '        Requires:':
            skill.setdefault('Requires', {})
        elif (match := re.match(r'^          - Name: (\w+)$', line)):
            requirement = match[1]
        elif requirement and (match := re.match(r'^            Level: (\d+)$', line)):
            skill['Requires'][requirement] = int(match[1])
        else:
            raise ValueError(f'Unsupported tree syntax in {path}: {line}')
    return result


def export(root, ruleset):
    paths = [root / 'src/common/mmo.hpp', root / 'src/map/script_constants.hpp',
             root / f'db/{ruleset}/skill_db.yml', root / 'db/import/skill_db.yml',
             root / f'db/{ruleset}/skill_tree.yml', root / 'db/import/skill_tree.yml']
    enum = re.search(r'enum e_job\s*\{(.*?)\};', paths[0].read_text(), re.S)[1]
    enum = re.sub(r'//[^\n]*|/\*.*?\*/', '', enum, flags=re.S)
    jobs, value = {}, -1
    for item in enum.split(','):
        item = item.strip()
        if not item:
            continue
        name, _, explicit = item.partition('=')
        value = int(explicit.strip(), 0) if explicit else value + 1
        jobs[name.strip().lower()] = value
    for alias, constant in re.findall(r'export_constant2\("(Job_\w+)",\s*(JOB_\w+)\)', paths[1].read_text()):
        jobs[alias.lower()] = jobs[constant.lower()]

    skills = {}
    for path in paths[2:4]:
        for chunk in re.split(r'(?m)^  - Id: ', path.read_text(encoding='utf-8-sig'))[1:]:
            sid = int(chunk.splitlines()[0].split("#", 1)[0].strip())
            fields = dict(re.findall(r'^    (Name|Description|MaxLevel|TargetType): (.+)$', chunk, re.M))
            old_symbol = next((k for k, v in skills.items() if v['id'] == sid), None)
            symbol = fields.get('Name', old_symbol)
            if symbol is None:
                raise ValueError(f'Missing name for new skill {sid} in {path}')
            meta = skills.pop(old_symbol, {}) if old_symbol else {}
            meta.update({'id': sid, 'name': fields.get('Description', meta.get('name', symbol)).strip('"\''),
                'passive': fields.get('TargetType', 'Passive' if meta.get('passive', True) else 'Attack') == 'Passive',
                'quest': meta.get('quest', False)})
            for flag, key in [('IsQuest', 'quest'), ('IsWedding', 'wedding'), ('IsSpirit', 'spirit')]:
                setting = re.search(r'^      ' + flag + r': (true|false)$', chunk, re.M)
                if setting:
                    meta[key] = setting[1] == 'true'
            meta['grantedOnly'] = meta.get('wedding', False) or meta.get('spirit', False)
            skills[symbol] = meta
    raw = trees(paths[4])
    for name, changes in trees(paths[5]).items():
        job = raw.setdefault(name, {'parents': {}, 'skills': {}})
        job['parents'].update(changes['parents'])
        for skill, fields in changes['skills'].items():
            base = job['skills'].setdefault(skill, {})
            requirements = {**base.get('Requires', {}), **fields.get('Requires', {})}
            base.update(fields)
            if requirements:
                base['Requires'] = requirements
    output = []
    for name, job in raw.items():
        combined = {}
        origins = {}
        # Match SkillTreeDatabase::loadingFinished: inherit each parent's own
        # declared entries, not an already recursively-expanded parent tree.
        for parent, enabled in job['parents'].items():
            if enabled:
                if parent not in raw:
                    raise ValueError(f'Missing inherited job {parent}')
                combined.update({k: copy.deepcopy(v) for k, v in raw[parent]['skills'].items() if not v.get('Exclude')})
                for symbol, entry in raw[parent]['skills'].items():
                    if entry.get('MaxLevel', 0) > 0 and not entry.get('Exclude'):
                        origins.setdefault(symbol, parent)
        combined.update(copy.deepcopy(job['skills']))
        for symbol in job['skills']:
            origins.setdefault(symbol, name)
        nodes = []
        for symbol, entry in combined.items():
            cap = entry.get('MaxLevel', 0)
            if not cap:
                continue
            meta = skills[symbol]
            requirements = [{'id': skills[k]['id'], 'level': v} for k, v in entry.get('Requires', {}).items() if v > 0]
            nodes.append({**{k: v for k, v in meta.items() if k not in ('wedding', 'spirit')}, 'symbol': symbol, 'cap': cap, 'baseLevel': entry.get('BaseLevel', 0),
                          'jobLevel': entry.get('JobLevel', 0), 'requires': requirements,
                          'accessGroup': jobs[('JOB_' + origins[symbol]).lower()]})
        jobid = jobs.get(('JOB_' + name).lower())
        if jobid is None:
            raise ValueError(f'Unknown job ID: {name}')
        by_id = {s['id']: s for s in nodes}
        def visit(sid, trail):
            if sid in trail:
                raise ValueError(f'Cycle in {name}: {trail + [sid]}')
            for req in by_id[sid]['requires']:
                if req['id'] not in by_id or req['level'] > by_id[req['id']]['cap']:
                    raise ValueError(f'Invalid prerequisite in {name}: {sid} -> {req}')
                visit(req['id'], trail + [sid])
        for sid in by_id:
            visit(sid, [])
        output.append({'id': jobid, 'name': name.replace('_', ' '), 'skills': nodes})
    digest = hashlib.sha256(b''.join(p.read_bytes() for p in paths)).hexdigest()
    try:
        revision = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        revision = 'unavailable; sourceHash identifies the copied inputs'
    return {'schema': 1, 'ruleset': ruleset, 'packetVersion': 20080910, 'source': 'rAthena (GPL-3.0-or-later)',
            'sourceRevision': revision, 'sourceHash': digest, 'jobs': output}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=Path)
    parser.add_argument('--ruleset', choices=['re', 'pre-re'], default='re')
    parser.add_argument('--output', type=Path, default=Path('src/qtui/data/skill_tree_catalog.json'))
    args = parser.parse_args()
    catalog = export(args.server, args.ruleset)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(catalog, ensure_ascii=True, separators=(',', ':')) + '\n')
    print(f"Exported {len(catalog['jobs'])} jobs and {sum(len(j['skills']) for j in catalog['jobs'])} job/skill entries ({args.ruleset})")
