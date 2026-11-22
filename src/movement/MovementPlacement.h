#pragma once
#include "MovementRecovery.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace BetterCiv::Placement {
using Recovery::Vector3;
using Recovery::Distance;
using Recovery::Finite;
using Recovery::Sample;

constexpr unsigned kMaxBlockers = 1024;
constexpr uint64_t kWaitMS = 8000, kLoopWaitMS = 20000, kCooldownMS = 30000, kStepMS = 6000;

// Small vector operations used by the inner clearance and footprint sampling loops.
inline float Dot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline bool Normalize(Vector3& vector)
{
    const float length = Distance(vector, {});
    if (!Finite(vector) || !std::isfinite(length) || length < 0.001f) return false;
    vector = vector * (1 / length);
    return true;
}

bool SameOrder(const Sample& a, const Sample& b);
bool OwnsStep(const Sample& a, const Sample& b);
bool Stationary(const Sample& sample);
bool Moving(const Sample& sample);

struct Blocker {
    Vector3 position{};
    float radius = 0;
    bool allow_exit = false;
};
struct Space {
    Blocker blockers[kMaxBlockers];
    unsigned count = 0;
    bool complete = true;

    void Add(const Vector3& origin, const Vector3& position, float radius, bool allow_exit);
};
struct SearchReport {
    unsigned terrain = 0, occupied = 0;
    const char* reason = "no-clear-candidate";
};

bool CorridorBlocked(const Vector3& from, const Vector3& goal, float radius,
    const Vector3& other, float other_radius, float lookahead = 24);
bool ClearSegment(const Vector3& origin, const Vector3& a, const Vector3& b,
    float radius, const Blocker& obstacle, bool endpoint);

template<class Height>
bool ProjectAir(const Vector3& raw, float radius, float altitude, float sea, Height height_at, Vector3& out)
{
    auto up = raw;
    if (!Normalize(up) || !std::isfinite(altitude) || altitude < 100 || altitude > 10000 ||
        !std::isfinite(sea) || sea < 100 || radius <= 0 || radius > 8) return false;
    out = up * altitude;
    const float height = height_at(raw);
    if (!std::isfinite(height) || altitude - std::max(height, sea) < radius + 4) return false;
    auto side = Cross(up, std::fabs(up.z) < 0.8f ? Vector3{0, 0, 1} : Vector3{0, 1, 0});
    if (!Normalize(side)) return false;
    const auto forward = Cross(up, side);
    for (unsigned i = 0; i < 8; ++i) {
        const float angle = float(i) * 0.78539816339f;
        const auto edge = out + (side * std::cos(angle) + forward * std::sin(angle)) * radius;
        const float edge_height = height_at(edge);
        if (!std::isfinite(edge_height) || altitude - std::max(edge_height, sea) < radius + 4) return false;
    }
    return true;
}

template<class Height>
bool ProjectTerrain(const Vector3& raw, bool water, float radius, float sea,
    float offset, Height height_at, Vector3& out)
{
    auto up = raw;
    if (!Normalize(up) || !std::isfinite(sea) || sea < 100 || sea > 10000 ||
        !std::isfinite(radius) || radius <= 0 || radius > 8 ||
        !std::isfinite(offset) || std::fabs(offset) > 5) return false;
    const float height = height_at(raw);
    if (!std::isfinite(height) || height < 100 || height > 10000 ||
        (water ? height >= sea - 0.5f : height <= sea + 0.5f)) return false;
    out = up * ((water ? sea : height) + offset);

    auto right = Cross(up, std::fabs(up.z) < 0.8f ? Vector3{0, 0, 1} : Vector3{0, 1, 0});
    if (!Normalize(right)) return false;
    const auto forward = Cross(up, right);
    for (unsigned i = 0; i < 8; ++i) {
        const float angle = float(i) * 0.78539816339f;
        const auto edge = out + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        const float edge_height = height_at(edge);
        if (!std::isfinite(edge_height) || edge_height < 100 || edge_height > 10000) return false;
        if (water ? edge_height >= sea - 0.5f
                  : edge_height <= sea + 0.5f || std::fabs(edge_height - height) > radius * 0.65f) return false;
    }
    return true;
}

// Check the footprint and the 12 intermediate positions before accepting a short detour.
template<class Project>
bool FindSpace(const Sample& sample, float radius, const Space& space, Project project,
    Vector3& result, SearchReport& report)
{
    if (!space.complete) {
        report.reason = "incomplete-obstacles";
        return false;
    }
    if (!Finite(sample.position) || !Finite(sample.goal) ||
        !std::isfinite(radius) || radius <= 0 || radius > 8) {
        report.reason = "invalid-geometry";
        return false;
    }

    Vector3 up = sample.position;
    const float altitude = Distance(up, {});
    if (altitude < 100 || altitude > 10000 || !Normalize(up)) return false;
    Vector3 forward = sample.goal - up * Dot(sample.goal, up);
    if (!Normalize(forward)) {
        forward = Cross(up, std::fabs(up.z) < 0.8f ? Vector3{0, 0, 1} : Vector3{0, 1, 0});
        if (!Normalize(forward)) return false;
    }
    const auto side = Cross(up, forward);
    const bool siege = sample.siege_range > 0;
    const unsigned directions = siege ? 16 : 8;
    const unsigned rings = siege ? 3 : 2;
    float best = -INFINITY;
    bool found = false;
    for (unsigned ring = 0; ring < rings; ++ring) {
        const float length = siege ? 10.0f + 12.0f * ring
            : std::min(24.0f, std::max(6.0f, radius * 2 + 2) * (ring ? 1.5f : 1.0f));
        for (unsigned direction = 0; direction < directions; ++direction) {
            const float angle = float(direction) * (6.28318530718f / directions);
            const auto tangent = forward * std::cos(angle) + side * std::sin(angle);
            auto destination = sample.position + tangent * length;
            if (!Normalize(destination)) continue;
            destination = destination * altitude;

            auto previous = sample.position;
            bool clear = true;
            for (unsigned step = 1; step <= 12 && clear; ++step) {
                Vector3 point;
                const auto raw = sample.position + (destination - sample.position) * (float(step) / 12);
                if (!project(raw, point) || !Finite(point) || Distance(sample.position, point) > (siege ? 38 : 28)) {
                    ++report.terrain;
                    clear = false;
                    break;
                }
                const float horizontal = Distance(raw, previous);
                if (std::fabs(Distance(point, {}) - Distance(previous, {})) > std::max(0.8f, horizontal * 0.65f)) {
                    ++report.terrain;
                    clear = false;
                    break;
                }
                for (unsigned i = 0; i < space.count; ++i) {
                    if (!ClearSegment(sample.position, previous, point, radius, space.blockers[i], step == 12)) {
                        ++report.occupied;
                        clear = false;
                        break;
                    }
                }
                previous = point;
            }

            if (!clear || Distance(previous, sample.position) < 4) continue;
            float margin = 10;
            for (unsigned i = 0; i < space.count; ++i) {
                const auto& blocker = space.blockers[i];
                margin = std::min(margin, Distance(previous, blocker.position) - radius - blocker.radius);
            }
            // Penalize an exit that points the vehicle straight back into the same traffic.
            unsigned onward_blockers = 0;
            for (unsigned i = 0; i < space.count; ++i) {
                const auto& blocker = space.blockers[i];
                if (blocker.allow_exit && CorridorBlocked(previous, sample.goal, radius, blocker.position, blocker.radius)) {
                    ++onward_blockers;
                }
            }
            const float remaining = Distance(previous, sample.goal);
            const float outside = std::max(0.0f, remaining - sample.siege_range + 2.0f);
            // A lateral step may clear a queue while staying just outside firing range.
            if (siege && outside > 0 && remaining > Distance(sample.position, sample.goal) + 3) continue;
            const float approach = siege ? -4.0f * outside : 0.0f;
            const float score = approach + margin - 0.05f * length - 0.01f * remaining - 30.0f * onward_blockers;
            if (score > best) {
                best = score;
                result = previous;
                found = true;
            }
        }
    }
    if (found) report.reason = "clear-candidate";
    return found;
}

struct Record {
    uint64_t key = 0, last_seen = 0, start = 0, cooldown = 0, step_until = 0;
    uint64_t followup_at = 0, reserved_until = 0, trade_settle_until = 0;
    unsigned blocked_resumes = 0;
    Sample signature{}, original{}, imposed{};
    Vector3 anchor{}, previous{}, episode_origin{}, step_origin{}, reserved_goal{}, direction{}, window_goal{};
    float path = 0, radius = 0, turns = 0, excursion = 0;
    uint64_t next_watch = 0;
    unsigned attempts = 0;
    bool tracking = false, active = false, moving_window = false, loop_detected = false;
    bool hold_native = false, separating = false, siege_step = false;
    const void* siege_aim = nullptr; // Identity only; never dereferenced.
    Vector3 firing_position{};
    float firing_range = 0;
    uint64_t overlap_since = 0, overlap_peer = 0;
    uint64_t siege_since = 0, siege_seen = 0, siege_target = 0;
    Vector3 siege_anchor{};

    bool StillApproaching(const Vector3& position) const;
    bool SiegeWaiting(const Sample& sample, bool eligible, uint64_t now);
    bool Overlap(uint64_t peer, uint64_t now);
    void Window(const Sample& sample, uint64_t now);
    bool Observe(const Sample& sample, bool quiet, uint64_t now);
    void Begin(const Sample& sample, const Vector3& goal, float footprint, uint64_t now);
    void Finish(const Sample& sample, uint64_t now);
};

struct State {
    Record records[Recovery::kMaxRecords];
    uint64_t next_sample = 0, next_step = 0, next_summary = 0;
    unsigned cursor = 0, lines = 0, watch_lines = 0;

    void ResetSession();
    Record* Find(uint64_t key, uint64_t now);
};

} // namespace BetterCiv::Placement
