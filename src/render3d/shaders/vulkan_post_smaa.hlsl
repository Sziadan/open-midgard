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

float ComputeSmaaLuma(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

float ComputeSmaaColorDelta(float3 a, float3 b)
{
    const float3 delta = abs(a - b);
    return max(delta.r, max(delta.g, delta.b));
}

float4 PSMainSMAAEdge(VSOutputPost input) : SV_Target
{
    const float2 invResolution = float2(
        1.0f / max(g_drawConstants.screenWidth, 1.0f),
        1.0f / max(g_drawConstants.screenHeight, 1.0f));

    const float4 centerSample = g_texture0.Sample(g_sampler0, input.uv);
    const float3 leftSample = g_texture0.Sample(g_sampler0, input.uv + float2(-1.0f, 0.0f) * invResolution).rgb;
    const float3 topSample = g_texture0.Sample(g_sampler0, input.uv + float2(0.0f, -1.0f) * invResolution).rgb;
    const float3 rightSample = g_texture0.Sample(g_sampler0, input.uv + float2(1.0f, 0.0f) * invResolution).rgb;
    const float3 bottomSample = g_texture0.Sample(g_sampler0, input.uv + float2(0.0f, 1.0f) * invResolution).rgb;

    const float lumaCenter = ComputeSmaaLuma(centerSample.rgb);
    const float lumaLeft = ComputeSmaaLuma(leftSample);
    const float lumaTop = ComputeSmaaLuma(topSample);
    const float lumaRight = ComputeSmaaLuma(rightSample);
    const float lumaBottom = ComputeSmaaLuma(bottomSample);

    const float lumaHorizontal = max(abs(lumaCenter - lumaTop), abs(lumaCenter - lumaBottom));
    const float lumaVertical = max(abs(lumaCenter - lumaLeft), abs(lumaCenter - lumaRight));
    const float colorHorizontal = max(ComputeSmaaColorDelta(centerSample.rgb, topSample), ComputeSmaaColorDelta(centerSample.rgb, bottomSample));
    const float colorVertical = max(ComputeSmaaColorDelta(centerSample.rgb, leftSample), ComputeSmaaColorDelta(centerSample.rgb, rightSample));

    const float threshold = 0.050f;
    const float horizontalEdge = step(threshold, max(lumaHorizontal, colorHorizontal));
    const float verticalEdge = step(threshold, max(lumaVertical, colorVertical));

    return float4(horizontalEdge, verticalEdge, 0.0f, centerSample.a);
}