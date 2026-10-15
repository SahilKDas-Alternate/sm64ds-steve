package steve;
import java.lang.instrument.*;
import java.security.ProtectionDomain;
import jdk.internal.org.objectweb.asm.*;

/** A small official-server adapter; no Fabric/Gradle remapped caches needed. */
public final class Agent {
    public static void premain(String options,Instrumentation instrumentation) throws Exception {
        Reflect mappings=new Reflect(ClassLoader.getSystemClassLoader());
        // Mojang's bundler uses an isolated URLClassLoader. Only the adapter's
        // JDK-only backend goes on bootstrap search; Minecraft code stays isolated.
        instrumentation.appendToBootstrapClassLoaderSearch(new java.util.jar.JarFile("steve-backend.jar"));
        String server="net/minecraft/server/MinecraftServer";
        String tick=mappings.mapped("net.minecraft.server.MinecraftServer","tickServer");
        String prepare=mappings.mapped("net.minecraft.server.MinecraftServer","prepareLevels");
        String spawn=mappings.mapped("net.minecraft.server.MinecraftServer","setInitialSpawn");
        String connection=mappings.named.get("net.minecraft.network.Connection").obfuscated.replace('.','/');
        String send=mappings.mapped("net.minecraft.network.Connection","send");
        String entity=mappings.named.get("net.minecraft.world.entity.Entity").obfuscated.replace('.','/');
        String controlled=mappings.mapped("net.minecraft.world.entity.Entity","isControlledByLocalInstance");
        String move=mappings.mapped("net.minecraft.world.entity.Entity","move");
        String mover=mappings.named.get("net.minecraft.world.entity.MoverType").obfuscated.replace('.','/');
        String vector=mappings.named.get("net.minecraft.world.phys.Vec3").obfuscated.replace('.','/');
        String level=mappings.named.get("net.minecraft.world.level.Level").obfuscated.replace('.','/');
        String worldLevel=mappings.named.get("net.minecraft.server.level.ServerLevel").obfuscated.replace('.','/');
        String blockUpdated=mappings.mapped("net.minecraft.server.level.ServerLevel","sendBlockUpdated");
        String setBlock=mappings.mapped("net.minecraft.world.level.Level","setBlock");
        String pos=mappings.named.get("net.minecraft.core.BlockPos").obfuscated.replace('.','/');
        String state=mappings.named.get("net.minecraft.world.level.block.state.BlockState").obfuscated.replace('.','/');
        String tnt=mappings.named.get("net.minecraft.world.level.block.TntBlock").obfuscated.replace('.','/');
        String tntPlace=mappings.mapped("net.minecraft.world.level.block.TntBlock","onPlace");
        String tntNeighbor=mappings.mapped("net.minecraft.world.level.block.TntBlock","neighborChanged");
        String stateBase=mappings.named.get("net.minecraft.world.level.block.state.BlockBehaviour$BlockStateBase").obfuscated.replace('.','/');
        java.util.Set<String> collision=new java.util.HashSet<>(mappings.named.get("net.minecraft.world.level.block.state.BlockBehaviour$BlockStateBase").methods.get("getCollisionShape"));
        String voxel=mappings.named.get("net.minecraft.world.phys.shapes.VoxelShape").obfuscated.replace('.','/');
        String prepareDesc="(L"+mappings.named.get("net.minecraft.server.level.progress.ChunkProgressListener").obfuscated.replace('.','/')+";)V";
        String spawnDesc="(L"+mappings.named.get("net.minecraft.server.level.ServerLevel").obfuscated.replace('.','/')+";L"+mappings.named.get("net.minecraft.world.level.storage.ServerLevelData").obfuscated.replace('.','/')+";ZZ)V";
        String packetPrefix="(L"+mappings.named.get("net.minecraft.network.protocol.Packet").obfuscated.replace('.','/')+";";
        instrumentation.addTransformer(new ClassFileTransformer() {
            public byte[] transform(ClassLoader loader,String name,Class<?> redef,ProtectionDomain domain,byte[] bytes) {
                if(!server.equals(name) && !connection.equals(name) && !level.equals(name) && !entity.equals(name) && !worldLevel.equals(name) && !tnt.equals(name) && !stateBase.equals(name)) return null;
                try {
                    ClassReader reader=new ClassReader(bytes);
                    ClassWriter writer=new ClassWriter(reader,ClassWriter.COMPUTE_MAXS);
                    reader.accept(new ClassVisitor(Opcodes.ASM8,writer) {
                        @Override public MethodVisitor visitMethod(int access,String method,String descriptor,String signature,String[] exceptions) {
                            MethodVisitor output=super.visitMethod(access,method,descriptor,signature,exceptions);
                            if(stateBase.equals(name) && collision.contains(method) && Type.getArgumentTypes(descriptor).length>=2 && Type.getArgumentTypes(descriptor).length<=3 && descriptor.endsWith("L"+voxel+";")) {
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitInsn(int opcode) {
                                        if(opcode==Opcodes.ARETURN) {
                                            super.visitVarInsn(Opcodes.ALOAD,0);super.visitVarInsn(Opcodes.ALOAD,1);super.visitVarInsn(Opcodes.ALOAD,2);
                                            super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","terrainCollision","(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;",false);
                                            super.visitTypeInsn(Opcodes.CHECKCAST,voxel);
                                        }
                                        super.visitInsn(opcode);
                                    }
                                };
                            }
                            if(tnt.equals(name) && descriptor.endsWith(")V") &&
                               ((method.equals(tntPlace) && Type.getArgumentTypes(descriptor).length==5) ||
                                (method.equals(tntNeighbor) && Type.getArgumentTypes(descriptor).length==6))) {
                                boolean placement=method.equals(tntPlace) && Type.getArgumentTypes(descriptor).length==5;
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitCode() {
                                        super.visitCode();Label normal=new Label();
                                        super.visitVarInsn(Opcodes.ALOAD,2);super.visitVarInsn(Opcodes.ALOAD,3);
                                        if(placement)super.visitVarInsn(Opcodes.ILOAD,5);
                                        super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend",placement?"tntPlacement":"tntUpdate",placement?"(Ljava/lang/Object;Ljava/lang/Object;Z)Z":"(Ljava/lang/Object;Ljava/lang/Object;)Z",false);
                                        super.visitJumpInsn(Opcodes.IFEQ,normal);super.visitInsn(Opcodes.RETURN);
                                        super.visitLabel(normal);super.visitFrame(Opcodes.F_SAME,0,null,0,null);
                                    }
                                };
                            }
                            if(entity.equals(name) && method.equals(move) && descriptor.equals("(L"+mover+";L"+vector+";)V")) {
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitCode() {
                                        super.visitCode();super.visitVarInsn(Opcodes.ALOAD,0);super.visitVarInsn(Opcodes.ALOAD,1);super.visitVarInsn(Opcodes.ALOAD,2);
                                        super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","pushed","(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V",false);
                                    }
                                };
                            }
                            if(server.equals(name) && method.equals(prepare) && descriptor.equals(prepareDesc)) {
                                // Suppress 441 spawn chunks; bridge explicitly loads its bounded arena.
                                return new MethodVisitor(Opcodes.ASM8) {
                                    public void visitEnd() {output.visitCode(); output.visitInsn(Opcodes.RETURN); output.visitMaxs(0,2); output.visitEnd();}
                                };
                            }
                            if(server.equals(name) && method.equals(spawn) && descriptor.equals(spawnDesc) && (access&Opcodes.ACC_STATIC)!=0) {
                                return new MethodVisitor(Opcodes.ASM8) {
                                    public void visitEnd() {
                                        output.visitCode(); output.visitVarInsn(Opcodes.ALOAD,0); output.visitVarInsn(Opcodes.ALOAD,1);
                                        output.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","spawn","(Ljava/lang/Object;Ljava/lang/Object;)V",false);
                                        output.visitInsn(Opcodes.RETURN); output.visitMaxs(2,4); output.visitEnd();
                                    }
                                };
                            }
                            if(worldLevel.equals(name) && method.equals(blockUpdated) && descriptor.equals("(L"+pos+";L"+state+";L"+state+";I)V")) {
                                System.out.println("[steve] Block-update hook installed on "+name+"."+method+descriptor);
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitCode() {
                                        super.visitCode();
                                        super.visitVarInsn(Opcodes.ALOAD,0);super.visitVarInsn(Opcodes.ALOAD,1);super.visitVarInsn(Opcodes.ALOAD,3);
                                        super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","changed","(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V",false);
                                    }
                                };
                            }
                            if(level.equals(name) && method.equals(setBlock) && descriptor.equals("(L"+pos+";L"+state+";II)Z")) {
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitCode() {
                                        super.visitCode();
                                        // Dirty positions are queried after the entire server tick. This
                                        // also captures updates made without the client-notification flag.
                                        super.visitVarInsn(Opcodes.ALOAD,0); super.visitVarInsn(Opcodes.ALOAD,1); super.visitVarInsn(Opcodes.ALOAD,2);
                                        super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","changed","(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V",false);
                                    }
                                };
                            }
                            if(server.equals(name) && method.equals(tick) && descriptor.equals("(Ljava/util/function/BooleanSupplier;)V")) {
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    boolean live=true;
                                    public void visitFrame(int type,int locals,Object[] local,int stack,Object[] values) {
                                        if(type==Opcodes.F_FULL || type==Opcodes.F_NEW) live=locals>0;
                                        super.visitFrame(type,locals,local,stack,values);
                                    }
                                    public void visitInsn(int opcode) {
                                        if(opcode==Opcodes.RETURN && live) {
                                            super.visitVarInsn(Opcodes.ALOAD,0);
                                            super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","tick","(Ljava/lang/Object;)V",false);
                                        }
                                        super.visitInsn(opcode);
                                    }
                                };
                            }
                            if(entity.equals(name) && method.equals(controlled) && descriptor.equals("()Z")) {
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitCode() {
                                        super.visitCode(); Label normal=new Label();
                                        super.visitVarInsn(Opcodes.ALOAD,0);
                                        super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","isLocalVehicle","(Ljava/lang/Object;)Z",false);
                                        super.visitJumpInsn(Opcodes.IFEQ,normal); super.visitInsn(Opcodes.ICONST_1); super.visitInsn(Opcodes.IRETURN);
                                        super.visitLabel(normal); super.visitFrame(Opcodes.F_SAME,0,null,0,null);
                                    }
                                };
                            }
                            if(connection.equals(name) && method.equals(send) && descriptor.startsWith(packetPrefix) && descriptor.endsWith(")V")) {
                                return new MethodVisitor(Opcodes.ASM8,output) {
                                    public void visitCode() {
                                        super.visitCode(); Label normal=new Label();
                                        super.visitVarInsn(Opcodes.ALOAD,0);
                                        super.visitMethodInsn(Opcodes.INVOKESTATIC,"steve/Backend","isLocalConnection","(Ljava/lang/Object;)Z",false);
                                        super.visitJumpInsn(Opcodes.IFEQ,normal); super.visitInsn(Opcodes.RETURN);
                                        super.visitLabel(normal); super.visitFrame(Opcodes.F_SAME,0,null,0,null);
                                    }
                                };
                            }
                            return output;
                        }
                    },(level.equals(name) || server.equals(name) || worldLevel.equals(name))?ClassReader.EXPAND_FRAMES:0);
                    return writer.toByteArray();
                } catch(Throwable e) {e.printStackTrace(); throw new IllegalStateException("Steve adapter transformation failed",e);}
            }
        });
        System.out.println("[steve] Official Minecraft 1.18 adapter installed; protocol 1");
    }
}
