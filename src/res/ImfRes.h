#pragma once

#include "Res.h"
#include <windows.h>
#include <array>
#include <vector>

struct ImfData {
    std::vector<int> m_Priority;
    std::vector<int> m_cx;
    std::vector<int> m_cy;
};

class CImfRes : public CRes {
public:
    std::vector<std::vector<ImfData>> m_ImfData;
    std::array<int, 15> m_maxAction;
    std::array<std::array<int, 104>, 15> m_maxMotion;

    CImfRes();
    virtual ~CImfRes();

    virtual bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;

    int CheckSum(int);
    int GetLayer(int, int, int);
    int GetLongestMotion(int);
    int GetNumAction(int);
    int GetNumLayer();
    int GetNumMotion(int, int);
    int GetPriority(int, int, int);
    POINT GetPoint(int, int, int);
    void Create();
};
