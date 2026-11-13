#include "CompanionAiController.h"

#include "GameActor.h"
#include "session/Session.h"

namespace companion_ai {

Controller::Controller(Kind kind)
    : m_kind(kind)
    , m_brain(kind)
{
}

void Controller::Open()
{
    m_opened = true;
}

void Controller::Proc(u32 gid, u32 now)
{
    if (!m_opened || gid == 0) {
        return;
    }
    m_brain.Tick(gid, now);
}

void TickOwnedCompanionActor(CGameActor& actor, u32 now)
{
    if (actor.m_gid == 0) {
        return;
    }

    if (g_session.m_homunGID != 0 && actor.m_gid == g_session.m_homunGID && g_session.m_homunStatus.alive) {
        auto*& slot = actor.m_homunAI;
        if (!slot) {
            slot = new Controller(Kind::Homunculus);
        }
        if (!slot->IsOpened()) {
            slot->Open();
        }
        slot->Proc(actor.m_gid, now);
    }

    if (g_session.m_merGID != 0 && actor.m_gid == g_session.m_merGID && g_session.m_merStatus.alive) {
        auto*& slot = actor.m_merAI;
        if (!slot) {
            slot = new Controller(Kind::Mercenary);
        }
        if (!slot->IsOpened()) {
            slot->Open();
        }
        slot->Proc(actor.m_gid, now);
    }
}

} // namespace companion_ai
