#pragma once
#include "SourceChatTokenizer.h"
#include <string_view>

namespace ro::chat {
struct SourceNoParamRule { std::string_view utf8,legacy;int messageId,type;bool ordinaryServerOnly; };
#include "SourceChatAliases.generated.inc"

// The source stable-sorts then lower_bounds strcmp. Original registration
// order chooses the first equal alias. Linear lookup preserves that rule,
// including duplicate/empty language labels, while avoiding borrowed pointers.
// ReadMessage returns optional<string>; a missing port resource is skipped.
template<class ReadMessage>
int LookupSourceTalkType(std::string_view command,ReadMessage readMessage,SourceChatEncoding encoding=SourceChatEncoding::Utf8,bool instantEvent=false)
{
    for(const auto& rule:kSourceNoParamRules){
        if(instantEvent && rule.ordinaryServerOnly)continue;
        if(rule.messageId>=0){const auto text=readMessage(rule.messageId);if(text && command==*text)return rule.type;}
        else if(command==(encoding==SourceChatEncoding::Utf8?rule.utf8:rule.legacy))return rule.type;
    }
    return 3;
}
} // namespace ro::chat
