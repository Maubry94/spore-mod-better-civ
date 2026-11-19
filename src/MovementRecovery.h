#pragma once
#include <Spore/MathUtils.h>
#include <cmath>
#include <cstdint>

namespace BetterCiv::Recovery {
using Math::Vector3;
constexpr uint64_t kSampleMS = 500, kWaitMS = 8000, kCooldownMS = 20000;
constexpr unsigned kMaxRecords = 512;
inline bool Finite(const Vector3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline float Distance(const Vector3& a, const Vector3& b) {
    const auto x = a.x-b.x, y = a.y-b.y, z = a.z-b.z;
    return std::sqrt(x*x+y*y+z*z);
}
struct Sample {
    uint64_t key = 0, target = 0;
    uint32_t owner = 0;
    Vector3 position{}, goal{}, order_position{};
    int order = -1;
    float desired = 0, speed = 0, since_fire = 0, goal_stop = 0, acceptable_stop = 0;
    bool eligible = false, crowded = false;
    bool Valid() const {
        return eligible && crowded && Finite(position) && Finite(goal) && Finite(order_position) &&
            std::isfinite(desired) && desired > 1 && std::isfinite(speed) && speed < 0.5f &&
            std::isfinite(since_fire) && since_fire >= 8000 &&
            std::isfinite(goal_stop) && goal_stop >= 0 && goal_stop <= 100 &&
            std::isfinite(acceptable_stop) && acceptable_stop >= goal_stop && acceptable_stop <= 100 &&
            Distance(position, goal) > acceptable_stop + 1;
    }
};
inline bool SameRequest(const Sample& a, const Sample& b) {
    return a.key == b.key && a.owner == b.owner && a.order == b.order && a.target == b.target &&
        Distance(a.order_position,b.order_position) < 0.1f && Distance(a.goal,b.goal) < 0.1f &&
        a.goal_stop == b.goal_stop && a.acceptable_stop == b.acceptable_stop;
}
struct Record {
    uint64_t key = 0, last_seen = 0, start = 0, cooldown_until = 0;
    uint64_t followup_at = 0;
    Sample signature{};
    Vector3 anchor{}, previous{}, retry_position{};
    float path = 0;
    unsigned attempts = 0;
    bool tracking = false;
    void ResetWindow(const Sample& s, uint64_t now) {
        signature = s; anchor = previous = s.position; path = 0; start = now; tracking = s.Valid();
    }
    bool Observe(const Sample& s, uint64_t now) {
        const bool changed = !SameRequest(signature,s);
        // A real new command/goal or clear displacement begins a new episode.
        // Temporary exclusion, loss of visibility and a sampling gap do not
        // bypass the per-unit retry budget or cooldown.
        if (changed || (Finite(s.position) && Distance(s.position,anchor) > 2)) attempts = 0;
        const bool gap = now < last_seen || now-last_seen > 1500;
        last_seen = now;
        if (!s.Valid() || changed || gap || !tracking) { ResetWindow(s,now); return false; }
        path += Distance(previous,s.position); previous = s.position;
        if (path > 1 || Distance(s.position,anchor) > 1) { ResetWindow(s,now); return false; }
        return now-start >= kWaitMS && now >= cooldown_until && attempts < 2;
    }
    void Retried(const Sample& s, uint64_t now) {
        ++attempts; cooldown_until = now+kCooldownMS;
        followup_at = now+5000; retry_position = s.position; ResetWindow(s,now);
    }
};
struct State {
    Record records[kMaxRecords];
    uint64_t next_sample = 0, next_retry = 0, next_summary = 0;
    unsigned cursor = 0, lines = 0;
    void ResetSession() {
        for (auto& r : records) r = {};
        next_sample = next_retry = next_summary = 0; cursor = 0;
    }
    Record* Find(uint64_t key, uint64_t now) {
        for (auto& r : records) if (r.key == key) return &r;
        // Never evict a live retry budget when a large fleet fills the table.
        for (auto& r : records) if (!r.key || (now >= r.last_seen && now-r.last_seen > 60000)) {
            r = {}; r.key = key; return &r;
        }
        return nullptr;
    }
};
}
