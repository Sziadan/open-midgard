// High-level GR2 extractors. MIT attribution: third_party/gr2-reader/LICENSE.
#pragma once
#include "Gr2TypeTree.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ro::gr2 {

constexpr int kEncodingRaw = 1;
constexpr int kEncodingS3tc = 2;
constexpr int kEncodingIgc = 3;

struct ExtractedTexture {
    std::string name;
    int width = 0;
    int height = 0;
    bool alpha = false;
    int encoding = 0;
    std::vector<uint8_t> rgba; // RGBA8888
};

struct ExtractedBone {
    std::string name;
    int parent = -1;
    Gr2Transform bind{};
    std::array<float, 16> inverse{};
};

struct ExtractedSkeleton {
    std::string name;
    std::vector<ExtractedBone> bones;
};

struct ExtractedCurve {
    int degree = 0;
    int dimension = 0;
    std::vector<double> knots;
    std::vector<double> controls;
    std::vector<double> sample;
};

struct ExtractedTrack {
    std::string name;
    ExtractedCurve position;
    ExtractedCurve orientation;
    ExtractedCurve scale;
};

struct ExtractedAnimation {
    double duration = 0;
    std::vector<ExtractedTrack> tracks;
};

struct ExtractedBatch {
    std::string textureFile;
    int materialIndex = 0;
    int first = 0;
    int count = 0;
};

struct ExtractedVertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
    std::vector<std::pair<int, float>> weights; // local bone index, weight
};

struct ExtractedMesh {
    std::string name;
    std::vector<std::string> boneBindings;
    std::vector<std::string> materialTextures;
    std::vector<ExtractedVertex> vertices;
    std::vector<int> indices;
    std::vector<ExtractedBatch> batches;
};

struct ExtractedModel {
    int skeletonIdx = -1;
    std::vector<int> meshIndices;
};

bool ExtractTextures(const LoadedGr2& loaded, std::vector<ExtractedTexture>& out, std::string& error);
bool ExtractSkeletons(const LoadedGr2& loaded, std::vector<ExtractedSkeleton>& out, std::string& error);
bool ExtractMeshes(const LoadedGr2& loaded, std::vector<ExtractedMesh>& out, std::string& error);
bool ExtractAnimations(const LoadedGr2& loaded, std::vector<ExtractedAnimation>& out, std::string& error);
bool ExtractModels(const LoadedGr2& loaded, std::vector<ExtractedModel>& out, std::string& error);

SectionRef RootTypeRef(const File& file);
SectionRef RootObjectRef(const File& file);

} // namespace ro::gr2
