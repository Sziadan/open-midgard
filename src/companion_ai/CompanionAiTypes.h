#pragma once

#include "CompanionAiConfig.h"
#include "Types.h"

#include <cstring>

namespace companion_ai {

enum class Kind : int {
    Homunculus = 0,
    Mercenary = 1,
};

struct Msg2Ai {
    int command = NONE_CMD;
    int x = 0;
    int y = 0;
    int skillLevel = 0;
    int skillId = 0;
    u32 targetId = 0;

    void Clear()
    {
        std::memset(this, 0, sizeof(*this));
        command = NONE_CMD;
    }
};

} // namespace companion_ai
