#include "core.h"
#include <algorithm>
#include <queue>

namespace steve {
static constexpr int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
static int div16(int n) { int q = n / 16; return q - (n % 16 < 0); }
static Cell key(Cell p) { return {div16(p.x),div16(p.y),div16(p.z)}; }
static unsigned index(Cell p) {
    Cell k = key(p);
    return unsigned((p.y-k.y*16)*256 + (p.z-k.z*16)*16 + p.x-k.x*16);
}
static Cell adjacent(Cell p, unsigned d) {
    return {p.x+dirs[d][0],p.y+dirs[d][1],p.z+dirs[d][2]};
}
State World::get(Cell p) const {
    auto it = sections.find(key(p));
    return it == sections.end() ? State{} : it->second.cells[index(p)];
}
bool World::set(Cell p, State s) {
    // Bounded coordinates avoid overflow in section arithmetic and explosions.
    if (p.x < -32768 || p.x > 32767 || p.y < -32768 || p.y > 32767 ||
        p.z < -32768 || p.z > 32767 || s.power > 15) return false;
    Cell k = key(p);
    auto it = sections.find(k);
    if (it == sections.end()) {
        if (s.block == Block::Air) return true;
        if (sections.size() >= limit) return false;
        it = sections.emplace(k, Section{}).first;
    }
    State old = it->second.cells[index(p)];
    if (old.block == s.block && old.power == s.power) return true;
    it->second.cells[index(p)] = s;
    dirty.insert(k);
    // Neighbor sections must rebuild their boundary faces too.
    for (unsigned d=0; d<6; ++d) dirty.insert(key(adjacent(p,d)));
    if (s.block == Block::Air && std::all_of(it->second.cells.begin(),
        it->second.cells.end(), [](State a){return a.block == Block::Air;}))
        sections.erase(it);
    return true;
}
std::set<Cell> World::takeDirty() { std::set<Cell> out; out.swap(dirty); return out; }
std::vector<Face> World::mesh(Cell k) const {
    std::vector<Face> out;
    if(k.x < -2048 || k.x > 2047 || k.y < -2048 || k.y > 2047 ||
       k.z < -2048 || k.z > 2047) return out;
    auto it=sections.find(k);
    if (it==sections.end()) return out;
    for (int y=0;y<16;++y) for(int z=0;z<16;++z) for(int x=0;x<16;++x) {
        Cell p{k.x*16+x,k.y*16+y,k.z*16+z};
        Block b=get(p).block;
        if (b==Block::Air || b==Block::Wire) continue;
        for(unsigned d=0;d<6;++d) {
            Block other=get(adjacent(p,d)).block;
            if (other==Block::Air || other==Block::Wire ||
                (other==Block::Water && b!=Block::Water)) out.push_back({p,d,b});
        }
    }
    return out;
}
void World::redstone() {
    // Minimal same-height dust network; recompute from sources so power-off
    // cannot leave a self-sustaining loop. Full Java update ordering is pending.
    std::queue<Cell> frontier;
    for (auto& section : sections) {
        Cell k=section.first;
        for(unsigned i=0;i<4096;++i) {
            State& s=section.second.cells[i];
            if(s.block==Block::Wire) s.power=0;
            if(s.block==Block::Power) {
                s.power=15;
                frontier.push({k.x*16+int(i%16),k.y*16+int(i/256),k.z*16+int((i/16)%16)});
            }
        }
    }
    while(!frontier.empty()) {
        Cell p=frontier.front(); frontier.pop();
        State source=get(p);
        unsigned strength=source.block==Block::Power?15:source.power-1;
        if(!strength) continue;
        for(unsigned d : {0u,1u,4u,5u}) {
            Cell n=adjacent(p,d); State s=get(n);
            if(s.block==Block::Wire && s.power<strength) {
                s.power=uint8_t(strength); set(n,s); frontier.push(n);
            }
        }
    }
}
bool World::ignite(Cell p,unsigned fuse) {
    if(get(p).block!=Block::TNT) return false;
    set(p,{}); tnt.push_back({p,fuse}); return true;
}
void World::explode(Cell p) {
    // Prototype spherical blast. Minecraft ray attenuation and moving entities
    // require the block registry/physics layer, and are deliberately not claimed.
    for(int y=-3;y<=3;++y) for(int z=-3;z<=3;++z) for(int x=-3;x<=3;++x) {
        if(x*x+y*y+z*z>9) continue;
        Cell n{p.x+x,p.y+y,p.z+z};
        if(get(n).block==Block::TNT) ignite(n,10);
        else if(get(n).block!=Block::Water) set(n,{});
    }
}
void World::tick() {
    redstone();
    std::vector<Cell> powered;
    for(const auto& section:sections) {
        Cell k=section.first;
        for(unsigned i=0;i<4096;++i) if(section.second.cells[i].block==Block::TNT) {
            Cell p{k.x*16+int(i%16),k.y*16+int(i/256),k.z*16+int((i/16)%16)};
            for(unsigned d=0;d<6;++d) {
                State n=get(adjacent(p,d));
                if(n.block==Block::Power || (n.block==Block::Wire && n.power)) {powered.push_back(p); break;}
            }
        }
    }
    // Step only the entities present at tick start; chain-primed TNT starts
    // next tick. Avoid invalidating a vector reference during explosions.
    std::vector<PrimedTNT> current; current.swap(tnt);
    for(auto e:current) {
        if(e.fuse && --e.fuse) tnt.push_back(e);
        else explode(e.cell);
    }
    for(Cell p:powered) ignite(p);
}
unsigned Clock::advance(uint64_t dt) {
    // Cap catch-up to five ticks; debugger stalls must not freeze the game.
    remainder+=std::min<uint64_t>(dt,250000);
    unsigned ticks=unsigned(remainder/50000); remainder%=50000; return ticks;
}
unsigned Breath::tick(bool underwater) {
    if(!underwater) { air=300; damageClock=0; return 0; }
    if(air) { --air; return 0; }
    if(++damageClock==20) { damageClock=0; return 2; }
    return 0;
}
bool craft(const Recipe& r,std::array<Stack,9>& grid,unsigned size,
           Stack& out,unsigned maxStack) {
    if(size<2 || size>3 || !r.output.item || !r.output.count ||
       r.output.count>maxStack || out.count>maxStack ||
       (out.count && out.item!=r.output.item) ||
       r.output.count>maxStack-out.count) return false;
    std::array<bool,9> used{};
    bool matches=false;
    if(r.shapeless) {
        std::map<uint16_t,unsigned> needed, present;
        for(uint16_t id:r.ingredients) if(id) ++needed[id];
        for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) {
            unsigned i=y*3+x;
            if(grid[i].count) {
                if(!grid[i].item) return false;
                ++present[grid[i].item]; used[i]=true;
            }
        }
        matches=!needed.empty() && needed==present;
    } else {
        if(!r.width || !r.height || r.width>size || r.height>size) return false;
        for(unsigned oy=0;oy<=size-r.height && !matches;++oy)
        for(unsigned ox=0;ox<=size-r.width && !matches;++ox)
        for(unsigned mirror=0;mirror<2 && !matches;++mirror) {
            bool ok=true; std::array<bool,9> candidate{};
            for(unsigned y=0;y<size;++y) for(unsigned x=0;x<size;++x) {
                uint16_t id=0; unsigned i=y*3+x;
                if(x>=ox && x<ox+r.width && y>=oy && y<oy+r.height) {
                    unsigned rx=mirror?r.width-1-(x-ox):x-ox;
                    id=r.ingredients[(y-oy)*3+rx];
                }
                uint16_t actual=grid[i].count?grid[i].item:0;
                if(actual!=id || (grid[i].count && !grid[i].item)) ok=false;
                candidate[i]=id!=0;
            }
            if(ok) {matches=true; used=candidate;}
        }
        if(std::none_of(used.begin(),used.end(),[](bool a){return a;})) return false;
    }
    if(!matches) return false;
    // Validate completely before changing any slot or the output.
    for(unsigned i=0;i<9;++i) if(used[i]) {
        if(--grid[i].count==0) grid[i].item=0;
    }
    out.item=r.output.item; out.count+=r.output.count;
    return true;
}
}
