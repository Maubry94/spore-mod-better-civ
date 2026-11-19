// Included in BetterCiv. No game objects are written and no pointer survives
// this synchronous observation. Only the already-guarded order getter is used.
static void observe_movement()
{
    namespace D = CombatDiagnostics;
    namespace M = Movement;
    const auto& civs = portrait_civilizations();
    struct Candidate { Vehicle* unit; Civilization* civ; City* city; M::Snapshot snapshot; };
    Candidate candidates[M::kMaxUnits];
    unsigned count = 0, omitted = 0;
    for (const auto& civ : civs) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit : civ->mVehicles) {
            if (!unit || unit->mbIsDestroyed || unit->mbDead || !unit->mbInView) continue;
            if (count == M::kMaxUnits) { ++omitted; continue; }
            M::Snapshot s;
            s.unit = D::ReadUnit(*unit, civ->mID); s.in_view = unit->mbInView;
            const auto* order = current_vehicle_order ? current_vehicle_order(unit.get()) : nullptr;
            if (order) {
                s.unit.order = order->kind; s.order_position = order->position;
                s.unit.order_target = D::Resolve(order->target, false, civs);
            }
            s.unit.aim = D::Resolve(unit->mpTarget.get(), true, civs);
            s.angular_velocity = unit->mAngularVelocity;
            s.neighbors = unsigned(unit->mNeighbors.size()); // Count only; no neighbor dereference.
            City* nearby = nullptr;
            for (const auto& owner : civs) {
                if (!owner || owner->mbIsDestroyed) continue;
                for (const auto& city : owner->mCities) {
                    if (!city || city->mbIsDestroyed || city.get() != unit->mpNearestCity.get()) continue;
                    nearby = city.get(); s.unit.near_city = city->mID;
                    s.city_under_attack = city->mCapturePercent > 0 || city->mConvertPercent > 0;
                }
            }
            if (s.Eligible()) candidates[count++] = {unit.get(), civ.get(), nearby, s};
        }
    }
    // Report threatening turrets before unit detail; periodic rotation below
    // avoids always favoring the same units when the shared rate limit is hit.
    for (unsigned i = 0; i < count; ++i) {
        auto* city = candidates[i].city;
        if (!city) continue;
        auto& record = D::Journal::For(movement_journal.cities, object_key(city), ai_time);
        record.last_seen = ai_time;
        if (ai_time < record.next_report) continue;
        if (!movement_journal.Reserve()) continue;
        record.next_report = ai_time + 2000;
        char name[256] = {};
        const auto length = city->mName.size() < 60 ? city->mName.size() : 60;
        if (length) WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(city->mName.c_str()),
            int(length), name, sizeof(name)-1, nullptr, nullptr);
        ModAPI::Log("[Better Civ Movement] t=%llu city=%u name=\"%s\" owner-political=%u capture=%.3f conversion=%.3f in-view=%d visible-units=%u omitted-visible=%u rate-suppressed=%u",
            ai_time, city->mID, name, city->mPoliticalID, city->mCapturePercent, city->mConvertPercent,
            city->mbInView, count, omitted, movement_journal.suppressed);
        unsigned slot = 0;
        for (const auto& turret : city->mTurrets) {
            const auto index = slot++;
            if (!turret || turret->mbIsDestroyed || turret->mHealthPoints <= 0) continue;
            if (!movement_journal.Reserve()) break;
            const auto aim = D::Resolve(turret->mpTarget.get(), true, civs);
            ModAPI::Log("[Better Civ Movement] t=%llu city=%u turret=%u slot=%u frozen=%d health=%.1f aim=%s:%u aim-distance=%.2f",
                ai_time, city->mID, turret->mID, index, turret->mFreezeCount, turret->mHealthPoints,
                D::Name(aim.kind), aim.id, aim.Known() ? D::Distance(turret->mPosition, aim.position) : -1.0f);
        }
    }
    for (unsigned offset = 0; offset < count; ++offset) {
        const auto& c = candidates[(movement_journal.cursor + offset) % count];
        const auto& s = c.snapshot; const auto& u = s.unit;
        auto& rec = D::Journal::For(movement_journal.units, object_key(c.unit), ai_time);
        rec.Observe(s, ai_time);
        if (!rec.Due(ai_time) || !movement_journal.Reserve()) continue;
        const auto* weapon = c.unit->mpWeapon.get();
        const float range = weapon ? weapon->GetMaxRange() : -1.0f;
        ModAPI::Log("[Better Civ Movement] t=%llu unit=%u civ=%u player=%d purpose=%d locomotion=%d city=%u order=%d order-target=%s:%u order-city=%u aim=%s:%u aim-city=%u aim-distance=%.2f range=%.2f position=%.2f,%.2f,%.2f velocity=%.2f,%.2f,%.2f angular=%.2f,%.2f,%.2f order-position=%.2f,%.2f,%.2f desired-speed=%.2f neighbors=%u hostiles=%u health=%.1f since-fire-ms=%.0f window-ms=%llu steps=%u path=%.2f net=%.2f velocity-turn-deg=%.1f valid=%d order-changes=%u aim-changes=%u",
            ai_time, u.id, u.owner, c.civ->mIsPlayerOwned, u.purpose, u.locomotion, u.near_city,
            u.order, D::Name(u.order_target.kind), u.order_target.id, u.order_target.city,
            D::Name(u.aim.kind), u.aim.id, u.aim.city,
            u.aim.Known() ? D::Distance(u.position, u.aim.position) : -1.0f, range,
            u.position.x, u.position.y, u.position.z, u.velocity.x, u.velocity.y, u.velocity.z,
            s.angular_velocity.x, s.angular_velocity.y, s.angular_velocity.z,
            s.order_position.x, s.order_position.y, s.order_position.z, u.desired_speed,
            s.neighbors, u.hostiles, u.health, u.since_fire_ms, ai_time - rec.start, rec.samples,
            rec.path, D::Distance(u.position, rec.anchor), rec.turn_degrees, rec.valid,
            rec.order_changes, rec.aim_changes);
        rec.Reported(ai_time);
    }
    if (count) movement_journal.cursor = (movement_journal.cursor + 17) % count;
    if (omitted && !movement_journal.capacity_notified && movement_journal.Reserve()) {
        movement_journal.capacity_notified = true;
        ModAPI::Log("[Better Civ Movement] Visible-unit capacity reached; detail covers at most 128 units per sample.");
    }
    if (movement_journal.lines >= M::kMaxLines && !movement_journal.limit_notified) {
        movement_journal.limit_notified = true;
        ModAPI::Log("[Better Civ Movement] Log limit reached; copy spore_log.txt before restarting for another movement test.");
    }
}
