#include "audio/WaveMath.h"
#include <cstdio>

// Pure math: these checks never initialize an audio device or load/play media.
int main()
{
    using ro::audio::DigitalWaveVolume;
    int failures = 0;
    auto check = [&](const char* name, int actual, int expected) {
        if (actual != expected) {
            std::fprintf(stderr, "%s: got %d, expected %d\n", name, actual, expected);
            ++failures;
        }
    };
    check("listener", DigitalWaveVolume(0, 0, 0, 250, 40, 127, 1), 127);
    check("minimum distance", DigitalWaveVolume(40, 0, 0, 250, 40, 127, 1), 127);
    check("inverse distance", DigitalWaveVolume(80, 0, 0, 250, 40, 127, 1), 63);
    check("two integer truncations", DigitalWaveVolume(80, 0, 0, 250, 40, 127, 0.5f), 31);
    check("height counts", DigitalWaveVolume(0, 80, 0, 250, 40, 127, 1), 63);
    check("xyz distance", DigitalWaveVolume(48, 0, 64, 250, 40, 127, 1), 63);
    check("negative coordinates", DigitalWaveVolume(-48, 0, -64, 250, 40, 127, 1), 63);
    check("maximum distance excluded", DigitalWaveVolume(250, 0, 0, 250, 40, 127, 1), 0);
    check("beyond maximum", DigitalWaveVolume(251, 0, 0, 250, 40, 127, 1), 0);
    check("before cutoff", DigitalWaveVolume(249, 0, 0, 250, 40, 127, 1), 20);
    check("custom distances", DigitalWaveVolume(100, 0, 0, 200, 20, 127, 1), 25);
    check("zero volume", DigitalWaveVolume(0, 0, 0, 250, 40, 0, 1), 0);
    check("factor applied after attenuation", DigitalWaveVolume(80, 0, 0, 250, 40, 127, 2), 126);
    if (!failures) {
        std::puts("13 reference wave-volume checks passed; no media played.");
    }
    return failures ? 1 : 0;
}
