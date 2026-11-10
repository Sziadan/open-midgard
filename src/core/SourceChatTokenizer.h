#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ro::chat {

enum class SourceChatEncoding { LegacyCp949, Utf8 };

inline constexpr bool SourceChatIsWhitespace(unsigned char ch)
{
    return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

// Pure reconstruction of CTokenizer::SetString's source-chat tokenization.
// Input follows the caller's C-string convention: bytes after the first NUL
// are ignored. Quotes begin a fresh token and emit it on close; an open quote
// is emitted at end of input. An unquoted // ends the line and drops the
// currently pending unquoted token.
inline std::vector<std::string> SourceChatTokens(std::string_view text)
{
    enum class Mode { Read, Skip, Quote };

    std::vector<std::string> tokens;
    std::string token;
    Mode mode = Mode::Read;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == 0) {
            break;
        }

        if (mode == Mode::Quote) {
            if (ch == '"') {
                tokens.push_back(token);
                token.clear();
                mode = Mode::Skip;
            } else {
                token.push_back(static_cast<char>(ch));
            }
            continue;
        }

        if (mode == Mode::Skip) {
            if (!SourceChatIsWhitespace(ch)) {
                mode = Mode::Read;
                --i; // CTokenizer reprocesses the first non-space byte.
            }
            continue;
        }

        if (ch == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            return tokens; // The source routine returns before emitting pending text.
        }
        if (ch == '"') {
            token.clear(); // Quotes reset the in-progress unquoted token.
            mode = Mode::Quote;
        } else if (SourceChatIsWhitespace(ch)) {
            if (i != 0) {
                tokens.push_back(token);
                token.clear();
            }
            mode = Mode::Skip;
        } else {
            token.push_back(static_cast<char>(ch));
        }
    }

    // SetString emits any unfinished read/quote buffer, but not a token after
    // trailing whitespace (Skip mode). Its loop index is nonzero in that case.
    if (mode != Mode::Skip && !text.empty()) {
        const auto nul = text.find('\0');
        const std::size_t visibleSize = nul == std::string_view::npos ? text.size() : nul;
        if (visibleSize != 0) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

} // namespace ro::chat
