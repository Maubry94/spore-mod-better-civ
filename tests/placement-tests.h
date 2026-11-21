static unsigned placement_mock_moves,placement_mock_plans;
static bool placement_mock_has_space;
static bool placement_trade_phase(Vehicle&){return true;}
static void __fastcall placement_mock_move(Simulator::cLocomotiveObject* self,int,
    const Math::Vector3& goal,float stop,float acceptable,bool direct)
{
    assert(stop==1 && acceptable==2 && !direct);
    ++placement_mock_moves; self->field_208=goal; self->field_214=stop; self->field_254=acceptable;
}
static bool placement_mock_plan(Vehicle&,const Recovery::Sample& s,Math::Vector3& goal,Placement::SearchReport&) {
    ++placement_mock_plans; goal=s.position+Math::Vector3{0,-8,0}; return placement_mock_has_space;
}
static void verify_placement_adapter()
{
    const auto old_phase=trade_detour_check;trade_detour_check=placement_trade_phase;
    alignas(8) unsigned char cb[sizeof(Civilization)]={},ub[sizeof(Vehicle)]={},nb[sizeof(Vehicle)]={};
    auto* civ=reinterpret_cast<Civilization*>(cb); auto* u=reinterpret_cast<Vehicle*>(ub); auto* n=reinterpret_cast<Vehicle*>(nb);
    uintptr_t civ_table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    uintptr_t move_table[64]={}; move_table[0xe0/4]=reinterpret_cast<uintptr_t>(placement_mock_move);
    put_pointer(civ,0,civ_table); put_pointer(u,0x34,move_table);
    civ->mID=10; u->mID=30; n->mID=31;
    Vehicle* units[]={u,n}; put_pointer(civ,kCivVehiclesBegin,units); put_pointer(civ,kCivVehiclesEnd,units+2);
    u->mHealthPoints=n->mHealthPoints=100; u->mFootprintRadius=n->mFootprintRadius=2;
    u->mbInView=true; u->field_250=1; u->field_214=1; u->field_254=2;
    u->mDesiredSpeed=20; u->mTimeSinceLastWeaponFireMS=15000;
    u->mPosition={0,0,500}; n->mPosition={1,0,500};
    const auto saved_order=current_vehicle_order;
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    portrait_fixture_civs.push_back(civ); portrait_civilizations=mock_portrait_civilizations;
    recovery_check_methods=recovery_mock_methods; recovery_native_ready=true;
    placement_plan=placement_mock_plan; current_mode=kCivilizationMode; game_mode=mock_mode; ai_observer_ready=true;
    settings=defaults; settings.retry_blocked_paths=2;
    auto run=[&](unsigned start,unsigned end) { for (unsigned t=start;t<=end;t+=500) { ai_time=t; recover_blocked_movement(500,true); } };
    auto reset=[&]() {
        placement_state={}; placement_mock_moves=placement_mock_plans=0; placement_mock_has_space=true;
        combat_fixture_order={}; combat_fixture_order.kind=2; u->field_208={30,0,499};
        u->mPosition={0,0,500}; u->mTimeSinceLastWeaponFireMS=15000; u->mDesiredSpeed=20;
    };
    for (int water=0;water<=1;++water) {
        u->mLocomotion=n->mLocomotion=static_cast<VehicleLocomotion>(water); civ->mIsPlayerOwned=water==0;
        reset();
        auto original_order=combat_fixture_order;
        run(0,8000); assert(placement_mock_moves==1 && u->field_208.y==-8 && placement_state.lines==0);
        assert(placement_state.Find(object_key(u),8000)->hold_native==!civ->mIsPlayerOwned);
        assert(!memcmp(&combat_fixture_order,&original_order,sizeof original_order));
        assert(u->mOrders.empty() && u->mIdlePosition.x==0 && u->mIdlePosition.y==0 && !u->mpTarget);
        run(8500,14000); assert(placement_mock_moves==2 && u->field_208.x==30); // Timed step resumes unchanged attack.
        run(14500,38000); assert(placement_mock_moves==3);
        run(38500,80000); assert(placement_mock_moves==4); // Two steps, two safe resumes; then capped.
    }
    reset(); run(0,8000); combat_fixture_order.kind=1; combat_fixture_order.position={100,0,500};
    u->field_208={100,0,500}; run(8500,14000);
    assert(placement_mock_moves==1 && u->field_208.x==100); // Manual order wins.
    reset(); run(0,8000); u->field_208={40,0,499}; run(8500,14000);
    assert(placement_mock_moves==1 && u->field_208.x==40); // Native replacement also wins.
    reset(); run(0,8000); const uintptr_t unknown=1,zero=0;
    memcpy(&u->mpTarget,&unknown,4); run(8500,14000); assert(placement_mock_moves==1);
    memcpy(&u->mpTarget,&zero,4);
    reset(); run(0,8000); u->mTimeSinceLastWeaponFireMS=0; run(8500,14000); assert(placement_mock_moves==1);
    reset(); combat_fixture_order.kind=0; run(0,8000); u->mPosition=u->field_208; u->mDesiredSpeed=0;
    run(8500,14000); assert(placement_mock_moves==1); // Idle unit keeps its free parking point.
    reset(); placement_mock_has_space=false; run(0,68000);
    assert(placement_mock_moves==0 && placement_mock_plans==4); // No spam/no fallback to invalid point.
    reset(); memcpy(&u->mpTarget,&unknown,4); run(0,20000); assert(!placement_mock_moves);
    memcpy(&u->mpTarget,&zero,4);
    reset(); u->mPurpose=kVehicleEconomic; combat_fixture_order.kind=9;
    const auto retained_trade_order=combat_fixture_order; u->mCargoUnits=42;
    run(0,8000); assert(placement_mock_moves==1);
    assert(!memcmp(&retained_trade_order,&combat_fixture_order,sizeof retained_trade_order) && u->mCargoUnits==42);
    combat_fixture_order.position.x+=100; // Trade payload is not a manual move point.
    run(8500,14000); assert(placement_mock_moves==2 && u->field_208.x==30);
    reset(); combat_fixture_order.kind=5; run(0,8000); assert(placement_mock_moves==1);
    reset(); combat_fixture_order.kind=11; run(0,20000); assert(!placement_mock_moves);
    reset();u->mPurpose=kVehicleMilitary;combat_fixture_order.kind=7;run(0,8000);assert(placement_mock_moves==1);
    assert(placement_state.Find(object_key(u),8000)->hold_native);
    u->mPurpose=kVehicleEconomic;
    // The blocker may belong to a different nation, including another trader.
    alignas(8) unsigned char other_cb[sizeof(Civilization)]={};
    auto* other_civ=reinterpret_cast<Civilization*>(other_cb);
    put_pointer(other_civ,0,civ_table); other_civ->mID=11;
    put_pointer(civ,kCivVehiclesEnd,units+1);
    put_pointer(other_civ,kCivVehiclesBegin,units+1); put_pointer(other_civ,kCivVehiclesEnd,units+2);
    portrait_fixture_civs.push_back(other_civ);
    reset(); combat_fixture_order.kind=9; run(0,8000); assert(placement_mock_moves==1);
    // A trader's new destination wins over the temporary clearance step.
    combat_fixture_order.kind=1; combat_fixture_order.position={100,0,500}; u->field_208={100,0,500};
    run(8500,14000); assert(placement_mock_moves==1 && u->field_208.x==100);
    portrait_fixture_civs.pop_back(); put_pointer(civ,kCivVehiclesEnd,units+2);
    u->mPurpose=kVehicleMilitary;
    reset(); u->mLocomotion=kVehicleAir; run(0,20000); assert(!placement_mock_moves); u->mLocomotion=kVehicleWater;
    reset(); u->field_270=4; run(0,20000); assert(!placement_mock_moves); u->field_270=0;
    reset(); u->mbInView=false; run(0,20000); assert(!placement_mock_moves); u->mbInView=true;
    // End-to-end loop detection does not require a stationary neighbor.
    reset(); n->mPosition={100,100,500}; u->mVelocity={20,0,0}; u->field_208={100,0,500};
    for (unsigned t=0;t<=20000;t+=500) {
        const float angle=float(t)*0.001f;
        u->mPosition={6*std::cos(angle),6*std::sin(angle),500}; run(t,t);
    }
    assert(placement_mock_moves==1 && combat_fixture_order.kind==2);
    u->mVelocity={}; n->mPosition={1,0,500};
    // The reported player/AI stack: firing or moving units at identical
    // positions must separate, while an anchor, normal spacing and new orders stay untouched.
    n->mID=29;n->mPosition=u->mPosition={0,0,500};
    for(bool player:{false,true}) {
        civ->mIsPlayerOwned=player;
        reset();n->mPosition=u->mPosition;u->mTimeSinceLastWeaponFireMS=0;
        put_pointer(u,offsetof(Vehicle,mpTarget),n);
        run(0,1500);assert(placement_mock_moves==1);
        auto* record=placement_state.Find(object_key(u),1500);
        assert(record->separating && record->hold_native && combat_fixture_order.kind==2 && reinterpret_cast<void*>(u->mpTarget.get())==n);
        combat_fixture_order.kind=1;combat_fixture_order.position={99,0,500};u->field_208={99,0,500};
        run(2000,2000);assert(!record->active && placement_mock_moves==1);
        put_pointer(u,offsetof(Vehicle,mpTarget),nullptr);
        reset();n->mPosition=u->mPosition;u->mVelocity={5,0,0};n->mVelocity=u->mVelocity;
        run(0,1500);assert(placement_mock_moves==1);u->mVelocity=n->mVelocity={};
        reset();n->mPosition={5,0,500};u->mTimeSinceLastWeaponFireMS=0;
        run(0,2000);assert(!placement_mock_moves);
    }
    n->mID=31;n->mPosition={1,0,500};
    // A queued local waypoint is present in the live mixed siege. Native
    // combat must retain precedence as soon as a defender or a preacher acts.
    alignas(8) unsigned char city_bytes[sizeof(City)]={}, culture_bytes[sizeof(Simulator::cCulturalTarget)]={},
        weapon_bytes[sizeof(Simulator::cSpaceToolData)]={}, building_bytes[sizeof(Simulator::cBuilding)]={};
    auto* siegeCity=reinterpret_cast<City*>(city_bytes);
    auto* culture=reinterpret_cast<Simulator::cCulturalTarget*>(culture_bytes);
    auto* weapon=reinterpret_cast<Simulator::cSpaceToolData*>(weapon_bytes);
    auto* building=reinterpret_cast<Simulator::cBuilding*>(building_bytes);
    City* siegeCities[]={siegeCity}; Simulator::cCulturalTarget* cultures[]={culture};
    Simulator::cBuilding* buildings[]={building};
    put_pointer(other_civ,kCivVehiclesBegin,nullptr);put_pointer(other_civ,kCivVehiclesEnd,nullptr);
    put_pointer(other_civ,kCivCitiesBegin,siegeCities);put_pointer(other_civ,kCivCitiesEnd,siegeCities+1);
    put_pointer(siegeCity,offsetof(City,mBuildings),buildings);put_pointer(siegeCity,offsetof(City,mBuildings)+4,buildings+1);
    put_pointer(siegeCity,offsetof(City,mCulturalTargets),cultures);put_pointer(siegeCity,offsetof(City,mCulturalTargets)+4,cultures+1);
    siegeCity->mPosition=culture->mPosition=building->mPosition={90,0,500};
    building->mHealthPoints=100;weapon->mRangeMultiplier=1;weapon->mMaxRange=100;
    put_pointer(u,offsetof(Vehicle,mpWeapon),weapon);
    int waypoint=0;
    put_pointer(u,offsetof(Vehicle,field_1F4),&waypoint);put_pointer(u,offsetof(Vehicle,field_1F4)+4,&waypoint+1);
    portrait_fixture_civs.push_back(other_civ);
    reset();combat_fixture_order.target=siegeCity;run(0,7500);assert(!placement_mock_moves);
    run(8000,8000);assert(placement_mock_moves==1 && combat_fixture_order.target==siegeCity && !u->field_1F4.empty());
    reset();combat_fixture_order.target=siegeCity;
    put_pointer(u,offsetof(Vehicle,mpTarget),static_cast<Simulator::cCombatant*>(building));
    building->mPosition={105,0,500};run(0,8000);assert(placement_mock_moves==1);
    reset();combat_fixture_order.target=siegeCity;
    put_pointer(u,offsetof(Vehicle,mpTarget),static_cast<Simulator::cCombatant*>(building));
    building->mPosition={90,0,500};run(0,12000);assert(!placement_mock_moves);
    put_pointer(u,offsetof(Vehicle,mpTarget),nullptr);
    reset();combat_fixture_order.target=siegeCity;put_pointer(u,offsetof(Vehicle,mpTarget),static_cast<Simulator::cCombatant*>(n));
    run(0,12000);assert(!placement_mock_moves);
    u->mPurpose=kVehicleCultural;
    reset();combat_fixture_order.kind=3;combat_fixture_order.target=culture;
    put_pointer(u,offsetof(Vehicle,mpTarget),static_cast<Simulator::cCombatant*>(culture));
    u->mTimeSinceLastWeaponFireMS=0;run(0,8000);assert(placement_mock_moves==1);
    put_pointer(culture,offsetof(Simulator::cCulturalTarget,mAttackers),units);
    put_pointer(culture,offsetof(Simulator::cCulturalTarget,mAttackers)+4,units+1);
    reset();combat_fixture_order.kind=3;combat_fixture_order.target=culture;
    run(0,12000);assert(!placement_mock_moves);
    put_pointer(u,offsetof(Vehicle,mpTarget),nullptr);put_pointer(u,offsetof(Vehicle,mpWeapon),nullptr);
    put_pointer(u,offsetof(Vehicle,field_1F4),nullptr);put_pointer(u,offsetof(Vehicle,field_1F4)+4,nullptr);
    portrait_fixture_civs.pop_back();u->mPurpose=kVehicleMilitary;
    puts("Naval siege: queued warship, defender priority, unregistered preacher and registered conversion tested.");
    reset(); ai_time=9000; recover_blocked_movement(0,true); recover_blocked_movement(500,false);
    assert(!placement_mock_moves && !placement_state.records[0].key);
    run(10000,18000); assert(placement_mock_moves==1);
    ai_lifecycle.Read(nullptr); assert(!placement_state.records[0].key && placement_state.next_step==0);
    reset(); settings.retry_blocked_paths=0; run(0,20000); assert(!placement_mock_moves && !placement_mock_plans);
    portrait_fixture_civs.clear(); portrait_civilizations=sdk_portrait_civilizations; current_vehicle_order=saved_order;
    recovery_check_methods=recovery_methods_match; placement_plan=placement_find_space;trade_detour_check=old_phase;
    placement_state={}; recovery_native_ready=false; ai_observer_ready=false; settings=defaults;
    assert(ai_refs==0);
    puts("Nearby-space adapter: player/AI land/sea, native movement, retained orders, manual/native/combat precedence, idle arrival, bounded retries, no-space, pause and load reset passed.");
    puts("Trade/loop adapters: foreign-nation blockers, unchanged trade order/cargo, trader manual override and quiet-loop displacement passed.");
}

static void verify_placement_loops()
{
    Recovery::Sample s;
    s.key=1; s.owner=1; s.order=2; s.target=10; s.goal={100,0,500};
    s.desired=s.speed=20; s.since_fire=12000; s.eligible=true;
    s.goal_stop=1; s.acceptable_stop=2;
    auto circle=[&](unsigned t) {
        const float angle=float(t)*0.001f; s.position={6*std::cos(angle),6*std::sin(angle),500};
    };
    Placement::Record r;
    for (unsigned t=0;t<=20000;t+=500) {
        circle(t); const bool due=r.Observe(s,true,t);
        assert(due==(t==20000));
    }
    assert(r.loop_detected && r.path>80 && r.turns>6.28f);
    r.Begin(s,{-10,0,500},2,20000); r.Finish(s,26000);
    assert(r.attempts==1 && r.cooldown==50000);
    r={};
    for (unsigned t=0;t<=40000;t+=500) { circle(t); assert(!r.Observe(s,false,t)); }
    r={}; s.since_fire=100;
    for (unsigned t=0;t<=40000;t+=500) { circle(t); assert(!r.Observe(s,true,t)); }
    r={}; s.since_fire=12000;
    for (unsigned t=0;t<=40000;t+=500) { s.position={float(t)*0.001f,0,500}; assert(!r.Observe(s,true,t)); }
    // Moving native destinations, manual commands and long sampling gaps reset the evidence window.
    r={};
    for (unsigned t=0;t<=40000;t+=500) { circle(t); s.goal.x+=5; assert(!r.Observe(s,true,t)); }
    r={}; s.goal={100,0,500};
    for (unsigned t=0;t<=40000;t+=500) { circle(t); if (t%5000==0) ++s.target; assert(!r.Observe(s,true,t)); }
    r={};
    for (unsigned t=0;t<=40000;t+=2000) { circle(t); assert(!r.Observe(s,true,t)); }
    puts("Quiet loops: sustained circling detected; firing, active aim, real progress, changing goals/orders and gaps protected.");
}

static bool placement_mock_ready=true, placement_mock_terrain_match=true;
static Terrain::cTerrainMapSet* placement_mock_map;
static bool __fastcall placement_mock_is_ready(void*,int) { return placement_mock_ready; }
static Terrain::cTerrainMapSet* __fastcall placement_mock_get_map(void*,int) { return placement_mock_map; }
static bool placement_mock_match(const Terrain::ITerrain&) { return placement_mock_terrain_match; }
static void verify_placement_terrain()
{
    alignas(4) unsigned char pb[sizeof(Simulator::cPlanetModel)]={},tb[0x100]={};
    auto* planet=reinterpret_cast<Simulator::cPlanetModel*>(pb);
    auto* terrain=reinterpret_cast<Terrain::ITerrain*>(tb);
    uintptr_t table[16]={}; put_pointer(terrain,0,table);
    table[0xc/4]=0x967d90; table[0x30/4]=0xfa0d40;
    assert(Recovery::MatchingTerrain(*terrain,0x400000));
    table[0x30/4]^=1; assert(!Recovery::MatchingTerrain(*terrain,0x400000));
    table[0x30/4]=reinterpret_cast<uintptr_t>(placement_mock_is_ready);
    table[0xc/4]=reinterpret_cast<uintptr_t>(placement_mock_get_map);
    auto saved=placement_check_terrain; placement_check_terrain=placement_mock_match;
    Placement::SearchReport report;
    assert(!placement_read_terrain(nullptr,report) && !strcmp(report.reason,"no-planet"));
    assert(!placement_read_terrain(planet,report) && !strcmp(report.reason,"no-sphere"));
    const uintptr_t sentinel=1;
    memcpy(&planet->mpSphere,&sentinel,4);
    assert(!placement_read_terrain(planet,report) && !strcmp(report.reason,"no-terrain"));
    memcpy(&planet->mpTerrain,&terrain,4);
    placement_mock_terrain_match=false;
    assert(!placement_read_terrain(planet,report) && !strcmp(report.reason,"unsupported-terrain-methods"));
    placement_mock_terrain_match=true; placement_mock_ready=false;
    assert(!placement_read_terrain(planet,report) && !strcmp(report.reason,"terrain-loading"));
    placement_mock_ready=true; placement_mock_map=nullptr;
    assert(!placement_read_terrain(planet,report) && !strcmp(report.reason,"no-terrain-map"));
    placement_mock_map=reinterpret_cast<Terrain::cTerrainMapSet*>(16);
    assert(placement_read_terrain(planet,report)==placement_mock_map);
    placement_check_terrain=saved;
    puts("Terrain adapter: finished/loading polarity, missing resources and unsupported methods checked.");
}
