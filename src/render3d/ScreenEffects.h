#pragma once

// Ref/GameActor.cpp status 34 enables the wave pass. The Confusion health bit
// is a separate ailment. Disabling waves does not remove Hallucination.
constexpr unsigned int ResolveScreenEffectFlags(bool ssao, bool confusionWaves, bool effectsOn, int healthState,
    bool hallucination = false)
{
    return (ssao ? 1u : 0u)
        | ((confusionWaves && effectsOn && hallucination) ? 2u : 0u)
        | ((healthState & 0x10) != 0 ? 4u : 0u);
}
