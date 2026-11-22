#pragma once
#include <cmath>
#include <cstdint>
#include <Spore/Simulator/cCivilization.h>

namespace BetterCiv::CombatDiagnostics {
constexpr unsigned kMaxUnits = 512, kMaxCivs = 32, kMaxCities = 16;
constexpr unsigned kMaxLines = 16384;
constexpr uint64_t kSampleMs = 5000, kReportMs = 30000, kStationaryMs = 10000;

enum class Kind { None, Unknown, City, Turret, Culture, Vehicle, Building };
inline const char* Name(Kind k) {
    constexpr const char* names[] = {"none", "unresolved", "city", "turret", "culture", "vehicle", "building"};
    return names[unsigned(k)];
}
struct Target {
    Kind kind = Kind::None;
    uint32_t id = UINT32_MAX, city = UINT32_MAX;
    Math::Vector3 position = {};
    bool Known() const { return kind != Kind::None && kind != Kind::Unknown; }
};
inline float Distance(const Math::Vector3& a, const Math::Vector3& b) {
    const auto d = a - b;
    const float squared = d.x*d.x + d.y*d.y + d.z*d.z;
    return std::isfinite(squared) && squared >= 0 ? std::sqrt(squared) : -1.0f;
}

// Unknown pointers are compared, never cast/dereferenced. Only objects reached
// through the live civilization collections are read. No pointer is retained.
template<class Civs>
Target Resolve(const void* pointer, bool combatant, const Civs& civs) {
    if (!pointer) return {};
    auto match = [=](const auto* object) {
        return pointer == (combatant ? static_cast<const void*>(static_cast<const Simulator::cCombatant*>(object))
                                   : static_cast<const void*>(static_cast<const Simulator::cGameData*>(object)));
    };
    for (const auto& civ : civs) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& city : civ->mCities) {
            if (!city || city->mbIsDestroyed) continue;
            // A city has no cCombatant base; city attacks use its subtargets.
            if (!combatant && pointer == static_cast<const Simulator::cGameData*>(city.get()))
                return {Kind::City, city->mID, city->mID, city->mPosition};
            for (const auto& building : city->mBuildings)
                if (building && !building->mbIsDestroyed && match(building.get()))
                    return {Kind::Building, building->mID, city->mID, building->mPosition};
            for (const auto& turret : city->mTurrets)
                if (turret && !turret->mbIsDestroyed && match(turret.get()))
                    return {Kind::Turret, turret->mID, city->mID, turret->mPosition};
            for (const auto& culture : city->mCulturalTargets)
                if (culture && !culture->mbIsDestroyed && match(culture.get()))
                    return {Kind::Culture, culture->mID, city->mID, culture->mPosition};
        }
        for (const auto& unit : civ->mVehicles)
            if (unit && !unit->mbIsDestroyed && !unit->mbDead && match(unit.get()))
                return {Kind::Vehicle, unit->mID, UINT32_MAX, unit->mPosition};
    }
    Target unknown; unknown.kind = Kind::Unknown; return unknown;
}

struct UnitSnapshot {
    uint32_t id = UINT32_MAX, owner = UINT32_MAX, near_city = UINT32_MAX;
    int purpose = -1, locomotion = -1, order = -1;
    Target order_target, aim;
    Math::Vector3 position = {}, velocity = {};
    float desired_speed = 0, health = 0, since_fire_ms = 0;
    unsigned hostiles = 0;
    bool nearby_city_under_attack = false;
    bool Interesting() const {
        return order > 0 || aim.kind != Kind::None || hostiles || nearby_city_under_attack;
    }
};
inline UnitSnapshot ReadUnit(const Simulator::cVehicle& unit, uint32_t owner) {
    UnitSnapshot s;
    s.id = unit.mID; s.owner = owner;
    s.purpose = int(unit.mPurpose); s.locomotion = int(unit.mLocomotion);
    s.position = unit.mPosition; s.velocity = unit.mVelocity;
    s.desired_speed = unit.mDesiredSpeed; s.health = unit.mHealthPoints;
    s.since_fire_ms = float(unit.mTimeSinceLastWeaponFireMS);
    s.hostiles = unsigned(unit.mHostileUnits.size());
    return s;
}
struct UnitRecord {
    uint64_t key = 0, last_seen = 0, anchor_since = 0, next_report = 0;
    Math::Vector3 anchor = {};
    uint32_t owner = UINT32_MAX, order_id = UINT32_MAX, aim_id = UINT32_MAX;
    Kind order_kind = Kind::None, aim_kind = Kind::None;
    int order = -1;
    bool seen = false, interesting = false, stationary = false;
    uint64_t stationary_ms = 0;
    bool Observe(const UnitSnapshot& s, uint64_t now) {
        // Physical immobility must survive aim changes: the observed religious
        // aircraft alternated between two buildings for more than 30 minutes.
        const bool changed = !seen || now < last_seen || s.owner != owner || s.order != order ||
            s.order_target.id != order_id || s.order_target.kind != order_kind;
        const float moved = Distance(s.position, anchor);
        if (changed || moved < 0 || moved > 0.25f) { anchor = s.position; anchor_since = now; }
        stationary_ms = now - anchor_since;
        const bool current_stationary = stationary_ms >= kStationaryMs;
        const bool current = s.Interesting();
        // Stationary is an observation, not a stuck verdict: firing, preaching
        // and idle defense can all legitimately stay in one location.
        const bool report = current && (!interesting || now >= next_report ||
            (current_stationary && !stationary));
        if (report) next_report = now + kReportMs;
        seen = true; last_seen = now; owner = s.owner; order = s.order;
        order_id = s.order_target.id; order_kind = s.order_target.kind;
        aim_id = s.aim.id; aim_kind = s.aim.kind;
        interesting = current; stationary = current_stationary;
        return report;
    }
};
struct CommRecord {
    uint64_t key = 0, last_seen = 0;
    uint32_t event = UINT32_MAX;
    int slot = -1;
    unsigned cities = 0;
    bool seen = false, present = false;
    bool Observe(uint32_t next_event, int next_slot, unsigned next_cities) {
        const bool changed = !seen || !present || event != next_event || slot != next_slot || cities != next_cities;
        seen = present = true; event = next_event; slot = next_slot; cities = next_cities; return changed;
    }
};
struct CityRecord { uint64_t key = 0, last_seen = 0, next_report = 0; };
struct Journal {
    UnitRecord units[kMaxUnits];
    CommRecord civs[kMaxCivs];
    CityRecord cities[kMaxCities];
    uint64_t next_sample = 0;
    unsigned lines = 0;
    bool limit_notified = false;
    void ResetSession() {
        for (auto& r : units) r = {};
        for (auto& r : civs) r = {};
        for (auto& r : cities) r = {};
        next_sample = 0;
    }
    bool SampleDue(bool enabled, bool ready, bool civ_mode, int delta, uint64_t now) {
        if (!enabled || !ready || !civ_mode || delta <= 0 || now < next_sample || lines >= kMaxLines) return false;
        next_sample = now + kSampleMs; return true;
    }
    template<class R, unsigned N>
    static R& For(R (&records)[N], uint64_t key, uint64_t now) {
        R* oldest = &records[0];
        for (auto& r : records) {
            if (r.key == key) return r;
            if (!r.key) { r.key = key; r.last_seen = now; return r; }
            if (r.last_seen < oldest->last_seen) oldest = &r;
        }
        *oldest = {}; oldest->key = key; oldest->last_seen = now; return *oldest;
    }
    bool Reserve(unsigned count = 1) {
        if (count > kMaxLines - lines) return false;
        lines += count; return true;
    }
};
} // namespace BetterCiv::CombatDiagnostics
