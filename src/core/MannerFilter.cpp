#include "MannerFilter.h"
#include "File.h"
#include "Globals.h"
#include "SourceTextCodec.h"
#include <algorithm>

namespace ro {
namespace {
std::size_t NextUtf8(std::string_view bytes,std::size_t offset)
{
    if(offset>=bytes.size())return bytes.size();
    const auto c=static_cast<unsigned char>(bytes[offset]);
    const std::size_t count=c<0x80?1:c>=0xc2 && c<=0xdf?2:c>=0xe0 && c<=0xef?3:c>=0xf0 && c<=0xf4?4:1;
    if(offset+count>bytes.size())return offset+1;
    for(std::size_t i=1;i<count;++i)if((static_cast<unsigned char>(bytes[offset+i])&0xc0)!=0x80)return offset+1;
    return offset+count;
}
}
std::vector<std::string> TokenizeMannerFile(std::string_view bytes)
{
    std::vector<std::string> tokens;std::size_t start=0;
    const auto emit=[&](std::size_t end){
        auto token=bytes.substr(start,end-start);const auto nul=token.find('\0');
        if(nul!=std::string_view::npos)token=token.substr(0,nul);
        if(token.substr(0,2)!="//")tokens.emplace_back(token);
    };
    for(std::size_t i=0;i<bytes.size();++i){
        if(bytes[i]=='\n')start=i+1;
        else if(bytes[i]=='\r'){if(i>start && bytes[start]!='\0')emit(i);start=i+1;}
        else if(bytes[i]=='#'){
            // Hashes inside an exact leading comment do not advance its start.
            if(bytes.substr(start,2)=="//")continue;
            emit(i);start=i+1;
        }
    }
    return tokens;
}
std::size_t NextMannerCharacter(std::string_view bytes,std::size_t offset,int codePage)
{
    if(offset>=bytes.size())return bytes.size();
    if(codePage==65001)return NextUtf8(bytes,offset); // Explicit UTF-8 port input.
#if RO_PLATFORM_WINDOWS
    const auto* current=bytes.data()+offset;
    const auto* next=CharNextExA(static_cast<WORD>(codePage),current,0);
    return (std::min)(bytes.size(),offset+static_cast<std::size_t>((std::max)(std::ptrdiff_t(1),next-current)));
#else
    const auto c=static_cast<unsigned char>(bytes[offset]);bool lead=false;
    if(codePage==932)lead=(c>=0x81 && c<=0x9f) || (c>=0xe0 && c<=0xfc);
    else if(codePage==936 || codePage==949 || codePage==950)lead=c>=0x81 && c<=0xfe;
    return offset+((lead && offset+1<bytes.size() && bytes[offset+1]!='\0')?2:1);
#endif
}
void MannerFilter::AppendEncodedFile(std::string_view bytes,int codePage)
{
    for(const auto& token:TokenizeMannerFile(bytes)){
        m_encoded.AppendToken(token,[codePage](auto text,auto at){return NextMannerCharacter(text,at,codePage);});
        if (auto utf8 = DecodeSourceText(token, codePage)) m_utf8.AppendToken(*utf8, NextUtf8);
        else ++m_undecodable;
    }
}
const char* MannerLanguageFolder(unsigned primaryLanguage)
{
    switch(primaryLanguage & 0x3ffu){
    case 7:return "german\\";case 0x10:return "italian\\";
    case 0x16:return "portuguese\\";case 0x1f:return "turkish\\";
    default:return "english\\";
    }
}
bool MannerFilter::Load(const char* path)
{
    m_attempted=true;
    if(!path || !*path)return false;
    std::string candidate(path);
    // LanguageFile chooses an existing OS-language folder before the bare name.
    // Korea bypasses LanguageFile. Non-Windows has no source LANGID: use English.
    if(g_serviceType!=ServiceKorea){
        unsigned language=0;
#if RO_PLATFORM_WINDOWS
        language=GetUserDefaultLangID();
#endif
        const std::string localized=std::string(MannerLanguageFolder(language))+path;
        if(g_fileMgr.IsExist(localized.c_str()))candidate=localized;
    }
    int size=0;auto* bytes=g_fileMgr.GetData(candidate.c_str(),&size);
    if(bytes && size>=0){AppendEncodedFile(std::string_view(reinterpret_cast<const char*>(bytes),static_cast<std::size_t>(size)),SelectedSourceTextCodePage());delete[] bytes;m_loaded=true;return true;}
    delete[] bytes;
    return false; // Failed loads preserve all previously accepted words.
}
bool MannerFilter::EnsureLoaded(){if(!m_attempted)Load();return m_loaded;}
void MannerFilter::Clear(){m_encoded.Clear();m_utf8.Clear();m_attempted=m_loaded=false;m_undecodable=0;}
bool MannerFilter::IsBadEncodedSentence(std::string_view bytes,int codePage) const
{
    return m_encoded.IsBadSentence(bytes,[codePage](auto text,auto at){return NextMannerCharacter(text,at,codePage);});
}
bool MannerFilter::IsBadUtf8Sentence(std::string_view text) const{return m_utf8.IsBadSentence(text,NextUtf8);}
MannerFilter& GetMannerFilter(){static MannerFilter filter;return filter;}
}
