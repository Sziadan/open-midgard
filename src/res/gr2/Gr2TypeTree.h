// DataTypeDefinition walker. MIT attribution: third_party/gr2-reader/LICENSE.
#pragma once
#include "Gr2File.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ro::gr2 {

constexpr uint32_t kFakePointerBase = 0x10000000u;
constexpr uint32_t kFakeSectionStride = 0x100000u;
constexpr int kMaxInlineDepth = 64;

enum MemberType : int {
    MT_END = 0,
    MT_INLINE = 1,
    MT_REFERENCE = 2,
    MT_REFERENCE_TO_ARRAY = 3,
    MT_ARRAY_OF_REFERENCES = 4,
    MT_VARIANT_REFERENCE = 5,
    MT_UNSUPPORTED = 6,
    MT_REFERENCE_TO_VARIANT_ARRAY = 7,
    MT_STRING = 8,
    MT_TRANSFORM = 9,
    MT_REAL32 = 10,
    MT_INT8 = 11,
    MT_UINT8 = 12,
    MT_BINORMAL_INT8 = 13,
    MT_NORMAL_UINT8 = 14,
    MT_INT16 = 15,
    MT_UINT16 = 16,
    MT_BINORMAL_INT16 = 17,
    MT_NORMAL_UINT16 = 18,
    MT_INT32 = 19,
    MT_UINT32 = 20,
    MT_REAL16 = 21,
    MT_EMPTY_REFERENCE = 22,
};

struct SectionRef {
    int section = -1;
    int offset = -1;
    bool valid() const { return section >= 0 && offset >= 0; }
};

struct Gr2Transform {
    uint32_t flags = 0;
    std::array<float, 3> position{0, 0, 0};
    std::array<float, 4> orientation{0, 0, 0, 1};
    std::array<float, 9> scaleShear{1, 0, 0, 0, 1, 0, 0, 0, 1};
};

struct TypeMember {
    int memberType = MT_END;
    std::string name;
    SectionRef referenceType;
    uint32_t arrayWidth = 1;
    uint32_t extra[3]{};
    int offset = 0;
};

struct ParsedField;
using ParsedObject = std::unordered_map<std::string, ParsedField>;

struct ParsedField {
    int memberType = MT_END;
    int offset = 0;
    SectionRef referenceType;
    SectionRef target;
    SectionRef variantType;
    uint32_t count = 0;
    bool truncated = false;
    double number = 0;
    std::string text;
    Gr2Transform transform;
    bool hasNumber = false;
    bool hasText = false;
    bool hasTransform = false;
    std::vector<SectionRef> elementRefs;
    ParsedObject inlineObject;
    bool hasInline = false;
};

struct ReferenceArrayObject {
    SectionRef ref;
    ParsedObject fields;
};

struct LoadedGr2 {
    File file;
    std::vector<std::vector<uint8_t>> sectionsOriginal;
    std::vector<std::vector<uint8_t>> sectionsFixed;
    int pointerSize = 4;
};

uint32_t MakeFakePointer(int section, int offset);
SectionRef DecodeFakePointer(uint32_t pointer, int sectionCount);

bool LoadGr2(const File& file, LoadedGr2& out, std::string& error);
std::vector<TypeMember> ParseTypeTree(const LoadedGr2& loaded, SectionRef ref, int maxMembers = 512);
int ObjectStorageSize(const LoadedGr2& loaded, const std::vector<TypeMember>& members,
    std::unordered_map<uint32_t, int>* memo = nullptr);
ParsedObject ParseObject(const LoadedGr2& loaded, const std::vector<TypeMember>& typeTree, SectionRef ref,
    int maxArrayRefs = 256, int depth = 0);
std::vector<ReferenceArrayObject> ReadReferenceArrayObjects(const LoadedGr2& loaded, SectionRef arrayRef,
    uint32_t count, SectionRef typeRef, int maxCount = 256);
Gr2Transform ReadTransformBytes(const LoadedGr2& loaded, int section, int offset);
std::string ReadStringAt(const LoadedGr2& loaded, SectionRef ref, int maxLength = 1024);

uint32_t ReadU32(const std::vector<uint8_t>& data, size_t offset, bool littleEndian);
uint32_t ReadPointer(const std::vector<uint8_t>& data, size_t offset, int pointerSize, bool littleEndian);
float ReadF32(const std::vector<uint8_t>& data, size_t offset, bool littleEndian);

} // namespace ro::gr2
