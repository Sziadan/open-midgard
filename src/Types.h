#pragma once
//===========================================================================
// Types.h  –  Ragnarok Online client common type definitions
// Clean C++17 rewrite of the HighPriest 2008 client.
//===========================================================================
// Do NOT define WIN32_LEAN_AND_MEAN here.  The project already defines NOMINMAX
// via CMakeLists.txt to prevent min/max collisions.  The lean macro breaks
// the UCRT locale header chain on MSVC v14.44 (VS2022 latest), causing
// 'struct lconv' to be missing when <clocale> does 'using ::lconv'.
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
// Convenience integer type aliases (match the original decompiler output)
// ---------------------------------------------------------------------------
typedef std::uint8_t  u8;
typedef std::uint16_t u16;
typedef std::uint32_t u32;
typedef std::uint64_t u64;

typedef std::int8_t   s8;
typedef std::int16_t  s16;
typedef std::int32_t  s32;
typedef std::int64_t  s64;

typedef float  f32;
typedef double f64;

// ---------------------------------------------------------------------------
// Common structures
// ---------------------------------------------------------------------------
struct UVRECT
{
    float u1, v1;
    float u2, v2;
};

struct IRECT
{
    int left, top, right, bottom;
};

struct vector2d {
    float x;
    float y;
};

struct vector3d {
    float x;
    float y;
    float z;
};

struct matrix {
    float m[4][4];
};

struct plane3d {
    vector3d n;
    float d;
};

struct ray3d {
    vector3d p;
    vector3d d;
};

struct SERVER_ADDR
{
    u32 ip;
    s16 port;
    u8  name[20];
    u16 usercount;
    u16 state;
    u16 property;
};

struct CHARACTER_INFO
{
    u32 GID;
    int exp;
    int money;
    int jobexp;
    int joblevel;
    int bodystate;
    int healthstate;
    int effectstate;
    int virtue;
    int honor;
    s16 jobpoint;
    s16 hp;
    s16 maxhp;
    s16 sp;
    s16 maxsp;
    s16 speed;
    s16 job;
    s16 head;
    s16 weapon;
    s16 level;
    s16 sppoint;
    s16 accessory;
    s16 shield;
    s16 accessory2;
    s16 accessory3;
    s16 headpalette;
    s16 bodypalette;
    u8  name[24];
    u8  Str;
    u8  Agi;
    u8  Vit;
    u8  Int;
    u8  Dex;
    u8  Luk;
    u8  CharNum;
    u8  haircolor;
    s16 bIsChangedCharName;
};

struct DRAG_INFO {
    int type;
    void* data;
};

// ---------------------------------------------------------------------------
// Global game-wide flags (extern declarations; definitions in WinMain.cpp)
// ---------------------------------------------------------------------------
extern HWND  g_hMainWnd;
extern HINSTANCE g_hInstance;
extern bool  g_isAppActive;
extern int   g_soundMode;
extern int   g_frameskip;
extern char  g_baseDir[MAX_PATH];
