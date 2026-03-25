#pragma once

#include "Res.h"

#include <array>
#include <list>
#include <map>
#include <string>
#include <vector>

struct texCoor {
    float u;
    float v;
};

struct tvertex3d {
    u32 color;
    float u;
    float v;
};

struct face3d {
    u16 vertindex[3];
    u16 tvertindex[3];
    u16 meshMtlId;
    int twoSide;
    int smoothGroup;
};

struct posKeyframe {
    int frame;
    float px;
    float py;
    float pz;
};

struct rotKeyframe {
    int frame;
    float qx;
    float qy;
    float qz;
    float qw;
};

struct scaleKeyframe {
    int frame;
    float sx;
    float sy;
    float sz;
    float ax;
    float ay;
    float az;
    float angle;
};

class C3dPosAnim {
public:
    void Reset();
    bool LoadFromBuffer(const unsigned char* buffer, size_t size, size_t* offset);

    std::vector<posKeyframe> m_animdata;
};

class C3dRotAnim {
public:
    void Reset();
    bool LoadFromBuffer(const unsigned char* buffer, size_t size, size_t* offset);

    std::vector<rotKeyframe> m_animdata;
};

class C3dScaleAnim {
public:
    void Reset();
    bool LoadFromBuffer(const unsigned char* buffer, size_t size, size_t* offset);

    std::vector<scaleKeyframe> m_animdata;
};

class C3dMesh {
public:
    C3dMesh();

    void Reset();
    void UpdateNormal();

    int m_numVert;
    int m_numFace;
    int m_numTVert;
    std::vector<vector3d> m_vert;
    std::vector<vector3d> m_faceNormal;
    std::vector<vector3d> m_vertNormal;
    std::vector<tvertex3d> m_tvert;
    std::vector<face3d> m_face;
    C3dMesh* m_parent;
    std::array<std::vector<int>, 32> m_shadeGroup;
};

class CVolumeBox {
public:
    vector3d m_size;
    vector3d m_pos;
    vector3d m_rot;
    int flag;
};

class C3dModelRes;

class C3dNodeRes {
public:
    C3dNodeRes();
    ~C3dNodeRes();

    void Reset();

    std::string name;
    std::string parentname;
    C3dModelRes* scene;
    C3dNodeRes* parent;
    std::list<C3dNodeRes*> child;
    C3dMesh* mesh;
    std::vector<int> textureIndices;
    float pos[3];
    float rotaxis[3];
    float rotangle;
    float scale[3];
    u8 alpha;
    C3dPosAnim posanim;
    C3dRotAnim rotanim;
    C3dScaleAnim scaleanim;
};

class C3dModelRes : public CRes {
public:
    C3dModelRes();
    ~C3dModelRes() override;

    bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    CRes* Clone() override;
    void Reset() override;

    C3dNodeRes* FindNode(const char* name);
    const C3dNodeRes* FindNode(const char* name) const;

    int m_numMaterials;
    std::vector<std::string> m_materialNames;
    std::list<C3dNodeRes*> m_objectList;
    std::list<std::string> m_rootObjList;
    std::map<std::string, C3dMesh*> m_meshList;
    char name[80];
    int m_shadeType;
    int m_animLen;
    std::list<CVolumeBox*> m_volumeBoxList;
    u8 m_alpha;
};

static_assert(sizeof(tvertex3d) == 12, "tvertex3d size mismatch");
static_assert(sizeof(face3d) == 24, "face3d size mismatch");
static_assert(sizeof(posKeyframe) == 16, "posKeyframe size mismatch");
static_assert(sizeof(rotKeyframe) == 20, "rotKeyframe size mismatch");
static_assert(sizeof(scaleKeyframe) == 32, "scaleKeyframe size mismatch");