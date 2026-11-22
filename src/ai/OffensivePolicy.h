#pragma once
#include <cstdint>
#include <cmath>

namespace BetterCiv::Offensive {
// An observation-based definition, not an undocumented engine "wave ended" flag.
// Wave rotation and an optional stalled-siege timeout are independent policies.
constexpr uint64_t kQuietMs = 20000;
constexpr uint64_t kRetryMs = 60000;
constexpr uint64_t kAvoidMs = 120000;
constexpr unsigned kMinimumWave = 3;
constexpr float kProgressEpsilon = 0.0001f;
enum class Event { None, Target, Started, Progress, Failed, EndedWithProgress };
struct Observation {
    uint64_t now = 0, target = 0, territory = 0;
    uint32_t owner = 0;
    unsigned committed = 0, uncertain = 0;
    float progress = 0;
    unsigned nearby = 0;
};
struct State {
    uint64_t target = 0, territory = 0, last_time = 0, quiet_since = 0, retry_after = 0;
    uint32_t owner = 0;
    unsigned failures = 0, threshold = 2, peak_units = 0;
    float progress = 0;
    bool wave = false, made_progress = false, quiet = false, initialized = false;
    uint64_t last_advance = 0;
    bool engaged = false;

    Event Observe(const Observation& o, uint32_t seed) {
        if (!initialized || o.now < last_time || o.target != target ||
            o.owner != owner || o.territory != territory) {
            *this = {};
            initialized = true;
            target = o.target; owner = o.owner; territory = o.territory;
            progress = std::isfinite(o.progress) ? o.progress : 0;
            // Own deterministic variation; do not consume Spore's generation RNG.
            threshold = 2 + ((seed * 2654435761u) ^ uint32_t(target >> 32)) % 3;
            last_time = o.now;
            return Event::Target;
        }
        last_time = o.now;
        if (!target || !std::isfinite(o.progress)) return Event::None;
        Event event = Event::None;
        if (o.progress > progress + kProgressEpsilon) {
            failures = 0; made_progress = true; quiet = false;
            event = Event::Progress;
        }
        // A shared gauge can regress or switch credited attacker. Only a new
        // high-water mark is net progress; do not reward 20 -> 0 -> 20 loops.
        if (event == Event::Progress) {
            progress = o.progress;
            last_advance = o.now;
        }
        if (!engaged && o.nearby && o.committed >= kMinimumWave) {
            engaged = true; last_advance = o.now;
        }
        if (o.committed >= kMinimumWave && !wave) {
            wave = true;
            made_progress = event == Event::Progress;
            peak_units = o.committed;
            event = Event::Started;
        }
        if (o.committed > peak_units) peak_units = o.committed;
        if (o.committed || o.uncertain || event == Event::Progress) quiet = false;
        else if (!quiet) { quiet = true; quiet_since = o.now; }
        if (wave && quiet && o.now - quiet_since >= kQuietMs) {
            wave = false;
            if (made_progress) { failures = 0; event = Event::EndedWithProgress; }
            else { if (failures < threshold) ++failures; event = Event::Failed; }
            made_progress = false;
            engaged = false;
        }
        return event;
    }
    bool CanReconsider(const Observation& o) const {
        return initialized && target && o.target == target && o.owner == owner &&
            o.territory == territory && o.now >= last_time && std::isfinite(o.progress) &&
            o.progress <= progress + kProgressEpsilon && failures >= threshold &&
            !wave && !o.committed && !o.uncertain && quiet &&
            o.now - quiet_since >= kQuietMs && o.now >= retry_after;
    }
    void NoAlternative(uint64_t now) { retry_after = now + kRetryMs; }
    bool IsStalled(const Observation& o, uint64_t timeout) const {
        return timeout && engaged && initialized && target && o.target == target &&
            o.owner == owner && o.territory == territory && o.committed &&
            std::isfinite(o.progress) && o.progress <= progress + kProgressEpsilon &&
            o.now >= last_time && o.now >= last_advance && o.now - last_advance >= timeout;
    }
};

struct Cooldown {
    uint64_t target = 0, until = 0;
    uint32_t owner = 0;
    bool Blocks(uint64_t key, uint32_t political_owner, uint64_t now) const {
        return target && key == target && owner == political_owner && now < until;
    }
};
}
