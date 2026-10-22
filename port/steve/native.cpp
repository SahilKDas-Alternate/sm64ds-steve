#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "native.h"
#include "client.h"
#include "assets.h"
#include "geometry.h"
#include "Player.h"
#include "dBgCh_Gnd.h"
#include "dBgCh_Lin.h"
#include "Bowser.h"
#include "daBombking_c.h"
#include "Whomp.h"
#include "BigBully.h"
#include "daIDonketu_c.h"
#include "Goomboss.h"
#include "ChiefChilly.h"
#include "Eyerok.h"
#include "Wiggler.h"
#include "daKrb_c.h"
#include "Koopa.h"
#include "daTrs_c.h"
#include "daTrsIcon_c.h"
#include "Key.h"
#include "math/Matrix.h"
#include "../hal/player_fields.h"
#include "ntr/gx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <fstream>

extern "C" {
extern void* data_0209f394[];
extern unsigned char data_0209f250;
extern unsigned char data_0209f2c4;
extern signed char data_0209f2f8,data_02092120;
extern int data_0209fc48;
extern int data_0209caa0[];
int IsStarCollectedInCurLevel(int);
int _ZN5Event6GetBitEj(unsigned);
extern int data_0209f32c[];
extern short data_02092144[];
extern Player::State data_ov002_021104b4,data_ov002_02110154;
extern Matrix4x3 data_0209b3ec;
int port_level_id(void);
int port_course_coins(void);
void port_clsn_prepare_dispatch(void);
void KillPlayer(void);
void GiveHealth(int,int);
int func_ov002_020ceb54(void*);
}
#include "native_collision.inl"
namespace steve {
namespace {
constexpr float scale=100.f,fix=4096.f,pi=3.14159265358979323846f;
struct Vec {float x{},y{},z{};};
struct Draw {Cell at;unsigned id;const MeshFace* face;};
Client client;
Assets assets;
bool loaded{},failed{},inventoryOpen{},catalogOpen{},mobOpen{},respawnPending{},capFlight{},menuClosing{};
unsigned frames{},catalogPage{};
bool submerged{};
struct AirBubble {Vec at;unsigned age;};
std::vector<AirBubble> airBubbles;
bool airRefillPending{};
uint64_t airRefillTick{};
int scene=-1,lastCoins{},previousHP=-1;
int terrainQuota{};
uint64_t previousTick{};
float holeFallVelocity{};
float flightYaw{},flightPitch{-10};
bool flightControls{};
Cell shapeFixture{};
bool shapePending{};
Vec previousPosition{},eye{},direction{0,0,-1};
Vec consumedPush{};
unsigned beforePhysicsFrame=~0u,afterPhysicsFrame=~0u;
Cell target{};
int targetSide=2,targetEntity=-1;
bool hasTarget{},nativeTarget{};
float targetDistance{};
std::string root;
std::set<Cell> dirtyChunks,terrain,holes,terrainSurfaces;
std::map<Cell,std::vector<Draw>> chunkMeshes;
std::map<Cell,bool> terrainQueue;
std::set<std::array<int,9>> terrainTriangles;
bool keys[256]{}, miningHeld{},itemHeld{};
struct InputApi {
    decltype(&GetWindowThreadProcessId) process{};decltype(&GetForegroundWindow) foreground{};
    decltype(&GetAsyncKeyState) key{};decltype(&GetCursorPos) cursor{};
    decltype(&ScreenToClient) client{};decltype(&GetClientRect) rect{};
    bool load() {
        if(key) return true;
        HMODULE library=GetModuleHandleA("user32.dll");if(!library) library=LoadLibraryExA("user32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library) return false;
#define STEVE_UI(field,name) field=reinterpret_cast<decltype(field)>(GetProcAddress(library,name))
        STEVE_UI(process,"GetWindowThreadProcessId");STEVE_UI(foreground,"GetForegroundWindow");STEVE_UI(key,"GetAsyncKeyState");
        STEVE_UI(cursor,"GetCursorPos");STEVE_UI(client,"ScreenToClient");STEVE_UI(rect,"GetClientRect");
#undef STEVE_UI
        return process && foreground && key && cursor && client && rect;
    }
} inputApi;
const int offsets[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
int floor16(int n) {int q=n/16;return q-(n%16<0);}
Cell section(Cell c) {return {floor16(c.x),floor16(c.y),floor16(c.z)};}
Cell cell(Vec p) {return {int(std::floor(p.x/scale)),int(std::floor(p.y/scale)),int(std::floor(p.z/scale))};}
Cell neighbor(Cell p,int side) {return {p.x+offsets[side][0],p.y+offsets[side][1],p.z+offsets[side][2]};}
std::string coordinates(Cell c) {return std::to_string(c.x)+"\t"+std::to_string(c.y)+"\t"+std::to_string(c.z);}
bool same(Cell a,Cell b) {return a.x==b.x && a.y==b.y && a.z==b.z;}
void* local() {return data_0209f394[data_0209f250];}
Vec position(void* p) {auto* player=static_cast<Player*>(p);return {player->mPosX/fix,player->mPosY/fix,player->mPosZ/fix};}
bool scripted(void* p) {return !p || data_0209fc48 || static_cast<Player*>(p)->mIsNoControl;}
int currentScene() {if(const char* test=std::getenv("SM64DS_STEVE_TEST_SCENE")) return std::clamp(std::atoi(test),0,1023);return port_level_id()*8+int(data_02092120);}
void view(float out[16]) {
    const int* m=reinterpret_cast<const int*>(&data_0209b3ec);
    std::fill(out,out+16,0.f);out[15]=1;
    for(int row=0;row<4;row++) for(int col=0;col<3;col++) out[row*4+col]=m[row*3+col]/fix;
}
bool focused() {
    if(!inputApi.load()) return false;
    DWORD process=0;inputApi.process(inputApi.foreground(),&process);return process==GetCurrentProcessId();
}
bool down(int key) {return focused() && (inputApi.key(key)&0x8000)!=0;}
bool pressed(int key) {bool value=down(key),edge=value&&!keys[key];keys[key]=value;return edge;}
bool transparent(const BlockMesh* mesh) {
    if(!mesh) return true;const std::string& n=mesh->name;
    return n=="water" || n=="lava" || n.find("glass")!=std::string::npos || n.find("leaves")!=std::string::npos ||
           n=="slime_block" || n=="honey_block" || n.find("ice")!=std::string::npos;
}
bool solid(unsigned id) {
    auto* m=assets.block(id);if(!m || m->faces.empty()) return false;
    if(assets.collisionReady) return !m->collision.empty();
    const std::string& n=m->name;
    return n!="water" && n!="lava" && n!="redstone_wire" && n.find("torch")==std::string::npos &&
           n.find("rail")==std::string::npos && n.find("flower")==std::string::npos && n!="grass" && n!="tall_grass" && n!="vine" && n!="tripwire";
}
unsigned block(Cell c) {auto it=client.blocks.find(c);return it==client.blocks.end()?0:it->second;}
bool opaqueCube(unsigned id) {
    auto* m=assets.block(id);if(!m || transparent(m) || m->faces.size()!=6) return false;
    for(auto& f:m->faces) for(auto& v:f.vertex)
        if((std::abs(v.x)>0.0001f && std::abs(v.x-1)>0.0001f) || (std::abs(v.y)>0.0001f && std::abs(v.y-1)>0.0001f) ||
           (std::abs(v.z)>0.0001f && std::abs(v.z-1)>0.0001f)) return false;
    return true;
}
uint32_t tint(const BlockMesh& mesh,const MeshFace& face) {
    // Liquid geometry has no vanilla block-model tintindex; its grayscale
    // water texture still needs the biome water color.
    if(mesh.name=="water") return 0xff3f76e4u;
    if(face.tint<0) return 0xffffffffu;
    if(mesh.name.find("banner")!=std::string::npos) {
        static const std::pair<const char*,uint32_t> dyes[]={
            {"light_blue",0xff6699d9},{"light_gray",0xff999999},{"white",0xffffffff},
            {"orange",0xffd98033},{"magenta",0xffb34dd9},{"yellow",0xffe6e633},
            {"lime",0xff80cc1a},{"pink",0xfff280a6},{"gray",0xff4d4d4d},
            {"cyan",0xff4d8099},{"purple",0xff8040b3},{"blue",0xff334db3},
            {"brown",0xff664d33},{"green",0xff668033},{"red",0xff993333},{"black",0xff1a1a1a}};
        for(auto dye:dyes) if(mesh.name.rfind(std::string(dye.first)+"_",0)==0) return dye.second;
    }
    if(mesh.name=="redstone_wire") {
        int power=0;auto pos=mesh.properties.find("power=");if(pos!=std::string::npos) power=std::atoi(mesh.properties.c_str()+pos+6);
        float f=power/15.f,r=power?f*.6f+.4f:.3f,g=std::max(0.f,f*f*.7f-.5f),b=std::max(0.f,f*f*.6f-.7f);
        return 0xff000000u|(unsigned(r*255)<<16)|(unsigned(g*255)<<8)|unsigned(b*255);
    }
    return mesh.name.find("water")!=std::string::npos?0xff3f76e4u:0xff80b85bu;
}
void quad(const float camera[16],const MeshVertex vertices[4],const Texture* texture,Vec base,float size,
          uint32_t color=0xffffffffu,unsigned alpha=31,float yaw=0) {
    if(!texture) return;
    ntr::GxHostVertex out[4];float cs=std::cos(yaw),sn=std::sin(yaw),offset=float(texture->frameOffset(client.state.tick));
    for(int i=0;i<4;i++) {
        auto v=vertices[i];
        out[i]={(base.x+(v.x*cs-v.z*sn)*size)/8,(base.y+v.y*size)/8,(base.z+(v.x*sn+v.z*cs)*size)/8,
                v.u*texture->w/16.f,v.v*texture->frameHeight/16.f+offset,color};
    }
    ntr::gx_host_quad(camera,out,texture->pixels.data(),texture->w,texture->h,alpha);
}
void blockDraw(const float camera[16],unsigned id,Vec base,float size=scale) {
    auto* mesh=assets.block(id);if(!mesh) return;
    for(const auto& face:mesh->faces) quad(camera,face.vertex,assets.texture(face.texture),base,size,tint(*mesh,face),mesh->name=="water"?22:31);
}
void coinAir(Vec at,int coins) {
    client.send("air");
    airRefillPending=true;airRefillTick=client.state.tick;
    for(int i=0;i<std::min(coins*6,24);i++) {
        if(airBubbles.size()>=32)airBubbles.erase(airBubbles.begin());
        airBubbles.push_back({{at.x+float((i%5-2)*12),at.y+50+float(i%3*18),at.z+float((i%4-2)*12)},0});
    }
    std::fprintf(stderr,"[steve-air] underwater coins=%d refill=300 bubbles=%u before=%d\n",coins,unsigned(airBubbles.size()),client.state.air);
}
void drawAirBubbles(const float camera[16]) {
    static Texture bubble;
    if(bubble.pixels.empty()) {
        const Texture* icons=assets.texture("gui/icons");if(!icons || icons->w<25 || icons->h<27)return;
        bubble.w=bubble.h=bubble.frameHeight=9;bubble.pixels.resize(81);
        for(unsigned y=0;y<9;y++)for(unsigned x=0;x<9;x++)bubble.pixels[y*9+x]=icons->pixels[(y+18)*icons->w+x+16];
    }
    MeshVertex face[4]={{-.5f,0,0,0,16},{.5f,0,0,16,16},{.5f,1,0,16,0},{-.5f,1,0,0,0}};
    float yaw=std::atan2(camera[8],camera[0]);
    for(const auto& b:airBubbles)quad(camera,face,&bubble,b.at,14,0xffffffffu,24,yaw);
}
#include "block_entities.inl"
void rebuild(Cell key) {
    std::vector<Draw> draws;
    Cell start{key.x*16,key.y*16,key.z*16};
    for(int y=0;y<16;y++) for(int z=0;z<16;z++) for(int x=0;x<16;x++) {
        Cell at{start.x+x,start.y+y,start.z+z};unsigned id=block(at);auto* mesh=assets.block(id);if(!mesh || mesh->name=="moving_piston") continue;
        if(dynamicModel(*mesh)) continue;
        for(const auto& face:mesh->faces) {
            if(terrain.count(at) && client.terrainOriginal.count(at) && block(at)==client.terrainOriginal.at(at) && (face.cull<0 || !holes.count(neighbor(at,face.cull)))) continue;
            if(face.cull>=0) {
                unsigned next=block(neighbor(at,face.cull));
                if(opaqueCube(next) || (next==id && (mesh->name=="water" || mesh->name.find("glass")!=std::string::npos))) continue;
            }
            draws.push_back({at,id,&face});
        }
    }
    if(draws.empty()) chunkMeshes.erase(key);else chunkMeshes[key]=std::move(draws);
}
void mark(Cell at) {
    dirtyChunks.insert(section(at));
    for(int side=0;side<6;side++) dirtyChunks.insert(section(neighbor(at,side)));
}
bool ground(Vec p,void* owner,float& height) {
    port_clsn_prepare_dispatch();
    dBgCh_Gnd ray;
    Vector3 point{int(p.x*fix),int(p.y*fix),int(p.z*fix)};
    ray.SetObjAndPos(point,static_cast<dActor_c*>(owner));ray.mProbeHeight=65536*4096;
    if(!ray.DetectClsn() || !ray.hasClsn) return false;
    height=static_cast<int>(ray.clsnY)/fix;return true;
}
bool waterSurface(Vec p,void* owner,float& height) {
    port_clsn_prepare_dispatch();
    dBgCh_Gnd ray;ray.StartDetectingWater();ray.StopDetectingOrdinary();
    Vector3 point{int(p.x*fix),std::max(int(p.y*fix),data_0209f32c[0]+200*4096),int(p.z*fix)};
    ray.SetObjAndPos(point,static_cast<dActor_c*>(owner));ray.mProbeHeight=65536*4096;
    if(!ray.DetectClsn() || !ray.hasClsn) return false;
    height=static_cast<int>(ray.clsnY)/fix;return true;
}
#include "boss_combat.inl"
void mirror(Cell at,const char* material) {
    if(terrainQuota<=0 || terrain.size()>=65536 || terrain.count(at) || holes.count(at) || client.blocks.count(at)) return;
    if(client.send("terrain\t"+coordinates(at)+"\t"+material)) {--terrainQuota;terrain.insert(at);mark(at);}
}
void surface(Cell at,float height) {
    if(terrainQuota<=0 || !terrain.count(at) || holes.count(at) || terrainSurfaces.count(at)) return;
    float top=std::clamp(height/scale-at.y,.001f,1.f);
    if(client.send("surface\t"+coordinates(at)+"\t"+std::to_string(top))) {terrainSurfaces.insert(at);--terrainQuota;}
}
void aim(void* p) {
    eye=position(p);eye.y+=162;
    float camera[16];view(camera);
    direction={-camera[2],-camera[6],-camera[10]};
    float length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if(length>0) {direction.x/=length;direction.y/=length;direction.z/=length;}
    hasTarget=false;nativeTarget=false;targetEntity=-1;targetDistance=500;
    Cell previous=cell(eye);
    for(float distance=4;distance<=500;distance+=4) {
        Vec point{eye.x+direction.x*distance,eye.y+direction.y*distance,eye.z+direction.z*distance};Cell at=cell(point);
        if(block(at) && !terrain.count(at)) {
            target=at;hasTarget=true;targetDistance=distance;
            if(at.x!=previous.x) targetSide=point.x>eye.x?1:0;
            else if(at.y!=previous.y) targetSide=point.y>eye.y?3:2;
            else targetSide=point.z>eye.z?5:4;
            break;
        }
        float floor;
        if(ground({point.x,point.y+10,point.z},p,floor) && point.y<=floor+5 && !native_removed(point.x,floor,point.z)) {
            target=cell({point.x,floor-1,point.z});hasTarget=true;nativeTarget=true;targetDistance=distance;targetSide=2;break;
        }
        previous=at;
    }
    for(const auto& e:client.state.entities) {
        Vec relative{e.x*scale-eye.x,(e.y+0.8f)*scale-eye.y,e.z*scale-eye.z};
        float distance=relative.x*direction.x+relative.y*direction.y+relative.z*direction.z;
        if(distance<0 || distance>targetDistance) continue;
        float dx=relative.x-direction.x*distance,dy=relative.y-direction.y*distance,dz=relative.z-direction.z*distance;
        if(dx*dx+dy*dy+dz*dz<3600) {targetEntity=e.id;targetDistance=distance;hasTarget=true;}
    }
    aimBoss();
}
void uiQuad(float x,float y,float w,float h,const Texture* t,float u0=0,float v0=0,float u1=-1,float v1=-1,uint32_t color=0xffffffffu) {
    static uint32_t white=0xffffffffu;
    if(t) ntr::gx_host_screen_quad(x,y,w,h,u0,v0,u1<0?float(t->w):u1,v1<0?float(t->h):v1,t->pixels.data(),t->w,t->h,color);
    else ntr::gx_host_screen_quad(x,y,w,h,0,0,1,1,&white,1,1,color,(color>>24)*31/255);
}
void text(float x,float y,const std::string& value,float size=1.5f,uint32_t color=0xffffffffu) {
    const Texture* font=assets.texture("font/ascii");if(!font) return;
    float cellSize=font->w/16.f;
    for(unsigned char c:value) {if(c=='\n') {y+=cellSize*size;x=12;continue;}
        float u=(c%16)*cellSize,v=(c/16)*cellSize;uiQuad(x,y,cellSize*size*.5f,cellSize*size*.5f,font,u,v,u+cellSize,v+cellSize,color);x+=cellSize*size*.375f;}
}
const Texture* icon(unsigned item) {
    const auto& layers=assets.itemLayers(item);if(!layers.empty()) return assets.texture(layers.front());
    std::string name=assets.item(item);auto* t=assets.texture("item/"+name);if(!t)t=assets.texture("block/"+name);
    if(!t) {auto* mesh=assets.block(assets.defaultBlock(name));if(mesh&&!mesh->faces.empty()) t=assets.texture(mesh->faces.front().texture);}
    return t;
}
void blockItemIcon(float x,float y,float size,unsigned id) {
    const auto* mesh=assets.block(id);if(!mesh) return;
    struct Face {const MeshFace* source;float depth;ntr::GxHostVertex vertices[4];};std::vector<Face> faces;
    float yaw=225*pi/180,pitch=30*pi/180,cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f;
    for(const auto& source:mesh->faces) {
        Face face{};face.source=&source;
        for(int i=0;i<4;i++) {
            auto v=source.vertex[i];float px=v.x-.5f,py=v.y-.5f,pz=v.z-.5f;
            float rx=px*cy+pz*sy,rz=-px*sy+pz*cy,ry=py*cp-rz*sp;rz=py*sp+rz*cp;
            face.depth+=rz;
            minX=std::min(minX,rx);maxX=std::max(maxX,rx);minY=std::min(minY,ry);maxY=std::max(maxY,ry);
            face.vertices[i]={rx,ry,0,v.u,v.v,tint(*mesh,source)};
        }
        faces.push_back(face);
    }
    float fit=size*.9f/std::max({maxX-minX,maxY-minY,1e-5f});
    for(auto& face:faces) for(auto& vertex:face.vertices) {
        vertex.x=x+size*.5f+(vertex.x-(minX+maxX)*.5f)*fit;
        vertex.y=y+size*.5f-(vertex.y-(minY+maxY)*.5f)*fit;
    }
    std::stable_sort(faces.begin(),faces.end(),[](const Face& a,const Face& b){return a.depth<b.depth;});
    for(const auto& face:faces) {auto* texture=assets.texture(face.source->texture);if(texture) ntr::gx_host_screen_vertices(face.vertices,texture->pixels.data()+texture->frameOffset(client.state.tick)*texture->w,texture->w,texture->frameHeight?texture->frameHeight:texture->h);}
}
void itemIcon(float x,float y,float size,const ItemState& item) {
    if(!item.count) return;
    const auto& layers=assets.itemLayers(item.id);
    if(!layers.empty()) for(const auto& path:layers) uiQuad(x,y,size,size,assets.texture(path));
    else if(const Texture* texture=assets.texture("item/"+assets.item(item.id))) uiQuad(x,y,size,size,texture);
    else blockItemIcon(x,y,size,assets.defaultBlock(assets.item(item.id)));
    if(item.count>1) text(x+size*.45f,y+size*.7f,std::to_string(item.count),size/16.f);
    if(item.maximum) {uiQuad(x,y+size-2,size,2,nullptr,0,0,1,1,0xff101010u);uiQuad(x,y+size-2,size*(1-float(item.damage)/item.maximum),2,nullptr,0,0,1,1,0xff45c94du);}
}
struct SlotRect {int slot;float x,y;};
std::vector<SlotRect> slotRects(float x,float y,float uiScale) {
    std::vector<SlotRect> out;
    if(client.state.craftingWidth==0) {for(auto slot:client.state.menuSlots)out.push_back({slot.index,x+slot.x*uiScale,y+slot.y*uiScale});return out;}
    bool workbench=client.state.craftingWidth==3;
    int width=workbench?3:2,gridStart=1,inventoryStart=workbench?10:9,hotbarStart=workbench?37:36;
    float gx=workbench?30.f:98.f,gy=workbench?17.f:18.f;
    for(int row=0;row<width;row++) for(int col=0;col<width;col++) out.push_back({gridStart+row*width+col,x+(gx+col*18)*uiScale,y+(gy+row*18)*uiScale});
    out.push_back({0,x+(workbench?124.f:154.f)*uiScale,y+(workbench?35.f:28.f)*uiScale});
    if(!workbench) {for(int i=0;i<4;i++) out.push_back({5+i,x+8*uiScale,y+(8+i*18)*uiScale});out.push_back({45,x+77*uiScale,y+62*uiScale});}
    for(int row=0;row<3;row++) for(int col=0;col<9;col++) out.push_back({inventoryStart+row*9+col,x+(8+col*18)*uiScale,y+(84+row*18)*uiScale});
    for(int col=0;col<9;col++) out.push_back({hotbarStart+col,x+(8+col*18)*uiScale,y+142*uiScale});
    return out;
}
float menuHeight() {float h=166;if(client.state.craftingWidth==0)for(auto slot:client.state.menuSlots)h=std::max(h,float(slot.y+24));return h;}
void hud() {
    int x,y,w,h,sets;ntr::gx_debug_viewport(x,y,w,h,sets);float s=std::max(1.f,w/256.f);
    float bx=x+(w-182*s)/2,by=y+h-24*s;
    uiQuad(bx,by,182*s,22*s,assets.texture("gui/widgets"),0,0,182,22);
    uiQuad(bx+client.state.selected*20*s-1*s,by-1*s,24*s,24*s,assets.texture("gui/widgets"),0,22,24,46);
    for(unsigned i=0;i<9;i++) itemIcon(bx+(3+i*20)*s,by+3*s,16*s,client.state.inventory[i]);
    auto* icons=assets.texture("gui/icons");
    for(int i=0;i<10;i++) {
        uiQuad(bx+i*8*s,by-12*s,9*s,9*s,icons,16,0,25,9);
        if(client.state.health>i*2) uiQuad(bx+i*8*s,by-12*s,9*s,9*s,icons,client.state.health>=i*2+2?52.f:61.f,0,client.state.health>=i*2+2?61.f:70.f,9);
    }
    for(int i=0;i<10;i++) {
        uiQuad(bx+(172-i*8)*s,by-12*s,9*s,9*s,icons,16,27,25,36);
        if(client.state.food>i*2) uiQuad(bx+(172-i*8)*s,by-12*s,9*s,9*s,icons,client.state.food>=i*2+2?52.f:61.f,27,client.state.food>=i*2+2?61.f:70.f,36);
    }
    if(submerged || client.state.air<300) for(int i=0;i<std::max(0,(client.state.air+29)/30);i++) uiQuad(bx+(172-i*8)*s,by-24*s,9*s,9*s,icons,16,18,25,27);
    uiQuad(x+w/2-8*s,y+h/2-8*s,16*s,16*s,icons,0,0,16,16);
    if(targetBoss) {
        auto& health=bossHealth(targetBoss);
        if(health.maximum) {
            std::string label=std::string(bossName(targetBoss))+" "+std::to_string(health.health)+"/"+std::to_string(health.maximum);
            text(x+6*s,y+20*s,label,s);
        }
    }
    if(client.state.mining>=0) {uiQuad(x+w/2-30*s,y+h/2+14*s,60*s,3*s,nullptr,0,0,1,1,0xff202020u);uiQuad(x+w/2-30*s,y+h/2+14*s,(client.state.mining+1)*6*s,3*s,nullptr,0,0,1,1,0xffc8c8c8u);}
    if(inventoryOpen) {
        float mh=menuHeight();s=std::min(s,(h-8)/mh);float width=176*s,height=mh*s,ux=x+(w-width)/2,uy=y+(h-height)/2;
        uiQuad(ux-4*s,uy-4*s,width+8*s,height+8*s,nullptr,0,0,1,1,0xdd141414u);
        uiQuad(ux,uy,width,height,assets.texture(client.state.craftingWidth==3?"gui/container/crafting_table":"gui/container/inventory"),0,0,176,166);
        if(client.state.craftingWidth==0)uiQuad(ux,uy,width,height,nullptr,0,0,1,1,0xffc6c6c6u);
        for(const auto& r:slotRects(ux,uy,s)) if(r.slot<128) {if(client.state.craftingWidth==0)uiQuad(r.x-s,r.y-s,18*s,18*s,nullptr,0,0,1,1,0xff777777u);itemIcon(r.x,r.y,16*s,client.state.container[r.slot]);}
        text(ux+8*s,uy+4*s,client.state.craftingWidth==0?"Container":"Crafting",s,0xff404040u);
        if(client.state.carried.count && inputApi.load()) {
            POINT point{};inputApi.cursor(&point);inputApi.client(inputApi.foreground(),&point);RECT window{};inputApi.rect(inputApi.foreground(),&window);
            itemIcon(float(point.x)*w/std::max(1L,window.right)-8*s,float(point.y)*h/std::max(1L,window.bottom)-8*s,16*s,client.state.carried);
        }
    }
    if(catalogOpen || mobOpen) {
        float ux=x+20*s,uy=y+20*s;uiQuad(ux,uy,w-40*s,h-60*s,nullptr,0,0,1,1,0xee252525u);
        text(ux+8*s,uy+8*s,mobOpen?"Mobs and boats":"Blocks  [ / ] pages",s);
        if(mobOpen) {
            const char* mobs[]={"zombie","skeleton","creeper","spider","pig","cow","sheep","chicken","boat"};
            for(int i=0;i<9;i++) text(ux+10*s,uy+(28+i*14)*s,std::to_string(i+1)+" "+mobs[i],s);
        } else {
            for(unsigned i=0;i<35;i++) {
                unsigned index=catalogPage*35+i;if(index>=assets.catalog.size()) break;
                const std::string& name=assets.catalog[index];float ix=ux+(8+(i%7)*30)*s,iy=uy+(26+(i/7)*30)*s;
                itemIcon(ix,iy,24*s,{assets.itemId(name),64,0,0});
            }
            text(ux+8*s,uy+181*s,"Page "+std::to_string(catalogPage+1),s);
        }
    }
    if(frames<180) text(x+6*s,y+6*s,"I inventory  B blocks  M mobs  1-9 hotbar",s);
    if(!client.fresh()) text(x+6*s,y+20*s,"Minecraft connection lost",s,0xff4040ffu);
    if(GetTickCount64()<client.diagnosticUntil) text(x+6*s,y+32*s,client.diagnostic.substr(0,42),s,0xff40a0ffu);
}
void cube(const float camera[16],Vec base,Vec low,Vec high,const Texture* texture,
          float yaw,float pitch=0,Vec pivot={},int u=0,int v=0,float bodyTilt=0) {
    if(!texture) return;
    float X=high.x,Y=high.y,Z=high.z,x=low.x,y=low.y,z=low.z;
    Vec vertices[6][4]={{{X,Y,z},{X,Y,Z},{X,y,Z},{X,y,z}},{{x,Y,Z},{x,Y,z},{x,y,z},{x,y,Z}},
        {{x,Y,z},{X,Y,z},{X,Y,Z},{x,Y,Z}},{{x,y,Z},{X,y,Z},{X,y,z},{x,y,z}},
        {{X,Y,Z},{x,Y,Z},{x,y,Z},{X,y,Z}},{{x,Y,z},{X,Y,z},{X,y,z},{x,y,z}}};
    float width=X-x,height=Y-y,depth=Z-z;
    float rectangles[6][4]={{float(u),v+depth,u+depth,v+depth+height},
        {u+depth+width,v+depth,u+depth+width+depth,v+depth+height},
        {u+depth,float(v),u+depth+width,v+depth},{u+depth+width,float(v),u+depth+2*width,v+depth},
        {u+2*depth+width,v+depth,u+2*depth+2*width,v+depth+height},
        {u+depth,v+depth,u+depth+width,v+depth+height}};
    float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    for(int side=0;side<6;side++) {
        ntr::GxHostVertex out[4];float* rect=rectangles[side];float uv[4][2]={{rect[0],rect[1]},{rect[2],rect[1]},{rect[2],rect[3]},{rect[0],rect[3]}};
        for(int i=0;i<4;i++) {
            Vec p=vertices[side][i];float py=(p.y-pivot.y)*cp-(p.z-pivot.z)*sp,pz=(p.y-pivot.y)*sp+(p.z-pivot.z)*cp;
            p.y=py+pivot.y;p.z=pz+pivot.z;
            if(bodyTilt) {float ct=std::cos(bodyTilt),st=std::sin(bodyTilt),y=(p.y-12)*ct-p.z*st,z=(p.y-12)*st+p.z*ct;p.y=y+12;p.z=z;}
            float wx=(p.x*cy-p.z*sy)*5.625f,wz=(p.x*sy+p.z*cy)*5.625f;
            out[i]={(base.x+wx)/8,(base.y+p.y*5.625f)/8,(base.z+wz)/8,uv[i][0],uv[i][1],0xffffffffu};
        }
        ntr::gx_host_quad(camera,out,texture->pixels.data(),texture->w,texture->h,31);
    }
}
void humanoid(const float camera[16],Vec at,float yaw,float swing,const Texture* skin,bool skeleton=false,bool zombie=false,float tilt=0,bool riding=false) {
    if(!skin) return;float limb=skeleton?1.f:2.f;
    cube(camera,at,{-4,24,-4},{4,32,4},skin,yaw,0,{0,24,0},0,0,tilt);
    cube(camera,at,{-4,12,-2},{4,24,2},skin,yaw,0,{},16,16,tilt);
    cube(camera,at,{4,12,-limb},{4+limb*2,24,limb},skin,yaw,zombie?-1.4f:swing,{5,24,0},40,16,tilt);
    cube(camera,at,{-4-limb*2,12,-limb},{-4,24,limb},skin,yaw,zombie?-1.4f:-swing,{-5,24,0},skin->h>=64?32:40,skin->h>=64?48:16,tilt);
    cube(camera,at,{0,0,-2},{4,12,2},skin,yaw,riding?-1.3f:-swing,{2,12,0},0,16,tilt);
    cube(camera,at,{-4,0,-2},{0,12,2},skin,yaw,riding?-1.3f:swing,{-2,12,0},skin->h>=64?16:0,skin->h>=64?48:16,tilt);
}
void entities(const float camera[16]) {
    for(const auto& e:client.state.entities) {
        std::string name=assets.entity(e.type);Vec at{e.x*scale,e.y*scale,e.z*scale};
        float yaw=(e.yaw+180)*pi/180,swing=std::sin(float(client.state.tick)*.65f)*.6f;
        if(name=="item" && e.count>0) {
            float spin=float(client.state.tick)*.08f;at.y+=20+std::sin(spin)*5;
            unsigned blockId=assets.defaultBlock(assets.item(e.item));
            if(blockId) blockDraw(camera,blockId,{at.x-12,at.y,at.z-12},24);
            else {MeshVertex face[4]={{-.25f,0,0,0,16},{.25f,0,0,16,16},{.25f,.5f,0,16,0},{-.25f,.5f,0,0,0}};quad(camera,face,icon(e.item),at,50,0xffffffffu,31,spin);}
            continue;
        }
        if(name=="arrow" || name=="spectral_arrow") {cube(camera,at,{-1,-1,-8},{1,1,8},assets.texture("entity/projectiles/arrow"),yaw,e.pitch*pi/180,{},0,0);continue;}
        if(name=="chicken") {
            auto* t=assets.texture("entity/chicken");
            cube(camera,at,{-2,9,-6},{2,15,-2},t,yaw,0,{},0,0);
            cube(camera,at,{-2,9,-8},{2,11,-6},t,yaw,0,{},14,0);
            cube(camera,at,{-1,7,-7},{1,9,-5},t,yaw,0,{},14,4);
            cube(camera,at,{-3,4,-3},{3,10,5},t,yaw,0,{},0,9);
            for(int side:{-1,1}) {
                float x=float(side*2);cube(camera,at,{x-.5f,0,0},{x+.5f,4,1},t,yaw,side*swing,{},26,0);
                cube(camera,at,{x-1,0,-2},{x+1,1,2},t,yaw,side*swing,{},26,0);
                cube(camera,at,{side<0?-4.f:3.f,5,-2},{side<0?-3.f:4.f,9,4},t,yaw,0,{},24,13);
            }
            continue;
        }
        if(e.fuse>=0) {blockDraw(camera,assets.defaultBlock("tnt"),{at.x-49,at.y,at.z-49},98);continue;}
        if(e.block) {blockDraw(camera,unsigned(e.block),{at.x-50,at.y,at.z-50});continue;}
        if(name=="zombie" || name=="skeleton") {
            humanoid(camera,at,yaw,swing,assets.texture(name=="zombie"?"entity/zombie/zombie":"entity/skeleton/skeleton"),name=="skeleton",name=="zombie");
            if(e.item) {MeshVertex face[4]={{0,0,0,0,16},{.7f,0,0,16,16},{.7f,.7f,0,16,0},{0,.7f,0,0,0}};quad(camera,face,icon(e.item),{at.x+30*std::cos(yaw),at.y+65,at.z-30*std::sin(yaw)},60,0xffffffffu,31,yaw);}
            continue;
        }
        if(name=="creeper") {
            auto* texture=assets.texture("entity/creeper/creeper");
            cube(camera,at,{-4,18,-4},{4,26,4},texture,yaw,0,{},0,0);
            cube(camera,at,{-4,6,-2},{4,18,2},texture,yaw,0,{},16,16);
            for(int side:{-1,1}) for(int front:{-1,1}) cube(camera,at,{side<0?-4.f:0.f,0,front<0?-6.f:2.f},{side<0?0.f:4.f,6,front<0?-2.f:6.f},texture,yaw,swing*front,{},0,16);
        } else if(name=="spider") {
            auto* t=assets.texture("entity/spider/spider");cube(camera,at,{-5,4,-4},{5,12,8},t,yaw,0,{},0,0);
            cube(camera,at,{-4,4,-12},{4,12,-4},t,yaw,0,{},32,4);
            for(int i=0;i<4;i++) for(int side:{-1,1}) cube(camera,at,{side<0?-20.f:4.f,4,float(i*4-6)},{side<0?-4.f:20.f,6,float(i*4-4)},t,yaw+(i-1.5f)*side*.2f,0,{},18,0);
        } else if(name=="minecart") {
            auto* t=assets.texture("entity/minecart");
            cube(camera,at,{-10,0,-8},{10,2,8},t,yaw,0,{},0,10);
            cube(camera,at,{-10,2,-9},{10,10,-7},t,yaw,0,{},0,0);
            cube(camera,at,{-10,2,7},{10,10,9},t,yaw,0,{},0,0);
            cube(camera,at,{-11,2,-7},{-9,10,7},t,yaw,0,{},0,0);
            cube(camera,at,{9,2,-7},{11,10,7},t,yaw,0,{},0,0);
        } else if(name=="boat") {
            std::string wood=assets.item(e.item);auto suffix=wood.find("_boat");wood=suffix==std::string::npos?"oak":wood.substr(0,suffix);
            auto* t=assets.texture("entity/boat/"+wood);cube(camera,at,{-14,0,-8},{14,2,8},t,yaw,0,{},0,0);
            cube(camera,at,{-14,2,-9},{14,8,-7},t,yaw,0,{},0,19);cube(camera,at,{-14,2,7},{14,8,9},t,yaw,0,{},0,19);
            cube(camera,at,{-15,2,-7},{-13,8,7},t,yaw,0,{},0,19);cube(camera,at,{13,2,-7},{15,8,7},t,yaw,0,{},0,19);
            for(int side:{-1,1}) cube(camera,at,{side<0?-23.f:14.f,4,-2},{side<0?-14.f:23.f,5,1},t,yaw,(e.count&(side<0?1:2))?swing:0,{},62,0);
        } else if(name=="pig" || name=="cow" || name=="sheep") {
            std::string path=name=="sheep"?"entity/sheep/sheep":name=="chicken"?"entity/chicken":"entity/"+name+"/"+name;
            auto* t=assets.texture(path);float h=name=="chicken"?8.f:14.f;
            cube(camera,at,{-4,6,-7},{4,h,7},t,yaw,0,{},28,8);cube(camera,at,{-4,h-4,-13},{4,h+4,-5},t,yaw,0,{},0,0);
            for(int side:{-1,1}) for(int front:{-1,1}) cube(camera,at,{side<0?-4.f:0.f,0,front<0?-6.f:2.f},{side<0?0.f:4.f,6,front<0?-2.f:6.f},t,yaw,swing*front,{},0,16);
        }
    }
}
void clicks() {
    if(pressed('I')) {inventoryOpen=!inventoryOpen;menuClosing=!inventoryOpen;catalogOpen=mobOpen=false;client.send("inventory\t"+std::to_string(inventoryOpen));client.send("cancel");itemHeld=miningHeld=false;}
    if(pressed('V')) client.send("creative\t"+std::to_string(!(client.state.flags&1)));
    if(pressed('B')) {if(inventoryOpen){client.send("inventory\t0");menuClosing=true;}catalogOpen=!catalogOpen;inventoryOpen=mobOpen=false;client.send("cancel");}
    if(pressed('M')) {if(inventoryOpen){client.send("inventory\t0");menuClosing=true;}mobOpen=!mobOpen;inventoryOpen=catalogOpen=false;client.send("cancel");}
    bool left=pressed(VK_LBUTTON),right=pressed(VK_RBUTTON);
    for(int i=0;i<9;i++) if(pressed('1'+i)) {
        if(mobOpen) {
            const char* names[]={"zombie","skeleton","creeper","spider","pig","cow","sheep","chicken","boat"};
            client.send("spawn\t"+std::string(names[i])+"\t"+std::to_string(eye.x/scale+direction.x*3)+"\t"+std::to_string((eye.y-162)/scale)+"\t"+std::to_string(eye.z/scale+direction.z*3));mobOpen=false;
        } else client.send("slot\t"+std::to_string(i));
    }
    if(catalogOpen) {
        if(pressed(VK_OEM_6)) catalogPage=(catalogPage+1)%std::max(1u,unsigned((assets.catalog.size()+34)/35));
        if(pressed(VK_OEM_4) && catalogPage) --catalogPage;
    }
    if(inventoryOpen || catalogOpen) {
        if(!left && !right) return;
        POINT point{};inputApi.cursor(&point);inputApi.client(inputApi.foreground(),&point);
        RECT window{};inputApi.rect(inputApi.foreground(),&window);
        int x,y,w,h,sets;ntr::gx_debug_viewport(x,y,w,h,sets);float s=std::max(1.f,w/256.f);
        float px=float(point.x)*w/std::max(1L,window.right),py=float(point.y)*h/std::max(1L,window.bottom);
        if(inventoryOpen) s=std::min(s,(h-8)/menuHeight());
        if(inventoryOpen) for(auto rect:slotRects(x+(w-176*s)/2,y+(h-menuHeight()*s)/2,s))
            if(px>=rect.x && px<rect.x+16*s && py>=rect.y && py<rect.y+16*s) client.send("click\t"+std::to_string(rect.slot)+"\t"+(right?"1":"0"));
        if(catalogOpen) for(unsigned i=0;i<35;i++) {
            float ix=x+(28+(i%7)*30)*s,iy=y+(46+(i/7)*30)*s;
            unsigned index=catalogPage*35+i;
            if(px>=ix && px<ix+24*s && py>=iy && py<iy+24*s && index<assets.catalog.size()) {client.send("give\t"+assets.catalog[index]+"\t64");catalogOpen=false;}
        }
        return;
    }
    if(mobOpen) return;
    if(pressed('G')) {
        Cell at=cell(position(local()));at.x+=5;at.y+=7;
        client.send("machine\t"+coordinates(at)+"\t"+std::to_string(down(VK_SHIFT)));
    }
    if(down(VK_LBUTTON) && hasTarget) {
        if(targetBoss) {if(miningHeld){client.send("cancel");miningHeld=false;}if(left) punchBoss(targetBoss);}
        else if(targetEntity>=0 && left) client.send("attack\t"+std::to_string(targetEntity));
        else if(targetEntity<0) {miningHeld=true;if(nativeTarget) mirror(target,"stone");client.send("mine\t"+coordinates(target));}
    } else if(!down(VK_LBUTTON) && miningHeld) {client.send("cancel");miningHeld=false;}
    if(right) {
        itemHeld=true;
        if(targetEntity>=0) client.send("interact\t"+std::to_string(targetEntity));
        else if(hasTarget) {
            if(nativeTarget) mirror(target,"stone");
            const char* side[]={"east","west","up","down","south","north"};
            client.send("use\t"+coordinates(target)+"\t"+side[targetSide]);
        } else client.send("item");
    }
    if(itemHeld && !down(VK_RBUTTON)) {client.send("release");itemHeld=false;}
    if(pressed('F')) {if(client.state.flags&4) client.send("dismount");else client.send(client.state.flags&2?"wing\t0":"wing");}
    if(pressed('R') && (client.state.flags&2)) client.send("boost");
    if(client.state.flags&4) client.send("boat\t"+std::to_string(down('A'))+"\t"+std::to_string(down('D'))+"\t"+std::to_string(down('W') || std::getenv("SM64DS_STEVE_TEST_BOAT"))+"\t"+std::to_string(down('S')));
}
}
bool enabled_impl() {static int enabled=-1;if(enabled<0) enabled=std::getenv("SM64DS_STEVE")?1:0;return enabled!=0;}
void frame_impl() {
    if(!native_enabled() || failed) return;
    bossFrame();
    if(root.empty()) {const char* path=std::getenv("SM64DS_ASSET_ROOT");root=path?path:".";}
    if(!loaded) {loaded=assets.load(root+"/runtime/steve-assets");if(!loaded) {failed=true;return;}}
    client.poll(root);if(!client.fresh()) return;
    if(airRefillPending && client.state.tick>airRefillTick && client.state.air>=295) {
        airRefillPending=false;std::fprintf(stderr,"[steve-air] acknowledged air=%d tick=%llu\n",client.state.air,static_cast<unsigned long long>(client.state.tick));
    }
    void* p=local();if(!p) return;
    int token=currentScene();
    if(token!=scene) {scene=token;chunkMeshes.clear();dirtyChunks.clear();terrain.clear();holes.clear();terrainSurfaces.clear();terrainQueue.clear();terrainTriangles.clear();client.blocks.clear();client.terrainOriginal.clear();client.edits.clear();airBubbles.clear();lastCoins=port_course_coins();previousHP=-1;consumedPush={};client.send("sync");}
    auto* player=static_cast<Player*>(p);
    // Steve has no Nintendo entrance animation. Complete that one state through
    // its real cleanup/init path once the cutscene has released the avatar.
    if(!data_0209fc48 && frames>2 && player->IsState(data_ov002_021104b4)) player->ChangeState(data_ov002_02110154);
    Vec at=position(p);bool noControl=scripted(p);
    int hp=player->GetHealth();float damage=0;
    if(client.state.health<=0 && hp>0 && !noControl) {
        if(!respawnPending) {client.send("respawn\t"+std::to_string(at.x/scale)+"\t"+std::to_string(at.y/scale)+"\t"+std::to_string(at.z/scale));respawnPending=true;}
    } else if(client.state.health>0) respawnPending=false;
    previousHP=hp;
    float camera[16];view(camera);float yaw=std::atan2(-camera[2],camera[10])*180/pi+180,pitch=std::asin(std::clamp(camera[6],-1.f,1.f))*180/pi;
    if(client.state.flags&2) {
        if(!flightControls) {flightYaw=client.state.yaw;flightPitch=-10;flightControls=true;}
        if(!noControl && !inventoryOpen && !catalogOpen && !mobOpen) {flightYaw+=2*(int(down('D'))-int(down('A')));flightPitch=std::clamp(flightPitch+int(down('W'))-int(down('S')),-60.f,60.f);}
        yaw=flightYaw;pitch=flightPitch;
    } else flightControls=false;
    bool underwater=false;
    auto* bm=assets.block(block(cell({at.x,at.y+((client.state.flags&18)?40.f:162.f),at.z})));underwater=bm&&bm->name=="water";
    // Native swimming is a state, so its oxygen needs the same Minecraft clock.
    underwater=underwater || (player->mIsUnderwater && func_ov002_020ceb54(player));
    submerged=underwater;
    if(!noControl)for(auto& b:airBubbles){++b.age;b.at.y+=1.5f;}
    airBubbles.erase(std::remove_if(airBubbles.begin(),airBubbles.end(),[](const AirBubble& b){return b.age>=60;}),airBubbles.end());
    std::ostringstream pose;pose<<"pose\t"<<scene<<'\t'<<at.x/scale<<'\t'<<at.y/scale<<'\t'<<at.z/scale<<'\t'<<yaw<<'\t'<<pitch<<'\t'<<underwater<<'\t'<<noControl<<'\t'<<damage<<'\t'<<!player->mIsAirborne;
    client.send(pose.str());
    if(!noControl) {
        if(player->mHasWings && player->mIsAirborne && !(client.state.flags&6) && frames%20==0) client.send("wing");
        if(capFlight && !player->mHasWings && (client.state.flags&2)) client.send("wing\t0");
        capFlight=player->mHasWings!=0;
    }
    int coins=port_course_coins();if(coins>lastCoins && underwater) coinAir(at,coins-lastCoins);lastCoins=coins;
    if(client.state.token!=scene) return;
    terrainQuota=12;
    float standing;
    if(ground({at.x,at.y+300,at.z},p,standing)) {Cell base=cell({at.x,standing,at.z});for(int depth=0;depth<4;depth++) mirror({base.x,base.y-depth,base.z},depth?"stone":"grass_block");surface(base,standing);}
    for(auto original:client.terrainOriginal) terrain.insert(original.first);
    for(Cell at:client.edits) {
        if(terrain.count(at) && client.terrainOriginal.count(at) && block(at)!=client.terrainOriginal.at(at)) holes.insert(at);
        if(client.terrainOriginal.count(at) && block(at)==client.terrainOriginal.at(at)) holes.erase(at);mark(at);
    }
    client.edits.clear();
    for(const auto& e:client.state.entities) if(e.fuse>=0 && terrainQuota>0) {
        float floor;Vec sample{e.x*scale,e.y*scale+100,e.z*scale};
        if(ground(sample,p,floor)) {Cell base=cell({sample.x,floor,sample.z});for(int depth=0;depth<4;depth++) mirror({base.x,base.y-depth,base.z},depth?"stone":"grass_block");surface(base,floor);}
    }
    for(int i=0;i<12 && terrainQuota>2 && !terrainQueue.empty();i++) {
        Cell around=cell(at);
        auto next=std::min_element(terrainQueue.begin(),terrainQueue.end(),[&](const auto& a,const auto& b) {
            auto distance=[&](Cell c){long long x=c.x-around.x,y=c.y-around.y,z=c.z-around.z;return x*x+y*y+z*z;};return distance(a.first)<distance(b.first);
        });Cell at=next->first;bool floor=next->second;terrainQueue.erase(next);
        mirror(at,floor?"grass_block":"stone");
    }
    if(client.state.containerSize==0) menuClosing=false;
    if(!menuClosing && client.state.containerSize>0 && client.state.craftingWidth!=2) {inventoryOpen=true;catalogOpen=mobOpen=false;}
    if(std::getenv("SM64DS_STEVE_TEST_ANIM"))inventoryOpen=false;
    if(!noControl) {aim(p);clicks();}
    else if(inventoryOpen || catalogOpen || mobOpen) {client.send("inventory\t0");inventoryOpen=catalogOpen=mobOpen=false;}
    Cell body=cell({at.x,at.y+5,at.z});const auto* bodyMesh=assets.block(block(body));
    bool swim=bodyMesh && bodyMesh->name=="water" && (!client.terrainOriginal.count(body) || client.terrainOriginal.at(body)!=block(body));
    bool controls=!noControl && !inventoryOpen && !catalogOpen && !mobOpen;
    if(!controls && (client.state.flags&4)) client.send("boat\t0\t0\t0\t0");
    client.send("swim\t"+std::to_string(swim && !(client.state.flags&4))+"\t"+std::to_string(controls?(int(down('D'))-int(down('A'))):0)+"\t"+std::to_string(controls?(int(down(VK_SPACE))-int(down(VK_CONTROL))):0)+"\t"+std::to_string(std::getenv("SM64DS_STEVE_TEST_SWIM")?1:(controls?(int(down('W'))-int(down('S'))):0)));
    // Incremental terrain import: one column per frame, no large world generation.
    if(!noControl && frames%2==0) {
        int index=int((frames/2)%81),dx=index%9-4,dz=index/9-4;Cell center=cell(at);
        float height=at.y-600;
        Vec sample{(center.x+dx+.5f)*scale,at.y+300,(center.z+dz+.5f)*scale};
        if(ground(sample,p,height)) {Cell top{center.x+dx,int(std::floor(height/scale)),center.z+dz};for(int depth=0;depth<4;depth++) mirror({top.x,top.y-depth,top.z},depth?"stone":"grass_block");surface(top,height);}
        float surface;
        if(waterSurface(sample,p,surface) && surface>height+10 && surface>at.y-600) {
            int top=int(std::floor((surface-1)/scale));int bottom=std::max(int(std::floor(height/scale))+1,top-5);
            for(int y=top;y>=bottom;y--) mirror({center.x+dx,y,center.z+dz},"water");
        }
    }
    ++frames;
    if(std::getenv("SM64DS_STEVE_TEST_SPECIAL") && frames==40) {
        Cell origin=cell(at);
        const char* names[]={"chest[facing=south]","red_bed[facing=south,part=head]","blue_shulker_box","skeleton_skull","oak_sign[rotation=0]","red_banner[rotation=0]","dragon_head[rotation=0]","bell[facing=south]","conduit"};
        for(int i=0;i<9;i++) {
            Cell c{origin.x+(i%3-1)*2,origin.y+1,origin.z-3-(i/3)*2};
            client.send("block\t"+coordinates({c.x,c.y-1,c.z})+"\tstone");
            if(i==1) {
                client.send("block\t"+coordinates({c.x,c.y-1,c.z-1})+"\tstone");
                client.send("block\t"+coordinates({c.x,c.y,c.z-1})+"\tred_bed[facing=south,part=foot]");
            }
            client.send("block\t"+coordinates(c)+"\t"+names[i]);
        }
        const char* items[]={"shield","trident","chest","blue_shulker_box","red_banner"};
        for(int i=0;i<5;i++) {client.send("slot\t"+std::to_string(i+1));client.send(std::string("give\t")+items[i]+"\t1");}
        client.send("slot\t0");
    }
    if(std::getenv("SM64DS_STEVE_TEST_CONDUIT")) {
        Cell origin=cell(at);origin.y+=2;origin.z-=4;
        if(frames==40) {
            for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)for(int z=-1;z<=1;z++)client.send("block\t"+coordinates({origin.x+x,origin.y+y,origin.z+z})+"\twater");
            for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++)for(int z=-2;z<=2;z++)if((x==0 && (std::abs(y)==2 || std::abs(z)==2)) || (y==0 && (std::abs(x)==2 || std::abs(z)==2)) || (z==0 && (std::abs(x)==2 || std::abs(y)==2)))client.send("block\t"+coordinates({origin.x+x,origin.y+y,origin.z+z})+"\tprismarine");
            client.send("block\t"+coordinates(origin)+"\tconduit");
        }
        static bool opened=false;
        for(const auto& v:client.state.visuals)if(v.kind=="conduit") {
            if(!opened && v.progress>0 && v.glow) {
                std::fprintf(stderr,"[steve-conduit] full-frame active=1 hunting=1\n");
                for(int y=-2;y<=2;y++)client.send("block\t"+coordinates({origin.x,origin.y+y,origin.z+2})+"\tair");opened=true;
            }
            if(opened && frames%30==0)std::fprintf(stderr,"[steve-conduit] window active=%.0f hunting=%u\n",v.progress,v.glow);
        }
    }
    if(std::getenv("SM64DS_STEVE_TEST_ANIM")) {
        Cell origin=cell(at);
        if(frames==65) {
            client.send("decorate\t"+coordinates({origin.x,origin.y+1,origin.z-5})+"\te1RleHQxOid7InRleHQiOiJTVEVWRSIsImJvbGQiOnRydWUsImNvbG9yIjoiZ29sZCJ9JyxUZXh0MjoneyJ0ZXh0IjoiU002NCBEUyJ9JyxUZXh0MzoneyJ0ZXh0IjoiTkJUIHdvcmtzIn0nLFRleHQ0Oid7InRleHQiOiJIZWxsbyDkuJbnlYwiLCJpdGFsaWMiOnRydWV9JyxDb2xvcjoid2hpdGUiLEdsb3dpbmdUZXh0OjFifQ==");
            client.send("decorate\t"+coordinates({origin.x+2,origin.y+1,origin.z-5})+"\te1BhdHRlcm5zOlt7UGF0dGVybjoiY3IiLENvbG9yOjB9LHtQYXR0ZXJuOiJibyIsQ29sb3I6MTF9LHtQYXR0ZXJuOiJtYyIsQ29sb3I6NH1dfQ==");
        }
        if(frames==65)client.send("block\t"+coordinates({origin.x-2,origin.y,origin.z-7})+"\tredstone_block");
        if(frames==80)client.send("use\t"+coordinates({origin.x-2,origin.y+1,origin.z-3})+"\tup");
        if(frames==115)client.send("inventory\t0");
        if(frames==145)client.send("use\t"+coordinates({origin.x+2,origin.y+1,origin.z-3})+"\tup");
        if(frames==105 || frames==140 || frames==170) for(const auto& v:client.state.visuals)std::fprintf(stderr,"[steve-visual] frame=%u kind=%s progress=%.2f text=%zu patterns=%s\n",frames,v.kind.c_str(),v.progress,v.text.size(),v.patterns.c_str());
    }
    if(std::getenv("SM64DS_STEVE_TEST_HEALTH")) {if(frames==40) GiveHealth(data_0209f250,-256);if(frames==80) GiveHealth(data_0209f250,256);}
    if(std::getenv("SM64DS_STEVE_TEST_SHAPE") && frames==40) {
        Cell c=cell(at);std::string shape=std::getenv("SM64DS_STEVE_TEST_SHAPE");
        client.send("block\t"+coordinates(c)+"\t"+(shape=="fence"?"oak_fence":"stone_slab[type=bottom]"));
        shapeFixture=c;shapePending=true;
    }
    if(shapePending) {
        const auto* placed=assets.block(block(shapeFixture));
        std::string wanted=std::getenv("SM64DS_STEVE_TEST_SHAPE");
        if(placed && placed->name==(wanted=="fence"?"oak_fence":"stone_slab")) {
            Cell c=shapeFixture;player->mPosX=int((c.x+.5f)*scale*fix);player->mPosZ=int((c.z+.5f)*scale*fix);player->mPosY=int((c.y+3)*scale*fix);player->mIsAirborne=1;shapePending=false;
        }
    }
    if((std::getenv("SM64DS_STEVE_TEST_SWIM") || std::getenv("SM64DS_STEVE_TEST_BOAT")) && frames==40) {
        Cell c=cell(at);
        for(int dx=-2;dx<=2;dx++) for(int dz=-3;dz<=5;dz++) {
            client.send("block\t"+coordinates({c.x+dx,c.y-1,c.z+dz})+"\tstone");
            for(int dy=0;dy<=3;dy++) client.send("block\t"+coordinates({c.x+dx,c.y+dy,c.z+dz})+"\t"+(dx==-2 || dx==2 || dz==-3 || dz==5?"stone":"water"));
        }
        player->mPosY=int((c.y+1.5f)*scale*fix);player->mIsAirborne=1;
    }
    if(std::getenv("SM64DS_STEVE_TEST_SWIM_COIN") && frames==140) {
        Vector3 at{player->mPosX,player->mPosY,player->mPosZ};
        auto* coin=dActor_c::Spawn(288,0,at,nullptr,player->mAreaId,-1);
        std::fprintf(stderr,"[steve-air-test] native coin=%u air=%d underwater=%d\n",coin?coin->uniqueID:0,client.state.air,underwater);
    }
    if(std::getenv("SM64DS_STEVE_TEST_BOAT")) {
        if(frames==60) client.send("spawn\tboat\t"+std::to_string(at.x/scale)+"\t"+std::to_string(at.y/scale+5)+"\t"+std::to_string(at.z/scale));
        if(frames>60 && !(client.state.flags&4)) for(const auto& e:client.state.entities) if(assets.entity(e.type)=="boat") {client.send("ride\t"+std::to_string(e.id));break;}
    }
    if(std::getenv("SM64DS_STEVE_TEST_FLIGHT") && frames==40) {
        player->mPosY+=int(4000*fix);player->mIsAirborne=1;
        client.send("pose\t"+std::to_string(scene)+"\t"+std::to_string(at.x/scale)+"\t"+std::to_string(at.y/scale+40)+"\t"+std::to_string(at.z/scale)+"\t0\t-10\t0\t0\t0\t0");client.send("wing");
    }
    if(std::getenv("SM64DS_STEVE_TEST_CRAFT")) {
        Cell c=cell(at);c.x+=3;
        if(frames==40) {client.send("block\t"+coordinates(c)+"\tcrafting_table");client.send("slot\t0");client.send("give\toak_planks\t6");client.send("use\t"+coordinates(c)+"\tup");}
        if(frames==70 && client.state.craftingWidth==3) {client.send("click\t37\t0");for(int slot:{1,2,4,5,7,8}) client.send("click\t"+std::to_string(slot)+"\t1");}
        if(frames==100) std::fprintf(stderr,"[steve-craft] width=%u output=%s count=%u\n",client.state.craftingWidth,assets.item(client.state.container[0].id).c_str(),client.state.container[0].count);
    }
    if(std::getenv("SM64DS_STEVE_TEST") && frames==30) {
        Cell c=cell(at);
        client.send("block\t"+coordinates({c.x+8,c.y+1,c.z-8})+"\ttnt");
        client.send("block\t"+coordinates({c.x+9,c.y+1,c.z-8})+"\tredstone_block");
        client.send("block\t"+coordinates({c.x+2,c.y,c.z-3})+"\tstone");
        client.send("spawn\tzombie\t"+std::to_string(at.x/scale+3)+"\t"+std::to_string(at.y/scale+2)+"\t"+std::to_string(at.z/scale-4));
        std::fprintf(stderr,"[steve-test] submitted TNT/redstone/block/mob fixture\n");
    }
    if(std::getenv("SM64DS_STEVE_TEST_HOLE") && frames==80) {
        Cell atCell=cell(at);for(int dx=-1;dx<=0;dx++) for(int dz=-1;dz<=0;dz++) {
            Cell target{atCell.x+dx,atCell.y,atCell.z+dz};
            if(!client.terrainOriginal.count(target)) client.send("terrain\t"+coordinates(target)+"\tgrass_block");
            client.send("block\t"+coordinates(target)+"\tair");
        }
        std::fprintf(stderr,"[steve-test] carved native terrain fixture\n");
    }
    if(frames%60==0) std::fprintf(stderr,"[steve] frame=%u tick=%llu blocks=%zu entities=%zu chunks=%zu\n",frames,
        static_cast<unsigned long long>(client.state.tick),client.blocks.size(),client.state.entities.size(),chunkMeshes.size());
    if(frames%60==0) std::fprintf(stderr,"[steve-physics] flags=%d native=(%.2f,%.2f,%.2f) server=(%.2f,%.2f,%.2f) air=%d shapes=%d health=%.2f hp=%d script=%x control=%d\n",client.state.flags,at.x,at.y,at.z,client.state.x*scale,client.state.y*scale,client.state.z*scale,client.state.air,assets.collisionReady,client.state.health,player->GetHealth(),data_0209fc48,player->mIsNoControl);
    if(std::getenv("SM64DS_STEVE_TEST_FLIGHT") && frames>=35 && frames<=70 && frames%5==0) std::fprintf(stderr,"[steve-flight] frame=%u flags=%d noControl=%d pitch=%.2f nativeY=%.2f serverY=%.2f\n",frames,client.state.flags,noControl,pitch,at.y,client.state.y*scale);
    if(std::getenv("SM64DS_STEVE_TEST_HOLE") && frames%60==0) std::fprintf(stderr,"[steve-hole] holes=%zu pos=(%.2f,%.2f,%.2f)\n",holes.size(),at.x,at.y,at.z);
}
void before_impl(void* p) {
    if(native_enabled())port_clsn_prepare_dispatch();
    if(!p) p=local();
    if(beforePhysicsFrame==frames) return;
    if(native_enabled() && p && p==local()) {
        beforePhysicsFrame=frames;
        previousPosition=position(p);
        if(client.fresh() && client.state.token==currentScene() && static_cast<Player*>(p)->mIsUnderwater) {
            // Player.cpp's water helper uses +0x6ca for its damage timer and
            // +0x6b8 for automatic surface healing. Minecraft owns both clocks.
            std::memset(static_cast<Player*>(p)->pad_6ca,0,2);
            static_cast<Player*>(p)->mWalkTimer=30;
        }
    }
}
bool health_impl(int index,int amount) {
    if(!native_enabled() || !client.fresh() || !local() || index!=data_0209f250 || client.state.token!=currentScene()) return false;
    if(amount<0 && (client.state.flags&1)) return true;
    if(client.state.health>0 && !(amount<0 && scripted(local()))) {
        float units=std::min(20.f,std::abs(float(amount))*2.5f/256.f);
        if(units>0) client.send(std::string(amount<0?"hurt\t":"heal\t")+std::to_string(units));
    }
    return false;
}
void after_impl(void* p) {
    if(!p) p=local();
    if(afterPhysicsFrame==frames) return;
    if(!native_enabled() || !p || !client.fresh() || p!=local() || scripted(p) || client.state.token!=currentScene()) return;
    afterPhysicsFrame=frames;
    auto* player=static_cast<Player*>(p);Vec now=position(p);
    Vec push{client.state.pushedX,client.state.pushedY,client.state.pushedZ};
    if(!(client.state.flags&22)) {now.x+=(push.x-consumedPush.x)*scale;now.y+=(push.y-consumedPush.y)*scale;now.z+=(push.z-consumedPush.z)*scale;}
    consumedPush=push;
    if(!(client.state.flags&22) && now.y<previousPosition.y) {
        float floor;
        if(ground({now.x,previousPosition.y+8,now.z},p,floor) && !native_removed(now.x,floor,now.z) && previousPosition.y>=floor-4 && now.y<floor) {
            now.y=floor;player->mVertSpeed=0;player->mGroundY=int(floor*fix);player->mIsAirborne=0;player->mMeshClsn.SetGroundFlag();
        }
    }
    auto voxelSupport=[&]() {
        Cell low=cell({now.x-25,now.y-151,now.z-25}),high=cell({now.x+25,now.y-.01f,now.z+25});
        for(int y=low.y;y<=high.y;y++) for(int z=low.z;z<=high.z;z++) for(int x=low.x;x<=high.x;x++) {
            unsigned id=block({x,y,z});if(!solid(id)) continue;
            const auto* mesh=assets.block(id);if(!mesh) continue;
            for(const auto& box:mesh->collision)
                if(std::abs((y+box.maxY)*scale-std::max(now.y,previousPosition.y))<.1f && now.x+25>(x+box.minX)*scale && now.x-25<(x+box.maxX)*scale && now.z+25>(z+box.minZ)*scale && now.z-25<(z+box.maxZ)*scale) return true;
        }
        return false;
    };
    if(holes.count(cell({now.x,float(player->mGroundY)/fix,now.z})) && !(client.state.flags&6) && player->mVertSpeed<=0 && now.y<=previousPosition.y && !voxelSupport()) {
        holeFallVelocity=std::min(80.f,holeFallVelocity+4.f);
        now.y=previousPosition.y-holeFallVelocity;player->mIsAirborne=1;
    } else holeFallVelocity=0;

    // The interaction proxy keeps native scripted actions; voxel surfaces add
    // support and walls without changing Player's recovered object layout.
    Cell min=cell({now.x-25,now.y-150,now.z-25}),max=cell({now.x+25,now.y+180,now.z+25});
    for(int y=min.y;y<=max.y;y++) for(int z=min.z;z<=max.z;z++) for(int x=min.x;x<=max.x;x++) {
        Cell c{x,y,z};if((terrain.count(c) && (!client.terrainOriginal.count(c) || block(c)==client.terrainOriginal.at(c)) && !holes.count(neighbor(c,2))) || !solid(block(c))) continue;
        auto* mesh=assets.block(block(c));
        static const std::vector<CollisionBox> cubeBounds{{0,0,0,1,1,1}};
        const auto& boxes=assets.collisionReady?mesh->collision:cubeBounds;
        for(const auto& box:boxes) {
        float loX=(x+box.minX)*scale,hiX=(x+box.maxX)*scale,loY=(y+box.minY)*scale,hiY=(y+box.maxY)*scale,loZ=(z+box.minZ)*scale,hiZ=(z+box.maxZ)*scale;
        if(now.x+25<=loX || now.x-25>=hiX || now.z+25<=loZ || now.z-25>=hiZ || now.y+180<=loY || now.y>=hiY) continue;
        if((previousPosition.y>=hiY-4 || (!player->mIsAirborne && hiY-now.y<=60)) && now.y<hiY) {now.y=hiY;player->mIsAirborne=0;player->mGroundY=int(hiY*fix);if(player->mVertSpeed<0) player->mVertSpeed=0;holeFallVelocity=0;}
        else if(previousPosition.x+25<=loX) now.x=loX-25;
        else if(previousPosition.x-25>=hiX) now.x=hiX+25;
        else if(previousPosition.z+25<=loZ) now.z=loZ-25;
        else if(previousPosition.z-25>=hiZ) now.z=hiZ+25;
        else if(previousPosition.y+180<=loY) now.y=loY-180;
        }
    }
    if(client.state.flags&22) {now={client.state.x*scale,client.state.y*scale,client.state.z*scale};if(client.state.flags&18) player->mIsAirborne=1;if(client.state.flags&4) {player->mGroundY=int(now.y*fix);player->mIsAirborne=0;}}
    if((inventoryOpen || catalogOpen || mobOpen) && !(client.state.flags&22)) now=previousPosition;
    player->mPosX=int(now.x*fix);player->mPosY=int(now.y*fix);player->mPosZ=int(now.z*fix);
    if(std::getenv("SM64DS_STEVE_TEST_HOLE") && frames%60==0) std::fprintf(stderr,"[steve-after] y=%.2f previous=%.2f ground=%.2f fall=%.2f\n",now.y,previousPosition.y,float(player->mGroundY)/fix,holeFallVelocity);
    if(client.state.tick!=previousTick && !(respawnPending && client.state.health<=0)) {
        previousTick=client.state.tick;
        int targetHP=client.state.health>0?std::clamp(int(std::ceil(client.state.health*.4f)),1,8):0;
        int current=player->GetHealth();
        if(targetHP!=current && current>0) {data_02092144[data_0209f250]=short(targetHP<<8);previousHP=targetHP;}
        if(targetHP==0 && current>0) KillPlayer();
    }
}
bool render_player_impl(void* p) {
    if(!native_enabled() || !loaded || p!=local()) return false;
    Vec at=position(p);auto* player=static_cast<Player*>(p);float camera[16];view(camera);
    float yaw=static_cast<unsigned short>(player->mAngleY)*2*pi/65536+pi;
    if(client.state.flags&6) yaw=(client.state.yaw+180)*pi/180;
    float movement=std::hypot(at.x-previousPosition.x,at.z-previousPosition.z);
    float swing=movement>0.5f?std::sin(float(frames)*.5f)*.65f:0;
    if(miningHeld) swing=std::sin(float(frames))*.9f;
    if(punchAnimation) swing=std::sin((10-punchAnimation)*pi/10.f)*1.4f;
    float tilt=client.state.flags&18?-pi/2:0;
    humanoid(camera,at,yaw,tilt?0:swing,assets.texture("entity/steve"),false,false,tilt,(client.state.flags&4)!=0);
    if(client.state.flags&2 || player->mHasWings) {
        auto* wing=assets.texture("entity/elytra");
        cube(camera,at,{-13,6,2},{-3,26,4},wing,yaw+.4f,-.25f,{-3,24,2},22,0,tilt);
        cube(camera,at,{3,6,2},{13,26,4},wing,yaw-.4f,-.25f,{3,24,2},22,0,tilt);
    }
    auto held=client.state.inventory[std::clamp(client.state.selected,0,8)];
    if(held.count) {
        std::string name=assets.item(held.id);unsigned id=assets.defaultBlock(name);
        Vec hand{at.x+std::cos(yaw)*48,at.y+75,at.z+std::sin(yaw)*48};
        if(id) blockDraw(camera,id,hand,30);
        else {
            const Texture* item=icon(held.id);MeshVertex face[4]={{0,0,0,0,16},{.5f,0,0,16,16},{.5f,.75f,0,16,0},{0,.75f,0,0,0}};
            quad(camera,face,item,hand,70,0xffffffffu,31,yaw);
        }
    }
    return true;
}
void render_world_impl() {
    if(!native_enabled() || !loaded || !local()) return;
    float camera[16];view(camera);
    Cell current=section(cell(position(local())));
    for(int i=0;i<4 && !dirtyChunks.empty();i++) {
        auto nearest=std::min_element(dirtyChunks.begin(),dirtyChunks.end(),[&](Cell a,Cell b) {
            auto distance=[&](Cell c){long long x=c.x-current.x,y=c.y-current.y,z=c.z-current.z;return x*x+y*y+z*z;};
            return distance(a)<distance(b);
        });Cell key=*nearest;dirtyChunks.erase(nearest);rebuild(key);
    }
    Vec eye=position(local());
    for(const auto& chunk:chunkMeshes) {
        float dx=(chunk.first.x*16+8)*scale-eye.x,dz=(chunk.first.z*16+8)*scale-eye.z;
        if(dx*dx+dz*dz>6400.f*6400.f) continue;
        for(const auto& draw:chunk.second) {
            auto* model=assets.block(draw.id);const auto& face=*draw.face;
            quad(camera,face.vertex,assets.texture(face.texture),{draw.at.x*scale,draw.at.y*scale,draw.at.z*scale},scale,tint(*model,face),model->name=="water"?22:31);
        }
    }
    std::map<Cell,const BlockVisual*> visuals;for(const auto& v:client.state.visuals)visuals[v.at]=&v;
    for(const auto& entry:client.blocks) {const auto* mesh=assets.block(entry.second);if(!mesh || !dynamicModel(*mesh))continue;float dx=entry.first.x*scale-eye.x,dz=entry.first.z*scale-eye.z;if(dx*dx+dz*dz>6400.f*6400.f)continue;auto found=visuals.find(entry.first);blockEntityDraw(camera,entry.first,entry.second,found==visuals.end()?nullptr:found->second);}
    for(const auto& moving:client.state.moving) blockDraw(camera,moving.block,{(moving.at.x+moving.x)*scale,(moving.at.y+moving.y)*scale,(moving.at.z+moving.z)*scale});
    if(hasTarget && targetEntity<0 && client.state.mining>=0) {
        auto* model=assets.block(block(target));auto* cracks=assets.texture("block/destroy_stage_"+std::to_string(std::clamp(client.state.mining,0,9)));
        if(model) for(const auto& face:model->faces) quad(camera,face.vertex,cracks,{target.x*scale-.25f,target.y*scale-.25f,target.z*scale-.25f},scale+.5f);
    }
    entities(camera);
    drawAirBubbles(camera);
    hud();
}
    bool removed_impl(float x,float y,float z) {return native_enabled() && client.fresh() && holes.count(cell({x,y,z}))!=0;}
#include "terrain_clip.inl"
namespace {
struct Register {
    Register() {
        ntr::gx_host_triangle_filter=cutStage;
        collision_prepare_hook=port_clsn_prepare_dispatch;sphere_support_hook=port_steve_sphere_support;
        continuous_override_hook=port_steve_override_continuous;
        enabled_hook=enabled_impl;frame_hook=frame_impl;before_hook=before_impl;after_hook=after_impl;
        player_render_hook=render_player_impl;world_render_hook=render_world_impl;removed_hook=removed_impl;
          health_hook=health_impl;
    }
} registration;
}
}
