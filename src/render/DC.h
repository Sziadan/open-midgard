#pragma once

#include <windows.h>

class ArgbDibSurface {
public:
    ArgbDibSurface();
    ~ArgbDibSurface();

    ArgbDibSurface(const ArgbDibSurface&) = delete;
    ArgbDibSurface& operator=(const ArgbDibSurface&) = delete;

    bool EnsureSize(int width, int height);
    void Release();

    bool IsValid() const;
    HDC GetDC() const;
    void* GetBits() const;
    unsigned int* GetPixels() const;
    int GetWidth() const;
    int GetHeight() const;

private:
    HDC m_dc;
    HBITMAP m_bitmap;
    HGDIOBJ m_oldBitmap;
    void* m_bits;
    int m_width;
    int m_height;
};

void BlitMotionToArgb(unsigned int* dest, int destW, int destH, int baseX, int baseY, class CSprRes* sprRes, const struct CMotion* motion, unsigned int* palette);
bool StretchArgbToHdc(HDC hdc,
                      int dstX,
                      int dstY,
                      int dstWidth,
                      int dstHeight,
                      const unsigned int* pixels,
                      int pixelWidth,
                      int pixelHeight,
                      int srcX = 0,
                      int srcY = 0,
                      int srcWidth = -1,
                      int srcHeight = -1);
bool AlphaBlendArgbToHdc(HDC hdc,
                         int dstX,
                         int dstY,
                         int dstWidth,
                         int dstHeight,
                         const unsigned int* pixels,
                         int pixelWidth,
                         int pixelHeight,
                         int srcX = 0,
                         int srcY = 0,
                         int srcWidth = -1,
                         int srcHeight = -1);
bool DrawActMotionToHdc(HDC hdc, int x, int y, class CSprRes* sprRes, const struct CMotion* motion, unsigned int* palette);
bool DrawActMotionToArgb(unsigned int* dest, int destW, int destH, int x, int y, class CSprRes* sprRes, const struct CMotion* motion, unsigned int* palette);
