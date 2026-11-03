#pragma once
#include "Types.h"
#include <algorithm>
#include <cmath>

namespace ro::lighting {
inline float Unit(float value) {return (std::max)(0.0f,(std::min)(1.0f,value));}

// Ref transforms authored normals through the node matrix, then normalizes.
// Recomputing a cross product after a mirrored transform reverses the result.
inline vector3d TransformNormal(const vector3d& normal,const matrix& transform)
{
    vector3d result{normal.x*transform.m[0][0]+normal.y*transform.m[1][0]+normal.z*transform.m[2][0],
        normal.x*transform.m[0][1]+normal.y*transform.m[1][1]+normal.z*transform.m[2][1],
        normal.x*transform.m[0][2]+normal.y*transform.m[1][2]+normal.z*transform.m[2][2]};
    const float length=std::sqrt(result.x*result.x+result.y*result.y+result.z*result.z);
    if(length<.000001f)return {0,1,0};
    return {result.x/length,result.y/length,result.z/length};
}

// Ref/3dActor.cpp UpdateVertexColor: ambient plus directional diffuse.
inline vector3d Directional(const vector3d& normal,const vector3d& direction,
    const vector3d& diffuse,const vector3d& ambient)
{
    const float intensity=(std::max)(0.0f,normal.x*direction.x+normal.y*direction.y+normal.z*direction.z);
    return {Unit(ambient.x+diffuse.x*intensity),Unit(ambient.y+diffuse.y*intensity),Unit(ambient.z+diffuse.z*intensity)};
}

// Ref CWeather::Process; this tint is applied at draw time, including night
// transitions. It must not be permanently baked into terrain/model colors.
inline vector3d Environment(const vector3d& ambient,const vector3d& diffuse)
{
    return {Unit(1-(1-ambient.x)*(1-diffuse.x)),Unit(1-(1-ambient.y)*(1-diffuse.y)),Unit(1-(1-ambient.z)*(1-diffuse.z))};
}
inline u32 Tint(u32 color,const vector3d& tint)
{
    return (color&0xFF000000u)
        |(static_cast<u32>(((color>>16)&255)*Unit(tint.x)+0.5f)<<16)
        |(static_cast<u32>(((color>>8)&255)*Unit(tint.y)+0.5f)<<8)
        |static_cast<u32>((color&255)*Unit(tint.z)+0.5f);
}
}
