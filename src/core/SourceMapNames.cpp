#include "SourceMapNames.h"
#include "File.h"
#include "Globals.h"
#include "TipOfTheDay.h"
#include <algorithm>
#include <cstring>

namespace ro {
bool SourceMapNames::Load()
{
#if RO_PLATFORM_WINDOWS
    return Load(GetUserDefaultLangID());
#else
    return Load(9);
#endif
}
bool SourceMapNames::Load(u16 userLanguageId)
{
    m_rows.clear();
    std::string path="mapnametable.txt";
    if(g_serviceType!=ServiceKorea){
        const auto candidate=SourceTipLanguageFolder(userLanguageId)+path;
        // Source CFile::IsFileExist consults archives as well as the disk.
        if(g_fileMgr.IsDataExist(candidate.c_str()))path=candidate;
    }
    CFile file;if(!file.Open(path.c_str()))return false;
    const int size=file.Size();if(size<0)return false;
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if(size && !file.Read(bytes.data(),static_cast<u32>(size)))return false;
    Parse(bytes);return true;
}
void SourceMapNames::Parse(std::string_view input)
{
    m_rows.clear();std::string bytes(input);bytes.push_back('\0');
    std::vector<std::string> tokens;std::size_t start=0;
    for(std::size_t i=0;i<input.size();++i){
        if(bytes[i]=='\r'){bytes[i]='\0';start=i+1;}
        else if(bytes[i]=='\n')start=i+1;
        else if(bytes[i]=='#'){
            if(bytes[start]=='/' && start+1<input.size() && bytes[start+1]=='/')continue;
            bytes[i]='\0';tokens.emplace_back(bytes.c_str()+start);start=i+1;
        }
    }
    // Source consumes pairs without an odd-tail check. Suppress that undefined
    // read while retaining all complete pairs, including empty fields.
    for(std::size_t i=0;i+1<tokens.size();i+=2)m_rows.emplace_back(std::move(tokens[i]),std::move(tokens[i+1]));
    std::sort(m_rows.begin(),m_rows.end(),[](const auto& a,const auto& b){return std::strcmp(a.first.c_str(),b.first.c_str())<0;});
}
const std::string* SourceMapNames::Find(const char* rswName) const
{
    std::string key=rswName?rswName:"";
    const auto at=key.rfind('@');
    if(at!=std::string::npos && at>=4)key.erase(at-4,3);
    const auto row=std::lower_bound(m_rows.begin(),m_rows.end(),key,[](const auto& pair,const auto& value){return std::strcmp(pair.first.c_str(),value.c_str())<0;});
    return row!=m_rows.end() && std::strcmp(row->first.c_str(),key.c_str())==0?&row->second:nullptr;
}
}
