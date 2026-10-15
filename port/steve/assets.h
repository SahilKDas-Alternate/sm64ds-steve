#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace steve {
struct Texture {
    unsigned w{},h{},frameHeight{};
    std::vector<uint32_t> pixels;
    std::vector<std::pair<unsigned,unsigned>> frames;
    unsigned frameOffset(uint64_t tick) const;
};
struct MeshVertex {float x{},y{},z{},u{},v{};};
struct MeshFace {std::string texture;int cull{-1},tint{-1};MeshVertex vertex[4];};
struct CollisionBox {float minX{},minY{},minZ{},maxX{},maxY{},maxZ{};};
struct BlockMesh {std::string name,properties;std::vector<MeshFace> faces;std::vector<CollisionBox> collision;};
class Assets {
public:
    bool load(const std::string& directory);
    bool collisionReady{};
    const Texture* texture(const std::string& name) const;
    const BlockMesh* block(unsigned id) const;
    std::string item(unsigned id) const;
    std::string entity(unsigned id) const;
    unsigned defaultBlock(const std::string& name) const;
    unsigned itemId(const std::string& name) const;
    const std::vector<std::string>& itemLayers(unsigned id) const;
    std::vector<std::string> catalog;
private:
    std::map<std::string,Texture> textures;
    std::map<unsigned,BlockMesh> models;
    std::map<unsigned,std::string> items,entities;
    std::map<unsigned,std::vector<std::string>> itemTextures;
    std::map<std::string,unsigned> blockNames,itemNames;
};
}
