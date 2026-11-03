"""Exercise production emote decoding, commands, and overhead anchoring."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'src/gamemode/GameModePacket.cpp').read_text(encoding='utf-8')
handler = source[source.index('void HandleEmotion('):source.index('void HandleActorStateChange(')]
message_source = (root / 'src/world/MsgEffect.cpp').read_text(encoding='utf-8')
layout = message_source[message_source.index('auto OverheadClipPoints('):message_source.index('bool QueueMsgSpriteQuad(')]
build = root / 'build-mingw-preset/status-check'
build.mkdir(parents=True, exist_ok=True)
cpp = build / 'check.cpp'
cpp.write_text(r'''
#include "world/Emotion.h"
#include "world/OverheadEffect.h"
#include "render3d/ScreenEffects.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <map>
#include <tuple>
#include <vector>
using LONG=long;
struct RECT { LONG left,top,right,bottom; };
struct SprImg { int width=10,height=10,isHalfW=0,isHalfH=0; };
struct CSprClip { int x=0,y=0,sprIndex=0,clipType=0,angle=0,a=255; float zoomX=1,zoomY=1; };
struct CMotion { std::vector<CSprClip> sprClips; };
struct CSprRes { SprImg image; const SprImg* GetSprite(int,int) { return &image; } };
struct CActRes {
    std::vector<CMotion> motions;
    int GetMotionCount(int) { return motions.size(); }
    const CMotion* GetMotion(int,int index) { return index>=0 && index<int(motions.size())?&motions[index]:nullptr; }
};
struct QueuedMsgSpriteEffectDraw { int actionIndex=0,motionIndex=0,screenX=100,screenY=100; bool normalizeEmotionAnchor=false; };
''' + layout + r'''
using u32 = uint32_t;
struct CGameActor {
    int calls=0, emotion=-1;
    void SendMsg(CGameActor*, int msg, int value, int, int) { assert(msg==21); ++calls; emotion=value; }
} local, remote;
struct CGameMode { int m_isReqEmotion=1; } mode;
struct PacketView { const unsigned char* data; int packetLength; };
u32 ReadLE32(const unsigned char* p) { return u32(p[0]) | u32(p[1])<<8 | u32(p[2])<<16 | u32(p[3])<<24; }
CGameActor* ResolveSkillPacketActor(CGameMode&, u32 id) { return id==1?&local:id==2?&remote:nullptr; }
bool IsLocalPlayerActor(const CGameMode&, u32 id) { return id==1; }
''' + handler + r'''
int main() {
    for (int id=0;id<=80;++id) {
        assert(EmotionAction(id)>=0 && EmotionAction(id)<80);
        assert(ResolveEmotionCommand("/emotion "+std::to_string(id))==id);
        unsigned char data[]={0xC0,0,1,0,0,0,static_cast<unsigned char>(id)};
        HandleEmotion(mode,{data,7}); assert(local.emotion==id);
    }
    assert(local.calls==81 && mode.m_isReqEmotion==0);
    unsigned char data[]={0xC0,0,2,0,0,0,80};
    mode.m_isReqEmotion=1; HandleEmotion(mode,{data,7});
    assert(remote.emotion==80 && mode.m_isReqEmotion==1);
    data[6]=81; HandleEmotion(mode,{data,7});
    data[6]=3; HandleEmotion(mode,{data,6}); HandleEmotion(mode,{nullptr,7});
    data[2]=99; HandleEmotion(mode,{data,7}); assert(remote.calls==1);
    assert(EmotionAction(-1)==-1 && EmotionAction(81)==-1);
    assert(EmotionAction(4)==5 && EmotionAction(14)==4 && EmotionAction(34)==10 && EmotionAction(80)==79);
    assert(ResolveEmotionCommand("/!")==0 && ResolveEmotionCommand("/lv")==3);
    assert(ResolveEmotionCommand("/whisp")==80 && ResolveEmotionCommand("/emote 34")==34);
    for (const char* bad : {"/emote", "/emotion -1", "/emotion 81", "/emotion 2x", "/emotion "})
        assert(ResolveEmotionCommand(bad)==-2);
    assert(ResolveEmotionCommand("hello")==-1 && ResolveEmotionCommand("/sit")==-1);
    for (int head : {10,100,500}) for (int height : {1,16,64,150})
        assert(OverheadIconTop(head,height)+height==head-6);
    assert(kOverheadIconScale==3.0f*0.75f);
    assert(OverheadAnchorBottom({6,6,6,-7.6f},true)==6); // lv stays fixed.
    assert(OverheadAnchorBottom({5,5.95f,5,5.95f,7.728f},true)==7.728f); // gg stays fixed.
    assert(OverheadAnchorBottom({5,5,5,5,66.01f},true)==5); // kiss overshoot.
    assert(OverheadAnchorBottom({6,6,6,6,42},true)==6); // lv2 overshoot.
    assert(OverheadAnchorBottom({5,5,5,5,66.01f},false)==66.01f);
    const auto rotated=OverheadClipQuad(-10,-5,20,10,2,3,90);
    assert(std::fabs(rotated[0].x-15)<0.001 && std::fabs(rotated[0].y+20)<0.001);
    const auto scaled=OverheadClipQuad(4,-8,10,20,2,0.5f,0);
    assert(scaled[0].x==8 && scaled[0].y==-4 && scaled[3].x==28 && scaled[3].y==6);
    CSprRes sprite; CActRes act;
    CSprClip first; first.x=-5;first.y=-10;
    CSprClip second=first;second.x+=12;second.y-=3;second.zoomY=2;second.angle=30;
    act.motions={{{first}},{{second}}};
    QueuedMsgSpriteEffectDraw draw;
    RECT a{},b{};float ax,ay,bx,by;
    assert(ResolveOverheadSpriteLayout(draw,&sprite,&act,2.25f,&ax,&ay,&a));
    draw.motionIndex=1;
    assert(ResolveOverheadSpriteLayout(draw,&sprite,&act,2.25f,&bx,&by,&b));
    assert(ax==bx && ay==by && ax==100); // Frame changes cannot move the anchor.
    assert(a.bottom<=95 && b.bottom<=95);
    assert(a.left!=b.left && a.top!=b.top); // Preserve ACT movement and scaling.
    // Real ACT frame boundaries; one-shot completion and status loops both
    // consume this same position, regardless of how often OnProcess runs.
    assert(OverheadAnimationPosition(95,4)<1);
    assert(OverheadAnimationPosition(96,4)==1);
    assert(OverheadAnimationPosition(144,6)==1);
    assert(OverheadAnimationPosition(960,4)==10);
    assert(OverheadAnimationPosition(96,0)==4);
    assert(OverheadAnimationPosition(24,0.5f)==1);
    assert(OverheadAnimationPosition(96,NAN)==1);
    for (int type : {1,7,8,9,10,11,17}) assert(IsOverheadMsgEffect(type));
    assert(!IsOverheadMsgEffect(5) && !IsOverheadMsgEffect(18));
    for (bool effects : {false,true}) for (bool ssao : {false,true}) for (bool waves : {false,true}) {
        assert((ResolveScreenEffectFlags(ssao,waves,effects,16)&4)==4);
        assert((ResolveScreenEffectFlags(ssao,waves,effects,0)&4)==0);
    }
    std::cout << "PASS: all 81 emote IDs, packet validation, chat commands, head clearance, blindness independent of cosmetic settings\n";
}
''')
compiler = Path('C:/Qt/Tools/mingw1310_64/bin/g++.exe')
exe = build / 'check.exe'
subprocess.run([str(compiler), '-std=c++17', '-O3', str(cpp), '-I', str(root/'src'), '-o', str(exe)], check=True)
env = os.environ.copy()
env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH','')
subprocess.run([str(exe)], check=True, env=env)

