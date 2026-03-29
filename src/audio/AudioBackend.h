#pragma once

#include <memory>
#include <string>

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual bool Init() = 0;
    virtual void Shutdown() = 0;

    virtual bool PlayWave(const char* fName, float volume) = 0;
    virtual bool PlayBGM(const char* normalizedPath, int volume, bool paused) = 0;
    virtual void StopBGM() = 0;
    virtual void StopAllSounds() = 0;
    virtual void SetBgmVolume(int volume) = 0;
    virtual void SetBgmPaused(bool paused) = 0;

    virtual const std::string& GetCurrentBgmPath() const = 0;
};

std::unique_ptr<IAudioBackend> CreateMilesAudioBackend();
std::unique_ptr<IAudioBackend> CreateMiniaudioAudioBackend();
