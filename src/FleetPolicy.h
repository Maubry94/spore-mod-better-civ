#pragma once
#include <cstdint>

namespace BetterCiv::Fleet {

enum class Reason { None, NoPort, LandSurplus, ReclaimPort, MerchantSurplus, StrandedMerchant };
enum class Veto { Eligible, PlayerOrInvalid, Busy, Moving, NotCandidate, LowPressure,
    MapUnavailable, NotLand, HostileLand, NoBuilderPort, NoEnemyPort, EnoughShips, Garrison, Count };
const char* VetoName(Veto veto);

struct Facts {
    bool player = false, alive = true, idle = false, safe = false, no_port = false;
    bool sea_target = false, hostile_land = true, suitable_port = false;
    bool ship = false, combat_land = false;
    unsigned total = 0, local_ground = 0, local_cities = 0, sea_units = 0;
};
unsigned GroundReserve(unsigned cities);
Reason Choose(const Facts& facts, bool retire_stranded, bool sell_surplus);

struct Record {
    uint64_t key = 0, owner = 0, target = 0, since = 0, last_seen = 0;
    unsigned reclaim_attempts = 0;
    uint64_t next_reclaim = 0;
    int region = -1;
    Reason reason = Reason::None;
    bool Observe(uint64_t new_owner, uint64_t new_target, Reason next, uint64_t now, int new_region = -1);
};

struct State {
    struct Nation {
        uint64_t key = 0, next_sale = 0, next_review = 0, next_veto_log = 0, next_reserve_log = 0;
    };

    Record units[512];
    Nation nations[32];
    uint64_t next_sample = 0;
    unsigned lines = 0;

    void ResetSession();
    Record* Find(uint64_t key, uint64_t now);
    Nation* FindNation(uint64_t key);
};
} // namespace BetterCiv::Fleet
