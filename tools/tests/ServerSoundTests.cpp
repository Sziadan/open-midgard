#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
struct vector3d { float x, y, z; };
struct CGameActor {
    vector3d m_pos;
    u32 delay = 0;
    u32 forceDuration = 0;
    int m_isMotionFreezed = 0, m_isSprArgbFixed = 0, tint = 255;
    void SetForceAnimEndTick(u32 tick) { delay = tick; }
    void SetForceAnimation(int act, int motion, int maximum, int finishAct, int finishMotion, int animate, u32 duration, int loop) {
        if (act == 96 && motion == 4 && maximum == 4 && finishAct == 96 && finishMotion == 4 && animate == 1 && loop == 0) {
            forceDuration = duration;
        }
    }
    void SetArgb(int, int r, int g, int b) { tint = (r == g && g == b) ? r : -1; }
};
struct CSession {
    struct ForceAnimTick { u32 aid, tick; };
    std::vector<ForceAnimTick> m_forceAnimEndTick;
    u32 m_aid = 77;
    u32 GetForceAnimEndTick(u32 aid) const;
    void SetForceAnimEndTick(u32 aid, u32 tick);
} g_session;
struct CWorld {
    CGameActor* m_player = nullptr;
    CGameActor* actor = nullptr;
    CGameActor* GetGameActorByAID(u32 aid) { return aid == 42 ? actor : nullptr; }
};
struct PLAY_WAVE_INFO {
    std::string wavName;
    u32 nAID, term, endTick;
};
struct CGameMode {
    CWorld* m_world = nullptr;
    std::vector<PLAY_WAVE_INFO> m_playWaveList;
    void ProcessPlayWave();
};
struct PacketView { const u8* data; unsigned int packetLength; };
struct WaveCall { std::string name; float x, y, z; };
std::vector<WaveCall> calls;
u32 now = 0;
u32 timeGetTime() { return now; }
u32 ReadLE32(const u8* p)
{
    return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);
}
bool PlayWave(const char* name, float x, float y, float z)
{
    calls.push_back({ name, x, y, z });
    return true;
}

#include "ComboBodies.inc"
#include "SoundBodies.inc"

int main()
{
    int failures = 0;
    auto check = [&](bool condition, const char* name) {
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };
    std::array<u8, 35> packet{};
    packet[0] = 0xD3;
    packet[1] = 1;
    std::memcpy(packet.data() + 2, "effect\\sample.wav", 17);
    auto write = [&](int offset, u32 value) {
        for (int byte = 0; byte < 4; ++byte) packet[offset + byte] = u8(value >> (byte * 8));
    };
    write(31, 42);
    CGameActor player{ { 10, 100, 20 } }, actor{ { 4, -10, 12 } };
    CWorld world{ &player, &actor };
    CGameMode mode;
    mode.m_world = &world;
    HandleSound(mode, { packet.data(), 34 });
    HandleSound(mode, { nullptr, 35 });
    check(calls.empty(), "short/null packet rejected");
    HandleSound(mode, { packet.data(), 35 });
    check(calls.size() == 1 && calls.back().x == 6 && calls.back().y == 0 && calls.back().z == 8,
        "one-shot uses player minus actor, zero height");
    write(31, 999);
    HandleSound(mode, { packet.data(), 35 });
    check(calls.size() == 2 && calls.back().x == 0 && calls.back().z == 0,
        "missing one-shot actor plays at listener");
    calls.clear();
    packet[26] = 1;
    write(27, 50);
    write(31, 42);
    now = 100;
    HandleSound(mode, { packet.data(), 35 });
    check(mode.m_playWaveList.size() == 1 && mode.m_playWaveList[0].endTick == 150,
        "repeat stores term plus receipt tick");
    check(calls.size() == 1 && calls.back().x == 0 && calls.back().z == 0,
        "repeat starts immediately at listener");
    now = 149;
    mode.ProcessPlayWave();
    check(calls.size() == 1, "repeat waits before deadline");
    now = 150;
    mode.ProcessPlayWave();
    check(calls.size() == 2 && calls.back().x == 6 && calls.back().z == 8
        && mode.m_playWaveList[0].endTick == 200, "repeat delivers at deadline and reschedules");
    world.actor = nullptr;
    now = 200;
    mode.ProcessPlayWave();
    check(calls.size() == 2 && mode.m_playWaveList[0].endTick == 250,
        "missing repeated actor skips playback and advances deadline");
    HandleSound(mode, { packet.data(), 35 });
    packet[26] = 2;
    write(31, 123); // Cancellation matches name, not AID.
    HandleSound(mode, { packet.data(), 35 });
    check(mode.m_playWaveList.size() == 1, "cancel removes only first equal name regardless of aid");
    HandleSound(mode, { packet.data(), 35 });
    check(mode.m_playWaveList.empty(), "second cancel removes second repeat");
    calls.clear();
    packet[26] = 7;
    HandleSound(mode, { packet.data(), 35 });
    check(calls.empty() && mode.m_playWaveList.empty(), "unknown action has no effect");
    packet[26] = 1;
    write(31, 0);
    HandleSound(mode, { packet.data(), 35 });
    calls.clear();
    now += 50;
    mode.ProcessPlayWave();
    check(calls.size() == 1 && calls.back().x == 0 && calls.back().z == 0,
        "aid-zero repeat is listener-relative");
    packet[26] = 0;
    std::memset(packet.data() + 2, 'x', 24);
    HandleSound(mode, { packet.data(), 35 });
    check(calls.back().name.size() == 24, "24-byte nonterminated name bounded to packet field");
    check(g_session.GetForceAnimEndTick(42) == 0xFFFFFFFFu, "missing combo tick uses reference sentinel");
    std::array<u8, 10> combo{ 0xD2, 1, 42, 0, 0, 0, 123, 0, 0, 0 };
    world.actor = &actor;
    HandleComboDelay(mode, { combo.data(), 9 });
    check(g_session.m_forceAnimEndTick.empty(), "short combo packet rejected");
    HandleComboDelay(mode, { combo.data(), 10 });
    check(g_session.GetForceAnimEndTick(42) == 123 && actor.delay == 123,
        "combo stores raw duration and updates remote actor");
    combo[6] = 55;
    HandleComboDelay(mode, { combo.data(), 10 });
    check(g_session.GetForceAnimEndTick(42) == 55 && g_session.m_forceAnimEndTick.size() == 1,
        "combo replaces existing duration without duplicate entries");
    combo[2] = 77;
    HandleComboDelay(mode, { combo.data(), 10 });
    check(player.delay == 55 && g_session.GetForceAnimEndTick(77) == 55,
        "combo local aid selects player");
    combo[2] = 99;
    HandleComboDelay(mode, { combo.data(), 10 });
    check(g_session.GetForceAnimEndTick(99) == 55 && g_session.m_forceAnimEndTick.size() == 3,
        "combo retained for absent actor");
    std::array<u8, 14> blade{ 0xD1, 1, 77, 0, 0, 0, 42, 0, 0, 0, 1, 0, 0, 0 };
    HandleBladeStop(mode, { blade.data(), 13 });
    check(player.forceDuration == 0, "short Blade Stop packet rejected");
    HandleBladeStop(mode, { blade.data(), 14 });
    check(player.forceDuration == 0x5F5E0FFu && player.m_isSprArgbFixed == 1 && player.tint == 64
        && actor.m_isMotionFreezed == 1 && actor.m_isSprArgbFixed == 1 && actor.tint == 64,
        "Blade Stop start fixes both tints and freezes target");
    blade[10] = 0;
    world.actor = nullptr;
    HandleBladeStop(mode, { blade.data(), 14 });
    check(player.forceDuration == 1 && player.m_isSprArgbFixed == 0 && player.tint == 255,
        "Blade Stop release restores source with absent target");
    blade[2] = 99;
    world.actor = &actor;
    HandleBladeStop(mode, { blade.data(), 14 });
    check(actor.m_isMotionFreezed == 0 && actor.m_isSprArgbFixed == 0 && actor.tint == 255,
        "Blade Stop release restores target with absent source");
    blade[10] = 1;
    HandleBladeStop(mode, { blade.data(), 14 });
    check(actor.m_isMotionFreezed == 0 && actor.tint == 255, "Blade Stop start requires both actors");
    if (!failures) std::puts("24 sound/combo/Blade Stop checks passed; no media played.");
    return failures ? 1 : 0;
}
