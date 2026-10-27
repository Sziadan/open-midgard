#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

unsigned int g_now = 0;
unsigned int timeGetTime()
{
    return g_now;
}

class CSkill {
public:
    std::string text;
    unsigned int expiresAt = 0;
    int updates = 0;

    void SetChatBubbleText(const std::string& value, unsigned int untilTick)
    {
        text = value;
        expiresAt = untilTick;
        ++updates;
    }
};

#include "SkillTalkboxTextBody.inc"

int main()
{
    CSkill skill;
    g_now = 123456;
    SetSkillTalkboxText(skill, "Talkbox text");
    assert(skill.text == "Talkbox text");
    assert(skill.expiresAt == 128456);
    assert(skill.updates == 1);

    SetSkillTalkboxText(skill, "");
    SetSkillTalkboxText(skill, nullptr);
    assert(skill.text == "Talkbox text");
    assert(skill.expiresAt == 128456);
    assert(skill.updates == 1);

    char fixedPayload[80];
    std::memset(fixedPayload, 'x', sizeof(fixedPayload));
    SetSkillTalkboxText(skill, fixedPayload);
    assert(skill.text.size() == sizeof(fixedPayload));
    assert(skill.text.front() == 'x' && skill.text.back() == 'x');
    assert(skill.expiresAt == 128456);
    assert(skill.updates == 2);
    return 0;
}
