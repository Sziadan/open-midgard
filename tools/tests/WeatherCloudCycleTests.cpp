#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
using u8=std::uint8_t;
struct vector3d {float x=0,y=0,z=0;};
#include "WeatherCloudGi.inc"
struct View {float GetCameraLongitude(){return 15;}float GetCameraLatitude(){return -45;}} view;
struct Attr {float GetHeight(float,float){return -100;}} attr;
struct World {Attr* m_attr=&attr;} world;
struct CGameMode {View* m_view=&view;World* m_world=&world;} mode;
struct {CGameMode* GetCurrentGameMode(){return &mode;}} g_modeMgr;
struct CRagEffect {vector3d pos;vector3d EffectPosition(){return pos;}} parent;
struct CEffectPrim {vector3d m_pos;CRagEffect* m_master=&parent;std::array<EffectGi,4> m_gi;};
float g_sinTable[360];
#include "WeatherCloudCycleBody.inc"
int main(){
    for(int i=0;i<360;++i)g_sinTable[i]=std::sin(i*3.141592653589793/180);
    assert(!UpdateWeatherCloudPrimitive(nullptr));
    for(int variant:{0,1,2,3,4,5,7,8}){
        parent.pos={-1200,-200,-2300};CEffectPrim cloud;
        for(auto& band:cloud.m_gi){
            band.life=1;band.height[7]=static_cast<float>(variant);band.height[6]=-152;
            band.rotStart=300;band.height[1]=90;band.height[3]=90;
            band.sparkX=parent.pos.x+50;band.sparkZ=parent.pos.z-50;
        }
        int recycled=0,visible=0;
        for(int step=0;step<5000;++step){
            if(step==1800)parent.pos={1500,-220,1700};
            std::array<int,4> previous{};
            std::array<float,4> previousX{},previousZ{};
            for(int i=0;i<4;++i){previous[i]=cloud.m_gi[i].process;previousX[i]=cloud.m_gi[i].sparkX;previousZ[i]=cloud.m_gi[i].sparkZ;}
            assert(UpdateWeatherCloudPrimitive(&cloud));
            assert(cloud.m_pos.x==parent.pos.x&&cloud.m_pos.y==parent.pos.y&&cloud.m_pos.z==parent.pos.z);
            for(int i=0;i<4;++i){
                const auto& band=cloud.m_gi[i];
                assert(band.life==1);
                if(band.process<previous[i]){
                    ++recycled;
                    if(variant==0){
                        // Canonical map zero retains its old offset and adds the
                        // parent again; preserve that separate source behavior.
                        assert(std::abs(band.sparkX-previousX[i]-parent.pos.x)<=0.051f);
                        assert(std::abs(band.sparkZ-previousZ[i]-parent.pos.z)<=0.051f);
                    }else{
                        // Yuno and other randomized modes respawn near the player.
                        assert(std::abs(band.sparkX-parent.pos.x)<=225);
                        assert(std::abs(band.sparkZ-parent.pos.z)<=225);
                    }
                }
                if(step>4500&&band.alphaB>0)++visible;
            }
        }
        assert(recycled>=20&&visible>0);
    }
    std::cout<<"Production weather cloud cycles pass for all eight variants; Yuno respawns near the moving player\n";
}
