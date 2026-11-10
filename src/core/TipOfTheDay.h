#pragma once
#include "Types.h"
#include <string>
#include <vector>

// Source CTipOfTheDay519F50/51A000/51A350. Content is retained as raw
// resource bytes; the viewer owns charset conversion and coloured line layout.
class CTipOfTheDay {
public:
    using Tip=std::vector<std::string>;
    bool Load();
    bool Load(u16 userLanguageId);
    std::size_t GetNumTip();
    const Tip& GetTip(int index);
    bool m_bTryLoading=false;
private:
    void AppendDelimited(const std::string& bytes);
    std::vector<Tip> m_tips;
};

std::string SourceTipLanguageFolder(u16 userLanguageId);
extern CTipOfTheDay g_tipOfTheDay;
extern int g_showTipsAtStartup;
void LoadSourceTipOptions();
bool SaveSourceTipOptions(int value);
