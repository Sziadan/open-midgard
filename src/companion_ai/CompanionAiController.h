#pragma once

#include "CompanionAiBrain.h"

class CGameActor;

namespace companion_ai {

// Owns one companion AI instance (homunculus or mercenary). Mirrors Ref CMercenaryAI.
class Controller {
public:
    explicit Controller(Kind kind);
    ~Controller() = default;

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    void Open();
    void Proc(u32 gid);
    bool IsOpened() const { return m_opened; }
    Kind kind() const { return m_kind; }

private:
    Kind m_kind;
    bool m_opened = false;
    Brain m_brain;
};

// Called once per companion actor ProcessState tick.
void TickOwnedCompanionActor(CGameActor& actor);

} // namespace companion_ai
