#include "Gr2Monster.h"
#include "res/Gr2Reader.h"
#include "core/File.h"
#include "core/Timer.h"
#include "render/Renderer.h"
#include "DebugLog.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {
struct CpuAsset {
    ro::gr2::Asset model;
    std::map<int,ro::gr2::Animation> animations;
    std::string error;
};
struct Asset {
    std::unique_ptr<CpuAsset> cpu;
    std::vector<std::unique_ptr<CTexture>> textures;
};

struct Instance {
    Asset* asset=nullptr;
    int state=-1;
    u32 start=0,lastTick=0;
    float clock=0,facing=0,idleStart=0;
    bool resumedIdle=false,finished=false,hasBounds=false;
    RECT bounds{};
    std::vector<ro::gr2::Matrix> matrices;
    std::vector<std::array<float,3>> positions,normals;
    std::vector<tlvertex3d> projected;
    std::vector<unsigned char> valid;
};
std::map<std::string,std::unique_ptr<Asset>> assets;
std::map<std::string,std::future<std::unique_ptr<CpuAsset>>> pending;
std::map<u32,std::unique_ptr<Instance>> instances;

std::vector<unsigned char> ReadBytes(const std::string& path)
{
    CFile file;
    if(!file.Open(path.c_str()) || file.Size()<=0 || file.Size()>16*1024*1024)return {};
    std::vector<unsigned char> bytes(file.Size());
    if(!file.Read(bytes.data(),static_cast<u32>(bytes.size())))return {};
    return bytes;
}
void StartAsset(int job)
{
    const char* modelPath=Gr2MonsterModel(job);
    if(!modelPath || assets.count(modelPath) || pending.count(modelPath))return;
    auto model=ReadBytes(modelPath);
    if(model.empty()) {DbgLog("[Gr2Monster] missing model %s\n",modelPath);assets.emplace(modelPath,nullptr);return;}
    const int type=std::atoi(std::strrchr(modelPath,'_')+1);
    std::map<int,std::vector<unsigned char>> animationBytes;
    // Guardians (7–9) ship move/attack/dead/damage. Emperium (0) is idle-only.
    // Guild flag (1) has attack; treasure box (2) has dead/damage. Match on-disk
    // 3dmob_bone banks and skip missing ones rather than failing the asset.
    const std::pair<int,const char*> names[]={{1,"move"},{2,"attack"},{3,"dead"},{4,"damage"}};
    // Resource-manager access stays on the calling/main thread. Only isolated
    // byte buffers are passed to the decoder worker; no engine/GPU state is shared.
    for(const auto& name:names) {
        auto bytes=ReadBytes("data\\model\\3dmob_bone\\"+std::to_string(type)+"_"+name.second+".gr2");
        if(!bytes.empty())animationBytes.emplace(name.first,std::move(bytes));
    }
    pending.emplace(modelPath,std::async(std::launch::async,[model=std::move(model),animations=std::move(animationBytes)]() mutable {
        auto result=std::make_unique<CpuAsset>();
        if(!ro::gr2::Decode(model,result->model,result->error))return result;
        for(auto& animation:animations) {
            ro::gr2::Asset decoded;
            if(!ro::gr2::Decode(animation.second,decoded,result->error,true))return result;
            if(decoded.animations.empty()) {result->error="Empty GR2 animation bank";return result;}
            result->animations.emplace(animation.first,std::move(decoded.animations.front()));
        }
        return result;
    }));
}
Asset* GetAsset(int job)
{
    const char* path=Gr2MonsterModel(job);if(!path)return nullptr;
    StartAsset(job);
    const auto existing=assets.find(path);if(existing!=assets.end())return existing->second.get();
    const auto loading=pending.find(path);
    if(loading==pending.end() || loading->second.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return nullptr;
    std::unique_ptr<CpuAsset> cpu;
    try {cpu=loading->second.get();} catch(const std::exception& exception) {
        cpu=std::make_unique<CpuAsset>();cpu->error=exception.what();
    }
    pending.erase(loading);
    if(!cpu || !cpu->error.empty()) {
        DbgLog("[Gr2Monster] independent decode failed path=%s error=%s\n",path,cpu?cpu->error.c_str():"no data");
        assets.emplace(path,nullptr);return nullptr;
    }
    auto asset=std::make_unique<Asset>();asset->cpu=std::move(cpu);
    for(auto& image:asset->cpu->model.images) {
        auto texture=std::make_unique<CTexture>();
        // Ref/Model.cpp preserves authored RGBA. Sprite magenta-key heuristics
        // and low-alpha wipes destroy legitimate GR2 texels and soft edges.
        // Keep native model textures out of the sprite upscaler as well.
        if(!texture->Create(image.width,image.height,PF_A8R8G8B8,false)) {assets.emplace(path,nullptr);return nullptr;}
        texture->Update(0,0,image.width,image.height,image.pixels.data(),true,image.width*4);
        asset->textures.push_back(std::move(texture));
    }
    DbgLog("[Gr2Monster] independent reader ready path=%s meshes=%u bones=%u textures=%u animationBanks=%u\n",path,
        static_cast<unsigned>(asset->cpu->model.meshes.size()),static_cast<unsigned>(asset->cpu->model.bones.size()),
        static_cast<unsigned>(asset->textures.size()),static_cast<unsigned>(asset->cpu->animations.size()));
    return assets.emplace(path,std::move(asset)).first->second.get();
}
const ro::gr2::Animation* Animation(const CpuAsset& asset,int state)
{
    if(state==9)state=2;
    const auto found=asset.animations.find(state);
    if(found!=asset.animations.end())return &found->second;
    return asset.model.animations.empty()?nullptr:&asset.model.animations.front();
}
vector3d Rotate(vector3d point,float rotation) {
    // Ref/GameActor3d.cpp RenderNormal: 90-degree X rotation for *90_*.gr2,
    // then Y(rotation+180), then translate by actor position minus (2,0,2).
    point={point.x,-point.z,point.y};
    const float angle=(rotation+180)*0.017453292519943295f,c=std::cos(angle),s=std::sin(angle);
    return {point.x*c+point.z*s,point.y,-point.x*s+point.z*c};
}
bool Project(const vector3d& point,const matrix& view,tlvertex3d& vertex) {
    const float z=point.x*view.m[0][2]+point.y*view.m[1][2]+point.z*view.m[2][2]+view.m[3][2];
    if(!std::isfinite(z) || z<=10)return false;
    vertex.oow=1/z;vertex.z=(1500.0f/1490.0f)*(z-10)/z;
    vertex.x=g_renderer.m_xoffset+(point.x*view.m[0][0]+point.y*view.m[1][0]+point.z*view.m[2][0]+view.m[3][0])*g_renderer.m_hpc/z;
    vertex.y=g_renderer.m_yoffset+(point.x*view.m[0][1]+point.y*view.m[1][1]+point.z*view.m[2][1]+view.m[3][1])*g_renderer.m_vpc/z;
    return std::isfinite(vertex.x) && std::isfinite(vertex.y);
}
}

bool RenderGr2Monster(const Gr2MonsterState& state,const matrix& view)
{
    Asset* asset=GetAsset(state.job);if(!asset || asset->cpu->model.meshes.empty())return false;
    auto& item=instances[state.gid];
    if(!item || item->asset!=asset) {
        item=std::make_unique<Instance>();item->asset=asset;item->facing=180-state.rotation;
    }
    auto& instance=*item;
    if(instance.state!=state.state || instance.start!=state.stateStartTick) {
        instance.state=state.state;instance.start=state.stateStartTick;instance.lastTick=state.stateStartTick;
        instance.clock=0;instance.resumedIdle=false;instance.finished=false;instance.idleStart=0;
    }
    const u32 now=timeGetTime();const float elapsed=static_cast<float>(now-instance.lastTick);
    const float delta=std::remainder((180-state.rotation)-instance.facing,360.0f);
    if(std::fabs(delta)>12)instance.facing+=(std::max)(-elapsed*0.36f,(std::min)(elapsed*0.36f,delta));
    instance.facing=std::remainder(instance.facing,360.0f);
    const float factor=(state.state==2 || state.state==9) && state.attackSpeedFactor>0 && state.attackSpeedFactor<1?state.attackSpeedFactor:1;
    if(!state.frozen)instance.clock+=elapsed*0.002f/factor;
    instance.lastTick=now;
    const auto& cpu=*asset->cpu;
    const auto* animation=Animation(cpu,state.state);
    bool loop=state.state==0 || state.state==1;
    instance.finished=!loop && (!animation || instance.clock>=animation->duration);
    if((state.state==2 || state.state==9) && instance.finished && !instance.resumedIdle) {
        instance.resumedIdle=true;instance.idleStart=instance.clock;
    }
    double sampleTime=instance.clock;
    if(instance.resumedIdle) {animation=Animation(cpu,0);sampleTime-=instance.idleStart;loop=true;}
    instance.matrices=ro::gr2::Sample(cpu.model,animation,sampleTime,loop);
    float minX=1e9f,minY=1e9f,maxX=-1e9f,maxY=-1e9f;
    unsigned triangles=0;
    for(size_t meshIndex=0;meshIndex<cpu.model.meshes.size();++meshIndex) {
        const auto& mesh=cpu.model.meshes[meshIndex];
        // Ref/GameActor3d.cpp RenderGuildMark replaces mesh 1 with the guild's
        // emblem. Its embedded green emblem.tif is an authoring placeholder.
        // Until an emblem resource is available, Ref uses a transparent image;
        // omitting that surface is equivalent and leaves the banner intact.
        if(state.job==722 && meshIndex==1)continue;
        if(mesh.batches.empty())continue;
        ro::gr2::Skin(mesh,instance.matrices,instance.positions,instance.normals);
        instance.projected.resize(mesh.vertices.size());instance.valid.assign(mesh.vertices.size(),0);
        for(size_t index=0;index<mesh.vertices.size();++index) {
            const auto& vertex=mesh.vertices[index];const auto& p=instance.positions[index];const auto& n=instance.normals[index];
            vector3d point=Rotate({p[0],p[1],p[2]},instance.facing);
            const float scale=state.zoom>0?state.zoom:1;
            point={point.x*scale+state.pos.x-2,point.y*scale+state.pos.y,point.z*scale+state.pos.z-2};
            auto& out=instance.projected[index];instance.valid[index]=Project(point,view,out);if(!instance.valid[index])continue;
            const vector3d normal=Rotate({n[0],n[1],n[2]},instance.facing);
            const float lit=(std::max)(0.0f,-(normal.x*state.light.x+normal.y*state.light.y+normal.z*state.light.z));
            const auto channel=[&](float ambient,float diffuse,int shift) {
                return static_cast<u32>((std::max)(0.0f,(std::min)(1.0f,ambient+diffuse*lit))*((state.color>>shift)&255));
            };
            out.color=(state.color&0xFF000000u)|(channel(state.ambient.x,state.diffuse.x,16)<<16)
                |(channel(state.ambient.y,state.diffuse.y,8)<<8)|channel(state.ambient.z,state.diffuse.z,0);
            out.specular=0xFF000000u;out.tu=vertex.uv[0];out.tv=vertex.uv[1];
            minX=(std::min)(minX,out.x);maxX=(std::max)(maxX,out.x);minY=(std::min)(minY,out.y);maxY=(std::max)(maxY,out.y);
        }
        for(const auto& batch:mesh.batches) {
            if(batch.texture<0 || batch.texture>=static_cast<int>(asset->textures.size()))continue;
            for(int triangle=0;triangle<batch.count;++triangle) {
                const size_t first=static_cast<size_t>(batch.first+triangle)*3;
                const int indices[3]={mesh.indices[first],mesh.indices[first+1],mesh.indices[first+2]};
                bool valid=true;for(int index:indices)if(!instance.valid[index])valid=false;
                if(!valid)continue;
                RPFace* face=g_renderer.BorrowNullRP();if(!face)continue;
                face->primType=D3DPT_TRIANGLELIST;face->verts=face->m_verts;face->numVerts=3;face->indices=nullptr;face->numIndices=0;
                CTexture* texture=asset->textures[batch.texture].get();
                face->tex=texture;face->mtPreset=0;face->cullMode=D3DCULL_NONE;
                face->srcAlphaMode=D3DBLEND_SRCALPHA;face->destAlphaMode=D3DBLEND_INVSRCALPHA;
                const unsigned contentW=texture->m_surfaceUpdateWidth?texture->m_surfaceUpdateWidth:texture->m_w;
                const unsigned contentH=texture->m_surfaceUpdateHeight?texture->m_surfaceUpdateHeight:texture->m_h;
                const float uScale=(texture->m_w>0)?static_cast<float>(contentW)/static_cast<float>(texture->m_w):1.0f;
                const float vScale=(texture->m_h>0)?static_cast<float>(contentH)/static_cast<float>(texture->m_h):1.0f;
                for(int corner=0;corner<3;++corner){
                    face->m_verts[corner]=instance.projected[indices[corner]];
                    face->m_verts[corner].tu*=uScale;
                    face->m_verts[corner].tv*=vScale;
                }
                face->alphaSortKey=(face->m_verts[0].oow+face->m_verts[1].oow+face->m_verts[2].oow)/3;
                // Ref RenderNormal/RenderGuildMark submit models with flag 0:
                // cutout alpha with depth writes, including alpha-bearing maps.
                // Sorting individual translucent triangles cannot resolve the
                // flag's overlapping front/back strips and frame correctly.
                g_renderer.AddRP(face,(state.color>>24)<255?1:0);++triangles;
            }
        }
    }
    instance.hasBounds=minX<maxX && minY<maxY;
    if(instance.hasBounds)instance.bounds={static_cast<LONG>(std::floor(minX)),static_cast<LONG>(std::floor(minY)),static_cast<LONG>(std::ceil(maxX)),static_cast<LONG>(std::ceil(maxY))};
    static std::map<int,int> logs;
    if(logs[state.job]<3) {
        ++logs[state.job];DbgLog("[Gr2Monster] rendered job=%d gid=%u state=%d triangles=%u bounds=%ld,%ld,%ld,%ld\n",state.job,state.gid,state.state,triangles,
            instance.bounds.left,instance.bounds.top,instance.bounds.right,instance.bounds.bottom);
    }
    return triangles>0;
}
bool GetGr2MonsterBounds(u32 gid,RECT* bounds)
{
    const auto found=instances.find(gid);if(found==instances.end() || !found->second->hasBounds)return false;
    *bounds=found->second->bounds;return true;
}
bool Gr2MonsterMotionFinished(u32 gid,int state,u32 start)
{
    const auto found=instances.find(gid);
    return found!=instances.end() && found->second->state==state && found->second->start==start && found->second->finished;
}
void ReleaseGr2Monster(u32 gid) {instances.erase(gid);}
void ClearGr2Monsters() {pending.clear();instances.clear();assets.clear();}
void PreloadGr2Monsters() {for(int job:{1288,722,1324,1285,1286,1287})StartAsset(job);}
void WaitForGr2Monsters() {for(auto& load:pending)load.second.wait();}
