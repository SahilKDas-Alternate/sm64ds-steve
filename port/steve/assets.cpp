#include "assets.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <cstdio>
namespace steve {
namespace {
template<class T> T read(std::ifstream& file) {
    T value{}; if(!file.read(reinterpret_cast<char*>(&value),sizeof(T))) throw std::runtime_error("truncated asset pack"); return value;
}
std::string text(std::ifstream& file) {
    auto length=read<uint16_t>(file); if(length>4096) throw std::runtime_error("asset string budget");
    std::string out(length,'\0');if(!file.read(out.data(),length)) throw std::runtime_error("truncated asset name");return out;
}
void header(std::ifstream& file,const char* magic) {
    char found[4]{};if(!file.read(found,4)||std::memcmp(found,magic,4)) throw std::runtime_error("asset pack version mismatch");
}
}
unsigned Texture::frameOffset(uint64_t tick) const {
    unsigned total=0;for(auto frame:frames) total+=frame.second;
    if(!total) return 0; tick%=total;
    for(auto frame:frames) {if(tick<frame.second) return frame.first*frameHeight;tick-=frame.second;}return 0;
}
bool Assets::load(const std::string& directory) {
    try {
        std::ifstream pixels(directory+"/textures.bin",std::ios::binary);header(pixels,"STX1");
        unsigned count=read<uint32_t>(pixels);if(count>4096) throw std::runtime_error("texture budget");
        size_t total=0;
        for(unsigned i=0;i<count;i++) {
            std::string name=text(pixels); Texture texture;
            texture.w=read<uint16_t>(pixels);texture.h=read<uint16_t>(pixels);texture.frameHeight=read<uint16_t>(pixels);
            unsigned frames=read<uint16_t>(pixels);
            if(!texture.w || !texture.h || texture.w>4096 || texture.h>4096 || frames>2048) throw std::runtime_error("texture dimensions");
            for(unsigned j=0;j<frames;j++) {unsigned index=read<uint16_t>(pixels),duration=read<uint16_t>(pixels);texture.frames.push_back({index,duration});}
            total+=static_cast<size_t>(texture.w)*texture.h*4;if(total>128*1024*1024) throw std::runtime_error("decoded texture memory budget");
            texture.pixels.resize(static_cast<size_t>(texture.w)*texture.h);
            if(!pixels.read(reinterpret_cast<char*>(texture.pixels.data()),texture.pixels.size()*4)) throw std::runtime_error("texture payload");
            textures.emplace(std::move(name),std::move(texture));
        }
        std::ifstream meshes(directory+"/models.bin",std::ios::binary);header(meshes,"SMD1");
        count=read<uint32_t>(meshes);if(count>65536) throw std::runtime_error("block state budget");
        size_t faces=0;
        for(unsigned i=0;i<count;i++) {
            unsigned id=read<uint32_t>(meshes);BlockMesh model;model.name=text(meshes);model.properties=text(meshes);
            unsigned n=read<uint32_t>(meshes);faces+=n;if(n>1024 || faces>1000000) throw std::runtime_error("mesh face budget");
            for(unsigned j=0;j<n;j++) {
                MeshFace face;face.texture=text(meshes);face.cull=read<int32_t>(meshes);face.tint=read<int32_t>(meshes);
                for(auto& v:face.vertex) {v.x=read<float>(meshes);v.y=read<float>(meshes);v.z=read<float>(meshes);v.u=read<float>(meshes);v.v=read<float>(meshes);}
                model.faces.push_back(std::move(face));
            }
            if(!blockNames.count(model.name)) {blockNames[model.name]=id;if(id<0xffff0000u && !model.faces.empty()) catalog.push_back(model.name);}
            models.emplace(id,std::move(model));
        }
        std::ifstream shapes(directory+"/shapes.bin",std::ios::binary);
        if(shapes) {
            header(shapes,"SCS1");unsigned states=read<uint32_t>(shapes);
            size_t realStates=std::count_if(models.begin(),models.end(),[](const auto& entry){return entry.first<0xffff0000u;});
            if(states!=realStates) throw std::runtime_error("collision registry mismatch");
            size_t bounds=0;
            for(unsigned i=0;i<states;i++) {
                unsigned id=read<uint32_t>(shapes),count=read<uint32_t>(shapes);bounds+=count;
                if(!models.count(id) || count>256 || bounds>1000000) throw std::runtime_error("collision shape budget");
                for(unsigned j=0;j<count;j++) {
                    CollisionBox box;box.minX=read<float>(shapes);box.minY=read<float>(shapes);box.minZ=read<float>(shapes);box.maxX=read<float>(shapes);box.maxY=read<float>(shapes);box.maxZ=read<float>(shapes);
                    if(!(box.minX>=-2 && box.minY>=-2 && box.minZ>=-2 && box.maxX<=3 && box.maxY<=3 && box.maxZ<=3 && box.minX<=box.maxX && box.minY<=box.maxY && box.minZ<=box.maxZ)) throw std::runtime_error("collision coordinates");
                    models.at(id).collision.push_back(box);
                }
            }
            collisionReady=true;
        }
        std::ifstream registry(directory+"/registry.tsv");std::string line;bool mappedCatalog=false;
        while(std::getline(registry,line)) {
            std::istringstream in(line);std::string kind,id,name;std::getline(in,kind,'\t');std::getline(in,id,'\t');std::getline(in,name,'\t');
            if(name.rfind("minecraft:",0)==0) name=name.substr(10);
            if(kind=="B") {
                std::string props,item,defaultState;std::getline(in,props,'\t');std::getline(in,item,'\t');std::getline(in,defaultState,'\t');
                if(!item.empty()) {
                    if(!mappedCatalog) {catalog.clear();mappedCatalog=true;}
                    if(item.rfind("minecraft:",0)==0) item=item.substr(10);
                    if(item!="air") catalog.push_back(item);
                    if(defaultState=="true") blockNames[name]=static_cast<unsigned>(std::stoul(id));
                }
            }
            if(kind=="I") {unsigned numeric=static_cast<unsigned>(std::stoul(id));items[numeric]=name;itemNames[name]=numeric;}
            if(kind=="E") entities[static_cast<unsigned>(std::stoul(id))]=name;
        }
        std::ifstream icons(directory+"/items.tsv");
        while(std::getline(icons,line)) {
            std::istringstream row(line);std::string id,path;std::getline(row,id,'\t');
            unsigned numeric=static_cast<unsigned>(std::stoul(id));if(!items.count(numeric)) throw std::runtime_error("item texture registry mismatch");
            while(std::getline(row,path,'\t')) {if(!textures.count(path) || itemTextures[numeric].size()>=16) throw std::runtime_error("item texture reference");itemTextures[numeric].push_back(path);}
        }
        std::sort(catalog.begin(),catalog.end());catalog.erase(std::unique(catalog.begin(),catalog.end()),catalog.end());
        std::fprintf(stderr,"[steve] assets loaded: %zu textures, %zu states, %zu items\n",textures.size(),models.size(),items.size());
        return true;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"[steve] assets refused: %s\n",e.what());textures.clear();models.clear();items.clear();entities.clear();blockNames.clear();itemNames.clear();catalog.clear();return false;
    }
}
const Texture* Assets::texture(const std::string& name) const {auto it=textures.find(name);return it==textures.end()?nullptr:&it->second;}
const BlockMesh* Assets::block(unsigned id) const {auto it=models.find(id);return it==models.end()?nullptr:&it->second;}
std::string Assets::item(unsigned id) const {auto it=items.find(id);return it==items.end()?"air":it->second;}
std::string Assets::entity(unsigned id) const {auto it=entities.find(id);return it==entities.end()?"unknown":it->second;}
unsigned Assets::defaultBlock(const std::string& name) const {auto it=blockNames.find(name);return it==blockNames.end()?0:it->second;}
unsigned Assets::itemId(const std::string& name) const {auto it=itemNames.find(name);return it==itemNames.end()?0:it->second;}
}

namespace steve {
const std::vector<std::string>& Assets::itemLayers(unsigned id) const {static const std::vector<std::string> empty;auto it=itemTextures.find(id);return it==itemTextures.end()?empty:it->second;}
}
