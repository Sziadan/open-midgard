#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ro {

// Owns the tokens used by the byte-level insult filter. The caller supplies
// the encoding-aware step function used to find character boundaries.
class InsultWords
{
public:
    void Clear() noexcept { words_.clear(); }

    template <typename NextBoundary>
    void AppendToken(std::string_view token, NextBoundary nextBoundary)
    {
        token = BeforeNul(token);

        bool nonblank = false;
        for (const char byte : token) {
            if (byte != ' ' && byte != '\t') {
                nonblank = true;
                break;
            }
        }
        if (!nonblank)
            return;

        std::string owned(token);
        LowerAtBoundaries(owned, nextBoundary);
        words_.push_back(std::move(owned));
    }

    template <typename NextBoundary>
    bool IsBadSentence(std::string_view sentence, NextBoundary nextBoundary) const
    {
        sentence = BeforeNul(sentence);
        if (sentence.empty() || words_.empty())
            return false;

        std::string lowered(sentence);
        LowerAtBoundaries(lowered, nextBoundary);

        for (std::size_t offset = 0; offset < lowered.size();) {
            for (const std::string& word : words_) {
                if (word.size() <= lowered.size() - offset &&
                    lowered.compare(offset, word.size(), word) == 0) {
                    return true;
                }
            }

            const std::size_t next = nextBoundary(lowered, offset);
            if (next <= offset || next > lowered.size())
                break;
            offset = next;
        }
        return false;
    }

    const std::vector<std::string>& words() const noexcept { return words_; }

private:
    static std::string_view BeforeNul(std::string_view bytes) noexcept
    {
        const std::size_t nul = bytes.find('\0');
        return nul == std::string_view::npos ? bytes : bytes.substr(0, nul);
    }

    template <typename NextBoundary>
    static void LowerAtBoundaries(std::string& bytes, NextBoundary& nextBoundary)
    {
        const std::string_view view(bytes);
        for (std::size_t offset = 0; offset < bytes.size();) {
            const unsigned char byte = static_cast<unsigned char>(bytes[offset]);
            if (byte >= 'A' && byte <= 'Z')
                bytes[offset] = static_cast<char>(byte + ('a' - 'A'));

            const std::size_t next = nextBoundary(view, offset);
            if (next <= offset || next > bytes.size())
                break;
            offset = next;
        }
    }

    std::vector<std::string> words_;
};

} // namespace ro
