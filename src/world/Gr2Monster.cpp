#include "Gr2Monster.h"
#include "CorpseState.h"
#include "res/Gr2Reader.h"
#include "res/Bitmap.h"
#include "core/GuildEmblemKey.h"
#include "core/GuildFlagTexture.h"
#include "gamemode/GameMode.h"
#include "network/Connection.h"
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
#include <zlib.h>

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
    u32 loggedGuild=0,loggedEmblemVersion=0;
    std::vector<ro::gr2::Matrix> matrices;
    std::vector<std::array<float,3>> positions,normals;
    std::vector<tlvertex3d> projected;
    std::vector<unsigned char> valid;
};
std::map<std::string,std::shared_ptr<Asset>> assets;
std::map<std::string,std::future<std::unique_ptr<CpuAsset>>> pending;
std::map<u32,std::unique_ptr<Instance>> instances;
struct GuildTexture {u32 version;std::unique_ptr<CTexture> texture;};
std::map<u32,GuildTexture> guildTextures;
std::map<u32,std::unique_ptr<CTexture>> graffitiTextures;

std::unique_ptr<CTexture> BuildGuildEmblemTexture(const unsigned char* bytes, int size, bool graffiti)
{
    CBitmapRes decoded;
    if (!decoded.LoadFromBuffer("guild-emblem.bmp", bytes, size) || !decoded.m_data
        || (!graffiti && (decoded.m_width != ro::guild::FlagEmblemSize
            || decoded.m_height != ro::guild::FlagEmblemSize))) {
        return nullptr;
    }
    constexpr int extent = ro::guild::FlagTextureSize;
    std::array<u32, extent * extent> pixels{};
    if (graffiti) {
        const int width = (std::min)(decoded.m_width, ro::guild::FlagEmblemSize);
        const int height = (std::min)(decoded.m_height, ro::guild::FlagEmblemSize);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const u32 pixel = decoded.m_data[y * decoded.m_width + x];
                pixels[y * extent + x] = ro::guild::IsEmblemKey(pixel) ? 0 : pixel | 0xFF000000u;
            }
        }
    } else {
        pixels = ro::guild::FlagEmblemCanvas(decoded.m_data);
    }
    auto texture = std::make_unique<CTexture>();
    if (!texture->Create(extent, extent, PF_A8R8G8B8, false)) {
        return nullptr;
    }
    texture->Update(0, 0, extent, extent, pixels.data(), true, extent * 4);
    return texture;
}

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
    auto asset=std::make_shared<Asset>();asset->cpu=std::move(cpu);
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
bool Project(const vector3d& point,const matrix& view,tlvertex3d& vertex,bool corpse=false) {
    if(corpse) {
        const double z=static_cast<double>(point.x)*view.m[0][2]+static_cast<double>(point.y)*view.m[1][2]
            +static_cast<double>(point.z)*view.m[2][2]+view.m[3][2];
        if(!(z>0) || !std::isfinite(z))return false;
        vertex.oow=static_cast<float>(1.0/z);vertex.z=ro::corpse::DeviceDepth(vertex.oow);
        vertex.x=static_cast<float>((static_cast<double>(point.x)*view.m[0][0]+static_cast<double>(point.y)*view.m[1][0]
            +static_cast<double>(point.z)*view.m[2][0]+view.m[3][0])/z*g_renderer.m_hpc+g_renderer.m_xoffset);
        vertex.y=static_cast<float>((static_cast<double>(point.x)*view.m[0][1]+static_cast<double>(point.y)*view.m[1][1]
            +static_cast<double>(point.z)*view.m[2][1]+view.m[3][1])*g_renderer.m_vpc*vertex.oow+g_renderer.m_yoffset);
        return std::isfinite(vertex.x) && std::isfinite(vertex.y);
    }
    const float z=point.x*view.m[0][2]+point.y*view.m[1][2]+point.z*view.m[2][2]+view.m[3][2];
    if(!std::isfinite(z) || z<=10)return false;
    vertex.oow=1/z;vertex.z=(1500.0f/1490.0f)*(z-10)/z;
    vertex.x=g_renderer.m_xoffset+(point.x*view.m[0][0]+point.y*view.m[1][0]+point.z*view.m[2][0]+view.m[3][0])*g_renderer.m_hpc/z;
    vertex.y=g_renderer.m_yoffset+(point.x*view.m[0][1]+point.y*view.m[1][1]+point.z*view.m[2][1]+view.m[3][1])*g_renderer.m_vpc/z;
    return std::isfinite(vertex.x) && std::isfinite(vertex.y);
}
}

namespace {
// Original corpse Render0x565F19-0x565FA7 computes these components in order
// from the inverse view, then normalizes. Preserve the sequential dependencies.
vector3d CorpseEye(const matrix& view)
{
    double a[3][6]{};
    for(int row=0;row<3;++row)for(int col=0;col<3;++col) {
        a[row][col]=view.m[row][col];a[row][col+3]=row==col?1.0:0.0;
    }
    for(int col=0;col<3;++col) {
        int pivot=col;
        for(int row=col+1;row<3;++row)if(std::fabs(a[row][col])>std::fabs(a[pivot][col]))pivot=row;
        if(std::fabs(a[pivot][col])<1e-12)return {0,0,1};
        for(int j=0;j<6;++j)std::swap(a[col][j],a[pivot][j]);
        const double divisor=a[col][col];for(double& v:a[col])v/=divisor;
        for(int row=0;row<3;++row)if(row!=col) {
            const double factor=a[row][col];for(int j=0;j<6;++j)a[row][j]-=factor*a[col][j];
        }
    }
    float inverse[4][3]{};
    for(int row=0;row<3;++row)for(int col=0;col<3;++col)inverse[row][col]=static_cast<float>(a[row][col+3]);
    for(int col=0;col<3;++col)inverse[3][col]=static_cast<float>(
        -view.m[3][0]*a[0][col+3]-view.m[3][1]*a[1][col+3]-view.m[3][2]*a[2][col+3]);
    vector3d eye;
    eye.x=static_cast<float>(inverse[3][0]-static_cast<double>(inverse[1][0])*300.0);
    eye.y=static_cast<float>(static_cast<double>(inverse[0][1])*eye.x-inverse[1][1]*300.0+inverse[3][1]);
    eye.z=static_cast<float>(static_cast<double>(inverse[1][2])*eye.y+static_cast<double>(inverse[0][2])*eye.x+inverse[3][2]);
    const double length=std::sqrt(static_cast<double>(eye.x)*eye.x+static_cast<double>(eye.y)*eye.y+static_cast<double>(eye.z)*eye.z);
    if(!(length>0))return {0,0,1};
    return {static_cast<float>(eye.x/length),static_cast<float>(eye.y/length),static_cast<float>(eye.z/length)};
}

bool RenderGr2Instance(const Gr2MonsterState& state,const matrix& view,Instance& instance,bool corpse,u32 now,double corpseElapsed=0)
{
    Asset* asset=instance.asset;if(!asset || asset->cpu->model.meshes.empty())return false;
    if(instance.state!=state.state || instance.start!=state.stateStartTick) {
        instance.state=state.state;instance.start=state.stateStartTick;instance.lastTick=state.stateStartTick;
        instance.clock=0;instance.resumedIdle=false;instance.finished=false;instance.idleStart=0;
    }
    const float elapsed=static_cast<float>(now-instance.lastTick);
    const float delta=std::remainder((180-state.rotation)-instance.facing,360.0f);
    if(!corpse && std::fabs(delta)>12)instance.facing+=(std::max)(-elapsed*0.36f,(std::min)(elapsed*0.36f,delta));
    instance.facing=std::remainder(instance.facing,360.0f);
    const float factor=(state.state==2 || state.state==9) && state.attackSpeedFactor>0 && state.attackSpeedFactor<1?state.attackSpeedFactor:1;
    if(corpse)instance.clock=static_cast<float>(corpseElapsed+corpseElapsed+instance.clock);
    else if(!state.frozen)instance.clock+=elapsed*0.002f/factor;
    instance.lastTick=now;
    const auto& cpu=*asset->cpu;
    const auto* animation=Animation(cpu,state.state);
    // Source GetAnimation/ActAnimation returns null for a missing death bank;
    // it does not substitute the embedded idle animation for a corpse.
    if(corpse) {
        const auto death=cpu.animations.find(3);
        animation=death==cpu.animations.end()?nullptr:&death->second;
    }
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
    const vector3d eye=corpse?CorpseEye(view):vector3d{};
    for(size_t meshIndex=0;meshIndex<cpu.model.meshes.size();++meshIndex) {
        const auto& mesh=cpu.model.meshes[meshIndex];
        // Ref/GameActor3d.cpp RenderGuildMark replaces mesh 1 with the guild's
        // emblem. Its embedded green emblem.tif is an authoring placeholder.
        // Until an emblem resource is available, Ref uses a transparent image;
        // omitting that surface is equivalent and leaves the banner intact.
        const bool guildMark=!corpse && state.job==722 && meshIndex==1;
        CTexture* emblem = guildMark
            ? (state.graffitiEmblem ? state.graffitiEmblem : Gr2GuildEmblemTexture(state.guildId, state.emblemVersion))
            : nullptr;
        if(guildMark && !emblem)continue;
        const unsigned before=triangles;
        if(mesh.batches.empty())continue;
        ro::gr2::Skin(mesh,instance.matrices,instance.positions,instance.normals);
        instance.projected.resize(mesh.vertices.size());instance.valid.assign(mesh.vertices.size(),0);
        for(size_t index=0;index<mesh.vertices.size();++index) {
            const auto& vertex=mesh.vertices[index];const auto& p=instance.positions[index];const auto& n=instance.normals[index];
            vector3d point;
            if(corpse && !std::strstr(Gr2MonsterModel(state.job),"90")) {
                // Non-*90* resources use source X180, then Y(rotation+180).
                const float angle=(instance.facing+180)*0.017453292519943295f,c=std::cos(angle),s=std::sin(angle);
                point={p[0]*c-p[2]*s,-p[1],-p[0]*s-p[2]*c};
            } else point=Rotate({p[0],p[1],p[2]},instance.facing);
            const float scale=corpse?1.0f:(state.zoom>0?state.zoom:1);
            const float offset=corpse?0.0f:2.0f;
            point={point.x*scale+state.pos.x-offset,point.y*scale+state.pos.y,point.z*scale+state.pos.z-offset};
            auto& out=instance.projected[index];instance.valid[index]=Project(point,view,out,corpse);if(!instance.valid[index])continue;
            const vector3d normal=Rotate({n[0],n[1],n[2]},instance.facing);
            const float lit=(std::max)(0.0f,-(normal.x*state.light.x+normal.y*state.light.y+normal.z*state.light.z));
            const auto channel=[&](float ambient,float diffuse,int shift) {
                return static_cast<u32>((std::max)(0.0f,(std::min)(1.0f,ambient+diffuse*lit))*((state.color>>shift)&255));
            };
            out.color=(state.color&0xFF000000u)|(channel(state.ambient.x,state.diffuse.x,16)<<16)
                |(channel(state.ambient.y,state.diffuse.y,8)<<8)|channel(state.ambient.z,state.diffuse.z,0);
            if(corpse) {
                const double dot=static_cast<double>(eye.x)*n[0]+static_cast<double>(eye.y)*n[1]+static_cast<double>(eye.z)*n[2];
                const u32 shade=static_cast<u32>(std::clamp(dot,0.0,1.0)*100.0+155.0);
                out.color=(state.color&0xFF000000u)|shade*0x010101u;
            }
            if(corpse && meshIndex==0 && index==0 && std::getenv("RO_GR2_CORPSE_TRACE"))
                DbgLog("[Gr2CorpseVertex] rawx=%.9g rawy=%.9g rawz=%.9g nx=%.9g ny=%.9g nz=%.9g x=%.9g y=%.9g oow=%.9g z=%.9g color=%08x\n",
                    p[0],p[1],p[2],n[0],n[1],n[2],out.x,out.y,out.oow,out.z,out.color);
            out.specular=0xFF000000u;out.tu=vertex.uv[0];out.tv=vertex.uv[1];
            minX=(std::min)(minX,out.x);maxX=(std::max)(maxX,out.x);minY=(std::min)(minY,out.y);maxY=(std::max)(maxY,out.y);
        }
        for(const auto& batch:mesh.batches) {
            if(!guildMark && (batch.texture<0 || batch.texture>=static_cast<int>(asset->textures.size())))continue;
            for(int triangle=0;triangle<batch.count;++triangle) {
                const size_t first=static_cast<size_t>(batch.first+triangle)*3;
                const int indices[3]={mesh.indices[first],mesh.indices[first+1],mesh.indices[first+2]};
                bool valid=true;for(int index:indices)if(!instance.valid[index])valid=false;
                if(!valid)continue;
                RPFace* face=g_renderer.BorrowNullRP();if(!face)continue;
                face->primType=D3DPT_TRIANGLELIST;face->verts=face->m_verts;face->numVerts=3;face->indices=nullptr;face->numIndices=0;
                CTexture* texture=guildMark?emblem:asset->textures[batch.texture].get();
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
                g_renderer.AddRP(face,corpse?0:((state.color>>24)<255?1:0));++triangles;
            }
        }
        if(guildMark && triangles>before && (instance.loggedGuild!=state.guildId || instance.loggedEmblemVersion!=state.emblemVersion)) {
            instance.loggedGuild=state.guildId;instance.loggedEmblemVersion=state.emblemVersion;
            DbgLog("[GuildFlag] rendered emblem actor=%u guild=%u version=%u triangles=%u\n",state.gid,state.guildId,state.emblemVersion,triangles-before);
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
} // namespace

bool RenderGr2Monster(const Gr2MonsterState& state,const matrix& view)
{
    Asset* asset=GetAsset(state.job);if(!asset)return false;
    auto& item=instances[state.gid];
    if(!item || item->asset!=asset) {
        item=std::make_unique<Instance>();item->asset=asset;item->facing=180-state.rotation;
    }
    return RenderGr2Instance(state,view,*item,false,timeGetTime());
}

struct Gr2CorpseModel::Impl {
    int job;
    std::shared_ptr<Asset> asset;
    Instance instance;
    explicit Impl(int value):job(value) {
        instance.state=3;
        StartAsset(job);
        const char* path=Gr2MonsterModel(job);
        if(!path)return;
        const auto loading=pending.find(path);
        if(loading!=pending.end())loading->second.wait();
        if(GetAsset(job)) {asset=assets.at(path);instance.asset=asset.get();}
    }
};
Gr2CorpseModel::Gr2CorpseModel(int job):m_impl(std::make_unique<Impl>(job)) {}
Gr2CorpseModel::~Gr2CorpseModel()=default;
float Gr2CorpseModel::Clock() const {return m_impl->instance.clock;}
bool Gr2CorpseModel::Render(const vector3d& position,float rotation,u8 alpha,const matrix& view,double elapsedSeconds)
{
    auto& own=*m_impl;
    if(!own.asset) {
        if(!GetAsset(own.job))return false;
        own.asset=assets.at(Gr2MonsterModel(own.job));
        own.instance.asset=own.asset.get();
    }
    own.instance.facing=rotation;
    const Gr2MonsterState state{0,own.job,3,own.instance.start,position,rotation,1,false,
        (static_cast<u32>(alpha)<<24)|0xFFFFFFu,{},{},{}};
    return RenderGr2Instance(state,view,own.instance,true,0,elapsedSeconds);
}
float Gr2MonsterCurrentRotation(u32 gid,float actorRotation)
{
    const auto found=instances.find(gid);
    return found!=instances.end()?found->second->facing:180-actorRotation;
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
void ClearGr2Monsters() {pending.clear();instances.clear();assets.clear();guildTextures.clear();graffitiTextures.clear();}
void PreloadGr2Monsters() {for(int job:{1288,722,1324,1285,1286,1287})StartAsset(job);}
void WaitForGr2Monsters() {for(auto& load:pending)load.second.wait();}

CTexture* GetGuildGraffitiTexture(u32 guildId)
{
    if (!guildId) return nullptr;
    auto [entry, inserted] = graffitiTextures.try_emplace(guildId);
    if (!inserted) {
        return entry->second.get();
    }
    const std::string path = "data\\texture\\effect\\flagGraffiti" + std::to_string(guildId) + ".ebm";
    std::array<unsigned char, 8192> compressed{};
    std::array<unsigned char, 8192> bitmap{};
    CFile file;
    if (!file.Open(path.c_str()) || file.Size() <= 0 || file.Size() > static_cast<int>(compressed.size())
        || !file.Read(compressed.data(), static_cast<u32>(file.Size()))) {
        return nullptr;
    }
    uLongf length = static_cast<uLongf>(bitmap.size());
    if (uncompress(bitmap.data(), &length, compressed.data(), static_cast<uLong>(file.Size())) != Z_OK) {
        return nullptr;
    }
    entry->second = BuildGuildEmblemTexture(bitmap.data(), static_cast<int>(length), true);
    return entry->second.get();
}

CTexture* Gr2GuildEmblemTexture(u32 guildId,u32 version,const std::vector<unsigned char>* bitmap)
{
    if (!guildId || !version) return nullptr;
    const auto found=guildTextures.find(guildId);
    if(found!=guildTextures.end() && found->second.version==version)return found->second.texture.get();
    if(!bitmap || bitmap->empty())return nullptr;
    auto texture = BuildGuildEmblemTexture(bitmap->data(), static_cast<int>(bitmap->size()), false);
    if (!texture) return nullptr;
    CTexture* result=texture.get();guildTextures[guildId]={version,std::move(texture)};
    DbgLog("[GuildFlag] cached emblem guild=%u version=%u\n",guildId,version);
    return result;
}

u32 PrepareGuildEmblem(CGameMode& mode,u32 guildId,u32 advertisedVersion)
{
    if(!guildId)return 0;
    const auto cached=mode.m_guildEmblems.find(guildId);
    // Zero in a flag spawn is an unspecified version, not an absent emblem.
    // The server's 0152 download supplies its actual current version.
    if(cached!=mode.m_guildEmblems.end() && (!advertisedVersion || cached->second.version==advertisedVersion)) {
        const u32 version=cached->second.version;
        Gr2GuildEmblemTexture(guildId,version,&cached->second.bitmap);
        return version;
    }
    const u32 now=GetTickCount();const auto requested=mode.m_emblemRequestTicks.find(guildId);
    if(requested==mode.m_emblemRequestTicks.end() || now-requested->second>=5000) {
        const u8 packet[6]={0x51,1,static_cast<u8>(guildId),static_cast<u8>(guildId>>8),static_cast<u8>(guildId>>16),static_cast<u8>(guildId>>24)};
        if(CRagConnection::instance()->SendPacket(reinterpret_cast<const char*>(packet),6)) {
            mode.m_emblemRequestTicks[guildId]=now;
            DbgLog("[GuildFlag] request guild=%u advertisedVersion=%u\n",guildId,advertisedVersion);
        }
    }
    return 0;
}
