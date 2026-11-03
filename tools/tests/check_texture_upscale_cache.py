"""Compile production scaler functions in isolation; check cache correctness and timing."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--compiler", default="g++")
parser.add_argument("--optimization", default="-O3")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = root / "build-mingw-preset" / "upscale-check"
build.mkdir(parents=True, exist_ok=True)
source = (root / "src/res/Texture.cpp").read_text()
body = source[source.index("enum class TextureUpscaleKind"):source.index("void WritePackedPixel")]
harness = r'''
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include "xbrz.h"
enum class TextureUpscaleAlgorithm { Automatic, Xbrz };
struct Settings { TextureUpscaleAlgorithm textureUpscaleAlgorithm; } settings{};
const Settings& GetCachedGraphicsSettings() { return settings; }
bool IsLightmapAtlasName(const char* name) { return name && std::strncmp(name, "__lightmap_atlas_", 17) == 0; }
'''
checks = r'''
void check(const std::vector<unsigned int>& src, int w, int h, int pitch, int scale, bool key, const char* name = "asset") {
    std::vector<unsigned int> expected, first, second;
    if (!BuildTextureUpscaleUncached(src.data(), w, h, pitch, scale, name, key, &expected)
        || !BuildTextureUpscale(src.data(), w, h, pitch, scale, name, key, &first)
        || !BuildTextureUpscale(src.data(), w, h, pitch, scale, name, key, &second)
        || expected != first || expected != second) throw std::runtime_error("cached output differs");
}
int main() {
    std::vector<unsigned int> pixels(16 * 16);
    unsigned int random = 17;
    for (auto& p : pixels) { random = random * 1664525u + 1013904223u; p = random; }
    pixels[0] = 0x00ff00ff; pixels[1] = 0; pixels[2] = 0x80ffffff;
    for (auto algorithm : {TextureUpscaleAlgorithm::Automatic, TextureUpscaleAlgorithm::Xbrz}) {
        settings.textureUpscaleAlgorithm = algorithm;
        for (int scale : {2, 3, 4}) for (bool key : {false, true}) {
            check(pixels, 16, 16, 64, scale, key);
            check(pixels, 12, 16, 64, scale, key); // padded rows
            check(pixels, 16, 16, 64, scale, key, "__lightmap_atlas_0");
            pixels[13] ^= 0x0000ff00; // changing a frame/palette must invalidate
            check(pixels, 16, 16, 64, scale, key);
        }
    }
    for (unsigned int frame = 0; frame < 600; ++frame) {
        pixels[3] = 0xff000000u | frame;
        check(pixels, 16, 16, 64, 4, true);
    }
    std::vector<unsigned int> large(128 * 128);
    for (auto& p : large) { random = random * 1664525u + 1013904223u; p = 0xff000000u | random; }
    for (auto algorithm : {TextureUpscaleAlgorithm::Automatic, TextureUpscaleAlgorithm::Xbrz}) {
        settings.textureUpscaleAlgorithm = algorithm;
        std::vector<unsigned int> out;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 20; ++i) BuildTextureUpscaleUncached(large.data(), 128, 128, 512, 4, "asset", false, &out);
        double cold = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20;
        BuildTextureUpscale(large.data(), 128, 128, 512, 4, "asset", false, &out);
        start = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; ++i) BuildTextureUpscale(large.data(), 128, 128, 512, 4, "asset", false, &out);
        double warm = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 100;
        std::cout << (algorithm == TextureUpscaleAlgorithm::Automatic ? "Automatic" : "xBRZ")
                  << ": uncached=" << cold << "ms cached=" << warm << "ms\n";
    }
    std::cout << "Cache equivalence, padded rows, alpha/color-key, frame changes, algorithms, and eviction: PASS\n";
}
'''
cpp = build / "TextureUpscaleCacheCheck.cpp"
cpp.write_text(harness + body + checks)
exe = build / "check.exe"
subprocess.run([args.compiler, "-std=c++17", args.optimization, str(cpp),
                str(root / "third_party/xbrz/xbrz.cpp"), "-I", str(root / "third_party/xbrz"),
                "-o", str(exe)], check=True)
env = os.environ.copy()
env["PATH"] = str(Path(args.compiler).resolve().parent) + os.pathsep + env.get("PATH", "")
subprocess.run([str(exe)], check=True, env=env)
