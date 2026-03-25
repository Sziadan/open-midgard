#pragma once

#include "Types.h"
#include "res/ModelRes.h"

#include <array>
#include <string>
#include <vector>

class CTexture;

class C3dNode {
public:
    struct FaceColor {
        std::array<u32, 3> color;
    };

    C3dNode();
    ~C3dNode();

    bool AssignModel(const C3dNodeRes& nodeRes, const C3dModelRes& modelRes);
    void SetFrame(int frame);
    void UpdateWorldMatrix(const matrix& parentWorld);
    void UpdateBounds(vector3d* minBounds, vector3d* maxBounds, const matrix& parentWorld) const;
    void UpdateVertexColor(const matrix& parentWorld,
        const vector3d& lightDir,
        const vector3d& diffuseCol,
        const vector3d& ambientCol);
    void Render(const matrix& parentWorld, const matrix& viewMatrix, bool forceDoubleSided) const;

    std::string m_name;
    C3dMesh* m_mesh;
    std::vector<CTexture*> m_textures;
    std::vector<FaceColor> m_colorInfo;
    std::vector<C3dNode*> m_children;
    std::vector<posKeyframe> m_posAnim;
    std::vector<rotKeyframe> m_rotAnim;
    std::vector<scaleKeyframe> m_scaleAnim;
    vector3d m_basePos;
    vector3d m_baseScale;
    rotKeyframe m_baseRot;
    int m_shadeType;
    matrix m_ltm;
    matrix m_wtm;
    float m_opacity;
};

class C3dActor {
public:
    C3dActor();
    ~C3dActor();

    bool AssignModel(const C3dModelRes& modelRes);
    void SetFrame(int frame);
    void AdvanceFrame();
    void UpdateMatrix();
    void UpdateBound();
    void UpdateVertexColor(const vector3d& lightDir,
        const vector3d& diffuseCol,
        const vector3d& ambientCol);
    void Render(const matrix& viewMatrix) const;

    char m_name[128];
    C3dNode* m_node;
    matrix m_wtm;
    vector3d m_pos;
    vector3d m_rot;
    vector3d m_scale;
    vector3d m_posOffset;
    float m_animSpeed;
    int m_animType;
    int m_animLen;
    int m_curMotion;
    int m_blockType;
    int m_isHideCheck;
    int m_isMatrixNeedUpdate;
    float m_boundRadius;
    bool m_forceDoubleSided;
    std::string m_debugModelPath;
    std::string m_debugNodeName;
};