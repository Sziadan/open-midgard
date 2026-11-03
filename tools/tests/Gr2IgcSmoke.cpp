// Smoke-test native GR2 file load + IGC texture decode. Writes packed pixels for hashing.
#include "res/gr2/Gr2Extract.h"
#include "res/gr2/Gr2File.h"
#include "res/gr2/Gr2TypeTree.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> ReadFile(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), {});
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: Gr2IgcSmoke <gr2> <out.pixels>\n";
        return 2;
    }
    const auto bytes = ReadFile(argv[1]);
    ro::gr2::File file;
    std::string error;
    if (!ro::gr2::ParseFile(bytes, file, error)) {
        std::cerr << "ParseFile: " << error << "\n";
        return 1;
    }
    ro::gr2::LoadedGr2 loaded;
    if (!ro::gr2::LoadGr2(file, loaded, error)) {
        std::cerr << "LoadGr2: " << error << "\n";
        return 1;
    }
    std::vector<ro::gr2::ExtractedTexture> textures;
    if (!ro::gr2::ExtractTextures(loaded, textures, error)) {
        std::cerr << "ExtractTextures: " << error << "\n";
        return 1;
    }

    std::ofstream out(argv[2], std::ios::binary);
    for (const auto& image : textures) {
        const size_t count = size_t(image.width) * size_t(image.height);
        if (image.rgba.size() != count * 4) {
            std::cerr << "bad rgba size for " << image.name << "\n";
            return 1;
        }
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* b = image.rgba.data() + i * 4;
            const uint32_t packed =
                (uint32_t(b[3]) << 24) | (uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2];
            out.write(reinterpret_cast<const char*>(&packed), 4);
        }
    }
    std::cout << "OK images=" << textures.size() << "\n";
    return 0;
}
