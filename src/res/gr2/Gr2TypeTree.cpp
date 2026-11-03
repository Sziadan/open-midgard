// DataTypeDefinition walker. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2TypeTree.h"
#include <cstring>
#include <set>
#include <stdexcept>

namespace ro::gr2 {
namespace {

int ScalarSize(int memberType)
{
    switch (memberType) {
    case MT_REAL32:
    case MT_INT32:
    case MT_UINT32:
        return 4;
    case MT_INT16:
    case MT_UINT16:
    case MT_BINORMAL_INT16:
    case MT_NORMAL_UINT16:
    case MT_REAL16:
        return 2;
    case MT_INT8:
    case MT_UINT8:
    case MT_BINORMAL_INT8:
    case MT_NORMAL_UINT8:
        return 1;
    default:
        return -1;
    }
}

int MemberStorageSize(const TypeMember& member, int pointerSize)
{
    const int width = member.arrayWidth ? int(member.arrayWidth) : 1;
    const int scalar = ScalarSize(member.memberType);
    if (scalar >= 0) return scalar * width;
    const int t = member.memberType;
    if (t == MT_REFERENCE || t == MT_EMPTY_REFERENCE || t == MT_STRING) return pointerSize;
    if (t == MT_REFERENCE_TO_ARRAY || t == MT_ARRAY_OF_REFERENCES) return 4 + pointerSize;
    if (t == MT_VARIANT_REFERENCE) return pointerSize * 2;
    if (t == MT_REFERENCE_TO_VARIANT_ARRAY) return pointerSize + 4 + pointerSize;
    if (t == MT_TRANSFORM) return 68;
    if (t == MT_INLINE) return 0;
    return pointerSize;
}

void WritePointer(std::vector<uint8_t>& data, size_t offset, uint32_t value, int pointerSize, bool littleEndian)
{
    if (pointerSize == 4) {
        if (littleEndian) {
            data[offset] = uint8_t(value);
            data[offset + 1] = uint8_t(value >> 8);
            data[offset + 2] = uint8_t(value >> 16);
            data[offset + 3] = uint8_t(value >> 24);
        } else {
            data[offset] = uint8_t(value >> 24);
            data[offset + 1] = uint8_t(value >> 16);
            data[offset + 2] = uint8_t(value >> 8);
            data[offset + 3] = uint8_t(value);
        }
        return;
    }
    const uint32_t lo = value;
    const uint32_t hi = 0;
    if (littleEndian) {
        WritePointer(data, offset, lo, 4, true);
        WritePointer(data, offset + 4, hi, 4, true);
    } else {
        WritePointer(data, offset, hi, 4, false);
        WritePointer(data, offset + 4, lo, 4, false);
    }
}

bool LooksText(const uint8_t* bytes, size_t len)
{
    if (len == 0) return false;
    for (size_t i = 0; i < len; ++i) {
        const uint8_t v = bytes[i];
        if (v == 9 || v == 10 || v == 13) continue;
        if (v < 32 || v == 127) return false;
    }
    return true;
}

std::vector<SectionRef> ReadArrayReferences(const LoadedGr2& loaded, SectionRef arrayRef, uint32_t count, int maxCount)
{
    std::vector<SectionRef> refs;
    if (!arrayRef.valid() || count == 0) return refs;
    const bool little = !loaded.file.byteReversed;
    const auto& data = loaded.sectionsFixed[size_t(arrayRef.section)];
    const int limit = int((std::min)(count, uint32_t(maxCount)));
    refs.reserve(size_t(limit));
    for (int i = 0; i < limit; ++i) {
        const size_t off = size_t(arrayRef.offset) + size_t(i) * size_t(loaded.pointerSize);
        if (off + size_t(loaded.pointerSize) > data.size()) break;
        const uint32_t pointer = ReadPointer(data, off, loaded.pointerSize, little);
        const SectionRef decoded = DecodeFakePointer(pointer, int(loaded.sectionsFixed.size()));
        if (decoded.valid()) refs.push_back(decoded);
    }
    return refs;
}

int ObjectStorageSizeImpl(const LoadedGr2& loaded, const std::vector<TypeMember>& members,
    std::set<uint32_t>& seen, std::unordered_map<uint32_t, int>& memo)
{
    int size = 0;
    for (const TypeMember& member : members) {
        if (member.memberType == MT_INLINE && member.referenceType.valid()) {
            const uint32_t key = uint32_t(member.referenceType.section) * kFakeSectionStride +
                uint32_t(member.referenceType.offset);
            if (seen.count(key)) continue;
            const auto memoIt = memo.find(key);
            if (memoIt != memo.end()) {
                size += memoIt->second;
                continue;
            }
            std::set<uint32_t> childSeen = seen;
            childSeen.insert(key);
            const auto sub = ParseTypeTree(loaded, member.referenceType);
            const int subSize = ObjectStorageSizeImpl(loaded, sub, childSeen, memo);
            memo[key] = subSize;
            size += subSize;
        } else {
            size += MemberStorageSize(member, loaded.pointerSize);
        }
    }
    return size;
}

} // namespace

uint32_t ReadU32(const std::vector<uint8_t>& data, size_t offset, bool littleEndian)
{
    if (offset + 4 > data.size()) throw std::runtime_error("gr2 type-tree read out of range");
    if (littleEndian) {
        return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
            (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
    }
    return uint32_t(data[offset + 3]) | (uint32_t(data[offset + 2]) << 8) |
        (uint32_t(data[offset + 1]) << 16) | (uint32_t(data[offset]) << 24);
}

uint32_t ReadPointer(const std::vector<uint8_t>& data, size_t offset, int pointerSize, bool littleEndian)
{
    if (pointerSize == 4) return ReadU32(data, offset, littleEndian);
    const uint32_t lo = ReadU32(data, offset, littleEndian);
    ReadU32(data, offset + 4, littleEndian); // hi unused for fake pointers
    return lo;
}

float ReadF32(const std::vector<uint8_t>& data, size_t offset, bool littleEndian)
{
    const uint32_t bits = ReadU32(data, offset, littleEndian);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t MakeFakePointer(int section, int offset)
{
    return kFakePointerBase + uint32_t(section) * kFakeSectionStride + uint32_t(offset);
}

SectionRef DecodeFakePointer(uint32_t pointer, int sectionCount)
{
    if (pointer < kFakePointerBase) return {};
    const uint32_t value = pointer - kFakePointerBase;
    const int section = int(value / kFakeSectionStride);
    const int offset = int(value - uint32_t(section) * kFakeSectionStride);
    if (section < 0 || section >= sectionCount) return {};
    return {section, offset};
}

Gr2Transform ReadTransformBytes(const LoadedGr2& loaded, int section, int offset)
{
    Gr2Transform identity{};
    if (section < 0 || section >= int(loaded.sectionsOriginal.size())) return identity;
    const auto& data = loaded.sectionsOriginal[size_t(section)];
    if (offset < 0 || size_t(offset) + 68 > data.size()) return identity;
    const bool little = !loaded.file.byteReversed;
    identity.flags = ReadU32(data, size_t(offset), little);
    for (int i = 0; i < 3; ++i) identity.position[i] = ReadF32(data, size_t(offset + 4 + i * 4), little);
    for (int i = 0; i < 4; ++i) identity.orientation[i] = ReadF32(data, size_t(offset + 16 + i * 4), little);
    for (int i = 0; i < 9; ++i) identity.scaleShear[i] = ReadF32(data, size_t(offset + 32 + i * 4), little);
    return identity;
}

std::string ReadStringAt(const LoadedGr2& loaded, SectionRef ref, int maxLength)
{
    if (!ref.valid() || ref.section >= int(loaded.sectionsOriginal.size())) return {};
    const auto& data = loaded.sectionsOriginal[size_t(ref.section)];
    if (ref.offset < 0 || ref.offset >= int(data.size())) return {};
    const bool little = !loaded.file.byteReversed;
    if (size_t(ref.offset) + 4 <= data.size()) {
        const uint32_t length = ReadU32(data, size_t(ref.offset), little);
        const size_t end = size_t(ref.offset) + 4 + length;
        if (length > 0 && int(length) <= maxLength && end <= data.size()) {
            if (LooksText(data.data() + ref.offset + 4, length)) {
                std::string text(reinterpret_cast<const char*>(data.data() + ref.offset + 4), length);
                while (!text.empty() && text.back() == '\0') text.pop_back();
                return text;
            }
        }
    }
    const size_t cap = (std::min)(data.size(), size_t(ref.offset) + size_t(maxLength));
    size_t end = cap;
    for (size_t i = size_t(ref.offset); i < cap; ++i) {
        if (data[i] == 0) {
            end = i;
            break;
        }
    }
    const size_t len = end - size_t(ref.offset);
    if (!LooksText(data.data() + ref.offset, len)) return {};
    return std::string(reinterpret_cast<const char*>(data.data() + ref.offset), len);
}

bool LoadGr2(const File& file, LoadedGr2& out, std::string& error)
{
    out = {};
    error.clear();
    try {
        out.file = file;
        out.pointerSize = file.pointerSize / 8;
        if (out.pointerSize != 4 && out.pointerSize != 8) {
            error = "unsupported pointer size";
            return false;
        }
        const size_t sectionCount = file.sections.size();
        out.sectionsOriginal.resize(sectionCount);
        out.sectionsFixed.resize(sectionCount);
        for (size_t i = 0; i < sectionCount; ++i) {
            std::vector<uint8_t> expanded;
            if (!DecompressSection(file.sections[i], file.sectionBytes(i), expanded, error)) return false;
            out.sectionsOriginal[i] = expanded;
            out.sectionsFixed[i] = expanded;
        }

        const bool little = !file.byteReversed;
        struct PointerFixup {
            int sourceSection = 0;
            uint32_t sourceOffset = 0;
            SectionRef target;
        };
        std::vector<PointerFixup> pointerFixups;
        int mixedCount = 0;
        for (size_t s = 0; s < sectionCount; ++s) {
            const Section& section = file.sections[s];
            if (section.pointerFixupCount && section.pointerFixupOffset) {
                const uint32_t count = section.pointerFixupCount;
                const uint32_t offset = section.pointerFixupOffset;
                if (size_t(offset) + size_t(count) * 12 > file.data.size()) {
                    error = "pointer fixup table escapes file";
                    return false;
                }
                for (uint32_t i = 0; i < count; ++i) {
                    const size_t base = size_t(offset) + size_t(i) * 12;
                    PointerFixup fixup;
                    fixup.sourceSection = int(s);
                    fixup.sourceOffset = ReadU32(file.data, base, little);
                    fixup.target = {int(ReadU32(file.data, base + 4, little)),
                        int(ReadU32(file.data, base + 8, little))};
                    pointerFixups.push_back(fixup);
                }
            }
            if (section.mixedMarshallingCount && section.mixedMarshallingOffset) {
                mixedCount += int(section.mixedMarshallingCount);
            }
        }
        if (file.byteReversed && mixedCount > 0) {
            error = "cross-endian asset is not supported";
            return false;
        }
        for (const PointerFixup& fixup : pointerFixups) {
            if (fixup.sourceSection >= int(sectionCount)) continue;
            auto& data = out.sectionsFixed[size_t(fixup.sourceSection)];
            if (size_t(fixup.sourceOffset) + size_t(out.pointerSize) > data.size()) continue;
            const uint32_t fake = MakeFakePointer(fixup.target.section, fixup.target.offset);
            WritePointer(data, fixup.sourceOffset, fake, out.pointerSize, little);
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        out = {};
        return false;
    }
}

std::vector<TypeMember> ParseTypeTree(const LoadedGr2& loaded, SectionRef ref, int maxMembers)
{
    std::vector<TypeMember> entries;
    if (!ref.valid() || ref.section >= int(loaded.sectionsFixed.size())) return entries;
    const auto& data = loaded.sectionsFixed[size_t(ref.section)];
    const bool little = !loaded.file.byteReversed;
    int offset = ref.offset;
    for (int i = 0; i < maxMembers; ++i) {
        if (size_t(offset) + 32 > data.size()) break;
        TypeMember member;
        member.memberType = int(ReadU32(data, size_t(offset), little));
        if (member.memberType == MT_END) break;
        const uint32_t namePtr = ReadU32(data, size_t(offset + 4), little);
        const uint32_t typePtr = ReadU32(data, size_t(offset + 8), little);
        member.arrayWidth = ReadU32(data, size_t(offset + 12), little);
        member.extra[0] = ReadU32(data, size_t(offset + 16), little);
        member.extra[1] = ReadU32(data, size_t(offset + 20), little);
        member.extra[2] = ReadU32(data, size_t(offset + 24), little);
        member.offset = offset;
        member.name = ReadStringAt(loaded, DecodeFakePointer(namePtr, int(loaded.sectionsFixed.size())));
        if (member.name.empty()) member.name = "member_" + std::to_string(entries.size());
        member.referenceType = DecodeFakePointer(typePtr, int(loaded.sectionsFixed.size()));
        if (member.arrayWidth == 0) member.arrayWidth = 1;
        entries.push_back(std::move(member));
        offset += 32;
    }
    return entries;
}

int ObjectStorageSize(const LoadedGr2& loaded, const std::vector<TypeMember>& members,
    std::unordered_map<uint32_t, int>* memo)
{
    std::unordered_map<uint32_t, int> localMemo;
    std::unordered_map<uint32_t, int>& useMemo = memo ? *memo : localMemo;
    std::set<uint32_t> seen;
    return ObjectStorageSizeImpl(loaded, members, seen, useMemo);
}

ParsedObject ParseObject(const LoadedGr2& loaded, const std::vector<TypeMember>& typeTree, SectionRef ref,
    int maxArrayRefs, int depth)
{
    ParsedObject out;
    if (!ref.valid() || ref.section >= int(loaded.sectionsFixed.size())) return out;
    const bool little = !loaded.file.byteReversed;
    const auto& fixed = loaded.sectionsFixed[size_t(ref.section)];
    const auto& raw = loaded.sectionsOriginal[size_t(ref.section)];
    const int sectionCount = int(loaded.sectionsFixed.size());
    int offset = ref.offset;
    for (const TypeMember& member : typeTree) {
        const int size = MemberStorageSize(member, loaded.pointerSize);
        if (size > 0 && size_t(offset) + size_t(size) > fixed.size()) break;
        ParsedField field;
        field.memberType = member.memberType;
        field.offset = offset;
        field.referenceType = member.referenceType;
        const int t = member.memberType;
        if (t == MT_REFERENCE || t == MT_EMPTY_REFERENCE) {
            const uint32_t pointer = ReadPointer(fixed, size_t(offset), loaded.pointerSize, little);
            field.target = DecodeFakePointer(pointer, sectionCount);
        } else if (t == MT_STRING) {
            const uint32_t pointer = ReadPointer(fixed, size_t(offset), loaded.pointerSize, little);
            field.target = DecodeFakePointer(pointer, sectionCount);
            field.text = ReadStringAt(loaded, field.target);
            field.hasText = true;
        } else if (t == MT_REFERENCE_TO_ARRAY || t == MT_ARRAY_OF_REFERENCES) {
            field.count = ReadU32(raw, size_t(offset), little);
            const uint32_t pointer = ReadPointer(fixed, size_t(offset + 4), loaded.pointerSize, little);
            field.target = DecodeFakePointer(pointer, sectionCount);
            field.truncated = field.count > uint32_t(maxArrayRefs);
            if (t == MT_ARRAY_OF_REFERENCES) {
                field.elementRefs = ReadArrayReferences(loaded, field.target, field.count, maxArrayRefs);
            }
        } else if (t == MT_REFERENCE_TO_VARIANT_ARRAY) {
            const uint32_t typePointer = ReadPointer(fixed, size_t(offset), loaded.pointerSize, little);
            field.count = ReadU32(raw, size_t(offset) + size_t(loaded.pointerSize), little);
            const uint32_t objectPointer =
                ReadPointer(fixed, size_t(offset) + size_t(loaded.pointerSize) + 4, loaded.pointerSize, little);
            field.variantType = DecodeFakePointer(typePointer, sectionCount);
            field.target = DecodeFakePointer(objectPointer, sectionCount);
            field.truncated = field.count > uint32_t(maxArrayRefs);
        } else if (t == MT_VARIANT_REFERENCE) {
            const uint32_t typePointer = ReadPointer(fixed, size_t(offset), loaded.pointerSize, little);
            const uint32_t objectPointer =
                ReadPointer(fixed, size_t(offset) + size_t(loaded.pointerSize), loaded.pointerSize, little);
            field.variantType = DecodeFakePointer(typePointer, sectionCount);
            field.target = DecodeFakePointer(objectPointer, sectionCount);
        } else if (t == MT_INT32) {
            field.number = int32_t(ReadU32(raw, size_t(offset), little));
            field.hasNumber = true;
        } else if (t == MT_UINT32) {
            field.number = ReadU32(raw, size_t(offset), little);
            field.hasNumber = true;
        } else if (t == MT_INT16 || t == MT_UINT16) {
            if (size_t(offset) + 2 > raw.size()) break;
            const uint16_t v = little
                ? uint16_t(raw[size_t(offset)] | (uint16_t(raw[size_t(offset) + 1]) << 8))
                : uint16_t(raw[size_t(offset) + 1] | (uint16_t(raw[size_t(offset)]) << 8));
            field.number = (t == MT_INT16) ? double(int16_t(v)) : double(v);
            field.hasNumber = true;
        } else if (t == MT_INT8) {
            field.number = int8_t(raw[size_t(offset)]);
            field.hasNumber = true;
        } else if (t == MT_UINT8) {
            field.number = raw[size_t(offset)];
            field.hasNumber = true;
        } else if (t == MT_REAL32) {
            field.number = ReadF32(raw, size_t(offset), little);
            field.hasNumber = true;
        } else if (t == MT_TRANSFORM) {
            field.transform = ReadTransformBytes(loaded, ref.section, offset);
            field.hasTransform = true;
        } else if (t == MT_INLINE && member.referenceType.valid()) {
            if (depth >= kMaxInlineDepth) throw std::runtime_error("inline nesting exceeds MAX_INLINE_DEPTH");
            const auto subTree = ParseTypeTree(loaded, member.referenceType);
            field.inlineObject = ParseObject(loaded, subTree, {ref.section, offset}, maxArrayRefs, depth + 1);
            field.hasInline = true;
            const int subSize = ObjectStorageSize(loaded, subTree);
            offset += subSize;
            out.emplace(member.name, std::move(field));
            continue;
        }
        out.emplace(member.name, std::move(field));
        offset += size;
    }
    return out;
}

std::vector<ReferenceArrayObject> ReadReferenceArrayObjects(const LoadedGr2& loaded, SectionRef arrayRef,
    uint32_t count, SectionRef typeRef, int maxCount)
{
    std::vector<ReferenceArrayObject> out;
    if (!arrayRef.valid() || !typeRef.valid() || count == 0) return out;
    const auto typeMembers = ParseTypeTree(loaded, typeRef);
    const int stride = ObjectStorageSize(loaded, typeMembers);
    if (stride <= 0) return out;
    const int limit = int((std::min)(count, uint32_t(maxCount)));
    out.reserve(size_t(limit));
    for (int i = 0; i < limit; ++i) {
        ReferenceArrayObject entry;
        entry.ref = {arrayRef.section, arrayRef.offset + i * stride};
        entry.fields = ParseObject(loaded, typeMembers, entry.ref, maxCount);
        out.push_back(std::move(entry));
    }
    return out;
}

} // namespace ro::gr2
