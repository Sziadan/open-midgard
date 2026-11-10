#pragma once
#include <string>
namespace ro::chat {
struct SourceChatModifiers { bool control=false,alt=false; };
// ProcessTalkType578F10, selected kinds0/50. The prefix mutation precedes
// battle/channel/modifier routing, including the source room distinctions.
inline int SourceChatRoute(int kind,std::string& text,int talkMode,bool battle,bool party,bool guild,SourceChatModifiers keys)
{
    bool partyPrefix=false,guildPrefix=false;
    if(!text.empty() && (text[0]=='%' || text[0]=='$')){
        if(text.size()==1)return 0;
        partyPrefix=text[0]=='%';guildPrefix=text[0]=='$';text.erase(0,1);
    }
    const bool partyRequested=keys.control || partyPrefix;
    const bool guildRequested=keys.alt || guildPrefix;
    if(kind!=50){
        if(battle)return 271;
        if(talkMode==2 && guild)return guildRequested?6:142;
        if(talkMode==1)return !partyRequested && party?72:6;
        if(partyRequested && party)return 72;
        return guildRequested?142:6;
    }
    if(talkMode==2 && guild)return 142;
    return partyRequested && party?72:6;
}
} // namespace ro::chat
