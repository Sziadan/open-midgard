#pragma once
#include "Types.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ro {
// Source CommonObject type0 token file and session map-name table. Values stay
// in source bytes; display conversion belongs to the receiving text owner.
class SourceMapNames {
public:
    bool Load();
    bool Load(u16 userLanguageId);
    void Parse(std::string_view bytes);
    const std::string* Find(const char* rswName) const;
private:
    std::vector<std::pair<std::string,std::string>> m_rows;
};
}
