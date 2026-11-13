#pragma once

#include "Types.h"

#include <array>
#include <map>
#include <optional>
#include <string>

namespace ro::quest {

enum class State : u8 {
    Inactive = 0,
    Active = 1,
};

struct Objective {
    u32 monsterId = 0;
    u16 currentCount = 0;
    std::string name;
    std::optional<u16> requiredCount;
};

struct Mission {
    std::optional<u32> startedAt;
    u32 expiresAt = 0;
    u16 objectiveCount = 0;
    std::array<Objective, 3> objectives;
};

struct Journal {
    // The server supplies membership and mission details in independent packets.
    // Only IDs in states are journal entries. Details can arrive before membership.
    std::map<u32, State> states;
    std::map<u32, Mission> missions;
};

} // namespace ro::quest
