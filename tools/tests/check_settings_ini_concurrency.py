import configparser
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def main():
    executable = Path(sys.argv[1]).resolve()
    parent = Path(sys.argv[2]).resolve()
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ini-concurrency-", dir=parent) as temp:
        directory = Path(temp)
        initial = directory / "initial"
        initial.mkdir()
        subprocess.run([str(executable), "initialize", str(initial)], cwd=initial, check=True, timeout=30)
        initial_config = configparser.ConfigParser()
        initial_config.read(initial / "open-midgard.ini", encoding="ascii")
        assert initial_config.getint("Graphics", "Width") == 1920
        assert initial_config.getint("Graphics", "Height") == 480
        ini = directory / "open-midgard.ini"
        ini.write_text(
            "[Sentinel]\nPreserve=123456\n[Debug]\nAssetLoadPerf=1\n"
            "[Graphics]\nWidth=1920\nHeight=1080\n[Padding]\n"
            + "".join(f"key{i:04d}={'x' * 80}\n" for i in range(500)),
            encoding="ascii",
        )
        workers = [
            subprocess.Popen(
                [str(executable), f"writer{i}", str(directory)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                cwd=directory,
            )
            for i in range(4)
        ]
        deadline = time.monotonic() + 30
        while not all((directory / f"writer{i}.ready").exists() for i in range(4)):
            if time.monotonic() >= deadline or any(worker.poll() is not None for worker in workers):
                raise RuntimeError("workers failed to reach the start barrier")
            time.sleep(0.01)
        (directory / "start").touch()
        for worker in workers:
            stdout, stderr = worker.communicate(timeout=120)
            if worker.returncode:
                raise RuntimeError(f"worker failed: {worker.returncode}\n{stdout}\n{stderr}")

        config = configparser.ConfigParser()
        config.read(ini, encoding="ascii")
        assert config.getint("Sentinel", "Preserve") == 123456
        assert config.getint("Debug", "AssetLoadPerf") == 1
        assert config.getint("Graphics", "Width") == 1920
        assert config.getint("Graphics", "Height") == 1080
        assert len(config["Padding"]) == 500
        for i in range(4):
            assert config.getint("Concurrent", f"writer{i}") == 79

        if os.name == "nt":
            subprocess.run([str(executable), "blocked", str(directory)], cwd=directory, check=True, timeout=30)
            config.read(ini, encoding="ascii")
            assert config.getint("Concurrent", "afterBlocked") == 1
            assert not config.has_option("Concurrent", "blocked")
            assert not config.has_option("Concurrent", "readBlocked")
            assert not list(directory.glob("omi*.tmp"))


if __name__ == "__main__":
    main()
