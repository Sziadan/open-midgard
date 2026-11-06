#include "UiMessageStrings.h"
#include "File.h"

bool ro::UiMessageStrings::Load()
{
    m_rows.clear();
    for (const char* path : {"msgstringtable.txt","data\\msgstringtable.txt","data/msgstringtable.txt"}) {
        int size=0;
        auto* bytes=g_fileMgr.GetData(path,&size);
        if (bytes && size>0) {
            const std::string text(reinterpret_cast<const char*>(bytes),static_cast<std::size_t>(size));
            delete[] bytes;
            Parse(text);
            return true;
        }
        delete[] bytes;
    }
    return false;
}
