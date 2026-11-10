#pragma once

namespace ro::chat {

struct CommandListEntry {
    const char* caption;
    int value;
};

#include "SourceCommandListTables.generated.inc"

} // namespace ro::chat
