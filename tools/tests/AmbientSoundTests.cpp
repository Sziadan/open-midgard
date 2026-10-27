#include <cmath>
#include <cstdint>
#include <cstdio>

constexpr float kAmbientSoundBoundarySlack = 75.0f;
constexpr float kAmbientSoundCullSlack = 130.0f;
#include "AmbientSoundGeometry.inc"

int main()
{
    int checks = 0;
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) {
            ++failures;
            std::fprintf(stderr, "FAIL: %s\n", name);
        }
    };

    AmbientSoundTrigger trigger;
    check(ResolveAmbientSoundTrigger(100.0f, 199.0f, 0.0f, 0.0f,
            10, 19, 1000.0f, std::sqrt(461.0f), &trigger)
        && std::fabs(trigger.relativeX + 95.0f) < 0.0001f
        && std::fabs(trigger.relativeZ + 189.05f) < 0.0001f,
        "source truncates slope before comparing with integer height/width ratio in quadrant 0");

    check(ResolveAmbientSoundTrigger(-100.0f, 199.0f, 0.0f, 0.0f,
            10, 19, 1000.0f, std::sqrt(461.0f), &trigger)
        && std::fabs(trigger.relativeX - 95.0f) < 0.0001f
        && std::fabs(trigger.relativeZ + 189.05f) < 0.0001f,
        "horizontal edge selects the source-signed X/Z corner in quadrant 1");

    check(ResolveAmbientSoundTrigger(100.0f, 250.0f, 0.0f, 0.0f,
            10, 19, 1000.0f, std::sqrt(461.0f), &trigger)
        && std::fabs(trigger.relativeX + 96.2f) < 0.0001f
        && std::fabs(trigger.relativeZ + 240.5f) < 0.0001f,
        "steep source slope selects the signed vertical rectangle edge");

    check(!ResolveAmbientSoundTrigger(100.0f, 0.0f, 0.0f, 0.0f,
            10, 10, 19.99f, std::sqrt(200.0f), &trigger),
        "audibility edge uses the strict source comparison and rejects distance-range above 75");
    check(ResolveAmbientSoundTrigger(100.0f, 0.0f, 0.0f, 0.0f,
            10, 10, 20.0f, std::sqrt(200.0f), &trigger),
        "audibility edge accepts distance-range at or below 75");

    check(!ResolveAmbientSoundTrigger(1000.0f, 0.0f, 0.0f, 0.0f,
            10, 10, 10.0f, std::sqrt(200.0f), &trigger),
        "source center cull rejects listeners more than 130 beyond diagonal plus range");

    std::printf("Ambient sound geometry: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
