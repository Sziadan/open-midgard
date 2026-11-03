"""Check reference map-lighting math; optionally check actual GPU pixel output."""
import argparse
import os
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--gpu",action="store_true",help="Run installed-asset fixtures on D3D11, D3D12 and Vulkan")
args=parser.parse_args()
build=root/"build-mingw-preset/lighting-check"
build.mkdir(parents=True,exist_ok=True)
compiler=Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
env=os.environ.copy();env["PATH"]=str(compiler.parent)+os.pathsep+env.get("PATH","")
test=build/"map-lighting-tests.exe"
subprocess.run([str(compiler),"-std=c++17","-DNOMINMAX","-DRO_PLATFORM_WINDOWS=1","-O2",
    "-ffunction-sections","-fdata-sections","-Wl,--gc-sections",
    str(root/"tools/tests/MapLightingTests.cpp"),str(root/"src/render3d/ModernRenderState.cpp"),
    str(root/"src/res/ModelRes.cpp"),
    str(root/"src/core/Hash.cpp"),
    "-I",str(root/"src"),"-o",str(test)],check=True)
subprocess.run([str(test)],env=env,check=True)
if args.gpu:
    from PIL import Image
    executable=root/"build-mingw-preset/open-midgard.exe"
    # Keep unrelated fixture overrides out of this isolated test process.
    env={key:value for key,value in env.items() if not key.startswith("RO_HEAL_PREVIEW_")}
    for backend in (1,2,3):
        output=build/f"lightmap-backend-{backend}"
        output.mkdir(parents=True,exist_ok=True)
        (output/"open-midgard.ini").write_text(f"[Graphics]\nWidth=800\nHeight=600\nSSAO=0\nRenderScale=100\nAntiAliasing=0\nTextureUpscale=1\nWindowMode=0\n[Renderer]\nBackend={backend}\n")
        env.update(OPEN_MIDGARD_DATA_DIR=str(output),RO_HEAL_PREVIEW_ASSETS="D:/Spel/OldRO",
                   RO_HEAL_PREVIEW_OUT=str(output/"frame.bmp"),RO_HEAL_PREVIEW_LIGHTMAP_TEST="1")
        with (output/"stderr.log").open("w") as errors:
            process=subprocess.Popen([str(executable)],cwd=root,env=env,stderr=errors)
            assert process.wait(timeout=30)==0,("fixture exit",backend)
        log=(executable.parent/f"debug_hp_{process.pid}.log").read_text(errors="replace")
        (output/"client.log").write_text(log)
        errors=(output/"stderr.log").read_text(errors="replace")
        assert "VUID-" not in log+errors and "Validation Error" not in log+errors
        image=Image.open(output/"frame.bmp").convert("RGB")
        # D3DTOP_MODULATEALPHA_ADDCOLOR: base * shadow alpha + light RGB.
        for position,expected in [((32,32),(64,64,64)),((112,32),(64,96,192))]:
            actual=image.getpixel(position)
            assert max(abs(x-y) for x,y in zip(actual,expected))<=1,(backend,position,actual,expected)
        print(f"PASS: backend {backend}: shadow-only and colored-lightmap pixels match reference equation")
