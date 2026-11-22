#include "MovementAdapterInternal.h"
#include "MovementNative.h"
#include "diplomacy/PortraitPages.h"
#include "engine/Runtime.h"
#include "engine/SimulationRuntime.h"
#include "engine/NativePatches.h"
#include <Spore/ModAPI.h>

namespace BetterCiv {

// Capture the order and local movement request before either recovery mode evaluates the unit.
Recovery::Sample sample_movement(Simulator::cVehicle& unit, uint32_t owner,
                                 const Native::VehicleOrder*& order) {
    Recovery::Sample sample;
    sample.key = object_key(&unit);
    sample.owner = owner;
    sample.position = unit.mPosition;
    sample.goal = unit.field_208;
    sample.desired = unit.mDesiredSpeed;
    sample.speed = Recovery::Finite(unit.mVelocity) ? Recovery::Distance(unit.mVelocity, {}) : INFINITY;
    sample.since_fire = float(unit.mTimeSinceLastWeaponFireMS);
    sample.goal_stop = unit.field_214;
    sample.acceptable_stop = unit.field_254;
    order = current_vehicle_order(&unit);
    if (order) {
        sample.order = order->kind;
        sample.order_position = order->position;
        sample.target = reinterpret_cast<uintptr_t>(order->target);
    }
    return sample;
}

Recovery::State recovery_state;
bool recovery_native_ready;
uintptr_t recovery_image_base;

bool recovery_methods_match(const Vehicle& unit) {
    return Recovery::MatchingMethods(unit, recovery_image_base);
}

void recovery_apply(Vehicle& unit, const Recovery::Sample& s) {
    // MoveTo only replaces the local goal. Spore keeps the strategic order and handles collision and speed.
    unit.MoveTo(s.goal, s.goal_stop, s.acceptable_stop, false);
}

bool (*recovery_check_methods)(const Vehicle&) = recovery_methods_match;

static bool recovery_live(const Vehicle* u) {
    return u && !u->mbIsDestroyed && !u->mbDead && u->mHealthPoints > 0 && !u->mbIsInvalid &&
           !u->mbIsBeingEdited && !u->mbIsGhost && !u->mbFixed &&
           (u->mLocomotion == kVehicleLand || u->mLocomotion == kVehicleWater);
}

static bool recovery_contact(const Vehicle& u, const Civilization& civ) {
    const float radius = u.mFootprintRadius;
    if (!std::isfinite(radius) || radius <= 0 || radius > 30) return false;
    for (const auto& other : civ.mVehicles) {
        if (other.get() == &u || !recovery_live(other.get()) || other->mLocomotion != u.mLocomotion ||
            !Recovery::Finite(other->mPosition) || !Recovery::Finite(other->mVelocity) ||
            Recovery::Distance(other->mVelocity, {}) >= 0.5f)
            continue;
        const float other_radius = other->mFootprintRadius;
        if (!std::isfinite(other_radius) || other_radius <= 0 || other_radius > 30) continue;
        if (Recovery::Distance(u.mPosition, other->mPosition) < radius + other_radius + 0.5f) return true;
    }
    return false;
}

static bool recovery_log() {
    return settings.ai_diagnostics && recovery_state.lines < 4096 && (++recovery_state.lines != 0);
}

static void retry_blocked_movement(int deltaGameTime, bool civ_mode) {
    if (!settings.retry_blocked_paths || !recovery_native_ready || !ai_observer_ready || !civ_mode ||
        deltaGameTime <= 0 || !current_vehicle_order || ai_time < recovery_state.next_sample)
        return;
    recovery_state.next_sample = ai_time + Recovery::kSampleMS;
    struct Due {
        Vehicle* unit;
        Recovery::Record* record;
        Recovery::Sample sample;
        bool player;
    };
    Due due[Recovery::kMaxRecords];
    unsigned count = 0, observed = 0, eligible = 0, crowded = 0;
    for (const auto& civ : portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit : civ->mVehicles) {
            if (!recovery_live(unit.get()) || !unit->mbInView) continue;
            ++observed;
            const Native::VehicleOrder* order = nullptr;
            auto s = sample_movement(*unit, civ->mID, order);
            // Restrict the first trial to native type-1 point requests with no
            // waypoint list or asynchronous route calculation. Never interrupt
            // a weapon/cultural target, retreat/trade order or aircraft.
            s.eligible = order && s.order >= 0 && s.order <= 3 &&
                         (unit->mPurpose == kVehicleMilitary || unit->mPurpose == kVehicleCultural) &&
                         !unit->mpTarget && unit->field_250 == 1 && !unit->field_240 &&
                         unit->field_1F4.empty() && !(unit->field_270 & 4) && recovery_check_methods(*unit);
            s.crowded = s.eligible && recovery_contact(*unit, *civ);
            if (s.eligible) ++eligible;
            if (s.Valid()) ++crowded;
            auto* record = recovery_state.Find(s.key, ai_time);
            if (!record) continue;
            if (record->followup_at && ai_time >= record->followup_at) {
                if (recovery_log())
                    ModAPI::Log(
                        "[Better Civ Recovery] t=%llu event=follow-up unit=%u civ=%u displacement=%.3f desired=%.2f actual-speed=%.2f same-request=%d crowded=%d aim-present=%d since-fire-ms=%.0f",
                        ai_time,
                        unit->mID,
                        civ->mID,
                        Recovery::Distance(s.position, record->retry_position),
                        s.desired,
                        s.speed,
                        Recovery::SameRequest(record->signature, s),
                        s.crowded,
                        bool(unit->mpTarget),
                        s.since_fire);
                record->followup_at = 0;
            }
            if (record->Observe(s, ai_time) && count < Recovery::kMaxRecords)
                due[count++] = {unit.get(), record, s, civ->mIsPlayerOwned};
        }
    }
    if (ai_time >= recovery_state.next_summary) {
        recovery_state.next_summary = ai_time + 30000;
        if (recovery_log())
            ModAPI::Log(
                "[Better Civ Recovery] t=%llu event=summary visible-land-sea=%u supported-idle-aim=%u stationary-contact=%u retry-due=%u",
                ai_time,
                observed,
                eligible,
                crowded,
                count);
    }
    if (!count || ai_time < recovery_state.next_retry) return;
    const auto& candidate = due[recovery_state.cursor++ % count];
    if (recovery_log())
        ModAPI::Log(
            "[Better Civ Recovery] t=%llu event=retry unit=%u civ=%u player=%d locomotion=%d order=%d wait-ms=%llu path=%.3f attempt=%u goal=%.2f,%.2f,%.2f stop=%.2f acceptable=%.2f",
            ai_time,
            candidate.unit->mID,
            candidate.sample.owner,
            candidate.player,
            candidate.unit->mLocomotion,
            candidate.sample.order,
            ai_time - candidate.record->start,
            candidate.record->path,
            candidate.record->attempts + 1,
            candidate.sample.goal.x,
            candidate.sample.goal.y,
            candidate.sample.goal.z,
            candidate.sample.goal_stop,
            candidate.sample.acceptable_stop);
    candidate.record->Retried(candidate.sample, ai_time);
    recovery_state.next_retry = ai_time + 2000;
    recovery_apply(*candidate.unit, candidate.sample);
}

void reset_movement_session() {
    recovery_state.ResetSession();
    placement_state.ResetSession();
}

void bind_movement_native(const unsigned char* base) {
    recovery_image_base = reinterpret_cast<uintptr_t>(base);
    const auto delta = uint32_t(recovery_image_base) - kReferenceBase;
    recovery_native_ready = Recovery::ValidateNative(base, delta);
    original_local_request = reinterpret_cast<LocalRequestSetter>(base + 0xc43fa0 - kReferenceBase);
    placement_terrain_native_ready = Recovery::ValidateTerrain(base, delta);
}

void recover_blocked_movement(int deltaGameTime, bool civ_mode) {
    if (settings.retry_blocked_paths == 2) {
        recover_nearby_space(deltaGameTime, civ_mode);
    } else if (settings.retry_blocked_paths == 1) {
        retry_blocked_movement(deltaGameTime, civ_mode);
    }
}

} // namespace BetterCiv
