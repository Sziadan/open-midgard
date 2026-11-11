#include "Session.h"

#include "core/File.h"
#include "core/SourceTextCodec.h"

#include <cstdlib>
#include <memory>
#include <vector>

namespace {

const char* PetTalkKey(int job)
{
    // Ref CSession::GetPetName uses explicit table keys and defaults to Poring.
    constexpr struct { int job; const char* key; } keys[] = {
        {1011, "chonchon"}, {1014, "spore"}, {1019, "pecopeco"}, {1023, "ork_warrior"},
        {1026, "munak"}, {1029, "isis"}, {1031, "poporing"}, {1035, "hunter_fly"},
        {1042, "steel_chonchon"}, {1049, "picky"}, {1052, "rocker"}, {1056, "smokie"},
        {1057, "yoyo"}, {1063, "lunatic"}, {1077, "poison_spore"}, {1101, "baphomet_j"},
        {1107, "desert_wolf_babe"}, {1109, "deviruchi"}, {1110, "dokebi"}, {1113, "drops"},
        {1122, "goblin_1"}, {1123, "goblin_2"}, {1125, "goblin_4"}, {1155, "petit"},
        {1167, "savage_babe"}, {1170, "sohee"}, {1188, "bon_gun"}, {1200, "zherlthsh"},
        {1208, "wander_man"}, {1245, "gobline_xmas"}, {1275, "alice"}, {1382, "diabolic"},
        {1385, "deleter_"}, {1519, "chung_e"}, {1815, "event_ricecake"}, {1879, "eclipse_p"},
        {1963, "p_chung_e"},
    };
    for (const auto& entry : keys) {
        if (entry.job == job) return entry.key;
    }
    return "poring";
}

} // namespace

std::string CSession::GetPetSpeech(int job, int hunger, int action)
{
    if (!m_petTalkLoaded) {
        m_petTalkLoaded = true;
        int size = 0;
        std::unique_ptr<unsigned char[]> bytes(g_fileMgr.GetData("PetTalkTable.xml", &size));
        if (!bytes) bytes.reset(g_fileMgr.GetData("data\\pettalktable.xml", &size));
        if (bytes && size > 0) {
            const auto* data = reinterpret_cast<const char*>(bytes.get());
            m_petTalk.ReadDocument(data, data + size);
        }
    }

    std::string rootKey = "monster_talk_table";
    ro::DecodeSourceResourceText(rootKey.data());
    auto* root = m_petTalk.m_root.FindChild(rootKey.c_str());
    const bool encoded = root && rootKey != "monster_talk_table";
    if (!root) root = m_petTalk.m_root.FindChild("monster_talk_table");
    if (!root) return {};

    const auto key = [encoded](const char* text) {
        std::string value(text);
        if (encoded) ro::DecodeSourceResourceText(value.data());
        return value;
    };
    constexpr const char* hungerKeys[] = {"hungry", "bit_hungry", "noting", "full", "so_full"};
    constexpr const char* actionKeys[] = {"feeding", "hunting", "danger", "dead", "stand", "perfor_s",
        "levelup", "perfor_1", "perfor_2", "perfor_3", "connect"};
    auto* pet = root->FindChild(key(PetTalkKey(job)).c_str());
    auto* mood = pet ? pet->FindChild(key(hungerKeys[hunger >= 0 && hunger < 5 ? hunger : 0]).c_str()) : nullptr;
    if (!mood) return {};
    const auto actionKey = key(action >= 0 && action < 11 ? actionKeys[action] : "normal");
    auto* speech = mood->FindChild(actionKey.c_str());
    if (!speech) return {};

    std::vector<XMLElement*> lines;
    for (auto* line = speech; line; line = line->FindNext(actionKey.c_str())) {
        lines.push_back(line);
    }
    std::string text = lines[static_cast<size_t>(std::rand()) % lines.size()]->GetContents();
    if (encoded) ro::DecodeSourceResourceText(text.data());
    if (!text.empty() && text.front() == '\t') text.erase(0, 1);
    if (!text.empty() && text.back() == '\t') text.pop_back();
    return text;
}
