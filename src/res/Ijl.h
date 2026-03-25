#pragma once
#include "Types.h"

//===========================================================================
// Intel JPEG Library (IJL) SDK Structures (mapped from HighPriest.exe.h)
//===========================================================================

enum _IJLIOTYPE : int {
    IJL_SETUP = -1,
    IJL_JFILE_READPARAMS = 0,
    IJL_JBUFF_READPARAMS = 1,
    IJL_JFILE_READWHOLEIMAGE = 2,
    IJL_JBUFF_READWHOLEIMAGE = 3,
    IJL_JFILE_READHEADER = 4,
    IJL_JBUFF_READHEADER = 5
};

enum _IJL_COLOR : int {
    IJL_RGB = 1,
    IJL_BGR = 2,
    IJL_YCBCR = 3,
    IJL_G = 4
};

struct JPEG_PROPERTIES {
    u32 ijl_command; // _IJLIOTYPE
    u8* jbuf;
    u32 jbuf_size;
    int jwidth;
    int jheight;
    int jchannels;
    u32* dither_ptr;
    u8* dIBBuffer;
    int dIBWidth;
    int dIBHeight;
    int dIBChannels;
    u32 ijl_color; // _IJL_COLOR
    u32 dIBColor;  // _IJL_COLOR
};

// IJL helper functions (stubs for ijl15.dll)
class CIjl {
public:
    static bool LoadJPG(const unsigned char* buffer, int size, int& width, int& height, u32*& data);
};
