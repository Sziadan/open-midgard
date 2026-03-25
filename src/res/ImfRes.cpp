#include "ImfRes.h"

#include <algorithm>
#include <cstring>

CImfRes::CImfRes() {
    Reset();
}

CImfRes::~CImfRes() {
}

bool CImfRes::Load(const char* fName) {
    return CRes::Load(fName);
}

bool CImfRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) {
    (void)fName;
    Reset();

    if (!buffer || size < 16) {
        return false;
    }

    const unsigned char* cursor = buffer;
    const unsigned char* end = buffer + size;

    auto readInt = [&](int& out) -> bool {
        if (cursor + 4 > end) {
            return false;
        }
        std::memcpy(&out, cursor, sizeof(out));
        cursor += 4;
        return true;
    };

    int version = 0;
    int checksum = 0;
    int maxLayers = 0;
    if (!readInt(version) || !readInt(checksum) || !readInt(maxLayers)) {
        return false;
    }

    if (maxLayers < 0 || maxLayers > 15) {
        return false;
    }

    m_ImfData.resize(static_cast<size_t>(maxLayers));
    for (int layer = 0; layer < maxLayers; ++layer) {
        int numAction = 0;
        if (!readInt(numAction) || numAction < 0) {
            return false;
        }

        m_ImfData[layer].resize(static_cast<size_t>(numAction));
        m_maxAction[layer] = numAction;

        for (int action = 0; action < numAction; ++action) {
            int numMotion = 0;
            if (!readInt(numMotion) || numMotion < 0) {
                return false;
            }

            if (action < static_cast<int>(m_maxMotion[layer].size())) {
                m_maxMotion[layer][action] = numMotion;
            }

            ImfData& motionData = m_ImfData[layer][action];
            motionData.m_Priority.resize(static_cast<size_t>(numMotion));
            motionData.m_cx.resize(static_cast<size_t>(numMotion));
            motionData.m_cy.resize(static_cast<size_t>(numMotion));

            for (int motion = 0; motion < numMotion; ++motion) {
                if (!readInt(motionData.m_Priority[motion])
                    || !readInt(motionData.m_cx[motion])
                    || !readInt(motionData.m_cy[motion])) {
                    return false;
                }
            }
        }
    }

    return true;
}

CRes* CImfRes::Clone() {
    return new CImfRes();
}

void CImfRes::Reset() {
    m_ImfData.clear();
    m_maxAction.fill(0);
    for (auto& motions : m_maxMotion) {
        motions.fill(0);
    }
}

int CImfRes::CheckSum(int) { return 0; }
int CImfRes::GetLayer(int priority, int action, int motion) {
    if (priority < 0 || priority >= static_cast<int>(m_maxAction.size())) {
        return -1;
    }
    if (action < 0 || action >= m_maxAction[priority]) {
        return -1;
    }
    if (motion < 0 || motion >= m_maxMotion[priority][action]) {
        return -1;
    }

    const int numLayers = GetNumLayer();
    for (int layer = 0; layer < numLayers; ++layer) {
        if (GetPriority(layer, action, motion) == priority) {
            return layer;
        }
    }
    return -1;
}

int CImfRes::GetLongestMotion(int action) {
    int longest = 0;
    const int numLayers = GetNumLayer();
    for (int layer = 0; layer < numLayers; ++layer) {
        longest = (std::max)(longest, GetNumMotion(layer, action));
    }
    return longest;
}

int CImfRes::GetNumAction(int layer) {
    if (layer < 0 || layer >= static_cast<int>(m_ImfData.size())) {
        return 0;
    }
    return m_maxAction[layer];
}

int CImfRes::GetNumLayer() {
    return static_cast<int>(m_ImfData.size());
}

int CImfRes::GetNumMotion(int layer, int action) {
    if (layer < 0 || layer >= static_cast<int>(m_ImfData.size())) {
        return 0;
    }
    if (action < 0 || action >= static_cast<int>(m_maxMotion[layer].size())) {
        return 0;
    }
    return m_maxMotion[layer][action];
}

int CImfRes::GetPriority(int layer, int action, int motion) {
    if (layer < 0 || layer >= static_cast<int>(m_ImfData.size())) {
        return 0;
    }
    if (action < 0 || action >= static_cast<int>(m_ImfData[layer].size())) {
        return 0;
    }
    const ImfData& data = m_ImfData[layer][action];
    if (motion < 0 || motion >= static_cast<int>(data.m_Priority.size())) {
        return 0;
    }
    return data.m_Priority[motion];
}

POINT CImfRes::GetPoint(int layer, int action, int motion) {
    POINT point{ 0, 0 };
    if (layer < 0 || layer >= static_cast<int>(m_ImfData.size())) {
        return point;
    }
    if (action < 0 || action >= static_cast<int>(m_ImfData[layer].size())) {
        return point;
    }
    const ImfData& data = m_ImfData[layer][action];
    if (motion < 0 || motion >= static_cast<int>(data.m_cx.size()) || motion >= static_cast<int>(data.m_cy.size())) {
        return point;
    }
    point.x = data.m_cx[motion];
    point.y = data.m_cy[motion];
    return point;
}

void CImfRes::Create() {}
