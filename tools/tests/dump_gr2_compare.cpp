#include "res/Gr2Reader.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    const std::string name = argv[1];
    const std::string outDir = argv[2];
    std::ifstream file("Ref/GRF-Content/data/model/3dmob/" + name, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
    ro::gr2::Asset asset;
    std::string error;
    if (!ro::gr2::Decode(bytes, asset, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    for (size_t i = 0; i < asset.images.size(); ++i) {
        const auto& image = asset.images[i];
        const std::string path = outDir + "/" + name + ".img" + std::to_string(i) + ".bgra";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(image.pixels.data()),
            static_cast<std::streamsize>(image.pixels.size() * 4));
        std::printf("img %zu %dx%d alpha=%d bytes=%zu\n", i, image.width, image.height, image.alpha ? 1 : 0,
            image.pixels.size() * 4);
    }

    std::ofstream meshOut(outDir + "/" + name + ".mesh.bin", std::ios::binary);
    const uint32_t meshCount = static_cast<uint32_t>(asset.meshes.size());
    meshOut.write(reinterpret_cast<const char*>(&meshCount), 4);
    for (const auto& mesh : asset.meshes) {
        const uint32_t vcount = static_cast<uint32_t>(mesh.vertices.size());
        const uint32_t icount = static_cast<uint32_t>(mesh.indices.size());
        const uint32_t bcount = static_cast<uint32_t>(mesh.batches.size());
        meshOut.write(reinterpret_cast<const char*>(&vcount), 4);
        meshOut.write(reinterpret_cast<const char*>(&icount), 4);
        meshOut.write(reinterpret_cast<const char*>(&bcount), 4);
        for (const auto& vertex : mesh.vertices) {
            meshOut.write(reinterpret_cast<const char*>(vertex.uv.data()), 8);
        }
        meshOut.write(reinterpret_cast<const char*>(mesh.indices.data()),
            static_cast<std::streamsize>(mesh.indices.size() * 4));
        for (const auto& batch : mesh.batches) {
            int values[3] = {batch.texture, batch.first, batch.count};
            meshOut.write(reinterpret_cast<const char*>(values), sizeof(values));
        }
    }
    return 0;
}
