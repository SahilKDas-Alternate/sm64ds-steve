package steve;

import java.util.*;
import java.nio.charset.StandardCharsets;

/** Bounded, complete visual snapshots. All Minecraft access stays on its tick thread. */
final class Visuals {
    static final Map<Integer,Boolean> entityStates=new HashMap<>();
    static final Map<Backend.Pos,Float> lids=new HashMap<>();
    static boolean is(Object e,String type) throws Exception {return Backend.r.type("net.minecraft.world.level.block.entity."+type+"BlockEntity").isInstance(e);}
    static int rgb(Object dye) throws Exception {return ((Number)Backend.r.call(dye,"getTextColor")).intValue();}
    static int cloth(Object dye) throws Exception {float[] c=(float[])Backend.r.call(dye,"getTextureDiffuseColors");return ((int)(c[0]*255)<<16)|((int)(c[1]*255)<<8)|(int)(c[2]*255);}
    static String encode(String s) {return Base64.getEncoder().encodeToString(s.getBytes(StandardCharsets.UTF_8));}
    static void append(StringBuilder b) throws Exception {
        var r=Backend.r;Set<Backend.Pos> alive=new HashSet<>();int count=0;
        double px=Backend.value(Backend.player,"getX"),pz=Backend.value(Backend.player,"getZ");
        for(var entry:Backend.cells.entrySet()) {
            var p=entry.getKey();int id=entry.getValue();Boolean has=entityStates.get(id);
            if(has==null){has=(Boolean)r.call(r.call(r.type("net.minecraft.world.level.block.Block"),"stateById",id),"hasBlockEntity");entityStates.put(id,has);}
            if(!has)continue;
            if(Math.abs(p.x()-px)>64 || Math.abs(p.z()-pz)>64 || !Backend.inArena(p)) continue;
            Object e=r.call(Backend.world,"getBlockEntity",Backend.pos(p));if(e==null) continue;
            String kind="";float progress=0;int color=0,glow=0;String text="",patterns="",styles="";
            if(is(e,"Chest") || is(e,"EnderChest")) {
                kind="chest";int open=((Number)r.get(r.get(e,"openersCounter"),"openCount")).intValue();
                progress=Math.max(0,Math.min(1,lids.getOrDefault(p,0f)+(open>0?.1f:-.1f)));lids.put(p,progress);alive.add(p);
            } else if(is(e,"ShulkerBox")) {kind="shulker";progress=((Number)r.get(e,"progress")).floatValue();}
            else if(is(e,"Sign")) {
                kind="sign";color=rgb(r.call(e,"getColor"));glow=(Boolean)r.call(e,"hasGlowingText")?1:0;
                StringJoiner lines=new StringJoiner("\n"),styleLines=new StringJoiner(";");
                for(int i=0;i<4;i++) {
                    Object message=r.call(e,"getMessage",i,false);String line=(String)r.call(message,"getString");
                    lines.add(line.substring(0,line.offsetByCodePoints(0,Math.min(96,line.codePointCount(0,line.length())))));
                    StringJoiner glyphs=new StringJoiner(",");int chars=0;
                    for(Object span:(List<?>)r.call(message,"toFlatList",r.constant("net.minecraft.network.chat.Style","EMPTY"))) {
                        Object style=r.call(span,"getStyle"),ink=r.call(style,"getColor");int rgb=ink==null?color:((Number)r.call(ink,"getValue")).intValue(),bits=0;
                        String[] names={"isBold","isItalic","isUnderlined","isStrikethrough","isObfuscated"};for(int k=0;k<names.length;k++)if((Boolean)r.call(style,names[k]))bits|=1<<k;
                        String content=(String)r.call(span,"getString");for(int k=0;k<content.codePointCount(0,content.length()) && chars<96;k++,chars++)glyphs.add(Integer.toHexString(rgb)+":"+bits);
                    }styleLines.add(glyphs.toString());
                }text=encode(lines.toString());styles=styleLines.toString();
            } else if(is(e,"Banner")) {
                kind="banner";StringJoiner layers=new StringJoiner(",");
                for(Object pair:(List<?>)r.call(e,"getPatterns")) {
                    Object pattern=r.call(pair,"getFirst"),dye=r.call(pair,"getSecond");
                    if(layers.length()<2048) layers.add(r.call(pattern,"getFilename")+":"+cloth(dye));
                }patterns=layers.toString();
            } else if(is(e,"Bell")) {kind="bell";progress=(Boolean)r.get(e,"shaking")?((Number)r.get(e,"ticks")).floatValue()+1:0;Object direction=r.get(e,"clickDirection");color=direction==null?0:((Enum<?>)direction).ordinal();}
            else if(is(e,"Skull")) {kind="skull";progress=(Boolean)r.call(Backend.world,"hasNeighborSignal",Backend.pos(p))?1:0;}
            else if(is(e,"Conduit")) {kind="conduit";progress=(Boolean)r.get(e,"isActive")?1:0;glow=(Boolean)r.get(e,"isHunting")?1:0;}
            else continue;
            if(++count>512 || b.length()>400000) break;
            b.append("V\t").append(p.x()-Backend.arenaX).append('\t').append(p.y()).append('\t').append(p.z()-Backend.arenaZ).append('\t').append(kind).append('\t').append(progress).append('\t').append(color).append('\t').append(glow).append('\t').append(text.isEmpty()?"-":text).append('\t').append(patterns.isEmpty()?"-":patterns).append('\t').append(styles.isEmpty()?"-":styles).append('\n');
        }
        lids.keySet().retainAll(alive);
    }
    /** Load only decorative NBT keys, preserving inventories and other block data. */
    static void decorate(Backend.Pos p,String encoded) throws Exception {
        var r=Backend.r;Object e=r.call(Backend.world,"getBlockEntity",Backend.pos(p));
        if(e==null || !(is(e,"Sign") || is(e,"Banner"))) throw new IllegalArgumentException("sign or banner required");
        String raw=new String(Base64.getDecoder().decode(encoded),StandardCharsets.UTF_8);
        if(raw.length()>4096) throw new IllegalArgumentException("decoration size");
        Object supplied=r.call(r.type("net.minecraft.nbt.TagParser"),"parseTag",raw),tag=r.call(e,"saveWithoutMetadata");
        Set<String> allowed=is(e,"Sign")?Set.of("Text1","Text2","Text3","Text4","Color","GlowingText"):Set.of("Patterns");
        for(Object key:(Set<?>)r.call(supplied,"getAllKeys")) {if(!allowed.contains(key)) throw new IllegalArgumentException("decoration key");r.call(tag,"put",key,r.call(supplied,"get",key));}
        r.call(e,"load",tag);r.call(e,"setChanged");Backend.dirty.add(p);
    }
}
