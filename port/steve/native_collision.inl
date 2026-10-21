// Game-only collision adapters; minimal ROM-free smoke targets retain null hooks.
extern "C" {
extern void *_ZTV7dBgW_Kc[],*_ZTV10dBgW_KcMbg[],*data_020a0c80[];
}
// Real C++ constructors install compiler tables. The registry walkers above
// still use the ROM-indexed query slots (6/7/8), while MSVC reverses the three
// overloads into 7/6/5 after its single destructor slot. Reseat only tables
// whose exact identity is known; derived colliders keep their own dispatch.
extern "C" void *port_cpp_kc_vtable[],*port_cpp_mmc_vtable[];
#pragma comment(linker,"/alternatename:_port_cpp_kc_vtable=??_7dBgW_Kc@@6B@")
#pragma comment(linker,"/alternatename:_port_cpp_mmc_vtable=??_7dBgW_KcMbg@@6B@")
extern "C" void port_clsn_prepare_dispatch()
{
    for(unsigned i=0;i<24;i++)if(void* collider=data_020a0c80[i]) {
        void*** table=static_cast<void***>(collider);
        if(*table==port_cpp_kc_vtable)*table=_ZTV7dBgW_Kc;
        else if(*table==port_cpp_mmc_vtable)*table=_ZTV10dBgW_KcMbg;
        else if(steve::compiled_extending_table && *table==steve::compiled_extending_table)*table=steve::host_extending_table;
    }
}

// Contact at exactly one radius can have zero penetration, so the host sphere
// pass returns no floor flag. Keep resting actors (and the floating Grand Star)
// supported using the same KCL hit record rather than a guessed floor height.
extern "C" int port_steve_sphere_support(void *object,int result)
{
    if(!steve::native_enabled())return result;
    auto& sphere=*static_cast<dBgCh_SphCrr*>(object);
    auto* owner=reinterpret_cast<dActor_c*>(sphere.unk_00c);
    if(!owner || owner->mVertSpeed>0 || (sphere.flags&4))return result;
    const int offset=sphere.centre.y-owner->mPosY;
    const int previousBottom=owner->mPrevPosY+offset-int(sphere.radius);
    const int bottom=sphere.centre.y-int(sphere.radius);
    dBgCh_Gnd ray;Vector3 point=sphere.centre;
    point.y=std::max(point.y,owner->mPrevPosY+offset)+4*4096;
    ray.SetObjAndPos(point,owner);ray.mProbeHeight=point.y-bottom+8*4096;
    if(!ray.DetectClsn() || !ray.hasClsn)return result;
    int penetration=int(ray.clsnY)+int(sphere.radius)-sphere.centre.y;
    const bool resting=penetration>=-2*4096 && penetration<=4*4096;
    const bool crossed=owner->mPosY<=owner->mPrevPosY && previousBottom>=int(ray.clsnY)-4*4096 && bottom<=int(ray.clsnY);
    if(!resting && !crossed)return result;
    if(steve::native_removed(sphere.centre.x/4096.f,int(ray.clsnY)/4096.f,sphere.centre.z/4096.f))return result;
    sphere.flags|=5;
    sphere.disp.y=std::max(sphere.disp.y,std::max(0,penetration));
    // dBgPi's vptr stays the sphere result's; copy only its hit payload.
    memcpy(reinterpret_cast<char*>(&sphere.mClsnResult1)+4,static_cast<char*>(static_cast<void*>(static_cast<dBgPi*>(&ray)))+4,sizeof(dBgPi)-4);
    return result|1;
}

