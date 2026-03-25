#include "PaletteRes.h"
#include "core/File.h"

CPaletteRes::CPaletteRes() {
    Reset();
}

CPaletteRes::~CPaletteRes() {
}

bool CPaletteRes::Load(const char* fName) {
    CFile file;
    if (!file.Open(fName)) return false;

    if (file.Read(m_pal, sizeof(m_pal)) == false) {
        return false;
    }

    return true;
}

CRes* CPaletteRes::Clone() {
    return new CPaletteRes();
}

void CPaletteRes::Reset() {
    memset(m_pal, 0, sizeof(m_pal));
}
