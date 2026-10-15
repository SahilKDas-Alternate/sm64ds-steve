#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "core.h"

namespace steve {
struct ItemState {unsigned id{},count{},damage{},maximum{};};
struct EntityState {int id{},type{},fuse{-1},block{},item{},count{}; float x{},y{},z{},yaw{},pitch{},health{};};
struct MovingState {Cell at;unsigned block;float x,y,z;};
struct BlockVisual {Cell at;std::string kind,text,patterns,styles;float progress{};unsigned color{},glow{};};
struct MenuSlot {int index,x,y;};
struct ServerState {
    uint64_t tick{},ack{};
    int token{-1},air{300},food{20},flags{},mining{-1},selected{};
    float x{},y{},z{},yaw{},pitch{},health{20};
    float pushedX{},pushedY{},pushedZ{};
    ItemState inventory[36]{},container[128]{},carried{};
    unsigned containerSize{},craftingWidth{2};
    std::vector<EntityState> entities;
    std::vector<MovingState> moving;
    std::vector<BlockVisual> visuals;
    std::vector<MenuSlot> menuSlots;
};
class Client {
public:
    Client();
    ~Client();
    void poll(const std::string& root);
    bool fresh() const;
    bool send(const std::string& verb);
    ServerState state;
    std::string diagnostic;
    uint64_t diagnosticUntil{};
    std::map<Cell,unsigned> blocks,terrainOriginal;
    std::vector<Cell> edits;
private:
    uintptr_t socket;
    bool started{},connecting{};
    uint64_t nextRetry{},lastPacket{},sequence{};
    std::vector<unsigned char> incoming,outgoing;
    size_t written{};
    void close();
    void message(const std::string& text);
    void packet(const std::string& text);
};
}
