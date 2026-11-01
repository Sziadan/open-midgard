#include <cassert>
#include <iostream>
using u8=unsigned char;
int g_isSkipFrame=1;
struct vector3d {float x=0,y=0,z=0;};
struct CRenderObject {virtual ~CRenderObject()=default;vector3d m_pos;float m_roty=0;int m_BeZero=0,m_BodyLight=0;};
struct CSkill : CRenderObject {};
bool EffectTicksWhenNoFrameTime(int){return true;}
struct CRagEffect {
    CRenderObject owner;vector3d m_cachedPos;float m_longitude=0;
    CRenderObject* master=&owner;
    int m_type=312,m_stateCnt=0,m_duration=100,emissions=0;bool m_isWorldEffect=false;
    CRenderObject* ResolveCurrentMaster(){return master;}
    u8 ProcessWorldEffect(){return 1;}
    bool ProcessEffectFrame(){++emissions;return true;}
    u8 OnProcess();
};
#include "HealTimingBody.inc"
int main()
{
    for(int type:{312,313,325})for(int refresh:{1,2,3,8}) {
        CRagEffect heal;heal.m_type=type;
        for(int frame=0;frame<60*refresh;++frame) {
            g_isSkipFrame=(frame%refresh)==0;
            assert(heal.OnProcess());
        }
        assert(heal.m_stateCnt==60&&heal.emissions==60);
        for(int frame=0;frame<39*refresh;++frame) {
            g_isSkipFrame=(frame%refresh)==0;
            assert(heal.OnProcess());
        }
        assert(heal.m_stateCnt==99);g_isSkipFrame=1;
        assert(!heal.OnProcess()&&heal.m_stateCnt==100);
    }
    CRagEffect other;other.m_type=37;g_isSkipFrame=0;
    assert(other.OnProcess()&&other.m_stateCnt==1); // Other effect policies unchanged.
    CSkill unit;
    for(int refresh:{1,3,8}) {
        CRagEffect flame;flame.master=&unit;flame.m_type=25;
        for(int frame=0;frame<60*refresh;++frame) {
            g_isSkipFrame=(frame%refresh)==0;
            assert(flame.OnProcess());
        }
        assert(flame.m_stateCnt==60 && flame.emissions==60);
    }
    std::cout<<"Production Heal timing checks passed at 1/2/3/8 render frames per logic tick\n";
}
