#include "Video.h"
#include "core/DllMgr.h"

CBink::CBink() : m_Bink(nullptr), m_BinkBuffer(nullptr), m_IsBinkPlaying(0), m_BinkMode(0) {}

CBink::~CBink() {
    Close();
}

bool CBink::Open(const char* fName, u32 flags) {
    if (!g_dllExports.BinkOpen) return false;

    // The BINK handle is actually void* in our DllProtos.h
    m_Bink = (BINK*)g_dllExports.BinkOpen(fName, flags);
    if (m_Bink) {
        m_IsBinkPlaying = 1;
        return true;
    }
    return false;
}

void CBink::Close() {
    if (m_Bink && g_dllExports.BinkClose) {
        g_dllExports.BinkClose(m_Bink);
        m_Bink = nullptr;
        m_IsBinkPlaying = 0;
    }
}

void CBink::Play() {
    if (m_Bink && g_dllExports.BinkDoFrame) {
        g_dllExports.BinkDoFrame(m_Bink);
        g_dllExports.BinkNextFrame(m_Bink);
    }
}

void CBink::Stop() {
    Close();
}

void CBink::SetPause(bool pause) {
    if (m_Bink && g_dllExports.BinkPause) {
        g_dllExports.BinkPause(m_Bink, pause ? 1 : 0);
    }
}

void CBink::Render() {
    // Rendering logic
}
