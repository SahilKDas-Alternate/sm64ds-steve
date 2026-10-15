#pragma once
#include <algorithm>
#include <cmath>
namespace steve {
struct GeometryPoint {float x,y,z;};
inline GeometryPoint subtract(GeometryPoint a,GeometryPoint b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline GeometryPoint cross(GeometryPoint a,GeometryPoint b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float dot(GeometryPoint a,GeometryPoint b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline GeometryPoint add(GeometryPoint a,GeometryPoint b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline GeometryPoint multiply(GeometryPoint a,float t) {return {a.x*t,a.y*t,a.z*t};}
inline GeometryPoint closestTriangle(GeometryPoint p,GeometryPoint a,GeometryPoint b,GeometryPoint c) {
    auto ab=subtract(b,a),ac=subtract(c,a),ap=subtract(p,a);
    float d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0 && d2<=0) return a;
    auto bp=subtract(p,b);float d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0 && d4<=d3) return b;
    float vc=d1*d4-d3*d2;
    if(vc<=0 && d1>=0 && d3<=0) return add(a,multiply(ab,d1/(d1-d3)));
    auto cp=subtract(p,c);float d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0 && d5<=d6) return c;
    float vb=d5*d2-d1*d6;
    if(vb<=0 && d2>=0 && d6<=0) return add(a,multiply(ac,d2/(d2-d6)));
    float va=d3*d6-d5*d4;
    if(va<=0 && d4-d3>=0 && d5-d6>=0)
        return add(b,multiply(subtract(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
    float denominator=va+vb+vc;
    if(std::abs(denominator)<1e-12f) return a;
    return add(a,add(multiply(ab,vb/denominator),multiply(ac,vc/denominator)));
}
// Separating-axis triangle/unit-cell overlap. An AABB-only test incorrectly
// fills the empty corners of slopes and large diagonal wall triangles.
inline bool triangleCell(GeometryPoint a,GeometryPoint b,GeometryPoint c,GeometryPoint center,float half=.5f) {
    a=subtract(a,center);b=subtract(b,center);c=subtract(c,center);
    auto separated=[&](GeometryPoint axis) {
        float pa=dot(a,axis),pb=dot(b,axis),pc=dot(c,axis);
        float radius=half*(std::abs(axis.x)+std::abs(axis.y)+std::abs(axis.z));
        return std::min({pa,pb,pc})>radius+1e-5f || std::max({pa,pb,pc})<-radius-1e-5f;
    };
    GeometryPoint axes[3]={{1,0,0},{0,1,0},{0,0,1}},edges[3]={subtract(b,a),subtract(c,b),subtract(a,c)};
    for(auto axis:axes) if(separated(axis)) return false;
    if(separated(cross(edges[0],edges[1]))) return false;
    for(auto edge:edges) for(auto axis:axes) if(separated(cross(edge,axis))) return false;
    return true;
}
}
