"""Check the production CP949 map-label conversion against known UTF-8 text."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from check_ammo_cursor_regressions import function

ROOT = Path(__file__).resolve().parents[2]


def literal(value):
    return '"' + ''.join(f'\\x{byte:02x}' for byte in value) + '"'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    compiler = shutil.which(args.cxx) or args.cxx
    source = (ROOT / 'src/ui/UISkillSelectWnd.cpp').read_text(encoding='utf-8')
    cases = ['', 'Prontera', '프론테라', '게펜 던전']
    checks = '\n'.join(f'assert(DecodeMapLabel({literal(text.encode("cp949"))}) == '
                       f'{literal(text.encode("utf-8"))});' for text in cases)
    with tempfile.TemporaryDirectory(prefix='ro_map_labels_') as directory:
        build = Path(directory)
        test = build / 'map_labels.cpp'
        test.write_text('#include <string>\n#include <cassert>\n'
                        '#ifdef _WIN32\n#include <windows.h>\n#else\n#include <iconv.h>\n#endif\n'
                        + function(source, 'std::string DecodeMapLabel(')
                        + '\nint main() {\n' + checks + '\n}\n', encoding='utf-8')
        executable = build / 'map_labels.exe'
        command = [compiler, '-std=c++17', '-UNDEBUG', str(test), '-o', str(executable)]
        if sys.platform == 'darwin':
            command.append('-liconv')
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['PATH'] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get('PATH', '')
        subprocess.run([str(executable)], check=True, env=environment)
    print('Production map-label conversion: empty, ASCII and Korean CP949 inputs passed')


if __name__ == '__main__':
    main()
