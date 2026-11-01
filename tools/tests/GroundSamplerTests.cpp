#include "render3d/ModernRenderState.h"
#include <cassert>
#include <iostream>
#include <vector>
using VkFilter=int;
using VkSamplerMipmapMode=int;
using VkSamplerAddressMode=int;
using VkSampler=unsigned long long;
constexpr int VK_FILTER_NEAREST=0,VK_FILTER_LINEAR=1,VK_SAMPLER_MIPMAP_MODE_LINEAR=1,
    VK_SAMPLER_MIPMAP_MODE_NEAREST=0,VK_SAMPLER_ADDRESS_MODE_REPEAT=0,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE=2,VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO=31,
    VK_FALSE=0,VK_SUCCESS=0;
constexpr VkSampler VK_NULL_HANDLE=0;
struct VkSamplerCreateInfo {
    int sType=0,magFilter=0,minFilter=0,mipmapMode=0,addressModeU=0,addressModeV=0,addressModeW=0;
    float minLod=0,maxLod=0,maxAnisotropy=0;
    int anisotropyEnable=0;
};
std::vector<VkSamplerCreateInfo> created;
int vkCreateSampler(int,const VkSamplerCreateInfo* info,void*,VkSampler* out) {
    created.push_back(*info); *out=created.size(); return VK_SUCCESS;
}
struct Device {
    ModernFixedFunctionState m_pipelineState{};
    int m_device=1;
    void SetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD value) {
        ApplyModernTextureStageState(&m_pipelineState,stage,type,value);
    }
    void BindTexture(int,void*) {}
#include "GroundSamplerBody.inc"
};
constexpr int kSpriteTextureMode=4,kClampedGroundTextureMode=5;
enum class SpriteFilterMode { Sharp,Smooth };
struct Graphics { SpriteFilterMode spriteFilter=SpriteFilterMode::Smooth; } graphics;
const Graphics& GetCachedGraphicsSettings() { return graphics; }
struct CRenderer {
    Device* m_renderDevice;
    void SetMultiTextureMode(int);
};
#include "GroundTextureModeBody.inc"
int main() {
    Device device; ResetModernFixedFunctionState(&device.m_pipelineState);
    CRenderer renderer{&device};
    const auto wrapped=device.GetSamplerState();
    assert(created.back().addressModeU==VK_SAMPLER_ADDRESS_MODE_REPEAT);
    ApplyModernTextureStageState(&device.m_pipelineState,0,D3DTSS_ADDRESS,D3DTADDRESS_CLAMP);
    const auto clamped=device.GetSamplerState();
    assert(clamped!=wrapped && created.size()==2);
    assert(created.back().addressModeU==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    assert(created.back().addressModeV==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    assert(device.GetSamplerState()==clamped && created.size()==2);
    ApplyModernTextureStageState(&device.m_pipelineState,0,D3DTSS_ADDRESS,D3DTADDRESS_WRAP);
    assert(device.GetSamplerState()==wrapped && created.size()==2);
    ResetModernFixedFunctionState(&device.m_pipelineState);
    assert(device.m_pipelineState.textureStages[0].address==D3DTADDRESS_WRAP);
    renderer.SetMultiTextureMode(kClampedGroundTextureMode);
    assert(device.GetSamplerState()==clamped);
    assert(device.m_pipelineState.textureStages[0].alphaOp==D3DTOP_MODULATE);
    for (int mode : {0,1,2,3,kSpriteTextureMode}) {
        renderer.SetMultiTextureMode(kClampedGroundTextureMode);
        renderer.SetMultiTextureMode(mode);
        const auto selected=device.GetSamplerState();
        assert(created[static_cast<size_t>(selected-1)].addressModeU==VK_SAMPLER_ADDRESS_MODE_REPEAT);
        assert(created[static_cast<size_t>(selected-1)].addressModeV==VK_SAMPLER_ADDRESS_MODE_REPEAT);
    }
    std::cout << "Production sampler checks passed: ground preset, separate repeat/clamp cache entries and wrap restoration for other presets\n";
}
