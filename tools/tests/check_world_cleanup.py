"""Compile the current world teardown against ownership-aware test objects."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from check_ammo_cursor_regressions import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    world = (ROOT / 'src/world/World.cpp').read_text(encoding='utf-8')
    body = function(world, 'void CWorld::ClearFixedObjects(')
    with tempfile.TemporaryDirectory(prefix='ro_world_cleanup_') as directory:
        build = Path(directory)
        (build / 'WorldCleanupBody.inc').write_text(body, encoding='utf-8')
        executable = build / 'world_cleanup_checks.exe'
        command = [args.cxx, '-std=c++17', '-UNDEBUG', f'-I{build}',
                   str(ROOT / 'tools/tests/WorldCleanupTests.cpp'), '-o', str(executable)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(args.cxx).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)


if __name__ == '__main__':
    main()
