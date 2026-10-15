package steve;

import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

/** Authoritative vanilla 1.18 gameplay adapter. All game objects stay on the
 * Minecraft tick thread; socket threads exchange immutable messages only. */
public final class Backend {
    static Reflect r;
    static Object server, world, player, connection, inventory, menu, controlledVehicle;
    static Object savedChest;
    static Object blockRegistry, itemRegistry, entityRegistry;
    static long tick, ack, lastPose,notifications;
    static int levelToken=-1, selected, miningStage=-1, errors;
    static int oxygen=300,drowningClock,arenaX,arenaZ;
    static float originX, originY, originZ, yaw, pitch, nativeHealth=20;
    static boolean underwater, creative, ready, paused;
    static String lastError="";
    static long errorUntil;
    static double mineProgress;
    static Pos mining;
    static Pos building;
    static long launchTick;
    static int launchPhase;
    static boolean bomber;
    static double swimSide,swimUp,swimForward;
    static boolean swimming;
    static double pushedX,pushedY,pushedZ;
    static final ConcurrentLinkedQueue<String> input=new ConcurrentLinkedQueue<>();
    static final AtomicReference<byte[]> snapshot=new AtomicReference<>();
    static final AtomicBoolean resync=new AtomicBoolean();
    static final Map<Pos,Integer> cells=new LinkedHashMap<>();
    static final Set<Pos> forced=new LinkedHashSet<>();
    static final Map<Pos,Integer> terrainOriginal=new LinkedHashMap<>();
    static final Map<Pos,Float> terrainHeights=new HashMap<>();
    static final Map<Pos,Object> terrainShapes=new HashMap<>();
    public static Object terrainCollision(Object original,Object state,Object level,Object at) {
        if(!ready || level!=world) return original;
        try {
            Pos p=position(at);Object shape=terrainShapes.get(p);
            return shape!=null && terrainOriginal.getOrDefault(p,-1)==stateId(state)?shape:original;
        } catch(Exception e) {failure("terrain collision",e);return original;}
    }
    static void surface(Pos p,float height) throws Exception {
        if(!Float.isFinite(height) || height<=0 || height>1) throw new IllegalArgumentException("terrain surface height");
        if(!terrainOriginal.containsKey(p)) return;
        terrainHeights.put(p,height);
        terrainShapes.put(p,r.call(r.type("net.minecraft.world.phys.shapes.Shapes"),"box",0d,0d,0d,1d,(double)height,1d));
    }
    static final Set<Pos> dirty=new LinkedHashSet<>();
    static final Set<Pos> moving=new LinkedHashSet<>();
    static final Map<Pos,Long> movedTnt=new HashMap<>();
    // Preserve the duper's retained block during moving-piston finalization.
    // Priming on the next observer/coral update still runs vanilla TNT code.
    public static boolean tntPlacement(Object level,Object at,boolean pistonMove) {
        if(!ready || level!=world || !pistonMove) return false;
        try {if(movedTnt.size()<1024) movedTnt.put(position(at),tick+2);return true;}
        catch(Exception e) {failure("moving TNT",e);return false;}
    }
    public static boolean tntUpdate(Object level,Object at) {
        if(!ready || level!=world) return false;
        try {Pos p=position(at);Long until=movedTnt.get(p);if(until==null)return false;if(tick<=until)return true;movedTnt.remove(p);}
        catch(Exception e) {failure("TNT update",e);}
        return false;
    }
    static final Object signal=new Object();
    static final int MAX_CELLS=65536, MAX_ENTITIES=128;
    static record Pos(int x,int y,int z) {}

    public static boolean isLocalVehicle(Object entity) {return entity==controlledVehicle && entity!=null;}
    public static void pushed(Object entity,Object mover,Object vector) {
        if(!ready || entity!=player) return;
        try {if(mover==r.constant("net.minecraft.world.entity.MoverType","PISTON")) {
            pushedX+=((Number)r.get(vector,"x")).doubleValue();pushedY+=((Number)r.get(vector,"y")).doubleValue();pushedZ+=((Number)r.get(vector,"z")).doubleValue();
        }} catch(Exception e) {failure("piston displacement",e);}
    }
    public static boolean isLocalConnection(Object c) { return c==connection && connection!=null; }
    public static void spawn(Object level,Object data) {
        try {
            if(r==null) r=new Reflect(level.getClass().getClassLoader());
            r.call(data,"setSpawn",r.make("net.minecraft.core.BlockPos",0,64,0),0f);
        } catch(Throwable e) {throw new IllegalStateException("Cannot set bounded bridge spawn",e);}
    }
    public static void changed(Object level,Object pos,Object state) {
        if(!ready || level!=world) return;
        try {
            ++notifications;Pos p=position(pos);
            if(Math.abs((long)p.x)>29999984 || Math.abs(p.y)>2048 || Math.abs((long)p.z)>29999984) return;
            if(inArena(p) && (cells.containsKey(p) || cells.size()+dirty.size()<MAX_CELLS)) dirty.add(p);
        } catch(Exception e) {failure("block notification",e);}
    }
    static Pos position(Object p) throws Exception {
        return new Pos(((Number)r.call(p,"getX")).intValue(),((Number)r.call(p,"getY")).intValue(),((Number)r.call(p,"getZ")).intValue());
    }
    static Object pos(Pos p) throws Exception {return r.make("net.minecraft.core.BlockPos",p.x,p.y,p.z);}
    static Object state(Pos p) throws Exception {return r.call(world,"getBlockState",pos(p));}
    static void loadChunk(Pos p) throws Exception {
        Pos chunk=new Pos(p.x>>4,0,p.z>>4);
        if(!forced.contains(chunk)) {if(forced.size()>=128) throw new IllegalStateException("Active chunk budget");forced.add(chunk);r.call(world,"setChunkForced",chunk.x,chunk.z,false);r.call(world,"setChunkForced",chunk.x,chunk.z,true);}
        r.call(world,"getChunk",p.x>>4,p.z>>4);
    }
    static int stateId(Object s) throws Exception {return ((Number)r.call(r.type("net.minecraft.world.level.block.Block"),"getId",s)).intValue();}
    static Object item(String name) throws Exception {
        if(!name.matches("(?:minecraft:)?[a-z0-9_]+")) throw new IllegalArgumentException("Invalid item name");
        return r.call(itemRegistry,"get",r.make("net.minecraft.resources.ResourceLocation",name.contains(":")?name:"minecraft:"+name));
    }
    static Object stack(String name,int count) throws Exception {return r.make("net.minecraft.world.item.ItemStack",item(name),count);}
    static int command(String command) throws Exception {
        return ((Number)r.call(r.call(server,"getCommands"),"performCommand",r.call(r.call(server,"createCommandSourceStack"),"withSuppressedOutput"),command)).intValue();
    }
    static void initialize(Object s) throws Exception {
        server=s; if(r==null) r=new Reflect(s.getClass().getClassLoader());
        world=r.call(s,"overworld");
        // Release saved tickets before activating this run's bounded scene set.
        List<Long> oldForced=new ArrayList<>();
        for(Object chunk:(Iterable<?>)r.call(world,"getForcedChunks")) oldForced.add(((Number)chunk).longValue());
        for(long chunk:oldForced) r.call(world,"setChunkForced",(int)chunk,(int)(chunk>>32),false);
        if(!oldForced.isEmpty()) System.out.println("STEVE_FORCED_CHUNKS_CLEARED "+oldForced.size());
        loadChunk(new Pos(0,64,0));
        blockRegistry=r.constant("net.minecraft.core.Registry","BLOCK");
        itemRegistry=r.constant("net.minecraft.core.Registry","ITEM");
        entityRegistry=r.constant("net.minecraft.core.Registry","ENTITY_TYPE");
        Object profile=r.make("com.mojang.authlib.GameProfile",UUID.fromString("d55d73f8-098f-4cbf-a10f-4f972f9354ff"),"SteveBridge");
        player=r.make("net.minecraft.server.level.ServerPlayer",s,world,profile);
        connection=r.make("net.minecraft.network.Connection",r.constant("net.minecraft.network.protocol.PacketFlow","SERVERBOUND"));
        Object handler=r.make("net.minecraft.server.network.ServerGamePacketListenerImpl",s,connection,player);
        r.set(player,"connection",handler);
        r.call(player,"setNoGravity",true);
        r.call(player,"moveTo",0.5d,65d,0.5d,0f,0f);
        inventory=r.call(player,"getInventory");
        r.call(world,"addNewPlayer",player);
        // The world owns entity ticking; doTick updates player inventory/food.
        @SuppressWarnings("unchecked") List<Object> players=(List<Object>)r.call(r.call(s,"getPlayerList"),"getPlayers");
        players.add(player);
        r.call(player,"setGameMode",r.constant("net.minecraft.world.level.GameType","SURVIVAL"));
        command("gamerule doMobSpawning false");
        command("gamerule doDaylightCycle false");
        command("gamerule announceAdvancements false");
        command("gamerule sendCommandFeedback false");
        command("gamerule logAdminCommands false");
        command("gamerule keepInventory true");
        command("time set day");
        String[] starter={"diamond_pickaxe","diamond_sword","stone","tnt","redstone","sticky_piston","observer","slime_block","oak_boat"};
        for(int i=0;i<starter.length;i++) r.call(inventory,"setItem",i,stack(starter[i],i<2?1:64));
        for(int i=0;i<4;i++) r.call(inventory,"setItem",9+i,stack(new String[]{"repeater","redstone_torch","flint_and_steel","firework_rocket"}[i],i==2?1:64));
        r.call(inventory,"setItem",13,stack("arrow",64));r.call(inventory,"setItem",14,stack("apple",16));r.call(inventory,"setItem",15,stack("crafting_table",1));r.call(inventory,"setItem",16,stack("water_bucket",1));
        for(int i=0;i<4;i++) r.call(inventory,"setItem",17+i,stack(new String[]{"dead_brain_coral_fan","cobblestone_wall","detector_rail","minecart"}[i],i==3?1:64));
        loadPlayer();applyGameMode();r.call(player,"setNoGravity",true);
        exportRegistry();Shapes.export();
        startTransport();
        ready=true;
        System.out.println("STEVE_BRIDGE_READY Minecraft=1.18 IPC=1 ticks=20Hz");
    }
    static void failure(String context,Throwable e) {
        errors++; lastError=context+": "+e;errorUntil=tick+60;
        if(errors<=12) {System.err.println("[steve] "+lastError); e.printStackTrace();}
    }
    public static void tick(Object s) {
        try {
            if(!ready) initialize(s);
            ++tick;
            movedTnt.entrySet().removeIf(entry->entry.getValue()+20<tick);
            if(Files.exists(Path.of("budget-stop.txt"))) {System.err.println("[steve] Workspace budget reached; saving and stopping");request("1\t"+(ack+1)+"\tstop");return;}
            for(Pos p:new ArrayList<>(moving)) if(forced.size()<124) {loadChunk(p);loadChunk(offset(p,16,0,0));loadChunk(offset(p,-16,0,0));loadChunk(offset(p,0,0,16));loadChunk(offset(p,0,0,-16));}
            if(resync.getAndSet(false)) dirty.addAll(cells.keySet());
            for(int i=0;i<64;i++) {
                String packet=input.poll(); if(packet==null) break;
                try {request(packet);} catch(Exception e) {failure("request",e);}
            }
            if(building!=null && tick>=launchTick) launchMachine();
            boolean stale=System.nanoTime()-lastPose>2_000_000_000L;
            r.call(player,"setInvulnerable",stale || paused);
            if(stale || paused) {if((Boolean)r.call(player,"isFallFlying")) System.out.println("STEVE_FLIGHT_STOP stale="+stale+" paused="+paused);mining=null;swimSide=swimUp=swimForward=0; r.call(player,"stopUsingItem");r.call(player,"stopFallFlying");restoreChest();r.call(player,"setNoGravity",true);Object vehicle=r.call(player,"getVehicle");if(vehicle!=null && r.type("net.minecraft.world.entity.vehicle.Boat").isInstance(vehicle)) r.call(vehicle,"setInput",false,false,false,false);}
            if(mining!=null && !paused) mine();
            if(!stale && !paused) {
                r.call(player,"doTick");
                if((Boolean)r.call(player,"isFallFlying")) {r.call(player,"setNoGravity",false);r.call(player,"travel",r.make("net.minecraft.world.phys.Vec3",0d,0d,0d));}
                else if(swimming) {
                    r.call(player,"setSwimming",true);r.call(player,"setPose",r.constant("net.minecraft.world.entity.Pose","SWIMMING"));r.call(player,"setNoGravity",false);
                    if(swimUp>0) r.call(player,"jumpInLiquid",r.constant("net.minecraft.tags.FluidTags","WATER"));
                    if(swimUp<0) r.call(player,"setDeltaMovement",r.call(r.call(player,"getDeltaMovement"),"add",0d,-.04d,0d));
                    r.call(player,"travel",r.make("net.minecraft.world.phys.Vec3",swimSide,swimUp,swimForward));
                }
                else {r.call(player,"setSwimming",false);r.call(player,"setNoGravity",true);}
                controlledVehicle=r.call(player,"getVehicle");
                if(controlledVehicle!=null && r.type("net.minecraft.world.entity.vehicle.Boat").isInstance(controlledVehicle)) r.call(controlledVehicle,"controlBoat");
            } else controlledVehicle=null;
            if(underwater && !paused && !stale) {
                if(creative || (Boolean)r.call(player,"canBreatheUnderwater") || (Boolean)r.call(player,"hasEffect",r.constant("net.minecraft.world.effect.MobEffects","WATER_BREATHING"))) {oxygen=300;drowningClock=0;}
                else if((Boolean)r.call(player,"isEyeInFluid",r.constant("net.minecraft.tags.FluidTags","WATER"))) {oxygen=((Number)r.call(player,"getAirSupply")).intValue();drowningClock=0;}
                else if(oxygen>0) oxygen=((Number)r.call(player,"decreaseAirSupply",oxygen)).intValue();
                else if(++drowningClock==20) {drowningClock=0;r.call(player,"hurt",r.constant("net.minecraft.world.damagesource.DamageSource","DROWN"),2f);}
            } else if(!underwater) {oxygen=300;drowningClock=0;}
            r.call(player,"setAirSupply",oxygen);
            if(savedChest!=null && !(Boolean)r.call(player,"isFallFlying")) restoreChest();
            if(tick%100==0) savePlayer();
            publish();
        } catch(Throwable e) {failure("tick",e);}
    }
    static int integer(String[] p,int i) {return Integer.parseInt(p[i]);}
    static void applyGameMode() throws Exception {
        Object mode=r.constant("net.minecraft.world.level.GameType",creative?"CREATIVE":"SURVIVAL");
        r.call(player,"setGameMode",mode);
        // Player NBT restores abilities independently of ServerPlayerGameMode.
        // Even an unchanged game type must reconcile those saved abilities.
        r.call(mode,"updatePlayerAbilities",r.call(player,"getAbilities"));r.call(player,"onUpdateAbilities");
    }
    static void restoreChest() throws Exception {
        if(savedChest!=null) {r.call(player,"setItemSlot",r.constant("net.minecraft.world.entity.EquipmentSlot","CHEST"),savedChest);savedChest=null;}
    }
    static double number(String[] p,int i) {
        double v=Double.parseDouble(p[i]); if(!Double.isFinite(v) || Math.abs(v)>32768) throw new IllegalArgumentException("coordinate range"); return v;
    }
    static Pos coordinates(String[] p,int i) {
        Pos v=new Pos(integer(p,i),integer(p,i+1),integer(p,i+2));
        if(Math.abs((long)v.x)>32768 || Math.abs((long)v.y)>2048 || Math.abs((long)v.z)>32768) throw new IllegalArgumentException("block coordinate range");
        return new Pos(v.x+arenaX,v.y,v.z+arenaZ);
    }
    static void request(String packet) throws Exception {
        String[] p=packet.split("\\t",-1);
        if(p.length<3 || !p[0].equals("1")) throw new IllegalArgumentException("IPC version");
        long sequence=Long.parseLong(p[1]); if(sequence<=ack) return;
        String verb=p[2];
        switch(verb) {
        case "pose": {
            if(p.length!=13) throw new IllegalArgumentException("pose size");
            int token=integer(p,3);
            if(token<0 || token>=1024) throw new IllegalArgumentException("scene token");
            if(levelToken!=token) {
                for(Pos chunk:forced) r.call(world,"setChunkForced",chunk.x,chunk.z,false);forced.clear();
                if(building!=null) {place(offset(building,-2,0,0),"air");place(offset(building,3,0,1),"air");place(offset(building,-1,2,0),"air");if(bomber) {place(offset(building,2,2,2),"air");place(offset(building,2,1,2),"air");}building=null;}
                levelToken=token;arenaX=(token%32)*65536;arenaZ=(token/32)*65536;swimming=false;pushedX=pushedY=pushedZ=0;movedTnt.clear();
                r.call(player,"setSwimming",false);r.call(player,"setPose",r.constant("net.minecraft.world.entity.Pose","STANDING"));r.call(player,"setDeltaMovement",r.constant("net.minecraft.world.phys.Vec3","ZERO"));
                mining=null;menu=null;r.call(player,"closeContainer");r.call(player,"stopRiding");r.call(player,"stopFallFlying");restoreChest();
                snapshot.set(null);dirty.clear();moving.clear();
                for(Pos at:cells.keySet()) if(inArena(at)) {loadChunk(at);dirty.add(at);}
            }
            double x=number(p,4)+arenaX,y=number(p,5),z=number(p,6)+arenaZ;
            yaw=(float)number(p,7); pitch=(float)number(p,8);
            underwater=integer(p,9)!=0; paused=integer(p,10)!=0;
            float damage=(float)number(p,11); boolean grounded=integer(p,12)!=0;
            if(!(Boolean)r.call(player,"isPassenger") && !(Boolean)r.call(player,"isFallFlying") && !swimming) r.call(player,"setPos",x,y,z); r.call(player,"setYRot",yaw); r.call(player,"setXRot",pitch);
            r.set(player,"onGround",grounded && !(Boolean)r.call(player,"isFallFlying"));
            r.call(r.call(world,"getChunkSource"),"move",player);
            if(damage>0 && damage<=20) r.call(player,"hurt",r.constant("net.minecraft.world.damagesource.DamageSource","GENERIC"),damage);
            lastPose=System.nanoTime(); break;
        }
        case "block": {
            Pos at=coordinates(p,3); String name=p[6];
            if(!name.matches("(?:minecraft:)?[a-z0-9_]+(?:\\[[a-z0-9_=,]+\\])?")) throw new IllegalArgumentException("block identifier");
            loadChunk(at);command("setblock "+at.x+" "+at.y+" "+at.z+" "+name+" replace");dirty.add(at); break;
        }
        case "terrain": {
            Pos at=coordinates(p,3); String name=p[6];
            if(!Set.of("stone","dirt","grass_block","sand","snow_block","bedrock","water","air").contains(name)) throw new IllegalArgumentException("terrain material");
            loadChunk(at);
            // A delayed native mesh import must never overwrite Minecraft edits.
            // Existing originals are immutable, including cells mined back to air.
            if(terrainOriginal.containsKey(at) || cells.containsKey(at) || !(Boolean)r.call(state(at),"isAir")) {dirty.add(at);break;}
            command("setblock "+at.x+" "+at.y+" "+at.z+" minecraft:"+name+" replace");terrainOriginal.put(at,stateId(state(at)));dirty.add(at); break;
        }
        case "surface": {surface(coordinates(p,3),(float)number(p,6));break;}
        case "mine": {Pos at=coordinates(p,3); if(!at.equals(mining)) {mining=at; mineProgress=0;} lastPose=System.nanoTime(); break;}
        case "cancel": {mining=null; miningStage=-1; r.call(player,"stopUsingItem"); break;}
        case "slot": {selected=integer(p,3); if(selected<0 || selected>8) throw new IllegalArgumentException("hotbar slot"); r.set(inventory,"selected",selected); break;}
        case "give": {int count=integer(p,4); if(count<1 || count>64) throw new IllegalArgumentException("stack count"); r.call(inventory,"setItem",selected,stack(p[3],count)); break;}
        case "creative": {creative=integer(p,3)!=0;applyGameMode();break;}
        case "use": {
            Pos at=coordinates(p,3);
            Object direction=r.constant("net.minecraft.core.Direction",p[6].toUpperCase(Locale.ROOT));
            Object hit=r.make("net.minecraft.world.phys.BlockHitResult",r.make("net.minecraft.world.phys.Vec3",at.x+0.5d,at.y+0.5d,at.z+0.5d),direction,pos(at),false);
            Object result=r.call(r.get(player,"gameMode"),"useItemOn",player,world,r.call(player,"getMainHandItem"),r.constant("net.minecraft.world.InteractionHand","MAIN_HAND"),hit);
            if(!(Boolean)r.call(result,"consumesAction")) r.call(r.get(player,"gameMode"),"useItem",player,world,r.call(player,"getMainHandItem"),r.constant("net.minecraft.world.InteractionHand","MAIN_HAND"));
            Object opened=r.get(player,"containerMenu");
            if(opened!=r.get(player,"inventoryMenu")) menu=opened; break;
        }
        case "item": {r.call(r.get(player,"gameMode"),"useItem",player,world,r.call(player,"getMainHandItem"),r.constant("net.minecraft.world.InteractionHand","MAIN_HAND")); break;}
        case "release": {r.call(player,"releaseUsingItem");break;}
        case "spawn": {
            if(!Set.of("zombie","skeleton","creeper","spider","pig","cow","sheep","chicken","boat").contains(p[3])) throw new IllegalArgumentException("entity type");
            if(countEntities()>=MAX_ENTITIES) throw new IllegalStateException("entity budget");
            loadChunk(new Pos((int)number(p,4)+arenaX,(int)number(p,5),(int)number(p,6)+arenaZ));
            command("summon minecraft:"+p[3]+" "+(number(p,4)+arenaX)+" "+number(p,5)+" "+(number(p,6)+arenaZ)); break;
        }
        case "attack": {Object e=r.call(world,"getEntity",integer(p,3)); if(e!=null) r.call(player,"attack",e); break;}
        case "interact": {Object e=r.call(world,"getEntity",integer(p,3));if(e!=null && ((Number)r.call(player,"distanceToSqr",e)).doubleValue()<=36) r.call(player,"interactOn",e,r.constant("net.minecraft.world.InteractionHand","MAIN_HAND"));break;}
        case "ride": {Object e=r.call(world,"getEntity",integer(p,3)); if(e!=null) {r.call(player,"startRiding",e,true);if(r.type("net.minecraft.world.entity.vehicle.Boat").isInstance(e)) r.call(e,"setYRot",yaw);} break;}
        case "dismount": {r.call(player,"stopRiding");r.call(player,"setDeltaMovement",r.constant("net.minecraft.world.phys.Vec3","ZERO"));break;}
        case "boat": {
            Object boat=r.call(player,"getVehicle");
            if(boat!=null && r.type("net.minecraft.world.entity.vehicle.Boat").isInstance(boat)) {
                r.call(boat,"setInput",integer(p,3)!=0,integer(p,4)!=0,integer(p,5)!=0,integer(p,6)!=0);
            } break;
        }
        case "wing": {
            if(p.length>3 && integer(p,3)==0) {r.call(player,"stopFallFlying");r.call(player,"setNoGravity",true);r.call(player,"setDeltaMovement",r.constant("net.minecraft.world.phys.Vec3","ZERO"));restoreChest();break;}
            Object chest=r.constant("net.minecraft.world.entity.EquipmentSlot","CHEST");
            Object wearing=r.call(player,"getItemBySlot",chest);
            if(!r.call(wearing,"getItem").equals(item("elytra"))) {savedChest=r.call(wearing,"copy");r.call(player,"setItemSlot",chest,stack("elytra",1));}
            boolean started=(Boolean)r.call(player,"tryToStartFallFlying");System.out.println("STEVE_FLIGHT_START success="+started+" ground="+r.get(player,"onGround")+" water="+r.call(player,"isInWater")+" y="+r.call(player,"getY"));
            if(started) {
                Object look=r.call(player,"getLookAngle");r.call(player,"setDeltaMovement",r.call(look,"scale",0.8d));
            } break;
        }
        case "boost": {
            Object rocket=r.call(player,"getOffhandItem");
            if(!r.call(rocket,"getItem").equals(item("firework_rocket"))) throw new IllegalStateException("Place fireworks in your offhand to boost");
            r.call(r.get(player,"gameMode"),"useItem",player,world,rocket,r.constant("net.minecraft.world.InteractionHand","OFF_HAND")); break;
        }
        case "travel": {
            r.call(player,"travel",r.make("net.minecraft.world.phys.Vec3",number(p,3),number(p,4),number(p,5))); break;
        }
        case "swim": {
            swimming=integer(p,3)!=0;swimSide=Math.max(-1,Math.min(1,number(p,4)));swimUp=Math.max(-1,Math.min(1,number(p,5)));swimForward=Math.max(-1,Math.min(1,number(p,6)));break;
        }
        case "machine": {buildMachine(coordinates(p,3),integer(p,6)!=0);break;}
        case "air": {oxygen=300;drowningClock=0;r.call(player,"setAirSupply",300); break;}
        case "heal": {float amount=(float)number(p,3);if(amount>0 && amount<=20) r.call(player,"heal",amount);break;}
        case "hurt": {float amount=(float)number(p,3);if(amount>0 && amount<=20) r.call(player,"hurt",r.constant("net.minecraft.world.damagesource.DamageSource","GENERIC"),amount);break;}
        case "inventory": {
            if(integer(p,3)!=0) {
                r.call(player,"closeContainer");
                Object access=r.constant("net.minecraft.world.inventory.ContainerLevelAccess","NULL");
                menu=p.length>4 && integer(p,4)==3 ? r.make("net.minecraft.world.inventory.CraftingMenu",1,inventory,access) : r.get(player,"inventoryMenu");
                r.set(player,"containerMenu",menu);
            } else {r.call(player,"closeContainer"); menu=null;}
            break;
        }
        case "click": {
            if(menu==null) throw new IllegalStateException("inventory is closed");
            int slot=integer(p,3),button=integer(p,4);
            if(slot<0 || slot>=((List<?>)r.get(menu,"slots")).size() || button<0 || button>1) throw new IllegalArgumentException("inventory click");
            r.call(menu,"clicked",slot,button,r.constant("net.minecraft.world.inventory.ClickType","PICKUP"),player); break;
        }
        case "decorate": {Visuals.decorate(coordinates(p,3),p[6]);break;}
        case "update": {
            Pos at=coordinates(p,3);Object current=state(at);
            r.call(world,"updateNeighborsAt",pos(at),r.call(current,"getBlock"));
            r.call(current,"updateNeighbourShapes",world,pos(at),3);
            break;
        }
        case "diagnose": {
            Pos at=coordinates(p,3);Object border=r.call(world,"getWorldBorder");
            long chunk=((long)(at.x>>4)&0xffffffffL)|(((long)(at.z>>4)&0xffffffffL)<<32);
            System.out.println("STEVE_DIAG at="+at+" ticking="+r.call(world,"shouldTickBlocksAt",chunk)+" withinBorder="+r.call(border,"isWithinBounds",pos(at))+" gameTime="+r.call(world,"getGameTime")+" forced="+forced.size()+" notifications="+notifications+" state="+state(at)+" neighborSignal="+r.call(world,"hasNeighborSignal",pos(at))+" debug="+r.call(world,"isDebug")+" pendingEvents="+r.get(world,"blockEvents"));
            Object current=state(at),block=r.call(current,"getBlock");
            if(r.type("net.minecraft.world.level.block.piston.PistonBaseBlock").isInstance(block)) {
                Object direction=r.call(current,"getValue",r.constant("net.minecraft.world.level.block.DirectionalBlock","FACING"));
                Object resolver=r.make("net.minecraft.world.level.block.piston.PistonStructureResolver",world,pos(at),direction,true);
                System.out.println("STEVE_PISTON signal="+r.call(block,"getNeighborSignal",world,pos(at),direction)+" canMove="+r.call(resolver,"resolve")+" pushes="+r.call(resolver,"getToPush"));
            }
            break;
        }
        case "respawn": {
            if(value(player,"getHealth")>0) break;
            player=r.call(r.call(server,"getPlayerList"),"respawn",player,false);
            inventory=r.call(player,"getInventory");menu=null;controlledVehicle=null;
            r.call(player,"setNoGravity",true);r.call(player,"setPos",number(p,3)+arenaX,number(p,4),number(p,5)+arenaZ);
            oxygen=300;drowningClock=0;break;
        }
        case "sync": {for(Pos at:cells.keySet()) if(inArena(at)) dirty.add(at);break;}
        case "save": {savePlayer(); command("save-all flush"); break;}
        case "stop": {
            savePlayer();
            @SuppressWarnings("unchecked") List<Object> players=(List<Object>)r.call(r.call(server,"getPlayerList"),"getPlayers");
            players.remove(player); // The IPC proxy has no Netty channel to disconnect.
            r.call(player,"discard");
            command("save-all flush");
            r.call(server,"halt",false); break;
        }
        default: throw new IllegalArgumentException("Unknown IPC verb: "+verb);
        }
        ack=sequence;
    }
    static void mine() throws Exception {
        Object state=state(mining);
        if((Boolean)r.call(state,"isAir")) {mining=null; miningStage=-1; return;}
        if(creative) mineProgress=1;
        else mineProgress+=((Number)r.call(state,"getDestroyProgress",player,world,pos(mining))).doubleValue();
        miningStage=Math.min(9,(int)(mineProgress*10));
        if(mineProgress>=1) {
            r.call(r.get(player,"gameMode"),"destroyBlock",pos(mining));
            dirty.add(mining); mining=null; miningStage=-1;
        }
    }
    static boolean inArena(Pos p) {return Math.abs((long)p.x-arenaX)<32768 && Math.abs((long)p.z-arenaZ)<32768;}
    static Pos offset(Pos p,int x,int y,int z) {return new Pos(p.x+x,p.y+y,p.z+z);}
    static void place(Pos p,String name) throws Exception {
        loadChunk(p);command("setblock "+p.x+" "+p.y+" "+p.z+" minecraft:"+name+" replace");dirty.add(p);
    }
    static void buildMachine(Pos p,boolean payload) throws Exception {
        if(building!=null) throw new IllegalStateException("A flying machine is still being assembled");
        if(payload && countEntities()>=MAX_ENTITIES) throw new IllegalStateException("Bomber minecart exceeds entity budget");
        if(p.y<1 || p.y>250 || cells.size()+dirty.size()+32>MAX_CELLS) throw new IllegalStateException("Flying machine exceeds world budget");
        for(int x=-2;x<=3;x++) for(int y=-1;y<=2;y++) for(int z=0;z<=(payload?2:1);z++) {
            Pos at=offset(p,x,y,z);loadChunk(at);
            if(!(Boolean)r.call(state(at),"isAir")) throw new IllegalStateException("Flying machine needs six by four by "+(payload?"three":"two")+" clear blocks");
        }
        Map<String,Integer> cost=new LinkedHashMap<>();cost.put("slime_block",payload?8:6);cost.put("sticky_piston",2);cost.put("observer",2);
        if(payload) {cost.put("tnt",1);cost.put("dead_brain_coral_fan",1);cost.put("cobblestone_wall",1);cost.put("detector_rail",1);cost.put("minecart",1);}
        int[] remove=new int[36];
        if(!creative) {
            for(var entry:cost.entrySet()) {
                Object wanted=item(entry.getKey());int remaining=entry.getValue();
                for(int i=0;i<36 && remaining>0;i++) {Object stack=r.call(inventory,"getItem",i);if(r.call(stack,"getItem").equals(wanted)) {int n=Math.min(remaining,((Number)r.call(stack,"getCount")).intValue());remove[i]=n;remaining-=n;}}
                if(remaining>0) throw new IllegalStateException("Flying machine requires "+entry.getValue()+" "+entry.getKey());
            }
            for(int i=0;i<36;i++) if(remove[i]>0) r.call(r.call(inventory,"getItem",i),"shrink",remove[i]);
        }
        // Stops prevent startup observer pulses from dismantling an unfinished
        // engine. They are removed after vanilla scheduled ticks settle.
        place(offset(p,-2,0,0),"obsidian");place(offset(p,3,0,1),"obsidian");
        place(p,"sticky_piston[facing=east]");place(offset(p,1,0,0),"slime_block");place(offset(p,0,0,1),"slime_block");
        place(offset(p,1,0,1),"sticky_piston[facing=west]");
        for(int[] v:new int[][]{{-1,0,1},{-1,0,0},{2,0,0},{2,0,1}}) place(offset(p,v[0],v[1],v[2]),"slime_block");
        place(offset(p,payload?1:2,1,payload?0:1),"observer[facing=up]");place(offset(p,-1,1,0),"observer[facing=up]");
        if(payload) {place(offset(p,2,-1,1),"slime_block");place(offset(p,3,-1,1),"slime_block");}
        building=p;bomber=payload;launchPhase=0;launchTick=tick+20;
    }
    static void launchMachine() throws Exception {
        Pos p=building;if(p==null)return;
        if(launchPhase==0) {
            long chunk=((long)(p.x>>4)&0xffffffffL)|(((long)(p.z>>4)&0xffffffffL)<<32);
            Object ticks=r.call(world,"getBlockTicks");
            Pos head=offset(p,bomber?1:2,1,bomber?0:1);
            Object observer=r.call(state(head),"getBlock");
            if(!(Boolean)r.call(world,"shouldTickBlocksAt",chunk) || (Boolean)r.call(ticks,"hasScheduledTick",pos(head),observer) || (Boolean)r.call(ticks,"hasScheduledTick",pos(offset(p,-1,1,0)),observer)) {launchTick=tick+1;return;}
        }
        if(bomber) {
            if(launchPhase==0) {
                place(offset(p,2,-1,0),"tnt");place(offset(p,3,-1,0),"dead_brain_coral_wall_fan[facing=north,waterlogged=false]");
                place(offset(p,3,0,0),"cobblestone_wall");place(offset(p,2,1,1),"detector_rail[shape=east_west]");place(offset(p,2,1,2),"piston[facing=north]");
                Pos rail=offset(p,2,1,1);command("summon minecraft:minecart "+(rail.x+.5)+" "+(rail.y+.0625)+" "+(rail.z+.5));
            } else if(launchPhase==1) place(offset(p,2,2,2),"redstone_block");
            else if(launchPhase==2) {place(offset(p,2,2,2),"air");place(offset(p,2,1,2),"air");}
            else if(launchPhase==3) {place(offset(p,-2,0,0),"air");place(offset(p,3,0,1),"air");place(offset(p,-1,2,0),"stone");}
            else {place(offset(p,-1,2,0),"air");building=null;}
            launchTick=tick+(launchPhase==0?25:launchPhase<3?10:1);
        } else {
            if(launchPhase==1) {place(offset(p,-2,0,0),"air");place(offset(p,3,0,1),"air");place(offset(p,-1,2,0),"stone");}
            else if(launchPhase==2) {place(offset(p,-1,2,0),"air");building=null;}
            launchTick=tick+1;
        }
        ++launchPhase;
    }
    static int countEntities() throws Exception {
        int n=0; for(Object e:(Iterable<?>)r.call(world,"getAllEntities")) if(e!=player && Math.abs(value(e,"getX")-arenaX)<32768 && Math.abs(value(e,"getZ")-arenaZ)<32768) ++n; return n;
    }
    static void appendStack(StringBuilder b,String kind,int index,Object stack) throws Exception {
        Object item=r.call(stack,"getItem");
        b.append(kind).append('\t').append(index).append('\t').append(r.call(itemRegistry,"getId",item))
         .append('\t').append(r.call(stack,"getCount")).append('\t').append(r.call(stack,"getDamageValue"))
         .append('\t').append(r.call(stack,"getMaxDamage")).append('\n');
    }
    static double value(Object object,String method) throws Exception {return ((Number)r.call(object,method)).doubleValue();}
    static void publish() throws Exception {
        StringBuilder b=new StringBuilder(8192);
        int flags=(creative?1:0)|((Boolean)r.call(player,"isFallFlying")?2:0)|((Boolean)r.call(player,"isPassenger")?4:0)| (paused?8:0)|(swimming?16:0);
        b.append("S\t1\t").append(tick).append('\t').append(ack).append('\t').append(levelToken);
        b.append('\t').append(value(player,"getX")-arenaX).append('\t').append(value(player,"getY")).append('\t').append(value(player,"getZ")-arenaZ);
        for(String method:new String[]{"getYRot","getXRot","getHealth","getAirSupply"}) b.append('\t').append(value(player,method));
        b.append('\t').append(r.call(r.call(player,"getFoodData"),"getFoodLevel")).append('\t').append(flags)
         .append('\t').append(miningStage).append('\t').append(selected).append('\n');
        b.append("D\t").append(pushedX).append('\t').append(pushedY).append('\t').append(pushedZ).append('\n');
        for(int i=0;i<36;i++) appendStack(b,"I",i,r.call(inventory,"getItem",i));
        if(menu!=null && menu!=r.get(player,"containerMenu")) menu=null;
        if(menu!=null) {
            b.append("G\t").append(r.type("net.minecraft.world.inventory.CraftingMenu").isInstance(menu)?3:menu==r.get(player,"inventoryMenu")?2:0).append('\n');
            @SuppressWarnings("unchecked") List<Object> slots=(List<Object>)r.get(menu,"slots");
            for(int i=0;i<slots.size();i++) {Object slot=slots.get(i);appendStack(b,"C",i,r.call(slot,"getItem"));b.append("L\t").append(i).append("\t").append(r.get(slot,"x")).append("\t").append(r.get(slot,"y")).append("\n");}
            appendStack(b,"H",0,r.call(menu,"getCarried"));
        }
        Visuals.append(b);
        int edits=0;
        for(Iterator<Pos> it=dirty.iterator();it.hasNext() && edits<2048;edits++) {
            Pos p=it.next(); it.remove();if(!inArena(p)) continue;Object current=state(p);int id=stateId(current);
            if(r.type("net.minecraft.world.level.block.piston.MovingPistonBlock").isInstance(r.call(current,"getBlock"))) {if(moving.size()<256) moving.add(p);} else moving.remove(p);
            if(cells.containsKey(p) || cells.size()<MAX_CELLS) cells.put(p,id);
            b.append("B\t").append(p.x-arenaX).append('\t').append(p.y).append('\t').append(p.z-arenaZ).append('\t').append(id).append('\n');
            if(terrainOriginal.containsKey(p)) b.append("T\t").append(p.x-arenaX).append('\t').append(p.y).append('\t').append(p.z-arenaZ).append('\t').append(terrainOriginal.get(p)).append('\n');
        }
        for(Iterator<Pos> it=moving.iterator();it.hasNext();) {
            Pos p=it.next();Object entity=r.call(world,"getBlockEntity",pos(p));
            if(entity==null || !r.type("net.minecraft.world.level.block.piston.PistonMovingBlockEntity").isInstance(entity)) {it.remove();continue;}
            b.append("P\t").append(p.x-arenaX).append('\t').append(p.y).append('\t').append(p.z-arenaZ).append('\t').append(stateId(r.call(entity,"getMovedState")));
            for(String method:new String[]{"getXOff","getYOff","getZOff"}) b.append('\t').append(r.call(entity,method,1f));b.append('\n');
        }
        int count=0;
        for(Object e:(Iterable<?>)r.call(world,"getAllEntities")) {
            if(e==player || Math.abs(value(e,"getX")-arenaX)>=32768 || Math.abs(value(e,"getZ")-arenaZ)>=32768 || ++count>MAX_ENTITIES) continue;
            b.append("E\t").append(r.call(e,"getId")).append('\t').append(r.call(entityRegistry,"getId",r.call(e,"getType")));
            b.append('\t').append(value(e,"getX")-arenaX).append('\t').append(value(e,"getY")).append('\t').append(value(e,"getZ")-arenaZ);
            for(String method:new String[]{"getYRot","getXRot"}) b.append('\t').append(value(e,method));
            float health=0; int fuse=-1,state=0;
            if(r.type("net.minecraft.world.entity.LivingEntity").isInstance(e)) health=((Number)r.call(e,"getHealth")).floatValue();
            if(r.type("net.minecraft.world.entity.item.PrimedTnt").isInstance(e)) fuse=((Number)r.call(e,"getFuse")).intValue();
            if(r.type("net.minecraft.world.entity.item.FallingBlockEntity").isInstance(e)) state=stateId(r.call(e,"getBlockState"));
            Object visibleItem=null;
            if(r.type("net.minecraft.world.entity.item.ItemEntity").isInstance(e)) visibleItem=r.call(e,"getItem");
            else if(r.type("net.minecraft.world.entity.LivingEntity").isInstance(e)) visibleItem=r.call(e,"getMainHandItem");
            else if(r.type("net.minecraft.world.entity.vehicle.Boat").isInstance(e)) visibleItem=r.make("net.minecraft.world.item.ItemStack",r.call(e,"getDropItem"),1);
            int item=visibleItem==null?0:((Number)r.call(itemRegistry,"getId",r.call(visibleItem,"getItem"))).intValue();
            int amount=visibleItem==null?0:((Number)r.call(visibleItem,"getCount")).intValue();
            if(r.type("net.minecraft.world.entity.vehicle.Boat").isInstance(e)) amount=((Boolean)r.call(e,"getPaddleState",0)?1:0)|((Boolean)r.call(e,"getPaddleState",1)?2:0);
            b.append('\t').append(health).append('\t').append(fuse).append('\t').append(state).append('\t').append(item).append('\t').append(amount).append('\n');
        }
        if(!lastError.isEmpty() && tick<errorUntil) b.append("R\t").append(Base64.getEncoder().encodeToString(lastError.getBytes(StandardCharsets.UTF_8))).append('\n');
        byte[] bytes=b.toString().getBytes(StandardCharsets.UTF_8);
        // Coalesce continuous state while retaining every unsent block edit.
        byte[] previous=snapshot.getAndSet(bytes);
        if(previous!=null) for(String line:new String(previous,StandardCharsets.UTF_8).split("\n"))
            if(line.startsWith("B\t")) {String[] p=line.split("\t");dirty.add(new Pos(integer(p,1)+arenaX,integer(p,2),integer(p,3)+arenaZ));}
        synchronized(signal) {signal.notifyAll();}
    }
    static void startTransport() throws Exception {
        String token=Files.readString(Path.of("ipc-token.txt")).trim();
        ServerSocket listener=new ServerSocket(); listener.bind(new InetSocketAddress(InetAddress.getLoopbackAddress(),Integer.getInteger("steve.port",25576)),1);
        Thread accept=new Thread(()-> {
            while(!listener.isClosed()) try {
                Socket client=listener.accept(); client.setTcpNoDelay(true); client.setSoTimeout(10000);
                DataInputStream in=new DataInputStream(client.getInputStream());
                int size=Integer.reverseBytes(in.readInt()); if(size<1 || size>128) {client.close(); continue;}
                if(!new String(in.readNBytes(size),StandardCharsets.UTF_8).equals("SM64DS1\t"+token)) {client.close(); continue;}
                client.setSoTimeout(0); resync.set(true);
                Thread writer=new Thread(()-> {
                    try {
                        DataOutputStream out=new DataOutputStream(client.getOutputStream());
                        while(!client.isClosed()) {
                            byte[] bytes=snapshot.getAndSet(null);
                            if(bytes==null) {synchronized(signal) {signal.wait(100);} continue;}
                            out.writeInt(Integer.reverseBytes(bytes.length)); out.write(bytes); out.flush();
                        }
                    } catch(Exception ignored) {try {client.close();} catch(IOException ignored2) {}}
                },"Steve IPC writer"); writer.setDaemon(true); writer.start();
                try {
                    while(!client.isClosed()) {
                        int n=Integer.reverseBytes(in.readInt());
                        if(n<1 || n>8192 || input.size()>=1024) throw new IOException("IPC request budget");
                        byte[] bytes=in.readNBytes(n); if(bytes.length!=n) throw new EOFException();
                        input.offer(new String(bytes,StandardCharsets.UTF_8));
                    }
                } finally {client.close();}
            } catch(IOException e) {System.err.println("[steve] IPC client disconnected: "+e.getMessage());}
        },"Steve IPC accept"); accept.setDaemon(true); accept.start();
    }
    static void savePlayer() throws Exception {
        Object tag=r.make("net.minecraft.nbt.CompoundTag"); r.call(player,"addAdditionalSaveData",tag);r.call(tag,"putBoolean","SteveCreative",creative);
        if(savedChest!=null) r.call(tag,"put","SteveChestBeforeFlight",r.call(savedChest,"save",r.make("net.minecraft.nbt.CompoundTag")));
        File temporary=new File("steve-player.tmp");
        r.call(r.type("net.minecraft.nbt.NbtIo"),"writeCompressed",tag,temporary);
        Files.move(temporary.toPath(),Path.of("steve-player.dat"),StandardCopyOption.REPLACE_EXISTING);
        Path cache=Path.of("steve-cells.tmp");
        try(DataOutputStream out=new DataOutputStream(Files.newOutputStream(cache))) {
            out.writeInt(0x53544333);out.writeInt(cells.size());
            for(var entry:cells.entrySet()) {Pos p=entry.getKey();out.writeInt(p.x);out.writeInt(p.y);out.writeInt(p.z);out.writeInt(entry.getValue());out.writeInt(terrainOriginal.getOrDefault(p,-1));out.writeFloat(terrainHeights.getOrDefault(p,Float.NaN));}
        }
        Files.move(cache,Path.of("steve-cells.dat"),StandardCopyOption.REPLACE_EXISTING);
    }
    static void loadPlayer() throws Exception {
        Path cache=Path.of("steve-cells.dat");
        if(Files.exists(cache)) try(DataInputStream in=new DataInputStream(Files.newInputStream(cache))) {
            int version=in.readInt();if(version!=0x53544331 && version!=0x53544332 && version!=0x53544333) throw new IOException("Block cache version");
            int count=in.readInt();if(count<0 || count>MAX_CELLS) throw new IOException("Block cache budget");
            for(int i=0;i<count;i++) {Pos p=new Pos(in.readInt(),in.readInt(),in.readInt());int id=in.readInt();
                if(Math.abs((long)p.x)>29999984 || Math.abs((long)p.y)>2048 || Math.abs((long)p.z)>29999984 || id<0 || id>100000) throw new IOException("Block cache range");
                cells.put(p,id);if(version>=0x53544332) {int original=in.readInt();if(original>=0) terrainOriginal.put(p,original);}if(version==0x53544333) {float height=in.readFloat();if(!Float.isNaN(height)) surface(p,height);}dirty.add(p);
            }
        }
        File file=new File("steve-player.dat"); if(file.isFile()) {Object tag=r.call(r.type("net.minecraft.nbt.NbtIo"),"readCompressed",file);r.call(player,"readAdditionalSaveData",tag);creative=(Boolean)r.call(tag,"getBoolean","SteveCreative");if((Boolean)r.call(tag,"contains","SteveChestBeforeFlight")) {savedChest=r.call(r.type("net.minecraft.world.item.ItemStack"),"of",r.call(tag,"getCompound","SteveChestBeforeFlight"));restoreChest();}}
    }
    static String quote(Object value) {return "\""+String.valueOf(value).replace("\\","\\\\").replace("\"","\\\"")+"\"";}
    static void exportRegistry() throws Exception {
        Path output=Path.of("registry.tsv");
        try(BufferedWriter writer=Files.newBufferedWriter(output)) {
            for(Object block:(Iterable<?>)blockRegistry) {
                String name=String.valueOf(r.call(blockRegistry,"getKey",block));
                String blockItem=String.valueOf(r.call(itemRegistry,"getKey",r.call(block,"asItem")));
                Object defaultState=r.call(block,"defaultBlockState");
                for(Object state:(List<?>)r.call(r.call(block,"getStateDefinition"),"getPossibleStates")) {
                    StringBuilder props=new StringBuilder();
                    @SuppressWarnings("unchecked") Map<Object,Object> values=(Map<Object,Object>)r.call(state,"getValues");
                    for(var entry:values.entrySet()) {
                        if(props.length()>0) props.append(',');
                        props.append(r.call(entry.getKey(),"getName")).append('=').append(r.call(entry.getKey(),"getName",entry.getValue()));
                    }
                    writer.write("B\t"+stateId(state)+"\t"+name+"\t"+props+"\t"+blockItem+"\t"+state.equals(defaultState)+"\n");
                }
            }
            for(Object item:(Iterable<?>)itemRegistry) writer.write("I\t"+r.call(itemRegistry,"getId",item)+"\t"+r.call(itemRegistry,"getKey",item)+"\n");
            for(Object type:(Iterable<?>)entityRegistry) writer.write("E\t"+r.call(entityRegistry,"getId",type)+"\t"+r.call(entityRegistry,"getKey",type)+"\n");
        }
    }
}
