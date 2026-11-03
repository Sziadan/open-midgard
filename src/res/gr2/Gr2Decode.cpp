// Native GR2 asset decoding. MIT attribution: third_party/gr2-reader/LICENSE.
#include "../Gr2Reader.h"
#include "Gr2Extract.h"
#include "Gr2File.h"
#include "Gr2TypeTree.h"
#include <algorithm>
#include <unordered_map>

namespace ro::gr2 {
namespace {

Curve ToCurve(const ExtractedCurve& src)
{
    Curve curve;
    curve.degree = src.degree;
    curve.dimension = src.dimension;
    curve.knots = src.knots;
    curve.controls = src.controls;
    curve.sample = src.sample;
    return curve;
}

Transform ToTransform(const Gr2Transform& src)
{
    Transform result;
    for (int i = 0; i < 3; ++i) result.position[size_t(i)] = src.position[size_t(i)];
    for (int i = 0; i < 4; ++i) result.orientation[size_t(i)] = src.orientation[size_t(i)];
    for (int i = 0; i < 9; ++i) result.scale[size_t(i)] = src.scaleShear[size_t(i)];
    return result;
}

} // namespace

bool DecodeNative(const std::vector<unsigned char>& bytes, Asset& asset, std::string& error,
    bool animationOnly)
{
    asset = {};
    error.clear();
    if (bytes.empty() || bytes.size() > 16u * 1024u * 1024u) {
        error = "GR2 input size exceeds supported limits";
        return false;
    }

    try {
        File file;
        if (!ParseFile(bytes, file, error)) return false;
        LoadedGr2 loaded;
        if (!LoadGr2(file, loaded, error)) return false;

        std::vector<ExtractedAnimation> animations;
        if (!ExtractAnimations(loaded, animations, error)) return false;
        for (const auto& src : animations) {
            Animation animation;
            animation.duration = src.duration;
            for (const auto& track : src.tracks) {
                animation.tracks[track.name] = {ToCurve(track.position), ToCurve(track.orientation),
                    ToCurve(track.scale)};
                if (animation.tracks.size() > 4096) {
                    error = "GR2 track count exceeds limit";
                    return false;
                }
            }
            asset.animations.push_back(std::move(animation));
            if (asset.animations.size() > 64) {
                error = "GR2 animation count exceeds limit";
                return false;
            }
        }
        if (animationOnly) return true;

        std::vector<ExtractedSkeleton> skeletons;
        if (!ExtractSkeletons(loaded, skeletons, error)) return false;
        std::vector<ExtractedMesh> meshes;
        if (!ExtractMeshes(loaded, meshes, error)) return false;
        std::vector<ExtractedTexture> textures;
        if (!ExtractTextures(loaded, textures, error)) return false;
        std::vector<ExtractedModel> models;
        if (!ExtractModels(loaded, models, error)) return false;

        int skeletonIndex = 0;
        if (!models.empty() && models[0].skeletonIdx >= 0) skeletonIndex = models[0].skeletonIdx;
        if (skeletonIndex < 0 || skeletonIndex >= int(skeletons.size())) {
            error = "GR2 model skeleton is missing";
            return false;
        }
        const auto& skeleton = skeletons[size_t(skeletonIndex)];
        if (skeleton.bones.empty() || skeleton.bones.size() > 1024) {
            error = "GR2 bone count is invalid";
            return false;
        }
        std::unordered_map<std::string, int> boneIndices;
        for (const auto& src : skeleton.bones) {
            Bone bone;
            bone.name = src.name;
            bone.parent = src.parent;
            if (bone.parent >= int(asset.bones.size())) {
                error = "GR2 bone hierarchy is not parent-first";
                return false;
            }
            bone.bind = ToTransform(src.bind);
            bone.inverse = src.inverse;
            boneIndices[bone.name] = int(asset.bones.size());
            asset.bones.push_back(std::move(bone));
        }

        if (textures.size() > 64) {
            error = "GR2 texture count exceeds limit";
            return false;
        }
        std::unordered_map<std::string, int> textureIndices;
        for (const auto& src : textures) {
            Image image;
            image.name = src.name;
            image.width = src.width;
            image.height = src.height;
            image.alpha = src.alpha;
            if (image.width <= 0 || image.height <= 0 || image.width > 4096 || image.height > 4096) {
                error = "GR2 texture dimensions are invalid";
                return false;
            }
            const size_t count = size_t(image.width) * size_t(image.height);
            if (src.rgba.size() != count * 4) {
                error = "GR2 texture byte count is invalid";
                return false;
            }
            image.pixels.resize(count);
            for (size_t i = 0; i < count; ++i) {
                const uint8_t* b = src.rgba.data() + i * 4;
                image.pixels[i] = (uint32_t(b[3]) << 24) | (uint32_t(b[0]) << 16) |
                    (uint32_t(b[1]) << 8) | b[2];
            }
            textureIndices[image.name] = int(asset.images.size());
            asset.images.push_back(std::move(image));
        }

        if (meshes.size() > 256) {
            error = "GR2 mesh count exceeds limit";
            return false;
        }
        std::vector<int> order;
        if (!models.empty()) {
            for (int meshIndex : models[0].meshIndices) order.push_back(meshIndex);
        }
        if (order.empty()) {
            for (int i = 0; i < int(meshes.size()); ++i) order.push_back(i);
        }

        for (int meshIndex : order) {
            if (meshIndex < 0 || meshIndex >= int(meshes.size())) {
                error = "GR2 mesh binding is out of range";
                return false;
            }
            const auto& src = meshes[size_t(meshIndex)];
            Mesh mesh;
            if (src.vertices.size() > 1000000) {
                error = "GR2 vertex array dimensions differ";
                return false;
            }
            std::vector<int> bindings;
            for (const std::string& boneName : src.boneBindings) {
                const auto found = boneIndices.find(boneName);
                if (found == boneIndices.end()) {
                    error = "GR2 mesh references an unknown skeleton bone";
                    return false;
                }
                bindings.push_back(found->second);
            }
            mesh.vertices.resize(src.vertices.size());
            for (size_t index = 0; index < src.vertices.size(); ++index) {
                auto& vertex = mesh.vertices[index];
                vertex.position = src.vertices[index].position;
                vertex.normal = src.vertices[index].normal;
                vertex.uv = src.vertices[index].uv;
                if (src.vertices[index].weights.size() > 16) {
                    error = "GR2 vertex has too many bone influences";
                    return false;
                }
                for (const auto& influence : src.vertices[index].weights) {
                    if (influence.first < 0 || influence.first >= int(bindings.size()) ||
                        !(influence.second >= 0.f && influence.second <= 1.f)) {
                        error = "GR2 skin weight is invalid";
                        return false;
                    }
                    if (influence.second > 0.f)
                        vertex.weights.push_back({bindings[size_t(influence.first)], influence.second});
                }
                if (vertex.weights.empty() && !bindings.empty())
                    vertex.weights.push_back({bindings[0], 1.f});
            }
            if (src.indices.size() > 3000000 || src.indices.size() % 3) {
                error = "GR2 index array is invalid";
                return false;
            }
            for (int number : src.indices) {
                if (number < 0 || number >= int(src.vertices.size())) {
                    error = "GR2 vertex index is out of range";
                    return false;
                }
                mesh.indices.push_back(number);
            }
            if (src.materialTextures.empty()) {
                asset.meshes.push_back(std::move(mesh));
                continue;
            }
            for (const auto& batchSrc : src.batches) {
                Batch batch;
                if (batchSrc.materialIndex < 0 ||
                    batchSrc.materialIndex >= int(src.materialTextures.size())) {
                    error = "GR2 material index is out of range";
                    return false;
                }
                const auto texture = textureIndices.find(batchSrc.textureFile);
                if (texture != textureIndices.end()) batch.texture = texture->second;
                batch.first = batchSrc.first;
                batch.count = batchSrc.count;
                if (batch.first < 0 || batch.count < 0 ||
                    size_t(batch.first + batch.count) > mesh.indices.size() / 3) {
                    error = "GR2 triangle batch is out of range";
                    return false;
                }
                mesh.batches.push_back(batch);
            }
            asset.meshes.push_back(std::move(mesh));
        }
        return true;
    } catch (const std::exception& ex) {
        asset = {};
        error = ex.what();
        return false;
    }
}

} // namespace ro::gr2
