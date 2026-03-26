struct DrawConstants
{
    float screenWidth;
    float screenHeight;
    float alphaRef;
    uint flags;
};

[[vk::push_constant]] DrawConstants g_drawConstants;
[[vk::binding(0, 0)]] Texture2D g_texture0 : register(t0);
[[vk::binding(1, 0)]] SamplerState g_sampler0 : register(s0);

struct VSOutputPost {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutputPost VSMainPost(uint vertexId : SV_VertexID)
{
    VSOutputPost output;
    float2 clipPos;
    clipPos.x = (vertexId == 2u) ? 3.0f : -1.0f;
    clipPos.y = (vertexId == 1u) ? 3.0f : -1.0f;
    output.pos = float4(clipPos, 0.0f, 1.0f);
    output.uv = float2((clipPos.x + 1.0f) * 0.5f, (clipPos.y + 1.0f) * 0.5f);
    return output;
}

float ComputeFxaaLuma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

float4 PSMainFXAA(VSOutputPost input) : SV_Target
{
    float2 invResolution = float2(
        1.0f / max(g_drawConstants.screenWidth, 1.0f),
        1.0f / max(g_drawConstants.screenHeight, 1.0f));

    float4 centerSample = g_texture0.Sample(g_sampler0, input.uv);
    float3 rgbM = centerSample.rgb;
    float lumaM = ComputeFxaaLuma(rgbM);

    float3 rgbNW = g_texture0.Sample(g_sampler0, input.uv + float2(-1.0f, -1.0f) * invResolution).rgb;
    float3 rgbNE = g_texture0.Sample(g_sampler0, input.uv + float2(1.0f, -1.0f) * invResolution).rgb;
    float3 rgbSW = g_texture0.Sample(g_sampler0, input.uv + float2(-1.0f, 1.0f) * invResolution).rgb;
    float3 rgbSE = g_texture0.Sample(g_sampler0, input.uv + float2(1.0f, 1.0f) * invResolution).rgb;

    float lumaNW = ComputeFxaaLuma(rgbNW);
    float lumaNE = ComputeFxaaLuma(rgbNE);
    float lumaSW = ComputeFxaaLuma(rgbSW);
    float lumaSE = ComputeFxaaLuma(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    float threshold = max(0.03125f, lumaMax * 0.125f);
    if (lumaRange < threshold) {
        return centerSample;
    }

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = (lumaNW + lumaSW) - (lumaNE + lumaSE);

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f / 8.0f),
        1.0f / 128.0f);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, float2(-8.0f, -8.0f), float2(8.0f, 8.0f)) * invResolution;

    float3 rgbA = 0.5f * (
        g_texture0.Sample(g_sampler0, input.uv + dir * (1.0f / 3.0f - 0.5f)).rgb +
        g_texture0.Sample(g_sampler0, input.uv + dir * (2.0f / 3.0f - 0.5f)).rgb);
    float3 rgbB = rgbA * 0.5f + 0.25f * (
        g_texture0.Sample(g_sampler0, input.uv + dir * -0.5f).rgb +
        g_texture0.Sample(g_sampler0, input.uv + dir * 0.5f).rgb);
    float lumaB = ComputeFxaaLuma(rgbB);

    return float4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, centerSample.a);
}