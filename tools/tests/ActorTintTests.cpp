#include "world/ActorTint.h"

#include <array>
#include <cstdio>

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

    check(!ro::actor::IsSingleColorExpired(true, 1000u, [] { return 1100u; }),
        "single-color timeout keeps the source strict greater-than boundary");
    check(ro::actor::IsSingleColorExpired(true, 1000u, [] { return 1101u; }),
        "single-color timeout expires one tick after 100 ms");
    bool sampledInactiveClock = false;
    check(!ro::actor::IsSingleColorExpired(false, 1000u, [&] {
              sampledInactiveClock = true;
              return 2000u;
          }) && !sampledInactiveClock,
        "inactive single-color state does not sample the clock");

    std::uint8_t alpha = 0;
    std::uint8_t red = 247;
    std::uint8_t green = 255;
    std::uint8_t blue = 16;
    ro::actor::AdvanceTint(alpha, red, green, blue, false);
    check(alpha == 8 && red == 255 && green == 255 && blue == 24,
        "source ModifyArgb adds eight and clamps every channel at 255");

    const auto fixedBefore = std::array<std::uint8_t, 4>{ alpha, red, green, blue };
    ro::actor::AdvanceTint(alpha, red, green, blue, true);
    check(fixedBefore == std::array<std::uint8_t, 4>{ alpha, red, green, blue },
        "fixed sprite color suppresses ModifyArgb channel advancement");

    check(ro::actor::ResolveBillboardColor(128, 255, 255, 255, 255,
              false, 0xFFFF0000u) == 0x80FFFFFFu,
        "normal actor preserves billboard visibility alpha and white tint");
    check(ro::actor::ResolveBillboardColor(128, 255, 10, 20, 30,
              true, 0xFF00FF00u) == 0x80FF0000u,
        "active non-red single-color actor renders source solid-red tint");
    check(ro::actor::ResolveBillboardColor(200, 128, 10, 20, 30,
              false, 0xFFFF0000u) == 0x640A141Eu,
        "base tint alpha composes with hide/vanish alpha while preserving RGB");

    std::printf("Actor tint: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
