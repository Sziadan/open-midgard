// Dump Granny SDK decoded textures for comparison with the native IGC port.
#ifndef GRANNY_64BIT_TYPE
#define GRANNY_64BIT_TYPE long long
#endif
#include "granny.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

#define LOAD(name) auto name = reinterpret_cast<decltype(&Granny##name)>(GetProcAddress(dll, "Granny" #name))

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: dump_sdk_textures <gr2> <outPrefix>\n");
        return 2;
    }
    HMODULE dll = LoadLibraryA("Ref/Granny2/lib/win64/granny2_x64.dll");
    if (!dll) {
        std::fprintf(stderr, "failed to load granny2_x64.dll err=%lu\n", GetLastError());
        return 1;
    }
    LOAD(ReadEntireFileFromMemory);
    LOAD(GetFileInfo);
    LOAD(CopyTextureImage);
    auto* RGBA8888PixelFormatPtr =
        reinterpret_cast<granny_pixel_layout const**>(GetProcAddress(dll, "GrannyRGBA8888PixelFormat"));
    if (!ReadEntireFileFromMemory || !GetFileInfo || !CopyTextureImage || !RGBA8888PixelFormatPtr ||
        !*RGBA8888PixelFormatPtr) {
        std::fprintf(stderr, "missing granny exports\n");
        return 1;
    }
    granny_pixel_layout const* rgba8888 = *RGBA8888PixelFormatPtr;

    std::ifstream file(argv[1], std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
    granny_file* gr2 = ReadEntireFileFromMemory(static_cast<granny_int32x>(bytes.size()), bytes.data());
    if (!gr2) {
        std::fprintf(stderr, "ReadEntireFileFromMemory failed\n");
        return 1;
    }
    granny_file_info* info = GetFileInfo(gr2);
    if (!info) {
        std::fprintf(stderr, "GetFileInfo failed\n");
        return 1;
    }

    for (granny_int32 t = 0; t < info->TextureCount; ++t) {
        granny_texture* texture = info->Textures[t];
        if (!texture || texture->ImageCount <= 0) continue;
        const int width = texture->Width;
        const int height = texture->Height;
        std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
        if (!CopyTextureImage(texture, 0, 0, rgba8888, width, height, width * 4, rgba.data())) {
            std::fprintf(stderr, "CopyTextureImage failed tex=%d\n", t);
            continue;
        }
        const std::string path = std::string(argv[2]) + ".sdk" + std::to_string(t) + ".rgba";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(rgba.data()), static_cast<std::streamsize>(rgba.size()));
        std::printf("sdk tex[%d] %dx%d encoding=%d bpp=%d -> %s\n", t, width, height, texture->Encoding,
            texture->Layout.BytesPerPixel, path.c_str());
    }
    return 0;
}
