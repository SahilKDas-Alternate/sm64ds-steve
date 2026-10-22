#pragma once
namespace steve {
// Null hooks let the shared host slices and ROM-free smoke targets retain
// their original link closure. The game frontend registers its own hooks.
__declspec(selectany) bool (*enabled_hook)()=nullptr;
__declspec(selectany) void (*frame_hook)()=nullptr;
__declspec(selectany) void (*before_hook)(void*)=nullptr;
__declspec(selectany) void (*after_hook)(void*)=nullptr;
__declspec(selectany) bool (*player_render_hook)(void*)=nullptr;
__declspec(selectany) void (*world_render_hook)()=nullptr;
__declspec(selectany) bool (*removed_hook)(float,float,float)=nullptr;
__declspec(selectany) bool (*health_hook)(int,int)=nullptr;
__declspec(selectany) void (*collision_prepare_hook)()=nullptr;
__declspec(selectany) int (*sphere_support_hook)(void*,int)=nullptr;
__declspec(selectany) bool (*continuous_override_hook)(void*)=nullptr;
inline bool native_override_continuous(void* p) {return continuous_override_hook && continuous_override_hook(p);}
__declspec(selectany) void **compiled_extending_table=nullptr,**host_extending_table=nullptr;
inline void native_prepare_collision() {if(collision_prepare_hook)collision_prepare_hook();}
inline int native_support_sphere(void* p,int result) {return sphere_support_hook?sphere_support_hook(p,result):result;}
inline bool native_enabled() {return enabled_hook && enabled_hook();}
inline void native_frame() {if(frame_hook) frame_hook();}
inline void native_before_player(void* p) {if(before_hook) before_hook(p);}
inline void native_after_player(void* p) {if(after_hook) after_hook(p);}
inline bool native_render_player(void* p) {return player_render_hook && player_render_hook(p);}
inline void native_render_world() {if(world_render_hook) world_render_hook();}
inline bool native_removed(float x,float y,float z) {return removed_hook && removed_hook(x,y,z);}
}
