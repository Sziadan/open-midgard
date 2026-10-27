#include "session/SkillActionInfo2.h"

#include <array>
#include <cstdio>

void RefGetSkillActionInfo2(int skillId, int* beginEffectId, int* motionType, int property, int sjob);

int main()
{
    constexpr std::array<int, 8> properties{ 0, 1, 2, 3, 4, 5, 6, 7 };
    constexpr std::array<int, 3> jobs{ 0, 4016, 9999 };
    for (int skillId = -1; skillId <= 9000; ++skillId) {
        for (int property : properties) {
            for (int job : jobs) {
                int expectedEffect = 0;
                int expectedMotion = 0;
                int actualEffect = 0;
                int actualMotion = 0;
                RefGetSkillActionInfo2(skillId, &expectedEffect, &expectedMotion, property, job);
                GetSkillActionInfo2(skillId, actualEffect, actualMotion, property, job);
                if (expectedEffect != actualEffect || expectedMotion != actualMotion) {
                    std::fprintf(stderr,
                        "skill %d property %d job %d: Ref=(%d,%d), current=(%d,%d)\n",
                        skillId, property, job, expectedEffect, expectedMotion, actualEffect, actualMotion);
                    return 1;
                }
            }
        }
    }
    return 0;
}
