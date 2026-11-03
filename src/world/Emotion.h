#pragma once
#include <charconv>
#include <string_view>

constexpr int EmotionAction(int id)
{
    constexpr int first[] = {0,1,2,3,5,6,7,8,9,10,12,11,13,14,4};
    if (id < 0 || id > 80) return -1;
    if (id < 15) return first[id];
    if (id == 34) return 10; // Chat prohibition uses motion 15 of silence.
    return id < 34 ? id : id - 1;
}

// -1: ordinary chat; -2: malformed explicit emotion command.
inline int ResolveEmotionCommand(std::string_view command)
{
    constexpr std::string_view names[] = {
        "!","?","ho","lv","swt","ic","an","ag","$","...","scissors","rock","paper","korea","lv2",
        "thx","wah","sry","heh","swt2","hmm","no1","??","omg","o","x","hlp","go","sob","gg",
        "kiss","kiss2","pif","ok","","indonesia","bzz","rice","awsm","meh","shy","pat","mp","slur",
        "com","yawn","con","hp","philippines","malaysia","singapore","brazil","fsh","spin","sigh","dum",
        "crwd","desp","dice1","dice2","dice3","dice4","dice5","dice6","india","love","russia","china",
        "mobile","mail","antenna0","antenna1","antenna2","antenna3","hum","abs","oops","spit","ene","panic","whisp"
    };
    static_assert(sizeof(names)/sizeof(names[0]) == 81);
    if (command.empty() || command.front() != '/') return -1;
    command.remove_prefix(1);
    if (command == "dice") return 58;
    for (int id = 0; id <= 80; ++id) if (!names[id].empty() && command == names[id]) return id;
    for (const std::string_view prefix : {std::string_view("emotion "),std::string_view("emote ")}) {
        if (command.substr(0,prefix.size()) != prefix) continue;
        command.remove_prefix(prefix.size());
        int id = -1;
        const auto result = std::from_chars(command.data(), command.data()+command.size(), id);
        return result.ec == std::errc{} && result.ptr == command.data()+command.size() && EmotionAction(id) >= 0 ? id : -2;
    }
    return command == "emotion" || command == "emote" ? -2 : -1;
}
