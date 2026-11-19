static void verify_movement_diagnostics()
{
    namespace M = Movement;
    namespace D = CombatDiagnostics;
    M::Snapshot s;
    s.unit.order = 2;
    assert(!s.Eligible());
    s.in_view = true; assert(s.Eligible());
    s.unit.order = 0; assert(!s.Eligible());
    s.city_under_attack = true; assert(s.Eligible()); // Idle defenders are observed.
    s.city_under_attack = false; s.unit.hostiles = 1; assert(s.Eligible());
    s.unit.hostiles = 0; s.unit.aim.kind = D::Kind::Building; assert(s.Eligible());
    s.unit.aim = {}; s.unit.order = 2;

    M::Record straight, loop;
    s.unit.velocity = {4,0,0};
    straight.Observe(s, 0); straight.Reported(0);
    for (unsigned i = 1; i <= 8; ++i) {
        s.unit.position = {float(i),0,0};
        straight.Observe(s, i*250);
    }
    assert(straight.Due(2000) && straight.valid && straight.samples == 8);
    assert(straight.path == 8 && D::Distance(s.unit.position, straight.anchor) == 8);
    assert(straight.turn_degrees < 0.01f);
    s.unit.position = {}; s.unit.velocity = {4,0,0};
    loop.Observe(s, 0); loop.Reported(0);
    const Math::Vector3 points[] = {{1,0,0},{1,1,0},{0,1,0},{0,0,0}};
    const Math::Vector3 velocities[] = {{0,4,0},{-4,0,0},{0,-4,0},{4,0,0}};
    for (unsigned i = 0; i < 4; ++i) {
        s.unit.position = points[i]; s.unit.velocity = velocities[i];
        loop.Observe(s, (i+1)*250);
    }
    assert(loop.valid && loop.path == 4 && D::Distance(s.unit.position, loop.anchor) == 0);
    assert(std::abs(loop.turn_degrees - 360) < 0.01f);
    s.unit.aim = {D::Kind::Building, 8, 9, {}};
    loop.Observe(s, 1250); assert(loop.aim_changes == 1 && loop.Due(1250));
    s.unit.order_target = {D::Kind::City, 9, 9, {}};
    loop.Observe(s, 1500); assert(loop.order_changes == 1);
    loop.Reported(1500);
    assert(!loop.order_changes && !loop.aim_changes && !loop.path && !loop.Due(1750));
    s.unit.aim.id = 10;
    loop.Observe(s, 1750); assert(!loop.Due(1750));
    loop.Observe(s, 2000); assert(loop.Due(2000));
    loop.Observe(s, 4000); assert(loop.start == 4000 && loop.path == 0 && loop.samples == 0);
    loop.Observe(s, 1); assert(loop.start == 1 && loop.path == 0); // Clock rollback.
    s.unit.position.x = std::numeric_limits<float>::quiet_NaN();
    loop.Observe(s, 251); assert(!loop.valid);
    s.unit.position = {}; loop.Observe(s, 501); assert(!loop.valid);
    loop.Reported(501); assert(loop.valid); // New finite window after invalid data.

    M::Journal journal;
    assert(!journal.SampleDue(false, true, true, 1, 0));
    assert(!journal.SampleDue(true, false, true, 1, 0));
    assert(!journal.SampleDue(true, true, false, 1, 0));
    assert(!journal.SampleDue(true, true, true, 0, 0));
    assert(!journal.SampleDue(true, true, true, -1, 0));
    assert(journal.SampleDue(true, true, true, 1, 0));
    assert(!journal.SampleDue(true, true, true, 1, 249));
    for (unsigned i = 0; i < M::kBurst; ++i) assert(journal.Reserve());
    assert(!journal.Reserve() && journal.suppressed == 1);
    assert(journal.SampleDue(true, true, true, 1, 250));
    assert(journal.tokens == 8);
    for (unsigned i = 0; i < 8; ++i) assert(journal.Reserve());
    assert(!journal.Reserve());
    journal.lines = M::kMaxLines - 1; journal.tokens = 2;
    assert(journal.Reserve() && !journal.Reserve());
    journal.ResetSession();
    assert(journal.lines == M::kMaxLines && !journal.SampleDue(true, true, true, 1, 0));

    // Exercise the SDK resolver and real lifecycle/observer with live collection
    // fixtures. The observer must not touch a byte of an engine-owned object.
    alignas(8) unsigned char civ_bytes[sizeof(Civilization)] = {};
    alignas(8) unsigned char city_bytes[sizeof(City)] = {};
    alignas(8) unsigned char unit_bytes[sizeof(Vehicle)] = {};
    alignas(8) unsigned char turret_bytes[sizeof(Simulator::cTurret)] = {};
    alignas(8) unsigned char building_bytes[sizeof(Simulator::cBuilding)] = {};
    auto* civ = reinterpret_cast<Civilization*>(civ_bytes);
    auto* city = reinterpret_cast<City*>(city_bytes);
    auto* unit = reinterpret_cast<Vehicle*>(unit_bytes);
    auto* turret = reinterpret_cast<Simulator::cTurret*>(turret_bytes);
    auto* building = reinterpret_cast<Simulator::cBuilding*>(building_bytes);
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ, 0, vtable);
    civ->mID = 12; city->mID = 20; unit->mID = 30; turret->mID = 40; building->mID = 50;
    unit->mbInView = true; unit->mPurpose = kVehicleMilitary;
    unit->mPosition = {1,2,3}; unit->mVelocity = {0,4,0}; unit->mAngularVelocity = {1,0,0};
    unit->mDesiredSpeed = 4; city->mCapturePercent = 5;
    turret->mHealthPoints = building->mHealthPoints = 100;
    building->mPosition = {5,2,3};
    City* cities[] = {city}; Vehicle* units[] = {unit};
    Simulator::cTurret* turrets[] = {turret}; Simulator::cBuilding* buildings[] = {building};
    put_pointer(civ, kCivCitiesBegin, cities); put_pointer(civ, kCivCitiesEnd, cities+1);
    put_pointer(civ, kCivVehiclesBegin, units); put_pointer(civ, kCivVehiclesEnd, units+1);
    put_pointer(city, 0x340, buildings); put_pointer(city, 0x344, buildings+1);
    put_pointer(city, 0x354, turrets); put_pointer(city, 0x358, turrets+1);
    auto* aim = static_cast<Simulator::cCombatant*>(building);
    memcpy(&unit->mpTarget, &aim, sizeof aim);
    memcpy(&unit->mpNearestCity, &city, sizeof city);
    portrait_fixture_civs.push_back(civ);
    assert(D::Resolve(building, false, portrait_fixture_civs).kind == D::Kind::Building);
    const auto target = D::Resolve(aim, true, portrait_fixture_civs);
    assert(target.kind == D::Kind::Building && target.id == 50 && target.city == 20 && target.position.x == 5);
    assert(D::Resolve(reinterpret_cast<void*>(1), true, portrait_fixture_civs).kind == D::Kind::Unknown);
    building->mbIsDestroyed = true;
    assert(D::Resolve(aim, true, portrait_fixture_civs).kind == D::Kind::Unknown);
    building->mbIsDestroyed = false;
    const auto before_civ = std::string(reinterpret_cast<char*>(civ_bytes), sizeof civ_bytes);
    const auto before_city = std::string(reinterpret_cast<char*>(city_bytes), sizeof city_bytes);
    const auto before_unit = std::string(reinterpret_cast<char*>(unit_bytes), sizeof unit_bytes);
    const auto before_turret = std::string(reinterpret_cast<char*>(turret_bytes), sizeof turret_bytes);
    const auto before_building = std::string(reinterpret_cast<char*>(building_bytes), sizeof building_bytes);
    const auto saved_order = current_vehicle_order;
    combat_fixture_order = {}; combat_fixture_order.kind = 2; combat_fixture_order.target = city;
    current_vehicle_order = reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    portrait_civilizations = mock_portrait_civilizations;
    current_mode = kCivilizationMode; game_mode = mock_mode; ai_observer_ready = true;
    movement_journal = {}; combat_journal = {}; conversion_journal = {}; settings = defaults; ai_time = 0;
    ai_lifecycle.Update(1, 1); assert(movement_journal.lines == 0);
    settings.ai_diagnostics = 1;
    ai_lifecycle.Update(1, 0); assert(movement_journal.lines == 0);
    ai_lifecycle.Update(1, 1); assert(movement_journal.lines == 3); // City, turret, unit.
    assert(movement_journal.units[0].previous.aim.kind == D::Kind::Building);
    assert(movement_journal.units[0].previous.order_target.id == city->mID);
    ai_lifecycle.Update(1, 249); assert(movement_journal.units[0].samples == 0);
    ai_lifecycle.Update(1, 1); assert(movement_journal.units[0].samples == 1);
    ai_lifecycle.Update(1, 0); assert(movement_journal.units[0].samples == 1);
    assert(!memcmp(civ_bytes, before_civ.data(), sizeof civ_bytes));
    assert(!memcmp(city_bytes, before_city.data(), sizeof city_bytes));
    assert(!memcmp(unit_bytes, before_unit.data(), sizeof unit_bytes));
    assert(!memcmp(turret_bytes, before_turret.data(), sizeof turret_bytes));
    assert(!memcmp(building_bytes, before_building.data(), sizeof building_bytes));
    unit->mbInView = false;
    ai_lifecycle.Update(1, 250); assert(movement_journal.units[0].samples == 1);
    unit->mbInView = true; unit->mbDead = true;
    ai_lifecycle.Update(1, 250); assert(movement_journal.units[0].samples == 1);
    unit->mbDead = false;
    const uintptr_t foreign = 1; memcpy(&unit->mpNearestCity, &foreign, sizeof foreign);
    memcpy(&unit->mpTarget, &foreign, sizeof foreign);
    ai_lifecycle.Update(1, 250); // Unmatched pointers must never be dereferenced.
    assert(movement_journal.units[0].previous.aim.kind == D::Kind::Unknown);
    const auto lines = movement_journal.lines;
    ai_lifecycle.Read(nullptr); assert(!movement_journal.units[0].key && movement_journal.lines == lines);
    portrait_fixture_civs.clear(); portrait_civilizations = sdk_portrait_civilizations;
    current_vehicle_order = saved_order; settings = defaults; ai_observer_ready = false; ai_log_lines = 0;
    movement_journal = {}; combat_journal = {}; conversion_journal = {};
    assert(ai_refs == 0);
    puts("Movement diagnostics: building resolution, visible combat, trajectories, transitions, rate caps, read-only SDK adapter and lifecycle passed.");
}
