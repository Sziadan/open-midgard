#pragma once
#include "Res.h"

class CBitmapRes : public CRes {
public:
    int m_isAlpha;
    int m_width;
    int m_height;
    u32* m_data;

    CBitmapRes();
    virtual ~CBitmapRes();

    virtual bool Load(const char* fName) override;
    virtual bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size);
    virtual CRes* Clone() override;
    virtual void Reset() override;

    bool ChangeToGrayScaleBMP(int a, int b);
    bool SetAlphaWithBMP(const char* fName);
    unsigned long GetColor(int x, int y);
    void CreateNull(int w, int h);

private:
    bool LoadBMPData(const unsigned char* buffer, int size);
    bool LoadJPGData(const unsigned char* buffer, int size);
    bool LoadTGAData(const unsigned char* buffer, int size);
};
