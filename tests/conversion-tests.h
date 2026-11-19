static void verify_conversion_diagnostics()
{
    using namespace Conversion;
    Journal journal;
    assert(!journal.SampleDue(false, true, true, 1, 0));
    assert(!journal.SampleDue(true, false, true, 1, 0));
    assert(!journal.SampleDue(true, true, false, 1, 0));
    assert(!journal.SampleDue(true, true, true, 0, 0));
    assert(!journal.SampleDue(true, true, true, -1, 0));
    assert(journal.SampleDue(true, true, true, 1, 0));
    assert(!journal.SampleDue(true, true, true, 1, 999));
    assert(journal.SampleDue(true, true, true, 1, 1000));
    Snapshot s;
    auto& r = journal.For(42, 0);
    assert(r.Observe(s, 0)); // Initial per-city baseline even before an attack.
    assert(!r.Observe(s, 10000)); // Quiet cities do not flood the journal.
    s.ordered = 6;
    assert(r.Observe(s, 11000));
    assert(!r.Observe(s, 12000));
    s.in_view = true;
    assert(r.Observe(s, 13000)); // Camera evidence is not delayed ten seconds.
    s.active = 1; s.registered = 3;
    assert(r.Observe(s, 14000));
    s.progress = 0.25f;
    assert(r.Observe(s, 15000));
    s.in_view = false;
    assert(r.Observe(s, 16000));
    s.progress = 1;
    assert(!r.Observe(s, 17000) && r.Observe(s, 26000));
    ++s.owner;
    assert(r.Observe(s, 27000));
    s = {};
    assert(r.Observe(s, 28000) && !r.Observe(s, 40000));
    for (unsigned i = 1; i <= kMaxCities + 4; ++i) journal.For(100 + i, i);
    assert(journal.Reserve(kMaxLines - 1) && !journal.Reserve(2));
    assert(journal.Reserve(1) && !journal.Reserve(1));
    journal.ResetSession();
    assert(journal.lines == kMaxLines && !journal.SampleDue(true, true, true, 1, 0));
    for (const auto& entry : journal.records) assert(!entry.key && !entry.seen);

    // Production SDK field readers, using the pinned layouts. The fixture
    // never constructs engine-owned objects or asks the game allocator to run.
    alignas(8) unsigned char city_bytes[sizeof(City)] = {};
    alignas(8) unsigned char target_bytes[3][sizeof(Simulator::cCulturalTarget)] = {};
    alignas(8) unsigned char turret_bytes[3][sizeof(Simulator::cTurret)] = {};
    alignas(8) unsigned char unit_bytes[sizeof(Vehicle)] = {};
    alignas(8) unsigned char bucket_bytes[3 * sizeof(Simulator::tCultureTargetInfo)] = {};
    auto* city = reinterpret_cast<City*>(city_bytes);
    auto* unit = reinterpret_cast<Vehicle*>(unit_bytes);
    auto* buckets = reinterpret_cast<Simulator::tCultureTargetInfo*>(bucket_bytes);
    Simulator::cCulturalTarget* targets[4] = {};
    Simulator::cTurret* turrets[3] = {};
    void* registered[] = {unit, unit};
    for (unsigned i = 0; i < 3; ++i) {
        targets[i] = reinterpret_cast<Simulator::cCulturalTarget*>(target_bytes[i]);
        targets[i]->mID = 20 + i;
        targets[i]->mAttackState = int(i);
        targets[i]->mAttackerPoliticalID = 100 + i;
        turrets[i] = reinterpret_cast<Simulator::cTurret*>(turret_bytes[i]);
        turrets[i]->mHealthPoints = 10;
        buckets[i].mBucketPopulation[0] = 4 + int(i);
        buckets[i].mBucketPopulation[1] = 1 + int(i);
        buckets[i].mBucketMaxPopulation[0] = 20;
        buckets[i].mBucketMaxPopulation[1] = 30;
    }
    turrets[0]->mFreezeCount = 1; turrets[2]->mbIsDestroyed = true;
    put_pointer(targets[2], 0x1f4, registered); put_pointer(targets[2], 0x1f8, registered + 2);
    put_pointer(city, 0x62c, targets); put_pointer(city, 0x630, targets + 4);
    put_pointer(city, 0x618, buckets); put_pointer(city, 0x61c, buckets + 3);
    put_pointer(city, 0x354, turrets); put_pointer(city, 0x358, turrets + 3);
    city->mID = 12; city->mPoliticalID = 7; city->mConvertID = 9;
    city->mVehicleSpecialty = kVehicleCultural;
    city->mConvertPercent = 12.5f; city->mbInView = true; city->mDistanceFromCamera = 800;
    const auto city_before = std::string(reinterpret_cast<char*>(city_bytes), sizeof city_bytes);
    const auto targets_before = std::string(reinterpret_cast<char*>(target_bytes), sizeof target_bytes);
    const auto buckets_before = std::string(reinterpret_cast<char*>(bucket_bytes), sizeof bucket_bytes);
    const auto turrets_before = std::string(reinterpret_cast<char*>(turret_bytes), sizeof turret_bytes);
    auto snapshot = ReadCity(*city);
    assert(snapshot.city == 12 && snapshot.owner == 7 && snapshot.credited == 9);
    assert(snapshot.specialty == kVehicleCultural);
    assert(snapshot.progress == 12.5f && snapshot.in_view && snapshot.distance == 800);
    assert(snapshot.targets == 4 && snapshot.buckets == 3 && snapshot.registered == 2);
    assert(snapshot.disabled == 1 && snapshot.ready == 1 && snapshot.active == 1);
    assert(snapshot.slots[2].listening == 6 && snapshot.slots[2].converted == 3);
    assert(snapshot.slots[2].listening_capacity == 20 && snapshot.slots[2].converted_capacity == 30);
    assert(snapshot.slots[3].id == UINT32_MAX && snapshot.turrets == 2 && snapshot.frozen == 1);
    CountAim(snapshot, *city, *unit);
    auto set_aim = [&](Simulator::cCombatant* aim) { memcpy(&unit->mpTarget, &aim, sizeof aim); };
    set_aim(targets[2]); CountAim(snapshot, *city, *unit);
    set_aim(turrets[0]); CountAim(snapshot, *city, *unit);
    set_aim(reinterpret_cast<Simulator::cCombatant*>(uintptr_t(0x1234))); CountAim(snapshot, *city, *unit);
    set_aim(nullptr);
    assert(snapshot.no_aim == 1 && snapshot.cultural_aim == 1 && snapshot.turret_aim == 1 && snapshot.other_aim == 1);
    assert(!memcmp(city_bytes, city_before.data(), sizeof city_bytes));
    assert(!memcmp(target_bytes, targets_before.data(), sizeof target_bytes));
    assert(!memcmp(bucket_bytes, buckets_before.data(), sizeof bucket_bytes));
    assert(!memcmp(turret_bytes, turrets_before.data(), sizeof turret_bytes));

    // Actual lifecycle -> production enumerator -> reader/log path. There are
    // no additional native patches; diagnostics Off and paused frames never
    // enumerate. Read/load resets observations but not the per-launch cap.
    alignas(8) unsigned char civ_bytes[sizeof(Civilization)] = {};
    auto* civ = reinterpret_cast<Civilization*>(civ_bytes);
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ, 0, vtable);
    City* cities[] = {city};
    put_pointer(civ, kCivCitiesBegin, cities); put_pointer(civ, kCivCitiesEnd, cities + 1);
    portrait_fixture_civs.push_back(civ);
    portrait_civilizations = mock_portrait_civilizations;
    current_mode = kCivilizationMode; game_mode = mock_mode; ai_observer_ready = true;
    conversion_journal = {}; ai_time = 0; settings = defaults;
    ai_lifecycle.Update(1, 1); assert(conversion_journal.lines == 0);
    settings.ai_diagnostics = 1;
    ai_lifecycle.Update(1, 0); assert(conversion_journal.lines == 0);
    ai_lifecycle.Update(1, 1); assert(conversion_journal.lines == 5);
    ai_lifecycle.Update(1, 1); assert(conversion_journal.lines == 5);
    assert(!memcmp(city_bytes, city_before.data(), sizeof city_bytes));
    ai_lifecycle.Read(nullptr);
    assert(!conversion_journal.records[0].key && conversion_journal.lines == 5);
    portrait_fixture_civs.clear(); portrait_civilizations = sdk_portrait_civilizations;
    conversion_journal = {}; settings = defaults; ai_log_lines = 0; ai_observer_ready = false;
    assert(ai_refs == 0);
    puts("Conversion diagnostics: read-only SDK fields, camera transitions, cadence, caps and lifecycle passed.");
}
