// Included inside BetterCiv after the existing SDK observer helpers.
// This observer is read-only and adds no native patch or serialized field.
static void observe_combat()
{
    namespace D = CombatDiagnostics;
    const auto& civs = portrait_civilizations();
    for (const auto& civ : civs) {
        if (!civ || civ->mbIsDestroyed) continue;
        auto& comm = D::Journal::For(combat_journal.civs, object_key(civ.get()), ai_time);
        comm.last_seen = ai_time;
        if (comm.Observe(civ->mCurrentCommEventId, civ->mCommRelPanelSlot, unsigned(civ->mCities.size())) &&
            combat_journal.Reserve()) {
            char name[256] = {};
            const auto length = civ->mName.size() < 60 ? civ->mName.size() : 60;
            if (length) WideCharToMultiByte(CP_UTF8, 0,
                reinterpret_cast<const wchar_t*>(civ->mName.c_str()), int(length), name, sizeof(name)-1, nullptr, nullptr);
            ModAPI::Log("[Better Civ Combat] t=%llu civ=%u name=\"%s\" player=%d cities=%u comm-event=%u portrait-slot=%d",
                ai_time, civ->mID, name, civ->mIsPlayerOwned, comm.cities, comm.event, comm.slot);
        }
        for (const auto& unit : civ->mVehicles) {
            if (!unit || unit->mbIsDestroyed || unit->mbDead) continue;
            auto s = D::ReadUnit(*unit, civ->mID);
            const auto* order = current_vehicle_order ? current_vehicle_order(unit.get()) : nullptr;
            if (order) {
                s.order = order->kind;
                s.order_target = D::Resolve(order->target, false, civs);
            }
            s.aim = D::Resolve(unit->mpTarget.get(), true, civs);
            // Do not dereference a nearest-city pointer not in the live list.
            const auto nearby = D::Resolve(unit->mpNearestCity.get(), false, civs);
            if (nearby.kind == D::Kind::City) {
                s.near_city = nearby.id;
                for (const auto& known_civ : civs) {
                    if (!known_civ || known_civ->mbIsDestroyed) continue;
                    for (const auto& city : known_civ->mCities) {
                        if (!city || city->mbIsDestroyed || city.get() != unit->mpNearestCity.get()) continue;
                        s.nearby_city_under_attack = city->mCapturePercent > 0 || city->mConvertPercent > 0;
                    }
                }
            }
            auto& record = D::Journal::For(combat_journal.units, object_key(unit.get()), ai_time);
            if (!record.Observe(s, ai_time) || !combat_journal.Reserve()) continue;
            const auto* weapon = unit->mpWeapon.get();
            const float range = weapon ? weapon->GetMaxRange() : -1.0f;
            const float distance = s.aim.Known() ? D::Distance(s.position, s.aim.position) : -1.0f;
            ModAPI::Log("[Better Civ Combat] t=%llu unit=%u civ=%u player=%d purpose=%d locomotion=%d order=%d order-target=%s:%u order-city=%u aim=%s:%u aim-city=%u near-city=%u near-attack=%d stationary-ms=%llu position=%.2f,%.2f,%.2f velocity=%.2f,%.2f,%.2f desired-speed=%.2f health=%.1f hostiles=%u since-fire-ms=%.0f range=%.2f aim-distance=%.2f",
                ai_time, s.id, s.owner, civ->mIsPlayerOwned, s.purpose, s.locomotion, s.order,
                D::Name(s.order_target.kind), s.order_target.id, s.order_target.city,
                D::Name(s.aim.kind), s.aim.id, s.aim.city, s.near_city, s.nearby_city_under_attack,
                record.stationary_ms, s.position.x, s.position.y, s.position.z,
                s.velocity.x, s.velocity.y, s.velocity.z, s.desired_speed, s.health,
                s.hostiles, s.since_fire_ms, range, distance);
        }
        for (const auto& city : civ->mCities) {
            if (!city || city->mbIsDestroyed) continue;
            auto& record = D::Journal::For(combat_journal.cities, object_key(city.get()), ai_time);
            record.last_seen = ai_time;
            if (ai_time < record.next_report) continue;
            bool active = city->mCapturePercent > 0 || city->mConvertPercent > 0;
            for (const auto& turret : city->mTurrets)
                if (turret && !turret->mbIsDestroyed && (turret->mpTarget || turret->mFreezeCount > 0)) active = true;
            if (!active) continue;
            record.next_report = ai_time + D::kReportMs;
            unsigned slot = 0;
            for (const auto& turret : city->mTurrets) {
                const unsigned index = slot++;
                if (!turret || turret->mbIsDestroyed || turret->mHealthPoints <= 0 || !combat_journal.Reserve()) continue;
                const auto target = D::Resolve(turret->mpTarget.get(), true, civs);
                const float distance = target.Known() ? D::Distance(turret->mPosition, target.position) : -1.0f;
                ModAPI::Log("[Better Civ Combat] t=%llu city=%u owner=%u capture=%.3f conversion=%.3f turret-slot=%u turret=%u frozen=%d health=%.1f aim=%s:%u aim-distance=%.2f position=%.2f,%.2f,%.2f",
                    ai_time, city->mID, civ->mID, city->mCapturePercent, city->mConvertPercent,
                    index, turret->mID, turret->mFreezeCount, turret->mHealthPoints,
                    D::Name(target.kind), target.id, distance,
                    turret->mPosition.x, turret->mPosition.y, turret->mPosition.z);
            }
        }
    }
    for (auto& comm : combat_journal.civs) {
        if (!comm.seen || !comm.present || comm.last_seen == ai_time) continue;
        comm.present = false;
        if (combat_journal.Reserve())
            ModAPI::Log("[Better Civ Combat] t=%llu civ=%u left-live-list last-event=%u last-slot=%d last-cities=%u",
                ai_time, uint32_t(comm.key >> 32), comm.event, comm.slot, comm.cities);
    }
    if (combat_journal.lines >= D::kMaxLines && !combat_journal.limit_notified) {
        combat_journal.limit_notified = true;
        ModAPI::Log("[Better Civ Combat] Log limit reached; restart Spore for another diagnostic session.");
    }
}
