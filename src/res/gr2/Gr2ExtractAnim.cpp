// Animation / curve extractors. MIT attribution: third_party/gr2-reader/LICENSE.
#include "Gr2Extract.h"
#include <algorithm>
#include <cmath>
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

constexpr float ONE_OVER_SQRT2 = 0.707106781f;
static const float QUATERNION_SCALE_OFFSET_TABLE[16][2] = {
    {ONE_OVER_SQRT2 * 2.f, -ONE_OVER_SQRT2},
    {ONE_OVER_SQRT2 * 1.f, -ONE_OVER_SQRT2 * 0.5f},
    {ONE_OVER_SQRT2 * 0.5f, -ONE_OVER_SQRT2 * 0.75f},
    {ONE_OVER_SQRT2 * 0.5f, -ONE_OVER_SQRT2 * 0.25f},
    {ONE_OVER_SQRT2 * 0.5f, ONE_OVER_SQRT2 * 0.25f},
    {ONE_OVER_SQRT2 * 0.25f, -ONE_OVER_SQRT2 * 0.250f},
    {ONE_OVER_SQRT2 * 0.25f, -ONE_OVER_SQRT2 * 0.125f},
    {ONE_OVER_SQRT2 * 0.25f, ONE_OVER_SQRT2 * 0.000f},
    {-ONE_OVER_SQRT2 * 2.f, ONE_OVER_SQRT2},
    {-ONE_OVER_SQRT2 * 1.f, ONE_OVER_SQRT2 * 0.5f},
    {-ONE_OVER_SQRT2 * 0.5f, ONE_OVER_SQRT2 * 0.75f},
    {-ONE_OVER_SQRT2 * 0.5f, ONE_OVER_SQRT2 * 0.25f},
    {-ONE_OVER_SQRT2 * 0.5f, -ONE_OVER_SQRT2 * 0.25f},
    {-ONE_OVER_SQRT2 * 0.25f, ONE_OVER_SQRT2 * 0.250f},
    {-ONE_OVER_SQRT2 * 0.25f, ONE_OVER_SQRT2 * 0.125f},
    {-ONE_OVER_SQRT2 * 0.25f, -ONE_OVER_SQRT2 * 0.000f},
};

int ScalarMemberSize(const TypeMember& member, int pointerSize)
{
    const int width = member.arrayWidth ? int(member.arrayWidth) : 1;
    const int t = member.memberType;
    if (t == MT_REAL32 || t == MT_INT32 || t == MT_UINT32) return 4 * width;
    if (t == MT_INT16 || t == MT_UINT16 || t == MT_BINORMAL_INT16 || t == MT_NORMAL_UINT16 || t == MT_REAL16)
        return 2 * width;
    if (t == MT_INT8 || t == MT_UINT8 || t == MT_BINORMAL_INT8 || t == MT_NORMAL_UINT8) return width;
    if (t == MT_REFERENCE || t == MT_EMPTY_REFERENCE || t == MT_STRING) return pointerSize;
    if (t == MT_REFERENCE_TO_ARRAY || t == MT_ARRAY_OF_REFERENCES) return 4 + pointerSize;
    if (t == MT_VARIANT_REFERENCE) return pointerSize * 2;
    if (t == MT_REFERENCE_TO_VARIANT_ARRAY) return pointerSize + 4 + pointerSize;
    if (t == MT_TRANSFORM) return 68;
    if (t == MT_INLINE) return 0;
    return pointerSize;
}

std::unordered_map<std::string, int> MemberOffsetsFor(const LoadedGr2& loaded,
    const std::vector<TypeMember>& members)
{
    std::unordered_map<std::string, int> out;
    int offset = 0;
    for (const TypeMember& member : members) {
        out[member.name] = offset;
        if (member.memberType == MT_INLINE && member.referenceType.valid()) {
            const auto sub = ParseTypeTree(loaded, member.referenceType);
            offset += ObjectStorageSize(loaded, sub);
        } else {
            offset += ScalarMemberSize(member, loaded.pointerSize);
        }
    }
    return out;
}

float FloatFromHighU16(uint16_t value)
{
    const uint32_t bits = uint32_t(value) << 16;
    float out = 0;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

int CurveDimension(const std::string& codec)
{
    if (codec.size() >= 2 && codec[0] == 'D') {
        const int digit = codec[1] - '0';
        if (digit >= 0 && digit <= 9) return digit;
    }
    return 0;
}

void QuaternionScalesOffsets(uint16_t entries, float quantizedMax, float scales[4], float offsets[4])
{
    uint32_t bits = entries;
    for (int i = 0; i < 4; ++i) {
        const int index = int(bits & 0xf);
        bits >>= 4;
        scales[i] = QUATERNION_SCALE_OFFSET_TABLE[index][0] / quantizedMax;
        offsets[i] = QUATERNION_SCALE_OFFSET_TABLE[index][1];
    }
}

SectionRef ReadReferenceArrayPointer(const LoadedGr2& loaded, const std::vector<uint8_t>& fixed, int offset)
{
    if (offset + 4 + loaded.pointerSize > int(fixed.size())) return {};
    const bool little = !loaded.file.byteReversed;
    const uint32_t pointer = ReadPointer(fixed, size_t(offset + 4), loaded.pointerSize, little);
    return DecodeFakePointer(pointer, int(loaded.sectionsFixed.size()));
}

uint16_t ReadU16(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset + 2 > data.size()) return 0;
    return uint16_t(data[offset] | (uint16_t(data[offset + 1]) << 8));
}

uint32_t ReadU32Local(const std::vector<uint8_t>& data, size_t offset)
{
    if (offset + 4 > data.size()) return 0;
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
        (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

float ReadF32Local(const std::vector<uint8_t>& data, size_t offset)
{
    const uint32_t bits = ReadU32Local(data, offset);
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

struct CurveData {
    std::string codec;
    int format = -1;
    int degree = 0;
    int dimension = 0;
    std::vector<float> knots;
    std::vector<float> controls;
    std::vector<float> sample;
};

void DecodeD3K16uC16u(const LoadedGr2& loaded, const std::string& codec, int dimension,
    const std::vector<uint8_t>& raw, int objectOffset,
    const std::unordered_map<std::string, int>& offsets, int knotControlCount, SectionRef arrayRef,
    CurveData& out)
{
    const int knotCount = knotControlCount / (codec == "D3I1K16uC16u" ? 2 : dimension + 1);
    if (knotCount <= 0) return;
    const auto oneIt = offsets.find("OneOverKnotScaleTrunc");
    const auto scalesIt = offsets.find("ControlScales");
    const auto offsIt = offsets.find("ControlOffsets");
    const int oneOverOffset = objectOffset + (oneIt != offsets.end() ? oneIt->second : 2);
    const int scalesOffset = objectOffset + (scalesIt != offsets.end() ? scalesIt->second : 4);
    const int offsetsOffset = objectOffset + (offsIt != offsets.end() ? offsIt->second : 16);
    if (offsetsOffset + dimension * 4 > int(raw.size())) return;
    const float oneOver = FloatFromHighU16(ReadU16(raw, size_t(oneOverOffset)));
    if (oneOver == 0.f) return;
    auto controlScales = std::vector<float>(size_t(dimension));
    auto controlOffsets = std::vector<float>(size_t(dimension));
    for (int d = 0; d < dimension; ++d) {
        controlScales[size_t(d)] = ReadF32Local(raw, size_t(scalesOffset + d * 4));
        controlOffsets[size_t(d)] = ReadF32Local(raw, size_t(offsetsOffset + d * 4));
    }
    const auto& arrayData = loaded.sectionsFixed[size_t(arrayRef.section)];
    const size_t byteCount = size_t(knotControlCount) * 2u;
    if (size_t(arrayRef.offset) + byteCount > arrayData.size()) return;
    const float knotScale = 1.f / oneOver;
    out.knots.resize(size_t(knotCount));
    for (int i = 0; i < knotCount; ++i)
        out.knots[size_t(i)] = knotScale * float(ReadU16(arrayData, size_t(arrayRef.offset + i * 2)));
    out.controls.resize(size_t(knotCount * dimension));
    if (codec == "D3I1K16uC16u") {
        for (int i = 0; i < knotCount; ++i) {
            const float param = float(ReadU16(arrayData, size_t(arrayRef.offset + (knotCount + i) * 2)));
            const int base = i * dimension;
            for (int d = 0; d < dimension; ++d)
                out.controls[size_t(base + d)] = controlOffsets[size_t(d)] + controlScales[size_t(d)] * param;
        }
    } else {
        for (int i = 0; i < knotCount; ++i) {
            const int base = i * dimension;
            const int srcBase = arrayRef.offset + (knotCount + i * dimension) * 2;
            for (int d = 0; d < dimension; ++d) {
                const float param = float(ReadU16(arrayData, size_t(srcBase + d * 2)));
                out.controls[size_t(base + d)] = controlOffsets[size_t(d)] + controlScales[size_t(d)] * param;
            }
        }
    }
}

void DecodeD4nK8uC7u(const LoadedGr2& loaded, const std::vector<uint8_t>& raw, int objectOffset,
    const std::unordered_map<std::string, int>& offsets, int knotControlCount, SectionRef arrayRef,
    CurveData& out)
{
    const int knotCount = knotControlCount / 4;
    if (knotCount <= 0) return;
    const auto scaleIt = offsets.find("ScaleOffsetTableEntries");
    const auto oneIt = offsets.find("OneOverKnotScale");
    const int scaleEntriesOffset = objectOffset + (scaleIt != offsets.end() ? scaleIt->second : 2);
    const int oneOverOffset = objectOffset + (oneIt != offsets.end() ? oneIt->second : 4);
    if (oneOverOffset + 4 > int(raw.size())) return;
    const float oneOver = ReadF32Local(raw, size_t(oneOverOffset));
    if (oneOver == 0.f) return;
    const auto& arrayData = loaded.sectionsFixed[size_t(arrayRef.section)];
    if (size_t(arrayRef.offset) + size_t(knotControlCount) > arrayData.size()) return;
    const float knotScale = 1.f / oneOver;
    out.knots.resize(size_t(knotCount));
    for (int i = 0; i < knotCount; ++i)
        out.knots[size_t(i)] = knotScale * float(arrayData[size_t(arrayRef.offset + i)]);
    float scales[4], tableOffsets[4];
    QuaternionScalesOffsets(ReadU16(raw, size_t(scaleEntriesOffset)), 127.f, scales, tableOffsets);
    out.controls.resize(size_t(knotCount * 4));
    const int controlBase = arrayRef.offset + knotCount;
    for (int i = 0; i < knotCount; ++i) {
        const uint8_t cur0 = arrayData[size_t(controlBase + i * 3 + 0)];
        const uint8_t cur1 = arrayData[size_t(controlBase + i * 3 + 1)];
        const uint8_t cur2 = arrayData[size_t(controlBase + i * 3 + 2)];
        const bool missingNegative = (cur0 & 0x80) != 0;
        const int missingIndex = (((cur1 >> 6) & 0x2) | (cur2 >> 7)) & 0x3;
        float result[4] = {};
        int dst = missingIndex;
        float summedSq = 0.f;
        const uint8_t srcBytes[3] = {cur0, cur1, cur2};
        for (int src = 0; src < 3; ++src) {
            dst = (dst + 1) & 0x3;
            const float v = tableOffsets[dst] + scales[dst] * float(srcBytes[src] & 0x7f);
            result[dst] = v;
            summedSq += v * v;
        }
        const float missing = std::sqrt((std::max)(0.f, 1.f - summedSq));
        result[missingIndex] = missingNegative ? -missing : missing;
        const int base = i * 4;
        out.controls[size_t(base)] = result[0];
        out.controls[size_t(base + 1)] = result[1];
        out.controls[size_t(base + 2)] = result[2];
        out.controls[size_t(base + 3)] = result[3];
    }
}

void DecodeD4nK16uC15u(const LoadedGr2& loaded, const std::vector<uint8_t>& raw, int objectOffset,
    const std::unordered_map<std::string, int>& offsets, int knotControlCount, SectionRef arrayRef,
    CurveData& out)
{
    const int knotCount = knotControlCount / 4;
    if (knotCount <= 0) return;
    const auto scaleIt = offsets.find("ScaleOffsetTableEntries");
    const auto oneIt = offsets.find("OneOverKnotScale");
    const int scaleEntriesOffset = objectOffset + (scaleIt != offsets.end() ? scaleIt->second : 2);
    const int oneOverOffset = objectOffset + (oneIt != offsets.end() ? oneIt->second : 4);
    if (oneOverOffset + 4 > int(raw.size())) return;
    const float oneOver = ReadF32Local(raw, size_t(oneOverOffset));
    if (oneOver == 0.f) return;
    const auto& arrayData = loaded.sectionsFixed[size_t(arrayRef.section)];
    const size_t byteCount = size_t(knotControlCount) * 2u;
    if (size_t(arrayRef.offset) + byteCount > arrayData.size()) return;
    const float knotScale = 1.f / oneOver;
    out.knots.resize(size_t(knotCount));
    for (int i = 0; i < knotCount; ++i)
        out.knots[size_t(i)] = knotScale * float(ReadU16(arrayData, size_t(arrayRef.offset + i * 2)));
    float scales[4], tableOffsets[4];
    QuaternionScalesOffsets(ReadU16(raw, size_t(scaleEntriesOffset)), 32767.f, scales, tableOffsets);
    out.controls.resize(size_t(knotCount * 4));
    const int controlBaseShorts = knotCount;
    for (int i = 0; i < knotCount; ++i) {
        const uint16_t cur0 = ReadU16(arrayData, size_t(arrayRef.offset + (controlBaseShorts + i * 3 + 0) * 2));
        const uint16_t cur1 = ReadU16(arrayData, size_t(arrayRef.offset + (controlBaseShorts + i * 3 + 1) * 2));
        const uint16_t cur2 = ReadU16(arrayData, size_t(arrayRef.offset + (controlBaseShorts + i * 3 + 2) * 2));
        const bool missingNegative = (cur0 & 0x8000) != 0;
        const int missingIndex = (((cur1 >> 14) & 0x2) | (cur2 >> 15)) & 0x3;
        float result[4] = {};
        int dst = missingIndex;
        float summedSq = 0.f;
        const uint16_t srcShorts[3] = {cur0, cur1, cur2};
        for (int src = 0; src < 3; ++src) {
            dst = (dst + 1) & 0x3;
            const float v = tableOffsets[dst] + scales[dst] * float(srcShorts[src] & 0x7fff);
            result[dst] = v;
            summedSq += v * v;
        }
        const float missing = std::sqrt((std::max)(0.f, 1.f - summedSq));
        result[missingIndex] = missingNegative ? -missing : missing;
        const int base = i * 4;
        out.controls[size_t(base)] = result[0];
        out.controls[size_t(base + 1)] = result[1];
        out.controls[size_t(base + 2)] = result[2];
        out.controls[size_t(base + 3)] = result[3];
    }
}

void ReadStaticCurveValue(const std::string& codec, int dimension, const std::vector<uint8_t>& raw,
    int offset, CurveData& out)
{
    if (dimension <= 0) return;
    if (codec.size() >= 11 && codec.compare(codec.size() - 11, 11, "Constant32f") == 0) {
        const int controlsOffset = offset + 4;
        if (controlsOffset + dimension * 4 > int(raw.size())) return;
        out.sample.resize(size_t(dimension));
        for (int d = 0; d < dimension; ++d)
            out.sample[size_t(d)] = ReadF32Local(raw, size_t(controlsOffset + d * 4));
        return;
    }
    if (codec == "DaIdentity") {
        if (dimension == 9) out.sample = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        else if (dimension == 4) out.sample = {0, 0, 0, 1};
        else out.sample.assign(size_t(dimension), 0.f);
    }
}

bool ReadLegacyCurveMetadata(const LoadedGr2& loaded, int section, int offset, CurveData& out)
{
    const int pointerSize = loaded.pointerSize;
    const auto& raw = loaded.sectionsOriginal[size_t(section)];
    const auto& fixed = loaded.sectionsFixed[size_t(section)];
    if (offset + 4 + (4 + pointerSize) * 2 > int(raw.size())) return false;
    const int32_t degree = int32_t(ReadU32Local(raw, size_t(offset)));
    if (degree < 0 || degree > 3) return false;
    const uint32_t knotCount = ReadU32Local(raw, size_t(offset + 4));
    const uint32_t controlCount = ReadU32Local(raw, size_t(offset + 4 + pointerSize + 4));
    if (knotCount == 0 && controlCount == 0) return false;
    if (knotCount == 0 || controlCount == 0 || controlCount % knotCount != 0) return false;
    const int dimension = int(controlCount / knotCount);
    if (dimension <= 0 || dimension > 16) return false;
    const bool little = !loaded.file.byteReversed;
    const SectionRef knotsRef = DecodeFakePointer(
        ReadPointer(fixed, size_t(offset + 4 + 4), pointerSize, little), int(loaded.sectionsFixed.size()));
    const SectionRef controlsRef = DecodeFakePointer(
        ReadPointer(fixed, size_t(offset + 4 + pointerSize + 4 + 4), pointerSize, little),
        int(loaded.sectionsFixed.size()));
    if (!knotsRef.valid() || !controlsRef.valid()) return false;
    const auto& knotBytes = loaded.sectionsFixed[size_t(knotsRef.section)];
    const auto& controlBytes = loaded.sectionsFixed[size_t(controlsRef.section)];
    if (size_t(knotsRef.offset) + knotCount * 4u > knotBytes.size()) return false;
    if (size_t(controlsRef.offset) + controlCount * 4u > controlBytes.size()) return false;
    out.codec = "LegacyCurve32f";
    out.format = -1;
    out.degree = degree;
    out.dimension = dimension;
    out.knots.resize(knotCount);
    for (uint32_t i = 0; i < knotCount; ++i)
        out.knots[i] = ReadF32Local(knotBytes, size_t(knotsRef.offset) + i * 4u);
    out.controls.resize(controlCount);
    for (uint32_t i = 0; i < controlCount; ++i)
        out.controls[i] = ReadF32Local(controlBytes, size_t(controlsRef.offset) + i * 4u);
    if (knotCount == 1) {
        out.sample.assign(out.controls.begin(), out.controls.begin() + dimension);
    }
    return true;
}

bool ReadCurveMetadata(const LoadedGr2& loaded, int parentSection, int curveOffset, CurveData& out)
{
    out = {};
    const int pointerSize = loaded.pointerSize;
    const auto& fixed = loaded.sectionsFixed[size_t(parentSection)];
    if (curveOffset + pointerSize * 2 > int(fixed.size())) return false;
    if (ReadLegacyCurveMetadata(loaded, parentSection, curveOffset, out)) return true;

    const bool little = !loaded.file.byteReversed;
    const SectionRef typeRef =
        DecodeFakePointer(ReadPointer(fixed, size_t(curveOffset), pointerSize, little),
            int(loaded.sectionsFixed.size()));
    const SectionRef objectRef = DecodeFakePointer(
        ReadPointer(fixed, size_t(curveOffset + pointerSize), pointerSize, little),
        int(loaded.sectionsFixed.size()));
    if (!typeRef.valid() || !objectRef.valid()) return false;
    const auto members = ParseTypeTree(loaded, typeRef);
    if (members.empty()) return false;
    std::string codec = members[0].name;
    const std::string prefix = "CurveDataHeader_";
    if (codec.rfind(prefix, 0) == 0) codec = codec.substr(prefix.size());
    const auto offsets = MemberOffsetsFor(loaded, members);
    const auto& raw = loaded.sectionsOriginal[size_t(objectRef.section)];
    if (size_t(objectRef.offset) + 2 > raw.size()) return false;
    out.codec = codec;
    out.format = raw[size_t(objectRef.offset)];
    out.degree = raw[size_t(objectRef.offset) + 1];
    out.dimension = CurveDimension(codec);
    if (codec == "DaIdentity") {
        const auto dimIt = offsets.find("Dimension");
        const int dimOffset = objectRef.offset + (dimIt != offsets.end() ? dimIt->second : 2);
        if (dimOffset + 2 <= int(raw.size())) out.dimension = int16_t(ReadU16(raw, size_t(dimOffset)));
    }
    int knotControlCount = 0;
    const auto kcIt = offsets.find("KnotsControls");
    if (kcIt != offsets.end() && objectRef.offset + kcIt->second + 4 <= int(raw.size()))
        knotControlCount = int(ReadU32Local(raw, size_t(objectRef.offset + kcIt->second)));

    if (knotControlCount > 0 && out.dimension > 0 && kcIt != offsets.end()) {
        const SectionRef arrayRef =
            ReadReferenceArrayPointer(loaded, loaded.sectionsFixed[size_t(objectRef.section)],
                objectRef.offset + kcIt->second);
        if (arrayRef.valid()) {
            if (codec == "D3K16uC16u" || codec == "D3I1K16uC16u") {
                DecodeD3K16uC16u(loaded, codec, out.dimension, raw, objectRef.offset, offsets,
                    knotControlCount, arrayRef, out);
            } else if (codec == "D4nK8uC7u") {
                DecodeD4nK8uC7u(loaded, raw, objectRef.offset, offsets, knotControlCount, arrayRef, out);
            } else if (codec == "D4nK16uC15u") {
                DecodeD4nK16uC15u(loaded, raw, objectRef.offset, offsets, knotControlCount, arrayRef, out);
            }
        }
    }
    ReadStaticCurveValue(codec, out.dimension, raw, objectRef.offset, out);
    return true;
}

ExtractedCurve ToExtractedCurve(const CurveData& data)
{
    ExtractedCurve curve;
    curve.degree = data.degree;
    curve.dimension = data.dimension;
    curve.knots.assign(data.knots.begin(), data.knots.end());
    curve.controls.assign(data.controls.begin(), data.controls.end());
    curve.sample.assign(data.sample.begin(), data.sample.end());
    return curve;
}

ExtractedTrack ReadTransformTrack(const LoadedGr2& loaded, const ReferenceArrayObject& parsedTrack)
{
    ExtractedTrack track;
    track.name = TextOr(Field(parsedTrack.fields, "Name"), "TransformTrack");
    const ParsedField* orientationField = Field(parsedTrack.fields, "OrientationCurve");
    const ParsedField* positionField = Field(parsedTrack.fields, "PositionCurve");
    const ParsedField* scaleField = Field(parsedTrack.fields, "ScaleShearCurve");
    CurveData curve;
    if (orientationField &&
        ReadCurveMetadata(loaded, parsedTrack.ref.section, orientationField->offset, curve))
        track.orientation = ToExtractedCurve(curve);
    if (positionField &&
        ReadCurveMetadata(loaded, parsedTrack.ref.section, positionField->offset, curve))
        track.position = ToExtractedCurve(curve);
    if (scaleField && ReadCurveMetadata(loaded, parsedTrack.ref.section, scaleField->offset, curve))
        track.scale = ToExtractedCurve(curve);
    return track;
}

struct TrackGroupData {
    std::string name;
    std::vector<ExtractedTrack> tracks;
};

TrackGroupData ReadTrackGroup(const LoadedGr2& loaded, SectionRef ref,
    const std::vector<TypeMember>& typeMembers, int maxTracks)
{
    TrackGroupData group;
    const auto fields = ParseObject(loaded, typeMembers, ref, maxTracks);
    group.name = TextOr(Field(fields, "Name"), "TrackGroup");
    const ParsedField* trackField = Field(fields, "TransformTracks");
    if (!trackField || !trackField->referenceType.valid()) return group;
    const auto parsed = ReadReferenceArrayObjects(loaded, trackField->target, trackField->count,
        trackField->referenceType, maxTracks);
    group.tracks.reserve(parsed.size());
    for (const auto& track : parsed) group.tracks.push_back(ReadTransformTrack(loaded, track));
    return group;
}

} // namespace

bool ExtractAnimations(const LoadedGr2& loaded, std::vector<ExtractedAnimation>& out, std::string& error)
{
    out.clear();
    error.clear();
    try {
        constexpr int maxTrackGroups = 64;
        constexpr int maxTracksPerGroup = 512;
        constexpr int maxAnimations = 64;
        const auto root = ParseObject(loaded, ParseTypeTree(loaded, RootTypeRef(loaded.file)),
            RootObjectRef(loaded.file), (std::max)(maxTrackGroups, maxAnimations));

        std::unordered_map<std::string, TrackGroupData> trackGroupByName;
        if (const ParsedField* trackGroupField = Field(root, "TrackGroups")) {
            if (trackGroupField->referenceType.valid() && !trackGroupField->elementRefs.empty()) {
                const auto tgTypeMembers = ParseTypeTree(loaded, trackGroupField->referenceType);
                const int limit =
                    int((std::min)(trackGroupField->elementRefs.size(), size_t(maxTrackGroups)));
                for (int i = 0; i < limit; ++i) {
                    const SectionRef ref = trackGroupField->elementRefs[size_t(i)];
                    if (!ref.valid()) continue;
                    TrackGroupData group = ReadTrackGroup(loaded, ref, tgTypeMembers, maxTracksPerGroup);
                    trackGroupByName[group.name] = std::move(group);
                }
            }
        }

        const ParsedField* animationField = Field(root, "Animations");
        if (!animationField || !animationField->referenceType.valid() ||
            animationField->elementRefs.empty())
            return true;

        const auto animTypeMembers = ParseTypeTree(loaded, animationField->referenceType);
        const int limit = int((std::min)(animationField->elementRefs.size(), size_t(maxAnimations)));
        for (int i = 0; i < limit; ++i) {
            const SectionRef ref = animationField->elementRefs[size_t(i)];
            if (!ref.valid()) continue;
            const auto fields = ParseObject(loaded, animTypeMembers, ref, 64);
            ExtractedAnimation animation;
            animation.duration = NumberOr(Field(fields, "Duration"), 0.0);

            // Resolve TrackGroups referenced by this animation (ARRAY_OF_REFERENCES).
            if (const ParsedField* tgRefs = Field(fields, "TrackGroups")) {
                if (tgRefs->referenceType.valid()) {
                    const auto tgMembers = ParseTypeTree(loaded, tgRefs->referenceType);
                    for (const SectionRef& tgRef : tgRefs->elementRefs) {
                        if (!tgRef.valid()) continue;
                        const auto tgFields = ParseObject(loaded, tgMembers, tgRef, 0);
                        const std::string name = TextOr(Field(tgFields, "Name"), "");
                        const auto it = trackGroupByName.find(name);
                        if (it == trackGroupByName.end()) continue;
                        for (const auto& track : it->second.tracks) animation.tracks.push_back(track);
                    }
                }
            }

            // iRO model files usually bundle one track group; attach it if refs
            // did not resolve by name.
            if (animation.tracks.empty()) {
                for (const auto& entry : trackGroupByName)
                    for (const auto& track : entry.second.tracks) animation.tracks.push_back(track);
            }

            out.push_back(std::move(animation));
        }
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        out.clear();
        return false;
    }
}

} // namespace ro::gr2
