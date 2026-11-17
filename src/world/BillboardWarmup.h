#pragma once

struct BillboardWarmupProgress {
    int motionCount = 0;
    int nextMotion = 0;
    int remaining = 0;

    void Start(int count, int currentMotion)
    {
        motionCount = count;
        nextMotion = (currentMotion + 1) % count;
        remaining = count - (currentMotion >= 0 ? 1 : 0);
    }

    int Next()
    {
        if (remaining <= 0) {
            return -1;
        }
        const int motion = nextMotion;
        nextMotion = (nextMotion + 1) % motionCount;
        --remaining;
        return motion;
    }
};
