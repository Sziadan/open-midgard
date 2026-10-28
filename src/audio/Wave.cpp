#include "Audio.h"
#include "AudioBackend.h"
#include "WaveMath.h"
#include "DebugLog.h"

bool PlayWave(const char* name, float x, float y, float z,
    int volumeMaxDist, int volumeMinDist, float vFactor)
{
    if (!g_soundMode || !g_isSoundOn || !name || !*name) {
        return false;
    }
#if defined(RO_SKILL_REPLAY_TRACE)
    DbgLog("[REPLAY] PlayWave name='%s' relative=(%.3f,%.3f,%.3f)\n", name, x, y, z);
#endif
    DbgLog("[SkillWave] name='%s' relative=(%.3f,%.3f,%.3f) max=%d min=%d factor=%.3f mode=%d\n",
        name, x, y, z, volumeMaxDist, volumeMinDist, vFactor, g_soundMode);
    return CAudio::GetInstance()->PlayWaveRelative(name, x, y, z,
        volumeMaxDist, volumeMinDist, vFactor);
}

bool CAudio::PlayWaveRelative(const char* name, float x, float y, float z,
    int volumeMaxDist, int volumeMinDist, float vFactor)
{
    if (!g_soundMode || !g_isSoundOn || !name || !*name || !Init()) {
        return false;
    }
    // Ref/Wave.cpp:309 loads the wave before selecting the playback mode.
    CWave* wave = g_resMgr.GetAs<CWave>(name);
    if (!wave || !wave->m_fileImage || wave->m_fileSize <= 0) {
        return false;
    }
    const int digitalVolume = static_cast<int>(m_soundVolume * 127.0f);
    if (g_soundMode == 2) {
        if (m_backend->Has3DProvider()) {
            const int volume = static_cast<int>(static_cast<double>(digitalVolume) * vFactor);
            return m_backend->PlayWave3D(name, x, y, z, volumeMaxDist, volumeMinDist,
                static_cast<float>(volume) / 127.0f);
        }
        // Ref/Sound.cpp:166: no provider uses these defaults, including factor 1.
        volumeMaxDist = 250;
        volumeMinDist = 40;
        vFactor = 1.0f;
    } else if (g_soundMode != 1) {
        return false;
    }
    const int volume = ro::audio::DigitalWaveVolume(x, y, z,
        volumeMaxDist, volumeMinDist, digitalVolume, vFactor);
    return m_backend->PlayWave(name, static_cast<float>(volume) / 127.0f);
}
