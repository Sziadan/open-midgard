#include "ModernRenderState.h"

void ResetModernFixedFunctionState(ModernFixedFunctionState* state)
{
    if (!state) {
        return;
    }

    state->alphaRef = 207u;
    state->alphaTestEnable = TRUE;
    state->alphaBlendEnable = FALSE;
    state->depthEnable = TRUE;
    state->depthWriteEnable = TRUE;
    state->cullMode = D3DCULL_NONE;
    state->colorKeyEnable = TRUE;
    state->srcBlend = D3DBLEND_SRCALPHA;
    state->destBlend = D3DBLEND_INVSRCALPHA;
    ResetModernTextureStageStates(state->textureStages);
}

void ApplyModernRenderState(ModernFixedFunctionState* state, D3DRENDERSTATETYPE renderState, DWORD value)
{
    if (!state) {
        return;
    }

    switch (renderState) {
    case D3DRENDERSTATE_ALPHAREF: state->alphaRef = static_cast<unsigned int>(value & 0xFFu); break;
    case D3DRENDERSTATE_ALPHATESTENABLE: state->alphaTestEnable = value; break;
    case D3DRENDERSTATE_ALPHABLENDENABLE: state->alphaBlendEnable = value; break;
    case D3DRENDERSTATE_ZENABLE: state->depthEnable = value; break;
    case D3DRENDERSTATE_ZWRITEENABLE: state->depthWriteEnable = value; break;
    case D3DRENDERSTATE_CULLMODE: state->cullMode = static_cast<D3DCULL>(value); break;
    case D3DRENDERSTATE_COLORKEYENABLE: state->colorKeyEnable = value; break;
    case D3DRENDERSTATE_SRCBLEND: state->srcBlend = static_cast<D3DBLEND>(value); break;
    case D3DRENDERSTATE_DESTBLEND: state->destBlend = static_cast<D3DBLEND>(value); break;
    default: break;
    }
}

void ApplyModernTextureStageState(ModernFixedFunctionState* state, DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)
{
    if (!state || stage >= 2) {
        return;
    }

    ModernTextureStageState& stageState = state->textureStages[stage];
    switch (type) {
    case D3DTSS_TEXCOORDINDEX: stageState.texCoordIndex = value; break;
    case D3DTSS_COLORARG1: stageState.colorArg1 = value; break;
    case D3DTSS_COLOROP: stageState.colorOp = value; break;
    case D3DTSS_COLORARG2: stageState.colorArg2 = value; break;
    case D3DTSS_ALPHAARG1: stageState.alphaArg1 = value; break;
    case D3DTSS_ALPHAOP: stageState.alphaOp = value; break;
    case D3DTSS_ALPHAARG2: stageState.alphaArg2 = value; break;
    case D3DTSS_MINFILTER: stageState.minFilter = value; break;
    case D3DTSS_MAGFILTER: stageState.magFilter = value; break;
    case D3DTSS_MIPFILTER: stageState.mipFilter = value; break;
    default: break;
    }
}

void ResetModernTextureStageStates(ModernTextureStageState states[2])
{
    if (!states) {
        return;
    }

    states[0] = { 0, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTFN_LINEAR, D3DTFG_LINEAR, D3DTFP_POINT };
    states[1] = { 1, D3DTA_TEXTURE, D3DTOP_DISABLE, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_DISABLE, D3DTA_CURRENT, D3DTFN_LINEAR, D3DTFG_LINEAR, D3DTFP_POINT };
}

unsigned int BuildModernDrawFlags(DWORD vertexFormat,
    const ModernFixedFunctionState& state,
    bool hasTexture0,
    bool hasTexture1)
{
    unsigned int flags = 0u;
    if (hasTexture0 && state.textureStages[0].colorOp != D3DTOP_DISABLE) {
        flags |= ModernDrawFlag_Texture0Enabled;
    }
    if (hasTexture1
        && vertexFormat == kModernLightmapFvf
        && state.textureStages[1].colorOp == D3DTOP_MODULATE
        && state.textureStages[1].colorArg1 == (D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE)
        && state.textureStages[1].colorArg2 == D3DTA_CURRENT) {
        flags |= ModernDrawFlag_Texture1Enabled | ModernDrawFlag_Stage1LightmapAlpha;
    }
    if (state.alphaTestEnable != FALSE) {
        flags |= ModernDrawFlag_AlphaTestEnabled;
    }
    if (state.colorKeyEnable != FALSE) {
        flags |= ModernDrawFlag_ColorKeyEnabled;
    }
    if (hasTexture0
        && state.textureStages[0].alphaOp == D3DTOP_SELECTARG1
        && state.textureStages[0].alphaArg1 == D3DTA_TEXTURE) {
        flags |= ModernDrawFlag_Stage0AlphaUseTexture;
    }
    if (hasTexture0
        && state.textureStages[0].alphaOp == D3DTOP_MODULATE
        && state.textureStages[0].alphaArg1 == D3DTA_TEXTURE
        && state.textureStages[0].alphaArg2 == D3DTA_DIFFUSE) {
        flags |= ModernDrawFlag_Stage0AlphaModulate;
    }
    return flags;
}

std::vector<unsigned short> BuildTriangleFanIndices(const unsigned short* indices, DWORD vertexCount, DWORD indexCount)
{
    std::vector<unsigned short> expanded;
    const DWORD sourceCount = indices && indexCount > 0 ? indexCount : vertexCount;
    if (sourceCount < 3) {
        return expanded;
    }

    expanded.reserve(static_cast<size_t>(sourceCount - 2) * 3u);
    for (DWORD i = 1; i + 1 < sourceCount; ++i) {
        expanded.push_back(indices ? indices[0] : static_cast<unsigned short>(0));
        expanded.push_back(indices ? indices[i] : static_cast<unsigned short>(i));
        expanded.push_back(indices ? indices[i + 1] : static_cast<unsigned short>(i + 1));
    }
    return expanded;
}
