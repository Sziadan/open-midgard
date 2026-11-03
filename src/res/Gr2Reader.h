#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ro::gr2 {
using Matrix = std::array<float,16>;
struct Transform {
    std::array<double,3> position{0,0,0};
    std::array<double,4> orientation{0,0,0,1};
    std::array<double,9> scale{1,0,0,0,1,0,0,0,1};
};
struct Curve {
    int degree=0, dimension=0;
    std::vector<double> knots, controls, sample;
};
struct Track { Curve position, orientation, scale; };
struct Animation {
    double duration=0;
    std::unordered_map<std::string,Track> tracks;
};
struct Bone {
    std::string name;
    int parent=-1;
    Transform bind;
    Matrix inverse{};
};
struct Weight { int bone=0; float value=0; };
struct Vertex {
    std::array<float,3> position{},normal{};
    std::array<float,2> uv{};
    std::vector<Weight> weights;
};
struct Batch { int texture=-1,first=0,count=0; };
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
    std::vector<Batch> batches;
};
struct Image {
    std::string name;
    int width=0,height=0;
    bool alpha=false;
    std::vector<std::uint32_t> pixels;
};
struct Asset {
    std::vector<Bone> bones;
    std::vector<Mesh> meshes;
    std::vector<Image> images;
    std::vector<Animation> animations;
};

// Pure data in/out. Decoding is run off the render thread. The embedded
// independent reader uses Qt's existing engine, never a Granny SDK or process.
bool Decode(const std::vector<unsigned char>& bytes,Asset& asset,std::string& error,bool animationOnly=false);
std::vector<Matrix> Sample(const Asset& asset,const Animation* animation,double time,bool loop);
void Skin(const Mesh& mesh,const std::vector<Matrix>& matrices,
    std::vector<std::array<float,3>>& positions,std::vector<std::array<float,3>>& normals);
}
