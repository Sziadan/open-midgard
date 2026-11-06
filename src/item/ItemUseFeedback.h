#pragma once

namespace ro::item {
// Ref CGameMode::MakePotionEffect: selection is separate from actor ownership.
struct UseFeedback {
    int effectId=-1;
    const char* sound=nullptr;
    unsigned noticeMessageId=0;
    int extraEffectId=-1;
};
UseFeedback GetUseFeedback(unsigned itemId);
}
