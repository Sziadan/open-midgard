#pragma once

#include <d3d.h>
#include <vector>

struct ModernTextureStageState {
    DWORD texCoordIndex;
    DWORD colorArg1;
    DWORD colorOp;
    DWORD colorArg2;
    DWORD alphaArg1;
    DWORD alphaOp;
    DWORD alphaArg2;
    DWORD minFilter;
    DWORD magFilter;
    DWORD mipFilter;
};

struct ModernFixedFunctionState {
    unsigned int alphaRef;
    DWORD alphaTestEnable;
    DWORD alphaBlendEnable;
    DWORD depthEnable;
    DWORD depthWriteEnable;
    D3DCULL cullMode;
    DWORD colorKeyEnable;
    D3DBLEND srcBlend;
    D3DBLEND destBlend;
    ModernTextureStageState textureStages[2];
};

struct ModernDrawConstants {
    float screenWidth;
    float screenHeight;
    float alphaRef;
    unsigned int flags;
};

enum ModernDrawFlags : unsigned int {
    ModernDrawFlag_Texture0Enabled = 1u << 0,
    ModernDrawFlag_Texture1Enabled = 1u << 1,
    ModernDrawFlag_AlphaTestEnabled = 1u << 2,
    ModernDrawFlag_ColorKeyEnabled = 1u << 3,
    ModernDrawFlag_Stage0AlphaUseTexture = 1u << 4,
    ModernDrawFlag_Stage0AlphaModulate = 1u << 5,
    ModernDrawFlag_Stage1LightmapAlpha = 1u << 6,
};

constexpr DWORD kModernLightmapFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2;

void ResetModernFixedFunctionState(ModernFixedFunctionState* state);
void ApplyModernRenderState(ModernFixedFunctionState* state, D3DRENDERSTATETYPE renderState, DWORD value);
void ApplyModernTextureStageState(ModernFixedFunctionState* state, DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value);
void ResetModernTextureStageStates(ModernTextureStageState states[2]);
unsigned int BuildModernDrawFlags(DWORD vertexFormat,
    const ModernFixedFunctionState& state,
    bool hasTexture0,
    bool hasTexture1);
std::vector<unsigned short> BuildTriangleFanIndices(const unsigned short* indices, DWORD vertexCount, DWORD indexCount);
