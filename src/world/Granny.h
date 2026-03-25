#pragma once
#include "Types.h"
#include "res/Res.h"
#include <string>
#include <vector>
#include <list>
#include <map>

//===========================================================================
// Granny 3D SDK Structures (mapped from HighPriest.exe.h)
//===========================================================================

struct granny_file;
struct granny_model;
struct granny_skeleton;
struct granny_bone;
struct granny_mesh;
struct granny_model_instance;
struct granny_world_pose;
struct granny_local_pose;
struct granny_control;
struct granny_vertex_data;
struct granny_tri_topology;
struct granny_material_binding;
struct granny_bone_binding;
struct granny_variant;
struct granny_string_table;
struct granny_art_tool_info;
struct granny_exporter_info;
struct granny_texture;
struct granny_material;
struct granny_animation;
struct granny_track_group;

struct granny_transform {
    unsigned int Flags;
    float Position[3];
    float Orientation[4];
    float ScaleShear[3][3];
};

struct granny_camera {
    float WpOverHp;
    float WrOverHr;
    float WwOverHw;
    float FOVY;
    float NearClipPlane;
    float FarClipPlane;
    float Orientation[4];
    float Position[3];
    float EAR[3];
    float Offset[3];
    float View4x4[4][4];
    float InverseView4x4[4][4];
    float Projection4x4[4][4];
    float InverseProjection4x4[4][4];
};

struct granny_pnt332_vertex {
    float Position[3];
    float Normal[3];
    float UV[2];
};

struct GrannyTexture {
    const char* Name;
    class CTexture* tex;
};

struct GrannyMesh {
    granny_mesh* Mesh;
    struct granny_mesh_binding* Binding;
    struct granny_mesh_deformer* Deformer;
    unsigned short* VerIdx;
    unsigned char* Vertices;
    int TextureCount;
    GrannyTexture** TextureReferences;
};

struct GrannyModel {
    granny_model_instance* GrannyInstance;
    granny_world_pose* WorldPose;
    int MeshCount;
    GrannyMesh* Meshes;
};

struct GrannyScene {
    granny_camera Camera;
    granny_file* LoadedFile;
    int TextureCount;
    GrannyTexture* Textures;
    int ModelCount;
    GrannyModel* Model;
    int MaxBoneCount;
    granny_local_pose* SharedLocalPose;
    int MaxMutableVertexBufferSize;
    int MaxMutableVertexBufferCount;
    int MutableVertexBufferIndex;
    granny_pnt332_vertex* MatVer;
    char BaseFileName[260];
};

//===========================================================================
// Ragnarok Online Granny Resource Wrappers
//===========================================================================

struct HairInfo {
    int nPartMeshIdx;
    int nSubMeshIdx;
    matrix poseMat;
    vector3d poseLoc;
};

class C3dGrannyBoneRes : public CRes {
public:
    C3dGrannyBoneRes() {}
    virtual ~C3dGrannyBoneRes() {}
    virtual bool Load(const char* fName) override { return true; }
    virtual CRes* Clone() override { return new C3dGrannyBoneRes(); }
    virtual void Reset() override {}
};

class C3dGrannyModelRes : public CRes {
public:
    float m_ProjectZ;
    GrannyScene m_Scene;
    granny_model* m_pModel;
    vector3d m_HitareaPos;
    float m_HitRadx;
    float m_HitRady;
    C3dGrannyBoneRes* m_pBoneRes;
    int m_nBoneType;
    HairInfo* m_hair;
    granny_skeleton* m_pSkeleton;

    C3dGrannyModelRes();
    virtual ~C3dGrannyModelRes();
    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;
};

// Phase 3: Granny DLL Wrapper & Client Classes
class CGranny {
public:
    void* m_grannyFile;
    void* m_grannyModel;

    CGranny();
    ~CGranny();
    bool LoadModel(const char* fName, const void* data, int size);
    void Tick(float elapsed);
};
