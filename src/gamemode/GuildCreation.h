#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <string>

namespace ro::guild {
enum class CommandResult { NotCommand, Invalid, Valid };

inline CommandResult ParseCreateCommand(const std::string& text,std::string& name)
{
    name.clear();
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos)return CommandResult::NotCommand;
    const auto end=text.find_first_of(" \t\r\n",first);
    std::string command=text.substr(first,end==std::string::npos?end:end-first);
    for(char& ch:command)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if(command!="/guild")return CommandResult::NotCommand;
    if(end==std::string::npos)return CommandResult::Invalid;
    const auto begin=text.find_first_not_of(" \t\r\n",end);
    const auto last=text.find_last_not_of(" \t\r\n");
    if(begin==std::string::npos)return CommandResult::Invalid;
    std::string argument=text.substr(begin,last-begin+1);
    if(argument.front()=='"') {
        if(argument.size()<2 || argument.find('"',1)!=argument.size()-1)return CommandResult::Invalid;
        argument=argument.substr(1,argument.size()-2);
    } else if(argument.find_first_of(" \t\r\n\"")!=std::string::npos)return CommandResult::Invalid;
    const auto nameFirst=argument.find_first_not_of(" \t\r\n");
    if(nameFirst==std::string::npos)return CommandResult::Invalid;
    argument=argument.substr(nameFirst,argument.find_last_not_of(" \t\r\n")-nameFirst+1);
    // Ref rejects length >= GUILD_NAME_LENGTH; rAthena reserves byte 23 for NUL.
    if(argument.size()>=24)return CommandResult::Invalid;
    for(unsigned char ch:argument)if(ch<32 || ch==127)return CommandResult::Invalid;
    name=argument;
    return CommandResult::Valid;
}

inline std::array<std::uint8_t,30> CreatePacket(std::uint32_t playerId,const std::string& name)
{
    std::array<std::uint8_t,30> packet{};
    packet[0]=0x65;packet[1]=0x01;
    for(int i=0;i<4;++i)packet[2+i]=static_cast<std::uint8_t>(playerId>>(i*8));
    if(name.size()<24)for(std::size_t i=0;i<name.size();++i)packet[6+i]=static_cast<std::uint8_t>(name[i]);
    return packet;
}

inline const char* CreateResultMessage(unsigned char result)
{
    switch(result) {
    case 0:return "Guild has been created.";
    case 1:return "You are already in a guild.";
    case 2:return "That guild name already exists.";
    case 3:return "You need the required item (Emperium) to create a guild.";
    default:return "Guild creation failed.";
    }
}
}
