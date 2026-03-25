#pragma once

#include <windows.h>
#include <ddraw.h>
#include <list>
#include "res/Texture.h"

// Base class for drawing contexts
class CDC {
public:
    CDC() {}
    virtual ~CDC() {}

    virtual bool GetDC(HDC* phdc) = 0;
    virtual void ReleaseDC(HDC hdc) = 0;

    virtual void BltSprite(int x, int y, class CSprRes* sprRes, struct CMotion* curMotion, unsigned int* palette) = 0;
    virtual void BltTexture2(int x, int y, class CTexture* src, int srcx, int srcy, int w, int h, int xflip, int zoomx, int zoomy) = 0;
    virtual void ClearSurface(RECT* rect, unsigned int color) = 0;
    virtual void CopyRect(int x, int y, int w, int h, CDC* src) = 0;
    virtual void DrawSurface(int x, int y, int w, int h, unsigned int color) = 0;
    virtual void Resize(int w, int h) = 0;
    virtual void Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey) = 0;
};

bool DrawActMotionToHdc(HDC hdc, int x, int y, class CSprRes* sprRes, const struct CMotion* motion, unsigned int* palette);

// Drawing context for software bitmaps backed by DIB sections
class CDCBitmap : public CDC {
public:
    CDCBitmap(unsigned int w, unsigned int h);
    virtual ~CDCBitmap();

    unsigned int* GetImageData() { return m_image; }
    const unsigned int* GetImageData() const { return m_image; }
    unsigned int GetWidth() const { return m_w; }
    unsigned int GetHeight() const { return m_h; }

    virtual bool GetDC(HDC* phdc) override;
    virtual void ReleaseDC(HDC hdc) override;

    virtual void BltSprite(int x, int y, class CSprRes* sprRes, struct CMotion* curMotion, unsigned int* palette) override;
    virtual void BltTexture2(int x, int y, class CTexture* src, int srcx, int srcy, int w, int h, int xflip, int zoomx, int zoomy) override;
    virtual void ClearSurface(RECT* rect, unsigned int color) override;
    virtual void CopyRect(int x, int y, int w, int h, CDC* src) override;
    virtual void DrawSurface(int x, int y, int w, int h, unsigned int color) override;
    virtual void Resize(int w, int h) override;
    virtual void Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey) override;

protected:
    void CreateDCSurface(unsigned int w, unsigned int h);
    void UpdateSurface();

    HDC m_dc;
    HBITMAP m_bitmap;
    HBITMAP m_bitmapOld;
    unsigned int* m_image;
    unsigned int m_w, m_h;
    bool m_dirty;

    std::list<CTexture*> m_textureList;
};

// Drawing context for DirectDraw surfaces
class CDCSurface : public CDC {
public:
    CDCSurface(unsigned int w, unsigned int h);
    CDCSurface(unsigned int w, unsigned int h, IDirectDrawSurface7* pSurface);
    virtual ~CDCSurface();

    virtual bool GetDC(HDC* phdc) override;
    virtual void ReleaseDC(HDC hdc) override;

    virtual void BltSprite(int x, int y, class CSprRes* sprRes, struct CMotion* curMotion, unsigned int* palette) override;
    virtual void BltTexture2(int x, int y, class CTexture* src, int srcx, int srcy, int w, int h, int xflip, int zoomx, int zoomy) override;
    virtual void ClearSurface(RECT* rect, unsigned int color) override;
    virtual void CopyRect(int x, int y, int w, int h, CDC* src) override;
    virtual void DrawSurface(int x, int y, int w, int h, unsigned int color) override;
    virtual void Resize(int w, int h) override;
    virtual void Update(int x, int y, int w, int h, unsigned int* data, bool skipColorKey) override;

    CSurface m_surface;
};
