#include "MovementPlacement.h"

namespace BetterCiv::Placement {

bool SameOrder(const Sample& a, const Sample& b)
{
    if (a.key != b.key || a.owner != b.owner || a.order != b.order || a.target != b.target) return false;

    // The native trade orders store trade state in the field normally used for a destination.
    if (a.order == 5 || a.order == 9) return true;
    return Finite(a.order_position) && Finite(b.order_position) &&
        Distance(a.order_position, b.order_position) < 0.1f;
}

bool OwnsStep(const Sample& a, const Sample& b)
{
    return SameOrder(a, b) && Finite(a.goal) && Finite(b.goal) && Distance(a.goal, b.goal) < 0.1f &&
        a.goal_stop == b.goal_stop && a.acceptable_stop == b.acceptable_stop;
}

bool Stationary(const Sample& sample)
{
    // Track physical immobility across aim changes. Observe checks time since firing before an intervention.
    Sample physical = sample;
    physical.since_fire = 8000;
    return physical.Valid();
}

bool Moving(const Sample& sample)
{
    Sample physical = sample;
    physical.since_fire = 8000;
    physical.speed = 0;
    physical.crowded = true;
    return physical.Valid() && std::isfinite(sample.speed) && sample.speed >= 0.5f;
}

void Space::Add(const Vector3& origin, const Vector3& position, float radius, bool allow_exit)
{
    if (!Finite(position) || !std::isfinite(radius) || radius < 0 || radius > 1000) {
        complete = false;
        return;
    }
    if (Distance(origin, position) > radius + 60) return;
    if (count == kMaxBlockers) {
        complete = false;
        return;
    }
    blockers[count++] = {position, radius, allow_exit};
}

bool CorridorBlocked(const Vector3& from, const Vector3& goal, float radius,
    const Vector3& other, float other_radius, float lookahead)
{
    if (!Finite(from) || !Finite(goal) || !Finite(other) || radius <= 0 || radius > 8 ||
        other_radius <= 0 || other_radius > 30) return false;

    // Ships can stop in a queue before their footprints touch.
    const float clearance = radius + other_radius + 3;
    const float length = Distance(goal, from);
    auto direction = goal - from;
    if (!Normalize(direction)) return Distance(from, other) < clearance;
    const float along = std::clamp(Dot(other - from, direction), 0.0f, std::min(length, lookahead));
    return Distance(from + direction * along, other) < clearance;
}

bool ClearSegment(const Vector3& origin, const Vector3& a, const Vector3& b,
    float radius, const Blocker& obstacle, bool endpoint)
{
    const float clearance = radius + obstacle.radius + 0.75f;
    const float initial = Distance(origin, obstacle.position);
    const auto direction = b - a;
    const float squared = Dot(direction, direction);
    const float fraction = squared > 0.0001f
        ? std::clamp(Dot(obstacle.position - a, direction) / squared, 0.0f, 1.0f)
        : 0;
    const float closest = Distance(a + direction * fraction, obstacle.position);
    if (initial < clearance && obstacle.allow_exit) {
        // An already overlapping unit may move out if the segment does not take it deeper.
        if (closest + 0.05f < initial) return false;
    } else if (closest < clearance) {
        return false;
    }
    return !endpoint || Distance(b, obstacle.position) >= clearance;
}

bool Record::StillApproaching(const Vector3& position) const
{
    return !siege_aim || Distance(position, firing_position) > firing_range;
}

bool Record::SiegeWaiting(const Sample& sample, bool eligible, uint64_t now)
{
    if (!eligible || !siege_target || siege_target != sample.target || now < siege_seen ||
        now - siege_seen > 1500 || Distance(sample.position, siege_anchor) > 1) {
        siege_since = now;
        siege_anchor = sample.position;
    }
    siege_target = eligible ? sample.target : 0;
    siege_seen = now;
    return eligible && now - siege_since >= 8000 && now >= cooldown && attempts < 2;
}

bool Record::Overlap(uint64_t peer, uint64_t now)
{
    if (!peer || peer != overlap_peer) {
        overlap_peer = peer;
        overlap_since = now;
        return false;
    }
    return now >= overlap_since + 1500 && now >= cooldown && attempts < 2;
}

void Record::Window(const Sample& sample, uint64_t now)
{
    signature = sample;
    anchor = previous = sample.position;
    start = now;
    path = 0;
    direction = {};
    turns = excursion = 0;
    window_goal = sample.goal;
    loop_detected = false;
    moving_window = Moving(sample);
    tracking = Stationary(sample) || moving_window;
}

bool Record::Observe(const Sample& sample, bool quiet, uint64_t now)
{
    const bool changed = !SameOrder(signature, sample);
    const bool progress = Finite(sample.position) && Distance(sample.position, episode_origin) > 40;
    if (changed || progress) {
        attempts = 0;
        episode_origin = sample.position;
    }

    const bool gap = now < last_seen || now - last_seen > 1500;
    last_seen = now;
    const bool moving = Moving(sample);
    if ((!Stationary(sample) && !moving) || changed || gap || !tracking || moving != moving_window) {
        Window(sample, now);
        return false;
    }

    signature = sample;
    auto segment = sample.position - previous;
    path += Distance(previous, sample.position);
    previous = sample.position;
    excursion = std::max(excursion, Distance(sample.position, anchor));
    if (!moving) {
        if (path > 1 || excursion > 1) {
            Window(sample, now);
            return false;
        }
    } else {
        if (Distance(sample.goal, window_goal) > 4 || excursion > 24) {
            Window(sample, now);
            return false;
        }
        if (Normalize(segment)) {
            if (Distance(direction, {}) > 0.5f) {
                turns += std::acos(std::clamp(Dot(direction, segment), -1.0f, 1.0f));
            }
            direction = segment;
        }
        if (now - start < kLoopWaitMS) return false;
        const float net = Distance(sample.position, anchor);
        const float progress_to_goal = Distance(anchor, window_goal) - Distance(sample.position, window_goal);
        loop_detected = path >= 80 && net <= std::max(6.0f, path * 0.08f) &&
            turns >= 6.2831853f && progress_to_goal < 2;
        if (!loop_detected) {
            Window(sample, now);
            return false;
        }
    }
    return quiet && std::isfinite(sample.since_fire) && sample.since_fire >= 8000 &&
        now - start >= kWaitMS && now >= cooldown && attempts < 2;
}

void Record::Begin(const Sample& sample, const Vector3& goal, float footprint, uint64_t now)
{
    ++attempts;
    cooldown = now + kCooldownMS;
    original = imposed = sample;
    imposed.goal = reserved_goal = goal;
    imposed.goal_stop = 1;
    imposed.acceptable_stop = 2;
    step_origin = sample.position;
    radius = footprint;
    active = true;
    separating = siege_step = false;
    siege_aim = nullptr;
    blocked_resumes = 0;
    hold_native = false;
    trade_settle_until = 0;
    step_until = now + kStepMS;
    followup_at = now + 5000;
    reserved_until = now + 8000;
    Window(sample, now);
}

void Record::Finish(const Sample& sample, uint64_t now)
{
    active = hold_native = separating = false;
    Window(sample, now);
    last_seen = now;
}

void State::ResetSession()
{
    for (auto& record : records) record = {};
    next_sample = next_step = next_summary = 0;
    cursor = 0;
    // The diagnostics quotas belong to the running game and continue across loads.
}

Record* State::Find(uint64_t key, uint64_t now)
{
    for (auto& record : records) {
        if (record.key == key) return &record;
    }
    for (auto& record : records) {
        if (!record.key || (now >= record.last_seen && now - record.last_seen > 60000)) {
            record = {};
            record.key = key;
            return &record;
        }
    }
    return nullptr;
}

} // namespace BetterCiv::Placement
