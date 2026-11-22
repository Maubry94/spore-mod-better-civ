static VehicleOrder combat_fixture_order;
static const VehicleOrder* __fastcall combat_fixture_order_getter(Vehicle*, void*) {
    return &combat_fixture_order;
}

static void verify_combat_diagnostics()
{
    namespace D = CombatDiagnostics;
    D::Journal journal;
    assert(!journal.SampleDue(false, true, true, 1, 0));
    assert(!journal.SampleDue(true, false, true, 1, 0));
    assert(!journal.SampleDue(true, true, false, 1, 0));
    assert(!journal.SampleDue(true, true, true, 0, 0));
    assert(!journal.SampleDue(true, true, true, -1, 0));
    assert(journal.SampleDue(true, true, true, 1, 0));
    assert(!journal.SampleDue(true, true, true, 1, 4999));
    assert(journal.SampleDue(true, true, true, 1, 5000));
    D::UnitSnapshot s;
    D::UnitRecord r;
    assert(!r.Observe(s, 0)); // Idle with no threat: no initial unit log.
    s.order = 3;
    assert(r.Observe(s, 5000));
    assert(!r.Observe(s, 10000));
    assert(r.Observe(s, 15000) && r.stationary_ms == 10000);
    assert(!r.Observe(s, 20000));
    s.position.x = 2;
    assert(!r.Observe(s, 25000) && r.stationary_ms == 0);
    s.aim.kind = D::Kind::Turret; s.aim.id = 22;
    assert(!r.Observe(s, 30000) && r.stationary_ms == 5000);
    assert(r.Observe(s, 40000) && r.stationary_ms == 15000);
    s.aim.id=23;r.Observe(s,41000);assert(r.stationary_ms==16000);
    s.owner = 8; r.Observe(s, 45000); assert(!r.stationary);
    r.Observe(s, 1); assert(r.stationary_ms == 0); // Timer rollback cannot underflow.
    s.position.x = std::numeric_limits<float>::quiet_NaN();
    r.Observe(s, 20000); assert(!r.stationary); // Invalid geometry is not evidence.
    for (unsigned i = 1; i <= D::kMaxUnits + 4; ++i) {
        auto& entry = D::Journal::For(journal.units, i, i);
        entry.Observe({}, i);
    }
    bool oldest_present = false;
    for (const auto& entry : journal.units) oldest_present |= entry.key == 1;
    assert(!oldest_present);
    D::CommRecord comm;
    assert(comm.Observe(UINT32_MAX, 0, 1));
    assert(!comm.Observe(UINT32_MAX, 0, 1));
    assert(comm.Observe(7, 0, 1) && comm.Observe(UINT32_MAX, -1, 0));
    assert(journal.Reserve(D::kMaxLines-1) && !journal.Reserve(2));
    assert(journal.Reserve() && !journal.Reserve());
    journal.ResetSession();
    assert(journal.lines == D::kMaxLines && !journal.SampleDue(true, true, true, 1, 0));
    for (const auto& entry : journal.units) assert(!entry.key && !entry.seen);

    // The real SDK reader/resolver and observer, with fake engine-owned storage.
    // Even an invalid target address must be treated as unresolved, not read.
    alignas(8) unsigned char civ_bytes[sizeof(Civilization)] = {};
    alignas(8) unsigned char city_bytes[sizeof(City)] = {};
    alignas(8) unsigned char unit_bytes[sizeof(Vehicle)] = {};
    alignas(8) unsigned char turret_bytes[sizeof(Simulator::cTurret)] = {};
    alignas(8) unsigned char culture_bytes[sizeof(Simulator::cCulturalTarget)] = {};
    auto* civ = reinterpret_cast<Civilization*>(civ_bytes);
    auto* city = reinterpret_cast<City*>(city_bytes);
    auto* unit = reinterpret_cast<Vehicle*>(unit_bytes);
    auto* turret = reinterpret_cast<Simulator::cTurret*>(turret_bytes);
    auto* culture = reinterpret_cast<Simulator::cCulturalTarget*>(culture_bytes);
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ, 0, vtable);
    civ->mID = 12; city->mID = 20; unit->mID = 30; turret->mID = 40; culture->mID = 50;
    unit->mPurpose = kVehicleCultural; unit->mHealthPoints = 10; unit->mDesiredSpeed = 7;
    unit->mPosition.x = 11; unit->mVelocity.y = 3; unit->mTimeSinceLastWeaponFireMS = 777;
    city->mConvertPercent = 5; turret->mHealthPoints = 10; turret->mFreezeCount = 1;
    City* cities[] = {city}; Vehicle* units[] = {unit};
    Simulator::cTurret* turrets[] = {turret}; Simulator::cCulturalTarget* cultures[] = {culture};
    put_pointer(civ, kCivCitiesBegin, cities); put_pointer(civ, kCivCitiesEnd, cities + 1);
    put_pointer(civ, kCivVehiclesBegin, units); put_pointer(civ, kCivVehiclesEnd, units + 1);
    put_pointer(city, 0x354, turrets); put_pointer(city, 0x358, turrets + 1);
    put_pointer(city, 0x62c, cultures); put_pointer(city, 0x630, cultures + 1);
    auto* aim = static_cast<Simulator::cCombatant*>(turret);
    memcpy(&unit->mpTarget, &aim, sizeof aim);
    memcpy(&unit->mpNearestCity, &city, sizeof city);
    portrait_fixture_civs.push_back(civ);
    const auto& civs = portrait_fixture_civs;
    assert(D::Resolve(nullptr, false, civs).kind == D::Kind::None);
    assert(D::Resolve(reinterpret_cast<void*>(1), false, civs).kind == D::Kind::Unknown);
    assert(D::Resolve(reinterpret_cast<void*>(1), true, civs).kind == D::Kind::Unknown);
    assert(D::Resolve(city, false, civs).kind == D::Kind::City);
    assert(D::Resolve(aim, true, civs).id == 40);
    assert(D::Resolve(culture, false, civs).kind == D::Kind::Culture);
    assert(D::Resolve(static_cast<Simulator::cCombatant*>(culture), true, civs).city == 20);
    assert(D::Resolve(static_cast<Simulator::cCombatant*>(unit), true, civs).kind == D::Kind::Vehicle);
    const auto unit_s = D::ReadUnit(*unit, 12);
    assert(unit_s.id == 30 && unit_s.owner == 12 && unit_s.desired_speed == 7);
    assert(unit_s.position.x == 11 && unit_s.velocity.y == 3 && unit_s.since_fire_ms == 777);
    const auto before_civ = std::string(reinterpret_cast<char*>(civ_bytes), sizeof civ_bytes);
    const auto before_city = std::string(reinterpret_cast<char*>(city_bytes), sizeof city_bytes);
    const auto before_unit = std::string(reinterpret_cast<char*>(unit_bytes), sizeof unit_bytes);
    const auto before_turret = std::string(reinterpret_cast<char*>(turret_bytes), sizeof turret_bytes);
    const auto before_culture = std::string(reinterpret_cast<char*>(culture_bytes), sizeof culture_bytes);
    const auto saved_order = current_vehicle_order;
    combat_fixture_order = {}; combat_fixture_order.kind = 3; combat_fixture_order.target = city;
    current_vehicle_order = reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    portrait_civilizations = mock_portrait_civilizations;
    current_mode = kCivilizationMode; game_mode = mock_mode; ai_observer_ready = true;
    combat_journal = {}; conversion_journal = {}; settings = defaults; ai_time = 0;
    ai_lifecycle.Update(1, 1); assert(combat_journal.lines == 0);
    settings.ai_diagnostics = 1;
    ai_lifecycle.Update(1, 0); assert(combat_journal.lines == 0);
    ai_lifecycle.Update(1, 1); assert(combat_journal.lines == 3); // Civ, vehicle, turret.
    ai_lifecycle.Update(1, 1); assert(combat_journal.lines == 3);
    ai_lifecycle.Update(1, 5000); assert(combat_journal.lines == 3);
    ai_lifecycle.Update(1, 5000); assert(combat_journal.lines == 4); // Stationary observation.
    assert(ai_lifecycle.Write(nullptr));
    ai_lifecycle.Read(nullptr); assert(!combat_journal.units[0].key && combat_journal.lines == 4);
    assert(!memcmp(civ_bytes, before_civ.data(), sizeof civ_bytes));
    assert(!memcmp(city_bytes, before_city.data(), sizeof city_bytes));
    assert(!memcmp(unit_bytes, before_unit.data(), sizeof unit_bytes));
    assert(!memcmp(turret_bytes, before_turret.data(), sizeof turret_bytes));
    assert(!memcmp(culture_bytes, before_culture.data(), sizeof culture_bytes));
    unit->mbDead = true;
    assert(D::Resolve(static_cast<Simulator::cCombatant*>(unit), true, civs).kind == D::Kind::Unknown);
    civ->mbIsDestroyed = true;
    assert(D::Resolve(city, false, civs).kind == D::Kind::Unknown);
    portrait_fixture_civs.clear(); portrait_civilizations = sdk_portrait_civilizations;
    current_vehicle_order = saved_order;
    combat_journal = {}; conversion_journal = {}; settings = defaults;
    ai_log_lines = 0; ai_observer_ready = false;
    assert(ai_refs == 0);
    puts("Combat diagnostics: unknown-pointer safety, stationary observations, bounded logs, read-only SDK adapter and lifecycle passed.");
}
