#include "../../src/core/ChatHistoryWrap.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct FixedAsciiMetric {
    int operator()(std::string_view text) const
    {
        int width = 0;
        for (char ch : text) {
            if (ch != '\r' && ch != '\n') width += 2;
        }
        return width;
    }
};

void Require(bool ok, const char* name)
{
    if (!ok) {
        std::cerr << "ChatHistoryWrapTests failed: " << name << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using ro::chat::WrapHistory;
    const FixedAsciiMetric ascii;

    Require(WrapHistory("abcd", 8, ascii) == std::vector<std::string>{"abcd"},
        "exact width fits");
    Require(WrapHistory("abcd", 6, ascii) == std::vector<std::string>{"abc", "d"},
        "splits inside word without word-boundary reflow");
    Require(WrapHistory("ab cd", 6, ascii) == std::vector<std::string>{"ab ", "cd"},
        "spaces are preserved");
    Require(WrapHistory("abc", 4, ascii) == std::vector<std::string>{"ab", "c"},
        "overflowing unit is excluded from preceding row");
    Require(WrapHistory("a\r\n\r\nb", 8, ascii)
        == std::vector<std::string>{"a", " ", "b"}, "CRLF blank line becomes a space");
    Require(WrapHistory("a\r\n", 8, ascii) == std::vector<std::string>{"a"},
        "terminal CRLF adds no empty row");
    Require(WrapHistory("", 8, ascii).empty(), "empty input");

    const auto utf8Metric = [](std::string_view unit) {
        if (unit == "\xc3\xa9" || unit == "\xe4\xb8\x80") return 2;
        return unit == "^000000" ? 14 : static_cast<int>(unit.size()) * 2;
    };
    Require(WrapHistory("\xc3\xa9\xe4\xb8\x80", 2, utf8Metric)
        == std::vector<std::string>{"\xc3\xa9", "\xe4\xb8\x80"},
        "UTF-8 units stay complete");
    Require(WrapHistory("^000000X", 2, utf8Metric)
        == std::vector<std::string>{"^000000X"}, "complete color marker has zero width");

    const auto narrowMetric = [](std::string_view unit) {
        return unit == "x" ? 4 : static_cast<int>(unit.size()) * 2;
    };
    Require(WrapHistory("x", 1, narrowMetric) == std::vector<std::string>{"x"},
        "tiny-width safe progress divergence from source parity");
    return 0;
}
