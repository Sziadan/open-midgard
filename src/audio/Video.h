#pragma once
#include "Types.h"
#include <string>

//===========================================================================
// Bink Video SDK Structures (mapped from HighPriest.exe.h)
//===========================================================================

struct BINK;
struct BINKBUFFER;

struct BINKRECT {
    int left, top, right, bottom;
};

// CBink is the high-level wrapper for binkw32.dll
class CBink {
public:
    BINK* m_Bink;
    BINKBUFFER* m_BinkBuffer;
    unsigned char m_IsBinkPlaying;
    int m_BinkMode;

    CBink();
    virtual ~CBink();
    bool Open(const char* fName, u32 flags);
    void Close();
    void Play();
    void Stop();
    void SetPause(bool pause);
    void Render();
};
