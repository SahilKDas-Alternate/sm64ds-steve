#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

// Original host-side mechanics. No Nintendo or Minecraft assets are embedded.
namespace steve {
struct Cell {
    int x{}, y{}, z{};
    bool operator<(const Cell& b) const {
        if (x != b.x) return x < b.x;
        if (y != b.y) return y < b.y;
        return z < b.z;
    }
};
enum class Block : uint16_t { Air, Stone, Dirt, Water, Wire, Power, TNT };
struct State { Block block{Block::Air}; uint8_t power{}; };
struct Section { std::array<State, 4096> cells{}; };
struct Face { Cell cell; unsigned side; Block block; };
struct PrimedTNT { Cell cell; unsigned fuse; };
class World {
public:
    explicit World(std::size_t maxSections = 256) : limit(maxSections) {}
    State get(Cell p) const;
    bool set(Cell p, State state);
    void tick();
    bool ignite(Cell p, unsigned fuse = 80);
    std::vector<Face> mesh(Cell section) const;
    std::size_t sectionCount() const { return sections.size(); }
    const std::vector<PrimedTNT>& explosives() const { return tnt; }
    std::set<Cell> takeDirty();
private:
    std::size_t limit;
    std::map<Cell, Section> sections;
    std::set<Cell> dirty;
    std::vector<PrimedTNT> tnt;
    void redstone();
    void explode(Cell p);
};
// Feed simulation time only: a paused game supplies no elapsed time.
class Clock {
public:
    unsigned advance(uint64_t microseconds);
    double alpha() const { return remainder / 50000.0; }
private:
    uint64_t remainder{};
};
struct Breath {
    unsigned air{300}, damageClock{};
    unsigned tick(bool headUnderwater); // returned damage is in half-hearts
    void collectCoin() { air = 300; damageClock = 0; }
};
struct Stack { uint16_t item{}; unsigned count{}; };
// Both grids use row stride 3; a 2x2 grid uses slots 0,1,3,4.
struct Recipe {
    unsigned width{}, height{};
    std::array<uint16_t,9> ingredients{};
    Stack output;
    bool shapeless{};
};
bool craft(const Recipe& recipe, std::array<Stack,9>& grid, unsigned gridSize,
           Stack& output, unsigned maxStack = 64);
}
