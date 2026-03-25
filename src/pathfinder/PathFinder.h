#pragma once

class C3dAttr;
struct CPathInfo;

class CPathFinder {
public:
    CPathFinder();
    ~CPathFinder();

    void Reset();
    void SetMap(C3dAttr* newMap);
    bool FindPath(unsigned int startTime,
        int sx,
        int sy,
        int cellX,
        int cellY,
        int dx,
        int dy,
        int speedFactor,
        CPathInfo* pathInfo);

private:
    bool IsWalkable(int x, int y) const;
    bool IsConnected(int sx, int sy, int dx, int dy) const;

    C3dAttr* m_map;
};

extern CPathFinder g_pathFinder;
