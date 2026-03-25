#include "ModelRes.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct face3d_v11 {
    u16 vertindex[3];
    u16 tvertindex[3];
    u16 meshMtlId;
    int twoSide;
};

struct offsetMatrix34 {
    float v11;
    float v12;
    float v13;
    float v21;
    float v22;
    float v23;
    float v31;
    float v32;
    float v33;
    float v41;
    float v42;
    float v43;
};

static_assert(sizeof(face3d_v11) == 20, "face3d_v11 size mismatch");
static_assert(sizeof(offsetMatrix34) == 48, "offsetMatrix34 size mismatch");

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

bool ReadBytes(const unsigned char* buffer, size_t size, size_t* offset, void* outValue, size_t byteCount)
{
    if (!offset || !outValue || *offset + byteCount > size) {
        return false;
    }

    std::memcpy(outValue, buffer + *offset, byteCount);
    *offset += byteCount;
    return true;
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

void NormalizeVector(vector3d* value)
{
    if (!value) {
        return;
    }

    const float lengthSq = value->x * value->x + value->y * value->y + value->z * value->z;
    if (lengthSq <= 1.0e-12f) {
        value->x = 0.0f;
        value->y = 1.0f;
        value->z = 0.0f;
        return;
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    value->x *= invLength;
    value->y *= invLength;
    value->z *= invLength;
}

vector3d Cross(const vector3d& a, const vector3d& b)
{
    return vector3d{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

vector3d Subtract(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x - b.x, a.y - b.y, a.z - b.z };
}

vector3d TransformPoint(const offsetMatrix34& matrix, const vector3d& point)
{
    return vector3d{
        matrix.v11 * point.x + matrix.v21 * point.y + matrix.v31 * point.z + matrix.v41,
        matrix.v12 * point.x + matrix.v22 * point.y + matrix.v32 * point.z + matrix.v42,
        matrix.v13 * point.x + matrix.v23 * point.y + matrix.v33 * point.z + matrix.v43,
    };
}

} // namespace

void C3dPosAnim::Reset()
{
    m_animdata.clear();
}

bool C3dPosAnim::LoadFromBuffer(const unsigned char* buffer, size_t size, size_t* offset)
{
    Reset();

    int count = 0;
    if (!ReadI32(buffer, size, offset, &count) || count < 0) {
        return false;
    }

    m_animdata.resize(static_cast<size_t>(count));
    if (count == 0) {
        return true;
    }

    return ReadBytes(buffer, size, offset, m_animdata.data(), sizeof(posKeyframe) * static_cast<size_t>(count));
}

void C3dRotAnim::Reset()
{
    m_animdata.clear();
}

bool C3dRotAnim::LoadFromBuffer(const unsigned char* buffer, size_t size, size_t* offset)
{
    Reset();

    int count = 0;
    if (!ReadI32(buffer, size, offset, &count) || count < 0) {
        return false;
    }

    m_animdata.resize(static_cast<size_t>(count));
    if (count == 0) {
        return true;
    }

    return ReadBytes(buffer, size, offset, m_animdata.data(), sizeof(rotKeyframe) * static_cast<size_t>(count));
}

void C3dScaleAnim::Reset()
{
    m_animdata.clear();
}

bool C3dScaleAnim::LoadFromBuffer(const unsigned char* buffer, size_t size, size_t* offset)
{
    Reset();

    int count = 0;
    if (!ReadI32(buffer, size, offset, &count) || count < 0) {
        return false;
    }

    m_animdata.resize(static_cast<size_t>(count));
    if (count == 0) {
        return true;
    }

    return ReadBytes(buffer, size, offset, m_animdata.data(), sizeof(scaleKeyframe) * static_cast<size_t>(count));
}

C3dMesh::C3dMesh()
    : m_numVert(0), m_numFace(0), m_numTVert(0), m_parent(nullptr)
{
}

void C3dMesh::Reset()
{
    m_numVert = 0;
    m_numFace = 0;
    m_numTVert = 0;
    m_vert.clear();
    m_faceNormal.clear();
    m_vertNormal.clear();
    m_tvert.clear();
    m_face.clear();
    m_parent = nullptr;
    for (std::vector<int>& group : m_shadeGroup) {
        group.clear();
    }
}

void C3dMesh::UpdateNormal()
{
    m_faceNormal.assign(static_cast<size_t>(m_numFace), vector3d{ 0.0f, 1.0f, 0.0f });
    m_vertNormal.assign(static_cast<size_t>(m_numFace) * 3u, vector3d{ 0.0f, 1.0f, 0.0f });
    for (std::vector<int>& group : m_shadeGroup) {
        group.clear();
    }

    for (int faceIndex = 0; faceIndex < m_numFace; ++faceIndex) {
        const face3d& face = m_face[static_cast<size_t>(faceIndex)];
        if (face.vertindex[0] >= m_vert.size()
            || face.vertindex[1] >= m_vert.size()
            || face.vertindex[2] >= m_vert.size()) {
            continue;
        }

        const vector3d edgeA = Subtract(m_vert[face.vertindex[1]], m_vert[face.vertindex[0]]);
        const vector3d edgeB = Subtract(m_vert[face.vertindex[2]], m_vert[face.vertindex[0]]);
        vector3d normal = Cross(edgeA, edgeB);
        NormalizeVector(&normal);
        m_faceNormal[static_cast<size_t>(faceIndex)] = normal;

        const size_t normalBaseIndex = static_cast<size_t>(faceIndex) * 3u;
        for (size_t corner = 0; corner < 3; ++corner) {
            m_vertNormal[normalBaseIndex + corner] = normal;
        }

        if (face.smoothGroup >= 0 && face.smoothGroup < static_cast<int>(m_shadeGroup.size())) {
            m_shadeGroup[static_cast<size_t>(face.smoothGroup)].push_back(faceIndex);
        }
    }

    for (const std::vector<int>& group : m_shadeGroup) {
        if (group.empty()) {
            continue;
        }

        for (int vertexIndex = 0; vertexIndex < m_numVert; ++vertexIndex) {
            vector3d smoothedNormal{ 0.0f, 0.0f, 0.0f };
            bool found = false;

            for (int faceIndex : group) {
                if (faceIndex < 0 || faceIndex >= m_numFace) {
                    continue;
                }

                const face3d& face = m_face[static_cast<size_t>(faceIndex)];
                if (face.vertindex[0] != static_cast<u16>(vertexIndex)
                    && face.vertindex[1] != static_cast<u16>(vertexIndex)
                    && face.vertindex[2] != static_cast<u16>(vertexIndex)) {
                    continue;
                }

                const vector3d& faceNormal = m_faceNormal[static_cast<size_t>(faceIndex)];
                smoothedNormal.x += faceNormal.x;
                smoothedNormal.y += faceNormal.y;
                smoothedNormal.z += faceNormal.z;
                found = true;
            }

            if (!found) {
                continue;
            }

            NormalizeVector(&smoothedNormal);
            for (int faceIndex : group) {
                if (faceIndex < 0 || faceIndex >= m_numFace) {
                    continue;
                }

                const face3d& face = m_face[static_cast<size_t>(faceIndex)];
                const size_t normalBaseIndex = static_cast<size_t>(faceIndex) * 3u;
                for (size_t corner = 0; corner < 3; ++corner) {
                    if (face.vertindex[corner] == static_cast<u16>(vertexIndex)) {
                        m_vertNormal[normalBaseIndex + corner] = smoothedNormal;
                    }
                }
            }
        }
    }
}

C3dNodeRes::C3dNodeRes()
    : scene(nullptr), parent(nullptr), mesh(nullptr), rotangle(0.0f), alpha(0xFF)
{
    pos[0] = pos[1] = pos[2] = 0.0f;
    rotaxis[0] = 0.0f;
    rotaxis[1] = 1.0f;
    rotaxis[2] = 0.0f;
    scale[0] = scale[1] = scale[2] = 1.0f;
}

C3dNodeRes::~C3dNodeRes()
{
    Reset();
}

void C3dNodeRes::Reset()
{
    child.clear();
    textureIndices.clear();
    posanim.Reset();
    rotanim.Reset();
    scaleanim.Reset();
    delete mesh;
    mesh = nullptr;
    parent = nullptr;
    scene = nullptr;
}

C3dModelRes::C3dModelRes()
{
    Reset();
}

C3dModelRes::~C3dModelRes()
{
    Reset();
}

bool C3dModelRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    Reset();

    if (!buffer || size < 6) {
        return false;
    }

    if (std::memcmp(buffer, "GRSM", 4) != 0) {
        return false;
    }

    size_t offset = 4;
    u8 verMajor = 0;
    u8 verMinor = 0;
    if (!ReadU8(buffer, static_cast<size_t>(size), &offset, &verMajor)
        || !ReadU8(buffer, static_cast<size_t>(size), &offset, &verMinor)) {
        return false;
    }

    if (verMajor != 1 || verMinor > 5) {
        return false;
    }

    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_animLen)
        || !ReadI32(buffer, static_cast<size_t>(size), &offset, &m_shadeType)) {
        return false;
    }

    if (verMinor < 4) {
        m_alpha = 0xFF;
    } else if (!ReadU8(buffer, static_cast<size_t>(size), &offset, &m_alpha)) {
        return false;
    }

    unsigned char reserved[16]{};
    if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, reserved, sizeof(reserved))) {
        return false;
    }

    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &m_numMaterials) || m_numMaterials < 0) {
        return false;
    }

    m_materialNames.resize(static_cast<size_t>(m_numMaterials));
    for (int materialIndex = 0; materialIndex < m_numMaterials; ++materialIndex) {
        m_materialNames[static_cast<size_t>(materialIndex)] = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
        if (offset > static_cast<size_t>(size)) {
            return false;
        }
    }

    const std::string mainNode = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
    if (offset > static_cast<size_t>(size)) {
        return false;
    }

    int numNodes = 0;
    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &numNodes) || numNodes < 0) {
        return false;
    }

    for (int nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex) {
        C3dNodeRes* node = new C3dNodeRes();
        node->scene = this;
        node->alpha = m_alpha;
        node->name = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
        node->parentname = ReadFixedString(buffer, static_cast<size_t>(size), &offset, 40);
        if (offset > static_cast<size_t>(size)) {
            delete node;
            return false;
        }

        int numTexture = 0;
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &numTexture) || numTexture < 0) {
            delete node;
            return false;
        }
        node->textureIndices.resize(static_cast<size_t>(numTexture));
        for (int textureIndex = 0; textureIndex < numTexture; ++textureIndex) {
            if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &node->textureIndices[static_cast<size_t>(textureIndex)])) {
                delete node;
                return false;
            }
        }

        offsetMatrix34 offsetTM{};
        if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, &offsetTM, sizeof(offsetTM))
            || !ReadBytes(buffer, static_cast<size_t>(size), &offset, node->pos, sizeof(node->pos))
            || !ReadF32(buffer, static_cast<size_t>(size), &offset, &node->rotangle)
            || !ReadBytes(buffer, static_cast<size_t>(size), &offset, node->rotaxis, sizeof(node->rotaxis))
            || !ReadBytes(buffer, static_cast<size_t>(size), &offset, node->scale, sizeof(node->scale))) {
            delete node;
            return false;
        }

        node->mesh = new C3dMesh();
        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &node->mesh->m_numVert) || node->mesh->m_numVert < 0) {
            delete node;
            return false;
        }
        node->mesh->m_vert.resize(static_cast<size_t>(node->mesh->m_numVert));
        if (node->mesh->m_numVert > 0
            && !ReadBytes(buffer, static_cast<size_t>(size), &offset, node->mesh->m_vert.data(), sizeof(vector3d) * static_cast<size_t>(node->mesh->m_numVert))) {
            delete node;
            return false;
        }
        for (vector3d& vertex : node->mesh->m_vert) {
            vertex = TransformPoint(offsetTM, vertex);
        }

        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &node->mesh->m_numTVert) || node->mesh->m_numTVert < 0) {
            delete node;
            return false;
        }
        node->mesh->m_tvert.resize(static_cast<size_t>(node->mesh->m_numTVert));
        if (verMinor == 1) {
            for (int texVertIndex = 0; texVertIndex < node->mesh->m_numTVert; ++texVertIndex) {
                texCoor uv{};
                if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, &uv, sizeof(uv))) {
                    delete node;
                    return false;
                }
                tvertex3d& tvert = node->mesh->m_tvert[static_cast<size_t>(texVertIndex)];
                tvert.color = 0xFFFFFFFFu;
                tvert.u = uv.u;
                tvert.v = uv.v;
            }
        } else if (node->mesh->m_numTVert > 0
            && !ReadBytes(buffer, static_cast<size_t>(size), &offset, node->mesh->m_tvert.data(), sizeof(tvertex3d) * static_cast<size_t>(node->mesh->m_numTVert))) {
            delete node;
            return false;
        }

        if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &node->mesh->m_numFace) || node->mesh->m_numFace < 0) {
            delete node;
            return false;
        }
        node->mesh->m_face.resize(static_cast<size_t>(node->mesh->m_numFace));
        if (verMinor == 1) {
            for (int faceIndex = 0; faceIndex < node->mesh->m_numFace; ++faceIndex) {
                face3d_v11 legacyFace{};
                if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, &legacyFace, sizeof(legacyFace))) {
                    delete node;
                    return false;
                }
                face3d& face = node->mesh->m_face[static_cast<size_t>(faceIndex)];
                std::memcpy(face.vertindex, legacyFace.vertindex, sizeof(face.vertindex));
                std::memcpy(face.tvertindex, legacyFace.tvertindex, sizeof(face.tvertindex));
                face.meshMtlId = legacyFace.meshMtlId;
                face.twoSide = legacyFace.twoSide;
                face.smoothGroup = 0;
            }
        } else if (node->mesh->m_numFace > 0
            && !ReadBytes(buffer, static_cast<size_t>(size), &offset, node->mesh->m_face.data(), sizeof(face3d) * static_cast<size_t>(node->mesh->m_numFace))) {
            delete node;
            return false;
        }

        std::sort(node->mesh->m_face.begin(), node->mesh->m_face.end(), [](const face3d& a, const face3d& b) {
            if (a.meshMtlId != b.meshMtlId) {
                return a.meshMtlId < b.meshMtlId;
            }
            if (a.twoSide != b.twoSide) {
                return a.twoSide < b.twoSide;
            }
            return a.smoothGroup < b.smoothGroup;
        });
        node->mesh->UpdateNormal();

        if (verMinor >= 5 && !node->posanim.LoadFromBuffer(buffer, static_cast<size_t>(size), &offset)) {
            delete node;
            return false;
        }
        if (!node->rotanim.LoadFromBuffer(buffer, static_cast<size_t>(size), &offset)) {
            delete node;
            return false;
        }
        if (verMinor >= 5 && !node->scaleanim.LoadFromBuffer(buffer, static_cast<size_t>(size), &offset)) {
            delete node;
            return false;
        }

        m_meshList[node->name] = node->mesh;
        m_objectList.push_back(node);
    }

    if (verMinor < 5) {
        C3dNodeRes* rootNode = FindNode(mainNode.c_str());
        if (rootNode && !rootNode->posanim.LoadFromBuffer(buffer, static_cast<size_t>(size), &offset)) {
            return false;
        }
    }

    for (C3dNodeRes* node : m_objectList) {
        if (node->parentname.empty()) {
            continue;
        }
        node->parent = FindNode(node->parentname.c_str());
        if (node->parent) {
            node->parent->child.push_back(node);
        }
    }

    if (!mainNode.empty()) {
        m_rootObjList.push_back(mainNode);
    }

    int numVol = 0;
    if (!ReadI32(buffer, static_cast<size_t>(size), &offset, &numVol) || numVol < 0) {
        return false;
    }

    if (verMinor == 2) {
        for (int volumeIndex = 0; volumeIndex < numVol; ++volumeIndex) {
            CVolumeBox* volume = new CVolumeBox();
            volume->flag = 0;
            if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, &volume->m_size, sizeof(vector3d))
                || !ReadBytes(buffer, static_cast<size_t>(size), &offset, &volume->m_pos, sizeof(vector3d))
                || !ReadBytes(buffer, static_cast<size_t>(size), &offset, &volume->m_rot, sizeof(vector3d))) {
                delete volume;
                return false;
            }
            m_volumeBoxList.push_back(volume);
        }
    } else if (verMinor >= 3) {
        for (int volumeIndex = 0; volumeIndex < numVol; ++volumeIndex) {
            CVolumeBox* volume = new CVolumeBox();
            if (!ReadBytes(buffer, static_cast<size_t>(size), &offset, &volume->m_size, sizeof(vector3d))
                || !ReadBytes(buffer, static_cast<size_t>(size), &offset, &volume->m_pos, sizeof(vector3d))
                || !ReadBytes(buffer, static_cast<size_t>(size), &offset, &volume->m_rot, sizeof(vector3d))
                || !ReadI32(buffer, static_cast<size_t>(size), &offset, &volume->flag)) {
                delete volume;
                return false;
            }
            if (volume->flag == 0) {
                volume->m_size.x += 3.0f;
                volume->m_size.z += 3.0f;
            }
            m_volumeBoxList.push_back(volume);
        }
    }

    std::memset(name, 0, sizeof(name));
    if (fName && *fName) {
        const char* baseName = std::strrchr(fName, '\\');
        baseName = baseName ? baseName + 1 : fName;
        std::strncpy(name, baseName, sizeof(name) - 1);
    }

    return true;
}

CRes* C3dModelRes::Clone()
{
    return new C3dModelRes();
}

void C3dModelRes::Reset()
{
    for (C3dNodeRes* node : m_objectList) {
        delete node;
    }
    m_objectList.clear();

    for (CVolumeBox* volume : m_volumeBoxList) {
        delete volume;
    }
    m_volumeBoxList.clear();

    m_rootObjList.clear();
    m_meshList.clear();
    m_materialNames.clear();
    m_numMaterials = 0;
    m_shadeType = 0;
    m_animLen = 0;
    m_alpha = 0xFF;
    std::memset(name, 0, sizeof(name));
}

C3dNodeRes* C3dModelRes::FindNode(const char* targetName)
{
    if (!targetName || !*targetName) {
        return nullptr;
    }

    for (C3dNodeRes* node : m_objectList) {
        if (node && node->name == targetName) {
            return node;
        }
    }
    return nullptr;
}

const C3dNodeRes* C3dModelRes::FindNode(const char* targetName) const
{
    if (!targetName || !*targetName) {
        return nullptr;
    }

    for (const C3dNodeRes* node : m_objectList) {
        if (node && node->name == targetName) {
            return node;
        }
    }
    return nullptr;
}