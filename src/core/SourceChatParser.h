#pragma once
#include "SourceChatTokenizer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace ro::chat {
enum class SourceParameterKind { Tail, Volume, RawParam, Join, One, Map, Bare, Three, MinEffect, Shift, Whole, Stat, Hi };
struct SourceParameterRule {
    std::string_view utf8, legacy;
    SourceParameterKind kind;
    int type, value;
};
#include "SourceChatParameterRules.generated.inc"

struct SourceChatParseContext {
    int soundVolume = 0, musicVolume = 0;
    std::optional<std::string> minEffectAlias;
    SourceChatEncoding encoding=SourceChatEncoding::Utf8;
};
struct SourceChatParseResult {
    int type = 3;
    std::ptrdiff_t messageOffset = -1;
    std::array<std::string,3> parameters;
    bool toggleMinEffect = false;
    int statIndex = -1;
    std::uint8_t statValue = 0;
    std::optional<std::string> messageOverride;
    // Source /shift can overrun the caller's three string objects. Retain the
    // first three arguments safely and expose the rejected overflow boundary.
    bool parameterOverflow = false;
};
inline std::string SourceChatLower(std::string_view value,SourceChatEncoding encoding=SourceChatEncoding::Utf8)
{
    std::string result(value);
    for(std::size_t i=0;i<result.size();++i){
        const auto byte=static_cast<unsigned char>(result[i]);
        // MakeStringLower526E40 skips a signed nonpositive byte AND the
        // following byte. Preserve CP949 trail bytes, including ASCII letters.
        if(encoding==SourceChatEncoding::LegacyCp949 && (byte==0 || byte>=0x80)){++i;continue;}
        auto& ch=result[i];if(ch>='A' && ch<='Z')ch=static_cast<char>(ch+('a'-'A'));
    }
    return result;
}
inline std::int32_t SourceChatAtoi(std::string_view value)
{
    std::size_t i=0;
    while(i<value.size() && SourceChatIsWhitespace(static_cast<unsigned char>(value[i])))++i;
    const bool negative=i<value.size() && value[i]=='-';
    if(i<value.size() && (value[i]=='-' || value[i]=='+'))++i;
    std::uint32_t bits=0;
    while(i<value.size() && value[i]>='0' && value[i]<='9')bits=bits*10u+static_cast<unsigned>(value[i++]-'0');
    if(negative)bits=0u-bits;
    // Selected statically-linked atoi6A8897 ->6A880C accumulates with32-bit
    // LEAs and negates modulo2^32, without a strtol saturation branch.
    return static_cast<std::int32_t>(static_cast<std::int64_t>(bits)
        -(bits>INT32_MAX?0x100000000ll:0));
}

// GetTalkType6621D0: parses original bytes, preserving the returned message
// offset separately from tokenized arguments. UTF-8 aliases adapt only the
// literal prefix's CP949 byte count; the legacy-byte form retains raw offsets.
// Lookup is the selected, initialized no-parameter table, not a command whitelist.
template<class Lookup>
SourceChatParseResult ParseSourceChat(std::string_view input,const SourceChatParseContext& context,Lookup lookup)
{
    const auto nul=input.find('\0');
    if(nul!=std::string_view::npos)input=input.substr(0,nul);
    SourceChatParseResult result;
    auto tokens=SourceChatTokens(input);
    if(tokens.empty())return result;
    if(input.empty() || input.front()!='/') { result.type=0;result.messageOffset=0;return result; }
    tokens[0]=SourceChatLower(tokens[0],context.encoding);
    const auto joined=[&](){std::string text;for(std::size_t i=1;i<tokens.size();++i){text+=tokens[i];text+=' ';}return text;};
    const auto offset=[&](const SourceParameterRule& rule){
        auto value=static_cast<std::ptrdiff_t>(rule.value);
        // Preserve odd fixed offsets for token-reset/quoted forms. Adapt a
        // genuine UTF-8 literal prefix without changing its argument bytes.
        if(context.encoding==SourceChatEncoding::Utf8 && rule.utf8!=rule.legacy && input.size()>=rule.utf8.size()
            && SourceChatLower(input.substr(0,rule.utf8.size()))==rule.utf8)
            value+=static_cast<std::ptrdiff_t>(rule.utf8.size())-static_cast<std::ptrdiff_t>(rule.legacy.size());
        return (std::min)(value,static_cast<std::ptrdiff_t>(input.size()));
    };
    for(const auto& rule:kSourceParameterRules){
        if(tokens[0]!=(context.encoding==SourceChatEncoding::Utf8?rule.utf8:rule.legacy)
            && !(rule.kind==SourceParameterKind::MinEffect && context.minEffectAlias && tokens[0]==*context.minEffectAlias))continue;
        const auto count=tokens.size();
        switch(rule.kind){
        case SourceParameterKind::Tail:
            result.type=rule.type;result.messageOffset=offset(rule);
            while(static_cast<std::size_t>(result.messageOffset)<input.size() && input[result.messageOffset]==' ')++result.messageOffset;
            return result;
        case SourceParameterKind::Volume:
            result.type=rule.type;result.parameters[0]=count>=2?tokens[1]:std::to_string(rule.type==12?context.soundVolume:context.musicVolume);return result;
        case SourceParameterKind::RawParam:
            if(count<2)break;
            result.type=rule.type;result.parameters[0]=input.substr(static_cast<std::size_t>(offset(rule)));return result;
        case SourceParameterKind::Join:
            result.type=rule.type;result.parameters[0]=joined();return result;
        case SourceParameterKind::One:
            if(count!=2)break;
            result.type=rule.type;result.parameters[0]=tokens[1];return result;
        case SourceParameterKind::Map:
            if(count<2)break;
            result.type=rule.type;result.parameters[0]=tokens[1];
            result.parameters[1]=count==4?tokens[2]:"0";result.parameters[2]=count==4?tokens[3]:"0";return result;
        case SourceParameterKind::Bare:
            result.type=rule.type;return result;
        case SourceParameterKind::Three:
            if(count!=4)break;
            result.type=rule.type;std::copy_n(tokens.begin()+1,3,result.parameters.begin());return result;
        case SourceParameterKind::MinEffect:
            result.type=rule.type;result.messageOffset=0;result.toggleMinEffect=true;return result;
        case SourceParameterKind::Shift:
            result.type=rule.type;result.parameterOverflow=count>4;
            for(std::size_t i=1;i<count && i<=3;++i)result.parameters[i-1]=tokens[i];return result;
        case SourceParameterKind::Whole:
            result.type=rule.type;result.messageOffset=0;return result;
        case SourceParameterKind::Stat:
            if(count<2)break;
            result.type=rule.type;result.messageOffset=0;result.statIndex=rule.value;
            result.statValue=static_cast<std::uint8_t>(SourceChatAtoi(tokens[1]));return result;
        case SourceParameterKind::Hi:
            result.type=rule.type;
            if(count>1){result.parameters[0]=tokens[1];result.messageOverride=joined();}return result;
        }
        // A failed parameter-count check falls through the rest of the source
        // parser and finally reaches the no-parameter table (not normal chat).
        continue;
    }
    result.type=lookup(tokens[0]);return result;
}
} // namespace ro::chat
