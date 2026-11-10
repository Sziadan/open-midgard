#include "core/SourceChatTokenizer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void Check(std::string_view input, const std::vector<std::string>& expected, int line)
{
    const auto actual = ro::chat::SourceChatTokens(input);
    if (actual != expected) {
        std::cerr << "SourceChatTokens mismatch at line " << line << ": got "
                  << actual.size() << " tokens, expected " << expected.size() << '\n';
        std::exit(1);
    }
}

} // namespace

#define CHECK(input, ...) Check(input, std::vector<std::string>{__VA_ARGS__}, __LINE__)

int main()
{
    CHECK("",);
    CHECK("one two\tthree\nfour", "one", "two", "three", "four");
    CHECK(" \t one  two ", "one", "two");
    CHECK("word", "word");
    CHECK("word ", "word");

    // Quotes reset prior pending text, preserve internal whitespace, and are
    // emitted even when empty. An unfinished quote is emitted at EOF.
    CHECK("discarded\"quoted value\" tail", "quoted value", "tail");
    CHECK("\"\" next", "", "next");
    CHECK("prefix\"unfinished value", "unfinished value");
    CHECK("\"", "");

    // A comment is recognized only in read mode and drops the current token.
    CHECK("first pending//comment", "first");
    CHECK("first pending // comment", "first", "pending");
    CHECK("first \"inside // quote\" end", "first", "inside // quote", "end");
    CHECK("//comment",);

    const std::string nulTerminated("first second\0 third", 20);
    CHECK(nulTerminated, "first", "second");

    std::string highByte = "a";
    highByte.push_back(static_cast<char>(0xE9));
    highByte += " b";
    std::string highByteToken = "a";
    highByteToken.push_back(static_cast<char>(0xE9));
    Check(highByte, {highByteToken, "b"}, __LINE__);
    return 0;
}
