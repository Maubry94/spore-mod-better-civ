#include "MovementAdapterInternal.h"
#include "MovementNative.h"
#include "PortraitPages.h"
#include "Runtime.h"
#include "SimulationRuntime.h"
#include <Spore/ModAPI.h>
#include <Spore/Simulator/cCulturalTarget.h>
#include <Spore/Simulator/cObstacle.h>
#include <Spore/Simulator/cCityWalls.h>
#include <Spore/Simulator/cTurret.h>
#include <Spore/Simulator/SubSystem/PlanetModel.h>
#include <Spore/Terrain/ITerrain.h>
// The pinned SDK's GetData template refers to intrusive_ptr unqualified.
namespace Simulator {
using eastl::intrusive_ptr;
}
#include <Spore/Simulator/SubSystem/GameNounManager.h>
#include "CombatDiagnostics.h"

namespace BetterCiv {

Placement::State placement_state;
bool placement_terrain_native_ready;

static bool placement_terrain_matches(const Terrain::ITerrain& terrain) {
    return placement_terrain_native_ready && Recovery::MatchingTerrain(terrain, recovery_image_base);
}

bool (*placement_check_terrain)(const Terrain::ITerrain&) = placement_terrain_matches;
Terrain::cTerrainMapSet* placement_read_terrain(Simulator::cPlanetModel* planet,
                                                Placement::SearchReport& report) {
    if (!planet) {
        report.reason = "no-planet";
        return nullptr;
    }
    if (!planet->mpSphere) {
        report.reason = "no-sphere";
        return nullptr;
    }
    if (!planet->mpTerrain) {
        report.reason = "no-terrain";
        return nullptr;
    }
    auto& terrain = *planet->mpTerrain;
    if (!placement_check_terrain(terrain)) {
        report.reason = "unsupported-terrain-methods";
        return nullptr;
    }
    // Verified against slot 30 at FA0D40 and the load callback at F96C70:
    // true means finished on this native implementation. Do not invert it.
    if (!terrain.IsLoading()) {
        report.reason = "terrain-loading";
        return nullptr;
    }
    auto* map = terrain.GetTerrainMapSet();
    if (!map) report.reason = "no-terrain-map";
    return map;
}

bool placement_live(const Vehicle* u) {
    return u && !u->mbIsDestroyed && !u->mbDead && u->mHealthPoints > 0 && !u->mbIsInvalid &&
           !u->mbIsBeingEdited && !u->mbIsGhost && !u->mbFixed &&
           (u->mLocomotion == kVehicleLand || u->mLocomotion == kVehicleWater ||
            (u->mLocomotion == kVehicleAir && u->mPurpose == kVehicleEconomic));
}

static bool placement_contact(const Vehicle& unit, const Math::Vector3& goal) {
    for (const auto& civ : portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& other : civ->mVehicles) {
            if (!other || other.get() == &unit || other->mbIsDestroyed || other->mbDead ||
                other->mLocomotion != unit.mLocomotion || !Recovery::Finite(other->mVelocity) ||
                Recovery::Distance(other->mVelocity, {}) >= 0.5f)
                continue;
            if (Placement::CorridorBlocked(
                    unit.mPosition, goal, unit.mFootprintRadius, other->mPosition, other->mFootprintRadius))
                return true;
        }
    }
    return false;
}

// Deep overlap is independent of movement and firing. Keep one anchor in each
// friendly cluster; units with larger stable IDs yield through normal MoveTo.
static uint64_t placement_overlap_peer(const Vehicle& unit, const Civilization& civ) {
    if (!Recovery::Finite(unit.mPosition) || !std::isfinite(unit.mFootprintRadius) ||
        unit.mFootprintRadius <= 0 || unit.mFootprintRadius > 8)
        return 0;
    for (const auto& other : civ.mVehicles) {
        if (!placement_live(other.get()) || other->mID >= unit.mID || other->mLocomotion != unit.mLocomotion ||
            !Recovery::Finite(other->mPosition) || !std::isfinite(other->mFootprintRadius) ||
            other->mFootprintRadius <= 0 || other->mFootprintRadius > 8)
            continue;
        if (Recovery::Distance(unit.mPosition, other->mPosition) <
            0.35f * (unit.mFootprintRadius + other->mFootprintRadius))
            return object_key(other.get());
    }
    return 0;
}

static bool placement_watch_log() {
    return settings.ai_diagnostics && placement_state.watch_lines < 2048 &&
           (++placement_state.watch_lines != 0);
}

static bool placement_log() {
    return settings.ai_diagnostics && placement_state.lines < 4096 && (++placement_state.lines != 0);
}

bool placement_find_space(Vehicle& unit, const Recovery::Sample& s, Math::Vector3& goal,
                          Placement::SearchReport& report) {
    namespace P = Placement;
    auto* planet = Simulator::cPlanetModel::Get();
    auto* map = placement_read_terrain(planet, report);
    if (!map) return false;
    const float radius = unit.mFootprintRadius;
    const bool water = unit.mLocomotion == kVehicleWater;
    const bool air = unit.mLocomotion == kVehicleAir;
    const float sea = map->GetWaterHeight();
    const float ground = planet->GetHeightAt(s.position);
    const float surface = water ? sea : ground;
    const float offset = P::Distance(s.position, {}) - surface;
    report.reason = "invalid-geometry";
    if (!std::isfinite(sea) || !std::isfinite(ground) || sea < 100 || ground < 100 || !std::isfinite(offset) ||
        (!air && std::fabs(offset) > 5) || radius <= 0 || radius > 8)
        return false;
    P::Space space;
    const float altitude = P::Distance(s.position, {});
    auto structure = [&](const Math::Vector3& p, float r) {
        if (!air) {
            space.Add(s.position, p, r, false);
            return;
        }
        auto up = p;
        if (!P::Normalize(up)) {
            space.complete = false;
            return;
        }
        // Conservatively avoid the whole city/obstacle column at the current
        // flight altitude; permit exiting an existing overlap, never deeper.
        space.Add(s.position, up * altitude, r, true);
    };
    const auto& civs = portrait_civilizations();
    for (const auto& civ : civs) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& other : civ->mVehicles) {
            if (!other || other.get() == &unit || other->mbIsDestroyed || other->mbDead || other->mbIsGhost ||
                other->mbIsInvalid ||
                (air ? other->mLocomotion != kVehicleAir : other->mLocomotion == kVehicleAir))
                continue;
            // Moving, hostile and trading vehicles also occupy physical space.
            space.Add(s.position, other->mPosition, other->mFootprintRadius, true);
        }
        for (const auto& city : civ->mCities) {
            if (!city || city->mbIsDestroyed) continue;
            const auto* walls = city->mpCityWalls.get();
            if (walls && !walls->mbIsDestroyed) {
                structure(walls->mPosition, walls->mOuterRadius);
                const auto* dock = walls->mpDock.get();
                if (dock && !dock->mbIsDestroyed && !dock->mbIsInvalid)
                    structure(dock->mPosition, dock->mFootprintRadius);
            } else
                structure(city->mPosition, city->mBuildingPlacementRadius + 4);
        }
    }
    for (const auto& obstacle : Simulator::GetData<Simulator::cObstacle>().data) {
        if (obstacle && !obstacle->mbIsDestroyed)
            structure(obstacle->mPosition, obstacle->mObstacleBaseCollisionRadius);
    }
    for (const auto& r : placement_state.records) {
        if (r.key != s.key && r.reserved_until > ai_time)
            space.Add(s.position, r.reserved_goal, r.radius, false);
    }
    for (const auto& r : placement_state.records)
        if (r.key == s.key && r.attempts && P::Distance(s.position, r.step_origin) < 1 &&
            P::Finite(r.imposed.goal))
            space.Add(s.position, r.imposed.goal, std::max(2.0f, radius), false);
    report.reason = "no-clear-candidate";
    auto project = [&](const Math::Vector3& raw, Math::Vector3& out) {
        if (air)
            return P::ProjectAir(
                raw,
                radius,
                altitude,
                sea,
                [&](const Math::Vector3& p) { return planet->GetHeightAt(p); },
                out);
        return P::ProjectTerrain(
            raw,
            water,
            radius,
            sea,
            offset,
            [&](const Math::Vector3& p) { return planet->GetHeightAt(p); },
            out);
    };
    return P::FindSpace(s, radius, space, project, goal, report);
}

bool (*placement_plan)(Vehicle&, const Recovery::Sample&, Math::Vector3&,
                       Placement::SearchReport&) = placement_find_space;

// A parked ship can block the only dock approach. Yield only genuinely idle
// units (order 0), never an explicit move, trade, defense or attack order.
static bool placement_yield_idle(Vehicle& blocked, const Recovery::Sample& request) {
    if (blocked.mLocomotion == kVehicleAir) return false;
    for (const auto& civ : portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit : civ->mVehicles) {
            if (!placement_live(unit.get()) || unit.get() == &blocked || !unit->mbInView ||
                unit->mLocomotion != blocked.mLocomotion || unit->mpTarget || !unit->mHostileUnits.empty() ||
                unit->mTimeSinceLastWeaponFireMS < 30000 || unit->mDesiredSpeed > 0.1f ||
                !Recovery::Finite(unit->mVelocity) || Recovery::Distance(unit->mVelocity, {}) >= 0.5f ||
                unit->field_250 != 1 || unit->field_240 || !unit->field_1F4.empty() || (unit->field_270 & 4) ||
                !recovery_check_methods(*unit))
                continue;
            const auto* order = current_vehicle_order(unit.get());
            if (!order || order->kind != 0 ||
                !Placement::CorridorBlocked(request.position,
                                            request.goal,
                                            blocked.mFootprintRadius,
                                            unit->mPosition,
                                            unit->mFootprintRadius))
                continue;
            auto* r = placement_state.Find(object_key(unit.get()), ai_time);
            if (!r || r->active || ai_time < r->cooldown || r->attempts >= 2) continue;
            Recovery::Sample s;
            s.key = object_key(unit.get());
            s.owner = civ->mID;
            s.order = 0;
            s.target = reinterpret_cast<uintptr_t>(order->target);
            s.order_position = order->position;
            s.position = unit->mPosition;
            s.goal = unit->field_208;
            s.goal_stop = unit->field_214;
            s.acceptable_stop = unit->field_254;
            s.eligible = true;
            s.since_fire = float(unit->mTimeSinceLastWeaponFireMS);
            auto plan = s;
            plan.goal = request.goal;
            Math::Vector3 goal;
            Placement::SearchReport report;
            if (!placement_plan(*unit, plan, goal, report)) continue;
            r->Begin(s, goal, unit->mFootprintRadius, ai_time);
            if (placement_log())
                ModAPI::Log(
                    "[Better Civ Placement] t=%llu event=idle-yield unit=%u civ=%u player=%d for-unit=%u",
                    ai_time,
                    unit->mID,
                    civ->mID,
                    civ->mIsPlayerOwned,
                    blocked.mID);
            recovery_apply(*unit, r->imposed);
            return true;
        }
    }
    return false;
}

// Keep the strategic order. Only idle members of a congested naval siege
// may take one native movement step toward a clear firing position.
static bool siege_approach(Vehicle& unit, const Civilization& owner, const Recovery::Sample& s,
                           Math::Vector3& aim, float& range, bool& contributing) {
    const bool religious = unit.mPurpose == kVehicleCultural && s.order == 3;
    if (unit.mLocomotion != kVehicleWater ||
        (!religious && !(s.order == 2 && unit.mPurpose == kVehicleMilitary)) || !s.target ||
        !Recovery::Finite(s.position) || !(s.speed < 0.5f) || unit.field_250 != 1 || unit.field_240 ||
        (unit.field_270 & 4) || !unit.mpWeapon || !recovery_check_methods(unit) ||
        (!religious && s.since_fire < 8000))
        return false;
    range = unit.mpWeapon->GetMaxRange();
    if (!std::isfinite(range) || range < 5 || range > 300) return false;
    bool traffic = false;
    for (const auto& peer : owner.mVehicles)
        if (peer.get() != &unit && placement_live(peer.get()) && peer->mLocomotion == kVehicleWater &&
            Recovery::Distance(peer->mPosition, s.position) <
                (std::max)(14.0f, unit.mFootprintRadius + peer->mFootprintRadius + 8)) {
            traffic = true;
            break;
        }
    if (!traffic) return false;
    const void* target = reinterpret_cast<const void*>(uintptr_t(s.target));
    for (const auto& other : portrait_civilizations()) {
        if (!other || other.get() == &owner || other->mbIsDestroyed) continue;
        for (const auto& city : other->mCities) {
            if (!city || city->mbIsDestroyed || Recovery::Distance(city->mPosition, s.position) > range + 110)
                continue;
            bool matches = target == static_cast<Simulator::cGameData*>(city.get());
            float nearest = INFINITY;
            auto consider = [&](const Math::Vector3& p) {
                const float distance = Recovery::Distance(p, s.position);
                if (distance < nearest) {
                    nearest = distance;
                    aim = p;
                }
            };
            if (religious) {
                bool aiming_at_city = !unit.mpTarget;
                for (const auto& culture : city->mCulturalTargets)
                    if (culture && !culture->mbIsDestroyed) {
                        const bool ordered = target == static_cast<Simulator::cGameData*>(culture.get());
                        const bool aiming =
                            unit.mpTarget.get() == static_cast<Simulator::cCombatant*>(culture.get());
                        matches |= ordered;
                        aiming_at_city |= aiming;
                        for (const auto& engaged : culture->mAttackers)
                            if (engaged.get() == &unit) {
                                contributing = true;
                                return false;
                            }
                        if (aiming || (!unit.mpTarget &&
                                       (ordered || target == static_cast<Simulator::cGameData*>(city.get()))))
                            consider(culture->mPosition);
                    }
                if (!aiming_at_city) continue;
            } else if (matches) {
                const auto consider_structure = [&](const auto& object) {
                    if (object && !object->mbIsDestroyed && object->mHealthPoints > 0 &&
                        (!unit.mpTarget ||
                         unit.mpTarget.get() == static_cast<Simulator::cCombatant*>(object.get())))
                        consider(object->mPosition);
                };
                for (const auto& b : city->mBuildings)
                    consider_structure(b);
                for (const auto& t : city->mTurrets)
                    consider_structure(t);
                // Keep a live target in range and always yield to a mobile defender.
                if (unit.mpTarget && nearest <= range) return false;
            }
            if (matches && nearest <= range + 42) return true;
        }
    }
    return false;
}

static bool hold_ai_destination(int order) {
    return order == 2 || order == 3 || order == 5 || order == 7 || order == 9;
}

static void update_active_step(Vehicle& unit, const Civilization& owner, Placement::Record& record,
                               const Recovery::Sample& sample, bool quiet) {
    namespace P = Placement;
    record.last_seen = ai_time;
    const bool owned = P::OwnsStep(record.imposed, sample);
    const bool allowed_aim = quiet || (record.siege_aim && record.siege_aim == unit.mpTarget.get());
    const bool safe = sample.eligible && P::Finite(sample.position) &&
                      record.StillApproaching(sample.position) &&
                      (record.separating || (allowed_aim && sample.since_fire >= 8000));
    const bool arrived =
        P::Finite(sample.position) && P::Distance(sample.position, record.imposed.goal) <= 2.5f;
    const bool expired = ai_time >= record.step_until;

    // A second leg can clear a queue while preserving the original strategic order.
    if (!record.siege_step && !record.separating && owned && safe && arrived && sample.order != 0 &&
        record.attempts < 2 && placement_contact(unit, record.original.goal)) {
        auto next = sample;
        next.goal = record.original.goal;
        Math::Vector3 goal;
        P::SearchReport report;
        if (placement_plan(unit, next, goal, report)) {
            const auto original = record.original;
            record.Begin(next, goal, unit.mFootprintRadius, ai_time);
            record.original = original;
            record.hold_native = !owner.mIsPlayerOwned && hold_ai_destination(sample.order);
            if (placement_log()) {
                ModAPI::Log("[Better Civ Placement] t=%llu event=detour-leg unit=%u civ=%u attempt=%u",
                            ai_time,
                            unit.mID,
                            owner.mID,
                            record.attempts);
            }
            recovery_apply(unit, record.imposed);
            return;
        }
    }

    if (owned && safe && !arrived && !expired) return;
    const bool resume = owned && safe && sample.order != 0 && (arrived || expired);
    if (placement_log()) {
        const char* reason = !owned    ? "native-request-changed"
                             : !safe   ? "combat-or-unsupported"
                             : arrived ? "arrived"
                                       : "timeout";
        ModAPI::Log(
            "[Better Civ Placement] t=%llu event=step-end unit=%u civ=%u reason=%s resume=%d displacement=%.3f",
            ai_time,
            unit.mID,
            owner.mID,
            reason,
            resume,
            P::Distance(sample.position, record.step_origin));
    }

    // Another command or recent combat takes precedence over the saved destination.
    const auto original = record.original;
    record.Finish(sample, ai_time);
    if (resume) {
        recovery_apply(unit, original);
        if (sample.order == 5 || sample.order == 9) record.trade_settle_until = ai_time + 500;
    }
}

struct PlacementCandidate {
    Vehicle* unit;
    Placement::Record* record;
    Recovery::Sample sample;
    bool player;
    bool separate;
    Math::Vector3 siege_aim;
    float siege_range;
};

static void begin_placement_step(const PlacementCandidate& candidate) {
    Math::Vector3 goal;
    Placement::SearchReport report;
    placement_state.next_step = ai_time + 2000;
    if (!candidate.siege_range && !candidate.separate &&
        placement_yield_idle(*candidate.unit, candidate.sample)) {
        candidate.record->cooldown = ai_time + 10000;
        return;
    }

    auto plan = candidate.sample;
    if (candidate.siege_range) {
        plan.goal = candidate.siege_aim;
        plan.siege_range = candidate.siege_range;
    }
    if (!placement_plan(*candidate.unit, plan, goal, report) || !Placement::Finite(goal)) {
        candidate.record->cooldown = ai_time + 20000;
        if (placement_log())
            ModAPI::Log(
                "[Better Civ Placement] t=%llu event=no-space unit=%u civ=%u reason=%s terrain-rejected=%u occupied-rejected=%u",
                ai_time,
                candidate.unit->mID,
                candidate.sample.owner,
                report.reason,
                report.terrain,
                report.occupied);
        return;
    }
    const auto waited = ai_time - candidate.record->start;
    const auto path = candidate.record->path;
    const char* reason = candidate.siege_range             ? "naval-siege-approach"
                         : candidate.separate              ? "deep-overlap"
                         : candidate.record->loop_detected ? "quiet-loop"
                                                           : "stationary-contact";
    candidate.record->Begin(candidate.sample, goal, candidate.unit->mFootprintRadius, ai_time);
    candidate.record->separating = candidate.separate;
    candidate.record->hold_native =
        candidate.separate || (!candidate.player && hold_ai_destination(candidate.sample.order));
    if (placement_log())
        ModAPI::Log(
            "[Better Civ Placement] t=%llu event=step unit=%u civ=%u player=%d locomotion=%d order=%d reason=%s attempt=%u wait-ms=%llu path=%.3f from=%.2f,%.2f,%.2f goal=%.2f,%.2f,%.2f old-goal=%.2f,%.2f,%.2f terrain-rejected=%u occupied-rejected=%u",
            ai_time,
            candidate.unit->mID,
            candidate.sample.owner,
            candidate.player,
            candidate.unit->mLocomotion,
            candidate.sample.order,
            reason,
            candidate.record->attempts,
            waited,
            path,
            candidate.sample.position.x,
            candidate.sample.position.y,
            candidate.sample.position.z,
            goal.x,
            goal.y,
            goal.z,
            candidate.sample.goal.x,
            candidate.sample.goal.y,
            candidate.sample.goal.z,
            report.terrain,
            report.occupied);
    if (candidate.siege_range) {
        // Hold a repeated simple request only. Leave queued navigation to the engine.
        candidate.record->siege_step = true;
        candidate.record->siege_aim = candidate.unit->mpTarget.get();
        candidate.record->firing_position = candidate.siege_aim;
        candidate.record->firing_range = candidate.siege_range;
        candidate.record->active = candidate.sample.eligible;
        candidate.record->hold_native = candidate.record->active;
    }
    recovery_apply(*candidate.unit, candidate.record->imposed);
}

void recover_nearby_space(int deltaGameTime, bool civ_mode) {
    namespace P = Placement;
    if (!recovery_native_ready || !ai_observer_ready || !civ_mode || deltaGameTime <= 0 ||
        !current_vehicle_order || ai_time < placement_state.next_sample)
        return;
    placement_state.next_sample = ai_time + 500;
    PlacementCandidate due[Recovery::kMaxRecords];
    unsigned count = 0, observed = 0, stationary = 0, aim_blocked = 0, firing_blocked = 0;
    unsigned traders = 0, supported_traders = 0, loops = 0;
    for (const auto& civ : portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit : civ->mVehicles) {
            if (!placement_live(unit.get()) || !unit->mbInView) continue;
            ++observed;
            const Native::VehicleOrder* order = nullptr;
            auto s = sample_movement(*unit, civ->mID, order);
            const bool trader = unit->mPurpose == kVehicleEconomic;
            const bool supported_order =
                order && ((s.order >= 0 && s.order <= 3) || (trader && (s.order == 5 || s.order == 9)) ||
                          (!trader && s.order == 7));
            s.eligible = supported_order && (!trader || s.order != 5 || trade_detour_check(*unit)) &&
                         (unit->mLocomotion != kVehicleAir || s.order == 5 || s.order == 9) &&
                         (trader || unit->mPurpose == kVehicleMilitary || unit->mPurpose == kVehicleCultural) &&
                         unit->field_250 == 1 && !unit->field_240 && unit->field_1F4.empty() &&
                         !(unit->field_270 & 4) && recovery_check_methods(*unit);
            s.crowded = s.eligible && placement_contact(*unit, s.goal);
            traders += trader;
            supported_traders += trader && s.eligible;
            const bool quiet = !unit->mpTarget;
            if (P::Stationary(s)) {
                ++stationary;
                if (!quiet) ++aim_blocked;
                if (s.since_fire < 8000) ++firing_blocked;
            }
            auto* r = placement_state.Find(s.key, ai_time);
            if (!r) continue;
            if (r->followup_at && ai_time >= r->followup_at) {
                if (placement_log())
                    ModAPI::Log(
                        "[Better Civ Placement] t=%llu event=follow-up unit=%u civ=%u displacement=%.3f actual-speed=%.2f same-order=%d step-request=%d crowded=%d aim-present=%d siege=%d request-kind=%d queued=%u desired-speed=%.2f remaining=%.2f since-fire-ms=%.0f",
                        ai_time,
                        unit->mID,
                        civ->mID,
                        P::Distance(s.position, r->step_origin),
                        s.speed,
                        P::SameOrder(r->original, s),
                        P::OwnsStep(r->imposed, s),
                        s.crowded,
                        !quiet,
                        r->siege_step,
                        unit->field_250,
                        unsigned(unit->field_1F4.size()),
                        s.desired,
                        P::Distance(s.position, r->imposed.goal),
                        s.since_fire);
                r->followup_at = 0;
            }
            if (r->active) {
                update_active_step(*unit, *civ, *r, s, quiet);
                continue;
            }

            const bool ready = r->Observe(s, quiet, ai_time);
            loops += r->loop_detected;
            const auto overlap = placement_overlap_peer(*unit, *civ);
            const bool separate = r->Overlap(s.eligible ? overlap : 0, ai_time);
            Math::Vector3 siege_aim{};
            float range = 0;
            bool contributing = false;
            const bool candidate = siege_approach(*unit, *civ, s, siege_aim, range, contributing);
            const bool coastal = r->SiegeWaiting(s, candidate, ai_time);
            // A registered preacher or firing warship holds its position at a siege.
            bool naval_attack = candidate;
            if (separate && !naval_attack && s.target && unit->mLocomotion == kVehicleWater &&
                (s.order == 2 || s.order == 3)) {
                const auto target = CombatDiagnostics::Resolve(order->target, false, portrait_civilizations());
                naval_attack = target.kind == CombatDiagnostics::Kind::City ||
                               target.kind == CombatDiagnostics::Kind::Culture;
            }
            if ((coastal || (!naval_attack && (ready || separate)) ||
                 (naval_attack && ready && !unit->mpTarget)) &&
                !contributing && count < Recovery::kMaxRecords)
                due[count++] = {unit.get(),
                                r,
                                s,
                                civ->mIsPlayerOwned,
                                separate && !naval_attack,
                                siege_aim,
                                coastal ? range : 0};
            // Focused diagnostics survive the general trajectory log's cap.
            // Include unsupported local requests so an exclusion is visible.
            if ((overlap || r->loop_detected || (trader && s.desired > 1 && s.speed < 0.5f)) &&
                ai_time >= r->next_watch) {
                r->next_watch = ai_time + 60000;
                if (placement_watch_log())
                    ModAPI::Log(
                        "[Better Civ Placement] t=%llu event=watch unit=%u civ=%u purpose=%d locomotion=%d order=%d request=%d pending=%d flags=%u supported=%d crowded=%d overlap-peer=%llu loop=%d wait-ms=%llu path=%.2f net=%.2f turns=%.1f position=%.2f,%.2f,%.2f local-goal=%.2f,%.2f,%.2f speed=%.2f desired=%.2f aim=%d since-fire=%.0f",
                        ai_time,
                        unit->mID,
                        civ->mID,
                        unit->mPurpose,
                        unit->mLocomotion,
                        s.order,
                        unit->field_250,
                        !unit->field_1F4.empty(),
                        unsigned(unit->field_270),
                        s.eligible,
                        s.crowded,
                        overlap,
                        r->loop_detected,
                        ai_time - r->start,
                        r->path,
                        P::Distance(s.position, r->anchor),
                        r->turns * 57.2957795f,
                        s.position.x,
                        s.position.y,
                        s.position.z,
                        s.goal.x,
                        s.goal.y,
                        s.goal.z,
                        s.speed,
                        s.desired,
                        !quiet,
                        s.since_fire);
            }
        }
    }
    if (ai_time >= placement_state.next_summary) {
        placement_state.next_summary = ai_time + 60000;
        if (placement_watch_log())
            ModAPI::Log(
                "[Better Civ Placement] t=%llu event=summary visible=%u traders=%u supported-traders=%u stationary-contact=%u loops=%u active-aim=%u recent-fire=%u step-due=%u",
                ai_time,
                observed,
                traders,
                supported_traders,
                stationary,
                loops,
                aim_blocked,
                firing_blocked,
                count);
    }
    if (!count || ai_time < placement_state.next_step) return;
    begin_placement_step(due[placement_state.cursor++ % count]);
}

LocalRequestSetter original_local_request;

static bool placement_hold_request(Vehicle& unit, const Native::LocalPointRequest* request) {
    namespace P = Placement;
    if (settings.retry_blocked_paths != 2 || !recovery_native_ready || !ai_observer_ready ||
        !current_vehicle_order || !in_civilization())
        return false;

    // The native callback may run every frame. Look up an active hold without creating a record.
    P::Record* record = nullptr;
    const auto key = object_key(&unit);
    for (auto& item : placement_state.records) {
        if (item.key == key && item.active && item.hold_native) {
            record = &item;
            break;
        }
    }
    if (!record) return false;

    auto sample = record->imposed;
    sample.position = unit.mPosition;
    const auto* order = current_vehicle_order(&unit);
    if (order) {
        sample.order = order->kind;
        sample.target = reinterpret_cast<uintptr_t>(order->target);
        sample.order_position = order->position;
    }

    const bool supported = request && request->kind == 1 && !request->relative &&
                           request->waypoints_begin == request->waypoints_end && P::Finite(request->position) &&
                           std::isfinite(request->stop) && request->stop >= 0 && request->stop <= 100 &&
                           std::isfinite(request->acceptable) && request->acceptable >= 0 &&
                           request->acceptable <= 100;
    if (!order || !P::SameOrder(record->imposed, sample) || !placement_live(&unit) || !unit.mbInView ||
        (!record->separating && ((unit.mpTarget && unit.mpTarget.get() != record->siege_aim) ||
                                 unit.mTimeSinceLastWeaponFireMS < 8000)) ||
        !supported || ai_time >= record->step_until || ai_time < record->last_seen ||
        ai_time - record->last_seen > 1500 || !P::Finite(sample.position) ||
        !record->StillApproaching(sample.position) ||
        P::Distance(sample.position, record->imposed.goal) <= 2.5f) {
        record->Finish(sample, ai_time);
        return false;
    }

    // Our own MoveTo and a genuinely new destination both take precedence over the hold.
    if (P::Distance(request->position, record->imposed.goal) < 0.1f &&
        request->stop == record->imposed.goal_stop && request->acceptable == record->imposed.acceptable_stop) {
        return false;
    }
    if (P::Distance(request->position, record->original.goal) > 4) {
        record->Finish(sample, ai_time);
        return false;
    }

    record->original.goal = request->position;
    record->original.goal_stop = request->stop;
    record->original.acceptable_stop = request->acceptable;
    if (++record->blocked_resumes == 1 && placement_log()) {
        ModAPI::Log(
            "[Better Civ Placement] t=%llu event=defer-native-refresh unit=%u order=%d remaining-ms=%llu",
            ai_time,
            unit.mID,
            sample.order,
            record->step_until - ai_time);
    }
    return true;
}

void __fastcall retained_local_request(Simulator::cLocomotiveObject* locomotive, int,
                                       const Native::LocalPointRequest* request) {
    // Only cVehicle's locomotive subobject uses the verified request thunk at C9EA20.
    auto* unit = static_cast<Vehicle*>(locomotive);
    if (!placement_hold_request(*unit, request)) {
        original_local_request(locomotive, request);
    }
}

} // namespace BetterCiv
