#include "session/SkillActionInfo.h"

#include <cstdio>

void RefGetSkillActionInfo(int skillId, int* beginEffectId, int* motionType);

int main()
{
    for (int skillId = -1; skillId <= 9000; ++skillId) {
        int expectedEffect = 0;
        int expectedMotion = 0;
        int actualEffect = 0;
        int actualMotion = 0;
        RefGetSkillActionInfo(skillId, &expectedEffect, &expectedMotion);
        GetSkillActionInfo(skillId, actualEffect, actualMotion);
        if (expectedEffect != actualEffect || expectedMotion != actualMotion) {
            std::fprintf(stderr,
                "skill %d: Ref=(%d,%d), current=(%d,%d)\n",
                skillId, expectedEffect, expectedMotion, actualEffect, actualMotion);
            return 1;
        }
    }
    return 0;
}
