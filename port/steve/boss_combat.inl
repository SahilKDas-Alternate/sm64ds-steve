// Host-only combat. Recovered actor sources and the ROM reward bookkeeping stay intact.
// Suspend a struck boss's AI while the same KCL/placed-block world moves it;
// otherwise Bowser teleports home and Whomp rewinds every step near a ledge.
struct BossProfile {
    unsigned id; const char* name; float radius,height;
    unsigned model,collider,count;
};
const BossProfile bossProfiles[]={
    {279,"Bowser",240,340,0xd4,0x360,1},
    {189,"King Bob-omb",160,320,0x2cc,0x33c,2},
    {165,"Whomp King",180,360,0x2cc,0,0},
    {216,"Big Bully",140,240,0x110,0,0},
    {217,"Chill Bully",140,240,0x110,0,0},
    {218,"Chief Chilly",180,280,0x30c,0x110,1},
    {198,"Goomboss",230,400,0x210,0x110,4},
    {176,"Eyerok",200,280,0x360,0x320,1},
    {248,"Wiggler",160,260,0x110,0x478,10},
    {200,"Goomba",65,110,0x370,0,0},
    {201,"Small Goomba",35,60,0x370,0,0},
    {202,"Large Goomba",130,220,0x370,0,0},
    {203,"Koopa",65,140,0x300,0,0},
    {204,"Small Koopa",40,90,0x300,0,0},
    {210,"Big Boo",160,300,0x380,0x184,1}
};
struct BossMotion {
    Vec pos{},velocity{},safe{}; unsigned lastPunch{},age{},clipMask{},actor{}; bool flying{},rewarded{},hasSafe{};
    int health{},maximum{};
    signed char area{-1};
    unsigned lastStep{};
};
using BossBehavior=int (__fastcall *)(void*,void*);
std::map<void**,BossBehavior> bossBehaviors;
std::map<unsigned,BossMotion> bossMotions;
dActor_c* targetBoss{};
unsigned bossClock{},punchAnimation{};
unsigned combatReward{},combatDefeatClock{};
unsigned combatTarget{};
bool combatExitOpened{};
Cell combatExitCell{};
unsigned combatRewardActor{},combatRewardIndex{},combatKeyBefore{};
bool combatCollectionLogged{};
int bossLevel=-1;
const BossProfile* bossProfile(unsigned id) {
    for(const auto& p:bossProfiles) if(p.id==id)return &p;
    return nullptr;
}
const char* bossName(dActor_c* a) {return a->actorID==210 && port_level_id()==47?"King Boo":bossProfile(a->actorID)->name;}
Vec actorPosition(dActor_c* a) {return {a->mPosX/fix,a->mPosY/fix,a->mPosZ/fix};}
unsigned bossHealthOwner(dActor_c* a) {
    if(a->actorID==176 && static_cast<Eyerok*>(a)->mPartIdx!=0)
        for(auto* p=dActor_c::FindWithActorID(176,nullptr);p;p=dActor_c::FindWithActorID(176,p)) {
            auto* parent=static_cast<Eyerok*>(p);
            if(parent->mPartIdx==0 && (parent->mHandUniqueID1==int(a->uniqueID) || parent->mHandUniqueID2==int(a->uniqueID)))return p->uniqueID;
        }
    return a->uniqueID;
}
BossMotion& bossHealth(dActor_c* a) {
    auto& h=bossMotions[bossHealthOwner(a)];
    if(!h.maximum)h.health=h.maximum=a->actorID==279?2000:(a->actorID==165 || a->actorID==176 || (a->actorID==210 && port_level_id()==47))?1000:0;
    return h;
}
int bossDamage() {
    const auto& held=client.state.inventory[std::clamp(client.state.selected,0,8)];
    std::string item=held.count?assets.item(held.id):"";
    if(item=="netherite_sword")return 8;
    if(item=="diamond_sword")return 7;
    if(item=="iron_sword")return 6;
    if(item=="stone_sword")return 5;
    if(item=="wooden_sword" || item=="golden_sword")return 4;
    return 1;
}
void actorPosition(dActor_c* a,Vec p) {a->mPosX=int(p.x*fix);a->mPosY=int(p.y*fix);a->mPosZ=int(p.z*fix);}
extern "C" void ChangeArea(int);
void combatPlayerPosition(Player* p,Vec at,int area) {
    actorPosition(p,at);p->mPrevPosX=p->mPosX;p->mPrevPosY=p->mPosY;p->mPrevPosZ=p->mPosZ;
    p->mHorzSpeed=p->mVertSpeed=p->unk_0a4=p->unk_0ac=0;
    if(area>=0){p->mAreaId=area;if(data_02092120!=area)ChangeArea(area);}
    previousPosition=at;
}
bool liveBoss(dActor_c* a) {
    if(!a || !bossProfile(a->actorID) || a->aliveState==2 || a->shouldBeKilled)return false;
    if(a->actorID==216 && static_cast<BigBully*>(a)->mNumBulliesKilled<3)return false;
    if(a->actorID==210 && static_cast<daTrs_c*>(a)->unk_5cc==7)return false;
    return true;
}
dBgCh_Actr* bossMesh(dActor_c* a) {
    switch(a->actorID) {
    case 279:return &static_cast<Bowser*>(a)->mWithMeshClsn;
    case 189:return &static_cast<daBombking_c*>(a)->mWithMeshClsn;
    case 165:return &static_cast<Whomp*>(a)->mWithMeshClsn;
    case 216:return &static_cast<BigBully*>(a)->mWithMeshClsn;
    case 217:return &static_cast<daIDonketu_c*>(a)->mWithMeshClsn;
    case 218:return &static_cast<ChiefChilly*>(a)->mWithMeshClsn;
    case 198:return &static_cast<Goomboss*>(a)->mWithMeshClsn;
    case 248:return &static_cast<Wiggler*>(a)->mWithMeshClsn;
    case 200:case 201:case 202:return &static_cast<daKrb_c*>(a)->mWithMeshClsn;
    case 203:case 204:return &static_cast<Koopa*>(a)->mWithMeshClsn;
    case 210:return &static_cast<daTrs_c*>(a)->mWithMeshClsn;
    default:return nullptr;
    }
}
bool bossGround(Vec at,dActor_c* a,float& y) {
    bool hit=ground(at,a,y);
    // The server supplies actual multipart collision boxes, not a full-cube guess.
    Cell c=cell(at);
    for(int dy=0;dy>=-2;--dy) {
        Cell key{c.x,c.y+dy,c.z};auto* mesh=assets.block(block(key));if(!mesh)continue;
        if(terrain.count(key))continue;
        for(const auto& box:mesh->collision) {
            float x=at.x/scale-key.x,z=at.z/scale-key.z,top=(key.y+box.maxY)*scale;
            if(x>=box.minX && x<=box.maxX && z>=box.minZ && z<=box.maxZ && top<=at.y+.5f && (!hit || top>y)) {y=top;hit=true;}
        }
    }
    return hit;
}
bool bossVisible(dActor_c* a,Vec from,Vec to) {
    dBgCh_Lin ray;
    Vector3 start{int(from.x*fix),int(from.y*fix),int(from.z*fix)};
    Vector3 end{int(to.x*fix),int(to.y*fix),int(to.z*fix)};
    ray.SetObjAndLine(start,end,a);
    if(ray.DetectClsn() && ray.hasClsn && int(ray.clsnDist)/fix+5<std::sqrt((to.x-from.x)*(to.x-from.x)+(to.y-from.y)*(to.y-from.y)+(to.z-from.z)*(to.z-from.z))) return false;
    // Eyerok's own moving collider is excluded by the query's actor owner.
    return true;
}
void aimBoss() {
    targetBoss=nullptr;
    for(dActor_c* a=dActor_c::Next(nullptr);a;a=dActor_c::Next(a)) {
        if(!liveBoss(a) || (a->mAreaId>=0 && a->mAreaId!=static_cast<Player*>(local())->mAreaId))continue;
        if(a->actorID==176 && static_cast<Eyerok*>(a)->mPartIdx==0)continue;
        auto& state=bossMotions[a->uniqueID];if(state.rewarded)continue;
        const auto& profile=*bossProfile(a->actorID);Vec p=actorPosition(a);p.y+=profile.height*.5f;
        Vec v{p.x-eye.x,p.y-eye.y,p.z-eye.z};float t=v.x*direction.x+v.y*direction.y+v.z*direction.z;
        if(t<0 || t-profile.radius>std::min(400.f,targetDistance))continue;
        float dx=v.x-t*direction.x,dy=v.y-t*direction.y,dz=v.z-t*direction.z;
        if(dx*dx+dz*dz+dy*dy>profile.radius*profile.radius)continue;
        float front=std::max(0.f,t-profile.radius);
        Vec contact{eye.x+direction.x*front,eye.y+direction.y*front,eye.z+direction.z*front};
        if(!bossVisible(a,eye,contact))continue;
        targetDistance=front;targetBoss=a;targetEntity=-1;hasTarget=true;nativeTarget=false;
    }
}
void translateBoss(dActor_c* a,const BossProfile& p,Vec delta) {
    auto translate=[&](unsigned model) {
        auto* matrix=reinterpret_cast<int*>(reinterpret_cast<char*>(a)+model+0x1c);
        matrix[9]+=int(delta.x*fix)/8;matrix[10]+=int(delta.y*fix)/8;matrix[11]+=int(delta.z*fix)/8;
    };
    translate(p.model);
    if(p.id==176)translate(0x3d0);
    if(p.id==210)translate(0x3e4);
    if(p.id==248)for(unsigned i=1;i<5;i++)translate(p.model+i*0x64);
    for(unsigned i=0;i<p.count;i++) {
        // Wiggler's two contiguous arrays and Goomboss's four cylinders.
        auto* c=reinterpret_cast<dCcAcPos_c*>(reinterpret_cast<char*>(a)+p.collider+i*0x40);
        // Update links into the active collision list. Behavior may already
        // have linked this cylinder, so registering it again makes a cycle.
        c->Clear();Vector3 relative{0,0,0};c->SetPosRelativeToActor(relative);
    }
}
extern "C" void func_ov060_021135fc(char*);
void defeatBoss(dActor_c* a,BossMotion& m) {
    // Set the latch BEFORE spawning: a second punch or a second hand cannot
    // spawn another reward. The reward appears on the last safe platform.
    m.rewarded=true;m.flying=false;
    a->mAreaId=m.area;
    // A player can mine the last platform after the hit. Rewards must return
    // to remaining supported terrain, rather than falling through that hole.
    float rewardFloor;
    if(!bossGround({m.safe.x,m.safe.y+8,m.safe.z},a,rewardFloor) || std::abs(rewardFloor-m.safe.y)>160) {
        bool found=false;
        for(int radius=100;radius<=600 && !found;radius+=100)
            for(Vec offset:std::array<Vec,4>{{{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}}}) {
                Vec candidate{m.safe.x+offset.x*radius,m.safe.y+8,m.safe.z+offset.z*radius};
                if(bossGround(candidate,a,rewardFloor) && std::abs(rewardFloor-m.safe.y)<160){m.safe={candidate.x,rewardFloor,candidate.z};found=true;break;}
            }
    }
    Vec fallen=actorPosition(a);actorPosition(a,m.safe);
    Vector3 at{a->mPosX,a->mPosY+100*4096,a->mPosZ};
    dActor_c* reward=nullptr;
    switch(a->actorID) {
    case 279:func_ov060_021135fc(reinterpret_cast<char*>(a));reward=dActor_c::FindWithActorID(static_cast<Bowser*>(a)->mVariantID==2?283:282,nullptr);break;
    case 189:{auto* b=static_cast<daBombking_c*>(a);reward=a->UntrackAndSpawnStar(reinterpret_cast<s8&>(b->mStarTracked),b->mStarID,at,4);break;}
    case 165:{auto* b=static_cast<Whomp*>(a);reward=a->UntrackAndSpawnStar(reinterpret_cast<s8&>(b->unk_408),b->mStarID,at,4);break;}
    case 216:{auto* b=static_cast<BigBully*>(a);reward=a->UntrackAndSpawnStar(reinterpret_cast<s8&>(b->unk_3fd),b->mStarID|0x40,at,4);break;}
    case 217:{auto* b=static_cast<daIDonketu_c*>(a);reward=a->UntrackAndSpawnStar(b->mStarSlot,b->mStarIdx|0x40,at,4);break;}
    case 218:reward=dActor_c::Spawn(0x11a,4,at,nullptr,a->mAreaId,-1);break;
    case 198:reward=dActor_c::Spawn(0x11a,2,at,nullptr,a->mAreaId,-1);break;
    case 210:{
        if(port_level_id()==47)reward=dActor_c::Spawn(0x11a,3,at,nullptr,a->mAreaId,-1);
        else {
            // func_ov063_02118eac resolves the icon owner stored at +0x49c;
            // that actor, not the Boo, owns the tracked course star.
            unsigned owner=*reinterpret_cast<unsigned*>(reinterpret_cast<char*>(a)+0x49c);
            auto* icon=dActor_c::FindWithID(owner);
            if(icon && icon->actorID==211) {auto* b=static_cast<daTrsIcon_c*>(icon);reward=b->UntrackAndSpawnStar(b->mTrackStarID,b->mStarID,at,4);b->MarkForDestruction();}
        }
        break;
    }
    case 248:{auto* b=static_cast<Wiggler*>(a);reward=a->UntrackAndSpawnStar(reinterpret_cast<s8&>(b->unk_8e3),b->unk_8e2,at,4);break;}
    case 176:{
        // The parent owns both hands and the tracked star. Resolve it from
        // the hand IDs, never assume the hand shares the parent's star slot.
        auto* parent=static_cast<Eyerok*>(a);
        for(dActor_c* candidate=dActor_c::FindWithActorID(176,nullptr);candidate;candidate=dActor_c::FindWithActorID(176,candidate)) {
            auto* b=static_cast<Eyerok*>(candidate);
            if(b->mPartIdx==0 && (b->mHandUniqueID1==int(a->uniqueID) || b->mHandUniqueID2==int(a->uniqueID)))parent=b;
        }
        auto& parentMotion=bossMotions[parent->uniqueID];parentMotion.rewarded=true;
        reward=parent->UntrackAndSpawnStar(reinterpret_cast<s8&>(parent->mStarTracked),parent->mStarId,at,4);
        for(int id:{parent->mHandUniqueID1,parent->mHandUniqueID2})if(auto* hand=dActor_c::FindWithID(id)) {bossMotions[hand->uniqueID].rewarded=true;hand->MarkForDestruction();}
        if(parent!=a)parent->MarkForDestruction();break;
    }
    }
    if(std::getenv("SM64DS_STEVE_TEST_COMBAT") && reward){
        combatReward=reward->uniqueID;combatDefeatClock=bossClock;combatCollectionLogged=false;
        combatRewardActor=reward->actorID;combatRewardIndex=reward->param1&15;combatKeyBefore=unsigned(data_0209caa0[1]);
    }
    std::fprintf(stderr,"[steve-boss] defeat id=%u name=%s fall=%.1f reward=%u safe=(%.1f,%.1f,%.1f)\n",a->actorID,bossName(a),m.safe.y-fallen.y,reward?reward->actorID:0,m.safe.x,m.safe.y,m.safe.z);
    actorPosition(a,fallen);a->MarkForDestruction();
}
void stopBossAtWall(dActor_c* a,BossMotion& m,Vec previous) {
    const auto& p=*bossProfile(a->actorID);float radius=std::min(80.f,p.radius*.5f);
    Vec start{previous.x,previous.y+p.height*.5f,previous.z},end{m.pos.x,m.pos.y+p.height*.5f,m.pos.z};
    Vector3 from{int(start.x*fix),int(start.y*fix),int(start.z*fix)},to{int(end.x*fix),int(end.y*fix),int(end.z*fix)};
    dBgCh_Lin ray;ray.SetObjAndLine(from,to,a);
    bool hit=ray.DetectClsn() && ray.hasClsn;
    Cell low=cell({m.pos.x-radius,m.pos.y+10,m.pos.z-radius}),high=cell({m.pos.x+radius,m.pos.y+p.height,m.pos.z+radius});
    for(int y=low.y;y<=high.y && !hit;y++)for(int z=low.z;z<=high.z && !hit;z++)for(int x=low.x;x<=high.x && !hit;x++) {
        Cell c{x,y,z};if(terrain.count(c))continue;auto* mesh=assets.block(block(c));if(!mesh)continue;
        for(const auto& b:mesh->collision)if(m.pos.x+radius>(x+b.minX)*scale && m.pos.x-radius<(x+b.maxX)*scale && m.pos.y+p.height>(y+b.minY)*scale && m.pos.y+10<(y+b.maxY)*scale && m.pos.z+radius>(z+b.minZ)*scale && m.pos.z-radius<(z+b.maxZ)*scale) {hit=true;break;}
    }
    if(hit){m.pos.x=previous.x;m.pos.z=previous.z;m.velocity.x=m.velocity.z=0;}
}
int __fastcall bossBehavior(void* object,void*) {
    auto* a=static_cast<dActor_c*>(object);auto table=*reinterpret_cast<void***>(a);
    auto original=bossBehaviors.find(table);if(original==bossBehaviors.end())return 1;
    auto motion=bossMotions.find(a->uniqueID);
    if(!native_enabled() || !bossProfile(a->actorID))return original->second(a,nullptr);
    if(motion==bossMotions.end() || !motion->second.flying) {
        Vec before=actorPosition(a);int result=original->second(a,nullptr);
        // Reconcile downward crossings against the host KCL query. This also
        // prevents an initially two-units-below-floor placement tunnelling
        // under the arena before the first punch.
        if(liveBoss(a) && (a->actorID<200 || a->actorID>204 || !static_cast<dEnemyBase_c*>(a)->mDeathState)) {
            Vec after=actorPosition(a);float floor;
            if(after.y<before.y && bossGround({after.x,before.y+8,after.z},a,floor) && before.y>=floor-4 && after.y<floor) {
                float adjustment=floor-after.y;after.y=floor;actorPosition(a,after);a->mVertSpeed=0;
                translateBoss(a,*bossProfile(a->actorID),{0,adjustment,0});
                if(auto* mesh=bossMesh(a))mesh->SetGroundFlag();
            }
        }
        return result;
    }
    if(data_0209fc48 || data_0209f2c4) {
        if(std::getenv("SM64DS_STEVE_TEST_COMBAT") && bossClock%60==0)std::fprintf(stderr,"[steve-boss-test] paused id=%u script=%x pause=%u\n",a->actorID,data_0209fc48,data_0209f2c4);
        return 1;
    }
    auto& m=motion->second;Vec previous=m.pos;
    if(m.lastStep==bossClock)return 1;
    m.lastStep=bossClock;
    if(std::getenv("SM64DS_STEVE_TEST_COMBAT") && m.age%15==0)std::fprintf(stderr,"[steve-boss-test] physics id=%u age=%u y=%.1f\n",a->actorID,m.age,m.pos.y);
    m.velocity.y=std::max(-90.f,m.velocity.y-2.f);m.pos.x+=m.velocity.x;m.pos.y+=m.velocity.y;m.pos.z+=m.velocity.z;++m.age;
    stopBossAtWall(a,m,previous);
    float floor=0;bool supported=bossGround({m.pos.x,previous.y+2,m.pos.z},a,floor);
    if(m.velocity.y<0 && supported && previous.y>=floor-2 && m.pos.y<=floor) {
        m.pos.y=floor;
        if(floor<m.safe.y-160) {
            actorPosition(a,m.pos);translateBoss(a,*bossProfile(a->actorID),{m.pos.x-previous.x,m.pos.y-previous.y,m.pos.z-previous.z});
            defeatBoss(a,m);return 1;
        }
        m.flying=false;m.safe=m.pos;m.hasSafe=true;
        a->mAreaId=m.area;
        a->mFlags|=m.clipMask;
        a->mVertSpeed=a->mHorzSpeed=0;if(auto* mesh=bossMesh(a))mesh->SetGroundFlag();
        std::fprintf(stderr,"[steve-boss] landed id=%u frames=%u\n",a->actorID,m.age);
    }
    actorPosition(a,m.pos);translateBoss(a,*bossProfile(a->actorID),{m.pos.x-previous.x,m.pos.y-previous.y,m.pos.z-previous.z});
    if(m.pos.y<m.safe.y-600)defeatBoss(a,m);
    return 1;
}
bool punchBoss(dActor_c* a) {
    if(!liveBoss(a) || scripted(local()))return false;
    auto& m=bossMotions[a->uniqueID];if(m.rewarded || (m.lastPunch && bossClock-m.lastPunch<12))return false;
    Vec p=actorPosition(a),player=position(local());float dx=p.x-player.x,dz=p.z-player.z,length=std::hypot(dx,dz);
    if(length>bossProfile(a->actorID)->radius+400 || std::abs(p.y-player.y)>500)return false;
    static_cast<Player*>(local())->mAngleY=short(std::atan2(dx,dz)*65536.f/(2*pi));
    if(a->actorID>=200 && a->actorID<=204) {
        auto* enemy=static_cast<dEnemyBase_c*>(a);if(enemy->mDeathState)return false;
        Vector3_16 spin{0x800,0x1000,0};
        enemy->KillByInvincibleChar(spin,*static_cast<Player*>(local()),Fix12<int>{0});
        enemy->mFlags&=~0x10000u;
        m.lastPunch=bossClock;m.actor=a->actorID;punchAnimation=10;
        std::fprintf(stderr,"[steve-boss] native-enemy-hit id=%u death=%d\n",a->actorID,enemy->mDeathState);
        return true;
    }
    if(length<1){dx=direction.x;dz=direction.z;length=std::max(.01f,std::hypot(dx,dz));}
    if(!m.flying){
        if(!m.hasSafe) {
            float floor;
            if(bossGround({p.x,p.y+8,p.z},a,floor) && p.y-floor<600) {m.safe={p.x,floor,p.z};m.hasSafe=true;}
            else if(bossGround({player.x,player.y+8,player.z},static_cast<dActor_c*>(local()),floor)) {m.safe={player.x,floor,player.z};m.hasSafe=true;}
            else return false;
        }
        m.age=0;m.clipMask=a->mFlags&0x10001;m.area=a->mAreaId;
    }
    auto& health=bossHealth(a);
    if(health.maximum) {
        int damage=bossDamage();health.health=std::max(0,health.health-damage);
        std::fprintf(stderr,"[steve-boss] damage id=%u amount=%d hp=%d maximum=%d\n",a->actorID,damage,health.health,health.maximum);
        if(!health.health && m.hasSafe){m.lastPunch=bossClock;punchAnimation=10;defeatBoss(a,m);return true;}
    }
    // Thrown actors must keep falling outside the camera. Render culling
    // remains active; only the two behavior-culling bits are suspended.
    a->mFlags&=~0x10001u;
    a->mAreaId=-1; // A thrown actor keeps simulating after the player changes rooms.
    m.pos=p;m.velocity={dx/length*75,35,dz/length*75};m.lastPunch=bossClock;m.flying=true;punchAnimation=10;
    std::fprintf(stderr,"[steve-boss] punch id=%u name=%s pos=(%.1f,%.1f,%.1f) velocity=(%.1f,%.1f,%.1f)\n",a->actorID,bossName(a),p.x,p.y,p.z,m.velocity.x,m.velocity.y,m.velocity.z);
    return true;
}
void bossFrame() {
    ++bossClock;if(punchAnimation)--punchAnimation;
    int level=port_level_id();if(level!=bossLevel){bossMotions.clear();targetBoss=nullptr;bossLevel=level;combatReward=0;combatTarget=0;combatExitOpened=false;}
    std::set<unsigned> alive;
    for(dActor_c* a=dActor_c::Next(nullptr);a;a=dActor_c::Next(a)) {
        if(!liveBoss(a))continue;alive.insert(a->uniqueID);
        auto& motion=bossMotions[a->uniqueID];Vec at=actorPosition(a);float floor;
        if(!motion.flying && bossGround({at.x,at.y+8,at.z},a,floor) && std::abs(at.y-floor)<4) {motion.safe={at.x,floor,at.z};motion.hasSafe=true;}
        auto** table=*reinterpret_cast<void***>(a);
        if(table[6]!=reinterpret_cast<void*>(&bossBehavior)) {
            // Some factories install synthetic writable tables; reconstructed
            // C++ factories install compiler tables in .rdata. Protect exactly
            // this slot while seating the host hook, then restore protection.
            DWORD protection=0,ignored=0;
            if(VirtualProtect(table+6,sizeof(void*),PAGE_READWRITE,&protection)) {
                bossBehaviors[table]=reinterpret_cast<BossBehavior>(table[6]);table[6]=reinterpret_cast<void*>(&bossBehavior);
                VirtualProtect(table+6,sizeof(void*),protection,&ignored);
            }
        }
        // Kinematic throws belong to the host update, even when Nintendo's
        // BeforeBehavior room gate skips the actor. lastStep prevents the
        // seated Behavior callback from integrating the same frame twice.
        if(motion.flying)bossBehavior(a,nullptr);
    }
    for(auto it=bossMotions.begin();it!=bossMotions.end();)if(!alive.count(it->first)) {
        if(it->second.lastPunch && it->second.actor>=200 && it->second.actor<=204)std::fprintf(stderr,"[steve-boss-test] native-enemy-defeated id=%u uid=%u\n",it->second.actor,it->first);
        it=bossMotions.erase(it);
    }else ++it;
    // Headless fixture: position the PLAYER, then use the same reach/aim/punch
    // path as mouse input. Never move the enemy or manufacture a defeat.
    const char* test=std::getenv("SM64DS_STEVE_TEST_COMBAT");
    if(test && std::atoi(test)==210 && bossClock%60==0)
        for(auto* a=dActor_c::FindWithActorID(210,nullptr);a;a=dActor_c::FindWithActorID(210,a)) {
            auto* b=static_cast<daTrs_c*>(a);Vec p=actorPosition(a);
            auto* talk=*reinterpret_cast<Player**>(reinterpret_cast<char*>(a)+0x488);Vec t=talk?actorPosition(talk):Vec{};
            std::fprintf(stderr,"[steve-boss-test] Boo state=%u step=%u alpha=%u area=%d home=%d pos=(%.1f,%.1f,%.1f) talkLocal=%d talk=(%.1f,%.1f,%.1f)\n",b->unk_5cc,reinterpret_cast<unsigned char*>(a)[0x5ce],b->unk_5c8,a->mAreaId,reinterpret_cast<signed char*>(a)[0x5d0],p.x,p.y,p.z,talk==local(),t.x,t.y,t.z);
        }
    // Enter King Boo's real Mirror Luigi trigger before aiming at the boss;
    // his pre-cutscene actor is parked outside the playable room.
    if(test && std::atoi(test)==210 && level==47 && local() && bossClock>=40 && bossClock<600 && !scripted(local()))
        combatPlayerPosition(static_cast<Player*>(local()),{-150,0,0},-1);
    if(test && std::atoi(test)==176 && local() && bossClock==40) {
        for(auto* a=dActor_c::FindWithActorID(176,nullptr);a;a=dActor_c::FindWithActorID(176,a))if(static_cast<Eyerok*>(a)->mPartIdx==0) {
            Vec at=actorPosition(a);at.z-=200;combatPlayerPosition(static_cast<Player*>(local()),at,a->mAreaId);
            std::fprintf(stderr,"[steve-boss-test] enter Eyerok at=(%.1f,%.1f,%.1f)\n",at.x,at.y,at.z);
        }
    }
    if(test && local() && bossClock==40) {
        unsigned id=unsigned(std::atoi(test));
        if(id>=200 && id<=204 && !dActor_c::FindWithActorID(id,nullptr)) {
            auto* player=static_cast<Player*>(local());Vec at=position(player);float floor;
            if(bossGround({at.x,at.y+200,at.z+250},player,floor))at={at.x,floor,at.z+250};
            else {at={0,0,0};if(bossGround({0,2000,0},player,floor))at.y=floor;}
            Vector3 spawn{int(at.x*fix),int(at.y*fix),int(at.z*fix)};
            auto* enemy=dActor_c::Spawn(id,0,spawn,nullptr,player->mAreaId,-1);
            std::fprintf(stderr,"[steve-boss-test] native-fixture id=%u uid=%u\n",id,enemy?enemy->uniqueID:0);
        }
    }
    if(test && combatReward && !combatCollectionLogged) {
        auto* remaining=dActor_c::FindWithID(combatReward);
        bool collected=combatRewardActor==178 ? IsStarCollectedInCurLevel(combatRewardIndex)!=0 :
            combatRewardActor==283 ? _ZN5Event6GetBitEj(0x1d) && (!remaining || remaining->shouldBeKilled || remaining->aliveState==2) : unsigned(data_0209caa0[1])!=combatKeyBefore;
        if(collected){combatCollectionLogged=true;std::fprintf(stderr,"[steve-boss-test] reward-collected actor=%u index=%u save=%x before=%x event1d=%d\n",combatRewardActor,combatRewardIndex,data_0209caa0[1],combatKeyBefore,_ZN5Event6GetBitEj(0x1d));}
    }
    if(test && combatReward && local() && bossClock>combatDefeatClock+90 && !scripted(local())) {
        if(auto* reward=dActor_c::FindWithID(combatReward)) {
            bool ready=true;
            if(reward->actorID==282 || reward->actorID==283)ready=(static_cast<Key*>(reward)->mdCcAcPos_c.flags&1)==0;
            if(ready) {
                combatPlayerPosition(static_cast<Player*>(local()),actorPosition(reward),reward->mAreaId);
                static_cast<Player*>(local())->mVertSpeed=0;
                if((bossClock-combatDefeatClock)%30==0)std::fprintf(stderr,"[steve-boss-test] collect-contact reward=%u id=%u\n",combatReward,reward->actorID);
            } else if((bossClock-combatDefeatClock)%60==0)std::fprintf(stderr,"[steve-boss-test] reward-wait id=%u stage=%u y=%.1f\n",reward->actorID,static_cast<Key*>(reward)->unk_442,reward->mPosY/fix);
        } else {std::fprintf(stderr,"[steve-boss-test] reward-consumed uid=%u\n",combatReward);combatReward=0;}
    }
    if(test && local() && bossClock>=80 && bossClock%48==32 && !(std::atoi(test)==210 && level==47 && bossClock<400)) {
        unsigned id=unsigned(std::atoi(test));auto* a=combatTarget?dActor_c::FindWithID(combatTarget):dActor_c::FindWithActorID(id,nullptr);
        while(a && !combatTarget && (!liveBoss(a) || (id==176 && static_cast<Eyerok*>(a)->mPartIdx==0)))a=dActor_c::FindWithActorID(id,a);
        if(a && !combatTarget)combatTarget=a->uniqueID;
        if(a && liveBoss(a) && !bossMotions[a->uniqueID].flying) {
            auto* player=static_cast<Player*>(local());Vec p=actorPosition(a);Vec start{p.x,p.y,p.z-360};
            if(id==210 && std::getenv("SM64DS_STEVE_TEST_COMBAT_EXIT") && bossMotions[a->uniqueID].age && !combatExitOpened) {
                float floor;
                if(bossGround({p.x,p.y+8,p.z},a,floor)) {
                    combatExitCell=cell({p.x,floor,p.z});combatExitOpened=true;
                    for(int x=-1;x<=1;x++)for(int z=-1;z<=1;z++) {
                        Cell c{combatExitCell.x+x,combatExitCell.y,combatExitCell.z+z};
                        client.send("terrain\t"+coordinates(c)+"\tgrass_block");terrain.insert(c);
                        client.send("block\t"+coordinates(c)+"\tair");
                    }
                    std::fprintf(stderr,"[steve-boss-test] opening floor through IPC cell=(%d,%d,%d)\n",combatExitCell.x,combatExitCell.y,combatExitCell.z);
                    return;
                }
            }
            if(combatExitOpened && !holes.count(combatExitCell))return;
            if(combatExitOpened)std::fprintf(stderr,"[steve-boss-test] floor opening acknowledged\n");
            int area=a->mAreaId;
            if(id==210 && area<0)area=reinterpret_cast<signed char*>(a)[0x5d0];
            if(area>=0 && data_02092120!=area)ChangeArea(area);
            float groundY=0;bool support=bossGround({p.x,p.y+8,p.z},a,groundY);
            // Approach from real supported terrain, rather than parking the
            // player beyond a small arena's edge while the boss is airborne.
            for(Vec offset:std::array<Vec,4>{{{0,0,-360},{360,0,0},{-360,0,0},{0,0,360}}}) {
                Vec candidate{p.x+offset.x,p.y+8,p.z+offset.z};float floor;
                if(bossGround(candidate,a,floor) && std::abs(floor-p.y)<600) {start={candidate.x,floor,candidate.z};break;}
            }
            std::fprintf(stderr,"[steve-boss-test] support=%d floor=%.1f foot=%.1f player=%.1f\n",support,groundY,p.y,player->mPosY/fix);
            combatPlayerPosition(player,start,area);
            eye=start;eye.y+=162;direction={p.x-eye.x,(p.y+bossProfile(id)->height*.5f-eye.y),p.z-eye.z};
            float length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);direction.x/=length;direction.y/=length;direction.z/=length;
            targetDistance=500;targetEntity=-1;aimBoss();
            std::fprintf(stderr,"[steve-boss-test] aim expected=%u actual=%u script=%x control=%u area=%d targetArea=%d\n",id,targetBoss?targetBoss->actorID:0,data_0209fc48,player->mIsNoControl,player->mAreaId,a->mAreaId);
            if(targetBoss)punchBoss(targetBoss);
        }
    }
}
