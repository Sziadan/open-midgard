"""Compare the native C++ reader with the reference archived in Ref/JsGranny.

Development-only check: needs Node and MinGW; no packages or generated bundle.
The regular client build and check_gr2_monsters.py do not require JavaScript.
"""
from __future__ import annotations
import binascii
import hashlib
import json
import os
import shutil
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / "build-mingw-preset/gr2-native-check"
build.mkdir(parents=True, exist_ok=True)
gpp = Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
node = shutil.which("node")
if not node:
    fallback = Path.home()/".cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe"
    if fallback.exists(): node = str(fallback)
if not node: raise SystemExit("Node is needed only for this optional reference parity check")
models = ["kguardian90_7.gr2", "aguardian90_8.gr2", "sguardian90_9.gr2",
          "empelium90_0.gr2", "guildflag90_1.gr2", "treasurebox_2.gr2"]
oracle = build / "reference-parity.mjs"
oracle.write_text(r"""
import {readFileSync,writeFileSync} from 'node:fs';
import {pathToFileURL} from 'node:url';
const {read} = await import(pathToFileURL(process.argv[2]+'/client.js').href);
const names=JSON.parse(process.argv[4]);
for(const name of names) {
 const input=readFileSync('Ref/GRF-Content/data/model/3dmob/'+name);
 writeFileSync(process.argv[3]+'/'+name+'.js.json',read(input.toString('hex'),false));
 console.log('reference '+name);
}
""",encoding="utf-8")
env = os.environ.copy()
env["PATH"] = str(gpp.parent)+os.pathsep+env.get("PATH", "")
subprocess.run([node,str(oracle),str(root/"Ref/JsGranny"),str(build),json.dumps(models)],check=True,cwd=root,env=env)

gr2_srcs = [
    root / "src/res/Gr2Reader.cpp",
    root / "src/res/Gr2Pose.cpp",
    root / "src/res/gr2/Gr2File.cpp",
    root / "src/res/gr2/Gr2Oodle0.cpp",
    root / "src/res/gr2/Gr2TypeTree.cpp",
    root / "src/res/gr2/Gr2Igc.cpp",
    root / "src/res/gr2/Gr2IgcArith.cpp",
    root / "src/res/gr2/Gr2IgcPlane.cpp",
    root / "src/res/gr2/Gr2IgcIdwt.cpp",
    root / "src/res/gr2/Gr2IgcYuv.cpp",
    root / "src/res/gr2/Gr2Extract.cpp",
    root / "src/res/gr2/Gr2ExtractMesh.cpp",
    root / "src/res/gr2/Gr2ExtractAnim.cpp",
    root / "src/res/gr2/Gr2Decode.cpp",
]
dump_cpp = build / "dump_native_all.cpp"
dump_cpp.write_text(
    r"""
#include "res/Gr2Reader.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
int main(){
 for(const char* name:{"kguardian90_7.gr2","aguardian90_8.gr2","sguardian90_9.gr2","empelium90_0.gr2","guildflag90_1.gr2","treasurebox_2.gr2"}){
  std::ifstream f(std::string("Ref/GRF-Content/data/model/3dmob/")+name,std::ios::binary);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});
  ro::gr2::Asset asset; std::string error;
  if(!ro::gr2::Decode(bytes,asset,error)){std::cerr<<name<<": "<<error<<"\n";return 1;}
  std::ofstream out(std::string("build-mingw-preset/gr2-native-check/")+name+".native.pixels",std::ios::binary);
  for(const auto& image:asset.images)
   out.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size()*4);
  std::cout<<"native "<<name<<" images="<<asset.images.size()<<"\n";
 }
}
""",
    encoding="utf-8",
)
dump = build / "dump_native_all.exe"
subprocess.run(
    [str(gpp), "-std=c++17", "-O3", str(dump_cpp), *[str(p) for p in gr2_srcs], "-I", str(root / "src"), "-o", str(dump)],
    check=True,
)
subprocess.run([str(dump)], check=True, cwd=root, env=env)

ok = True
for name in models:
    js = json.loads((build / f"{name}.js.json").read_text(encoding="utf-8"))
    cpp = (build / f"{name}.native.pixels").read_bytes()
    packed = bytearray()
    for texture in js["textures"]:
        px = binascii.unhexlify(texture["pixels"])
        for i in range(0, len(px), 4):
            r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
            packed += struct.pack("<I", (a << 24) | (r << 16) | (g << 8) | b)
    match = bytes(packed) == cpp
    print(
        f"{name}: js_images={len(js['textures'])} bytes={len(cpp)} "
        f"match={match} sha={hashlib.sha256(cpp).hexdigest()[:16]}"
    )
    ok = ok and match
print("PASS: native C++ textures match JS reader for all 6 3dmob models" if ok else "FAIL")
raise SystemExit(0 if ok else 1)
