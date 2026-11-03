"""SDK-free GR2 regression checks. Uses the repository's reference game assets."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
root=Path(__file__).resolve().parents[2]
build=root/"build-mingw-preset/gr2-native-check"
build.mkdir(parents=True,exist_ok=True)
compiler=Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe")
qt=Path("C:/Qt/6.11.0/mingw_64")
env=os.environ.copy()
env["PATH"]=str(qt/"bin")+os.pathsep+str(compiler.parent)+os.pathsep+env.get("PATH","")
reader=build/"reader-tests.exe"
subprocess.run([str(compiler),"-std=c++17","-O3","-DRO_ENABLE_QT6_UI=1",
 str(root/"tools/tests/Gr2ReaderTests.cpp"),str(root/"src/res/Gr2Reader.cpp"),str(root/"src/res/Gr2Pose.cpp"),
 "-I",str(root/"src"),"-I",str(qt/"include"),"-I",str(qt/"include/QtCore"),"-I",str(qt/"include/QtQml"),
 "-L",str(qt/"lib"),"-lQt6Qml","-lQt6Core","-o",str(reader)],check=True)
subprocess.run([str(reader),str(build)],check=True,cwd=root,env=env)
# Original decoded BGRA texture bytes, captured before retiring the SDK decoder.
hashes={"kguardian90_7.gr2":"4a32472836ff1f2d8636c757b8968c80e1344227046653073fa12618435d9773",
 "aguardian90_8.gr2":"d1aab23f768319b6c827f745863a20eefb78080fad0d433b335a5f1525b15016",
 "sguardian90_9.gr2":"bf27dd9cf40027552503dc372314647dba5551679f6cad5f6fe9ada982f89f96"}
for name,expected in hashes.items():
 assert hashlib.sha256((build/(name+".pixels")).read_bytes()).hexdigest()==expected,name
# SDK world/inverse-bind matrices captured at 0%, 25%, 50%, 90% of each
# animation, for root/middle/last bones. No SDK is needed to run these checks.
reference=json.loads((root/"tools/tests/gr2_pose_reference.json").read_text())
for name,golden in reference.items():
 data=(build/(name+".pose")).read_bytes();values=struct.unpack("<%df"%(len(data)//4),data)
 error=max(abs(values[i]-expected) for i,expected in zip(golden["indices"],golden["values"]))
 assert error<.001,(name,error)
print("PASS: all 13 texture images match original bytes; 15 animation pose snapshots match reference")
source=(root/"src/world/World.cpp").read_text(encoding="utf-8")
color=source[source.index("u32 ResolveWaterColor("):source.index("CTexture* UpdateGroundWater(")]
update=source[source.index("CTexture* UpdateGroundWater("):source.index("bool BuildWaterQuadHeights(")]
cpp=build/"check.cpp"
cpp.write_text(r'''
#include "world/Gr2Monster.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#define timeGetTime TestTime
unsigned now=100;unsigned TestTime(){return now;}
constexpr unsigned kWaterAnimationTicksPerSecond=60;
constexpr u32 kWaterDefaultColor=0x90FFFFFFu;
struct CTexture {} texture;
struct CTexMgr {static CTexture s_dummy_texture;CTexture* GetTexture(const char*,bool){return &texture;}} g_texMgr;
CTexture CTexMgr::s_dummy_texture;
int requestedSet=-1,requestedFrame=-1;
std::string ResolveWaterTexturePath(int set,int frame){requestedSet=set;requestedFrame=frame;return "water";}
struct C3dGround {
 int m_waterSet=4,m_texAnimCycle=3,m_waterCnt=0,m_waterOffset=0,m_waveSpeed=2;
 u32 m_lastWaterAnimTick=0,m_waterAnimAccumulator=0;
 vector3d m_ambientCol{.5f,.25f,1};CTexture* m_waterTex=nullptr;
};
'''+color+update+r'''
int main(){
 for(int job:{1285,1286,1287,1829,1830,1899,1900,1949,1950})assert(Gr2MonsterModel(job));
 assert(std::string(Gr2MonsterModel(1829)).find("kguardian90_7.gr2")!=std::string::npos);
 assert(std::string(Gr2MonsterModel(1830)).find("aguardian90_8.gr2")!=std::string::npos);
 assert(!Gr2MonsterModel(1002) && !Gr2MonsterModel(8));
 C3dGround ground;
 assert(ResolveWaterColor(ground)==0xFF7F3FFFu);
 ground.m_waterSet=0;assert(ResolveWaterColor(ground)==0x90FFFFFFu);ground.m_waterSet=4;
 assert(UpdateGroundWater(ground)==&texture && requestedSet==4 && requestedFrame==0);
 now+=1000;UpdateGroundWater(ground);assert(requestedFrame==20 && ground.m_waterOffset==120);
 now+=600;UpdateGroundWater(ground);assert(requestedFrame==0); // 32 frames * 3 ticks.
 std::cout<<"PASS: guardian class mappings, opaque ambient-tinted lava, ordinary water alpha, 32-frame cycle and waves\n";
}
''')
compiler=Path("C:/Qt/Tools/mingw1310_64/bin/g++.exe");exe=build/"check.exe"
subprocess.run([str(compiler),"-std=c++17","-DNOMINMAX","-O3",str(cpp),"-I",str(root/"src"),"-o",str(exe)],check=True)
env=os.environ.copy();env["PATH"]=str(compiler.parent)+os.pathsep+env.get("PATH","")
subprocess.run([str(exe)],check=True,env=env)
