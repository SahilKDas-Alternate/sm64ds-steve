package steve;
import java.io.*;
import java.nio.file.*;
import java.util.*;
/** Collision boxes come from the official server's own VoxelShapes. */
final class Shapes {
    static void integer(DataOutputStream out,int value)throws IOException {out.writeInt(Integer.reverseBytes(value));}
    static void export()throws Exception {
        Reflect r=Backend.r;List<Object> states=new ArrayList<>();
        for(Object block:(Iterable<?>)Backend.blockRegistry) states.addAll((List<?>)r.call(r.call(block,"getStateDefinition"),"getPossibleStates"));
        Object origin=r.make("net.minecraft.core.BlockPos",0,64,0);
        Object context=r.call(r.type("net.minecraft.world.phys.shapes.CollisionContext"),"empty");
        long boxes=0;
        try(DataOutputStream out=new DataOutputStream(new BufferedOutputStream(Files.newOutputStream(Path.of("shapes.bin"))))) {
            out.writeBytes("SCS1");integer(out,states.size());
            for(Object state:states) {
                List<?> bounds=(List<?>)r.call(r.call(state,"getCollisionShape",Backend.world,origin,context),"toAabbs");
                boxes+=bounds.size();if(bounds.size()>256 || boxes>1_000_000)throw new IOException("Collision shape budget");
                integer(out,Backend.stateId(state));integer(out,bounds.size());
                for(Object box:bounds)for(String field:new String[]{"minX","minY","minZ","maxX","maxY","maxZ"})integer(out,Float.floatToRawIntBits(((Number)r.get(box,field)).floatValue()));
            }
        }
        System.out.println("[steve] Exported "+states.size()+" vanilla collision states, "+boxes+" boxes");
    }
}
