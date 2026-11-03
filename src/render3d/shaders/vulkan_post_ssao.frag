#version 450

layout(push_constant) uniform DrawConstants {
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
} drawConstants;

layout(set = 0, binding = 0) uniform texture2D sceneTexture;
layout(set = 0, binding = 1) uniform texture2D depthTexture;
layout(set = 0, binding = 2) uniform sampler sceneSampler;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = inUv;
    if ((drawConstants.flags & 2u) != 0u) {
        // Reference confusion shifts each row by 1% of the screen width.
        // Normalize the 480-line waveform so render scale does not change it.
        uv.x = clamp(uv.x + 0.01 * sin(drawConstants.alphaRef + uv.y * 480.0 * 0.034906585), 0.0, 1.0);
    }
    vec4 color = texture(sampler2D(sceneTexture, sceneSampler), uv);
    if ((drawConstants.flags & 4u) != 0u) {
        vec2 delta = (inUv - vec2(drawConstants.fogStart, drawConstants.fogEnd))
            * vec2(drawConstants.screenWidth, drawConstants.screenHeight);
        float radius = max(drawConstants.fogColorR, 1.0);
        // Preserve the clear center and extend visibility with a broad fade.
        color.rgb *= 1.0 - smoothstep(radius * 0.75, radius * 2.0, length(delta));
    }
    if ((drawConstants.flags & 1u) == 0u) {
        outColor = color;
        return;
    }
    float centerDepth = texture(sampler2D(depthTexture, sceneSampler), uv).r;
    if (centerDepth >= 0.9999) {
        outColor = color;
        return;
    }

    vec2 pixel = vec2(
        1.0 / max(drawConstants.screenWidth, 1.0),
        1.0 / max(drawConstants.screenHeight, 1.0));
    const vec2 directions[12] = vec2[12](
        vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
        vec2(0.7071, 0.7071), vec2(-0.7071, 0.7071), vec2(0.7071, -0.7071), vec2(-0.7071, -0.7071),
        vec2(0.9239, 0.3827), vec2(-0.9239, -0.3827), vec2(0.3827, -0.9239), vec2(-0.3827, 0.9239));

    float bias = 0.001 + centerDepth * 0.003;
    float occlusion = 0.0;
    for (int index = 0; index < 12; ++index) {
        float radius = (index < 8) ? 3.0 : 6.0;
        float sampleDepth = texture(
            sampler2D(depthTexture, sceneSampler),
            uv + directions[index] * pixel * radius).r;
        occlusion += sampleDepth + bias < centerDepth ? 1.0 : 0.0;
    }

    float ao = 1.0 - min(0.48, occlusion * 0.04);
    outColor = vec4(color.rgb * ao, color.a);
}
