#include "audio/WaveMath.h"
#include <cstdio>
#include <memory>
#include <string>

int g_soundMode = 1, g_isSoundOn = 1;
struct CWave { unsigned char* m_fileImage; int m_fileSize; };
struct Resources {
    int gets = 0;
    unsigned char dummy = 1;
    CWave wave{ &dummy, 1 };
    template<class T> T* GetAs(const char*) { ++gets; return &wave; }
} g_resMgr;
struct Backend {
    bool provider = false;
    int digital = 0, spatial = 0, maxDist = 0, minDist = 0;
    float volume = 0, x = 0, y = 0, z = 0;
    std::string name;
    bool Has3DProvider() const { return provider; }
    bool PlayWave(const char* path, float gain) {
        ++digital; name = path; volume = gain; return true;
    }
    bool PlayWave3D(const char* path, float px, float py, float pz, int maximum, int minimum, float gain) {
        ++spatial; name = path; x = px; y = py; z = pz;
        maxDist = maximum; minDist = minimum; volume = gain; return true;
    }
};
class CAudio {
public:
    float m_soundVolume = 1;
    std::unique_ptr<Backend> m_backend = std::make_unique<Backend>();
    bool Init() { return true; }
    bool PlayWaveRelative(const char*, float, float, float, int, int, float);
};

#include "WaveBody.inc"

int main()
{
    CAudio audio;
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    const char* name = "effect\\exact.wav";
    audio.PlayWaveRelative(name, 80, 0, 0, 250, 40, 0.5f);
    check(audio.m_backend->digital == 1 && audio.m_backend->volume == 31.0f / 127.0f,
        "mode 1 preserves both integer truncations");
    check(audio.m_backend->name == name, "resource/backend name passed without prefix candidates");
    g_soundMode = 2;
    audio.PlayWaveRelative(name, 80, 0, 0, 1, 1, 0.0f);
    check(audio.m_backend->digital == 2 && audio.m_backend->volume == 63.0f / 127.0f,
        "mode 2 no-provider fallback uses 250/40/1 despite caller arguments");
    audio.m_backend->provider = true;
    audio.PlayWaveRelative(name, 5, -10, 15, 500, 20, 0.5f);
    check(audio.m_backend->spatial == 1 && audio.m_backend->x == 5 && audio.m_backend->y == -10
        && audio.m_backend->z == 15 && audio.m_backend->maxDist == 500 && audio.m_backend->minDist == 20
        && audio.m_backend->volume == 63.0f / 127.0f, "provider branch receives original relative coordinates/distances");
    const int gets = g_resMgr.gets;
    g_soundMode = 0;
    check(!audio.PlayWaveRelative(name, 0, 0, 0, 250, 40, 1) && g_resMgr.gets == gets,
        "mode zero does not load or play");
    g_soundMode = 1; g_isSoundOn = 0;
    check(!audio.PlayWaveRelative(name, 0, 0, 0, 250, 40, 1) && g_resMgr.gets == gets,
        "muted does not load or play");
    g_isSoundOn = 1;
    check(!audio.PlayWaveRelative(nullptr, 0, 0, 0, 250, 40, 1), "null name rejected");
    g_resMgr.wave.m_fileImage = nullptr;
    const int played = audio.m_backend->digital + audio.m_backend->spatial;
    check(!audio.PlayWaveRelative(name, 0, 0, 0, 250, 40, 1)
        && audio.m_backend->digital + audio.m_backend->spatial == played, "missing wave never reaches backend");
    if (!failures) std::puts("8 playback-dispatch checks passed; silent backend used.");
    return failures ? 1 : 0;
}
