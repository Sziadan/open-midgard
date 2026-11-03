#include "res/Gr2Reader.h"
#include <QCoreApplication>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
ro::gr2::Asset Read(const std::string& path,bool animationOnly=false) {
    std::ifstream file(path,std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    ro::gr2::Asset result;std::string error;
    if(!ro::gr2::Decode(bytes,result,error,animationOnly))throw std::runtime_error(path+": "+error);
    return result;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc!=2)return 2;
    const std::string output=argv[1];
    std::filesystem::create_directories(output);
    int type=6;
    for(const char* name:{"kguardian90_7.gr2","aguardian90_8.gr2","sguardian90_9.gr2"}) {
        ++type;
        auto model=Read(std::string("Ref/GRF-Content/data/model/3dmob/")+name);
        assert(model.bones.size()==(type==7?35:type==8?43:31));
        assert(model.images.size()==(type==7?5:4));
        std::ofstream pixels(output+"/"+name+".pixels",std::ios::binary);
        for(const auto& image:model.images)pixels.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size()*4);
        for(const char* action:{"idle","move","attack","dead","damage"}) {
            auto bank=std::string(action)=="idle"?ro::gr2::Asset{}:Read("Ref/GRF-Content/data/model/3dmob_bone/"+std::to_string(type)+"_"+action+".gr2",true);
            const auto& animation=std::string(action)=="idle"?model.animations.at(0):bank.animations.at(0);
            std::ofstream poses(output+"/"+name+"."+action+".pose",std::ios::binary);
            for(double fraction:{0.,.25,.5,.9}) {
                auto pose=ro::gr2::Sample(model,&animation,animation.duration*fraction,false);
                for(const auto& matrix:pose)for(float value:matrix)assert(std::isfinite(value));
                poses.write(reinterpret_cast<const char*>(pose.data()),pose.size()*16*sizeof(float));
                for(const auto& mesh:model.meshes) {
                    std::vector<std::array<float,3>> positions,normals;
                    ro::gr2::Skin(mesh,pose,positions,normals);
                    for(const auto& position:positions)for(float value:position)assert(std::isfinite(value));
                }
                const auto looped=ro::gr2::Sample(model,&animation,animation.duration*(fraction+2),true);
                for(size_t i=0;i<pose.size();++i)for(int k=0;k<16;++k)assert(std::fabs(looped[i][k]-pose[i][k])<.001f);
            }
        }
        std::cout<<"PASS: independent textures, all animation banks and finite skinning: "<<name<<"\n";
    }
    for(const auto& bytes:{std::vector<unsigned char>{},std::vector<unsigned char>(32,0),std::vector<unsigned char>{1,2,3}}) {
        ro::gr2::Asset asset;std::string error;assert(!ro::gr2::Decode(bytes,asset,error));assert(!error.empty());
    }
}
