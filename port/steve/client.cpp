#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "client.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
namespace steve {
namespace {
struct SocketApi {
    HMODULE library{};
    decltype(&WSAStartup) startup{};decltype(&WSACleanup) cleanup{};
    decltype(&::socket) create{};decltype(&closesocket) destroy{};
    decltype(&ioctlsocket) control{};decltype(&connect) connect_{};
    decltype(&select) select_{};decltype(&::send) send_{};decltype(&recv) recv_{};
    decltype(&WSAGetLastError) error{};
    bool load() {
        if(library) return startup && cleanup && create && destroy && control && connect_ && select_ && send_ && recv_ && error;
        // Load only after io_init's TLS reservation of the DS address windows.
        library=LoadLibraryExA("ws2_32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library) return false;
#define STEVE_SOCK(field,name) field=reinterpret_cast<decltype(field)>(GetProcAddress(library,name))
        STEVE_SOCK(startup,"WSAStartup");STEVE_SOCK(cleanup,"WSACleanup");STEVE_SOCK(create,"socket");STEVE_SOCK(destroy,"closesocket");
        STEVE_SOCK(control,"ioctlsocket");STEVE_SOCK(connect_,"connect");STEVE_SOCK(select_,"select");STEVE_SOCK(send_,"send");STEVE_SOCK(recv_,"recv");STEVE_SOCK(error,"WSAGetLastError");
#undef STEVE_SOCK
        return startup && cleanup && create && destroy && control && connect_ && select_ && send_ && recv_ && error;
    }
} sockets;
}
Client::Client():socket(INVALID_SOCKET) {}
Client::~Client() {close(); if(started) sockets.cleanup();}
void Client::close() {
    if(socket!=INVALID_SOCKET) sockets.destroy(static_cast<SOCKET>(socket));
    socket=INVALID_SOCKET; connecting=false; incoming.clear(); outgoing.clear(); written=0;
    lastPacket=0; nextRetry=GetTickCount64()+1000;
}
bool Client::fresh() const {return lastPacket && GetTickCount64()-lastPacket<2000;}
void Client::packet(const std::string& text) {
    if(text.size()>8192 || outgoing.size()>65536) return;
    uint32_t n=static_cast<uint32_t>(text.size());
    size_t start=outgoing.size(); outgoing.resize(start+4+n);
    std::memcpy(outgoing.data()+start,&n,4); std::memcpy(outgoing.data()+start+4,text.data(),n);
}
bool Client::send(const std::string& verb) {
    if(socket==INVALID_SOCKET || connecting || !fresh()) return false;
    packet("1\t"+std::to_string(++sequence)+"\t"+verb); return true;
}
static std::vector<std::string> fields(const std::string& text) {
    std::vector<std::string> out; size_t begin=0;
    while(begin<=text.size()) {
        size_t end=text.find('\t',begin);
        out.push_back(text.substr(begin,end==std::string::npos?end:end-begin));
        if(end==std::string::npos) break; begin=end+1;
    }
    return out;
}
void Client::message(const std::string& text) {
    ServerState next=state; next.entities.clear();next.moving.clear();next.visuals.clear();next.menuSlots.clear(); next.containerSize=0;
    bool valid=false;
    std::istringstream lines(text); std::string line;
    auto integer=[](const std::string& s){return std::strtol(s.c_str(),nullptr,10);};
    auto real=[](const std::string& s){return static_cast<float>(std::strtod(s.c_str(),nullptr));};
    while(std::getline(lines,line)) {
        auto p=fields(line); if(p.empty()) continue;
        if(p[0]=="S" && p.size()==16 && p[1]=="1") {
            next.tick=std::strtoull(p[2].c_str(),nullptr,10); next.ack=std::strtoull(p[3].c_str(),nullptr,10);
            if(next.token!=integer(p[4])) {blocks.clear();terrainOriginal.clear();edits.clear();next.pushedX=next.pushedY=next.pushedZ=0;}
            next.token=static_cast<int>(integer(p[4])); next.x=real(p[5]); next.y=real(p[6]); next.z=real(p[7]);
            next.yaw=real(p[8]); next.pitch=real(p[9]); next.health=real(p[10]);
            next.air=static_cast<int>(integer(p[11])); next.food=static_cast<int>(integer(p[12]));
            next.flags=static_cast<int>(integer(p[13])); next.mining=static_cast<int>(integer(p[14])); next.selected=static_cast<int>(integer(p[15]));
            valid=true;
        } else if((p[0]=="I" || p[0]=="C" || p[0]=="H") && p.size()==6) {
            int index=static_cast<int>(integer(p[1]));
            ItemState item{static_cast<unsigned>(integer(p[2])),static_cast<unsigned>(integer(p[3])),static_cast<unsigned>(integer(p[4])),static_cast<unsigned>(integer(p[5]))};
            if(p[0]=="I" && index>=0 && index<36) next.inventory[index]=item;
            if(p[0]=="C" && index>=0 && index<128) {next.container[index]=item; next.containerSize=std::max(next.containerSize,static_cast<unsigned>(index+1));}
            if(p[0]=="H") next.carried=item;
        } else if(p[0]=="G" && p.size()==2) {
            next.craftingWidth=unsigned(std::atoi(p[1].c_str()));
        } else if(p[0]=="L" && p.size()==4 && next.menuSlots.size()<128) {
            next.menuSlots.push_back({int(integer(p[1])),int(integer(p[2])),int(integer(p[3]))});
        } else if(p[0]=="V" && (p.size()==10 || p.size()==11) && next.visuals.size()<512) {
            BlockVisual v;v.at={int(integer(p[1])),int(integer(p[2])),int(integer(p[3]))};v.kind=p[4];v.progress=real(p[5]);v.color=unsigned(integer(p[6]));v.glow=unsigned(integer(p[7]));v.patterns=p[9];if(p.size()==11)v.styles=p[10];
            if(p[8]!="-") {unsigned value=0;int bits=0;const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";for(char c:p[8]) {auto at=alphabet.find(c);if(at==std::string::npos)break;value=(value<<6)|unsigned(at);bits+=6;if(bits>=8){bits-=8;v.text+=char((value>>bits)&255);}}}
            next.visuals.push_back(std::move(v));
        } else if(p[0]=="D" && p.size()==4) {
            next.pushedX=real(p[1]);next.pushedY=real(p[2]);next.pushedZ=real(p[3]);
        } else if(p[0]=="B" && p.size()==5) {
            Cell c{static_cast<int>(integer(p[1])),static_cast<int>(integer(p[2])),static_cast<int>(integer(p[3]))};
            unsigned id=static_cast<unsigned>(integer(p[4]));
            if(blocks.size()<65536 || blocks.count(c)) blocks[c]=id;
            if(edits.size()<65536) edits.push_back(c);
        } else if(p[0]=="T" && p.size()==5) {
            Cell c{static_cast<int>(integer(p[1])),static_cast<int>(integer(p[2])),static_cast<int>(integer(p[3]))};
            if(terrainOriginal.size()<65536 || terrainOriginal.count(c)) terrainOriginal[c]=static_cast<unsigned>(integer(p[4]));
        } else if(p[0]=="P" && p.size()==8 && next.moving.size()<256) {
            next.moving.push_back({{int(integer(p[1])),int(integer(p[2])),int(integer(p[3]))},unsigned(integer(p[4])),real(p[5]),real(p[6]),real(p[7])});
        } else if(p[0]=="E" && (p.size()==11 || p.size()==13) && next.entities.size()<128) {
            EntityState e;
            e.id=static_cast<int>(integer(p[1])); e.type=static_cast<int>(integer(p[2]));
            e.x=real(p[3]);e.y=real(p[4]);e.z=real(p[5]);e.yaw=real(p[6]);e.pitch=real(p[7]);
            e.health=real(p[8]);e.fuse=static_cast<int>(integer(p[9]));e.block=static_cast<int>(integer(p[10]));if(p.size()==13) {e.item=static_cast<int>(integer(p[11]));e.count=static_cast<int>(integer(p[12]));}next.entities.push_back(e);
        } else if(p[0]=="R" && p.size()==2) {
            static std::string previous;
            if(p[1]!=previous) {
                previous=p[1];diagnostic.clear();unsigned value=0;int bits=0;
                const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                for(char c:p[1]) {auto at=alphabet.find(c);if(at==std::string::npos) break;value=(value<<6)|unsigned(at);bits+=6;if(bits>=8) {bits-=8;diagnostic+=char((value>>bits)&255);}}
                auto colon=diagnostic.find(": ");if(colon!=std::string::npos) diagnostic.erase(0,colon+2);
                colon=diagnostic.find(": ");if(colon!=std::string::npos) diagnostic.erase(0,colon+2);
                diagnosticUntil=GetTickCount64()+6000;
                std::fprintf(stderr,"[steve] %s\n",diagnostic.c_str());
            }
        }
    }
    if(valid) {state=std::move(next); sequence=std::max(sequence,state.ack); lastPacket=GetTickCount64();}
}
void Client::poll(const std::string& root) {
    if(!started) {WSADATA data{}; started=sockets.load() && sockets.startup(MAKEWORD(2,2),&data)==0; if(!started) return;}
    if(socket==INVALID_SOCKET) {
        if(GetTickCount64()<nextRetry) return;
        std::ifstream file(root+"/runtime/minecraft/ipc-token.txt"); std::string token; std::getline(file,token);
        while(!token.empty() && (token.back()=='\r'||token.back()=='\n')) token.pop_back();
        if(token.size()!=64) {nextRetry=GetTickCount64()+1000; return;}
        SOCKET s=sockets.create(AF_INET,SOCK_STREAM,IPPROTO_TCP); if(s==INVALID_SOCKET) {nextRetry=GetTickCount64()+1000;return;}
        socket=static_cast<uintptr_t>(s); u_long mode=1; sockets.control(s,FIONBIO,&mode);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=static_cast<unsigned short>((25576u>>8)|(25576u<<8));address.sin_addr.s_addr=0x0100007fu;
        int result=sockets.connect_(s,reinterpret_cast<sockaddr*>(&address),sizeof(address));
        if(result==SOCKET_ERROR && sockets.error()!=WSAEWOULDBLOCK) {close();return;}
        connecting=result!=0; packet("SM64DS1\t"+token);
    }
    SOCKET s=static_cast<SOCKET>(socket);
    if(connecting) {
        fd_set write,error; FD_ZERO(&write);FD_ZERO(&error);FD_SET(s,&write);FD_SET(s,&error);timeval zero{};
        if(sockets.select_(0,nullptr,&write,&error,&zero)<0 || error.fd_count) {close();return;}
        if(!write.fd_count) return;
        connecting=false;
    }
    while(written<outgoing.size()) {
        int n=sockets.send_(s,reinterpret_cast<const char*>(outgoing.data()+written),static_cast<int>(outgoing.size()-written),0);
        if(n==SOCKET_ERROR) {if(sockets.error()!=WSAEWOULDBLOCK) close();break;}
        if(n<=0) {close();return;} written+=n;
    }
    if(written==outgoing.size()) {written=0;outgoing.clear();}
    if(socket==INVALID_SOCKET) return;
    char buffer[16384];
    for(int packets=0;packets<32;packets++) {
        int n=sockets.recv_(s,buffer,sizeof(buffer),0);
        if(n==0) {close();return;}
        if(n<0) {if(sockets.error()!=WSAEWOULDBLOCK) close();break;}
        if(incoming.size()+n>4*1024*1024) {close();return;}
        incoming.insert(incoming.end(),buffer,buffer+n);
    }
    size_t offset=0;
    while(incoming.size()-offset>=4) {
        uint32_t length;std::memcpy(&length,incoming.data()+offset,4);
        if(length>1024*1024 || length==0) {close();return;}
        if(incoming.size()-offset<length+4) break;
        message(std::string(reinterpret_cast<char*>(incoming.data()+offset+4),length));offset+=length+4;
    }
    if(offset) incoming.erase(incoming.begin(),incoming.begin()+offset);
}
}
