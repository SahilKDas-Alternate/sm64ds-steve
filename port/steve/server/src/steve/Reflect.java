package steve;

import java.lang.reflect.*;
import java.nio.file.*;
import java.util.*;

/** Resolve the official 1.18 obfuscated server from Mojang's verified mappings.
 * No Minecraft classes or implementation bytes are redistributed in this mod. */
final class Reflect {
    static final class Info {
        String named, obfuscated;
        Map<String,String> fields=new HashMap<>();
        Map<String,List<String>> methods=new HashMap<>();
        Map<String,List<Signature>> signatures=new HashMap<>();
    }
    static record Signature(String name,String result,int arity) {}
    final Map<String,Info> named=new HashMap<>(), obfuscated=new HashMap<>();
    final ClassLoader loader;
    final Map<String,Method> cache=new HashMap<>();
    Reflect(ClassLoader loader) throws Exception {
        this.loader=loader;
        Info current=null;
        for(String line:Files.readAllLines(Path.of(System.getProperty("steve.mappings","server-mappings.txt")))) {
            if(line.isBlank() || line.startsWith("#")) continue;
            if(!line.startsWith(" ")) {
                String[] parts=line.replace(":","").split(" -> ");
                if(parts.length!=2) continue;
                current=new Info(); current.named=parts[0]; current.obfuscated=parts[1];
                named.put(current.named,current); obfuscated.put(current.obfuscated,current);
            } else if(current!=null) {
                String[] parts=line.trim().split(" -> ");
                if(parts.length!=2) continue;
                String left=parts[0].replaceFirst("^\\d+:\\d+:","");
                int bracket=left.indexOf('(');
                if(bracket>=0) {
                    String name=left.substring(0,bracket);
                    name=name.substring(name.lastIndexOf(' ')+1);
                    current.methods.computeIfAbsent(name,k->new ArrayList<>()).add(parts[1]);
                    String parameters=left.substring(bracket+1,left.indexOf(')',bracket));
                    String result=left.substring(0,left.lastIndexOf(' ',bracket));
                    current.signatures.computeIfAbsent(name,k->new ArrayList<>()).add(new Signature(parts[1],result,
                            parameters.isEmpty()?0:parameters.split(",").length));
                } else {
                    current.fields.put(left.substring(left.lastIndexOf(' ')+1),parts[1]);
                }
            }
        }
    }
    Class<?> type(String name) throws Exception {
        Info info=named.get(name);
        return Class.forName(info==null?name:info.obfuscated,true,loader);
    }
    Object make(String name,Object...args) throws Exception {
        for(Constructor<?> c:type(name).getDeclaredConstructors()) if(fits(c.getParameterTypes(),args)) {
            c.setAccessible(true); return c.newInstance(args);
        }
        throw new NoSuchMethodException(name+" constructor "+Arrays.toString(args));
    }
    static boolean fits(Class<?>[] types,Object[] args) {
        if(types.length!=args.length) return false;
        for(int i=0;i<types.length;i++) {
            Class<?> t=types[i];
            if(args[i]==null) { if(t.isPrimitive()) return false; continue; }
            if(t==int.class) t=Integer.class; else if(t==float.class) t=Float.class;
            else if(t==double.class) t=Double.class; else if(t==boolean.class) t=Boolean.class;
            else if(t==long.class) t=Long.class; else if(t==short.class) t=Short.class;
            else if(t==byte.class) t=Byte.class;
            if(!t.isInstance(args[i])) return false;
        }
        return true;
    }
    Object call(Object receiver,String name,Object...args) throws Exception {
        Class<?> root=receiver instanceof Class<?>?(Class<?>)receiver:receiver.getClass();
        String key=root.getName()+"#"+name;
        for(Object arg:args) key+="/"+(arg==null?"null":arg.getClass().getName());
        Method found=cache.get(key);
        if(found==null) {
            for(Class<?> c=root;c!=null && found==null;c=c.getSuperclass()) {
                Info info=obfuscated.get(c.getName());
                List<Signature> signatures=info==null?List.of():info.signatures.getOrDefault(name,List.of());
                for(Method m:c.getDeclaredMethods()) if(matches(m,name,signatures,info==null,args)) {
                    if(receiver instanceof Class<?> && !Modifier.isStatic(m.getModifiers())) continue;
                    found=m; break;
                }
            }
            if(found==null) for(Method m:root.getMethods()) {
                Info info=obfuscated.get(m.getDeclaringClass().getName());
                if(matches(m,name,info==null?List.of():info.signatures.getOrDefault(name,List.of()),info==null,args)) {found=m;break;}
            }
            if(found==null) throw new NoSuchMethodException(key);
            found.setAccessible(true); cache.put(key,found);
        }
        try { return found.invoke(receiver instanceof Class<?>?null:receiver,args); }
        catch(InvocationTargetException e) { throw new RuntimeException(name,e.getCause()); }
    }
    boolean matches(Method m,String name,List<Signature> signatures,boolean plain,Object[] args) {
        if(!fits(m.getParameterTypes(),args)) return false;
        if(plain) return m.getName().equals(name);
        for(Signature signature:signatures) {
            String result=signature.result(); Info mapped=named.get(result);
            if(mapped!=null) result=mapped.obfuscated;
            if(signature.arity()==args.length && m.getName().equals(signature.name()) && m.getReturnType().getTypeName().equals(result)) return true;
        }
        return false;
    }
    Field field(Object object,String name) throws Exception {
        Class<?> c=object instanceof Class<?>?(Class<?>)object:object.getClass();
        for(;c!=null;c=c.getSuperclass()) {
            Info i=obfuscated.get(c.getName());
            String obf=i==null?name:i.fields.getOrDefault(name,name);
            try {Field f=c.getDeclaredField(obf); f.setAccessible(true); return f;}
            catch(NoSuchFieldException ignored) {}
        }
        throw new NoSuchFieldException(name);
    }
    Object get(Object object,String name) throws Exception {return field(object,name).get(object instanceof Class<?>?null:object);}
    void set(Object object,String name,Object value) throws Exception {field(object,name).set(object instanceof Class<?>?null:object,value);}
    Object constant(String owner,String field) throws Exception {return get(type(owner),field);}
    String mapped(String owner,String method) {
        Info i=named.get(owner);
        if(i==null || !i.methods.containsKey(method)) throw new IllegalArgumentException(owner+"."+method);
        return i.methods.get(method).get(0);
    }
}
