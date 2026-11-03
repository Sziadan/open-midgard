#include "res/Gr2Reader.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    const char* files[] = {
        "Ref/GRF-Content/data/model/3dmob/aguardian90_8.gr2",
        "Ref/GRF-Content/data/model/3dmob/kguardian90_7.gr2",
        "Ref/GRF-Content/data/model/3dmob/sguardian90_9.gr2",
        "Ref/GRF-Content/data/model/3dmob/empelium90_0.gr2",
        "Ref/GRF-Content/data/model/3dmob/guildflag90_1.gr2",
        "Ref/GRF-Content/data/model/3dmob/treasurebox_2.gr2",
        "Ref/GRF-Content/data/model/3dmob_bone/1_attack.gr2",
        "Ref/GRF-Content/data/model/3dmob_bone/2_damage.gr2",
        "Ref/GRF-Content/data/model/3dmob_bone/7_move.gr2",
        nullptr,
    };
    for (const char** path = files; *path; ++path) {
        std::ifstream file(*path, std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
        ro::gr2::Asset asset;
        std::string error;
        const bool animationOnly = std::string(*path).find("3dmob_bone") != std::string::npos;
        if (!ro::gr2::Decode(bytes, asset, error, animationOnly)) {
            std::cout << *path << " FAIL " << error << "\n";
            continue;
        }
        std::cout << *path << " bones=" << asset.bones.size() << " meshes=" << asset.meshes.size()
                  << " images=" << asset.images.size() << " anims=" << asset.animations.size();
        for (const auto& image : asset.images) {
            std::cout << " img=" << image.name << " " << image.width << "x" << image.height
                      << " a=" << image.alpha;
        }
        if (!asset.animations.empty()) {
            const auto& animation = asset.animations[0];
            std::cout << " dur=" << animation.duration << " tracks=" << animation.tracks.size();
        }
        std::cout << "\n";
    }
    return 0;
}
