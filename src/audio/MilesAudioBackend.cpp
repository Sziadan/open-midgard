#include "AudioBackend.h"

#include "Audio.h"
#include "core/DllMgr.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kDefaultDigitalRate = 22050;
constexpr int kDefaultDigitalBits = 16;
constexpr int kDefaultDigitalChannels = 1;
constexpr int kMaxManagedSamples = 24;
constexpr int kMaxVolume = 127;

int ClampMilesVolume(int value)
{
    return (std::max)(0, (std::min)(kMaxVolume, value));
}

class MilesAudioBackend final : public IAudioBackend {
public:
    MilesAudioBackend() = default;
    ~MilesAudioBackend() override
    {
        Shutdown();
    }

    bool Init() override
    {
        if (m_digDriver) {
            return true;
        }

        if (!g_dllExports.AIL_startup || !g_dllExports.AIL_open_digital_driver) {
            return false;
        }

        if (!m_startedUp) {
            if (g_dllExports.AIL_set_redist_directory) {
                g_dllExports.AIL_set_redist_directory(".\\");
            }
            if (g_dllExports.AIL_set_preference) {
                g_dllExports.AIL_set_preference(15u, 0);
            }
            g_dllExports.AIL_startup();
            m_startedUp = true;
        }

        m_digDriver = g_dllExports.AIL_open_digital_driver(
            kDefaultDigitalRate, kDefaultDigitalBits, kDefaultDigitalChannels, 1);
        if (!m_digDriver) {
            return false;
        }

        m_samples.clear();
        if (g_dllExports.AIL_allocate_sample_handle) {
            m_samples.reserve(kMaxManagedSamples);
            for (int index = 0; index < kMaxManagedSamples; ++index) {
                HSAMPLE sample = g_dllExports.AIL_allocate_sample_handle(m_digDriver);
                if (!sample) {
                    break;
                }
                m_samples.push_back(sample);
            }
        }

        return true;
    }

    void Shutdown() override
    {
        StopAllSounds();
        StopBGM();

        if (g_dllExports.AIL_release_sample_handle) {
            for (HSAMPLE sample : m_samples) {
                if (sample) {
                    g_dllExports.AIL_release_sample_handle(sample);
                }
            }
        }
        m_samples.clear();

        if (m_digDriver && g_dllExports.AIL_close_digital_driver) {
            g_dllExports.AIL_close_digital_driver(m_digDriver);
        }
        m_digDriver = nullptr;

        if (m_startedUp && g_dllExports.AIL_shutdown) {
            g_dllExports.AIL_shutdown();
        }
        m_startedUp = false;
    }

    bool PlayWave(const char* fName, float volume) override
    {
        if (!fName || !*fName || volume <= 0.0f || !Init()
            || !g_dllExports.AIL_init_sample
            || !g_dllExports.AIL_set_sample_file
            || !g_dllExports.AIL_start_sample) {
            return false;
        }

        CWave* wave = g_resMgr.GetAs<CWave>(fName);
        if (!wave || !wave->m_fileImage || wave->m_fileSize <= 0) {
            return false;
        }

        HSAMPLE sample = FindReusableSample();
        if (!sample) {
            return false;
        }

        g_dllExports.AIL_init_sample(sample);
        g_dllExports.AIL_set_sample_file(sample, wave->m_fileImage, 0);
        if (g_dllExports.AIL_set_sample_volume) {
            g_dllExports.AIL_set_sample_volume(
                sample, ClampMilesVolume(static_cast<int>(volume * static_cast<float>(kMaxVolume))));
        }
        g_dllExports.AIL_start_sample(sample);
        return true;
    }

    bool PlayBGM(const char* normalizedPath, int volume, bool paused) override
    {
        if (!normalizedPath || !*normalizedPath || !Init() || !g_dllExports.AIL_open_stream) {
            return false;
        }

        const std::string requestedPath = normalizedPath;
        if (m_bgmStream && requestedPath == m_bgmPath) {
            SetBgmVolume(volume);
            SetBgmPaused(paused);
            return true;
        }

        StopBGM();

        void* stream = g_dllExports.AIL_open_stream(m_digDriver, requestedPath.c_str(), 0);
        if (!stream) {
            return false;
        }

        m_bgmStream = stream;
        m_bgmPath = requestedPath;
        m_bgmPendingStart = paused;
        m_bgmPaused = paused;

        if (g_dllExports.AIL_set_stream_loop_count) {
            g_dllExports.AIL_set_stream_loop_count(stream, 0);
        }

        SetBgmVolume(volume);
        if (!m_bgmPendingStart && g_dllExports.AIL_start_stream) {
            g_dllExports.AIL_start_stream(stream);
        }
        if (g_dllExports.AIL_pause_stream) {
            g_dllExports.AIL_pause_stream(stream, paused ? 1 : 0);
        }

        return true;
    }

    void StopBGM() override
    {
        if (m_bgmStream && g_dllExports.AIL_close_stream) {
            g_dllExports.AIL_close_stream(m_bgmStream);
        }

        m_bgmStream = nullptr;
        m_bgmPath.clear();
        m_bgmPendingStart = false;
    }

    void StopAllSounds() override
    {
        if (!g_dllExports.AIL_end_sample) {
            return;
        }

        for (HSAMPLE sample : m_samples) {
            if (sample) {
                g_dllExports.AIL_end_sample(sample);
            }
        }
    }

    void SetBgmVolume(int volume) override
    {
        m_bgmVolume = ClampMilesVolume(volume);
        if (m_bgmStream && g_dllExports.AIL_set_stream_volume) {
            g_dllExports.AIL_set_stream_volume(m_bgmStream, m_bgmVolume);
        }
    }

    void SetBgmPaused(bool paused) override
    {
        const bool wasPaused = m_bgmPaused;
        m_bgmPaused = paused;

        if (!paused && wasPaused && m_bgmPendingStart && m_bgmStream && g_dllExports.AIL_start_stream) {
            g_dllExports.AIL_start_stream(m_bgmStream);
            m_bgmPendingStart = false;
        }

        if (m_bgmStream && g_dllExports.AIL_pause_stream) {
            g_dllExports.AIL_pause_stream(m_bgmStream, paused ? 1 : 0);
        }
    }

    const std::string& GetCurrentBgmPath() const override
    {
        return m_bgmPath;
    }

private:
    HSAMPLE FindReusableSample()
    {
        if (!g_dllExports.AIL_sample_status) {
            return nullptr;
        }

        for (HSAMPLE sample : m_samples) {
            if (!sample) {
                continue;
            }
            if (g_dllExports.AIL_sample_status(sample) == 2u) {
                return sample;
            }
        }

        if (m_digDriver && g_dllExports.AIL_allocate_sample_handle) {
            HSAMPLE sample = g_dllExports.AIL_allocate_sample_handle(m_digDriver);
            if (sample) {
                m_samples.push_back(sample);
            }
            return sample;
        }

        return nullptr;
    }

    HDIGDRIVER m_digDriver = nullptr;
    std::vector<HSAMPLE> m_samples;
    void* m_bgmStream = nullptr;
    int m_bgmVolume = kMaxVolume;
    bool m_bgmPaused = false;
    bool m_bgmPendingStart = false;
    bool m_startedUp = false;
    std::string m_bgmPath;
};

} // namespace

std::unique_ptr<IAudioBackend> CreateMilesAudioBackend()
{
    return std::make_unique<MilesAudioBackend>();
}
