// Ref CArrowEffect — Ref/Skill.cpp:235, OnProcess :492, SendMsg :1797.
// Sprite names are the cp949 strings at the byte_725* addresses in HighPriest.exe.
#include "GameActor.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "gamemode/View.h"
#include "render/DC.h"
#include "render/Renderer.h"
#include "res/Res.h"
#include "session/Session.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr const char* kSpriteRoot = "data\\sprite\\";
constexpr int kArrowBillboardSize = 96;
constexpr int kArrowBillboardAnchor = 48;

constexpr const char* kDefaultArrowSpr = "\xB8\xF3\xBD\xBA\xC5\xCD\\skel_archer_arrow.spr";
constexpr const char* kDefaultArrowAct = "\xB8\xF3\xBD\xBA\xC5\xCD\\skel_archer_arrow.act";
constexpr const char* kDefaultMandraSpr = "\xB8\xF3\xBD\xBA\xC5\xCD\\mandragora_atk.spr";
constexpr const char* kDefaultMandraAct = "\xB8\xF3\xBD\xBA\xC5\xCD\\mandragora_atk.act";

struct ArrowSpritePair {
    int job;
    const char* spr;
    const char* act;
};

constexpr ArrowSpritePair kJobArrowSprites[] = {
    { 1500, "\xB8\xF3\xBD\xBA\xC5\xCD\\PARASITE_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\PARASITE_BULLET.act" },
    { 1555, "\xB8\xF3\xBD\xBA\xC5\xCD\\PARASITE_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\PARASITE_BULLET.act" },
    { 1495, "\xB8\xF3\xBD\xBA\xC5\xCD\\STONE_SHOOTER_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\STONE_SHOOTER_BULLET.act" },
    { 1498, "\xB8\xF3\xBD\xBA\xC5\xCD\\WOOTAN_SHOOTER_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\WOOTAN_SHOOTER_BULLET.act" },
    { 1410, "\xB8\xF3\xBD\xBA\xC5\xCD\\PEACH_TREE_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\PEACH_TREE_BULLET.act" },
    { 1550, "\xB8\xF3\xBD\xBA\xC5\xCD\\PEACH_TREE_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\PEACH_TREE_BULLET.act" },
    { 1412, "\xB8\xF3\xBD\xBA\xC5\xCD\\GINSENG_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\GINSENG_BULLET.act" },
    { 1531, "\xB8\xF3\xBD\xBA\xC5\xCD\\GINSENG_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\GINSENG_BULLET.act" },
    { 1664, "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET.act" },
    { 1665, "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET_1.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET_1.act" },
    { 1666, "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET_2.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET_2.act" },
    { 1667, "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET_3.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\CANON_BULLET_3.act" },
    { 1688, "\xB8\xF3\xBD\xBA\xC5\xCD\\LADY_TANEE_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\LADY_TANEE_BULLET.act" },
    { 1865, "\xB8\xF3\xBD\xBA\xC5\xCD\\RAGGED_ZOMBIE_BULLET.spr", "\xB8\xF3\xBD\xBA\xC5\xCD\\RAGGED_ZOMBIE_BULLET.act" },
};

std::string SpriteFilePath(const char* relativeName)
{
    return std::string(kSpriteRoot) + (relativeName ? relativeName : "");
}

float ArrowStateFrames(u32 startTick)
{
    return static_cast<float>(static_cast<double>(timeGetTime() - startTick) * 0.041666668);
}

float CalcDist3(const vector3d& from, float x, float y, float z)
{
    const float dx = x - from.x;
    const float dy = y - from.y;
    const float dz = z - from.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void CalcDir(CRenderObject& object, float x, float z)
{
    float rot = std::atan2(x - object.m_pos.x, -(z - object.m_pos.z)) / 3.14159265f * 180.0f;
    if (rot >= 360.0f) {
        rot -= 360.0f;
    }
    if (rot < 0.0f) {
        rot += 360.0f;
    }
    object.m_roty = rot;
}

unsigned int ViewRelativeDir(float rot)
{
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (gameMode == nullptr || gameMode->m_view == nullptr) {
        return 0;
    }
    int angle = static_cast<int>(-gameMode->m_view->GetCameraLongitude() - rot);
    if (angle < 0) {
        angle += 360 * ((359 - angle) / 360);
    }
    if (angle >= 360) {
        angle %= 360;
    }
    return static_cast<unsigned int>(angle);
}

bool FindActorPos(unsigned int targetGid, vector3d* outPos)
{
    if (outPos == nullptr) {
        return false;
    }
    if (g_world.m_player != nullptr
        && (targetGid == g_session.m_aid || targetGid == g_world.m_player->m_gid)) {
        *outPos = g_world.m_player->m_pos;
        return true;
    }
    for (CGameActor* actor : g_world.m_actorList) {
        if (actor != nullptr && actor->m_gid == targetGid) {
            *outPos = actor->m_pos;
            return true;
        }
    }
    return false;
}

bool GetTargetPos(const CRenderObject& object, unsigned int targetGid, vector3d* outPos)
{
    if (FindActorPos(targetGid, outPos)) {
        return true;
    }
    if (g_world.m_player != nullptr) {
        const vector3d& playerPos = g_world.m_player->m_pos;
        *outPos = vector3d{
            object.m_pos.x - (playerPos.x - object.m_pos.x),
            object.m_pos.y - (playerPos.y - object.m_pos.y),
            object.m_pos.z - (playerPos.z - object.m_pos.z),
        };
    } else {
        *outPos = object.m_pos;
    }
    return false;
}

bool ProjectArrowPoint(const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (outVertex == nullptr) {
        return false;
    }
    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= 10.0f) {
        return false;
    }
    const float oow = 1.0f / clipZ;
    const float projectedX = point.x * viewMatrix.m[0][0]
        + point.y * viewMatrix.m[1][0]
        + point.z * viewMatrix.m[2][0]
        + viewMatrix.m[3][0];
    const float projectedY = point.x * viewMatrix.m[0][1]
        + point.y * viewMatrix.m[1][1]
        + point.z * viewMatrix.m[2][1]
        + viewMatrix.m[3][1];
    outVertex->x = g_renderer.m_xoffset + projectedX * g_renderer.m_hpc * oow;
    outVertex->y = g_renderer.m_yoffset + projectedY * g_renderer.m_vpc * oow;
    const float depth = (1500.0f / (1500.0f - 10.0f)) * ((1.0f / oow) - 10.0f) * oow;
    if (!std::isfinite(outVertex->x) || !std::isfinite(outVertex->y) || !std::isfinite(depth)) {
        return false;
    }
    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, depth));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return true;
}

} // namespace

CArrowEffect::CArrowEffect()
    : m_sPos{ 0.0f, 0.0f, 0.0f }
    , m_tPos{ 0.0f, 0.0f, 0.0f }
    , m_moveSpeed{ 0.0f, 0.0f, 0.0f }
    , m_hideTime(0.0f)
    , m_fastFactor(1.0f)
    , m_targetGid(0)
    , m_process(0)
    , m_spriteJob(0)
    , m_billboardTexture(nullptr)
    , m_cachedBillboardMotion(-1)
{
    m_baseAction = 0;
    m_curMotion = 0;
    m_isVisible = 0;
}

CArrowEffect::~CArrowEffect()
{
    delete m_billboardTexture;
    m_billboardTexture = nullptr;
}

void CArrowEffect::SetSpriteJob(int job)
{
    m_spriteJob = job;
}

void CArrowEffect::ApplySpriteForJob(int job)
{
    const char* spr = kDefaultArrowSpr;
    const char* act = kDefaultArrowAct;
    for (const ArrowSpritePair& pair : kJobArrowSprites) {
        if (pair.job == job) {
            spr = pair.spr;
            act = pair.act;
            break;
        }
    }
    if (m_sprName != spr || m_actName != act) {
        m_sprName = spr;
        m_actName = act;
        m_sprRes = nullptr;
        m_actRes = nullptr;
        m_cachedBillboardMotion = -1;
    }
}

void CArrowEffect::LoadSpriteResources()
{
    if (m_sprName.empty() || m_actName.empty()) {
        return;
    }
    if (m_sprRes == nullptr) {
        m_sprRes = g_resMgr.GetAs<CSprRes>(SpriteFilePath(m_sprName.c_str()).c_str());
    }
    if (m_actRes == nullptr) {
        m_actRes = g_resMgr.GetAs<CActRes>(SpriteFilePath(m_actName.c_str()).c_str());
    }
}

bool CArrowEffect::EnsureBillboard()
{
    LoadSpriteResources();
    if (m_sprRes == nullptr || m_actRes == nullptr) {
        return false;
    }
    const int motionCount = m_actRes->GetMotionCount(m_curAction);
    if (motionCount <= 0) {
        return false;
    }
    int motion = m_curMotion;
    if (motion < 0 || motion >= motionCount) {
        motion = motionCount - 1;
    }
    if (m_billboardTexture != nullptr && m_cachedBillboardMotion == motion) {
        return true;
    }
    const CMotion* spriteMotion = m_actRes->GetMotion(m_curAction, motion);
    if (spriteMotion == nullptr) {
        return false;
    }

    std::vector<unsigned int> pixels(static_cast<size_t>(kArrowBillboardSize) * kArrowBillboardSize, 0u);
    DrawActMotionToArgb(pixels.data(),
        kArrowBillboardSize,
        kArrowBillboardSize,
        kArrowBillboardAnchor,
        kArrowBillboardAnchor,
        m_sprRes,
        spriteMotion,
        m_sprRes->m_pal);
    for (unsigned int& pixel : pixels) {
        const unsigned int alpha = (pixel >> 24) & 0xFFu;
        if (alpha == 0u || alpha == 255u) {
            continue;
        }
        const unsigned int red = (((pixel >> 16) & 0xFFu) * 255u) / alpha;
        const unsigned int green = (((pixel >> 8) & 0xFFu) * 255u) / alpha;
        const unsigned int blue = (pixel & 0xFFu) * 255u / alpha;
        pixel = (alpha << 24) | ((red > 255u ? 255u : red) << 16) | ((green > 255u ? 255u : green) << 8) | (blue > 255u ? 255u : blue);
    }

    if (m_billboardTexture == nullptr) {
        m_billboardTexture = new CTexture();
        if (m_billboardTexture == nullptr
            || !m_billboardTexture->Create(kArrowBillboardSize, kArrowBillboardSize, PF_A8R8G8B8, false)) {
            delete m_billboardTexture;
            m_billboardTexture = nullptr;
            return false;
        }
    }
    m_billboardTexture->Update(0,
        0,
        kArrowBillboardSize,
        kArrowBillboardSize,
        pixels.data(),
        true,
        kArrowBillboardSize * static_cast<int>(sizeof(unsigned int)));
    m_cachedBillboardMotion = motion;
    return true;
}

u8 CArrowEffect::OnProcess()
{
    const float elapsed = ArrowStateFrames(m_stateStartTick);
    if (m_hideTime > elapsed) {
        return 1;
    }

    float flightFrames = elapsed;
    if (!m_isVisible) {
        m_isVisible = 1;
        vector3d targetPos = m_tPos;
        if (m_targetGid != 0) {
            vector3d lookedUp{};
            if (GetTargetPos(*this, m_targetGid, &lookedUp)) {
                targetPos = lookedUp;
            }
        }
        targetPos.y -= 10.0f;
        m_moveSpeed = vector3d{
            (targetPos.x - m_pos.x) * 0.125f,
            (targetPos.y - m_pos.y) * 0.125f,
            (targetPos.z - m_pos.z) * 0.125f,
        };
        const float distance = CalcDist3(m_pos, targetPos.x, targetPos.y, targetPos.z);
        if (distance < 40.0f && distance > 0.0001f) {
            m_fastFactor = 40.0f / distance;
            m_moveSpeed.x *= m_fastFactor;
            m_moveSpeed.y *= m_fastFactor;
            m_moveSpeed.z *= m_fastFactor;
        }
        CalcDir(*this, targetPos.x, targetPos.z);
        float dx = targetPos.x - m_pos.x;
        float dy = targetPos.y - m_pos.y;
        float dz = targetPos.z - m_pos.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length > 0.0001f) {
            const float scale = 2.5f / length;
            m_pos.x += dx * scale;
            m_pos.y += dy * scale;
            m_pos.z += dz * scale;
        }
    }

    flightFrames = elapsed - m_hideTime;
    m_curAction = m_baseAction;
    ++m_process;
    LoadSpriteResources();
    if (m_process > 8 && m_actRes != nullptr) {
        ++m_curMotion;
        const int motionCount = m_actRes->GetMotionCount(m_curAction);
        if (motionCount > 0 && m_curMotion >= motionCount) {
            m_curMotion = motionCount - 1;
        }
    }
    m_sprAngle = static_cast<int>(ViewRelativeDir(m_roty));
    m_pos.x = m_sPos.x + m_moveSpeed.x * flightFrames;
    m_pos.y = m_sPos.y + m_moveSpeed.y * flightFrames;
    m_pos.z = m_sPos.z + m_moveSpeed.z * flightFrames;
    if (m_fastFactor == 0.0f) {
        return 0;
    }
    return (8.0f / m_fastFactor >= flightFrames) ? 1 : 0;
}

void CArrowEffect::Render(matrix* viewMatrix)
{
    if (viewMatrix == nullptr || !m_isVisible || !EnsureBillboard()) {
        return;
    }

    tlvertex3d projected{};
    if (!ProjectArrowPoint(*viewMatrix, m_pos, &projected)) {
        return;
    }
    RPFace* face = g_renderer.BorrowNullRP();
    if (face == nullptr) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = m_billboardTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = projected.oow;

    const float rad = static_cast<float>(m_sprAngle) * 3.14159265f / 180.0f;
    const float cosAngle = std::cos(rad);
    const float sinAngle = std::sin(rad);
    const float left = -static_cast<float>(kArrowBillboardAnchor);
    const float top = -static_cast<float>(kArrowBillboardAnchor);
    const float right = static_cast<float>(kArrowBillboardSize - kArrowBillboardAnchor);
    const float bottom = static_cast<float>(kArrowBillboardSize - kArrowBillboardAnchor);
    const float corners[4][2] = {
        { left, top },
        { right, top },
        { left, bottom },
        { right, bottom },
    };
    for (int index = 0; index < 4; ++index) {
        const float x = corners[index][0];
        const float y = corners[index][1];
        face->m_verts[index].x = projected.x + x * cosAngle - y * sinAngle;
        face->m_verts[index].y = projected.y + x * sinAngle + y * cosAngle;
        face->m_verts[index].z = projected.z;
        face->m_verts[index].oow = projected.oow;
        face->m_verts[index].color = 0xFFFFFFFFu;
        face->m_verts[index].specular = 0xFF000000u;
    }
    face->m_verts[0].tu = 0.0f;
    face->m_verts[0].tv = 0.0f;
    face->m_verts[1].tu = 1.0f;
    face->m_verts[1].tv = 0.0f;
    face->m_verts[2].tu = 0.0f;
    face->m_verts[2].tv = 1.0f;
    face->m_verts[3].tu = 1.0f;
    face->m_verts[3].tv = 1.0f;
    g_renderer.AddRP(face, 1);
}

void CArrowEffect::SendMsg(CGameObject*, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3)
{
    if (message == 14) {
        ApplySpriteForJob(m_spriteJob);
        const vector3d* sourcePos = reinterpret_cast<const vector3d*>(arg1);
        if (sourcePos != nullptr) {
            m_pos = *sourcePos;
        }
        m_pos.y -= 10.0f;
        m_sPos = m_pos;
        m_fastFactor = 1.0f;
        m_isVisible = 0;
        m_targetGid = static_cast<u32>(arg2);
        vector3d lookedUp{};
        if (!GetTargetPos(*this, m_targetGid, &lookedUp)) {
            lookedUp = m_pos;
        }
        m_tPos = lookedUp;
        m_stateStartTick = timeGetTime();
        if (arg3 != 0) {
            m_hideTime = *reinterpret_cast<const float*>(arg3);
        }
        return;
    }
    if (message == 64) {
        ApplySpriteForJob(0);
        const vector3d* sourcePos = reinterpret_cast<const vector3d*>(arg1);
        const vector3d* targetPos = reinterpret_cast<const vector3d*>(arg2);
        if (sourcePos != nullptr) {
            m_pos = *sourcePos;
        }
        m_pos.y -= 10.0f;
        m_sPos = m_pos;
        if (targetPos != nullptr) {
            m_tPos = *targetPos;
        }
        m_tPos.y -= 10.0f;
        m_fastFactor = 1.0f;
        m_isVisible = 0;
        m_targetGid = 0;
        m_stateStartTick = timeGetTime();
        if (arg3 != 0) {
            m_hideTime = *reinterpret_cast<const float*>(arg3);
        }
    }
}

u8 CMandraAttackEffect::OnProcess()
{
    const float elapsedFrames = ArrowStateFrames(m_stateStartTick);
    if (elapsedFrames < m_mandraHideTime) {
        m_isVisible = 0;
        return 1;
    }

    m_isVisible = 1;
    const float animationFrames = elapsedFrames - m_mandraHideTime;
    if (m_mandraMotionFinished) {
        return 0;
    }

    LoadSpriteResources();
    if (m_actRes == nullptr) {
        return 1;
    }
    const int motionCount = m_actRes->GetMotionCount(m_curAction);
    if (motionCount <= 0) {
        return 1;
    }

    const double motionFrame = static_cast<double>(animationFrames) / m_motionSpeed;
    m_curMotion = static_cast<int>(motionFrame) % motionCount;
    if (motionFrame / static_cast<double>(motionCount) >= 1.0) {
        m_mandraMotionFinished = true;
        m_curMotion = motionCount - 1;
        return 0;
    }
    m_sprAngle = static_cast<int>(ViewRelativeDir(m_roty));
    return 1;
}

void CMandraAttackEffect::SendMsg(CGameObject* sender, int message, msgparam_t arg1, msgparam_t arg2, msgparam_t arg3)
{
    if (message != 14) {
        CRenderObject::SendMsg(sender, message, arg1, arg2, arg3);
        return;
    }

    const int job = static_cast<int>(arg3);
    const char* sprite = kDefaultMandraSpr;
    const char* action = kDefaultMandraAct;
    switch (job) {
    case 1884:
    case 1886:
        sprite = "\xB8\xF3\xBD\xBA\xC5\xCD\\drosera_bullet.spr";
        action = "\xB8\xF3\xBD\xBA\xC5\xCD\\drosera_bullet.act";
        break;
    case 1957:
    case 1958:
    case 1959:
    case 1960:
    case 1961:
        sprite = "\xB8\xF3\xBD\xBA\xC5\xCD\\ENTWEIHEN_BULLET.spr";
        action = "\xB8\xF3\xBD\xBA\xC5\xCD\\ENTWEIHEN_BULLET.act";
        break;
    case 1781:
        sprite = "\xB8\xF3\xBD\xBA\xC5\xCD\\hydra_atk.spr";
        action = "\xB8\xF3\xBD\xBA\xC5\xCD\\hydra_atk.act";
        break;
    case 1068:
        sprite = "\xB8\xF3\xBD\xBA\xC5\xCD\\MAVKA_BULLET.spr";
        action = "\xB8\xF3\xBD\xBA\xC5\xCD\\MAVKA_BULLET.act";
        break;
    case 1704:
    case 1709:
        sprite = "\xB8\xF3\xBD\xBA\xC5\xCD\\tha_odium_ef.spr";
        action = "\xB8\xF3\xBD\xBA\xC5\xCD\\tha_odium_ef.act";
        break;
    default:
        break;
    }

    m_sprName = sprite;
    m_actName = action;
    m_sprRes = nullptr;
    m_actRes = nullptr;
    m_billboardTexture = nullptr;
    m_cachedBillboardMotion = -1;
    m_curAction = 0;
    m_curMotion = 0;
    m_mandraMotionFinished = false;
    m_isVisible = 1;

    const unsigned int targetGid = static_cast<unsigned int>(arg1);
    vector3d targetPos{};
    GetTargetPos(*this, targetGid, &targetPos);
    m_pos = targetPos;
    m_targetGid = targetGid;
    m_stateStartTick = timeGetTime();
    m_motionSpeed = 1.0f;
    LoadSpriteResources();
    if (m_actRes != nullptr) {
        const float sourceDelay = m_actRes->GetDelay(0);
        if (sourceDelay > 0.0f) {
            m_motionSpeed = sourceDelay;
        }
    }
    if (job == 1068) {
        m_motionSpeed = 2.0f;
    }
    if (arg2 != 0) {
        m_mandraHideTime = *reinterpret_cast<const float*>(arg2);
    }
}

void CGameMode::MakeArrowEffectPos(const vector3d& sourcePos, const vector3d& targetPos, float hideTime)
{
    if (m_world == nullptr) {
        return;
    }
    CArrowEffect* arrow = new CArrowEffect();
    m_world->m_gameObjectList.push_back(arrow);
    arrow->SendMsg(nullptr,
        64,
        reinterpret_cast<msgparam_t>(&sourcePos),
        reinterpret_cast<msgparam_t>(&targetPos),
        reinterpret_cast<msgparam_t>(&hideTime));
}

void CGameMode::MakeArrowEffect(const vector3d& sourcePos, unsigned int targetGid, float hideTime, int job)
{
    if (m_world == nullptr || targetGid == 0) {
        return;
    }
    CArrowEffect* arrow = new CArrowEffect();
    arrow->SetSpriteJob(job);
    m_world->m_gameObjectList.push_back(arrow);
    arrow->SendMsg(nullptr,
        14,
        reinterpret_cast<msgparam_t>(&sourcePos),
        static_cast<msgparam_t>(targetGid),
        reinterpret_cast<msgparam_t>(&hideTime));
}

void CGameMode::MakeMandraAttackEffect(unsigned int targetGid, int job, float hideTime)
{
    if (m_world == nullptr || targetGid == 0) {
        return;
    }
    CMandraAttackEffect* effect = new CMandraAttackEffect();
    if (effect == nullptr) {
        return;
    }
    m_world->m_gameObjectList.push_back(effect);
    effect->SendMsg(nullptr,
        14,
        static_cast<msgparam_t>(targetGid),
        reinterpret_cast<msgparam_t>(&hideTime),
        static_cast<msgparam_t>(job));
}

void CGameMode::GetServerCoor(float clientX, float clientZ, int& serverX, int& serverY) const
{
    serverX = 0;
    serverY = 0;
    if (m_world == nullptr || m_world->m_attr == nullptr || m_world->m_attr->m_zoom == 0) {
        return;
    }
    const float zoom = static_cast<float>(m_world->m_attr->m_zoom);
    serverX = static_cast<int>(std::floor(static_cast<double>(m_world->m_attr->m_width / 2) + static_cast<double>(clientX / zoom)));
    serverY = static_cast<int>(std::floor(static_cast<double>(m_world->m_attr->m_height / 2) + static_cast<double>(clientZ / zoom)));
}

void CGameMode::GetClientCoor(int serverX, int serverY, float& clientX, float& clientZ) const
{
    clientX = 0.0f;
    clientZ = 0.0f;
    if (m_world == nullptr || m_world->m_attr == nullptr) {
        return;
    }
    const float zoom = static_cast<float>(m_world->m_attr->m_zoom);
    const float halfCell = zoom * 0.5f;
    clientX = (static_cast<float>(serverX) - static_cast<float>(m_world->m_attr->m_width) * 0.5f) * zoom + halfCell;
    clientZ = (static_cast<float>(serverY) - static_cast<float>(m_world->m_attr->m_height) * 0.5f) * zoom + halfCell;
}
