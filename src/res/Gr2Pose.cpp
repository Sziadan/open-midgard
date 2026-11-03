// Animation equations adapted from the MIT-licensed independent GR2 reader.
// Copyright/permission notices: third_party/gr2-reader/LICENSE.
#include "Gr2Reader.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ro::gr2 {
namespace {
using WideMatrix=std::array<double,16>;
WideMatrix Multiply(const WideMatrix& a,const WideMatrix& b)
{
    WideMatrix result{};
    for(int column=0;column<4;++column)for(int row=0;row<4;++row)
        for(int k=0;k<4;++k)result[column*4+row]+=a[k*4+row]*b[column*4+k];
    return result;
}
WideMatrix Local(const Transform& transform)
{
    const auto& q=transform.orientation;
    const double x=q[0],y=q[1],z=q[2],w=q[3];
    const double rotation[9]={1-2*(y*y+z*z),2*(x*y-w*z),2*(x*z+w*y),
        2*(x*y+w*z),1-2*(x*x+z*z),2*(y*z-w*x),
        2*(x*z-w*y),2*(y*z+w*x),1-2*(x*x+y*y)};
    WideMatrix result{};
    for(int row=0;row<3;++row)for(int column=0;column<3;++column)
        for(int k=0;k<3;++k)result[column*4+row]+=rotation[row*3+k]*transform.scale[k*3+column];
    for(int axis=0;axis<3;++axis)result[12+axis]=transform.position[axis];
    result[15]=1;
    return result;
}
double Divide(double numerator,double denominator)
{
    return std::fabs(denominator)<1e-12?0:numerator/denominator;
}
std::vector<double> Evaluate(const Curve& curve,double time)
{
    if(curve.knots.empty() || curve.controls.empty())return curve.sample;
    const int count=static_cast<int>(curve.knots.size()),dimension=curve.dimension;
    if(dimension<=0 || dimension>16 || curve.controls.size()<static_cast<size_t>(count*dimension))return {};
    int knot=static_cast<int>(std::upper_bound(curve.knots.begin(),curve.knots.end(),time)-curve.knots.begin());
    knot=(std::min)(knot,count-1);
    const auto control=[&](int index,int axis) {return curve.controls[static_cast<size_t>((std::max)(0,(std::min)(count-1,index)))*dimension+axis];};
    const auto at=[&](int index) {return curve.knots[(std::max)(0,(std::min)(count-1,index))];};
    std::vector<double> result(dimension);
    if(curve.degree==1 && count>1) {
        const double blend=Divide(time-at(knot-1),at(knot)-at(knot-1));
        for(int axis=0;axis<dimension;++axis)result[axis]=(1-blend)*control(knot-1,axis)+blend*control(knot,axis);
    } else if(curve.degree==2 && count>1) {
        const double l0=Divide(time-at(knot-1),at(knot)-at(knot-1));
        const double l11=Divide(time-at(knot-2),at(knot)-at(knot-2));
        const double l12=Divide(time-at(knot-1),at(knot+1)-at(knot-1));
        const double sum=(l11+l0)-l0*l11,c0=l0*l12,c1=sum-c0,c2=1-sum;
        for(int axis=0;axis<dimension;++axis)result[axis]=c2*control(knot-2,axis)+c1*control(knot-1,axis)+c0*control(knot,axis);
    } else {
        for(int axis=0;axis<dimension;++axis)result[axis]=control(knot,axis);
    }
    if(dimension==4 && curve.degree>0 && count>1) {
        double length=0;for(double value:result)length+=value*value;
        const double factor=(3-length)*0.5;
        for(double& value:result)value*=factor;
    }
    return result;
}
Transform SampleTrack(const Track& track,double time)
{
    Transform result;
    const auto position=Evaluate(track.position,time),orientation=Evaluate(track.orientation,time),scale=Evaluate(track.scale,time);
    if(position.size()==3)std::copy(position.begin(),position.end(),result.position.begin());
    if(orientation.size()==4)std::copy(orientation.begin(),orientation.end(),result.orientation.begin());
    if(scale.size()==9)std::copy(scale.begin(),scale.end(),result.scale.begin());
    else if(scale.size()==3) { result.scale={scale[0],0,0,0,scale[1],0,0,0,scale[2]}; }
    return result;
}
}

std::vector<Matrix> Sample(const Asset& asset,const Animation* animation,double time,bool loop)
{
    if(animation && animation->duration>0)
        time=loop?std::fmod((std::max)(0.0,time),animation->duration):(std::min)(animation->duration,(std::max)(0.0,time));
    std::vector<WideMatrix> worlds(asset.bones.size());
    std::vector<Matrix> result(asset.bones.size());
    for(size_t index=0;index<asset.bones.size();++index) {
        const auto& bone=asset.bones[index];
        Transform transform=bone.bind;
        if(animation) {
            const auto found=animation->tracks.find(bone.name);
            if(found!=animation->tracks.end())transform=SampleTrack(found->second,time);
        }
        const auto local=Local(transform);
        if(bone.parent>=0) {
            if(static_cast<size_t>(bone.parent)>=index)throw std::runtime_error("GR2 skeleton is not parent-first");
            worlds[index]=Multiply(worlds[bone.parent],local);
        } else worlds[index]=local;
        // Match the format's float boundary between world-pose composition
        // and inverse-bind multiplication, retaining double precision in FK.
        WideMatrix world{},inverse{};
        for(int element=0;element<16;++element) {world[element]=static_cast<float>(worlds[index][element]);inverse[element]=bone.inverse[element];}
        const auto skin=Multiply(world,inverse);
        for(int element=0;element<16;++element)result[index][element]=static_cast<float>(skin[element]);
    }
    return result;
}

void Skin(const Mesh& mesh,const std::vector<Matrix>& matrices,
    std::vector<std::array<float,3>>& positions,std::vector<std::array<float,3>>& normals)
{
    positions.resize(mesh.vertices.size());normals.resize(mesh.vertices.size());
    for(size_t index=0;index<mesh.vertices.size();++index) {
        const auto& vertex=mesh.vertices[index];
        auto& position=positions[index];auto& normal=normals[index];position={};normal={};
        if(vertex.weights.empty()) {position=vertex.position;normal=vertex.normal;continue;}
        for(const auto& weight:vertex.weights) {
            if(weight.bone<0 || static_cast<size_t>(weight.bone)>=matrices.size())throw std::runtime_error("GR2 vertex bone is out of range");
            const auto& m=matrices[weight.bone];
            for(int axis=0;axis<3;++axis) {
                position[axis]+=weight.value*(vertex.position[0]*m[axis]+vertex.position[1]*m[4+axis]+vertex.position[2]*m[8+axis]+m[12+axis]);
                normal[axis]+=weight.value*(vertex.normal[0]*m[axis]+vertex.normal[1]*m[4+axis]+vertex.normal[2]*m[8+axis]);
            }
        }
    }
}
}
