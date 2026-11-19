// Included by the existing offline SDK fixture after its native mocks.
static void verify_offensive_policy()
{
    settings = defaults;
    apply_option(&settings, "<OffensiveTargets>RethinkFailedAttacks");
    apply_option(&settings, "<AIDiagnostics>DiagnosticsOn");
    assert(settings.rethink_offensives == 1 && settings.ai_diagnostics == 1);
    apply_option(&settings, "<OffensiveTargets>TargetsVanilla");
    apply_option(&settings, "<AIDiagnostics>DiagnosticsOff");
    assert(memcmp(&settings, &defaults, sizeof settings) == 0);
    using namespace Offensive;
    for (uint32_t seed = 0; seed < 30; ++seed) {
        State state;
        Observation o{100, uint64_t(12) << 32, 7, 2, 0, 0, 0};
        assert(state.Observe(o, seed) == Event::Target);
        assert(state.threshold >= 2 && state.threshold <= 4);
        const unsigned threshold = state.threshold;
        // Each failure needs a real group, complete absence of assigned units,
        // then a full quiet period. One survivor prevents premature rotation.
        for (unsigned wave = 1; wave <= threshold; ++wave) {
            o.now += 1; o.committed = 3;
            assert(state.Observe(o, seed) == Event::Started);
            o.now += 90000; o.committed = 1;
            state.Observe(o, seed);
            assert(state.wave && !state.CanReconsider(o));
            o.now += 1; o.committed = 0;
            state.Observe(o, seed);
            o.now += kQuietMs - 1;
            state.Observe(o, seed);
            assert(state.wave && !state.CanReconsider(o));
            o.now += 1;
            assert(state.Observe(o, seed) == Event::Failed);
            assert(state.failures == wave);
            assert(state.CanReconsider(o) == (wave == threshold));
            // An ended wave is counted exactly once.
            o.now += 100;
            assert(state.Observe(o, seed) == Event::None && state.failures == wave);
        }
        state.NoAlternative(o.now);
        assert(!state.CanReconsider(o));
        o.now += kRetryMs;
        assert(state.CanReconsider(o));
        o.progress = 0.05f; o.now += 1;
        assert(state.Observe(o, seed) == Event::Progress && state.failures == 0);
        assert(!state.CanReconsider(o));
    }
    State state;
    Observation o{0, 11, 7, 2, 0, 0, 0};
    state.Observe(o, 1);
    o.committed = 2; state.Observe(o, 1);
    o.committed = 0; o.now = 100000; state.Observe(o, 1);
    assert(!state.wave && !state.failures); // Scouts are not a full wave.
    o.committed = 3; state.Observe(o, 1);
    o.committed = 0; o.now += 1; state.Observe(o, 1);
    o.now += kQuietMs - 1; o.committed = 1; state.Observe(o, 1);
    o.committed = 0; o.uncertain = 1; o.now += 200000; state.Observe(o, 1);
    assert(state.wave && !state.failures && !state.CanReconsider(o));
    o.uncertain = 0; o.progress = 0.2f; state.Observe(o, 1);
    o.now += 1; state.Observe(o, 1);
    o.now += kQuietMs;
    assert(state.Observe(o, 1) == Event::EndedWithProgress && !state.failures);
    state.failures = 4;
    o.territory++;
    assert(state.Observe(o, 1) == Event::Target && !state.failures);
    state.failures = 4; o.owner++;
    assert(state.Observe(o, 1) == Event::Target && !state.failures);
    state.failures = 4; o.target++;
    assert(state.Observe(o, 1) == Event::Target && !state.failures);
    state.failures = 4; o.now = 0;
    assert(state.Observe(o, 1) == Event::Target && !state.failures);
    o.progress = NAN;
    assert(state.Observe(o, 1) == Event::None && !state.CanReconsider(o));

    for (const auto seconds : {120, 180, 240}) {
        Settings choice = defaults;
        char option[64]; sprintf_s(option, "<StalledOffensives>Stalled%d", seconds);
        apply_option(&choice, option); assert(choice.stalled_seconds == seconds);
        apply_option(&choice, "<StalledOffensives>StalledOff");
        assert(memcmp(&choice, &defaults, sizeof choice) == 0);
        State siege;
        Observation s{0, 11, 7, 2, 15, 0, 0};
        siege.Observe(s, 1);
        s.now = 600000; siege.Observe(s, 1);
        assert(!siege.engaged && !siege.IsStalled(s, seconds * 1000)); // Still travelling.
        s.nearby = 1; siege.Observe(s, 1);
        s.now += seconds * 1000 - 1; s.committed = 27;
        siege.Observe(s, 1); assert(!siege.IsStalled(s, seconds * 1000));
        ++s.now; siege.Observe(s, 1);
        assert(siege.IsStalled(s, seconds * 1000)); // Reinforcements cannot extend it.
        assert(!siege.IsStalled(s, 0));
        s.progress = 20; siege.Observe(s, 1);
        assert(!siege.IsStalled(s, seconds * 1000));
        s.now += seconds * 1000;
        s.progress = 0; siege.Observe(s, 1);
        s.progress = 20; siege.Observe(s, 1);
        assert(siege.progress == 20 && siege.IsStalled(s, seconds * 1000));
        s.progress = 20.5f; siege.Observe(s, 1);
        assert(!siege.IsStalled(s, seconds * 1000)); // Actual net advance protects siege.
        s.progress = NAN; siege.Observe(s, 1);
        assert(!siege.IsStalled(s, seconds * 1000));
        s.progress = 20.5f; ++s.owner; siege.Observe(s, 1);
        assert(!siege.engaged && !siege.IsStalled(s, seconds * 1000));
    }
    Cooldown cooldown{11, 120000, 2};
    assert(cooldown.Blocks(11, 2, 119999));
    assert(!cooldown.Blocks(12, 2, 1) && !cooldown.Blocks(11, 3, 1) && !cooldown.Blocks(11, 2, 120000));
}

static City* ai_test_target;
static City* ai_test_alternative;
static unsigned planner_calls;
static Vehicle* siege_vehicles[7];
static VehicleOrder siege_orders[7];
static unsigned canceled_orders;
static const VehicleOrder* __fastcall mock_siege_order(Vehicle* vehicle, void*) {
    for (unsigned i = 0; i < 7; ++i) if (siege_vehicles[i] == vehicle) return &siege_orders[i];
    return nullptr;
}
static void __fastcall mock_cancel_order(Vehicle* vehicle, void*, void* target, int kind, int flags) {
    assert(!target && !kind && !flags);
    for (unsigned i = 0; i < 7; ++i) if (siege_vehicles[i] == vehicle) {
        siege_orders[i] = {}; ++canceled_orders; return;
    }
    assert(false);
}
static int ai_refs;
static int __fastcall ai_mock_addref(void*, void*) { return ++ai_refs; }
static int __fastcall ai_mock_release(void*, void*) { return --ai_refs; }
static void* __fastcall ai_mock_cast(void* object, void*, uint32_t type)
{ return type == Simulator::cCulturalTarget::TYPE ? object : nullptr; }
static void* __fastcall ai_mock_owner(void*, void*) { return ai_test_target; }
static void __fastcall ai_mock_planner(Civilization* civ, void*, int specialty)
{
    ++planner_calls;
    auto candidate_owner = reinterpret_cast<CityOwner>(filtered_expansion_owner::detoured_function);
    auto& slot = specialty == 0 ? civ->mMilitaryExpansionCity : civ->mReligiousExpansionCity;
    if (candidate_owner(ai_test_target) == civ) slot = ai_test_alternative;
}

static void verify_offensive_adapters()
{
    alignas(8) unsigned char civ_bytes[sizeof(Civilization)] = {}, enemy_bytes[sizeof(Civilization)] = {};
    alignas(8) unsigned char city_bytes[3][sizeof(City)] = {}, vehicle_bytes[sizeof(Vehicle)] = {};
    alignas(8) unsigned char cultural_bytes[sizeof(Simulator::cCulturalTarget)] = {};
    uintptr_t vtable[20] = {};
    vtable[0] = reinterpret_cast<uintptr_t>(ai_mock_addref);
    vtable[1] = reinterpret_cast<uintptr_t>(ai_mock_release);
    vtable[3] = reinterpret_cast<uintptr_t>(ai_mock_cast);
    vtable[12] = reinterpret_cast<uintptr_t>(ai_mock_owner);
    vtable[19] = reinterpret_cast<uintptr_t>(mock_political_id);
    auto* civ = reinterpret_cast<Civilization*>(civ_bytes);
    auto* enemy = reinterpret_cast<Civilization*>(enemy_bytes);
    auto* home = reinterpret_cast<City*>(city_bytes[0]);
    auto* target = reinterpret_cast<City*>(city_bytes[1]);
    auto* alternative = reinterpret_cast<City*>(city_bytes[2]);
    auto* vehicle = reinterpret_cast<Vehicle*>(vehicle_bytes);
    for (void* object : {static_cast<void*>(civ), static_cast<void*>(enemy), static_cast<void*>(home),
        static_cast<void*>(target), static_cast<void*>(alternative), static_cast<void*>(cultural_bytes)})
        put_pointer(object, 0, vtable);
    civ->mID = 1; enemy->mID = 2;
    home->mID = 10; target->mID = 11; alternative->mID = 12;
    put_pointer(target, 0x590, enemy); put_pointer(alternative, 0x590, enemy);
    void* cities[] = {home};
    put_pointer(civ, kCivCitiesBegin, cities); put_pointer(civ, kCivCitiesEnd, cities + 1);
    void* vehicles[] = {vehicle, vehicle, vehicle};
    put_pointer(civ, kCivVehiclesBegin, vehicles); put_pointer(civ, kCivVehiclesEnd, vehicles);
    ai_test_target = target; ai_test_alternative = alternative;
    original_expansion_planner = reinterpret_cast<ExpansionPlanner>(ai_mock_planner);
    city_owner = reinterpret_cast<CityOwner>(mock_city_owner);
    current_vehicle_order = reinterpret_cast<CurrentVehicleOrder>(mock_trade_order);
    game_mode = mock_mode; current_mode = kCivilizationMode;
    relationship_manager = mock_context;
    relationship_level = reinterpret_cast<RelationshipLevel>(mock_relationship);
    origin_opinion = partner_opinion = 0;
    settings = defaults; settings.rethink_offensives = 1;
    ai_observer_ready = true; ai_time = 60000;
    auto planner = reinterpret_cast<ExpansionPlanner>(observed_expansion_planner::detoured_function);
    for (int specialty = 0; specialty < 2; ++specialty) {
        auto& slot = specialty == 0 ? civ->mMilitaryExpansionCity : civ->mReligiousExpansionCity;
        slot = target;
        auto ready = [&]() {
            auto observation = observe_attack(civ, target, specialty);
            auto& state = record_for(civ).attacks[specialty];
            state = {}; state.Observe(observation, 1);
            state.failures = state.threshold; state.quiet = true; state.quiet_since = 0;
        };
        ready(); planner(civ, specialty);
        assert(slot.get() == alternative && selection_filter == nullptr);
        slot = target; ai_test_alternative = nullptr; ready(); planner(civ, specialty);
        assert(slot.get() == target); // No eligible alternative: keep the old city alive.
        assert(record_for(civ).attacks[specialty].retry_after == ai_time + Offensive::kRetryMs);
        ai_test_alternative = alternative;
        // An ongoing siege and unknown suborders veto rotation.
        put_pointer(civ, kCivVehiclesEnd, vehicles + 3);
        vehicle->mPurpose = static_cast<VehiclePurpose>(specialty);
        trade_order.kind = specialty + 2; trade_order.target = target;
        ready(); planner(civ, specialty); assert(slot.get() == target);
        trade_order.target = nullptr;
        ready(); planner(civ, specialty); assert(slot.get() == target);
        if (specialty == 1) {
            trade_order.target = cultural_bytes;
            auto observed = observe_attack(civ, target, specialty);
            assert(observed.committed == 3 && observed.uncertain == 0);
            ready(); planner(civ, specialty); assert(slot.get() == target);
        }
        put_pointer(civ, kCivVehiclesEnd, vehicles);
        // A forced diplomatic assignment is not overridden.
        put_pointer(civ, 0x460, target);
        ready(); planner(civ, specialty); assert(slot.get() == target);
        put_pointer(civ, 0x460, nullptr);
        // Logging alone, friendly targets and the player preserve native choices.
        settings.rethink_offensives = 0; settings.ai_diagnostics = 1;
        ready(); planner(civ, specialty); assert(slot.get() == target);
        settings.rethink_offensives = 1; settings.ai_diagnostics = 0;
        origin_opinion = 2; ready(); planner(civ, specialty); assert(slot.get() == target);
        origin_opinion = 0;
        civ->mIsPlayerOwned = true; ready(); planner(civ, specialty); assert(slot.get() == target);
        civ->mIsPlayerOwned = false;
        slot = nullptr;
    }
    // Continuous siege: real SDK geometry, bounded cancellation and cooldown.
    alignas(8) unsigned char siege_bytes[7][sizeof(Vehicle)] = {};
    alignas(8) unsigned char weapon_bytes[sizeof(Simulator::cSpaceToolData)] = {};
    auto* weapon = reinterpret_cast<Simulator::cSpaceToolData*>(weapon_bytes);
    weapon->mRangeMultiplier = 1; weapon->mMaxRange = 30;
    target->mFootprintRadius = 20;
    for (unsigned i = 0; i < 7; ++i) {
        siege_vehicles[i] = reinterpret_cast<Vehicle*>(siege_bytes[i]);
        put_pointer(siege_vehicles[i], 0, vtable);
        put_pointer(siege_vehicles[i], 0xb60, weapon);
        siege_orders[i] = {}; siege_orders[i].kind = 2; siege_orders[i].target = target;
    }
    siege_orders[3].target = alternative; // A different city's attack.
    siege_vehicles[4]->mPurpose = static_cast<VehiclePurpose>(2); siege_orders[4].kind = 5;
    siege_vehicles[5]->mbDead = true;
    siege_vehicles[6]->mPurpose = static_cast<VehiclePurpose>(1); siege_orders[6].kind = 3;
    current_vehicle_order = reinterpret_cast<CurrentVehicleOrder>(mock_siege_order);
    set_vehicle_order = reinterpret_cast<SetVehicleOrder>(mock_cancel_order);
    put_pointer(civ, kCivVehiclesBegin, siege_vehicles); put_pointer(civ, kCivVehiclesEnd, siege_vehicles + 7);
    settings = defaults; settings.stalled_seconds = 180;
    civ->mMilitaryExpansionCity = target;
    reset_ai_observations(); canceled_orders = 0;
    planner(civ, 0); ++ai_time; planner(civ, 0);
    auto& siege_state = record_for(civ).attacks[0];
    assert(siege_state.engaged);
    auto observed = observe_attack(civ, target, 0);
    assert(observed.committed == 3 && observed.nearby == 3 && observed.uncertain == 1);
    siege_vehicles[0]->mPosition.x = 51;
    assert(!near_attack_city(siege_vehicles[0], target));
    siege_vehicles[0]->mPosition.x = 0;
    target->mCapturePercent = 10; target->mCaptureID = 999;
    ai_time = 180001; planner(civ, 0);
    assert(canceled_orders == 0 && civ->mMilitaryExpansionCity.get() == target);
    target->mCaptureID = 42; // Changing the credited attacker is not progress.
    ai_time += 180000;
    civ->mIsPlayerOwned = true; planner(civ, 0);
    assert(canceled_orders == 0 && civ->mMilitaryExpansionCity.get() == target);
    civ->mIsPlayerOwned = false; civ->mDiploAttackCity = target; planner(civ, 0);
    assert(canceled_orders == 0 && civ->mMilitaryExpansionCity.get() == target);
    civ->mDiploAttackCity = nullptr;
    settings.stalled_seconds = 0; settings.ai_diagnostics = 1; planner(civ, 0);
    assert(canceled_orders == 0 && civ->mMilitaryExpansionCity.get() == target);
    settings.stalled_seconds = 180; settings.ai_diagnostics = 0;
    planner(civ, 0);
    assert(canceled_orders == 3 && civ->mMilitaryExpansionCity.get() == alternative);
    assert(siege_orders[3].kind == 2 && siege_orders[4].kind == 5 && siege_orders[5].kind == 2 && siege_orders[6].kind == 3);
    // With no alternate candidate the old city must NOT be restored.
    ai_test_alternative = nullptr; civ->mMilitaryExpansionCity = nullptr;
    planner(civ, 0); assert(!civ->mMilitaryExpansionCity);
    SelectionFilter cooling_filter{civ, nullptr, &record_for(civ).avoided[0], false};
    selection_filter = &cooling_filter;
    auto candidate_owner = reinterpret_cast<CityOwner>(filtered_expansion_owner::detoured_function);
    assert(candidate_owner(target) == civ);
    ai_time += Offensive::kAvoidMs;
    assert(candidate_owner(target) == enemy);
    selection_filter = nullptr;
    // A first timeout with no alternative must also stop reinforcement, rather
    // than restoring the stalled city through the older failed-wave fallback.
    reset_ai_observations(); civ->mMilitaryExpansionCity = target;
    for (unsigned i = 0; i < 3; ++i) { siege_orders[i].kind = 2; siege_orders[i].target = target; }
    planner(civ, 0); ++ai_time; planner(civ, 0);
    ai_time += 180000; planner(civ, 0);
    assert(canceled_orders == 6 && !civ->mMilitaryExpansionCity);
    // The same order matcher handles religious cultural subtargets, but never
    // turns military/economic/other-city orders into religious cancellations.
    siege_orders[6].target = cultural_bytes;
    assert(cancel_city_attack(civ, target, 1) == 1 && siege_orders[6].kind == 0);
    assert(cancel_city_attack(civ, target, 2) == 0);
    civ->mIsPlayerOwned = true;
    assert(cancel_city_attack(civ, alternative, 0) == 0);
    civ->mIsPlayerOwned = false;
    target->mCapturePercent = 0;
    put_pointer(civ, kCivVehiclesBegin, vehicles); put_pointer(civ, kCivVehiclesEnd, vehicles);
    civ->mMilitaryExpansionCity = nullptr;
    assert(ai_refs == 0 && planner_calls > 0);
    settings = defaults;
    assert(!ai_log_allowed());
    settings.ai_diagnostics = 1; ai_log_lines = 4094;
    assert(ai_log_allowed() && !ai_log_allowed() && !ai_log_allowed() && ai_log_lines == 4096);
    settings = defaults; ai_log_lines = 0;
    ai_time = 100; ai_lifecycle.Update(1000, 0); assert(ai_time == 100);
    ai_lifecycle.Update(1000, 500); assert(ai_time == 600);
    ai_lifecycle.Read(nullptr); assert(ai_time == 0 && ai_records[0].key == 0);
    record_for(civ);
    ai_lifecycle.OnModeExited(kCivilizationMode, 0);
    for (const auto& record : ai_records) assert(record.key == 0);
    // Exercise the actual linked SDK registration wrapper, not just the ModAPI
    // save registry. The subsystem list must contain the live callback object.
    alignas(8) unsigned char system_bytes[sizeof(Simulator::cSimulatorSystem)] = {};
    auto* system = reinterpret_cast<Simulator::cSimulatorSystem*>(system_bytes);
    assert(system->AddStrategy(&ai_lifecycle, 0xbec1a110));
    assert(system->mSubSystems.size() == 1 && system->mSubSystems[0].get() == &ai_lifecycle);
    auto* live = system->mSubSystems[0].get();
    live->OnModeExited(0, kCivilizationMode);
    live->OnModeEntered(0, kCivilizationMode);
    assert(live->func24h(kCivilizationMode));
    live->Update(1000, 250);
    assert(ai_time == 250);
    alignas(8) unsigned char header_bytes[sizeof(Simulator::cSavedGameHeader)] = {};
    live->OnLoad(*reinterpret_cast<const Simulator::cSavedGameHeader*>(header_bytes));
    assert(ai_time == 0);
    using Strategies = decltype(system->mSubSystems);
    system->mSubSystems.~Strategies();
    ai_observer_ready = false;
}
