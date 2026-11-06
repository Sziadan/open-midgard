#pragma once

#include <string>
#include <string_view>

namespace ro::chat {

// Reconstructs only the original source-chat command's first token. The
// original first byte must be '/'; tokenization then honors ASCII whitespace,
// quote reset/emit behavior, and an unquoted // comment that discards the
// current token. This is not a general chat or command parser.
inline std::string SourceChatCommandToken(std::string_view text)
{
    if (text.empty() || text.front() != '/') {
        return {};
    }

    std::string token;
    bool inQuote = false;
    for (const unsigned char ch : text) {
        if (ch == 0) {
            break;
        }

        if (inQuote) {
            if (ch == '"') {
                return token;
            }
            token.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch));
            continue;
        }

        if (ch == '/' && token.size() > 0 && token.back() == '/') {
            return {};
        }
        if (ch == '"') {
            token.clear();
            inQuote = true;
            continue;
        }
        if (ch == ' ' || (ch >= '\t' && ch <= '\r')) {
            return token;
        }

        token.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch));
    }

    return token;
}

} // namespace ro::chat
