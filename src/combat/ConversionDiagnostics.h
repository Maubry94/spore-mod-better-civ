#pragma once
#include <cstdint>
#include <cmath>
#include <Spore/Simulator/cCity.h>

namespace BetterCiv::Conversion {
constexpr unsigned kMaxSlots = 8;
constexpr unsigned kMaxCities = 16;
constexpr unsigned kMaxLines = 8192;
constexpr uint64_t kSampleMs = 1000, kReportMs = 10000;

// Values only: no engine pointers, owning references or serialized state.
struct Slot {
    uint32_t id = UINT32_MAX, attacker = UINT32_MAX;
    int state = -1;
    unsigned registered = 0, reservations = 0;
    int listening = 0, converted = 0, listening_capacity = 0, converted_capacity = 0;
};
struct Snapshot {
    uint32_t city = UINT32_MAX, owner = UINT32_MAX, credited = UINT32_MAX;
    int specialty = -1;
    bool in_view = false, has_view = false;
    float distance = 0, progress = 0;
    unsigned targets = 0, buckets = 0, disabled = 0, ready = 0, active = 0, registered = 0;
    unsigned ordered = 0, nearby = 0, cultural_aim = 0, turret_aim = 0, other_aim = 0, no_aim = 0;
    unsigned turrets = 0, frozen = 0;
    Slot slots[kMaxSlots];
    bool Interesting() const { return ordered || registered || active || progress > 0; }
};

inline Snapshot ReadCity(const Simulator::cCity& city)
{
    Snapshot s;
    s.city = city.mID;
    s.owner = city.mPoliticalID;
    s.credited = city.mConvertID;
    s.specialty = city.mVehicleSpecialty;
    s.in_view = city.mbInView;
    s.has_view = city.mpView != nullptr;
    s.distance = city.mDistanceFromCamera;
    s.progress = city.mConvertPercent;
    s.targets = unsigned(city.mCulturalTargets.size());
    s.buckets = unsigned(city.mCultureTargetInfo.size());
    for (unsigned i = 0; i < s.targets && i < kMaxSlots; ++i) {
        const auto* target = city.mCulturalTargets[i].get();
        if (!target) continue;
        auto& slot = s.slots[i];
        slot.id = target->mID;
        slot.state = target->mAttackState;
        slot.attacker = target->mAttackerPoliticalID;
        slot.registered = unsigned(target->mAttackers.size());
        slot.reservations = unsigned(target->field_1E0.size());
        s.registered += slot.registered;
        s.disabled += slot.state == 0;
        s.ready += slot.state == 1;
        s.active += slot.state == 2;
        if (i < s.buckets) {
            const auto& info = city.mCultureTargetInfo[i];
            slot.listening = info.mBucketPopulation[0];
            slot.converted = info.mBucketPopulation[1];
            slot.listening_capacity = info.mBucketMaxPopulation[0];
            slot.converted_capacity = info.mBucketMaxPopulation[1];
        }
    }
    for (const auto& turret : city.mTurrets) {
        if (!turret || turret->mbIsDestroyed || turret->mHealthPoints <= 0) continue;
        ++s.turrets;
        s.frozen += turret->mFreezeCount > 0;
    }
    return s;
}

// Inspect the current combat target, not visual effects. An aim is not proof
// that a shot or conversion tick happened. Call only for matched city orders.
inline void CountAim(Snapshot& s, const Simulator::cCity& city, const Simulator::cVehicle& vehicle)
{
    const auto* aim = vehicle.mpTarget.get();
    if (!aim) { ++s.no_aim; return; }
    for (const auto& target : city.mCulturalTargets)
        if (target && aim == static_cast<const Simulator::cCombatant*>(target.get())) {
            ++s.cultural_aim; return;
        }
    for (const auto& turret : city.mTurrets)
        if (turret && aim == static_cast<const Simulator::cCombatant*>(turret.get())) {
            ++s.turret_aim; return;
        }
    ++s.other_aim;
}

struct Record {
    uint64_t key = 0, next_report = 0, last_seen = 0;
    uint32_t owner = UINT32_MAX;
    bool seen = false, in_view = false, interesting = false;
    unsigned active = 0, registered = 0;
    float progress = 0;
    bool Observe(const Snapshot& s, uint64_t now) {
        const bool current = s.Interesting();
        const bool transition = !seen || s.owner != owner ||
            ((current || interesting) && (s.in_view != in_view || current != interesting ||
             s.active != active || (s.registered == 0) != (registered == 0) ||
             (s.progress > 0) != (progress > 0)));
        const bool report = transition || (current && now >= next_report);
        seen = true; last_seen = now; owner = s.owner; in_view = s.in_view;
        interesting = current; active = s.active; registered = s.registered; progress = s.progress;
        if (report) next_report = now + kReportMs;
        return report;
    }
};
struct Journal {
    Record records[kMaxCities];
    uint64_t next_sample = 0;
    unsigned lines = 0;
    bool limit_notified = false;
    // Keep the per-launch line budget across save and mode lifecycle resets.
    void ResetSession() { for (auto& r : records) r = {}; next_sample = 0; }
    bool SampleDue(bool enabled, bool ready, bool civ_mode, int delta, uint64_t now) {
        if (!enabled || !ready || !civ_mode || delta <= 0 || now < next_sample || lines >= kMaxLines) return false;
        next_sample = now + kSampleMs;
        return true;
    }
    Record& For(uint64_t key, uint64_t now) {
        Record* oldest = &records[0];
        for (auto& r : records) {
            if (r.key == key) return r;
            if (!r.key) { r.key = key; r.last_seen = now; return r; }
            if (r.last_seen < oldest->last_seen) oldest = &r;
        }
        *oldest = {}; oldest->key = key; oldest->last_seen = now;
        return *oldest;
    }
    bool Reserve(unsigned count) {
        if (count > kMaxLines - lines) return false;
        lines += count;
        return true;
    }
};
} // namespace BetterCiv::Conversion
