#pragma once
#include "Types.h"
#include "res/Res.h"
#include <string>
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
};

class CWave : public CRes {
public:
    unsigned char* m_fileImage;
    int m_isDecompress;

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

    HDIGDRIVER m_digDriver;
    std::vector<HSAMPLE> m_samples;
    float m_volume;
    void* m_bgmStream;
    int m_bgmVolume;
    bool m_bgmPaused;

    static CAudio* GetInstance();
    bool Init();
    void PlaySound(const char* fName, float vol = 1.0f);
    void PlayBGM(const char* fName);
    void StopBGM();
    void SetVolume(float vol);
    float GetVolume() const;
    void SetBgmVolume(int vol);
    int GetBgmVolume() const;
    void SetBgmPaused(bool paused);
    bool IsBgmPaused() const;
};
