#include <algorithm>
#include <cmath>
#include <cstdio>

struct tlvertex3d {
    float x, y, z, oow;
    unsigned int color, specular;
    float tu, tv;
};

int main()
{
    struct {
        float renderZ[4] = {0.980f, 0.980f, 0.984f, 0.984f};
        float renderOow[4] = {0.0024f, 0.0024f, 0.0020f, 0.0020f};
        float renderY[4] = {150.0f, 150.0f, 350.0f, 350.0f};
    } entry;
    struct { tlvertex3d m_verts[4]; } face{};
    auto* aura = &face;
    const float leftRatio = 0.1f, rightRatio = 0.9f;
    const float topRatio = 0.25f, bottomRatio = 0.75f;
    const float x[4] = {100.25f, 140.75f, 100.25f, 140.75f};
    const float y[4] = {200.0f, 200.0f, 300.0f, 300.0f};
    const float u[4] = {0, 1, 0, 1}, v[4] = {0, 0, 1, 1};
    float expansion = 5.5f;
    const float kBillboardDepthBias = 0.0005f;
    const unsigned int billboardColor = 0xffffffffu;

    // Exercise the actual production interpolation and vertex submission.
    auto buildVertices = [&] {
#include "AssumptioVertices.inc"
        return interpolate(100.25f, 140.75f, 100.25f, 140.75f, 0.5f, 0.5f);
    };
    const float screenMiddle = buildVertices();

    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        if (!ok) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    check(std::fabs(screenMiddle - 120.5f) < 0.0001f,
        "fractional screen positions survive interpolation");
    for (int i = 0; i < 4; ++i) {
        const auto& vertex = face.m_verts[i];
        const float playerDepth = i < 2 ? 0.98039f : 0.98261f;
        const float expectedOow = i < 2 ? 0.002311f : 0.002089f;
        check(std::fabs(vertex.z - playerDepth) < 0.000001f,
            "aura matches its player's fractional billboard depth");
        check(std::fabs(vertex.oow - expectedOow) < 0.0000001f,
            "inverse depth stays positive and fractional");
        check(!(vertex.z <= 0.979f), "foreground wall/NPC rejects aura with LEQUAL");
        check(vertex.z <= 0.99f, "background scene depth allows aura");
        check(vertex.x == x[i] + ((i % 2) ? expansion : -expansion), "horizontal expansion preserved");
        check(vertex.y == y[i] + ((i < 2) ? -expansion : expansion), "vertical expansion preserved");
    }
    // Along the expanded quad, the aura must stay on the player's depth
    // plane, so it overlays the head as well as the body without crossing
    // forward through scene occluders.
    for (float screenY : {210.0f, 250.0f, 290.0f}) {
        const float t = (screenY - face.m_verts[0].y) / (face.m_verts[2].y - face.m_verts[0].y);
        const float auraDepth = face.m_verts[0].z + (face.m_verts[2].z - face.m_verts[0].z) * t;
        const float bodyDepth = 0.980f + 0.004f * ((screenY - 150.0f) / 200.0f) - kBillboardDepthBias;
        check(std::fabs(auraDepth - bodyDepth) < 0.000001f, "expanded aura follows player depth plane across head/body");
        check(auraDepth <= bodyDepth, "aura passes own-player LEQUAL depth test");
    }
    // The body and expanded glow are rasterized as separate triangles. Sweep
    // pulse sizes and subpixel projections to catch self-depth flicker even
    // with an unchanged pose. Compare against the body's submitted Z values.
    int rejectedOwnPixels = 0;
    for (int frame = 0; frame < 1000; ++frame) {
        expansion = 3.5f + static_cast<float>(frame % 181) / 60.0f;
        entry.renderY[0] = entry.renderY[1] = 140.1f + (frame % 23) * 0.013f;
        entry.renderY[2] = entry.renderY[3] = 350.2f + (frame % 31) * 0.017f;
        entry.renderZ[0] = entry.renderZ[1] = 0.96f + (frame % 89) * 0.0001f;
        entry.renderZ[2] = entry.renderZ[3] = entry.renderZ[0] + 0.00317f;
        buildVertices();
        const float bodyTop = entry.renderZ[0] - kBillboardDepthBias;
        const float bodyBottom = entry.renderZ[2] - kBillboardDepthBias;
        for (int sample = 0; sample <= 100; ++sample) {
            const float screenY = face.m_verts[0].y + (face.m_verts[2].y - face.m_verts[0].y) * sample / 100.0f;
            const float auraT = (screenY - face.m_verts[0].y) / (face.m_verts[2].y - face.m_verts[0].y);
            const float bodyT = (screenY - entry.renderY[0]) / (entry.renderY[2] - entry.renderY[0]);
            const float auraDepth = face.m_verts[0].z + (face.m_verts[2].z - face.m_verts[0].z) * auraT;
            const float bodyDepth = bodyTop + (bodyBottom - bodyTop) * bodyT;
            if (auraDepth > bodyDepth) ++rejectedOwnPixels;
            check(auraDepth > bodyDepth - 0.000001f, "self-depth allowance stays below scene-scale bias");
        }
    }
    if (rejectedOwnPixels) std::fprintf(stderr, "Rejected %d own-player samples\n", rejectedOwnPixels);
    check(rejectedOwnPixels == 0, "pulse/projection changes do not intermittently reject aura on own sprite");
    return failures ? 1 : 0;
}
