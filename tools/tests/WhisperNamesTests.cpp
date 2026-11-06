#include "core/WhisperNames.h"

#include <iostream>
#include <string>
#include <string_view>

namespace {
int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void ExpectSame(std::string_view first, std::string_view second, const char* description)
{
    Check(ro::chat::WhisperNamesLookAlike(first, second), description);
}

void ExpectDifferent(std::string_view first, std::string_view second, const char* description)
{
    Check(!ro::chat::WhisperNamesLookAlike(first, second), description);
}
} // namespace

int main()
{
    // Short names inherit the source's signed minLength - 2 threshold.
    ExpectSame("", "", "empty names match at the non-positive positional threshold");
    ExpectSame("a", "x", "one-byte names match regardless of byte value");
    ExpectSame("ab", "xy", "two-byte names match regardless of byte values");
    ExpectSame("ab", "wxyz", "two-byte minimum matches with length difference two");

    // The only special byte equivalence is uppercase I with lowercase l.
    ExpectSame("IIIIII", "llllll", "all ASCII I and lowercase l bytes are interchangeable");
    ExpectDifferent("IIIIII", "iiiiii", "ordinary letter case remains byte-sensitive");
    ExpectDifferent("ABCD", "WXYZ", "four unrelated bytes do not match");

    // Positional scoring allows at most two mismatches for a five-byte minimum.
    ExpectSame("abcde", "aXXde", "two positional mismatches meet the m-minus-two threshold");
    ExpectDifferent("abcde", "aXXXe", "three positional mismatches miss the threshold");

    // First-argument-longer shifted comparisons, in the source's exact order.
    ExpectSame("xabc", "abc", "one-byte first-argument shift matches");
    ExpectSame("xxabc", "abc", "two-byte first-argument shift matches");
    ExpectDifferent("xxxabc", "abc", "three-byte first-argument shift is outside the accepted alignment");
    ExpectSame("xxabcZZ", "abc", "two-byte shift accepts a four-byte length excess");
    ExpectDifferent("abc", "abcXYZ", "a three-byte shorter-first case is length-gated");
    ExpectDifferent("abc", "abcWXYZ", "a four-byte shorter-first case is length-gated");
    ExpectSame("abc", "abcXY", "two-byte longer-second suffix passes positional scoring");

    // The final alignment pass skips only from its first argument.
    ExpectSame("abXcde", "abcde", "one inserted first-argument byte is skipped");
    ExpectDifferent("abcde", "abXcde", "reverse insertion is not aligned by the skip pass");

    // The source's C-string copies stop at the first NUL.
    const std::string_view nulTerminated{"abc\0trailing bytes", 18};
    ExpectSame(nulTerminated, "abc", "bytes after embedded NUL do not participate");
    ExpectDifferent(std::string_view{"abc\0x", 5}, "abcXYZ",
        "NUL truncation occurs before the three-byte length gate");

    // Protocol fields may contain all 24 bytes; the port handles them without
    // reproducing Ref's 24-byte stack-buffer overflow.
    const std::string fullName(24, 'a');
    ExpectSame(fullName, fullName, "identical 24-byte protocol names are safe");
    std::string changedFullName = fullName;
    changedFullName[22] = 'b';
    changedFullName[23] = 'b';
    ExpectSame(fullName, changedFullName, "24-byte names preserve the two-mismatch tolerance");
    changedFullName.assign(24, 'z');
    ExpectDifferent(fullName, changedFullName, "unrelated 24-byte names remain different");
    Check(!ro::chat::WhisperNamesLookAlike(std::string(25, 'a'), fullName),
        "names beyond the 24-byte protocol bound are rejected safely");

    if (failures != 0) {
        std::cerr << failures << " whisper-name checks failed\n";
        return 1;
    }
    std::cout << "PASS: Ref-compatible whisper name heuristic cases\n";
    return 0;
}
