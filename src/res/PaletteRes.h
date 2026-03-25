#pragma once

#include "Res.h"
#include <windows.h>

class CPaletteRes : public CRes {
public:
    PALETTEENTRY m_pal[256];

    CPaletteRes();
    virtual ~CPaletteRes();

    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;
};
