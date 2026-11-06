#pragma once

#include <cstddef>
#include <string_view>

namespace ro::chat {

// Reproduces Ref's byte-based CompareTwoNames heuristic for protocol names.
// Ref copied into char[24] with sprintf, which overflows for a 24-byte name.
// This safe port supports all 24 protocol bytes without a terminator; inputs
// longer than the protocol field are rejected. Embedded NUL ends each name,
// as it did for Ref's C-string copy and strlen calls.
inline bool WhisperNamesLookAlike(std::string_view first, std::string_view second)
{
    constexpr std::size_t kMaxProtocolNameBytes = 24;
    const auto firstNul = first.find('\0');
    if (firstNul != std::string_view::npos) first = first.substr(0, firstNul);
    const auto secondNul = second.find('\0');
    if (secondNul != std::string_view::npos) second = second.substr(0, secondNul);
    if (first.size() > kMaxProtocolNameBytes || second.size() > kMaxProtocolNameBytes) {
        return false;
    }

    const auto equivalent = [](unsigned char left, unsigned char right) {
        if (left == right) return true;
        const bool leftLookalike = left == 0x49 || left == 0x6c; // 'I' or 'l'
        const bool rightLookalike = right == 0x49 || right == 0x6c;
        return leftLookalike && rightLookalike;
    };

    const std::size_t firstLength = first.size();
    const std::size_t secondLength = second.size();
    const std::size_t minLength = firstLength < secondLength ? firstLength : secondLength;

    // Ref first accepts a two-byte shift when the first argument is 2..4
    // bytes longer. The loop compares exactly minLength pairs.
    if (firstLength >= secondLength + 2 && firstLength <= secondLength + 4) {
        std::size_t matches = 0;
        for (std::size_t i = 0; i < minLength; ++i) {
            matches += equivalent(static_cast<unsigned char>(first[i + 2]),
                static_cast<unsigned char>(second[i]));
        }
        if (matches == minLength) return true;
    }

    // Then it accepts a one-byte shift when the first argument is 1..4 bytes
    // longer.
    if (firstLength >= secondLength + 1 && firstLength <= secondLength + 4) {
        std::size_t matches = 0;
        for (std::size_t i = 0; i < minLength; ++i) {
            matches += equivalent(static_cast<unsigned char>(first[i + 1]),
                static_cast<unsigned char>(second[i]));
        }
        if (matches == minLength) return true;
    }

    const std::size_t lengthDifference = firstLength > secondLength
        ? firstLength - secondLength
        : secondLength - firstLength;
    if (lengthDifference > 2) return false;

    std::size_t positionalMatches = 0;
    for (std::size_t i = 0; i < minLength; ++i) {
        positionalMatches += equivalent(static_cast<unsigned char>(first[i]),
            static_cast<unsigned char>(second[i]));
    }
    // The original compares signed minLength against minLength - 2. For
    // minLength 0, 1, or 2 this threshold is non-positive, so the test passes.
    const auto positionalThreshold = static_cast<std::ptrdiff_t>(minLength) - 2;
    if (static_cast<std::ptrdiff_t>(positionalMatches) >= positionalThreshold) return true;

    // The final source loop permits one skipped byte from the first argument
    // only. A mismatch consumes the first byte by advancing i while leaving j
    // in place; subsequent mismatches advance both indices. It succeeds only
    // with exactly minLength - 1 equivalent pairs.
    std::size_t secondIndex = 0;
    std::size_t alignedMatches = 0;
    bool skippedFirstByte = false;
    for (std::size_t firstIndex = 0; firstIndex < minLength; ++firstIndex) {
        if (equivalent(static_cast<unsigned char>(first[firstIndex]),
                static_cast<unsigned char>(second[secondIndex]))) {
            ++alignedMatches;
            ++secondIndex;
        } else if (!skippedFirstByte) {
            skippedFirstByte = true;
        } else {
            ++secondIndex;
        }
    }
    return alignedMatches == minLength - 1;
}

} // namespace ro::chat
