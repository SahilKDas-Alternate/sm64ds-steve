#include "assets.h"
#include <cstdio>
#include <cstdlib>
int main() {
    steve::Assets assets;
    if(!assets.load("runtime/steve-assets") || !assets.collisionReady) return 1;
    for(const char* name:{"chest","red_bed","shield","trident","dragon_head"}) {
        auto* mesh=assets.block(assets.defaultBlock(name));
        if(!mesh || mesh->faces.size()<6 || mesh->faces[0].texture.find("entity/")!=0) return 2;
    }
    std::puts("Steve asset pack: real collision registry and special item/block meshes PASS");
}
