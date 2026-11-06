"""Validate actual Vulkan captures against original ACT bytes and PE equations.

Requires Pillow. Captures are produced by the opt-in built-client corpse preview,
using an isolated settings/data directory; this script never launches a client.
"""
import argparse
import json
import math
import re
import struct
from pathlib import Path, PureWindowsPath
from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    runs = {}
    for log in sorted((args.capture_dir / "stage").glob("debug_hp_*.log"), key=lambda p: p.stat().st_mtime):
        text = log.read_text(encoding="cp1252", errors="replace")
        records = list(re.finditer(r"\[CorpsePreview\] age=(\d+) alive=(\d) alpha=(\d+) action=(\d+) motion=(\d+) depthFaces=(\d+) alphaFaces=(\d+)", text))
        if not records:
            continue
        match = records[-1]  # Windows can reuse a PID/log name on a later run.
        text = text[match.start():]
        capture = re.search(r"captured GPU frame '([^']+)'", text)
        if not match or not capture:
            continue
        name = PureWindowsPath(capture[1]).stem
        vertices = []
        for line in text.splitlines():
            if "[CorpseVertex]" in line:
                vertices.append({k: int(v) if k == "i" else float(v)
                    for k, v in re.findall(r"(i|x|y|oow|z)=([^ ]+)", line)})
        fields = ("age", "alive", "alpha", "action", "motion", "depth_faces", "alpha_faces")
        runs[name] = dict(zip(fields, map(int, match.groups())))
        runs[name].update(vertices=vertices, log=log.name)
    names = [f"corpse-{age}" for age in (0, 3060, 6000, 6119, 6120)] + ["corpse-transform"]
    assert set(names) <= runs.keys(), "missing actual native capture/log"
    frames = {name: Image.open(args.capture_dir / (name + ".bmp")).convert("RGB") for name in names}
    background = frames["corpse-6120"]
    assert background.size == (640, 480)
    assert background.getextrema() == ((32, 32), (32, 32), (32, 32)), "expired corpse left pixels"
    for name, image in frames.items():
        difference = ImageChops.difference(image, background)
        runs[name].update(pixel_bbox=difference.getbbox(), contrast_sum=sum(ImageStat.Stat(difference).sum),
            dimensions=list(image.size))
    for age, alpha, alive in ((0,255,1),(3060,127,1),(6000,5,1),(6119,0,1),(6120,0,0)):
        result = runs[f"corpse-{age}"]
        assert (result["alpha"], result["alive"], result["action"], result["motion"]) == (alpha,alive,32,11)
        assert (result["contrast_sum"] > 0) == (alpha > 0), "native opacity/pixel mismatch"
        assert result["depth_faces"] == (3 if age == 0 else 0)
        assert result["alpha_faces"] == (3 if alive and age != 0 else 0)
    assert runs["corpse-0"]["contrast_sum"] > runs["corpse-3060"]["contrast_sum"] > runs["corpse-6000"]["contrast_sum"] > 0
    assert runs["corpse-transform"]["pixel_bbox"] != runs["corpse-0"]["pixel_bbox"]

    # Independent source fixture, not values exported by Corpse.cpp.
    asset = (ROOT / "Ref/GRF-Content/data/sprite/ork_warrior.act").read_bytes()
    assert struct.unpack_from("<H", asset)[0] == 0x4341
    assert struct.unpack_from("<iiii4Bffiiii", asset, 47220) == (-9,-1,33,1,255,255,255,255,1.,1.,0,0,94,72)
    # Original ACT ReCalcClipXY turns this center into(-56,-37). PE
    # ProjectVertex/GetPixelRatio and RenderBody/RenderSprite give the corners.
    last_ratio = .5 / (480 * .0017447917)
    bias = 3 * .0000090000003 + .000036000001 * last_ratio
    for name, transformed in (("corpse-0", False), ("corpse-transform", True)):
        sx, sy = (.75,.375) if transformed else (.5,.5)
        top, bottom = -37*sy+240, 35*sy+241
        left, right = (-56*sx+320,38*sx+321) if transformed else (38*sx+320,-56*sx+321)
        angle = math.pi/4 if transformed else 0
        cx, cy = (left+right)/2, (top+bottom)/2
        half_w, half_h = (right-left)/2, (bottom-top)/2
        expected = []
        assert len(runs[name]["vertices"]) == 4
        for i in range(4):
            dx, dy = (half_w if i&1 else -half_w), (half_h if i&2 else -half_h)
            reciprocal = 1/(500-((bottom if i&2 else top)-240)*.16)+bias
            point = {"x":cx+dx*math.cos(angle)-dy*math.sin(angle)+.5,
                "y":cy+dx*math.sin(angle)+dy*math.cos(angle)+.5,
                "oow":reciprocal, "z":1500/1490*(1-10*reciprocal)}
            expected.append(point)
            for key, value in point.items():
                tolerance = .0001 if key in ("x","y") else .000001 if key == "z" else .000000005
                assert abs(runs[name]["vertices"][i][key]-value) < tolerance, (name,i,key)
        runs[name].update(independent_PE_geometry="PASS", expected_first_layer=expected)
    result = {"cases": {name:runs[name] for name in names}, "timing_factor_bits":"3d2aaaab",
        "actual_gpu_capture":True, "geometry_oracle":"original ACT byte47220 and PE equations; height480 avgPixelRatio0.837500016"}
    args.output.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print("PASS six native corpse captures: original art, opacity, expiry, clip transforms and independent PE geometry")


if __name__ == "__main__":
    main()
