// Mesh + model extractors. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2Extract.h"
#include <algorithm>
#include <cstring>
#include <unordered_map>

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

std::string RefKey(SectionRef ref)
{
    return std::to_string(ref.section) + ":" + std::to_string(ref.offset);
}

struct VertexComponent {
    std::string name;
    int offset = 0;
    int width = 1;
    int memberType = MT_END;
};

int ComponentStorageSize(const VertexComponent& c)
{
    const int width = c.width ? c.width : 1;
    const int t = c.memberType;
    if (t == MT_REAL32 || t == MT_INT32 || t == MT_UINT32) return 4 * width;
    if (t == MT_INT16 || t == MT_UINT16 || t == MT_BINORMAL_INT16 || t == MT_NORMAL_UINT16 || t == MT_REAL16)
        return 2 * width;
    if (t == MT_INT8 || t == MT_UINT8 || t == MT_BINORMAL_INT8 || t == MT_NORMAL_UINT8) return width;
    return 4 * width;
}

std::vector<VertexComponent> VertexComponents(const std::vector<TypeMember>& members)
{
    std::vector<VertexComponent> out;
    int offset = 0;
    for (const TypeMember& member : members) {
        VertexComponent c;
        c.name = member.name;
        c.offset = offset;
        c.width = member.arrayWidth ? int(member.arrayWidth) : 1;
        c.memberType = member.memberType;
        offset += ComponentStorageSize(c);
        out.push_back(std::move(c));
    }
    return out;
}

int VertexStride(const std::vector<VertexComponent>& components)
{
    int max = 0;
    for (const VertexComponent& c : components) {
        const int end = c.offset + ComponentStorageSize(c);
        if (end > max) max = end;
    }
    return max;
}

float ReadF32LE(const uint8_t* data, size_t size, size_t offset)
{
    if (offset + 4 > size) return 0.f;
    uint32_t bits = uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
        (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint16_t ReadU16LE(const uint8_t* data, size_t size, size_t offset)
{
    if (offset + 2 > size) return 0;
    return uint16_t(data[offset] | (uint16_t(data[offset + 1]) << 8));
}

uint32_t ReadU32LE(const uint8_t* data, size_t size, size_t offset)
{
    if (offset + 4 > size) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
        (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

const VertexComponent* FindComponent(const std::vector<VertexComponent>& components, const char* name,
    int minWidth, bool prefix, int memberType = MT_REAL32)
{
    for (const VertexComponent& c : components) {
        if (c.memberType != memberType) continue;
        if (c.width < minWidth) continue;
        if (prefix) {
            if (c.name.rfind(name, 0) == 0) return &c;
        } else if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

float ReadWeightValue(const uint8_t* data, size_t size, size_t offset, int lane, const VertexComponent& c)
{
    const int t = c.memberType;
    if (t == MT_REAL32) {
        const float value = ReadF32LE(data, size, offset + size_t(lane) * 4u);
        if (value <= 0.f) return 0.f;
        return value > 1.f ? 1.f : value;
    }
    if (t == MT_UINT16 || t == MT_NORMAL_UINT16) {
        const uint16_t value = ReadU16LE(data, size, offset + size_t(lane) * 2u);
        return t == MT_NORMAL_UINT16 ? float(value) / 65535.f : float(value);
    }
    if (t == MT_UINT8 || t == MT_NORMAL_UINT8) {
        const size_t start = offset + size_t(lane);
        if (start >= size) return 0.f;
        const uint8_t value = data[start];
        return t == MT_NORMAL_UINT8 ? float(value) / 255.f : float(value);
    }
    return 0.f;
}

int ReadIntValue(const uint8_t* data, size_t size, size_t offset, int lane, const VertexComponent& c)
{
    const int t = c.memberType;
    if (t == MT_INT32 || t == MT_UINT32) {
        const uint32_t value = ReadU32LE(data, size, offset + size_t(lane) * 4u);
        return t == MT_INT32 ? int(int32_t(value)) : int(value);
    }
    if (t == MT_INT16 || t == MT_UINT16) {
        const uint16_t value = ReadU16LE(data, size, offset + size_t(lane) * 2u);
        return t == MT_INT16 ? int(int16_t(value)) : int(value);
    }
    if (t == MT_INT8 || t == MT_UINT8) {
        const size_t start = offset + size_t(lane);
        if (start >= size) return -1;
        return t == MT_INT8 ? int(int8_t(data[start])) : int(data[start]);
    }
    return -1;
}

struct MaterialInfo {
    int index = 0;
    std::string name;
    std::string textureFile;
};

using MaterialCache = std::unordered_map<std::string, MaterialInfo>;

MaterialInfo ReadMaterial(const LoadedGr2& loaded, SectionRef ref, SectionRef materialType, int index,
    MaterialCache& cache, std::unordered_map<std::string, bool>& seen);

std::pair<std::string, bool> ReadTextureField(const LoadedGr2& loaded, const ParsedField* textureField)
{
    if (!textureField || !textureField->target.valid() || !textureField->referenceType.valid())
        return {"", false};
    const auto textureMembers = ParseTypeTree(loaded, textureField->referenceType);
    const auto fields = ParseObject(loaded, textureMembers, textureField->target, 8);
    return {TextOr(Field(fields, "FromFileName"), ""), true};
}

std::pair<std::string, bool> ReadFirstMapTexture(const LoadedGr2& loaded, const ParsedField* mapsField,
    SectionRef materialType, MaterialCache& cache, std::unordered_map<std::string, bool>& seen)
{
    if (!mapsField || !mapsField->referenceType.valid()) return {"", false};
    const auto maps = ReadReferenceArrayObjects(loaded, mapsField->target, mapsField->count,
        mapsField->referenceType, 16);
    for (const auto& map : maps) {
        const ParsedField* mapField = Field(map.fields, "Map");
        if (!mapField || !mapField->target.valid()) continue;
        const MaterialInfo material = ReadMaterial(loaded, mapField->target, materialType, -1, cache, seen);
        if (!material.textureFile.empty()) return {material.textureFile, true};
    }
    return {"", false};
}

MaterialInfo ReadMaterial(const LoadedGr2& loaded, SectionRef ref, SectionRef materialType, int index,
    MaterialCache& cache, std::unordered_map<std::string, bool>& seen)
{
    const std::string key = RefKey(ref);
    const auto cached = cache.find(key);
    if (cached != cache.end()) return cached->second;
    if (!materialType.valid() || seen[key]) {
        MaterialInfo fallback;
        fallback.index = index;
        fallback.name = "Material_" + std::to_string(index);
        return fallback;
    }
    seen[key] = true;
    const auto materialMembers = ParseTypeTree(loaded, materialType);
    const auto fields = ParseObject(loaded, materialMembers, ref, 16);
    auto texture = ReadTextureField(loaded, Field(fields, "Texture"));
    if (!texture.second)
        texture = ReadFirstMapTexture(loaded, Field(fields, "Maps"), materialType, cache, seen);
    MaterialInfo info;
    info.index = index;
    info.name = TextOr(Field(fields, "Name"), "Material_" + std::to_string(index));
    info.textureFile = texture.second ? texture.first : "";
    cache[key] = info;
    return info;
}

MaterialCache BuildMaterialCache(const LoadedGr2& loaded, int maxMaterials)
{
    MaterialCache cache;
    const auto root = ParseObject(loaded, ParseTypeTree(loaded, RootTypeRef(loaded.file)),
        RootObjectRef(loaded.file), maxMaterials);
    const ParsedField* materialField = Field(root, "Materials");
    if (!materialField || !materialField->referenceType.valid()) return cache;
    const int limit = int((std::min)(materialField->elementRefs.size(), size_t(maxMaterials)));
    for (int i = 0; i < limit; ++i) {
        const SectionRef ref = materialField->elementRefs[size_t(i)];
        if (!ref.valid()) continue;
        std::unordered_map<std::string, bool> seen;
        ReadMaterial(loaded, ref, materialField->referenceType, i, cache, seen);
    }
    return cache;
}

} // namespace

bool ExtractMeshes(const LoadedGr2& loaded, std::vector<ExtractedMesh>& out, std::string& error)
{
    out.clear();
    error.clear();
    try {
        constexpr int maxMeshes = 32;
        constexpr int maxMaterials = 4096;
        constexpr int maxBones = 4096;

        const auto rootTypeTree = ParseTypeTree(loaded, RootTypeRef(loaded.file));
        const TypeMember* meshMember = nullptr;
        for (const TypeMember& member : rootTypeTree) {
            if (member.name == "Meshes") {
                meshMember = &member;
                break;
            }
        }
        if (!meshMember || !meshMember->referenceType.valid()) return true;

        const auto root = ParseObject(loaded, rootTypeTree, RootObjectRef(loaded.file), maxMeshes);
        const ParsedField* meshField = Field(root, "Meshes");
        if (!meshField || meshField->elementRefs.empty()) return true;

        const auto meshTypeMembers = ParseTypeTree(loaded, meshMember->referenceType);
        std::unordered_map<std::string, TypeMember> meshTypeMap;
        for (const TypeMember& member : meshTypeMembers) meshTypeMap[member.name] = member;

        MaterialCache materials = BuildMaterialCache(loaded, maxMaterials);
        const int limit = int((std::min)(meshField->elementRefs.size(), size_t(maxMeshes)));
        for (int i = 0; i < limit; ++i) {
            const SectionRef meshRef = meshField->elementRefs[size_t(i)];
            if (!meshRef.valid()) continue;
            const auto meshFields = ParseObject(loaded, meshTypeMembers, meshRef);

            ParsedObject primaryVertexData;
            ParsedObject primaryTopology;
            auto hydrate = [&](const char* fieldName, ParsedObject& dest) {
                const ParsedField* field = Field(meshFields, fieldName);
                const auto memberIt = meshTypeMap.find(fieldName);
                if (!field || !field->target.valid() || memberIt == meshTypeMap.end() ||
                    !memberIt->second.referenceType.valid())
                    return;
                const auto subMembers = ParseTypeTree(loaded, memberIt->second.referenceType);
                dest = ParseObject(loaded, subMembers, field->target);
            };
            hydrate("PrimaryVertexData", primaryVertexData);
            hydrate("PrimaryTopology", primaryTopology);

            const ParsedField* vertices = Field(primaryVertexData, "Vertices");
            if (!vertices || !vertices->target.valid() || !vertices->variantType.valid() ||
                vertices->count == 0)
                continue;

            const auto typeMembers = ParseTypeTree(loaded, vertices->variantType);
            const auto components = VertexComponents(typeMembers);
            const int stride = VertexStride(components);
            const int vertexCount = int(vertices->count);
            const size_t byteCount = size_t(vertexCount) * size_t(stride);
            if (vertices->target.section < 0 ||
                vertices->target.section >= int(loaded.sectionsOriginal.size()))
                continue;
            const auto& section = loaded.sectionsOriginal[size_t(vertices->target.section)];
            if (vertices->target.offset < 0 ||
                size_t(vertices->target.offset) + byteCount > section.size())
                continue;
            const uint8_t* vertexBytes = section.data() + vertices->target.offset;

            const ParsedField* indices16 = Field(primaryTopology, "Indices16");
            const ParsedField* indices32 = Field(primaryTopology, "Indices");
            const ParsedField* indexField = nullptr;
            bool indexIs32 = false;
            if (indices16 && indices16->count > 0) indexField = indices16;
            else if (indices32 && indices32->count > 0) {
                indexField = indices32;
                indexIs32 = true;
            } else if (indices16) {
                indexField = indices16;
            } else if (indices32) {
                indexField = indices32;
                indexIs32 = true;
            }

            ExtractedMesh mesh;
            mesh.name = TextOr(Field(meshFields, "Name"), "Mesh_" + std::to_string(i));

            const ParsedField* boneField = Field(meshFields, "BoneBindings");
            if (boneField && boneField->referenceType.valid()) {
                const auto bones = ReadReferenceArrayObjects(loaded, boneField->target, boneField->count,
                    boneField->referenceType, maxBones);
                for (size_t b = 0; b < bones.size(); ++b)
                    mesh.boneBindings.push_back(
                        TextOr(Field(bones[b].fields, "BoneName"), "Bone_" + std::to_string(b)));
            }

            const ParsedField* matBindField = Field(meshFields, "MaterialBindings");
            if (matBindField && matBindField->referenceType.valid()) {
                const auto mats = ReadReferenceArrayObjects(loaded, matBindField->target, matBindField->count,
                    matBindField->referenceType, maxBones);
                for (size_t m = 0; m < mats.size(); ++m) {
                    const ParsedField* materialRef = Field(mats[m].fields, "Material");
                    std::string texture;
                    if (materialRef && materialRef->target.valid()) {
                        const auto it = materials.find(RefKey(materialRef->target));
                        if (it != materials.end()) texture = it->second.textureFile;
                    }
                    mesh.materialTextures.push_back(texture);
                }
            }

            const VertexComponent* pos = FindComponent(components, "Position", 3, false);
            const VertexComponent* nrm = FindComponent(components, "Normal", 3, false);
            const VertexComponent* uv = FindComponent(components, "TextureCoordinates", 2, true);
            const VertexComponent* weightsC = nullptr;
            const VertexComponent* indicesC = nullptr;
            for (const VertexComponent& c : components) {
                if (c.name == "BoneWeights") weightsC = &c;
                else if (c.name == "BoneIndices") indicesC = &c;
            }

            mesh.vertices.resize(size_t(vertexCount));
            for (int v = 0; v < vertexCount; ++v) {
                ExtractedVertex& vertex = mesh.vertices[size_t(v)];
                const size_t base = size_t(v) * size_t(stride);
                if (pos) {
                    for (int k = 0; k < 3; ++k)
                        vertex.position[size_t(k)] =
                            ReadF32LE(vertexBytes, byteCount, base + size_t(pos->offset) + size_t(k) * 4u);
                }
                if (nrm) {
                    for (int k = 0; k < 3; ++k)
                        vertex.normal[size_t(k)] =
                            ReadF32LE(vertexBytes, byteCount, base + size_t(nrm->offset) + size_t(k) * 4u);
                }
                if (uv) {
                    for (int k = 0; k < 2; ++k)
                        vertex.uv[size_t(k)] =
                            ReadF32LE(vertexBytes, byteCount, base + size_t(uv->offset) + size_t(k) * 4u);
                }
                if (weightsC && indicesC && !mesh.boneBindings.empty()) {
                    const int width = (std::min)(weightsC->width, indicesC->width);
                    for (int lane = 0; lane < width; ++lane) {
                        const float weight = ReadWeightValue(vertexBytes, byteCount,
                            base + size_t(weightsC->offset), lane, *weightsC);
                        const int boneIndex = ReadIntValue(vertexBytes, byteCount,
                            base + size_t(indicesC->offset), lane, *indicesC);
                        if (weight <= 0.f || boneIndex < 0 || boneIndex >= int(mesh.boneBindings.size()))
                            continue;
                        vertex.weights.emplace_back(boneIndex, weight);
                    }
                }
            }

            if (indexField && indexField->target.valid() && indexField->count > 0 &&
                indexField->target.section >= 0 &&
                indexField->target.section < int(loaded.sectionsOriginal.size()) &&
                indexField->target.offset >= 0) {
                const auto& indexSection = loaded.sectionsOriginal[size_t(indexField->target.section)];
                const size_t indexBegin = size_t(indexField->target.offset);
                const int indexCount = int(indexField->count);
                if (indexIs32) {
                    if (indexBegin + size_t(indexCount) * 4u <= indexSection.size()) {
                        for (int n = 0; n < indexCount; ++n)
                            mesh.indices.push_back(int(ReadU32LE(indexSection.data(), indexSection.size(),
                                indexBegin + size_t(n) * 4u)));
                    }
                } else if (indexBegin + size_t(indexCount) * 2u <= indexSection.size()) {
                    for (int n = 0; n < indexCount; ++n)
                        mesh.indices.push_back(int(ReadU16LE(indexSection.data(), indexSection.size(),
                            indexBegin + size_t(n) * 2u)));
                }
            }

            const ParsedField* groupsField = Field(primaryTopology, "Groups");
            if (groupsField && groupsField->referenceType.valid()) {
                const auto groups = ReadReferenceArrayObjects(loaded, groupsField->target, groupsField->count,
                    groupsField->referenceType, 4096);
                for (const auto& group : groups) {
                    ExtractedBatch batch;
                    batch.materialIndex = int(NumberOr(Field(group.fields, "MaterialIndex"), 0));
                    batch.first = int(NumberOr(Field(group.fields, "TriFirst"), 0));
                    batch.count = int(NumberOr(Field(group.fields, "TriCount"), 0));
                    if (batch.materialIndex >= 0 &&
                        batch.materialIndex < int(mesh.materialTextures.size()))
                        batch.textureFile = mesh.materialTextures[size_t(batch.materialIndex)];
                    mesh.batches.push_back(std::move(batch));
                }
            }

            out.push_back(std::move(mesh));
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        out.clear();
        return false;
    }
}

bool ExtractModels(const LoadedGr2& loaded, std::vector<ExtractedModel>& out, std::string& error)
{
    out.clear();
    error.clear();
    try {
        constexpr int maxModels = 64;
        constexpr int maxBindings = 256;
        const auto root = ParseObject(loaded, ParseTypeTree(loaded, RootTypeRef(loaded.file)),
            RootObjectRef(loaded.file), maxModels);
        const ParsedField* modelField = Field(root, "Models");
        if (!modelField || !modelField->referenceType.valid() || modelField->elementRefs.empty())
            return true;

        std::unordered_map<std::string, int> skeletonRefIndex;
        std::unordered_map<std::string, int> meshRefIndex;
        if (const ParsedField* skeletons = Field(root, "Skeletons")) {
            for (size_t i = 0; i < skeletons->elementRefs.size(); ++i)
                if (skeletons->elementRefs[i].valid())
                    skeletonRefIndex[RefKey(skeletons->elementRefs[i])] = int(i);
        }
        if (const ParsedField* meshes = Field(root, "Meshes")) {
            for (size_t i = 0; i < meshes->elementRefs.size(); ++i)
                if (meshes->elementRefs[i].valid())
                    meshRefIndex[RefKey(meshes->elementRefs[i])] = int(i);
        }

        const auto modelTT = ParseTypeTree(loaded, modelField->referenceType);
        const int limit = int((std::min)(modelField->elementRefs.size(), size_t(maxModels)));
        for (int i = 0; i < limit; ++i) {
            const SectionRef mref = modelField->elementRefs[size_t(i)];
            if (!mref.valid()) continue;
            const auto fields = ParseObject(loaded, modelTT, mref);
            ExtractedModel model;
            if (const ParsedField* skeleton = Field(fields, "Skeleton")) {
                if (skeleton->target.valid()) {
                    const auto it = skeletonRefIndex.find(RefKey(skeleton->target));
                    if (it != skeletonRefIndex.end()) model.skeletonIdx = it->second;
                }
            }
            if (const ParsedField* mbField = Field(fields, "MeshBindings")) {
                if (mbField->referenceType.valid()) {
                    const auto mbObjects = ReadReferenceArrayObjects(loaded, mbField->target, mbField->count,
                        mbField->referenceType, maxBindings);
                    for (const auto& binding : mbObjects) {
                        int meshIdx = -1;
                        if (const ParsedField* mesh = Field(binding.fields, "Mesh")) {
                            if (mesh->target.valid()) {
                                const auto it = meshRefIndex.find(RefKey(mesh->target));
                                if (it != meshRefIndex.end()) meshIdx = it->second;
                            }
                        }
                        model.meshIndices.push_back(meshIdx);
                    }
                }
            }
            out.push_back(std::move(model));
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        out.clear();
        return false;
    }
}

} // namespace ro::gr2
