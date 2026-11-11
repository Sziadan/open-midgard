#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace ro {

class SourceMapTypeTable {
public:
    static constexpr std::size_t kMapTypes = 30;
    static constexpr std::size_t kProperties = 15;
    enum Property : std::size_t { AllyParty = 7, AllyGuild = 8, Siege = 9 };

    void EnsureLoaded();
    void Clear();
    bool Load();
    void Parse(std::string_view bytes);
    bool Enabled(int mapType, Property property);

private:
    std::array<std::array<int, kProperties>, kMapTypes> m_values{};
    bool m_initialized = false;
};

}
