#include "SourceMapTypeTable.h"
#include "File.h"

#include <cstdlib>
#include <algorithm>
#include <limits>
#include <string>

namespace ro {

void SourceMapTypeTable::EnsureLoaded()
{
    if (!m_initialized) Load();
}

void SourceMapTypeTable::Clear()
{
    m_initialized = true;
    m_values = {};
}

bool SourceMapTypeTable::Load()
{
    Clear();
    CFile file;
    if (!file.Open("MapInfoTable.scp") && !file.Open("data\\mapinfotable.scp")) return false;
    const int size = file.Size();
    if (size < 0) return false;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (size && !file.Read(bytes.data(), static_cast<u32>(size))) return false;
    Parse(bytes);
    return true;
}

void SourceMapTypeTable::Parse(std::string_view bytes)
{
    Clear();
    std::size_t offset = 0, count = 0;
    const auto space = [](char value) {
        return value == ' ' || value == '\t' || value == '\r' || value == '\n';
    };
    while (offset < bytes.size() && count < kMapTypes * kProperties) {
        if (space(bytes[offset])) {
            ++offset;
        } else if (bytes.substr(offset, 2) == "//") {
            const auto end = bytes.find('\n', offset + 2);
            offset = end == std::string_view::npos ? bytes.size() : end + 1;
        } else if (bytes.substr(offset, 2) == "/*") {
            const auto end = bytes.find("*/", offset + 2);
            offset = end == std::string_view::npos ? bytes.size() : end + 2;
        } else {
            const auto start = offset;
            while (offset < bytes.size() && !space(bytes[offset])) ++offset;
            const std::string token(bytes.substr(start, offset - start));
            const long number = std::strtol(token.c_str(), nullptr, 10);
            m_values[count / kProperties][count % kProperties] = static_cast<int>(std::clamp(number,
                static_cast<long>((std::numeric_limits<int>::min)()),
                static_cast<long>((std::numeric_limits<int>::max)())));
            ++count;
        }
    }
}

bool SourceMapTypeTable::Enabled(int mapType, Property property)
{
    EnsureLoaded();
    return mapType >= 0 && static_cast<std::size_t>(mapType) < kMapTypes
        && static_cast<std::size_t>(property) < kProperties && m_values[mapType][property] != 0;
}

}
