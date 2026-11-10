#pragma once

#include "SourceChatTokenizer.h"
#include <string>
#include <string_view>

namespace ro::chat {

// Reconstructs only the original source-chat command's first token. The
// original first byte must be '/'; tokenization then honors ASCII whitespace,
// quote reset/emit behavior, and an unquoted // comment that discards the
// current token. This is not a general chat or command parser.
inline std::string SourceChatCommandToken(std::string_view text)
{
    if(text.empty() || text.front()!='/')return {};
    auto tokens=SourceChatTokens(text);
    if(tokens.empty())return {};
    auto token=std::move(tokens.front());
    for(auto& ch:token)if(ch>='A' && ch<='Z')ch=static_cast<char>(ch+('a'-'A'));
    return token;
}

} // namespace ro::chat
