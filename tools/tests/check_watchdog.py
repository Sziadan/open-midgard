"""Build and exercise the actual watchdog lifecycle on Windows or Linux."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.cxx) or args.cxx
    with tempfile.TemporaryDirectory(prefix='ro_watchdog_') as directory:
        build = Path(directory)
        executable = build / 'watchdog_checks.exe'
        command = [compiler, '-std=c++17', '-UNDEBUG', '-pthread', f'-I{ROOT / "src"}']
        if os.name != 'nt':
            command += [f'-I{ROOT / "src/platform/win32"}']
        command += [str(ROOT / 'tools/tests/MainLoopWatchdogTests.cpp'),
                    str(ROOT / 'src/core/MainLoopWatchdog.cpp'), '-o', str(executable)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, cwd=build, env=environment, timeout=20)
    print('Production watchdog: start/stop, duplicate calls, prompt shutdown and restart passed')


if __name__ == '__main__':
    main()
