// Game-only collision adapters; minimal ROM-free smoke targets retain null hooks.
extern "C" bool port_steve_override_continuous(void *object)
{
    if(!steve::native_enabled())return false;
    auto& mesh=*static_cast<dBgCh_Actr*>(object);auto* a=mesh.mActor;
    if(!a || (a->actorID!=210 && a->actorID!=282 && a->actorID!=283) || port_level_id()!=47)return false;
    const double dx=double(a->mPosX)-a->mPrevPosX,dz=double(a->mPosZ)-a->mPrevPosZ;
    if(a->actorID==210 && static_cast<daTrs_c*>(a)->unk_5cc==7 && dx*dx+dz*dz>double(1000*4096)*double(1000*4096)) {
        // Introduction relocation is a teleport out of the holding position.
        a->mPrevPosX=a->mPosX;a->mPrevPosY=a->mPosY;a->mPrevPosZ=a->mPosZ;
        mesh.ClearAllGroundFlags();mesh.mSphereClsn.flags=0;mesh.mRaycastLine.hasClsn=false;
        mesh.mSphereClsn.disp={0,0,0};
        if(std::getenv("SM64DS_STEVE_TEST_COMBAT"))std::fprintf(stderr,"[steve-boss-test] King Boo intro teleport synchronized at=(%.1f,%.1f,%.1f)\n",a->mPosX/4096.f,a->mPosY/4096.f,a->mPosZ/4096.f);
    }
    // King Boo and his falling key move less than their radii per frame. Use the
    // native sphere resolver: the host continuous line correction projects
    // his scripted appearance back toward the offstage holding position.
    mesh.UpdateDiscreteNoLava();
    return true;
}
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
    if(owner && port_level_id()==47) {
        const double radius=int(sphere.radius),x=sphere.disp.x,z=sphere.disp.z;
        if(x*x+z*z>16*radius*radius) {
            // The host prism pass can select a nonlocal mirror-room wall for
            // a large sphere (a 200-unit body gets a 33,000-unit push).
            // Such a contact cannot lie in this query's sphere. Reject that
            // wall only; retain floor/ceiling results and the ground fallback.
            sphere.disp.x=sphere.disp.z=0;sphere.flags&=~8u;
            static unsigned reported;
            if(std::getenv("SM64DS_STEVE_TEST_COMBAT") && reported++<4)std::fprintf(stderr,"[steve-boss-test] rejected nonlocal mirror contact radius=%.1f displacement=(%.1f,%.1f)\n",radius/4096.,x/4096.,z/4096.);
        }
    }
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

