#pragma once
#include "combat/CombatDiagnostics.h"
#include <algorithm>

namespace BetterCiv::Movement {
constexpr unsigned kMaxUnits = 128, kMaxCities = 16, kMaxLines = 32768;
constexpr uint64_t kSampleMs = 250, kReportMs = 2000, kTransitionMs = 500;
constexpr unsigned kBurst = 64, kLinesPerSecond = 32;
using CombatDiagnostics::Distance;

inline bool Finite(const Math::Vector3& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline float Speed(const Math::Vector3& v) { return Distance(v, {}); }

struct Snapshot {
    CombatDiagnostics::UnitSnapshot unit;
    Math::Vector3 order_position = {}, angular_velocity = {};
    unsigned neighbors = 0;
    bool in_view = false, city_under_attack = false;
    bool Eligible() const {
        // Restrict expensive detail to visible combat. Idle defenders at an
        // attacked city remain eligible even when they have no attack order.
        return in_view && (unit.order == 2 || unit.order == 3 || unit.hostiles ||
            unit.aim.kind != CombatDiagnostics::Kind::None || city_under_attack);
    }
};

struct Record {
    uint64_t key = 0, last_seen = 0, start = 0, last_report = 0;
    CombatDiagnostics::UnitSnapshot previous;
    Math::Vector3 anchor = {};
    unsigned samples = 0, order_changes = 0, aim_changes = 0;
    float path = 0, turn_degrees = 0;
    bool seen = false, reported = false, valid = true;

    void Observe(const Snapshot& s, uint64_t now) {
        const auto& u = s.unit;
        if (!seen || now < last_seen || now - last_seen > 1000) {
            // Invisibility, load, long frames or clock rollback must not be
            // mistaken for a continuous trajectory. Keep identity, reset data.
            const auto identity = key;
            *this = {}; key = identity; start = now; anchor = u.position;
            valid = Finite(u.position); seen = true;
        } else {
            ++samples;
            const float step = Distance(u.position, previous.position);
            if (step < 0) valid = false;
            else path += step;
            const float a = Speed(previous.velocity), b = Speed(u.velocity);
            if (a > 0.2f && b > 0.2f) {
                float dot = (previous.velocity.x*u.velocity.x + previous.velocity.y*u.velocity.y +
                    previous.velocity.z*u.velocity.z) / (a*b);
                if (std::isfinite(dot)) {
                    dot = std::max(-1.0f, std::min(1.0f, dot));
                    turn_degrees += std::acos(dot) * 57.2957795f;
                }
            }
            order_changes += u.order != previous.order || u.owner != previous.owner ||
                u.order_target.id != previous.order_target.id || u.order_target.kind != previous.order_target.kind;
            aim_changes += u.aim.id != previous.aim.id || u.aim.kind != previous.aim.kind;
        }
        previous = u; last_seen = now;
    }
    bool Due(uint64_t now) const {
        return !reported || now - last_report >= kReportMs ||
            ((order_changes || aim_changes) && now - last_report >= kTransitionMs);
    }
    void Reported(uint64_t now) {
        reported = true; last_report = start = now; anchor = previous.position;
        path = turn_degrees = 0; samples = order_changes = aim_changes = 0;
        valid = Finite(anchor);
    }
};
struct CityRecord { uint64_t key = 0, last_seen = 0, next_report = 0; };
struct Journal {
    Record units[kMaxUnits];
    CityRecord cities[kMaxCities];
    uint64_t next_sample = 0, token_time = 0;
    unsigned lines = 0, tokens = kBurst, suppressed = 0;
    unsigned cursor = 0;
    bool limit_notified = false, capacity_notified = false;
    void ResetSession() {
        for (auto& r : units) r = {};
        for (auto& r : cities) r = {};
        next_sample = token_time = 0; tokens = kBurst; cursor = 0;
        // Absolute line cap and diagnostic notices survive load/mode resets.
    }
    bool SampleDue(bool enabled, bool ready, bool civ_mode, int delta, uint64_t now) {
        if (!enabled || !ready || !civ_mode || delta <= 0 || lines >= kMaxLines || now < next_sample) return false;
        next_sample = now + kSampleMs;
        if (now < token_time) token_time = now;
        const uint64_t earned = (now - token_time) * kLinesPerSecond / 1000;
        if (earned) {
            tokens = unsigned(std::min<uint64_t>(kBurst, uint64_t(tokens) + earned));
            token_time = now;
        }
        return true;
    }
    bool Reserve() {
        if (lines >= kMaxLines || !tokens) { if (suppressed != UINT32_MAX) ++suppressed; return false; }
        --tokens; ++lines; return true;
    }
};
} // namespace BetterCiv::Movement
