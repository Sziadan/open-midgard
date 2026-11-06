#pragma once
#include "GameActor.h"
#include <map>
#include <memory>

// A corpse is an independent render object, not an actor or an actor-ID owner.
class CCorpse final : public CRenderObject {
public:
    CCorpse(const vector3d& position, CActRes* act, CSprRes* spr,
        int action, int motion, float rotation);
    ~CCorpse() override;
    CCorpse(const CCorpse&) = delete;
    CCorpse& operator=(const CCorpse&) = delete;

    u8 OnProcess() override;
    u8 ProcessAt(u32 now);
    void Render(matrix* view) override;
    const std::string& ActName() const { return m_actName; }
    const std::string& SprName() const { return m_sprName; }

private:
    CTexture* TextureFor(int clipType, int spriteIndex);
    std::string m_actName;
    std::string m_sprName;
    std::map<std::pair<int, int>, std::unique_ptr<CTexture>> m_textures;
};
