#include "GrannyCorpse.h"

CGrannyCorpse3d::CGrannyCorpse3d(int job, const vector3d& position, float rotation)
    : m_position(position), m_rotation(rotation), m_model(job),
      m_startTick(timeGetTime()), m_lastRenderTick(m_startTick),
      m_lastRenderTime(std::chrono::steady_clock::now())
{
}

u8 CGrannyCorpse3d::OnProcess()
{
    // PE0x565C90 compares the old signed32 counter, subtracts its low byte
    // from0x58 into the byte alpha at+0x13C, then increments before returning.
    const int count = m_stateCount++;
    if (count > 355) m_alpha = static_cast<u8>(88 - count);
    return count <= 600;
}

void CGrannyCorpse3d::Render(matrix* view)
{
    if (!view) return;
    // Original GrannyGetSecondsElapsed uses a high-resolution elapsed clock,
    // initialized after model/resource setup, independent of the process count.
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - m_lastRenderTime).count();
    m_lastRenderTime = now;
    m_model.Render(m_position, m_rotation, m_alpha, *view, elapsed);
}

bool CGrannyCorpse3d::RenderAt(const matrix& view, u32 now)
{
    // Deterministic fixture entry; the live Render path uses the clock above.
    const double elapsed = static_cast<u32>(now - m_lastRenderTick) * 0.001;
    m_lastRenderTick = now;
    return m_model.Render(m_position, m_rotation, m_alpha, view, elapsed);
}
