// Included in BetterCiv after object_key/portrait_civilizations are available.
static Recovery::State recovery_state;
static bool recovery_native_ready;
static uintptr_t recovery_image_base;
static bool recovery_methods_match(const Vehicle& unit) {
    return Recovery::MatchingMethods(unit,recovery_image_base);
}
static void recovery_retry(Vehicle& unit, const Recovery::Sample& s) {
    // Same local goal, native collision and normal speed calculation. No stop,
    // teleport, order setter, diplomatic action or target reassignment.
    unit.MoveTo(s.goal,s.goal_stop,s.acceptable_stop,false);
}
static bool (*recovery_check_methods)(const Vehicle&) = recovery_methods_match;
static void (*recovery_apply)(Vehicle&, const Recovery::Sample&) = recovery_retry;

static bool recovery_live(const Vehicle* u) {
    return u && !u->mbIsDestroyed && !u->mbDead && u->mHealthPoints > 0 &&
        !u->mbIsInvalid && !u->mbIsBeingEdited && !u->mbIsGhost && !u->mbFixed &&
        (u->mLocomotion == kVehicleLand || u->mLocomotion == kVehicleWater);
}
static bool recovery_contact(const Vehicle& u, const Civilization& civ) {
    const float radius = u.mFootprintRadius;
    if (!std::isfinite(radius) || radius <= 0 || radius > 30) return false;
    for (const auto& other : civ.mVehicles) {
        if (other.get() == &u || !recovery_live(other.get()) || other->mLocomotion != u.mLocomotion ||
            !Recovery::Finite(other->mPosition) || !Recovery::Finite(other->mVelocity) ||
            Recovery::Distance(other->mVelocity,{}) >= 0.5f) continue;
        const float other_radius = other->mFootprintRadius;
        if (!std::isfinite(other_radius) || other_radius <= 0 || other_radius > 30) continue;
        if (Recovery::Distance(u.mPosition,other->mPosition) < radius+other_radius+0.5f) return true;
    }
    return false;
}
static bool recovery_log() {
    return settings.ai_diagnostics && recovery_state.lines < 4096 && (++recovery_state.lines != 0);
}
static void retry_blocked_movement(int deltaGameTime, bool civ_mode) {
    if (!settings.retry_blocked_paths || !recovery_native_ready || !ai_observer_ready ||
        !civ_mode || deltaGameTime <= 0 || !current_vehicle_order || ai_time < recovery_state.next_sample) return;
    recovery_state.next_sample = ai_time+Recovery::kSampleMS;
    struct Due { Vehicle* unit; Recovery::Record* record; Recovery::Sample sample; bool player; };
    Due due[Recovery::kMaxRecords]; unsigned count = 0, observed = 0, eligible = 0, crowded = 0;
    for (const auto& civ : portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit : civ->mVehicles) {
            if (!recovery_live(unit.get()) || !unit->mbInView) continue;
            ++observed;
            Recovery::Sample s;
            s.key = object_key(unit.get()); s.owner = civ->mID; s.position = unit->mPosition;
            s.goal = unit->field_208; s.desired = unit->mDesiredSpeed;
            s.speed = Recovery::Finite(unit->mVelocity) ? Recovery::Distance(unit->mVelocity,{}) : INFINITY;
            s.since_fire = float(unit->mTimeSinceLastWeaponFireMS);
            s.goal_stop = unit->field_214; s.acceptable_stop = unit->field_254;
            const auto* order = current_vehicle_order(unit.get());
            if (order) { s.order = order->kind; s.order_position = order->position;
                s.target = reinterpret_cast<uintptr_t>(order->target); }
            // Restrict the first trial to native type-1 point requests with no
            // waypoint list or asynchronous route calculation. Never interrupt
            // a weapon/cultural target, retreat/trade order or aircraft.
            s.eligible = order && s.order >= 0 && s.order <= 3 &&
                (unit->mPurpose == kVehicleMilitary || unit->mPurpose == kVehicleCultural) &&
                !unit->mpTarget && unit->field_250 == 1 && !unit->field_240 &&
                unit->field_1F4.empty() && !(unit->field_270 & 4) && recovery_check_methods(*unit);
            s.crowded = s.eligible && recovery_contact(*unit,*civ);
            if (s.eligible) ++eligible;
            if (s.Valid()) ++crowded;
            auto* record = recovery_state.Find(s.key,ai_time);
            if (!record) continue;
            if (record->followup_at && ai_time >= record->followup_at) {
                if (recovery_log()) ModAPI::Log("[Better Civ Recovery] t=%llu event=follow-up unit=%u civ=%u displacement=%.3f desired=%.2f actual-speed=%.2f same-request=%d crowded=%d aim-present=%d since-fire-ms=%.0f",
                    ai_time,unit->mID,civ->mID,Recovery::Distance(s.position,record->retry_position),
                    s.desired,s.speed,Recovery::SameRequest(record->signature,s),s.crowded,
                    bool(unit->mpTarget),s.since_fire);
                record->followup_at = 0;
            }
            if (record->Observe(s,ai_time) && count < Recovery::kMaxRecords) due[count++] = {unit.get(),record,s,civ->mIsPlayerOwned};
        }
    }
    if (ai_time >= recovery_state.next_summary) {
        recovery_state.next_summary = ai_time+30000;
        if (recovery_log()) ModAPI::Log("[Better Civ Recovery] t=%llu event=summary visible-land-sea=%u supported-idle-aim=%u stationary-contact=%u retry-due=%u",
            ai_time,observed,eligible,crowded,count);
    }
    if (!count || ai_time < recovery_state.next_retry) return;
    const auto& candidate = due[recovery_state.cursor++ % count];
    if (recovery_log()) ModAPI::Log("[Better Civ Recovery] t=%llu event=retry unit=%u civ=%u player=%d locomotion=%d order=%d wait-ms=%llu path=%.3f attempt=%u goal=%.2f,%.2f,%.2f stop=%.2f acceptable=%.2f",
        ai_time,candidate.unit->mID,candidate.sample.owner,candidate.player,
        candidate.unit->mLocomotion,candidate.sample.order,ai_time-candidate.record->start,
        candidate.record->path,candidate.record->attempts+1,candidate.sample.goal.x,
        candidate.sample.goal.y,candidate.sample.goal.z,candidate.sample.goal_stop,candidate.sample.acceptable_stop);
    candidate.record->Retried(candidate.sample,ai_time);
    recovery_state.next_retry = ai_time+2000;
    recovery_apply(*candidate.unit,candidate.sample);
}
