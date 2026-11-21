#pragma once
#include "MovementRecovery.h"
#include "ReferenceCode.h"

namespace BetterCiv::TradeTransit {
// Native delivery phases: 0 approach home, 1 load, 2 leave home,
// 3 approach partner, 4 unload, 5 leave partner. Never advance a phase here.
inline bool Travelling(int phase) { return phase==0 || phase==2 || phase==3 || phase==5; }
struct Record {
    uint64_t key=0,last_seen=0,target=0,wait_until=0,next_log=0,phase_seen=0;
    uint32_t owner=0;
    int phase=-1;
    unsigned attempts=0;
    Math::Vector3 anchor{};
    bool waiting=false;
    Recovery::Sample retried{};
    uint64_t diagnostic_hash=0,next_diagnostic=0;
    void Observe(uint64_t next_target,uint32_t next_owner,const Math::Vector3& position,uint64_t now) {
        if(target!=next_target || owner!=next_owner || now<last_seen ||
            (Recovery::Finite(position) && Recovery::Distance(position,anchor)>100)) {
            target=next_target;owner=next_owner;anchor=position;attempts=0;waiting=false;
        }
        last_seen=now;
    }
};
struct State {
    Record records[512];
    unsigned lines=0;
    void ResetSession() { for(auto& r:records)r={}; }
    Record* Find(uint64_t key,uint64_t now) {
        for(auto& r:records)if(r.key==key)return &r;
        for(auto& r:records)if(!r.key || (now>=r.last_seen && now-r.last_seen>60000)) {
            r={};r.key=key;r.last_seen=now;return &r;
        }
        return nullptr;
    }
};
inline bool ValidateNative(const unsigned char* base,uint32_t delta) {
    if(!base)return false;
    const struct {uint32_t address,size,hash;} guards[]={
        {0xdcc7c0,1444,0x70325dee}, // Complete trade update, including its return convention.
        {0xdccd64,24,0x9d27a6c9},  // Six phase dispatch addresses.
        {0xca73e0,69,0x4c7f38ae},  // Native diagnostic string at Vehicle+D14.
        {0xc42f80,896,0x8fc22c5d}, // Arrival checks use the current local request.
    };
    for(const auto& g:guards) {
        uint32_t hash;
        if(!Native::HashReferenceCode(base+g.address-0x400000,g.size,delta,
            Native::CodeRelocations(g.address),hash)||hash!=g.hash)return false;
    }
    return true;
}
}
