#include "AudioBackend.h"

#include "Audio.h"
#include "core/File.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kMaxVolume = 127;

int ClampMilesVolume(int value)
{
    return (std::max)(0, (std::min)(kMaxVolume, value));
}

float MilesVolumeToUnit(int value)
{
    return static_cast<float>(ClampMilesVolume(value)) / static_cast<float>(kMaxVolume);
}

struct MiniaudioOneShot {
    ma_decoder decoder{};
    ma_sound sound{};
    bool decoderInitialized = false;
    bool soundInitialized = false;
};

void CleanupOneShot(MiniaudioOneShot* instance)
{
    if (!instance) {
        return;
    }

    if (instance->soundInitialized) {
        ma_sound_uninit(&instance->sound);
        instance->soundInitialized = false;
    }

    if (instance->decoderInitialized) {
        ma_decoder_uninit(&instance->decoder);
        instance->decoderInitialized = false;
    }
}

class MiniaudioAudioBackend final : public IAudioBackend {
public:
    MiniaudioAudioBackend() = default;
    ~MiniaudioAudioBackend() override
    {
        Shutdown();
    }

    bool Init() override
    {
        if (m_initialized) {
            return true;
        }

        ma_engine_config engineConfig = ma_engine_config_init();
        const ma_result result = ma_engine_init(&engineConfig, &m_engine);
        if (result != MA_SUCCESS) {
            return false;
        }

        m_initialized = true;
        return true;
    }

    void Shutdown() override
    {
        StopAllSounds();
        StopBGM();

        if (m_initialized) {
            ma_engine_uninit(&m_engine);
            m_initialized = false;
        }
    }

    bool PlayWave(const char* fName, float volume) override
    {
        if (!fName || !*fName || volume <= 0.0f || !Init()) {
            return false;
        }

        CWave* wave = g_resMgr.GetAs<CWave>(fName);
        if (!wave || !wave->m_fileImage || wave->m_fileSize <= 0) {
            return false;
        }

        TrimFinishedSounds();

        auto instance = std::make_unique<MiniaudioOneShot>();
        ma_decoder_config decoderConfig = ma_decoder_config_init_default();
        if (ma_decoder_init_memory(
                wave->m_fileImage,
                static_cast<size_t>(wave->m_fileSize),
                &decoderConfig,
                &instance->decoder) != MA_SUCCESS) {
            return false;
        }
        instance->decoderInitialized = true;

        if (ma_sound_init_from_data_source(
                &m_engine,
                &instance->decoder,
                0,
                nullptr,
                &instance->sound) != MA_SUCCESS) {
            CleanupOneShot(instance.get());
            return false;
        }
        instance->soundInitialized = true;

        ma_sound_set_volume(&instance->sound, volume);
        if (ma_sound_start(&instance->sound) != MA_SUCCESS) {
            CleanupOneShot(instance.get());
            return false;
        }

        m_activeSounds.push_back(std::move(instance));
        return true;
    }

    bool PlayBGM(const char* normalizedPath, int volume, bool paused) override
    {
        if (!normalizedPath || !*normalizedPath || !Init()) {
            return false;
        }

        const std::string requestedPath = normalizedPath;
        if (m_bgmSoundInitialized && requestedPath == m_bgmPath) {
            SetBgmVolume(volume);
            SetBgmPaused(paused);
            return true;
        }

        std::unique_ptr<unsigned char[]> bgmBytes;
        int bgmSize = 0;
        if (!LoadFileBytes(requestedPath.c_str(), &bgmBytes, &bgmSize)) {
            return false;
        }

        StopBGM();

        m_bgmData = std::move(bgmBytes);
        m_bgmDataSize = bgmSize;

        ma_decoder_config decoderConfig = ma_decoder_config_init_default();
        if (ma_decoder_init_memory(
                m_bgmData.get(),
                static_cast<size_t>(m_bgmDataSize),
                &decoderConfig,
                &m_bgmDecoder) != MA_SUCCESS) {
            ClearBgmState();
            return false;
        }
        m_bgmDecoderInitialized = true;

        if (ma_sound_init_from_data_source(
                &m_engine,
                &m_bgmDecoder,
                0,
                nullptr,
                &m_bgmSound) != MA_SUCCESS) {
            StopBGM();
            return false;
        }
        m_bgmSoundInitialized = true;
        m_bgmPath = requestedPath;
        ma_sound_set_looping(&m_bgmSound, MA_TRUE);

        SetBgmVolume(volume);
        m_bgmPaused = paused;
        if (!paused && ma_sound_start(&m_bgmSound) != MA_SUCCESS) {
            StopBGM();
            return false;
        }

        return true;
    }

    void StopBGM() override
    {
        if (m_bgmSoundInitialized) {
            ma_sound_stop(&m_bgmSound);
            ma_sound_uninit(&m_bgmSound);
            m_bgmSoundInitialized = false;
        }

        if (m_bgmDecoderInitialized) {
            ma_decoder_uninit(&m_bgmDecoder);
            m_bgmDecoderInitialized = false;
        }

        ClearBgmState();
    }

    void StopAllSounds() override
    {
        for (const std::unique_ptr<MiniaudioOneShot>& instance : m_activeSounds) {
            CleanupOneShot(instance.get());
        }
        m_activeSounds.clear();
    }

    void SetBgmVolume(int volume) override
    {
        m_bgmVolume = ClampMilesVolume(volume);
        if (m_bgmSoundInitialized) {
            ma_sound_set_volume(&m_bgmSound, MilesVolumeToUnit(m_bgmVolume));
        }
    }

    void SetBgmPaused(bool paused) override
    {
        m_bgmPaused = paused;
        if (!m_bgmSoundInitialized) {
            return;
        }

        if (paused) {
            if (ma_sound_is_playing(&m_bgmSound)) {
                ma_sound_stop(&m_bgmSound);
            }
            return;
        }

        if (!ma_sound_is_playing(&m_bgmSound)) {
            ma_sound_start(&m_bgmSound);
        }
    }

    const std::string& GetCurrentBgmPath() const override
    {
        return m_bgmPath;
    }

private:
    bool LoadFileBytes(const char* path, std::unique_ptr<unsigned char[]>* outBytes, int* outSize)
    {
        if (!path || !*path || !outBytes || !outSize) {
            return false;
        }

        int size = 0;
        unsigned char* bytes = g_fileMgr.GetData(path, &size);
        if (!bytes || size <= 0) {
            delete[] bytes;
            return false;
        }

        outBytes->reset(bytes);
        *outSize = size;
        return true;
    }

    void TrimFinishedSounds()
    {
        auto it = std::remove_if(
            m_activeSounds.begin(),
            m_activeSounds.end(),
            [](const std::unique_ptr<MiniaudioOneShot>& instance) {
                if (!instance || !instance->soundInitialized) {
                    return true;
                }
                if (ma_sound_is_playing(&instance->sound)) {
                    return false;
                }
                CleanupOneShot(instance.get());
                return true;
            });
        m_activeSounds.erase(it, m_activeSounds.end());
    }

    void ClearBgmState()
    {
        m_bgmData.reset();
        m_bgmDataSize = 0;
        m_bgmPaused = false;
        m_bgmPath.clear();
    }

    ma_engine m_engine{};
    bool m_initialized = false;
    std::vector<std::unique_ptr<MiniaudioOneShot>> m_activeSounds;

    ma_decoder m_bgmDecoder{};
    ma_sound m_bgmSound{};
    bool m_bgmDecoderInitialized = false;
    bool m_bgmSoundInitialized = false;
    std::unique_ptr<unsigned char[]> m_bgmData;
    int m_bgmDataSize = 0;
    int m_bgmVolume = kMaxVolume;
    bool m_bgmPaused = false;
    std::string m_bgmPath;
};

} // namespace

std::unique_ptr<IAudioBackend> CreateMiniaudioAudioBackend()
{
    return std::make_unique<MiniaudioAudioBackend>();
}
