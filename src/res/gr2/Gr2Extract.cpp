// Texture + skeleton extractors. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2Extract.h"
#include "Gr2Igc.h"
#include <algorithm>
#include <cstring>

namespace ro::gr2 {
namespace {

const ParsedField* Field(const ParsedObject& object, const char* name)
{
    const auto it = object.find(name);
    return it == object.end() ? nullptr : &it->second;
}

double NumberOr(const ParsedField* field, double fallback)
{
    return (field && field->hasNumber) ? field->number : fallback;
}

std::string TextOr(const ParsedField* field, const std::string& fallback)
{
    return (field && field->hasText && !field->text.empty()) ? field->text : fallback;
}

int ReadAlphaFromLayout(const ParsedObject& texFields)
{
    const ParsedField* layoutField = Field(texFields, "Layout");
    if (!layoutField || !layoutField->hasInline) return 1;
    const ParsedField* bpp = Field(layoutField->inlineObject, "BytesPerPixel");
    if (bpp && bpp->hasNumber && bpp->number == 3) return 0;
    return 1;
}

bool DecodeRawBgra(const uint8_t* src, size_t srcSize, int width, int height, std::vector<uint8_t>& rgba,
    std::string& error)
{
    const size_t expected = size_t(width) * size_t(height) * 4u;
    if (!src || srcSize != expected) {
        error = "extractTextures: Raw texture byte count mismatch";
        return false;
    }
    rgba.resize(expected);
    for (size_t i = 0; i < expected; i += 4) {
        rgba[i] = src[i + 2];
        rgba[i + 1] = src[i + 1];
        rgba[i + 2] = src[i];
        rgba[i + 3] = src[i + 3];
    }
    return true;
}

void ReadReal32Array(const LoadedGr2& loaded, int section, int offset, int count, float* out)
{
    if (section < 0 || section >= int(loaded.sectionsOriginal.size()) || !out) {
        for (int i = 0; i < count; ++i) out[i] = (i % 5 == 0) ? 1.f : 0.f;
        return;
    }
    const auto& data = loaded.sectionsOriginal[size_t(section)];
    if (offset < 0 || size_t(offset) + size_t(count) * 4u > data.size()) {
        for (int i = 0; i < count; ++i) out[i] = (i % 5 == 0) ? 1.f : 0.f;
        return;
    }
    const bool little = !loaded.file.byteReversed;
    for (int i = 0; i < count; ++i) out[i] = ReadF32(data, size_t(offset) + size_t(i) * 4u, little);
}

} // namespace

SectionRef RootTypeRef(const File& file)
{
    return {int(file.rootType[0]), int(file.rootType[1])};
}

SectionRef RootObjectRef(const File& file)
{
    return {int(file.rootObject[0]), int(file.rootObject[1])};
}

bool ExtractTextures(const LoadedGr2& loaded, std::vector<ExtractedTexture>& out, std::string& error)
{
    out.clear();
    error.clear();
    try {
        constexpr int maxTextures = 256;
        constexpr int maxImages = 8;
        constexpr int maxMips = 32;

        const auto rootTypeTree = ParseTypeTree(loaded, RootTypeRef(loaded.file));
        const auto root = ParseObject(loaded, rootTypeTree, RootObjectRef(loaded.file), maxTextures);
        const ParsedField* texField = Field(root, "Textures");
        if (!texField || !texField->referenceType.valid()) return true;

        const auto textureType = texField->referenceType;
        const auto& textureRefs = texField->elementRefs;
        if (textureRefs.empty()) return true;

        const auto texMembers = ParseTypeTree(loaded, textureType);
        const int texCount = int((std::min)(textureRefs.size(), size_t(maxTextures)));
        for (int ti = 0; ti < texCount; ++ti) {
            const SectionRef texRef = textureRefs[size_t(ti)];
            if (!texRef.valid()) continue;
            const auto texFields = ParseObject(loaded, texMembers, texRef, 64);
            const int width = int(NumberOr(Field(texFields, "Width"), 0));
            const int height = int(NumberOr(Field(texFields, "Height"), 0));
            const int encoding = int(NumberOr(Field(texFields, "Encoding"), 0));
            const std::string fromFileName = TextOr(Field(texFields, "FromFileName"), "");
            const bool alpha = ReadAlphaFromLayout(texFields) != 0;

            const ParsedField* imagesField = Field(texFields, "Images");
            if (!imagesField || !imagesField->referenceType.valid()) continue;
            const auto images = ReadReferenceArrayObjects(loaded, imagesField->target, imagesField->count,
                imagesField->referenceType, maxImages);

            for (size_t ii = 0; ii < images.size(); ++ii) {
                const auto& imgFields = images[ii].fields;
                const ParsedField* mipsField = Field(imgFields, "MIPLevels");
                if (!mipsField || !mipsField->referenceType.valid()) continue;
                const auto mips = ReadReferenceArrayObjects(loaded, mipsField->target, mipsField->count,
                    mipsField->referenceType, maxMips);

                for (size_t mi = 0; mi < mips.size(); ++mi) {
                    const auto& mipFields = mips[mi].fields;
                    const ParsedField* pixelField = Field(mipFields, "Pixels");
                    if (!pixelField) pixelField = Field(mipFields, "PixelBytes");
                    if (!pixelField || !pixelField->target.valid() || pixelField->count == 0) continue;

                    const auto& section = loaded.sectionsOriginal[size_t(pixelField->target.section)];
                    const size_t begin = size_t(pixelField->target.offset);
                    const size_t count = size_t(pixelField->count);
                    if (begin + count > section.size()) continue;

                    ExtractedTexture tex;
                    tex.name = fromFileName.empty()
                        ? ("tex" + std::to_string(ti) + "_img" + std::to_string(ii))
                        : fromFileName;
                    tex.width = width;
                    tex.height = height;
                    tex.alpha = alpha;
                    tex.encoding = encoding;

                    const uint8_t* bytes = section.data() + begin;
                    if (encoding == kEncodingRaw) {
                        if (!DecodeRawBgra(bytes, count, width, height, tex.rgba, error)) return false;
                    } else if (encoding == kEncodingIgc) {
                        if (!DecodeIgcTexture(bytes, count, width, height, alpha, tex.rgba, error)) return false;
                    } else if (encoding == kEncodingS3tc) {
                        error = "extractTextures: S3TC textures not supported";
                        return false;
                    } else {
                        error = "extractTextures: unknown encoding";
                        return false;
                    }
                    out.push_back(std::move(tex));
                }
            }
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        out.clear();
        return false;
    }
}

bool ExtractSkeletons(const LoadedGr2& loaded, std::vector<ExtractedSkeleton>& out, std::string& error)
{
    out.clear();
    error.clear();
    try {
        constexpr int maxSkeletons = 16;
        constexpr int maxBones = 4096;
        const auto rootTypeTree = ParseTypeTree(loaded, RootTypeRef(loaded.file));
        const auto root = ParseObject(loaded, rootTypeTree, RootObjectRef(loaded.file), maxSkeletons);
        const ParsedField* skeletonField = Field(root, "Skeletons");
        if (!skeletonField || !skeletonField->referenceType.valid() || skeletonField->elementRefs.empty())
            return true;

        const auto skeletonTypeMembers = ParseTypeTree(loaded, skeletonField->referenceType);
        const int limit = int((std::min)(skeletonField->elementRefs.size(), size_t(maxSkeletons)));
        for (int i = 0; i < limit; ++i) {
            const SectionRef skeletonRef = skeletonField->elementRefs[size_t(i)];
            if (!skeletonRef.valid()) continue;
            const auto fields = ParseObject(loaded, skeletonTypeMembers, skeletonRef, maxBones);
            const ParsedField* boneField = Field(fields, "Bones");
            if (!boneField || !boneField->referenceType.valid()) continue;

            const auto boneObjects = ReadReferenceArrayObjects(loaded, boneField->target, boneField->count,
                boneField->referenceType, maxBones);

            ExtractedSkeleton skeleton;
            skeleton.name = TextOr(Field(fields, "Name"), "Skeleton_" + std::to_string(out.size()));
            skeleton.bones.resize(boneObjects.size());
            for (size_t b = 0; b < boneObjects.size(); ++b) {
                const auto& boneObject = boneObjects[b];
                ExtractedBone bone;
                bone.name = TextOr(Field(boneObject.fields, "Name"), "Bone_" + std::to_string(b));
                bone.parent = int(NumberOr(Field(boneObject.fields, "ParentIndex"), -1));
                const ParsedField* transformField = Field(boneObject.fields, "Transform");
                const ParsedField* inverseField = Field(boneObject.fields, "InverseWorldTransform");
                const int transformOffset = transformField ? transformField->offset : 0;
                const int inverseOffset = inverseField ? inverseField->offset : 0;
                if (boneObject.ref.valid()) {
                    bone.bind = ReadTransformBytes(loaded, boneObject.ref.section, transformOffset);
                    ReadReal32Array(loaded, boneObject.ref.section, inverseOffset, 16, bone.inverse.data());
                }
                skeleton.bones[b] = std::move(bone);
            }
            out.push_back(std::move(skeleton));
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        out.clear();
        return false;
    }
}

} // namespace ro::gr2
