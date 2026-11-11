#pragma once

// Tunable companion AI constants (ported from OldRO AI/Const.lua + AI.lua magic numbers).
// Edit this header to change follow distance, aggressiveness thresholds, etc.

namespace companion_ai {

// --- GetV keys (Const.lua) ---
enum ValueId : int {
    V_OWNER = 0,
    V_POSITION = 1,
    V_TYPE = 2,
    V_MOTION = 3,
    V_ATTACKRANGE = 4,
    V_TARGET = 5,
    V_SKILLATTACKRANGE = 6,
    V_HOMUNTYPE = 7,
    V_HP = 8,
    V_SP = 9,
    V_MAXHP = 10,
    V_MAXSP = 11,
    V_MERTYPE = 12,
};

// --- Motions (Const.lua / actor stateId) ---
enum MotionId : int {
    MOTION_STAND = 0,
    MOTION_MOVE = 1,
    MOTION_ATTACK = 2,
    MOTION_DEAD = 3,
    MOTION_ATTACK2 = 9,
};

// --- Commands (Const.lua) ---
enum CommandId : int {
    NONE_CMD = 0,
    MOVE_CMD = 1,
    STOP_CMD = 2,
    ATTACK_OBJECT_CMD = 3,
    ATTACK_AREA_CMD = 4,
    PATROL_CMD = 5,
    HOLD_CMD = 6,
    SKILL_OBJECT_CMD = 7,
    SKILL_AREA_CMD = 8,
    FOLLOW_CMD = 9,
};

// --- States (AI.lua) ---
enum StateId : int {
    IDLE_ST = 0,
    FOLLOW_ST = 1,
    CHASE_ST = 2,
    ATTACK_ST = 3,
    MOVE_CMD_ST = 4,
    STOP_CMD_ST = 5,
    ATTACK_OBJECT_CMD_ST = 6,
    ATTACK_AREA_CMD_ST = 7,
    PATROL_CMD_ST = 8,
    HOLD_CMD_ST = 9,
    SKILL_OBJECT_CMD_ST = 10,
    SKILL_AREA_CMD_ST = 11,
    FOLLOW_CMD_ST = 12,
};

// --- Homunculus types (Const.lua); job = 6000 + type for 6001..6016 ---
enum HomunType : int {
    LIF = 1,
    AMISTR = 2,
    FILIR = 3,
    VANILMIRTH = 4,
    LIF2 = 5,
    AMISTR2 = 6,
    FILIR2 = 7,
    VANILMIRTH2 = 8,
    LIF_H = 9,
    AMISTR_H = 10,
    FILIR_H = 11,
    VANILMIRTH_H = 12,
    LIF_H2 = 13,
    AMISTR_H2 = 14,
    FILIR_H2 = 15,
    VANILMIRTH_H2 = 16,
};

// --- Mercenary types (Const.lua); job = 6016 + type for 6017..6046 ---
enum MerType : int {
    ARCHER01 = 1,
    SWORDMAN10 = 30,
};

// --- Behavior distances (AI.lua / Util.lua) ---
constexpr int kFollowDistance = 3;          // IDLE→FOLLOW / FOLLOW arrive
constexpr int kOutOfSightDistance = 20;     // IsOutOfSight
constexpr int kLongMoveManhattan = 15;      // OnMOVE_CMD split waypoint
constexpr int kServerPathLimitManhattan = 24; // MercLuaFunc l_Move gate
constexpr int kReservedCmdQueueCap = 10;
constexpr int kDefaultAttackRange = 1;
constexpr int kMinEnemyScanDistance = 100;

// Attack packet throttle when combat.attackSpeed is missing (ms).
constexpr int kDefaultAttackIntervalMs = 500;

// Job id bases used to derive Lua-style type ids.
constexpr int kHomunJobBase = 6000; // type = job - base (6001→1)
constexpr int kMerJobBase = 6016;   // type = job - base (6017→1)
constexpr int kHomunJobMin = 6001;
constexpr int kHomunJobMax = 6016;
constexpr int kMerJobMin = 6017;
constexpr int kMerJobMax = 6046;
constexpr int kMonsterJobMin = 1000;

// Packet opcodes (RunningServer packet_ver 23 / packet_db).
constexpr unsigned short kPacketRequestMoveNpc = 0x0232;
constexpr unsigned short kPacketRequestActNpc = 0x0233;
constexpr unsigned short kPacketRequestMoveToOwner = 0x0234;
constexpr unsigned char kActionAttack = 0;

} // namespace companion_ai
