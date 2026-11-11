// Ref CSession::GetMonsterSkillInfo — Ref/Session2.cpp:3259 (0x67ff10).
#include "Session.h"

#include "DebugLog.h"
#include "core/File.h"
#include "core/Globals.h"
#include "core/SourceTextCodec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void DecodeInto(char* dest, int destSize, const char* source)
{
    if (destSize <= 0) {
        return;
    }
    dest[0] = '\0';
    if (source == nullptr) {
        return;
    }
    std::strncpy(dest, source, static_cast<size_t>(destSize - 1));
    dest[destSize - 1] = '\0';
    ro::DecodeSourceResourceText(dest);
}

// Ref/Session.cpp:10663 for showLevel == 1. The showLevel == 7 Gravity-aid
// branch cannot apply here.
bool IsMonsterSkillMasterAid(unsigned int aid)
{
    if (g_serviceType != ServiceKorea) {
        return false;
    }
    if (aid > 0x2C8C83u) {
        return aid == 2919558u;
    }
    if (aid == 2919555u || aid == 234073u) {
        return true;
    }
    return false;
}

} // namespace

void CSession::GetMonsterSkillInfo(const char* monsterName, int skillId, int& act, int& loopType)
{
    act = -1;
    if (!m_monsterSkillInfoLoaded) {
        m_monsterSkillInfoLoaded = true;
        int size = 0;
        unsigned char* bytes = g_fileMgr.GetData("MonsterSkillInfo.xml", &size);
        if (bytes != nullptr && size > 0) {
            m_monsterSkillInfo.ReadDocument(reinterpret_cast<const char*>(bytes),
                reinterpret_cast<const char*>(bytes) + size);
            delete[] bytes;
        }
    }

    char key[256];
    DecodeInto(key, sizeof(key), "Monster_Action_Table");
    XMLElement* table = m_monsterSkillInfo.m_root.FindChild(key);
    if (table == nullptr || monsterName == nullptr) {
        return;
    }

    char skillKey[16];
    std::snprintf(skillKey, sizeof(skillKey), "_%d", skillId);
    ro::DecodeSourceResourceText(skillKey);

    // Ref copies the name, lowercases it, then DataTxtDecode.
    char nameKey[256];
    nameKey[0] = '\0';
    std::strncpy(nameKey, monsterName, sizeof(nameKey) - 1);
    nameKey[sizeof(nameKey) - 1] = '\0';
    for (char* cursor = nameKey; *cursor != '\0'; ++cursor) {
        const unsigned char value = static_cast<unsigned char>(*cursor);
        if (value >= 'A' && value <= 'Z') {
            *cursor = static_cast<char>(value - 'A' + 'a');
        }
    }
    ro::DecodeSourceResourceText(nameKey);

    XMLElement* monster = table->FindChild(nameKey);
    if (monster == nullptr) {
        return;
    }
    XMLElement* skill = monster->FindChild(skillKey);
    if (skill == nullptr) {
        return;
    }

    char actionKey[32];
    char loopKey[32];
    DecodeInto(actionKey, sizeof(actionKey), "action");
    DecodeInto(loopKey, sizeof(loopKey), "loopType");
    XMLElement* actionElement = skill->FindChild(actionKey);
    XMLElement* loopElement = skill->FindChild(loopKey);
    if (IsMonsterSkillMasterAid(m_aid) && (actionElement == nullptr || loopElement == nullptr)) {
        act = 0;
        loopType = 0;
        DbgLog("[Skill] %s\n",
            "\xBF\xA1\xB7\xAF\xB9\xDF\xBB\xFD. \xC5\xAC\xB6\xF3\xC0\xCC\xBE\xF0\xC6\xAE \xB4\xE3\xB4\xE7\xC0\xDA \xBF\xA1\xB0\xD4 \xBF\xA1\xB7\xAF \xBB\xE7\xBD\xC7\xC0\xBB \xC5\xEB\xBA\xB8 \xC7\xCF\xBC\xBC\xBF\xE4. \xC0\xA7\xC4\xA1 - CPlayer :: GetMonsterSkillInfo");
        DbgLog("[Skill] %s\n",
            "action \xB6\xC7\xB4\xC2 loopType \xC0\xCC \xC1\xB8\xC0\xE7\xC7\xCF\xC1\xF6 \xBE\xCA\xC0\xBD.");
        return;
    }
    if (actionElement == nullptr || loopElement == nullptr) {
        return;
    }

    char actionText[64];
    char loopText[64];
    DecodeInto(actionText, sizeof(actionText), actionElement->GetContents().c_str());
    DecodeInto(loopText, sizeof(loopText), loopElement->GetContents().c_str());
    act = std::atoi(actionText);
    loopType = std::atoi(loopText);
}
