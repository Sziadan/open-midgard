#include "core/InsultWords.h"

#include <cstdio>
#include <string_view>

namespace {

void Check(bool condition, const char* name, int& failures)
{
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", name);
        ++failures;
    }
}

std::size_t ByteBoundary(std::string_view bytes, std::size_t offset)
{
    return offset < bytes.size() ? offset + 1 : bytes.size();
}

// Test-only stand-in for a DBCS stepping rule: 0x81 begins a two-byte unit.
std::size_t DbcsBoundary(std::string_view bytes, std::size_t offset)
{
    if (offset >= bytes.size())
        return bytes.size();
    if (static_cast<unsigned char>(bytes[offset]) == 0x81 && offset + 1 < bytes.size())
        return offset + 2;
    return offset + 1;
}

// Advance over one UTF-8 code point. The fixtures use valid UTF-8 sequences.
std::size_t Utf8Boundary(std::string_view bytes, std::size_t offset)
{
    if (offset >= bytes.size())
        return bytes.size();
    const unsigned char lead = static_cast<unsigned char>(bytes[offset]);
    std::size_t count = lead < 0x80 ? 1 : (lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4));
    if (count > bytes.size() - offset)
        count = bytes.size() - offset;
    return offset + count;
}

} // namespace

int main()
{
    int failures = 0;
    const auto byteStep = ByteBoundary;
    ro::InsultWords words;

    words.AppendToken("bad", byteStep);
    Check(words.IsBadSentence("This is BAD news", byteStep),
          "ASCII substring match ignores case", failures);
    Check(words.IsBadSentence("bade", byteStep),
          "prefix plus trailing text still matches substring", failures);
    Check(words.IsBadSentence("xxbadxx", byteStep),
          "substring matching starts at each byte", failures);
    Check(!words.IsBadSentence("kind words", byteStep),
          "unmatched sentence is good", failures);

    words.Clear();
    words.AppendToken("  BAD \t", byteStep);
    Check(words.words().size() == 1 && words.words()[0] == "  bad \t",
          "leading and trailing whitespace is retained", failures);
    Check(words.IsBadSentence("prefix  bad \t suffix", byteStep),
          "retained whitespace participates in matching", failures);
    words.AppendToken(" \t  ", byteStep);
    words.AppendToken("", byteStep);
    Check(words.words().size() == 1, "blank tokens are ignored", failures);

    words.Clear();
    words.AppendToken(std::string_view("bad\0ignored", 11), byteStep);
    Check(words.words().size() == 1 && words.words()[0] == "bad",
          "token is truncated at first NUL", failures);
    Check(words.IsBadSentence(std::string_view("bad\0clean", 9), byteStep),
          "sentence match before NUL is retained", failures);
    Check(!words.IsBadSentence(std::string_view("clean\0bad", 9), byteStep),
          "sentence bytes after NUL are ignored", failures);

    words.Clear();
    const std::string dbcsSentence("\x81" "A", 2);
    words.AppendToken("a", DbcsBoundary);
    Check(!words.IsBadSentence(dbcsSentence, DbcsBoundary),
          "DBCS trail ASCII is neither lowercased nor a match start", failures);
    words.Clear();
    words.AppendToken(std::string_view("\x81" "A", 2), DbcsBoundary);
    Check(words.words()[0] == std::string("\x81" "A", 2),
          "DBCS trail byte is retained when token is lowercased", failures);
    Check(words.IsBadSentence(dbcsSentence, DbcsBoundary),
          "DBCS token can match from its character boundary", failures);

    words.Clear();
    words.AppendToken("x", Utf8Boundary);
    const std::string utf8Sentence("\xC3\x84X", 3);
    Check(words.IsBadSentence(utf8Sentence, Utf8Boundary),
          "UTF-8 stepping reaches and lowercases following ASCII", failures);
    Check(!words.IsBadSentence(std::string_view("\xC3\x84", 2), Utf8Boundary),
          "UTF-8 continuation bytes are not sentence start positions", failures);

    words.Clear();
    Check(words.words().empty(), "Clear removes all owned words", failures);
    return failures == 0 ? 0 : 1;
}
