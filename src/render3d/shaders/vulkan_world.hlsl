struct DrawConstants
{
    float screenWidth;
    float screenHeight;
    float alphaRef;
    float fogStart;
    float fogEnd;
    float fogColorR;
    float fogColorG;
    float fogColorB;
    uint flags;
    float padding0;
    float padding1;
    float padding2;
};

[[vk::push_constant]] DrawConstants g_drawConstants;
[[vk::binding(0, 0)]] Texture2D g_texture0 : register(t0);
[[vk::binding(1, 0)]] Texture2D g_texture1 : register(t1);
[[vk::binding(2, 0)]] SamplerState g_sampler0 : register(s0);

struct VSInputTL {
    float4 pos : POSITION0;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
};

struct VSInputLM {
    float4 pos : POSITION0;
    float4 color : COLOR0;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct VSOutput {
    float4 pos : SV_Position;
    noperspective float4 color : COLOR0;
    noperspective float2 uv0 : TEXCOORD0;
    noperspective float2 uv1 : TEXCOORD1;
    noperspective float fogDepth : TEXCOORD2;
};

VSOutput VSMainTL(VSInputTL input)
{
    VSOutput output;
    float rhw = max(input.pos.w, 1e-6f);
    float clipW = 1.0f / rhw;
    float ndcX = (input.pos.x / max(g_drawConstants.screenWidth, 1.0f)) * 2.0f - 1.0f;
    float ndcY = 1.0f - (input.pos.y / max(g_drawConstants.screenHeight, 1.0f)) * 2.0f;
    output.pos = float4(ndcX * clipW, ndcY * clipW, input.pos.z * clipW, clipW);
    output.color = input.color;
    output.uv0 = input.uv0;
    output.uv1 = float2(0.0f, 0.0f);
    output.fogDepth = clipW;
    return output;
}

VSOutput VSMainLM(VSInputLM input)
{
    VSOutput output;
    float rhw = max(input.pos.w, 1e-6f);
    float clipW = 1.0f / rhw;
    float ndcX = (input.pos.x / max(g_drawConstants.screenWidth, 1.0f)) * 2.0f - 1.0f;
    float ndcY = 1.0f - (input.pos.y / max(g_drawConstants.screenHeight, 1.0f)) * 2.0f;
    output.pos = float4(ndcX * clipW, ndcY * clipW, input.pos.z * clipW, clipW);
    output.color = input.color;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.fogDepth = clipW;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 color = input.color;
    float tex0Alpha = 1.0f;

    if ((g_drawConstants.flags & 1u) != 0u) {
        float4 tex0 = g_texture0.Sample(g_sampler0, input.uv0);
        color.rgb *= tex0.rgb;
        tex0Alpha = tex0.a;
        if ((g_drawConstants.flags & 16u) != 0u) {
            color.a = tex0Alpha;
        } else if ((g_drawConstants.flags & 32u) != 0u) {
            color.a *= tex0Alpha;
        }
    }

    if ((g_drawConstants.flags & 64u) != 0u) {
        float lightmapAlpha = g_texture1.Sample(g_sampler0, input.uv1).a;
        color.rgb *= lightmapAlpha.xxx;
    }
    if ((g_drawConstants.flags & 256u) != 0u) {
        float4 lightmap = g_texture1.Sample(g_sampler0, input.uv1);
        color.rgb = saturate(color.rgb * lightmap.a + lightmap.rgb);
    }

    if ((g_drawConstants.flags & 128u) != 0u) {
        float fogRange = max(g_drawConstants.fogEnd - g_drawConstants.fogStart, 1e-6f);
        float fogVisibility = 1.0f - saturate((input.fogDepth - g_drawConstants.fogStart) / fogRange);
        color.rgb = lerp(float3(g_drawConstants.fogColorR, g_drawConstants.fogColorG, g_drawConstants.fogColorB), color.rgb, fogVisibility);
    }

    if ((g_drawConstants.flags & 8u) != 0u && (g_drawConstants.flags & 1u) != 0u && tex0Alpha <= 0.0f) {
        discard;
    }

    if ((g_drawConstants.flags & 4u) != 0u && color.a <= g_drawConstants.alphaRef) {
        discard;
    }

    return color;
}
