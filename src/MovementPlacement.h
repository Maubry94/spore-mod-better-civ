#pragma once
#include "MovementRecovery.h"
#include <algorithm>

namespace BetterCiv::Placement {
using Recovery::Vector3;
using Recovery::Distance;
using Recovery::Finite;
using Recovery::Sample;
constexpr unsigned kMaxBlockers = 1024;
constexpr uint64_t kWaitMS = 8000, kLoopWaitMS = 20000, kCooldownMS = 30000, kStepMS = 6000;
inline float Dot(const Vector3& a, const Vector3& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline bool Normalize(Vector3& v) {
    const float length = Distance(v,{});
    if (!Finite(v) || !std::isfinite(length) || length < 0.001f) return false;
    v = v*(1/length); return true;
}
inline bool SameOrder(const Sample& a, const Sample& b) {
    return a.key == b.key && a.owner == b.owner && a.order == b.order && a.target == b.target &&
        // Trade orders reuse this payload for trade state, not a move point.
        ((a.order == 5 || a.order == 9) ||
        (Finite(a.order_position) && Finite(b.order_position) && Distance(a.order_position,b.order_position) < 0.1f));
}
inline bool OwnsStep(const Sample& a, const Sample& b) {
    return SameOrder(a,b) && Finite(a.goal) && Finite(b.goal) && Distance(a.goal,b.goal)<0.1f &&
        a.goal_stop==b.goal_stop && a.acceptable_stop==b.acceptable_stop;
}
inline bool Stationary(const Sample& s) {
    // Observe physical immobility even while the native combat aim/local goal
    // changes. Quiet is a separate action gate; firing units are never moved.
    Sample physical = s; physical.since_fire = 8000;
    return physical.Valid();
}
inline bool Moving(const Sample& s) {
    Sample physical=s; physical.since_fire=8000; physical.speed=0; physical.crowded=true;
    return physical.Valid() && std::isfinite(s.speed) && s.speed>=0.5f;
}
struct Blocker { Vector3 position{}; float radius = 0; bool allow_exit = false; };
struct Space {
    Blocker blockers[kMaxBlockers]; unsigned count = 0; bool complete = true;
    void Add(const Vector3& origin, const Vector3& position, float radius, bool allow_exit) {
        if (!Finite(position) || !std::isfinite(radius) || radius < 0 || radius > 1000) {
            complete = false; return;
        }
        if (Distance(origin,position) > radius+60) return;
        if (count == kMaxBlockers) { complete = false; return; }
        blockers[count++] = {position,radius,allow_exit};
    }
};
struct SearchReport { unsigned terrain = 0, occupied = 0; const char* reason = "no-clear-candidate"; };
// Look ahead along the requested corridor, including safety separation before
// actual contact. The last test missed ships halted just outside their radii.
inline bool CorridorBlocked(const Vector3& from, const Vector3& goal, float radius,
    const Vector3& other, float other_radius, float lookahead = 24) {
    if (!Finite(from) || !Finite(goal) || !Finite(other) || radius<=0 || radius>8 ||
        other_radius<=0 || other_radius>30) return false;
    auto d=goal-from; const float length=Distance(goal,from);
    if (!Normalize(d)) return Distance(from,other)<radius+other_radius+3;
    const float along=std::clamp(Dot(other-from,d),0.0f,std::min(length,lookahead));
    return Distance(from+d*along,other)<radius+other_radius+3;
}
template<class Height>
bool ProjectAir(const Vector3& raw,float radius,float altitude,float sea,Height height_at,Vector3& out) {
    auto up=raw;
    if (!Normalize(up) || !std::isfinite(altitude) || altitude<100 || altitude>10000 ||
        !std::isfinite(sea) || sea<100 || radius<=0 || radius>8) return false;
    out=up*altitude;
    const float height=height_at(raw);
    if (!std::isfinite(height) || altitude-std::max(height,sea)<radius+4) return false;
    auto side=Cross(up,std::fabs(up.z)<0.8f?Vector3{0,0,1}:Vector3{0,1,0});
    if (!Normalize(side)) return false;
    const auto forward=Cross(up,side);
    for (unsigned i=0;i<8;++i) {
        const float a=float(i)*0.78539816339f;
        const float h=height_at(out+(side*std::cos(a)+forward*std::sin(a))*radius);
        if (!std::isfinite(h) || altitude-std::max(h,sea)<radius+4) return false;
    }
    return true;
}
template<class Height>
bool ProjectTerrain(const Vector3& raw, bool water, float radius, float sea,
    float offset, Height height_at, Vector3& out)
{
    auto up=raw;
    if (!Normalize(up) || !std::isfinite(sea) || sea<100 || sea>10000 ||
        !std::isfinite(radius) || radius<=0 || radius>8 || !std::isfinite(offset) || std::fabs(offset)>5) return false;
    const float height=height_at(raw);
    if (!std::isfinite(height) || height<100 || height>10000 ||
        (water ? height>=sea-0.5f : height<=sea+0.5f)) return false;
    out=up*((water?sea:height)+offset);
    auto right=Cross(up,std::fabs(up.z)<0.8f ? Vector3{0,0,1} : Vector3{0,1,0});
    if (!Normalize(right)) return false;
    const auto forward=Cross(up,right);
    for (unsigned i=0;i<8;++i) {
        const float angle=float(i)*0.78539816339f;
        const auto edge=out+(right*std::cos(angle)+forward*std::sin(angle))*radius;
        const float h=height_at(edge);
        if (!std::isfinite(h) || h<100 || h>10000 ||
            (water ? h>=sea-0.5f : h<=sea+0.5f || std::fabs(h-height)>radius*0.65f)) return false;
    }
    return true;
}
inline bool ClearSegment(const Vector3& origin, const Vector3& a, const Vector3& b,
    float radius, const Blocker& obstacle, bool endpoint)
{
    const float clearance = radius+obstacle.radius+0.75f;
    const float initial = Distance(origin,obstacle.position);
    const auto direction = b-a;
    const float squared = Dot(direction,direction);
    const float fraction = squared > 0.0001f ? std::clamp(Dot(obstacle.position-a,direction)/squared,0.0f,1.0f) : 0;
    const float closest = Distance(a+direction*fraction,obstacle.position);
    if (initial < clearance && obstacle.allow_exit) {
        // Permit escape from an existing overlap, never deeper penetration.
        if (closest+0.05f < initial) return false;
    } else if (closest < clearance) return false;
    return !endpoint || Distance(b,obstacle.position) >= clearance;
}

// project(point, result) checks ground/water and the vehicle's footprint using
// the SDK terrain sampler. This is bounded local clearance, not global routing.
template<class Project>
bool FindSpace(const Sample& s, float radius, const Space& space, Project project,
    Vector3& result, SearchReport& report)
{
    if (!space.complete) { report.reason="incomplete-obstacles"; return false; }
    if (!Finite(s.position) || !Finite(s.goal) ||
        !std::isfinite(radius) || radius <= 0 || radius > 8) { report.reason="invalid-geometry"; return false; }
    Vector3 up = s.position;
    const float altitude = Distance(up,{});
    if (altitude < 100 || altitude > 10000 || !Normalize(up)) return false;
    Vector3 forward = s.goal-up*Dot(s.goal,up);
    if (!Normalize(forward)) {
        forward = Cross(up,std::fabs(up.z) < 0.8f ? Vector3{0,0,1} : Vector3{0,1,0});
        if (!Normalize(forward)) return false;
    }
    const auto side = Cross(up,forward);
    float best = -INFINITY; bool found = false;
    for (unsigned ring = 0; ring < 2; ++ring) {
        const float length = std::min(24.0f,std::max(6.0f,radius*2+2)*(ring ? 1.5f : 1.0f));
        for (unsigned direction = 0; direction < 8; ++direction) {
            const float angle = float(direction)*0.78539816339f;
            const auto tangent = forward*std::cos(angle)+side*std::sin(angle);
            auto raw = s.position+tangent*length;
            if (!Normalize(raw)) continue;
            raw = raw*altitude;
            auto previous = s.position;
            bool clear = true;
            for (unsigned step = 1; step <= 12 && clear; ++step) {
                Vector3 point;
                const auto sample = s.position+(raw-s.position)*(float(step)/12);
                if (!project(sample,point) || !Finite(point) || Distance(s.position,point) > 28) {
                    ++report.terrain; clear = false; break;
                }
                const float horizontal = Distance(sample,previous);
                if (std::fabs(Distance(point,{})-Distance(previous,{})) > std::max(0.8f,horizontal*0.65f)) {
                    ++report.terrain; clear = false; break;
                }
                for (unsigned i=0; i<space.count; ++i) {
                    if (!ClearSegment(s.position,previous,point,radius,space.blockers[i],step==12)) {
                        ++report.occupied; clear = false; break;
                    }
                }
                previous = point;
            }
            if (!clear || Distance(previous,s.position) < 4) continue;
            float margin = 10;
            for (unsigned i=0; i<space.count; ++i)
                margin = std::min(margin,Distance(previous,space.blockers[i].position)-radius-space.blockers[i].radius);
            // Prefer a side with a clear onward corridor, rather than always
            // resuming into the same queue. Static city geometry is checked on
            // the local detour above; this penalty concerns vehicle traffic.
            unsigned onward=0;
            for (unsigned i=0;i<space.count;++i) if (space.blockers[i].allow_exit &&
                CorridorBlocked(previous,s.goal,radius,space.blockers[i].position,space.blockers[i].radius)) ++onward;
            const float score = margin-0.05f*length-0.01f*Distance(previous,s.goal)-30.0f*onward;
            if (score > best) { best = score; result = previous; found = true; }
        }
    }
    if (found) report.reason="clear-candidate";
    return found;
}

struct Record {
    uint64_t key = 0, last_seen = 0, start = 0, cooldown = 0, step_until = 0;
    uint64_t followup_at = 0, reserved_until = 0;
    uint64_t trade_settle_until = 0;
    unsigned blocked_resumes = 0;
    Sample signature{}, original{}, imposed{};
    Vector3 anchor{}, previous{}, episode_origin{}, step_origin{}, reserved_goal{}, direction{}, window_goal{};
    float path = 0, radius = 0, turns = 0, excursion = 0;
    uint64_t next_watch = 0;
    unsigned attempts = 0;
    bool tracking = false, active = false, moving_window = false, loop_detected = false;
    bool hold_native = false, separating = false;
    uint64_t overlap_since=0, overlap_peer=0;
    bool Overlap(uint64_t peer,uint64_t now) {
        if(!peer || peer!=overlap_peer){overlap_peer=peer;overlap_since=now;return false;}
        return now>=overlap_since+1500 && now>=cooldown && attempts<2;
    }
    void Window(const Sample& s, uint64_t now) {
        signature = s; anchor = previous = s.position; start = now; path = 0;
        direction={}; turns=excursion=0; window_goal=s.goal; loop_detected=false;
        moving_window=Moving(s); tracking = Stationary(s) || moving_window;
    }
    bool Observe(const Sample& s, bool quiet, uint64_t now) {
        const bool changed = !SameOrder(signature,s);
        const bool progress = Finite(s.position) && Distance(s.position,episode_origin) > 40;
        if (changed || progress) { attempts = 0; episode_origin = s.position; }
        const bool gap = now < last_seen || now-last_seen > 1500;
        last_seen = now;
        const bool moving=Moving(s);
        if ((!Stationary(s) && !moving) || changed || gap || !tracking || moving!=moving_window) {
            Window(s,now); return false;
        }
        signature = s;
        auto segment=s.position-previous;
        path += Distance(previous,s.position); previous = s.position;
        excursion=std::max(excursion,Distance(s.position,anchor));
        if (!moving) {
            if (path > 1 || excursion > 1) { Window(s,now); return false; }
        } else {
            if (Distance(s.goal,window_goal)>4 || excursion>24) { Window(s,now); return false; }
            if (Normalize(segment)) {
                if (Distance(direction,{})>0.5f) turns+=std::acos(std::clamp(Dot(direction,segment),-1.0f,1.0f));
                direction=segment;
            }
            if (now-start<kLoopWaitMS) return false;
            const float net=Distance(s.position,anchor);
            const float progress_to_goal=Distance(anchor,window_goal)-Distance(s.position,window_goal);
            loop_detected=path>=80 && net<=std::max(6.0f,path*0.08f) &&
                turns>=6.2831853f && progress_to_goal<2;
            if (!loop_detected) { Window(s,now); return false; }
        }
        return quiet && std::isfinite(s.since_fire) && s.since_fire >= 8000 &&
            now-start >= kWaitMS && now >= cooldown && attempts < 2;
    }
    void Begin(const Sample& s, const Vector3& goal, float footprint, uint64_t now) {
        ++attempts; cooldown = now+kCooldownMS; original = imposed = s;
        imposed.goal = reserved_goal = goal; imposed.goal_stop = 1; imposed.acceptable_stop = 2;
        step_origin = s.position; radius = footprint; active = true; separating=false; blocked_resumes=0; hold_native=false;trade_settle_until=0;
        step_until = now+kStepMS; followup_at = now+5000; reserved_until = now+8000;
        Window(s,now);
    }
    void Finish(const Sample& s, uint64_t now) { active = hold_native = separating = false; Window(s,now); last_seen = now; }
};
struct State {
    Record records[Recovery::kMaxRecords];
    uint64_t next_sample = 0, next_step = 0, next_summary = 0;
    unsigned cursor = 0, lines = 0, watch_lines = 0;
    void ResetSession() {
        for (auto& r : records) r = {};
        next_sample = next_step = next_summary = 0; cursor = 0;
    }
    Record* Find(uint64_t key, uint64_t now) {
        for (auto& r : records) if (r.key == key) return &r;
        for (auto& r : records) if (!r.key || (now >= r.last_seen && now-r.last_seen > 60000)) {
            r = {}; r.key = key; return &r;
        }
        return nullptr;
    }
};
}
