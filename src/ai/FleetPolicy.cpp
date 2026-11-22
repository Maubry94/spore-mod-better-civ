#include "FleetPolicy.h"
#include <iterator>

namespace BetterCiv::Fleet {

const char* VetoName(Veto veto)
{
    static constexpr const char* names[] = {
        "eligible", "player-or-invalid", "combat", "moving-or-assigned", "not-candidate",
        "low-pressure", "map-unavailable", "not-land", "hostile-land", "no-builder-port",
        "no-enemy-port", "enough-ships", "garrison"
    };
    static_assert(std::size(names) == static_cast<unsigned>(Veto::Count));
    const auto index = static_cast<unsigned>(veto);
    return index < std::size(names) ? names[index] : "unknown";
}

unsigned GroundReserve(unsigned cities)
{
    return cities * 3;
}

Reason Choose(const Facts& facts, bool retire_stranded, bool sell_surplus)
{
    if (facts.player || !facts.alive || !facts.idle || !facts.safe)
        return Reason::None;

    if (retire_stranded && facts.ship && facts.no_port)
        return Reason::NoPort;

    if (sell_surplus && facts.combat_land && facts.total >= 32 && facts.suitable_port &&
        facts.sea_target && !facts.hostile_land && facts.sea_units < 6 &&
        facts.local_ground > GroundReserve(facts.local_cities))
        return Reason::LandSurplus;

    return Reason::None;
}

static uint64_t GracePeriodMs(Reason reason)
{
    switch (reason) {
    case Reason::ReclaimPort:      return 30000;
    case Reason::MerchantSurplus:
    case Reason::StrandedMerchant:
    case Reason::NoPort:          return 120000;
    case Reason::LandSurplus:     return 180000;
    case Reason::None:            return 0;
    }
    return 0;
}

bool Record::Observe(uint64_t new_owner, uint64_t new_target, Reason next, uint64_t now, int new_region)
{
    const bool gap = now < last_seen || now - last_seen > 6000;
    // Another overseas target can become preferable while the same troops remain idle.
    // Only a change of eligibility, owner, landmass or a break in observation restarts grace.
    if (gap || next != reason || new_owner != owner || new_region != region || next == Reason::None)
        since = now;

    if (new_owner != owner) {
        reclaim_attempts = 0;
        next_reclaim = 0;
    }
    reason = next;
    owner = new_owner;
    target = new_target;
    region = new_region;
    last_seen = now;
    return next != Reason::None && now - since >= GracePeriodMs(next);
}

void State::ResetSession()
{
    for (auto& unit : units)
        unit = {};
    for (auto& nation : nations)
        nation = {};
    next_sample = 0;
}

Record* State::Find(uint64_t key, uint64_t now)
{
    for (auto& unit : units)
        if (unit.key == key)
            return &unit;

    for (auto& unit : units) {
        if (!unit.key || now < unit.last_seen || now - unit.last_seen > 60000) {
            unit = {};
            unit.key = key;
            return &unit;
        }
    }
    return nullptr;
}

State::Nation* State::FindNation(uint64_t key)
{
    for (auto& nation : nations)
        if (nation.key == key)
            return &nation;

    for (auto& nation : nations) {
        if (!nation.key) {
            nation.key = key;
            return &nation;
        }
    }
    return nullptr;
}
} // namespace BetterCiv::Fleet
