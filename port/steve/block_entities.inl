// Dynamic block entities use complete server snapshots; no NBT is interpreted on render threads.
bool dynamicModel(const BlockMesh& m) {
    const auto& n=m.name;
    return n=="chest" || n=="trapped_chest" || n=="ender_chest" || n.find("shulker_box")!=std::string::npos || n.find("_sign")!=std::string::npos || n.find("_banner")!=std::string::npos || n=="bell" || n=="conduit" || n=="dragon_head" || n=="dragon_wall_head";
}
float property(const BlockMesh& m,const char* key,float fallback=0) {auto p=m.properties.find(std::string(key)+"=");return p==std::string::npos?fallback:float(std::atof(m.properties.c_str()+p+std::strlen(key)+1));}
float facingAngle(const BlockMesh& m) {
    if(m.properties.find("facing=north")!=std::string::npos)return 3.14159265f;
    if(m.properties.find("facing=west")!=std::string::npos)return 1.57079633f;
    if(m.properties.find("facing=east")!=std::string::npos)return 4.71238898f;
    return 0;
}
MeshVertex turn(MeshVertex v,float angle) {float x=v.x-.5f,z=v.z-.5f;v.x=.5f+x*std::cos(angle)-z*std::sin(angle);v.z=.5f+x*std::sin(angle)+z*std::cos(angle);return v;}
void blockEntityDraw(const float camera[16],Cell at,unsigned id,const BlockVisual* state) {
    const auto* m=assets.block(id);if(!m)return;const auto& n=m->name;
    Vec base{at.x*scale,at.y*scale,at.z*scale};float yaw=facingAngle(*m),progress=state?std::clamp(state->progress,0.f,1.f):0;
    bool banner=n.find("_banner")!=std::string::npos,sign=n.find("_sign")!=std::string::npos,wall=n.find("_wall_")!=std::string::npos;
    if(n=="dragon_head")yaw=-property(*m,"rotation")*3.14159265f/8;
    if((banner || sign) && !wall) yaw=-property(*m,"rotation")*3.14159265f/8;
    auto transform=[&](MeshVertex v,size_t face) {
        v=turn(v,-yaw);
        if((n=="chest" || n=="trapped_chest" || n=="ender_chest") && face>=6) {
            float a=-(1-std::pow(1-progress,3.f))*1.57079633f,y=v.y-10.f/16,z=v.z-1.f/16;
            v.y=10.f/16+y*std::cos(a)-z*std::sin(a);v.z=1.f/16+y*std::sin(a)+z*std::cos(a);
        } else if(n.find("shulker_box")!=std::string::npos && face>=6) {v=turn(v,progress*4.71238898f);v.y+=progress*.5f;}
        else if(banner && face<6) {
            float a=(std::cos((float(client.state.tick)+at.x*7+at.z*9)*.05f)*.05f+.05f),y=v.y-29.f/16,z=v.z-.5f;
            v.y=29.f/16+y*std::cos(a)-z*std::sin(a);v.z=.5f+y*std::sin(a)+z*std::cos(a);
        } else if(n=="bell" && progress>0) {
            float t=state->progress;float a=std::sin(t/3.14159265f)*.7f/(1+t/4),y=v.y-.75f;
            if(state->color>=4){float x=v.x-.5f;if(state->color==4)a=-a;v.x=.5f+x*std::cos(a)-y*std::sin(a);v.y=.75f+x*std::sin(a)+y*std::cos(a);}
            else {float z=v.z-.5f;if(state->color==2)a=-a;v.y=.75f+y*std::cos(a)-z*std::sin(a);v.z=.5f+y*std::sin(a)+z*std::cos(a);}
        }
        if((n=="dragon_head" || n=="dragon_wall_head") && face>=6 && face<12 && progress>0) {
            float a=(std::sin(float(client.state.tick)*.2f)+1)*.2f,y=v.y-.25f,z=v.z-1;
            v.y=.25f+y*std::cos(a)-z*std::sin(a);v.z=1+y*std::sin(a)+z*std::cos(a);
        }
        if(n=="conduit" && progress>0) {v.x=.5f+(v.x-.5f)*4/3;v.y=.5f+(v.y-.5f)*4/3;v.z=.5f+(v.z-.5f)*4/3;v=turn(v,float(client.state.tick)*.025f);v.y+=.15f+std::sin(float(client.state.tick)*.1f)*.1f;}
        v=turn(v,yaw);
        if(n.find("shulker_box")!=std::string::npos) {
            float x=v.x-.5f,y=v.y-.5f,z=v.z-.5f;
            if(m->properties.find("facing=down")!=std::string::npos){v.y=.5f-y;v.z=.5f-z;}
            else if(m->properties.find("facing=north")!=std::string::npos){v.y=.5f+z;v.z=.5f-y;}
            else if(m->properties.find("facing=south")!=std::string::npos){v.y=.5f-z;v.z=.5f+y;}
            else if(m->properties.find("facing=east")!=std::string::npos){v.x=.5f+y;v.y=.5f-x;}
            else if(m->properties.find("facing=west")!=std::string::npos){v.x=.5f-y;v.y=.5f+x;}
        }
        return v;
    };
    for(size_t i=0;i<m->faces.size();i++) {
        const auto& f=m->faces[i];MeshVertex vertices[4];for(int j=0;j<4;j++)vertices[j]=transform(f.vertex[j],i);
        if(n=="conduit" && progress>0) {for(auto& v:vertices){v.u*=4.f/3;v.v*=4.f/3;}}
        quad(camera,vertices,assets.texture(n=="conduit" && progress>0?"entity/conduit/cage":f.texture),base,scale,tint(*m,f));
        if(banner && i<6 && state) {
            std::istringstream layers(state->patterns);std::string layer;unsigned depth=0;
            while(std::getline(layers,layer,',') && depth<17) {
                auto colon=layer.find(':');if(colon==std::string::npos)continue;
                auto* texture=assets.texture("entity/banner/"+layer.substr(0,colon));if(!texture)continue;
                MeshVertex overlay[4];for(int j=0;j<4;j++){overlay[j]=f.vertex[j];overlay[j].x=.5f+(overlay[j].x-.5f)*(1+.0008f*(depth+1));overlay[j].z=.5f+(overlay[j].z-.5f)*(1+.004f*(depth+1));overlay[j]=transform(overlay[j],i);}
                quad(camera,overlay,texture,base,scale,0xff000000u|unsigned(std::strtoul(layer.c_str()+colon+1,nullptr,10)));++depth;
            }
        }
    }
    if(n=="conduit" && progress>0) {
        const float uv[6][4]={{0,16,16,32},{16,16,32,32},{32,16,48,32},{48,16,64,32},{16,0,32,16},{32,0,48,16}};
        for(size_t i=0;i<m->faces.size() && i<6;i++) {MeshVertex wind[4];for(int j=0;j<4;j++){wind[j]=m->faces[i].vertex[j];wind[j].x=.5f+(wind[j].x-.5f)*3;wind[j].y=.5f+(wind[j].y-.5f)*3;wind[j].z=.5f+(wind[j].z-.5f)*3;wind[j]=turn(wind[j],float(client.state.tick)*.01f);wind[j].u=uv[i][j==0 || j==3?0:2]/4;wind[j].v=uv[i][j<2?1:3]/4;}
            quad(camera,wind,assets.texture((client.state.tick/66)%2?"entity/conduit/wind":"entity/conduit/wind_vertical"),base,scale);
        }
        float y=.65f+std::sin(float(client.state.tick)*.1f)*.1f;MeshVertex eye[4]={{.3125f,y-.1875f,.51f,0,16},{.6875f,y-.1875f,.51f,16,16},{.6875f,y+.1875f,.51f,16,0},{.3125f,y+.1875f,.51f,0,0}};
        Vec viewer=position(local());float angle=std::atan2(-(viewer.x-base.x-50),viewer.z-base.z-50);for(auto& v:eye)v=turn(v,angle);
        quad(camera,eye,assets.texture(state->glow?"entity/conduit/open_eye":"entity/conduit/closed_eye"),base,scale);
    }
    if(sign && state) {
        auto* font=assets.texture("font/ascii");if(!font)return;
        std::istringstream lines(state->text),styleLines(state->styles);std::string line;int row=0;float z=wall?4.f/48+.002f:26.f/48+.002f;
        while(row<4 && std::getline(lines,line)) {
            std::string styleLine;std::getline(styleLines,styleLine,';');std::istringstream styles(styleLine);
            // Fit the vanilla four-line text region to the board, keeping short lines legible.
            std::vector<unsigned> codes;for(size_t k=0;k<line.size();) {unsigned c=(unsigned char)line[k++];int continuation=c<128?0:(c&224)==192?1:(c&240)==224?2:(c&248)==240?3:0;if(continuation){c&=(1u<<(6-continuation))-1;for(int j=0;j<continuation && k<line.size();j++)c=(c<<6)|((unsigned char)line[k++]&63);}codes.push_back(c);}
            float step=std::min(.055f,.9f/std::max(size_t(1),codes.size())),left=.5f-codes.size()*step/2,top=.88f-row*.115f;
            for(unsigned c:codes) {
                std::string glyphStyle;std::getline(styles,glyphStyle,',');auto colon=glyphStyle.find(':');unsigned ink=state->color,flags=0;
                if(colon!=std::string::npos){ink=unsigned(std::strtoul(glyphStyle.c_str(),nullptr,16));flags=unsigned(std::strtoul(glyphStyle.c_str()+colon+1,nullptr,10));}
                if((flags&16) && c!=' ')c='A'+(unsigned(client.state.tick/2)+c)%26;
                const Texture* glyphFont=font;if(c>=128){char page[40];std::snprintf(page,sizeof(page),"font/unicode_page_%02x",c>>8);glyphFont=assets.texture(page);if(!glyphFont){glyphFont=font;c='?';}else c&=255;}
                float u=float(c%16),v=float(c/16);MeshVertex glyph[4]={{left,top-.105f,z,u,v+1},{left+step,top-.105f,z,u+1,v+1},{left+step,top,z,u+1,v},{left,top,z,u,v}};
                if(flags&2){glyph[2].x+=.015f;glyph[3].x+=.015f;glyph[0].x-=.015f;glyph[1].x-=.015f;}
                for(auto& vertex:glyph)vertex=turn(vertex,yaw);
                uint32_t color=0xff000000u|ink;if(!state->glow)color=0xff000000u|((color&0xfefefeu)>>1);
                quad(camera,glyph,glyphFont,base,scale,color);
                if(flags&1){MeshVertex bold[4];for(int j=0;j<4;j++){bold[j]=turn(glyph[j],-yaw);bold[j].x+=.008f;bold[j].z+=.0001f;bold[j]=turn(bold[j],yaw);}quad(camera,bold,glyphFont,base,scale,color);}
                for(unsigned bit:{4u,8u})if(flags&bit){float y=top-(bit==4?.112f:.055f);MeshVertex rule[4]={{left,y-.006f,z+.0002f,0,16},{left+step,y-.006f,z+.0002f,16,16},{left+step,y,z+.0002f,16,0},{left,y,z+.0002f,0,0}};for(auto& v:rule)v=turn(v,yaw);quad(camera,rule,assets.texture("block/white_concrete"),base,scale,color);}
                left+=step;
            }++row;
        }
    }
}
