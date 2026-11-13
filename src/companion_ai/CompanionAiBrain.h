#pragma once

#include "CompanionAiRuntime.h"

#include <deque>
#include <optional>

namespace companion_ai {

// Direct port of OldRO AI.lua / AI_M.lua. Homunculus vs mercenary only differ in
// GetMyEnemy type selection. Edit CompanionAiConfig.h for distances / caps;
// edit the On*_ST methods here for behavior changes.
class Brain {
public:
    explicit Brain(Kind kind);

    void Tick(u32 myId, u32 now);

    Kind kind() const { return m_runtime.kind(); }
    int state() const { return m_state; }

private:
    void ProcessCommand(const Msg2Ai& msg);
    bool ContinueApproach(const Position& destination);
    void CancelApproach();
    void AbandonChase();
    bool CanAcquireEnemy(u32 id) const;
    u32 MovementStallDelay() const;
    void RequestFollow(const Position& owner);

    void OnMOVE_CMD(int x, int y);
    void OnSTOP_CMD();
    void OnATTACK_OBJECT_CMD(u32 id);
    void OnATTACK_AREA_CMD(int x, int y);
    void OnPATROL_CMD(int x, int y);
    void OnHOLD_CMD();
    void OnSKILL_OBJECT_CMD(int level, int skill, u32 id);
    void OnSKILL_AREA_CMD(int level, int skill, int x, int y);
    void OnFOLLOW_CMD();

    void OnIDLE_ST();
    void OnFOLLOW_ST();
    void OnCHASE_ST();
    void OnATTACK_ST();
    void OnMOVE_CMD_ST();
    void OnSTOP_CMD_ST();
    void OnATTACK_OBJECT_CMD_ST();
    void OnATTACK_AREA_CMD_ST();
    void OnPATROL_CMD_ST();
    void OnHOLD_CMD_ST();
    void OnSKILL_OBJECT_CMD_ST();
    void OnSKILL_AREA_CMD_ST();
    void OnFOLLOW_CMD_ST();

    u32 GetOwnerEnemy(u32 myid);
    u32 GetMyEnemy(u32 myid);
    u32 GetMyEnemyA(u32 myid);
    u32 GetMyEnemyB(u32 myid);
    bool IsPassiveHomunType(int type) const;
    bool IsAggressiveHomunType(int type) const;

    Runtime m_runtime;
    int m_state = IDLE_ST;
    u32 m_enemy = 0;
    u32 m_stalledEnemy = 0;
    u32 m_chaseStalledAt = 0;
    int m_destX = 0;
    int m_destY = 0;
    Position m_moveDestination;
    Position m_followDestination;
    std::optional<u32> m_lastFollowRequest;
    int m_patrolX = 0;
    int m_patrolY = 0;
    u32 m_myId = 0;
    int m_skill = 0;
    int m_skillLevel = 0;
    u32 m_now = 0;
    struct Approach {
        Position destination;
        Position lastPosition;
        u32 lastProgress = 0;
        u32 lastRequest = 0;
        bool retried = false;
    } m_approach;
    std::deque<Msg2Ai> m_resCmdList;
};

} // namespace companion_ai
