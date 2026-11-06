#pragma once
#include "GameActor.h"
#include "Gr2Monster.h"
#include <chrono>

// Original CGrannyCorpse3d: no actor ID, an independent death animation, and
// a processing counter unrelated to the animation's elapsed-seconds clock.
class CGrannyCorpse3d final : public CGameObject {
public:
    CGrannyCorpse3d(int job, const vector3d& position, float rotation);
    u8 OnProcess() override;
    void Render(matrix* view) override;
    bool RenderAt(const matrix& view, u32 now);
    int StateCount() const { return m_stateCount; }
    u8 Alpha() const { return m_alpha; }
    float Rotation() const { return m_rotation; }
    const vector3d& Position() const { return m_position; }
    float AnimationClock() const { return m_model.Clock(); }
    u32 StartTick() const { return m_startTick; }
private:
    vector3d m_position;
    float m_rotation;
    int m_stateCount = 0;
    u8 m_alpha = 255;
    Gr2CorpseModel m_model;
    u32 m_startTick;
    u32 m_lastRenderTick;
    std::chrono::steady_clock::time_point m_lastRenderTime;
};
