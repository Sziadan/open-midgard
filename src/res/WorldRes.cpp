#include "WorldRes.h"

#include "DebugLog.h"
#include "render/Prim.h"

#include <cstring>

namespace {

struct LegacySoundSrcInfo {
    char name[80];
    char waveName[80];
    vector3d pos;
    float vol;
    int width;
    int height;
    float range;
};

static_assert(sizeof(LegacySoundSrcInfo) == 188, "LegacySoundSrcInfo size mismatch");

constexpr size_t kActorTailSize = sizeof(C3dWorldRes::actorInfo::modelName)
    + sizeof(C3dWorldRes::actorInfo::nodeName)
    + sizeof(vector3d) * 3;

bool WorldVersionAtLeast(u8 major, u8 minor, u8 requiredMajor, u8 requiredMinor)
{
    return major > requiredMajor || (major == requiredMajor && minor >= requiredMinor);
}

bool ReadU8(const unsigned char* buffer, size_t size, size_t* offset, u8* outValue)
{
    if (!offset || !outValue || *offset + sizeof(u8) > size) {
        return false;
    }

    *outValue = buffer[*offset];
    *offset += sizeof(u8);
    return true;
}

bool ReadI32(const unsigned char* buffer, size_t size, size_t* offset, int* outValue)
{
    if (!offset || !outValue || *offset + sizeof(int) > size) {
        return false;
    }

    std::memcpy(outValue, buffer + *offset, sizeof(int));
    *offset += sizeof(int);
    return true;
}

bool ReadF32(const unsigned char* buffer, size_t size, size_t* offset, float* outValue)
{
    if (!offset || !outValue || *offset + sizeof(float) > size) {
        return false;
    }

    std::memcpy(outValue, buffer + *offset, sizeof(float));
    *offset += sizeof(float);
    return true;
}

bool ReadVec3(const unsigned char* buffer, size_t size, size_t* offset, vector3d* outValue)
{
    return ReadF32(buffer, size, offset, &outValue->x)
        && ReadF32(buffer, size, offset, &outValue->y)
        && ReadF32(buffer, size, offset, &outValue->z);
}

bool ReadBytes(const unsigned char* buffer, size_t size, size_t* offset, void* outValue, size_t byteCount)
{
    if (!offset || !outValue || *offset + byteCount > size) {
        return false;
    }

    std::memcpy(outValue, buffer + *offset, byteCount);
    *offset += byteCount;
    return true;
}

bool FailWorldLoad(const char* fName, const char* stage, size_t offset, int size, u8 major, u8 minor)
{
    DbgLog("[WorldRes] load failed file='%s' stage=%s offset=%u size=%d ver=%u.%u\n",
        fName ? fName : "(null)",
        stage ? stage : "(unknown)",
        static_cast<unsigned>(offset),
        size,
        static_cast<unsigned>(major),
        static_cast<unsigned>(minor));
    return false;
}

std::string ReadFixedString(const unsigned char* buffer, size_t size, size_t* offset, size_t fieldLength)
{
    if (!offset || *offset + fieldLength > size) {
        return std::string();
    }

    size_t actualLength = 0;
    while (actualLength < fieldLength && buffer[*offset + actualLength] != 0) {
        ++actualLength;
    }

    const std::string result(reinterpret_cast<const char*>(buffer + *offset), actualLength);
    *offset += fieldLength;
    return result;
}

vector3d ComputeLightDirection(int lightLatitude, int lightLongitude)
{
    const float kPi = 3.141592f;
    matrix lightMatrix{};
    MatrixIdentity(lightMatrix);
    MatrixAppendXRotation(lightMatrix, static_cast<float>(lightLatitude) * kPi / 180.0f);
    MatrixAppendYRotation(lightMatrix, static_cast<float>(lightLongitude) * kPi / 180.0f);

    vector3d lightDir{};
    lightDir.x = lightMatrix.m[1][0];
    lightDir.y = lightMatrix.m[1][1];
    lightDir.z = lightMatrix.m[1][2];
    return lightDir;
}

} // namespace

C3dWorldRes::C3dWorldRes()
{
    Reset();
}

C3dWorldRes::~C3dWorldRes()
{
    Reset();
}

bool C3dWorldRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    Reset();

    if (!buffer || size < 6) {
        return FailWorldLoad(fName, "header", 0, size, 0, 0);
    }

    if (std::memcmp(buffer, "GRSW", 4) != 0) {
        return FailWorldLoad(fName, "magic", 0, size, 0, 0);
    }

    size_t offset = 4;
    if (!ReadU8(buffer, static_cast<size_t>(size), &offset, &m_verMajor)
        || !ReadU8(buffer, static_cast<size_t>(size), &offset, &m_verMinor)) {
        return FailWorldLoad(fName, "version", offset, size, m_verMajor, m_verMinor);
    }

    if (m_verMajor > 2 || (m_verMajor == 2 && m_verMinor > 1)) {
        return FailWorldLoad(fName, "unsupported_version", offset, size, m_verMajor, m_verMinor);
    }

    const std::string ignoredIniFile = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
    if (offset > static_cast<size_t>(size)) {
        return FailWorldLoad(fName, "ini_file", offset, size, m_verMajor, m_verMinor);
    }

    (void)ignoredIniFile;

    m_gndFile = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
    if (offset > static_cast<size_t>(size)) {
        return FailWorldLoad(fName, "gnd_file", offset, size, m_verMajor, m_verMinor);
    }

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 4)) {
        m_attrFile = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
        if (offset > static_cast<size_t>(size)) {
            return FailWorldLoad(fName, "attr_file", offset, size, m_verMajor, m_verMinor);
        }
    }

    m_scrFile = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
    if (offset > static_cast<size_t>(size)) {
        return FailWorldLoad(fName, "scr_file", offset, size, m_verMajor, m_verMinor);
    }

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 3)
        && !ReadF32(buffer, static_cast<size_t>(size), &offset, &m_waterLevel)) {
        return FailWorldLoad(fName, "water_level", offset, size, m_verMajor, m_verMinor);
    }

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 8)) {
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_waterType)
            || !ReadF32(buffer, static_cast<size_t>(size), &offset, &m_waveHeight)
            || !ReadF32(buffer, static_cast<size_t>(size), &offset, &m_waveSpeed)
            || !ReadF32(buffer, static_cast<size_t>(size), &offset, &m_wavePitch)) {
            return FailWorldLoad(fName, "water_params", offset, size, m_verMajor, m_verMinor);
        }
    }

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 9)
        && !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_waterAnimSpeed)) {
        return FailWorldLoad(fName, "water_anim", offset, size, m_verMajor, m_verMinor);
    }

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 5)) {
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_lightLongitude)
            || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_lightLatitude)
            || !ReadVec3(buffer, static_cast<size_t>(size), &offset, &m_diffuseCol)
            || !ReadVec3(buffer, static_cast<size_t>(size), &offset, &m_ambientCol)) {
            return FailWorldLoad(fName, "light", offset, size, m_verMajor, m_verMinor);
        }
    }

    m_lightDir = ComputeLightDirection(m_lightLatitude, m_lightLongitude);

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 7)) {
        float ignored = 0.0f;
        if (!ReadF32(buffer, static_cast<size_t>(size), &offset, &ignored)) {
            return FailWorldLoad(fName, "unknown_f32", offset, size, m_verMajor, m_verMinor);
        }
    }

    if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 6)) {
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_groundTop)
            || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_groundBottom)
            || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_groundLeft)
            || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_groundRight)) {
            return FailWorldLoad(fName, "ground_bounds", offset, size, m_verMajor, m_verMinor);
        }
    }

    int objectCount = 0;
    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &objectCount) || objectCount < 0) {
        return FailWorldLoad(fName, "object_count", offset, size, m_verMajor, m_verMinor);
    }

    for (int objectIndex = 0; objectIndex < objectCount; ++objectIndex) {
        int type = 0;
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &type)) {
            return FailWorldLoad(fName, "object_type", offset, size, m_verMajor, m_verMinor);
        }

        switch (type) {
        case 1: {
            actorInfo* actor = new actorInfo{};
            int animType = 0;
            int blockType = 0;
            float animSpeed = 1.0f;
            if (WorldVersionAtLeast(m_verMajor, m_verMinor, 1, 3)) {
                if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, actor->name, sizeof(actor->name))
                    || !ReadI32(buffer, static_cast<size_t>(size), &offset, &animType)
                    || !ReadF32(buffer, static_cast<size_t>(size), &offset, &animSpeed)
                    || !ReadI32(buffer, static_cast<size_t>(size), &offset, &blockType)) {
                    delete actor;
                    return FailWorldLoad(fName, "actor_header", offset, size, m_verMajor, m_verMinor);
                }
                if (animSpeed <= 0.0f || animSpeed > 100.0f) {
                    animSpeed = 1.0f;
                }
            }

            if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, actor->modelName, kActorTailSize)) {
                delete actor;
                return FailWorldLoad(fName, "actor_tail", offset, size, m_verMajor, m_verMinor);
            }

            actor->animType = animType;
            actor->blockType = blockType;
            actor->animSpeed = animSpeed;
            actor->posx = actor->pos.x;
            actor->posy = actor->pos.y;
            actor->posz = actor->pos.z;
            m_3dActors.push_back(actor);
            break;
        }
        case 2:
            if (offset + 108 > static_cast<size_t>(size)) {
                return FailWorldLoad(fName, "light_record", offset, size, m_verMajor, m_verMinor);
            }
            offset += 108;
            break;
        case 3:
            if (m_verMajor < 2) {
                LegacySoundSrcInfo legacy{};
                if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, &legacy, sizeof(legacy))) {
                    return FailWorldLoad(fName, "legacy_sound", offset, size, m_verMajor, m_verMinor);
                }

                soundSrcInfo* sound = new soundSrcInfo{};
                std::memcpy(sound->name, legacy.name, sizeof(legacy.name));
                std::memcpy(sound->waveName, legacy.waveName, sizeof(legacy.waveName));
                sound->pos = legacy.pos;
                sound->vol = legacy.vol;
                sound->width = legacy.width;
                sound->height = legacy.height;
                sound->range = legacy.range;
                sound->cycle = 4.0f;
                m_sounds.push_back(sound);
            } else {
                soundSrcInfo* sound = new soundSrcInfo{};
                if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, sound, sizeof(soundSrcInfo))) {
                    delete sound;
                    return FailWorldLoad(fName, "sound", offset, size, m_verMajor, m_verMinor);
                }
                m_sounds.push_back(sound);
            }
            break;
        case 4: {
            effectSrcInfo* effect = new effectSrcInfo{};
            if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, effect, sizeof(effectSrcInfo))) {
                delete effect;
                return FailWorldLoad(fName, "effect", offset, size, m_verMajor, m_verMinor);
            }
            m_particles.push_back(effect);
            break;
        }
        default:
            DbgLog("[WorldRes] unexpected object type file='%s' index=%d/%d type=%d offset=%u ver=%u.%u\n",
                fName ? fName : "(null)",
                objectIndex + 1,
                objectCount,
                type,
                static_cast<unsigned>(offset),
                static_cast<unsigned>(m_verMajor),
                static_cast<unsigned>(m_verMinor));
            return false;
        }
    }

    DbgLog("[WorldRes] loaded file='%s' ver=%u.%u actors=%u effects=%u sounds=%u gnd='%s' attr='%s' scr='%s'\n",
        fName ? fName : "(null)",
        static_cast<unsigned>(m_verMajor),
        static_cast<unsigned>(m_verMinor),
        static_cast<unsigned>(m_3dActors.size()),
        static_cast<unsigned>(m_particles.size()),
        static_cast<unsigned>(m_sounds.size()),
        m_gndFile.c_str(),
        m_attrFile.c_str(),
        m_scrFile.c_str());

    return true;
}

CRes* C3dWorldRes::Clone()
{
    return new C3dWorldRes();
}

void C3dWorldRes::Reset()
{
    for (actorInfo* actor : m_3dActors) {
        delete actor;
    }
    m_3dActors.clear();

    for (effectSrcInfo* effect : m_particles) {
        delete effect;
    }
    m_particles.clear();

    for (soundSrcInfo* sound : m_sounds) {
        delete sound;
    }
    m_sounds.clear();

    m_CalculatedNode = nullptr;
    m_gndFile.clear();
    m_attrFile.clear();
    m_scrFile.clear();
    m_waterLevel = 0.0f;
    m_waterType = 0;
    m_waveHeight = 1.0f;
    m_waveSpeed = 2.0f;
    m_wavePitch = 50.0f;
    m_waterAnimSpeed = 3;
    m_lightLongitude = 45;
    m_lightLatitude = 45;
    m_lightDir = vector3d{ 0.5f, 0.70710677f, 0.5f };
    m_diffuseCol = vector3d{ 1.0f, 1.0f, 1.0f };
    m_ambientCol = vector3d{ 0.3f, 0.3f, 0.3f };
    m_verMajor = 0;
    m_verMinor = 0;
    m_groundTop = -500;
    m_groundBottom = 500;
    m_groundLeft = -500;
    m_groundRight = 500;
}