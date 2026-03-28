#pragma once
#include "Types.h"
#include "res/Res.h"
#include <string>
#include <map>
#include <vector>

//===========================================================================
// Miles Sound System SDK Structures (stubs based on decompiler)
//===========================================================================

struct _AILSOUNDINFO {
    int format;
    const void* data_ptr;
    unsigned int data_len;
    unsigned int rate;
    int bits;
    int channels;
    unsigned int samples;
    unsigned int block_size;
    const void* initial_ptr;
};

// Standard Miles handles (void* or u32)
typedef void* HDIGDRIVER;
typedef void* HSAMPLE;
typedef void* HSEQUENCE;

//===========================================================================
// Ragnarok Online Audio Classes
//===========================================================================

struct PLAY_WAVE_INFO {
    std::string wavName;
    unsigned int nAID;
    unsigned int term;
    unsigned int endTick;
    vector3d pos;
    float volumeFactor;
    int volumeMaxDist;
    int volumeMinDist;
};

class CWave : public CRes {
public:
    unsigned char* m_fileImage;
    int m_isDecompress;
    int m_fileSize;

    CWave();
    virtual ~CWave();
    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;
};

// CAudio is the high-level manager for Miles
class CAudio {
public:
    CAudio();
    ~CAudio();

    struct MapBgmEntry {
        std::string rswName;
        std::string bgmPath;
    };

    HDIGDRIVER m_digDriver;
    std::vector<HSAMPLE> m_samples;
    float m_soundVolume;
    void* m_bgmStream;
    int m_bgmVolume;
    bool m_bgmPaused;
    bool m_bgmPendingStart;
    std::string m_bgmPath;
    bool m_startedUp;
    std::vector<MapBgmEntry> m_mapBgmTable;

    static CAudio* GetInstance();
    bool Init();
    void Shutdown();
    bool PlaySound(const char* fName, float vol = 1.0f);
    bool PlaySound3D(const char* fName, const vector3d& soundPos, const vector3d& listenerPos,
        int volumeMaxDist = 250, int volumeMinDist = 40, float vol = 1.0f);
    void PlayBGM(const char* fName);
    void StopBGM();
    void StopAllSounds();
    void SetVolume(float vol);
    float GetVolume() const;
    void SetBgmVolume(int vol);
    int GetBgmVolume() const;
    void SetBgmPaused(bool paused);
    bool IsBgmPaused() const;
    const std::string& GetCurrentBgmPath() const;
    std::string ResolveMapBgmPath(const char* rswName);

private:
    HSAMPLE FindReusableSample();
    bool EnsureMapBgmTableLoaded();
    static std::string CanonicalizeMapName(const char* rswName);
    static std::string NormalizeAudioPath(const char* path);
};
