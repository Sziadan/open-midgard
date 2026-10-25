// Ref CGroundScope — Ref/Skill.cpp ctor :296, OnProcess :662, Render :2307, SendMsg :2445, GetScopeSize :4.
#include "GameActor.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "render/Renderer.h"
#include "res/Texture.h"
#include "World.h"

#include <cmath>
#include <windows.h>

CGroundScope::CGroundScope()
{
    m_lastingTime = 0;
    m_rot = 0.0f;
    m_scopeSize = 1;
}

u8 CGroundScope::OnProcess()
{
    return m_lastingTime >= timeGetTime() ? 1 : 0;
}

void CGroundScope::Render(matrix* viewMatrix)
{
    if (viewMatrix == nullptr || m_scopeSize <= 0) {
        return;
    }

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode == nullptr || gameMode->m_world == nullptr || gameMode->m_world->m_ground == nullptr) {
        return;
    }

    const float sinR = std::sin(m_rot);
    const float cosR = std::cos(m_rot);
    const int originX = m_groundPosX - m_scopeSize / 2;
    const int originY = m_groundPosY - m_scopeSize / 2;
    const float half = static_cast<float>(m_scopeSize) * 0.5f;
    const float span = static_cast<float>(m_scopeSize);
    const u32 color = 0x80FFFFFFu - (static_cast<u32>(static_cast<long long>(sinR * -40.0f)) << 24);
    CTexture* texture = g_texMgr.GetTexture("effect\\magic_target.tga", false);

    for (int row = 0; row < m_scopeSize; ++row) {
        const float v0 = static_cast<float>(row) - half;
        const float v1 = static_cast<float>(row + 1) - half;
        for (int column = 0; column < m_scopeSize; ++column) {
            const float u0 = static_cast<float>(column) - half;
            const float u1 = static_cast<float>(column + 1) - half;
            vector2d uv[4];
            uv[0].x = u0 * cosR + v0 * sinR;
            uv[0].y = -u0 * sinR + v0 * cosR;
            uv[1].x = u1 * cosR + v0 * sinR;
            uv[1].y = -u1 * sinR + v0 * cosR;
            uv[2].x = u0 * cosR + v1 * sinR;
            uv[2].y = -u0 * sinR + v1 * cosR;
            uv[3].x = u1 * cosR + v1 * sinR;
            uv[3].y = -u1 * sinR + v1 * cosR;
            for (vector2d& coord : uv) {
                coord.x = (coord.x + half) / span;
                coord.y = (coord.y + half) / span;
            }
            gameMode->m_world->m_ground->RenderGroundTile(*viewMatrix,
                originX + column,
                originY + row,
                color,
                texture,
                uv);
        }
    }

    m_rot += m_rotSpeed;
}

void CGroundScope::SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3)
{
    switch (message) {
    case 14:
        m_groundPosX = static_cast<int>(arg1);
        m_groundPosY = static_cast<int>(arg2);
        break;
    case 22:
        m_masterGid = static_cast<u32>(arg1);
        break;
    case 115:
        m_lastingTime = static_cast<u32>(arg1);
        break;
    case 117: {
        const int scopeSize = GetScopeSize(static_cast<int>(arg1)) + 2;
        m_scopeSize = scopeSize;
        m_rotSpeed = static_cast<float>(0.5 / static_cast<double>(scopeSize));
        break;
    }
    default:
        CAbleToMakeEffect::SendMsg(sender, message, arg1, arg2, arg3);
        break;
    }
}

int CGroundScope::GetScopeSize(int skillId) const
{
    switch (skillId) {
    case 11:
    case 25:
    case 47:
    case 69:
    case 80:
    case 86:
    case 88:
    case 110:
    case 129:
    case 130:
    case 137:
    case 140:
    case 381:
    case 483:
        return 3;
    case 17:
    case 21:
    case 70:
    case 91:
    case 92:
    case 484:
        return 5;
    case 79:
    case 478:
        return 7;
    case 83:
    case 89:
        return 9;
    case 85:
        return 11;
    default:
        return 0;
    }
}
