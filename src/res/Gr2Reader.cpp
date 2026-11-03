#include "Gr2Reader.h"
#if RO_ENABLE_QT6_UI
#include "Gr2Reader.generated.h"
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QCoreApplication>
#endif
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ro::gr2 {
#if RO_ENABLE_QT6_UI
namespace {
std::vector<double> Numbers(const QJsonValue& value,size_t limit=1000000)
{
    const auto array=value.toArray();
    if(static_cast<size_t>(array.size())>limit)throw std::runtime_error("GR2 numeric array exceeds limit");
    std::vector<double> result;result.reserve(array.size());
    for(const auto& element:array) {
        const double number=element.toDouble(std::numeric_limits<double>::quiet_NaN());
        if(!std::isfinite(number))throw std::runtime_error("GR2 contains a non-finite number");
        result.push_back(number);
    }
    return result;
}
template<typename T,size_t N> void ReadArray(const QJsonValue& value,std::array<T,N>& target)
{
    const auto numbers=Numbers(value,N);
    if(numbers.size()!=N)throw std::runtime_error("GR2 vector has an invalid dimension");
    for(size_t i=0;i<N;++i)target[i]=static_cast<T>(numbers[i]);
}
Transform ReadTransform(const QJsonObject& object)
{
    Transform result;
    ReadArray(object["position"],result.position);ReadArray(object["orientation"],result.orientation);ReadArray(object["scaleShear"],result.scale);
    return result;
}
Curve ReadCurve(const QJsonValue& value)
{
    Curve result;if(value.isNull() || value.isUndefined())return result;
    const auto object=value.toObject();
    result.degree=object["degree"].toInt();result.dimension=object["dimension"].toInt();
    if(result.degree<0 || result.degree>2 || result.dimension<1 || result.dimension>9)
        throw std::runtime_error("Unsupported GR2 animation curve dimension/degree");
    result.knots=Numbers(object["knots"]);result.controls=Numbers(object["controls"]);result.sample=Numbers(object["sampleValue"],16);
    if(!std::is_sorted(result.knots.begin(),result.knots.end()))throw std::runtime_error("GR2 animation knots are not sorted");
    if(!result.controls.empty() && result.controls.size()!=result.knots.size()*result.dimension)
        throw std::runtime_error("GR2 animation control count is invalid");
    return result;
}
void Extract(const QJsonObject& root,Asset& asset,bool animationOnly)
{
    const auto animations=root["animations"].toArray();
    if(animations.size()>64)throw std::runtime_error("GR2 animation count exceeds limit");
    for(const auto& value:animations) {
        const auto object=value.toObject();Animation animation;
        animation.duration=object["duration"].toDouble();
        if(!std::isfinite(animation.duration) || animation.duration<0)throw std::runtime_error("GR2 animation duration is invalid");
        for(const auto& groupValue:object["trackGroups"].toArray()) {
            for(const auto& trackValue:groupValue.toObject()["transformTracks"].toArray()) {
                const auto track=trackValue.toObject();
                animation.tracks[track["name"].toString().toStdString()]={ReadCurve(track["positionCurve"]),ReadCurve(track["orientationCurve"]),ReadCurve(track["scaleShearCurve"])};
                if(animation.tracks.size()>4096)throw std::runtime_error("GR2 track count exceeds limit");
            }
        }
        asset.animations.push_back(std::move(animation));
    }
    if(animationOnly)return;
    const auto models=root["models"].toArray(),skeletons=root["skeletons"].toArray();
    const int skeletonIndex=models.isEmpty()?0:models[0].toObject()["skeletonIdx"].toInt();
    if(skeletonIndex<0 || skeletonIndex>=skeletons.size())throw std::runtime_error("GR2 model skeleton is missing");
    const auto bones=skeletons[skeletonIndex].toObject()["bones"].toArray();
    if(bones.isEmpty() || bones.size()>1024)throw std::runtime_error("GR2 bone count is invalid");
    std::unordered_map<std::string,int> boneIndices;
    for(const auto& value:bones) {
        const auto object=value.toObject();Bone bone;
        bone.name=object["name"].toString().toStdString();bone.parent=object["parentIndex"].toInt(-1);
        if(bone.parent>=static_cast<int>(asset.bones.size()))throw std::runtime_error("GR2 bone hierarchy is not parent-first");
        bone.bind=ReadTransform(object["transform"].toObject());ReadArray(object["inverseWorldTransform"],bone.inverse);
        boneIndices[bone.name]=static_cast<int>(asset.bones.size());asset.bones.push_back(std::move(bone));
    }
    const auto textures=root["textures"].toArray();
    if(textures.size()>64)throw std::runtime_error("GR2 texture count exceeds limit");
    std::unordered_map<std::string,int> textureIndices;
    for(const auto& value:textures) {
        const auto object=value.toObject();Image image;
        image.name=object["name"].toString().toStdString();image.width=object["width"].toInt();image.height=object["height"].toInt();image.alpha=object["alpha"].toInt()!=0;
        if(image.width<=0 || image.height<=0 || image.width>4096 || image.height>4096)throw std::runtime_error("GR2 texture dimensions are invalid");
        const size_t count=static_cast<size_t>(image.width)*image.height;
        const auto hex=object["pixels"].toString().toLatin1();
        if(static_cast<size_t>(hex.size())!=count*8)throw std::runtime_error("GR2 texture byte count is invalid");
        const QByteArray rgba=QByteArray::fromHex(hex);
        if(static_cast<size_t>(rgba.size())!=count*4)throw std::runtime_error("GR2 texture encoding is invalid");
        image.pixels.resize(count);
        const auto* bytes=reinterpret_cast<const unsigned char*>(rgba.constData());
        for(size_t i=0;i<count;++i)image.pixels[i]=(std::uint32_t(bytes[i*4+3])<<24)|(std::uint32_t(bytes[i*4])<<16)|(std::uint32_t(bytes[i*4+1])<<8)|bytes[i*4+2];
        textureIndices[image.name]=static_cast<int>(asset.images.size());asset.images.push_back(std::move(image));
    }
    const auto meshes=root["meshes"].toArray();
    if(meshes.size()>256)throw std::runtime_error("GR2 mesh count exceeds limit");
    std::vector<int> order;
    if(!models.isEmpty())for(const auto& binding:models[0].toObject()["meshBindings"].toArray())order.push_back(binding.toObject()["meshIdx"].toInt(-1));
    if(order.empty())for(int i=0;i<meshes.size();++i)order.push_back(i);
    for(int meshIndex:order) {
        if(meshIndex<0 || meshIndex>=meshes.size())throw std::runtime_error("GR2 mesh binding is out of range");
        const auto object=meshes[meshIndex].toObject();Mesh mesh;
        const auto positions=object["positions"].toArray(),normals=object["normals"].toArray(),uvs=object["uvs"].toArray(),weights=object["vertexWeights"].toArray();
        if(positions.size()>1000000 || normals.size()!=positions.size() || uvs.size()!=positions.size())throw std::runtime_error("GR2 vertex array dimensions differ");
        std::vector<int> bindings;
        for(const auto& binding:object["boneBindings"].toArray()) {
            const auto found=boneIndices.find(binding.toObject()["name"].toString().toStdString());
            if(found==boneIndices.end())throw std::runtime_error("GR2 mesh references an unknown skeleton bone");
            bindings.push_back(found->second);
        }
        mesh.vertices.resize(positions.size());
        for(int index=0;index<positions.size();++index) {
            auto& vertex=mesh.vertices[index];ReadArray(positions[index],vertex.position);ReadArray(normals[index],vertex.normal);ReadArray(uvs[index],vertex.uv);
            const auto influences=index<weights.size()?weights[index].toArray():QJsonArray{};
            if(influences.size()>16)throw std::runtime_error("GR2 vertex has too many bone influences");
            for(const auto& influence:influences) {
                const auto influenceObject=influence.toObject();const int local=influenceObject["boneIndex"].toInt(-1);
                const double weight=influenceObject["weight"].toDouble();
                if(local<0 || local>=static_cast<int>(bindings.size()) || !std::isfinite(weight) || weight<0 || weight>1)throw std::runtime_error("GR2 skin weight is invalid");
                if(weight>0)vertex.weights.push_back({bindings[local],static_cast<float>(weight)});
            }
            if(vertex.weights.empty() && !bindings.empty())vertex.weights.push_back({bindings[0],1});
        }
        const auto indices=object["indices"].toArray();
        if(indices.size()>3000000 || indices.size()%3)throw std::runtime_error("GR2 index array is invalid");
        for(const auto& index:indices) {
            const int number=index.toInt(-1);if(number<0 || number>=positions.size())throw std::runtime_error("GR2 vertex index is out of range");mesh.indices.push_back(number);
        }
        const auto materials=object["materials"].toArray();
        // Some authored helper meshes (Bow Guardian's upper-arm helpers)
        // have geometry but no material binding. The reference skips them.
        if(materials.isEmpty()) {asset.meshes.push_back(std::move(mesh));continue;}
        for(const auto& batchValue:object["triangleGroups"].toArray()) {
            const auto batchObject=batchValue.toObject();Batch batch;
            const int material=batchObject["materialIndex"].toInt(-1);
            if(material<0 || material>=materials.size())throw std::runtime_error("GR2 material index is out of range");
            const auto texture=textureIndices.find(materials[material].toObject()["textureFile"].toString().toStdString());
            if(texture!=textureIndices.end())batch.texture=texture->second;
            batch.first=batchObject["triFirst"].toInt(-1);batch.count=batchObject["triCount"].toInt(-1);
            if(batch.first<0 || batch.count<0 || static_cast<size_t>(batch.first+batch.count)>mesh.indices.size()/3)throw std::runtime_error("GR2 triangle batch is out of range");
            mesh.batches.push_back(batch);
        }
        asset.meshes.push_back(std::move(mesh));
    }
}
}
#endif

bool Decode(const std::vector<unsigned char>& bytes,Asset& asset,std::string& error,bool animationOnly)
{
    asset={};error.clear();
#if RO_ENABLE_QT6_UI
    if(!QCoreApplication::instance()) {error="GR2 reader requires the client's Qt application";return false;}
    if(bytes.empty() || bytes.size()>16u*1024u*1024u) {error="GR2 input size exceeds supported limits";return false;}
    try {
        QJSEngine engine;
        auto evaluated=engine.evaluate(QString::fromUtf8(kGr2ReaderScript),QStringLiteral("embedded-gr2-reader.js"));
        if(evaluated.isError())throw std::runtime_error(evaluated.toString().toStdString());
        const QByteArray input(reinterpret_cast<const char*>(bytes.data()),static_cast<qsizetype>(bytes.size()));
        auto read=engine.globalObject().property("OpenGr2Reader").property("read");
        const auto result=read.call({QString::fromLatin1(input.toHex()),animationOnly});
        if(result.isError())throw std::runtime_error(result.toString().toStdString());
        const auto json=result.toString().toUtf8();
        if(json.size()>128*1024*1024)throw std::runtime_error("GR2 decoded data exceeds limit");
        QJsonParseError parseError{};const auto document=QJsonDocument::fromJson(json,&parseError);
        if(parseError.error!=QJsonParseError::NoError || !document.isObject())throw std::runtime_error("GR2 decoded data is invalid");
        Extract(document.object(),asset,animationOnly);
        return true;
    } catch(const std::exception& exception) {asset={};error=exception.what();return false;}
#else
    (void)bytes;(void)animationOnly;
    error="GR2 decoding requires the Qt-enabled client build";
    return false;
#endif
}
}
