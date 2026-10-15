#include "core.h"
#include "geometry.h"
#include <cstdio>
#include <cstdlib>
using namespace steve;
static void check(bool pass,const char* message) {
    if(!pass) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main() {
    check(triangleCell({0,0,0},{2,0,0},{0,0,2},{.5f,0,.5f}),"floor triangle intersects occupied voxel");
    check(!triangleCell({0,0,0},{2,0,0},{0,0,2},{1.8f,0,1.8f},.1f),"triangle bounding-box corner is empty");
    check(!triangleCell({0,0,0},{2,0,0},{0,0,2},{.5f,2,.5f}),"separate vertical voxel is empty");
    check(triangleCell({0,0,0},{0,2,0},{0,0,2},{0,.5f,.5f}),"vertical wall triangle intersects voxel");
    auto contact=closestTriangle({.25f,2,.25f},{0,0,0},{1,0,0},{0,0,1});
    check(std::abs(contact.y)<1e-6f && std::abs(contact.x-.25f)<1e-6f,"sphere face contact projects onto triangle");
    contact=closestTriangle({2,1,2},{0,0,0},{1,0,0},{0,0,1});
    check(std::abs(contact.x-.5f)<1e-6f && std::abs(contact.z-.5f)<1e-6f,"sphere edge contact stays on triangle edge");
    contact=closestTriangle({-1,1,-1},{0,0,0},{1,0,0},{0,0,1});
    check(contact.x==0 && contact.y==0 && contact.z==0,"sphere vertex contact uses corner");
    Clock clock; unsigned count=0;
    for(int i=0;i<60;++i) count+=clock.advance(i%3==2?16666:16667);
    check(count==20,"60 host frames produce 20 ticks");
    check(clock.advance(1000000)==5,"catch-up is bounded");
    World w(2);
    check(w.set({-1,-1,-1},{Block::Stone}),"negative-coordinate placement");
    check(w.get({-1,-1,-1}).block==Block::Stone,"negative-coordinate lookup");
    check(w.mesh({-1,-1,-1}).size()==6,"isolated cube has six exposed faces");
    w.set({0,-1,-1},{Block::Stone});
    check(w.mesh({-1,-1,-1}).size()==5,"cross-section hidden face removed");
    check(!w.set({16,0,0},{Block::Stone}),"section memory limit enforced");
    w.set({-1,-1,-1},{}); w.set({0,-1,-1},{});
    check(w.sectionCount()==0,"empty sections reclaimed");
    World circuit;
    circuit.set({0,0,0},{Block::Power});
    for(int i=1;i<=16;++i) circuit.set({i,0,0},{Block::Wire});
    circuit.tick();
    check(circuit.get({1,0,0}).power==15 && circuit.get({15,0,0}).power==1 &&
          circuit.get({16,0,0}).power==0,"dust signal attenuates to zero");
    circuit.set({0,0,0},{}); circuit.tick();
    check(circuit.get({1,0,0}).power==0,"source removal clears signal");
    World blast;
    blast.set({0,0,0},{Block::TNT}); blast.set({2,0,0},{Block::TNT});
    blast.set({1,0,0},{Block::Stone}); blast.set({0,0,1},{Block::Water});
    blast.ignite({0,0,0},1); blast.tick();
    check(blast.get({1,0,0}).block==Block::Air,"TNT destroys nearby blocks");
    check(blast.get({0,0,1}).block==Block::Water,"prototype blast preserves water");
    check(blast.explosives().size()==1 && blast.explosives()[0].fuse==10,
          "chain reaction safely defers new TNT");
    Breath breath;
    for(int i=0;i<300;++i) check(breath.tick(true)==0,"15 seconds of air");
    unsigned damage=0; for(int i=0;i<20;++i) damage+=breath.tick(true);
    check(damage==2,"drowning causes a heart per second");
    breath.collectCoin(); check(breath.air==300,"coin restores air");
    Recipe recipe; recipe.width=1; recipe.height=2;
    recipe.ingredients[0]=1; recipe.ingredients[3]=1; recipe.output={2,4};
    std::array<Stack,9> grid{}; grid[1]={1,2}; grid[4]={1,2};
    Stack output{2,63};
    check(!craft(recipe,grid,2,output) && grid[1].count==2,
          "full output refuses crafting without consuming ingredients");
    output={}; check(craft(recipe,grid,2,output) && output.count==4 &&
                    grid[1].count==1,"offset shaped recipe consumes atomically");
    Recipe shapeless; shapeless.shapeless=true; shapeless.ingredients[0]=1;
    shapeless.ingredients[1]=3; shapeless.output={4,1};
    grid={}; grid[0]={3,1}; grid[8]={1,1}; output={};
    check(craft(shapeless,grid,3,output) && output.item==4,
          "shapeless ingredients match independent of order");
    grid={}; grid[0]={1,10}; output={};
    shapeless.ingredients[1]=1;
    check(!craft(shapeless,grid,3,output),"two ingredients require two occupied slots");
    std::puts("Steve core: clock, sections, mesh boundaries, dust, TNT, oxygen, crafting PASS");
}
