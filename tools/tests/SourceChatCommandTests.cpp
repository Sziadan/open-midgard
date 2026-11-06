#include "core/SourceChatCommand.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void Check(std::string_view input, std::string_view expected, int line)
{
    const std::string actual = ro::chat::SourceChatCommandToken(input);
    if (actual != expected) {
        std::cerr << "SourceChatCommandToken mismatch at line " << line << '\n';
        std::exit(1);
    }
}

} // namespace

#define CHECK(input, expected) Check(input, expected, __LINE__)

int main()
{
    CHECK("/AM args", "/am");
    CHECK("/am\targs", "/am");
    CHECK("/am\nargs", "/am");
    CHECK(" /am", "");
    CHECK("quoted /am", "");
    CHECK("//am", "");
    CHECK("/am//x", "");
    CHECK("/am //x", "/am");
    CHECK("/am\"x\"", "x");
    CHECK("/am\"MiXeD\" trailing", "mixed");
    CHECK("/am\"\"", "");
    CHECK("/am\"unclosed", "unclosed");
    CHECK("/AM\t\"Quoted\"", "/am");

    const std::string nulTerminated("/AM\0 trailing", 13);
    CHECK(nulTerminated, "/am");

    std::string highByte = "/A";
    highByte.push_back(static_cast<char>(0xE9));
    highByte += " args";
    std::string expectedHighByte = "/a";
    expectedHighByte.push_back(static_cast<char>(0xE9));
    CHECK(highByte, expectedHighByte);
    CHECK(std::string_view("\"/am\""), "");
    return 0;
}
