// Carve voxel holes from native stage triangles before near-plane clipping.
struct CutVertex {ntr::GxVertex vertex;Vec world;};
CutVertex cutLerp(const CutVertex& a,const CutVertex& b,float t) {
    CutVertex out=a;
#define STEVE_LERP(field) out.vertex.field=a.vertex.field+(b.vertex.field-a.vertex.field)*t
    STEVE_LERP(x);STEVE_LERP(y);STEVE_LERP(z);STEVE_LERP(w);STEVE_LERP(u);STEVE_LERP(v);
    STEVE_LERP(vx);STEVE_LERP(vy);STEVE_LERP(vz);STEVE_LERP(vw);STEVE_LERP(nx);STEVE_LERP(ny);STEVE_LERP(nz);
#undef STEVE_LERP
    out.vertex.color=0;
    for(int shift=0;shift<32;shift+=8) {
        float ca=float((a.vertex.color>>shift)&255),cb=float((b.vertex.color>>shift)&255);
        out.vertex.color|=uint32_t(std::clamp(int(ca+(cb-ca)*t),0,255))<<shift;
    }
    out.world={a.world.x+(b.world.x-a.world.x)*t,a.world.y+(b.world.y-a.world.y)*t,a.world.z+(b.world.z-a.world.z)*t};
    return out;
}
bool cutStage(const ntr::GxVertex& a,const ntr::GxVertex& b,const ntr::GxVertex& c,ntr::GxHostEmit emit) {
    if(!native_enabled() || !client.fresh() || !local() || client.state.token!=scene || scene!=currentScene()) return false;
    float m[16];view(m);
    float a00=m[0],a01=m[4],a02=m[8],a10=m[1],a11=m[5],a12=m[9],a20=m[2],a21=m[6],a22=m[10];
    float det=a00*(a11*a22-a12*a21)-a01*(a10*a22-a12*a20)+a02*(a10*a21-a11*a20);
    if(std::abs(det)<1e-6f) return false;
    float inv[9]={(a11*a22-a12*a21)/det,(a02*a21-a01*a22)/det,(a01*a12-a02*a11)/det,
                  (a12*a20-a10*a22)/det,(a00*a22-a02*a20)/det,(a02*a10-a00*a12)/det,
                  (a10*a21-a11*a20)/det,(a01*a20-a00*a21)/det,(a00*a11-a01*a10)/det};
    auto convert=[&](const ntr::GxVertex& v) {
        float x=v.vx-m[12],y=v.vy-m[13],z=v.vz-m[14];
        return CutVertex{v,{8*(inv[0]*x+inv[1]*y+inv[2]*z),8*(inv[3]*x+inv[4]*y+inv[5]*z),8*(inv[6]*x+inv[7]*y+inv[8]*z)}};
    };
    using Polygon=std::vector<CutVertex>;
    Polygon triangle={convert(a),convert(b),convert(c)};
    Vec minimum=triangle[0].world,maximum=minimum;
    for(auto p:triangle) {minimum={std::min(minimum.x,p.world.x),std::min(minimum.y,p.world.y),std::min(minimum.z,p.world.z)};maximum={std::max(maximum.x,p.world.x),std::max(maximum.y,p.world.y),std::max(maximum.z,p.world.z)};}
    if(frames%15==0 && terrainTriangles.size()<32768 && terrainQueue.size()<4096) {
        std::array<int,9> key{};int index=0;
        for(auto p:triangle) {key[index++]=int(std::round(p.world.x));key[index++]=int(std::round(p.world.y));key[index++]=int(std::round(p.world.z));}
        if(!terrainTriangles.count(key)) {
            Vec proximity=position(local());
            if(maximum.x>=proximity.x-4000 && minimum.x<=proximity.x+4000 && maximum.z>=proximity.z-4000 && minimum.z<=proximity.z+4000) {
                Cell low=cell(minimum),high=cell(maximum);
                long long count=1LL*(high.x-low.x+1)*(high.y-low.y+1)*(high.z-low.z+1);
                if(count<=16384) {
                    terrainTriangles.insert(key);
                    GeometryPoint pa{triangle[0].world.x/scale,triangle[0].world.y/scale,triangle[0].world.z/scale};
                    GeometryPoint pb{triangle[1].world.x/scale,triangle[1].world.y/scale,triangle[1].world.z/scale};
                    GeometryPoint pc{triangle[2].world.x/scale,triangle[2].world.y/scale,triangle[2].world.z/scale};
                    auto normal=cross(subtract(pb,pa),subtract(pc,pa));bool floor=std::abs(normal.y)>std::hypot(normal.x,normal.z);
                    for(int y=low.y;y<=high.y;y++) for(int z=low.z;z<=high.z;z++) for(int x=low.x;x<=high.x;x++) {
                        Cell at{x,y,z};
                        if(terrainQueue.size()>=4096) break;
                        if(!client.blocks.count(at) && !terrain.count(at) && triangleCell(pa,pb,pc,{x+.5f,y+.5f,z+.5f})) terrainQueue.emplace(at,floor);
                    }
                }
            }
        }
    }
    if(holes.empty()) return false;
    std::vector<Polygon> pieces{triangle};bool changed=false;
    for(Cell hole:holes) {
        Vec lo{hole.x*scale,hole.y*scale,hole.z*scale},hi{lo.x+scale,lo.y+scale,lo.z+scale};
        if(maximum.x<lo.x || minimum.x>hi.x || maximum.y<lo.y || minimum.y>hi.y || maximum.z<lo.z || minimum.z>hi.z) continue;
        std::vector<Polygon> next;
        for(auto inside:pieces) {
            for(int plane=0;plane<6 && inside.size()>=3;plane++) {
                auto distance=[&](Vec p) {switch(plane) {case 0:return p.x-lo.x;case 1:return hi.x-p.x;case 2:return p.y-lo.y;case 3:return hi.y-p.y;case 4:return p.z-lo.z;default:return hi.z-p.z;}};
                Polygon kept,outside;
                CutVertex previous=inside.back();float dp=distance(previous.world);
                for(auto current:inside) {
                    float dc=distance(current.world);
                    if((dp>=0)!=(dc>=0)) {auto crossing=cutLerp(previous,current,dp/(dp-dc));kept.push_back(crossing);outside.push_back(crossing);}
                    (dc>=0?kept:outside).push_back(current);previous=current;dp=dc;
                }
                if(outside.size()>=3) next.push_back(std::move(outside));
                inside=std::move(kept);
            }
            if(inside.size()>=3) changed=true;
        }
        pieces=std::move(next);
        if(pieces.size()>1024) return false;
        if(pieces.empty()) break;
    }
    if(!changed) return false;
    for(const auto& polygon:pieces) for(size_t i=1;i+1<polygon.size();i++) emit(polygon[0].vertex,polygon[i].vertex,polygon[i+1].vertex);
    return true;
}
