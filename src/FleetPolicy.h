#pragma once
#include <cstdint>
namespace BetterCiv::Fleet {
enum class Reason { None, NoPort, LandSurplus, ReclaimPort };
enum class Veto { Eligible, PlayerOrInvalid, Busy, Moving, NotCandidate, LowPressure,
    MapUnavailable, NotLand, HostileLand, NoBuilderPort, NoEnemyPort, EnoughShips, Garrison, Count };
inline const char* VetoName(Veto v) {
    const char* names[]={"eligible","player-or-invalid","combat","moving-or-assigned","not-candidate",
        "low-pressure","map-unavailable","not-land","hostile-land","no-builder-port","no-enemy-port",
        "enough-ships","garrison"};
    return names[unsigned(v)];
}
struct Facts {
    bool player=false, alive=true, idle=false, safe=false, no_port=false;
    bool sea_target=false, hostile_land=true, suitable_port=false;
    bool ship=false, combat_land=false;
    unsigned total=0, local_ground=0, local_cities=0, sea_units=0;
};
inline Reason Choose(const Facts& f,bool stranded,bool surplus) {
    if (f.player || !f.alive || !f.idle || !f.safe) return Reason::None;
    if (stranded && f.ship && f.no_port) return Reason::NoPort;
    if (surplus && f.combat_land && f.total>=32 && f.suitable_port &&
        f.sea_target && !f.hostile_land && f.sea_units<6 &&
        f.local_ground> (f.local_cities>1?f.local_cities*6:6)) return Reason::LandSurplus;
    return Reason::None;
}
struct Record {
    uint64_t key=0, owner=0, target=0, since=0, last_seen=0;
    unsigned reclaim_attempts=0;
    uint64_t next_reclaim=0;
    int region=-1;
    Reason reason=Reason::None;
    bool Observe(uint64_t new_owner,uint64_t new_target,Reason next,uint64_t now,int new_region=-1) {
        const bool gap=now<last_seen || now-last_seen>6000;
        // Grace measures continuously idle surplus on one peaceful landmass,
        // not loyalty to one particular overseas city. Opportunity is checked
        // again before every sale. Moving to another landmass resets it.
        if (gap || next!=reason || new_owner!=owner || new_region!=region || next==Reason::None) since=now;
        if(new_owner!=owner){reclaim_attempts=0;next_reclaim=0;}
        reason=next; owner=new_owner; target=new_target; region=new_region; last_seen=now;
        return next!=Reason::None && now-since>=(next==Reason::ReclaimPort?30000u:next==Reason::NoPort?120000u:180000u);
    }
};
struct State {
    Record units[512];
    struct Nation {uint64_t key=0,next_sale=0,next_review=0;} nations[32];
    uint64_t next_sample=0;
    unsigned lines=0;
    void ResetSession() { for (auto& r:units) r={}; for(auto& n:nations)n={}; next_sample=0; }
    Record* Find(uint64_t key,uint64_t now) {
        for(auto& r:units) if(r.key==key)return &r;
        for(auto& r:units) if(!r.key || now-r.last_seen>60000) {r={};r.key=key;return &r;}
        return nullptr;
    }
    Nation* FindNation(uint64_t key) {
        for(auto& n:nations)if(n.key==key)return &n;
        for(auto& n:nations)if(!n.key){n.key=key;return &n;}
        return nullptr;
    }
};
}
