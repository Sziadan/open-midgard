#pragma once
#include "Types.h"
#include "world/GameActor.h"
#include <string>
#include <map>
#include <list>
#include <vector>

// Forward declarations
class CMode;
class C3dGround;
class C3dAttr;
class C3dActor;
class CPlayer;
class CGameObject;
class CGameActor;
class CItem;
class CSkill;
class CView;
class CTexture;
class CGndRes;
class C3dWorldRes;

struct CLMInfo {
    u8 idata[8][8];
    u8 sdata[8][8][3];
};

struct CAttrCell {
    float h1, h2, h3, h4;
    int flag;
};

class C3dAttr : public CRes {
public:
    C3dAttr();
    ~C3dAttr() override;

    bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    CRes* Clone() override;
    void Reset() override;
    float GetHeight(float x, float z) const;

    int m_width;
    int m_height;
    int m_zoom;
    std::vector<CAttrCell> m_cells;
};

struct SceneGraphNode {
    SceneGraphNode();
    ~SceneGraphNode();

    void ClearChildren();
    void Build(int level, int maxLevel);
    void InsertGround(C3dGround* ground, int level, int maxLevel);
    void InsertAttr(C3dAttr* attr, int level, int maxLevel);
    void InsertActor(CGameActor* actor, float x, float z);
    bool RemoveActor(CGameActor* actor);
    std::vector<CGameActor*>* GetActorList(float x, float z);
    const std::vector<CGameActor*>* GetActorList(float x, float z) const;
    SceneGraphNode* FindLeaf(float x, float z);
    const SceneGraphNode* FindLeaf(float x, float z) const;

    SceneGraphNode* m_parent;
    SceneGraphNode* m_child[4];
    struct { vector3d min, max; } m_aabb;
    vector3d m_center;
    vector3d m_halfSize;
    int m_needUpdate;
    std::vector<CGameActor*> m_actorList;
    C3dGround* m_ground;
    tagRECT m_groundArea;
    C3dAttr* m_attr;
    tagRECT m_attrArea;
};

class CLightmapMgr {
public:
    struct CLightmap {
        u8 brightObj[3];
        CTexture* surface;
        vector2d coor[4];
        u32 intensity[4];
    };

    CLightmapMgr();
    ~CLightmapMgr();

    void Reset();
    bool Create(const CGndRes& gnd);
    const CLightmap* GetLightmap(int index) const;

    std::vector<CLightmap> m_lightmaps;
    std::vector<CTexture*> m_lmSurfaces;
    int m_numLightmaps;
    int m_numLmSurfaces;
};

struct CGroundVertex {
    vector3d wvert;
    vector2d uv;
};

struct CGroundSurface {
    int textureId;
    int lightmapId;
    u32 color;
    CTexture* tex;
    CTexture* lmtex;
    vector2d lmuv[4];
    CGroundVertex vertex[4];
};

struct CGroundCell {
    float h[4];
    vector3d watervert[4];
    int topSurfaceId;
    int frontSurfaceId;
    int rightSurfaceId;
};

class C3dGround {
public:
    virtual ~C3dGround();
    bool AssignGnd(const CGndRes& gnd, const vector3d& lightDir, const vector3d& diffuseCol, const vector3d& ambientCol);
    void SetWaterInfo(float waterLevel, int waterType, int waterAnimSpeed, int wavePitch, int waveSpeed, float waveHeight);
    const CGroundCell* GetCell(int x, int y) const;
    const CGroundSurface* GetSurface(int index) const;
    void RenderAttrTile(const matrix& viewMatrix, int attrX, int attrY, u32 color) const;
    void FlushGround(const matrix& viewMatrix);

    C3dAttr* m_attr;
    int m_width;
    int m_height;
    float m_zoom;
    CLightmapMgr m_lightmapMgr;
    int m_numSurfaces;
    float m_waterLevel;
    int m_texAnimCycle;
    int m_wavePitch;
    int m_waveSpeed;
    int m_waterSet;
    float m_waveHeight;
    CTexture* m_waterTex;
    CTexture* m_pBumpMap;
    int m_waterCnt;
    int m_waterOffset;
    u32 m_lastWaterAnimTick;
    u32 m_waterAnimAccumulator;
    int m_isNewVer;
    int m_waterType;
    int m_waterAnimSpeed;
    vector3d m_lightDir;
    vector3d m_diffuseCol;
    vector3d m_ambientCol;
    std::vector<std::string> m_textureNames;
    std::vector<CGroundSurface> m_surfaces;
    std::vector<CGroundCell> m_cells;
};

//===========================================================================
// CWorld  –  Manages all game objects, actors, and the 3D ground
//===========================================================================

class CWorld {
public:
    CWorld();
    virtual ~CWorld();

    struct BillboardScreenEntry {
        CPc* actor;
        float screenY;
        float depthKey;
        float renderX[4];
        float renderY[4];
        float left;
        float top;
        float right;
        float bottom;
        float labelX;
        float labelY;
        float baseX;
        float baseY;
        float baseZ;
        float baseOow;
    };

    CMode* m_curMode;
    std::list<CGameObject*> m_gameObjectList;
    std::list<CGameActor*> m_actorList;
    std::list<CItem*> m_itemList;
    std::list<CSkill*> m_skillList;
    C3dGround* m_ground;
    CPlayer* m_player;
    C3dAttr* m_attr;
    std::vector<C3dActor*> m_bgObjList;
    int m_bgObjCount;
    int m_bgObjThread;
    int m_isPKZone;
    int m_isSiegeMode;
    int m_isBattleFieldMode;
    int m_isEventPVPMode;
    vector3d m_bgLightDir;
    vector3d m_bgDiffuseCol;
    vector3d m_bgAmbientCol;
    SceneGraphNode m_rootNode;
    SceneGraphNode* m_Calculated;
    mutable std::vector<BillboardScreenEntry> m_billboardFrameEntries;
    mutable std::map<u32, size_t> m_billboardFrameEntryByGid;
    mutable matrix m_billboardFrameViewMatrix;
    mutable float m_billboardFrameCameraLongitude;
    mutable float m_billboardFrameZoom;
    mutable bool m_billboardFrameCacheValid;
    mutable bool m_billboardFrameCacheDirty;

    void ClearGround();
    void ClearBackgroundObjects();
    void ClearFixedObjects();
    void ResetSceneGraph();
    void RebuildSceneGraph();
    void InvalidateBillboardFrameCache();
    void EnsureBillboardFrameCache(const matrix& viewMatrix, float cameraLongitude) const;
    const BillboardScreenEntry* FindBillboardFrameEntryByGid(u32 gid) const;
    void UpdateCalculatedNodeForTile(int tileX, int tileY);
    void RegisterActor(CGameActor* actor);
    void UnregisterActor(CGameActor* actor);
    std::vector<CGameActor*>* GetActorsAtWorldPos(float x, float z);
    const std::vector<CGameActor*>* GetActorsAtWorldPos(float x, float z) const;
    bool BuildGroundFromGnd(const CGndRes& gnd,
        const vector3d& lightDir,
        const vector3d& diffuseCol,
        const vector3d& ambientCol,
        float waterLevel,
        int waterType,
        int waterAnimSpeed,
        int wavePitch,
        int waveSpeed,
        float waveHeight);
    bool BuildBackgroundObjects(const C3dWorldRes& worldRes,
        const vector3d& lightDir,
        const vector3d& diffuseCol,
        const vector3d& ambientCol);
    bool AppendBackgroundObjects(const C3dWorldRes& worldRes,
        size_t startIndex,
        size_t maxActors,
        const vector3d& lightDir,
        const vector3d& diffuseCol,
        const vector3d& ambientCol,
        size_t* outNextIndex,
        bool clearExisting);
    bool BuildFixedEffects(const C3dWorldRes& worldRes);
    bool AppendFixedEffects(const C3dWorldRes& worldRes,
        size_t startIndex,
        size_t maxEffects,
        size_t* outNextIndex,
        bool clearExisting);
    void UpdateGameObjects();
    void RenderGameObjects(const matrix& viewMatrix) const;
    void UpdateBackgroundObjects(const matrix* viewMatrix);
    void UpdateActors();
    void RenderActors(const matrix& viewMatrix, float cameraLongitude);
    bool GetPlayerScreenLabel(const matrix& viewMatrix,
        float cameraLongitude,
        int* outLabelX,
        int* outLabelY) const;
    bool GetActorScreenMarker(const matrix& viewMatrix,
        float cameraLongitude,
        u32 gid,
        int* outCenterX,
        int* outTopY,
        int* outLabelY = nullptr) const;
    bool FindHoveredActorScreen(const matrix& viewMatrix,
        float cameraLongitude,
        int screenX,
        int screenY,
        CGameActor** outActor,
        int* outLabelX,
        int* outLabelY) const;
    void RenderBackgroundObjects(const matrix& viewMatrix) const;
};

extern CWorld g_world;
