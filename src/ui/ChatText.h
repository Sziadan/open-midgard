#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace ro::chat {
// UI and chat packet payloads use UTF-8. Never truncate inside a code point.
inline std::string CleanInput(std::string_view text,std::size_t maxCharacters,std::size_t maxBytes=4096)
{
    std::string result;std::size_t characters=0;
    for(std::size_t i=0;i<text.size() && characters<maxCharacters;) {
        const unsigned char first=text[i];
        std::size_t count=first<0x80?1:first>=0xC2 && first<=0xDF?2:first>=0xE0 && first<=0xEF?3:first>=0xF0 && first<=0xF4?4:0;
        if(!count || i+count>text.size())break;
        unsigned value=first & (count==1?0x7F:count==2?0x1F:count==3?0x0F:0x07);
        bool valid=true;
        for(std::size_t k=1;k<count;++k) {const unsigned char next=text[i+k];if((next&0xC0)!=0x80)valid=false;value=(value<<6)|(next&0x3F);}
        if(!valid || (count==2 && value<0x80) || (count==3 && value<0x800) || (count==4 && value<0x10000)
            || value>0x10FFFF || (value>=0xD800 && value<=0xDFFF))break;
        if(value=='\r' || value=='\n' || value=='\t') {
            if(result.size()+1>maxBytes)break;
            result+=' ';++characters;
        } else if(value>=0x20 && value!=0x7F && !(value>=0x80 && value<=0x9F)) {
            if(result.size()+count>maxBytes)break;
            result.append(text.substr(i,count));++characters;
        }
        i+=count;
    }
    return result;
}
inline void EraseLastCharacter(std::string& text)
{
    if(text.empty())return;
    std::size_t start=text.size()-1;
    while(start>0 && (static_cast<unsigned char>(text[start])&0xC0)==0x80)--start;
    text.resize(start);
}
}
