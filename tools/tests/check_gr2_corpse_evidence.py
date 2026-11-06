"""Check native corpse captures, production lifecycle receipts and PE counter goldens."""
import argparse
import json
import math
import re
import struct
from pathlib import Path
from PIL import Image, ImageChops, ImageStat


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--capture-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    directory = args.capture_dir
    manifest = json.loads((directory / 'run-manifest.json').read_text(encoding='utf-8-sig'))
    assert len(manifest) == 16
    assert len({run['executableSha256'] for run in manifest}) == 1
    background = Image.open(directory / '1286-602.bmp').convert('RGB')
    assert background.size == (640, 480)
    assert background.getextrema() == ((32, 32), (32, 32), (32, 32)), 'expired model left pixels'
    # Original PE old signed32 counter and byte subtraction, not a duration
    # inferred from the sprite branch or an expected value exported by C++.
    expected = {1: (255, 1), 356: (255, 1), 357: (244, 1), 500: (101, 1),
                600: (1, 1), 601: (0, 1), 602: (255, 0)}
    results = []
    for run in manifest:
        assert run['exit'] == 0
        job, count = run['job'], run['count']
        scene = run['scene']
        receipts = [json.loads(line) for line in (directory / (scene + '.stdout')).read_text().splitlines()
                    if line.startswith('{')]
        receipt = next(row for row in receipts if row['case_id'] == 'gr2_corpse_native')
        assert any(row.get('case_id') == 'corpse_packet_lifecycle' and row.get('status') == 'PASS'
                   for row in receipts)
        assert (receipt['job'], receipt['count']) == (job, count)
        assert receipt['age_ms'] == run['ageMilliseconds']
        assert (receipt['alpha'], receipt['alive']) == expected[count]
        assert (receipt['faces'] > 0) == bool(receipt['alive'])
        assert not receipt['alive'] or abs(receipt['clock'] - run['ageMilliseconds'] * .002) < .0001
        frame = Image.open(directory / (scene + '.bmp')).convert('RGB')
        assert frame.size == background.size
        difference = ImageChops.difference(frame, background)
        bbox = difference.getbbox()
        # Source flag0 is the renderer's opaque/cutout pass (alpha reference
        # 207). Low-alpha model pixels disappear before counter expiry.
        visible = count in (1, 356, 357)
        assert bool(bbox) == visible, (scene, bbox)
        result = dict(receipt, pixel_bbox=bbox,
                      contrast_sum=sum(ImageStat.Stat(difference).sum), pid=run['pid'])
        if receipt['alive']:
            # Inputs are skinned mesh coordinates, already covered by the
            # independent SDK pose/texture goldens. Recompute the corpse-only
            # source transform, projection/depth and view-normal shade here.
            log = (directory / 'stage' / f"debug_hp_{run['pid']}.log").read_text(encoding='cp1252')
            lines = [line for line in log.splitlines() if '[Gr2CorpseVertex]' in line]
            assert lines, scene
            fields = dict(re.findall(r'(\w+)=([^ ]+)', lines[-1]))
            values = {key: float(fields[key]) for key in ('rawx', 'rawy', 'rawz', 'nx', 'ny', 'nz', 'x', 'y', 'oow', 'z')}
            px, py, pz = values['rawx'], values['rawy'], values['rawz']
            px, py, pz = (px, -py, -pz) if job == 1324 else (px, -pz, py)
            angle = (receipt['rotation'] + 180) * math.pi / 180
            world = (px * math.cos(angle) + pz * math.sin(angle), py,
                     -px * math.sin(angle) + pz * math.cos(angle))
            c = struct.unpack('<f', struct.pack('<f', .70710678))[0]
            depth = c * world[1] + c * world[2] + 500
            reciprocal = 1 / depth
            expected_vertex = {'x': world[0] / depth * 1750 + 320,
                'y': (c * world[1] - c * world[2]) / depth * 1750 + 240,
                'oow': reciprocal, 'z': 1500 / 1490 * (1 - 10 * reciprocal)}
            for key, value in expected_vertex.items():
                tolerance = .005 if key in ('x', 'y') else .000001
                assert abs(value - values[key]) < tolerance, (scene, key, value, values[key])
            # Source PE's inverse-view components have sequential dependencies.
            eye_y = -800 / (2 * c)
            eye_z = -eye_y / (2 * c) - 500 / (2 * c)
            length = math.hypot(eye_y, eye_z)
            dot = (eye_y * values['ny'] + eye_z * values['nz']) / length
            shade = int(min(1, max(0, dot)) * 100 + 155)
            color = int(fields['color'], 16)
            assert color >> 24 == receipt['alpha']
            assert all(abs(((color >> shift) & 255) - shade) <= 1 for shift in (0, 8, 16)), scene
            result.update(independent_PE_geometry='PASS', expected_vertex=expected_vertex,
                          expected_shade=shade, mesh_input=values)
        results.append(result)
    assert {row['job'] for row in results} == {1285, 1286, 1287, 1324, 1288, 722, 1830}
    guardian_birth = Image.open(directory / '1286-1-age0.bmp').convert('RGB')
    guardian_progress = Image.open(directory / '1286-1-age250.bmp').convert('RGB')
    guardian_end = Image.open(directory / '1286-1.bmp').convert('RGB')
    assert ImageChops.difference(guardian_birth, guardian_progress).getbbox(), 'death animation did not advance'
    assert ImageChops.difference(guardian_progress, guardian_end).getbbox(), 'death animation did not reach its final pose'
    args.output.write_text(json.dumps({'cases': results, 'actual_gpu_readback': True,
        'counter_oracle': 'PE0x565C90 byte subtraction and old-counter comparison',
        'coverage': 'six model types, guardian alias, death handoff, private resources after cache teardown, counter/cutout/expiry, birth/progress/long gap and independent PE geometry'},
        indent=2) + '\n', encoding='utf-8')
    print('PASS sixteen native GR2 corpse cases: model types, production retirement, counter, cutout, expiry, animation and PE geometry')


if __name__ == '__main__':
    main()
