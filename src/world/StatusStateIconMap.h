#pragma once

struct StatusStateIconEntry {
    int priority;
    int status;
    const char* standardPath;
    const char* specialPath;
};

#include "StatusStateIconMap.generated.inc"

// Canonical HighPriest.exe CSession::GetStateFileName pairs. Jobs 4046-4049 use
// the special path when the executable provides one (Aspersio / weapon property).
static inline bool StatusStateIconUsesSpecialJob(int job)
{
    return job >= 4046 && job <= 4049;
}

static inline const StatusStateIconEntry* FindStatusStateIcon(int priority, int status)
{
    for (const StatusStateIconEntry& entry : kStatusStateIconEntries) {
        if (entry.priority == priority && entry.status == status) {
            return &entry;
        }
    }
    return nullptr;
}

static inline const StatusStateIconEntry* FindStatusStateIconByStatus(int status)
{
    for (const StatusStateIconEntry& entry : kStatusStateIconEntries) {
        if (entry.status == status) {
            return &entry;
        }
    }
    return nullptr;
}

static inline const char* StatusStateIconFilePath(const StatusStateIconEntry* entry, int job)
{
    if (!entry) {
        return nullptr;
    }
    if (StatusStateIconUsesSpecialJob(job) && entry->specialPath && entry->specialPath[0]) {
        return entry->specialPath;
    }
    return entry->standardPath;
}
