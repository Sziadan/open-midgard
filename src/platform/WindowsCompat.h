#pragma once

#ifndef RO_PLATFORM_WINDOWS_COMPAT_H
#define RO_PLATFORM_WINDOWS_COMPAT_H

#if RO_PLATFORM_WINDOWS

#include <windows.h>

#else

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <string>
#include <mutex>
#include <thread>

#include "qtui/QtPlatformWindow.h"

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#if defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#endif
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif

#ifndef __stdcall
#define __stdcall
#endif

#ifndef __cdecl
#define __cdecl
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

using BOOL = int;
using BYTE = unsigned char;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using COLORREF = DWORD;
using UINT = unsigned int;
using ULONG = std::uint32_t;
using LONG = std::int32_t;
using ULONG_PTR = std::uintptr_t;
using UINT_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using HANDLE = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using HWND = void*;
using HDC = void*;
using HGDIOBJ = void*;
using HBITMAP = void*;
using HFONT = void*;
using HPEN = void*;
using HICON = void*;
using HCURSOR = void*;
using HMENU = void*;
using HBRUSH = void*;
using HMONITOR = void*;
using HRGN = void*;
using HKEY = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPBYTE = BYTE*;
using LPCSTR = const char*;
using LPSTR = char*;
using ATOM = WORD;

constexpr int LF_FACESIZE = 32;
constexpr int _TRUNCATE = -1;

struct POINT {
    LONG x;
    LONG y;
};

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

using tagRECT = RECT;

struct tagSIZE {
    LONG cx;
    LONG cy;
};

using SIZE = tagSIZE;

struct PALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};

#define RO_WINDOWS_COMPAT_HAS_PALETTEENTRY 1

struct LOGFONTA {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    char lfFaceName[LF_FACESIZE];
};

struct MONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
};

struct DEVMODEA {
    char dmDeviceName[32];
    WORD dmSpecVersion;
    WORD dmDriverVersion;
    WORD dmSize;
    WORD dmDriverExtra;
    DWORD dmFields;
    LONG dmPositionX;
    LONG dmPositionY;
    DWORD dmDisplayOrientation;
    DWORD dmDisplayFixedOutput;
    short dmColor;
    short dmDuplex;
    short dmYResolution;
    short dmTTOption;
    short dmCollate;
    char dmFormName[32];
    WORD dmLogPixels;
    DWORD dmBitsPerPel;
    DWORD dmPelsWidth;
    DWORD dmPelsHeight;
    DWORD dmDisplayFlags;
    DWORD dmDisplayFrequency;
    DWORD dmICMMethod;
    DWORD dmICMIntent;
    DWORD dmMediaType;
    DWORD dmDitherType;
    DWORD dmReserved1;
    DWORD dmReserved2;
    DWORD dmPanningWidth;
    DWORD dmPanningHeight;
};

struct MSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
    LONG lPrivate;
};

struct LARGE_INTEGER {
    long long QuadPart;
};

struct BITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
};

struct BITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    DWORD bmiColors[3];
};

struct BITMAP {
    LONG bmType;
    LONG bmWidth;
    LONG bmHeight;
    LONG bmWidthBytes;
    WORD bmPlanes;
    WORD bmBitsPixel;
    LPVOID bmBits;
};

struct DIBSECTION {
    BITMAP dsBm;
    BITMAPINFOHEADER dsBmih;
    DWORD dsBitfields[3];
    HANDLE dshSection;
    DWORD dsOffset;
};

struct BLENDFUNCTION {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
};

struct TEXTMETRICA {
    LONG tmHeight;
    LONG tmAscent;
    LONG tmDescent;
    LONG tmInternalLeading;
    LONG tmExternalLeading;
    LONG tmAveCharWidth;
    LONG tmMaxCharWidth;
    LONG tmWeight;
    LONG tmOverhang;
    LONG tmDigitizedAspectX;
    LONG tmDigitizedAspectY;
    BYTE tmFirstChar;
    BYTE tmLastChar;
    BYTE tmDefaultChar;
    BYTE tmBreakChar;
    BYTE tmItalic;
    BYTE tmUnderlined;
    BYTE tmStruckOut;
    BYTE tmPitchAndFamily;
    BYTE tmCharSet;
};

struct CRITICAL_SECTION {
    std::mutex mutex;
};

inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void DeleteCriticalSection(CRITICAL_SECTION*) {}
inline void EnterCriticalSection(CRITICAL_SECTION* section) { section->mutex.lock(); }
inline void LeaveCriticalSection(CRITICAL_SECTION* section) { section->mutex.unlock(); }

inline DWORD GetTickCount()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

inline DWORD GetCurrentProcessId()
{
#if defined(__unix__) || defined(__APPLE__)
    return static_cast<DWORD>(::getpid());
#else
    return 0;
#endif
}

inline DWORD GetLastError()
{
    return static_cast<DWORD>(errno);
}

inline LONG InterlockedIncrement(LONG* value)
{
    return ++(*value);
}

inline LONG InterlockedDecrement(LONG* value)
{
    return --(*value);
}

inline DWORD GetEnvironmentVariableA(const char* name, char* buffer, DWORD size)
{
    if (!name) {
        return 0;
    }

    const char* value = std::getenv(name);
    if (!value) {
        if (buffer && size > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }

    const size_t length = std::strlen(value);
    if (!buffer || size == 0) {
        return static_cast<DWORD>(length);
    }

    if (length >= size) {
        buffer[0] = '\0';
        return static_cast<DWORD>(length + 1);
    }

    std::memcpy(buffer, value, length + 1);
    return static_cast<DWORD>(length);
}

inline void OutputDebugStringA(const char*) {}

inline HDC GetDC(HWND)
{
    return nullptr;
}

inline HDC CreateCompatibleDC(HDC)
{
    return nullptr;
}

inline int ReleaseDC(HWND, HDC)
{
    return 0;
}

inline BOOL GetClientRect(HWND hwnd, RECT* rect)
{
    return RoQtGetClientRect(hwnd, rect) ? TRUE : FALSE;
}

inline BOOL UpdateWindow(HWND)
{
    return TRUE;
}

inline void PostQuitMessage(int)
{
}

inline BOOL PeekMessageA(MSG*, HWND, UINT, UINT, UINT)
{
    return FALSE;
}

inline BOOL TranslateMessage(const MSG*)
{
    return TRUE;
}

inline LRESULT DispatchMessageA(const MSG*)
{
    return 0;
}

inline BOOL GetCursorPos(POINT* point)
{
    return RoQtGetCursorPos(point) ? TRUE : FALSE;
}

inline BOOL ScreenToClient(HWND hwnd, POINT* point)
{
    return RoQtScreenToClient(hwnd, point) ? TRUE : FALSE;
}

inline HRGN CreateRoundRectRgn(int, int, int, int, int, int)
{
    return reinterpret_cast<HRGN>(1);
}

inline BOOL PtInRegion(HRGN, int, int)
{
    return TRUE;
}

inline BOOL DeleteObject(HGDIOBJ)
{
    return TRUE;
}

inline BOOL DeleteDC(HDC)
{
    return TRUE;
}

inline HBITMAP CreateDIBSection(HDC, const BITMAPINFO*, UINT, void** bits, HANDLE, DWORD)
{
    if (bits) {
        *bits = nullptr;
    }
    return nullptr;
}

inline HGDIOBJ GetCurrentObject(HDC, UINT)
{
    return nullptr;
}

inline int GetObjectA(HGDIOBJ, int, LPVOID)
{
    return 0;
}

inline BOOL SetRectEmpty(RECT* rect)
{
    if (!rect) {
        return FALSE;
    }

    rect->left = 0;
    rect->top = 0;
    rect->right = 0;
    rect->bottom = 0;
    return TRUE;
}

inline BOOL UnionRect(RECT* dst, const RECT* lhs, const RECT* rhs)
{
    if (!dst || !lhs || !rhs) {
        return FALSE;
    }

    dst->left = (std::min)(lhs->left, rhs->left);
    dst->top = (std::min)(lhs->top, rhs->top);
    dst->right = (std::max)(lhs->right, rhs->right);
    dst->bottom = (std::max)(lhs->bottom, rhs->bottom);
    return TRUE;
}

inline BOOL InflateRect(RECT* rect, int dx, int dy)
{
    if (!rect) {
        return FALSE;
    }

    rect->left -= dx;
    rect->right += dx;
    rect->top -= dy;
    rect->bottom += dy;
    return TRUE;
}

inline BOOL OffsetRect(RECT* rect, int dx, int dy)
{
    if (!rect) {
        return FALSE;
    }

    rect->left += dx;
    rect->right += dx;
    rect->top += dy;
    rect->bottom += dy;
    return TRUE;
}

inline BOOL IntersectRect(RECT* dst, const RECT* lhs, const RECT* rhs)
{
    if (!dst || !lhs || !rhs) {
        return FALSE;
    }

    dst->left = (std::max)(lhs->left, rhs->left);
    dst->top = (std::max)(lhs->top, rhs->top);
    dst->right = (std::min)(lhs->right, rhs->right);
    dst->bottom = (std::min)(lhs->bottom, rhs->bottom);
    if (dst->left >= dst->right || dst->top >= dst->bottom) {
        SetRectEmpty(dst);
        return FALSE;
    }
    return TRUE;
}

inline BOOL PtInRect(const RECT* rect, POINT point)
{
    if (!rect) {
        return FALSE;
    }

    return point.x >= rect->left && point.x < rect->right
        && point.y >= rect->top && point.y < rect->bottom;
}

inline int SaveDC(HDC)
{
    return 0;
}

inline BOOL RestoreDC(HDC, int)
{
    return TRUE;
}

inline BOOL SetViewportOrgEx(HDC, int, int, POINT*)
{
    return TRUE;
}

inline BOOL SetRect(RECT* rect, int left, int top, int right, int bottom)
{
    if (!rect) {
        return FALSE;
    }

    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
    return TRUE;
}

inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency)
{
    if (!frequency) {
        return FALSE;
    }

    frequency->QuadPart = 1000000000ll;
    return TRUE;
}

inline BOOL QueryPerformanceCounter(LARGE_INTEGER* counter)
{
    if (!counter) {
        return FALSE;
    }

    counter->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return TRUE;
}

inline int SetBkMode(HDC, int)
{
    return 0;
}

inline HPEN CreatePen(int, int, COLORREF)
{
    return reinterpret_cast<HPEN>(1);
}

inline HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char*)
{
    return reinterpret_cast<HFONT>(1);
}

inline HBRUSH CreateSolidBrush(COLORREF)
{
    return reinterpret_cast<HBRUSH>(1);
}

inline HGDIOBJ SelectObject(HDC, HGDIOBJ)
{
    return nullptr;
}

inline HGDIOBJ GetStockObject(int)
{
    return reinterpret_cast<HGDIOBJ>(1);
}

inline COLORREF SetDCBrushColor(HDC, COLORREF color)
{
    return color;
}

inline COLORREF SetDCPenColor(HDC, COLORREF color)
{
    return color;
}

inline BOOL Polygon(HDC, const POINT*, int)
{
    return TRUE;
}

inline BOOL Ellipse(HDC, int, int, int, int)
{
    return TRUE;
}

inline BOOL Rectangle(HDC, int, int, int, int)
{
    return TRUE;
}

inline BOOL RoundRect(HDC, int, int, int, int, int, int)
{
    return TRUE;
}

inline BOOL MoveToEx(HDC, int, int, POINT*)
{
    return TRUE;
}

inline BOOL LineTo(HDC, int, int)
{
    return TRUE;
}

inline int FillRect(HDC, const RECT*, HBRUSH)
{
    return 1;
}

inline int FrameRect(HDC, const RECT*, HBRUSH)
{
    return 1;
}

inline int IntersectClipRect(HDC, int, int, int, int)
{
    return 1;
}

inline int SetStretchBltMode(HDC, int)
{
    return 0;
}

inline int SelectClipRgn(HDC, HRGN)
{
    return 1;
}

inline int StretchDIBits(HDC, int, int, int, int, int, int, int, int, const void*, const BITMAPINFO*, UINT, DWORD)
{
    return 1;
}

inline BOOL AlphaBlend(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION)
{
    return FALSE;
}

inline BOOL PatBlt(HDC, int, int, int, int, DWORD)
{
    return TRUE;
}

inline COLORREF SetTextColor(HDC, COLORREF color)
{
    return color;
}

inline int DrawTextA(HDC, const char*, int, RECT*, UINT)
{
    return 0;
}

inline BOOL TextOutA(HDC, int, int, const char*, int)
{
    return TRUE;
}

inline BOOL GetTextExtentPoint32A(HDC, const char* text, int len, SIZE* size)
{
    if (!size) {
        return FALSE;
    }

    size->cx = (std::max)(0, len) * 8;
    size->cy = 16;
    (void)text;
    return TRUE;
}

inline BOOL GetTextMetricsA(HDC, TEXTMETRICA* metrics)
{
    if (!metrics) {
        return FALSE;
    }

    std::memset(metrics, 0, sizeof(*metrics));
    metrics->tmHeight = 16;
    metrics->tmAscent = 12;
    metrics->tmDescent = 4;
    metrics->tmAveCharWidth = 8;
    metrics->tmMaxCharWidth = 8;
    return TRUE;
}

inline DWORD timeGetTime()
{
    return GetTickCount();
}

inline short GetKeyState(int)
{
    return 0;
}

inline HWND GetCapture()
{
    return nullptr;
}

inline HWND SetCapture(HWND window)
{
    return window;
}

inline BOOL ReleaseCapture()
{
    return TRUE;
}

inline int GetSystemMetrics(int)
{
    return 4;
}

inline UINT GetDoubleClickTime()
{
    return 500u;
}

inline BOOL EnumDisplaySettingsA(const char*, DWORD, DEVMODEA*)
{
    return FALSE;
}

inline int MessageBoxA(HWND, const char*, const char*, UINT)
{
    return 0;
}

inline void Sleep(DWORD milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

constexpr int FW_NORMAL = 400;
constexpr int FW_BOLD = 700;
constexpr LONG ERROR_SUCCESS = 0;
constexpr int ANSI_CHARSET = 0;
constexpr int SHIFTJIS_CHARSET = 128;
constexpr int GB2312_CHARSET = 134;
constexpr int CHINESEBIG5_CHARSET = 136;
constexpr int RUSSIAN_CHARSET = 204;
constexpr int THAI_CHARSET = 222;
constexpr int VIETNAMESE_CHARSET = 163;
constexpr int DEFAULT_CHARSET = 1;
constexpr int OUT_DEFAULT_PRECIS = 0;
constexpr int CLIP_DEFAULT_PRECIS = 0;
constexpr int ANTIALIASED_QUALITY = 4;
constexpr int NONANTIALIASED_QUALITY = 3;
constexpr int DEFAULT_QUALITY = 0;
constexpr int CLEARTYPE_QUALITY = 5;
constexpr int DEFAULT_PITCH = 0;
constexpr int FF_DONTCARE = 0;
constexpr int FF_SWISS = 32;
constexpr int NULL_BRUSH = 5;
constexpr int HOLLOW_BRUSH = NULL_BRUSH;
constexpr int NULL_PEN = 8;
constexpr int OBJ_BITMAP = 7;
constexpr int OBJ_FONT = 6;
constexpr int DEFAULT_GUI_FONT = 17;
constexpr int PS_SOLID = 0;
constexpr int TRANSPARENT = 1;
constexpr int BI_RGB = 0;
constexpr int HALFTONE = 4;
constexpr int COLORONCOLOR = 3;
constexpr BYTE AC_SRC_OVER = 0x00;
constexpr BYTE AC_SRC_ALPHA = 0x01;
constexpr int WHEEL_DELTA = 120;
constexpr int DC_BRUSH = 18;
constexpr int DC_PEN = 19;
constexpr int WHITE_BRUSH = 0;
constexpr int GRAY_BRUSH = 2;
constexpr int BLACK_BRUSH = 4;
constexpr DWORD INVALID_FILE_ATTRIBUTES = 0xFFFFFFFFu;
constexpr DWORD FILE_ATTRIBUTE_DIRECTORY = 0x00000010u;
constexpr DWORD REG_DWORD = 4u;
constexpr DWORD REG_SZ = 1u;
constexpr DWORD KEY_READ = 0x20019u;
constexpr DWORD KEY_SET_VALUE = 0x0002u;
constexpr DWORD DIB_RGB_COLORS = 0;
constexpr DWORD SRCCOPY = 0x00CC0020u;
constexpr DWORD BLACKNESS = 0x00000042u;
constexpr UINT MB_OK = 0x00000000u;
constexpr UINT MB_YESNO = 0x00000004u;
constexpr UINT MB_ICONERROR = 0x00000010u;
constexpr UINT MB_ICONQUESTION = 0x00000020u;
constexpr int IDYES = 6;
constexpr int GDI_ERROR = -1;
constexpr UINT PM_REMOVE = 0x0001u;
constexpr UINT WM_QUIT = 0x0012u;
constexpr int VK_BACK = 0x08;
constexpr int VK_TAB = 0x09;
constexpr int VK_RETURN = 0x0D;
constexpr int VK_ESCAPE = 0x1B;
constexpr int VK_MENU = 0x12;
constexpr int VK_CONTROL = 0x11;
constexpr int VK_SHIFT = 0x10;
constexpr int VK_PRIOR = 0x21;
constexpr int VK_NEXT = 0x22;
constexpr int VK_LEFT = 0x25;
constexpr int VK_UP = 0x26;
constexpr int VK_RIGHT = 0x27;
constexpr int VK_DOWN = 0x28;
constexpr int VK_ADD = 0x6B;
constexpr int VK_SUBTRACT = 0x6D;
constexpr int VK_F1 = 0x70;
constexpr int VK_F9 = 0x78;
constexpr int VK_INSERT = 0x2D;
constexpr int VK_OEM_MINUS = 0xBD;
constexpr int VK_OEM_PLUS = 0xBB;
constexpr int VK_DELETE = 0x2E;
constexpr int SM_CXDOUBLECLK = 36;
constexpr int SM_CYDOUBLECLK = 37;

constexpr UINT DT_LEFT = 0x00000000u;
constexpr UINT DT_TOP = 0x00000000u;
constexpr UINT DT_CENTER = 0x00000001u;
constexpr UINT DT_RIGHT = 0x00000002u;
constexpr UINT DT_VCENTER = 0x00000004u;
constexpr UINT DT_BOTTOM = 0x00000008u;
constexpr UINT DT_WORDBREAK = 0x00000010u;
constexpr UINT DT_SINGLELINE = 0x00000020u;
constexpr UINT DT_CALCRECT = 0x00000400u;
constexpr UINT DT_NOPREFIX = 0x00000800u;
constexpr UINT DT_EDITCONTROL = 0x00002000u;
constexpr UINT DT_END_ELLIPSIS = 0x00008000u;

#ifndef RGB
#define RGB(r, g, b) \
    (static_cast<COLORREF>((static_cast<BYTE>(r) | (static_cast<WORD>(static_cast<BYTE>(g)) << 8)) | (static_cast<DWORD>(static_cast<BYTE>(b)) << 16)))
#endif

#ifndef GetRValue
#define GetRValue(rgb) (static_cast<BYTE>((rgb) & 0xFFu))
#endif

#ifndef GetGValue
#define GetGValue(rgb) (static_cast<BYTE>(((rgb) >> 8) & 0xFFu))
#endif

#ifndef GetBValue
#define GetBValue(rgb) (static_cast<BYTE>(((rgb) >> 16) & 0xFFu))
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination, Length) std::memset((Destination), 0, (Length))
#endif

#ifndef CopyMemory
#define CopyMemory(Destination, Source, Length) std::memcpy((Destination), (Source), (Length))
#endif

#ifndef MoveMemory
#define MoveMemory(Destination, Source, Length) std::memmove((Destination), (Source), (Length))
#endif

#ifndef HKEY_CURRENT_USER
#define HKEY_CURRENT_USER ((HKEY)(static_cast<std::uintptr_t>(0x80000001u)))
#endif

inline int _vsnprintf_s(char* buffer, size_t bufferCount, int count, const char* format, va_list argList)
{
    if (!buffer || bufferCount == 0 || !format) {
        return -1;
    }

    const int result = std::vsnprintf(buffer, bufferCount, format, argList);
    if (result < 0 || count == _TRUNCATE || static_cast<size_t>(result) >= bufferCount) {
        buffer[bufferCount - 1] = '\0';
    }
    return result;
}

template <typename... Args>
inline int _snprintf_s(char* buffer, size_t bufferCount, int count, const char* format, Args... args)
{
    if (!buffer || bufferCount == 0 || !format) {
        return -1;
    }

    const int result = std::snprintf(buffer, bufferCount, format, args...);
    if (result < 0 || count == _TRUNCATE || static_cast<size_t>(result) >= bufferCount) {
        buffer[bufferCount - 1] = '\0';
    }
    return result;
}

inline int fopen_s(FILE** file, const char* filename, const char* mode)
{
    if (!file) {
        return EINVAL;
    }

    FILE* handle = std::fopen(filename, mode);
    *file = handle;
    return handle ? 0 : errno;
}

inline int _stricmp(const char* lhs, const char* rhs)
{
#if defined(__unix__) || defined(__APPLE__)
    return ::strcasecmp(lhs, rhs);
#else
    while (*lhs && *rhs) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(*lhs)));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(*rhs)));
        if (a != b) {
            return static_cast<unsigned char>(a) - static_cast<unsigned char>(b);
        }
        ++lhs;
        ++rhs;
    }
    return static_cast<unsigned char>(*lhs) - static_cast<unsigned char>(*rhs);
#endif
}

inline int _strnicmp(const char* lhs, const char* rhs, size_t count)
{
#if defined(__unix__) || defined(__APPLE__)
    return ::strncasecmp(lhs, rhs, count);
#else
    for (size_t index = 0; index < count; ++index) {
        const unsigned char lhsChar = static_cast<unsigned char>(lhs[index]);
        const unsigned char rhsChar = static_cast<unsigned char>(rhs[index]);
        const char a = static_cast<char>(std::tolower(lhsChar));
        const char b = static_cast<char>(std::tolower(rhsChar));
        if (a != b || lhsChar == '\0' || rhsChar == '\0') {
            return static_cast<unsigned char>(a) - static_cast<unsigned char>(b);
        }
    }
    return 0;
#endif
}

inline int _itoa_s(unsigned int value, char* buffer, size_t bufferCount, int radix)
{
    if (!buffer || bufferCount == 0 || radix != 10) {
        return EINVAL;
    }

    const int result = std::snprintf(buffer, bufferCount, "%u", value);
    if (result < 0 || static_cast<size_t>(result) >= bufferCount) {
        buffer[bufferCount - 1] = '\0';
        return ERANGE;
    }
    return 0;
}

inline DWORD GetFileAttributesA(const char* path)
{
    if (!path || !*path) {
        return INVALID_FILE_ATTRIBUTES;
    }

#if defined(__unix__) || defined(__APPLE__)
    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    struct stat st = {};
    if (::stat(normalized.c_str(), &st) != 0) {
        return INVALID_FILE_ATTRIBUTES;
    }

    DWORD attributes = 0;
    if (S_ISDIR(st.st_mode)) {
        attributes |= FILE_ATTRIBUTE_DIRECTORY;
    }
    return attributes;
#else
    return INVALID_FILE_ATTRIBUTES;
#endif
}

inline LONG RegOpenKeyExA(HKEY, const char*, DWORD, DWORD, HKEY* key)
{
    if (key) {
        *key = nullptr;
    }
    return 1;
}

inline LONG RegCreateKeyExA(HKEY, const char*, DWORD, char*, DWORD, DWORD, void*, HKEY* key, DWORD*)
{
    if (key) {
        *key = nullptr;
    }
    return 1;
}

inline LONG RegQueryValueExA(HKEY, const char*, DWORD*, DWORD*, BYTE*, DWORD*)
{
    return 1;
}

inline LONG RegSetValueExA(HKEY, const char*, DWORD, DWORD, const BYTE*, DWORD)
{
    return 1;
}

inline LONG RegDeleteValueA(HKEY, const char*)
{
    return 1;
}

inline LONG RegCloseKey(HKEY)
{
    return 0;
}

inline DWORD GetModuleFileNameA(HMODULE, char* buffer, DWORD size)
{
    if (!buffer || size == 0) {
        return 0;
    }

#if defined(__unix__)
    const ssize_t length = ::readlink("/proc/self/exe", buffer, static_cast<size_t>(size) - 1);
    if (length <= 0 || static_cast<DWORD>(length) >= size) {
        buffer[0] = '\0';
        return 0;
    }

    buffer[length] = '\0';
    return static_cast<DWORD>(length);
#elif defined(__APPLE__)
    uint32_t pathSize = size;
    if (_NSGetExecutablePath(buffer, &pathSize) != 0 || pathSize >= size) {
        buffer[0] = '\0';
        return 0;
    }

    char resolvedPath[PATH_MAX] = {};
    const char* finalPath = ::realpath(buffer, resolvedPath);
    if (!finalPath) {
        finalPath = buffer;
    }

    const size_t finalLength = std::strlen(finalPath);
    if (finalLength == 0 || finalLength >= size) {
        buffer[0] = '\0';
        return 0;
    }

    std::memmove(buffer, finalPath, finalLength + 1);
    return static_cast<DWORD>(finalLength);
#else
    buffer[0] = '\0';
    return 0;
#endif
}

inline HMODULE LoadLibraryA(const char* libraryPath)
{
#if defined(__unix__) || defined(__APPLE__)
    if (!libraryPath || !*libraryPath) {
        return nullptr;
    }

    std::string normalized(libraryPath);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return ::dlopen(normalized.c_str(), RTLD_NOW | RTLD_LOCAL);
#else
    (void)libraryPath;
    return nullptr;
#endif
}

inline BOOL FreeLibrary(HMODULE module)
{
#if defined(__unix__) || defined(__APPLE__)
    return module && ::dlclose(module) == 0 ? TRUE : FALSE;
#else
    (void)module;
    return FALSE;
#endif
}

inline void* GetProcAddress(HMODULE module, const char* procName)
{
#if defined(__unix__) || defined(__APPLE__)
    return module ? ::dlsym(module, procName) : nullptr;
#else
    (void)module;
    (void)procName;
    return nullptr;
#endif
}

#endif

#endif