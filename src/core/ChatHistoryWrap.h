#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace ro::chat {
// PolishText4 sums the advances of individual CharNextExA units, rather than
// choosing word boundaries. The port uses UTF-8 units and an injected pixel metric.
template<class Measure>
std::vector<std::string> WrapHistory(std::string_view text, int width, Measure measure)
{
    std::vector<std::string> result;
    if (text.empty() || measure(text) <= 0) return result;
    std::size_t start = 0, cursor = 0;
    long long advance = 0;
    while (cursor < text.size()) {
        const auto unit = cursor;
        ++cursor;
        while (cursor < text.size() && (static_cast<unsigned char>(text[cursor]) & 0xc0) == 0x80) ++cursor;
        if (text[unit] == '\r') {
            // Source consumes CR plus the following byte, normally LF; blank
            // lines become a space. Bound the original read at the end of input.
            result.emplace_back(unit == start ? " " : text.substr(start, unit - start));
            start = cursor = cursor < text.size() ? cursor + 1 : cursor;
            advance = 0;
            continue;
        }
        advance += measure(text.substr(unit, cursor - unit));
        if (text[unit] == '^' && text.size() - unit >= 7) {
            bool hex = true;
            for (std::size_t i = unit + 1; i < unit + 7; ++i) {
                const char c = text[i];
                hex &= (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            }
            // Original subtracts the width of ^000000 here, then still visits
            // the six digits. Keep that arithmetic, without its malformed read.
            if (hex) advance -= measure("^000000");
        }
        if (advance > width) {
            // An overwide first unit made the source loop indefinitely. Retain
            // the complete unit once instead, so tiny/malformed layouts progress.
            const auto end = unit == start ? cursor : unit;
            result.emplace_back(text.substr(start, end - start));
            start = cursor = end;
            advance = 0;
        }
    }
    if (start < cursor) result.emplace_back(text.substr(start, cursor - start));
    return result;
}
} // namespace ro::chat
