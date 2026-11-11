#pragma once

class CWorld;
struct CPathInfo;

class CPathFinder {
public:
    CPathFinder();
    ~CPathFinder();

    void Reset();
    void SetMap(const CWorld* world);
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

    const CWorld* m_world;
};

extern CPathFinder g_pathFinder;
