#include "TipOfTheDay.h"
#include "File.h"
#include "Globals.h"
#include "SettingsIni.h"
#include <cstdio>
#include <fstream>
#include <utility>

CTipOfTheDay g_tipOfTheDay;
int g_showTipsAtStartup=1;

std::string SourceTipLanguageFolder(u16 userLanguageId)
{
    // CLanguage4FE220 selects resources by the OS USER language, independently
    // of service type and the keyboard/input LANGID used by chat charset tags.
    switch(userLanguageId&0x3ff){
    case 7:return "german\\";
    case 16:return "italian\\";
    case 22:return "portuguese\\";
    case 31:return "turkish\\";
    default:return "english\\";
    }
}

bool CTipOfTheDay::Load()
{
#if RO_PLATFORM_WINDOWS
    return Load(GetUserDefaultLangID());
#else
    return Load(9);
#endif
}

bool CTipOfTheDay::Load(u16 userLanguageId)
{
    m_bTryLoading=true;
    if(g_serviceType!=ServiceKorea){
        // LanguageFile falls back to the original name if its folder candidate
        // does not exist. An existing but unreadable selection must not trigger
        // a second, unrelated service fallback.
        std::string path=SourceTipLanguageFolder(userLanguageId)+"tipofday.txt";
        if(!g_fileMgr.IsDataExist(path.c_str()))path="tipofday.txt";
        if(g_fileMgr.IsDataExist(path.c_str())){
            CFile file;if(!file.Open(path.c_str()))return false;
            const int size=file.Size();if(size<0)return false;
            std::string bytes(static_cast<std::size_t>(size),'\0');
            if(size && !file.Read(bytes.data(),static_cast<u32>(size)))return false;
            AppendDelimited(bytes);return true;
        }
    }
    const char* path=g_serviceType==ServiceAmerica?"tipOfTheday_e.txt"
        :g_serviceType==ServiceJapan?"tipOfTheday_j.txt":"tipOfTheday.txt";
    auto* file=std::fopen(path,"r");if(!file)return false;
    if(std::fseek(file,0,SEEK_END)!=0){std::fclose(file);return false;}
    const long size=std::ftell(file);
    if(size<0 || std::fseek(file,0,SEEK_SET)!=0){std::fclose(file);return false;}
    std::string bytes(static_cast<std::size_t>(size),'\0');
    const auto read=std::fread(bytes.data(),1,bytes.size(),file);
    const bool failed=std::ferror(file)!=0;std::fclose(file);if(failed)return false;
    // Windows CRT text mode translates CRLF and may stop at Ctrl-Z. The source
    // iterates the physical size over unread allocation tail; only bytes actually
    // read have defined content. Bound that unsafe tail instead of fabricating it.
    bytes.resize(read);AppendDelimited(bytes);return true;
}

void CTipOfTheDay::AppendDelimited(const std::string& bytes)
{
    Tip lines;std::size_t start=0;
    for(std::size_t i=0;i<bytes.size();++i){
        if(bytes[i]!='\n' && bytes[i]!='#')continue;
        auto line=bytes.substr(start,i-start);
        const auto nul=line.find('\0');if(nul!=std::string::npos)line.resize(nul);
        lines.push_back(std::move(line));start=i+1;
        if(bytes[i]=='#'){m_tips.push_back(std::move(lines));lines.clear();}
    }
    // Neither an unterminated final line nor LF-only temporary lines are tips.
    // Explicit repeated Load appends, as in the source; lazy getters try once.
}

std::size_t CTipOfTheDay::GetNumTip()
{
    if(!m_bTryLoading)Load();return m_tips.size();
}

const CTipOfTheDay::Tip& CTipOfTheDay::GetTip(int index)
{
    if(!m_bTryLoading)Load();
    if(index>=0 && static_cast<std::size_t>(index)<m_tips.size())return m_tips[static_cast<std::size_t>(index)];
    if(!m_tips.empty())return m_tips.front();
    static const Tip empty;return empty; // Original empty-first-entry dereference is undefined.
}

void LoadSourceTipOptions()
{
    // Port ReadRegistry uses its existing INI store in place of HKLM. Preserve
    // selected68B7EB: a successfully opened settings store forces the flag OFF
    // after the source query, even when the persisted checkbox value is ON.
    std::ifstream store(GetOpenMidgardIniPath());
    if(store)g_showTipsAtStartup=0;
}

bool SaveSourceTipOptions(int value)
{
    g_showTipsAtStartup=value;
    return SaveSettingsIniInt("Option","SHOWTIPSATSTARTUP",value);
}
