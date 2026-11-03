"""Exercise the production socket flush with deterministic short-write failures."""
from pathlib import Path
import os
import subprocess

root=Path(__file__).resolve().parents[2]
build=root/'build-mingw-preset/send-queue-check'
build.mkdir(exist_ok=True)
source=(root/'src/network/Connection.cpp').read_text(encoding='utf-8')
def function(signature):
    start=source.index(signature)
    brace=source.index('{',start)
    end=brace+1
    depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}')
        end+=1
    return source[start:end]
(build/'SendQueue.inc').write_text(function('int CConnection::FlushSendQueue()')+'\n'+function('int CConnection::Send(const char* buf, int len)'))
compiler=Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
exe=build/'send-queue.exe'
subprocess.run([str(compiler),'-std=c++17','-O2','-I'+str(root/'src'),'-I'+str(build),str(root/'tools/tests/SendQueueTests.cpp'),str(root/'src/network/PacketQueue.cpp'),'-o',str(exe)],check=True)
env=dict(os.environ)
env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
subprocess.run([str(exe)],env=env,check=True)
