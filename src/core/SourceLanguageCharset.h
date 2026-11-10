#pragma once
#include <cstdint>
#include <string>

namespace ro::chat {
// CLanguage::GetLanguageCharset4FE720 uses the Windows primary/sublanguage IDs.
inline unsigned SourceCharsetForLanguage(std::uint16_t language)
{
    switch(language&0x3ff){
    case 1:return 178;
    case 2:case 0x19:case 0x22:case 0x23:return 204;
    case 4:return (language>>10)==1?136:(language>>10)==2?134:161;
    case 5:case 0x0e:case 0x15:case 0x18:case 0x1a:case 0x1b:case 0x24:case 0x2f:return 238;
    case 8:return 161;
    case 0x0d:return 177;
    case 0x11:return 128;
    case 0x12:return 129;
    case 0x1e:return 222;
    case 0x1f:return 162;
    case 0x25:case 0x26:case 0x27:return 186;
    case 0x2a:return 163;
    default:return 0;
    }
}
inline std::string SourceCharsetPrefix(bool enabled,std::uint16_t language)
{
    if(!enabled)return {};
    constexpr char hex[]="0123456789abcdef";
    const auto charset=SourceCharsetForLanguage(language);
    return std::string{'|',hex[charset>>4],hex[charset&15]};
}
}
