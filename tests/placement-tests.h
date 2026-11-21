static void verify_placement_policy()
{
    namespace P=Placement;
    Recovery::Sample s;
    s.key=1; s.owner=10; s.order=2; s.target=20;
    s.position={0,0,500}; s.goal={25,0,499}; s.goal_stop=1; s.acceptable_stop=2;
    s.eligible=s.crowded=true; s.desired=20; s.since_fire=12000;
    auto flat=[](const Math::Vector3& raw,Math::Vector3& out) { out=raw; return P::Normalize(out) ? (out=out*500,true) : false; };
    P::Space space;
    space.Add(s.position,{3,0,500},2,true);
    space.Add(s.position,{0,3,500},2,true);
    Math::Vector3 goal;
    P::SearchReport report;
    assert(P::FindSpace(s,2,space,flat,goal,report));
    assert(P::Distance(s.position,goal)>=4 && P::Distance(s.position,goal)<=28);
    for(unsigned i=0;i<space.count;++i) assert(P::ClearSegment(s.position,s.position,goal,2,space.blockers[i],true));
    for (unsigned i=0;i<space.count;++i) assert(P::Distance(goal,space.blockers[i].position)>=4.75f);
    // A reservation prevents a second unit from taking the same destination.
    const auto first=goal; space.Add(s.position,first,2,false);
    assert(P::FindSpace(s,2,space,flat,goal,report)); assert(P::Distance(first,goal)>=4.75f);
    // Starting overlapped is allowed only for vehicles, and the segment may
    // not deepen that overlap or cross an obstacle between its endpoints.
    assert(!P::ClearSegment(s.position,s.position,{10,0,500},1,{{5,0,500},0.1f,false},true));
    assert(!P::ClearSegment(s.position,s.position,{10,0,500},2,{{3,0,500},2,true},true));
    assert(P::ClearSegment(s.position,s.position,{-10,0,500},2,{{3,0,500},2,true},true));
    P::Space closed; closed.Add(s.position,s.position,50,true);
    assert(!P::FindSpace(s,2,closed,flat,goal,report));
    closed={}; closed.Add(s.position,s.position,2,false);
    assert(!P::FindSpace(s,2,closed,flat,goal,report));
    closed={}; closed.complete=false;
    assert(!P::FindSpace(s,2,closed,flat,goal,report));
    closed={};
    assert(!P::FindSpace(s,2,closed,[](const Math::Vector3&,Math::Vector3&){return false;},goal,report));
    // Terrain adapter vetoes the wrong medium along the path, not just the
    // endpoint. Here a shoreline excludes the positive-X half of the search.
    auto shore=[&](const Math::Vector3& raw,Math::Vector3& out) { return raw.x<=0 && flat(raw,out); };
    assert(P::FindSpace(s,2,closed,shore,goal,report) && goal.x<=0);
    auto cliff=[&](const Math::Vector3& raw,Math::Vector3& out) {
        if (!flat(raw,out)) return false;
        if (raw.x>1) out=out*(520.0f/500.0f);
        return true;
    };
    assert(P::FindSpace(s,2,closed,cliff,goal,report) && goal.x<=1);
    assert(!P::FindSpace(s,9,closed,flat,goal,report));
    assert(!P::FindSpace(s,NAN,closed,flat,goal,report));
    auto bad=s; bad.position={}; assert(!P::FindSpace(bad,2,closed,flat,goal,report));
    bad=s; bad.goal={INFINITY,0,0}; assert(!P::FindSpace(bad,2,closed,flat,goal,report));
    s.goal={0,0,500}; // Degenerate tangent direction still produces finite geometry.
    assert(P::FindSpace(s,2,closed,flat,goal,report)); s.goal={25,0,499};
    // Exercise the same terrain projection used by the production adapter.
    auto sea_floor=[](const Math::Vector3&){return 490.0f;};
    auto land=[](const Math::Vector3&){return 510.0f;};
    Math::Vector3 projected;
    assert(P::ProjectTerrain(s.position,true,2,500,1,sea_floor,projected) && std::fabs(P::Distance(projected,{})-501)<0.01f);
    assert(!P::ProjectTerrain(s.position,false,2,500,0,sea_floor,projected));
    assert(!P::ProjectTerrain(s.position,true,2,500,0,land,projected));
    assert(P::ProjectTerrain(s.position,false,2,500,0,land,projected) && std::fabs(P::Distance(projected,{})-510)<0.01f);
    assert(!P::ProjectTerrain(s.position,true,2,500,0,[](const Math::Vector3& p){return p.x>1?501.0f:490.0f;},projected));
    assert(!P::ProjectTerrain(s.position,false,2,500,0,[](const Math::Vector3& p){return p.y>1?520.0f:510.0f;},projected));
    assert(!P::ProjectTerrain(s.position,true,2,500,0,[](const Math::Vector3&){return NAN;},projected));
    assert(!P::ProjectTerrain(s.position,true,2,500,10,sea_floor,projected));

    P::Record record;
    for (unsigned t=0;t<8000;t+=500) {
        s.goal.x+=0.2f; s.since_fire=float(t);
        assert(!record.Observe(s,true,t));
    }
    s.since_fire=8000;
    assert(!record.Observe(s,false,8000)); // Active aim protects suppression/targeting.
    assert(record.Observe(s,true,8500));
    record.Begin(s,{-8,0,500},2,8500);
    assert(record.active && record.attempts==1 && record.reserved_until==16500);
    s.goal=record.imposed.goal; record.Finish(s,14500);
    for (unsigned t=15000;t<38500;t+=500) assert(!record.Observe(s,true,t));
    assert(record.Observe(s,true,38500)); record.Begin(s,{0,-8,500},2,38500);
    record.Finish(s,44500);
    for (unsigned t=45000;t<=80000;t+=500) assert(!record.Observe(s,true,t));
    assert(record.attempts==2);
    s.goal={30,0,499}; assert(!record.Observe(s,true,80500) && record.attempts==2);
    s.target=21; assert(!record.Observe(s,true,81000) && record.attempts==0);
    record={}; s.position={0,0,500};
    for (unsigned t=0;t<=30000;t+=500) {
        s.position.x=(t/500)%2?0.2f:0; assert(!record.Observe(s,true,t));
    }
    auto capacity=std::make_unique<P::State>();
    for (unsigned i=1;i<=Recovery::kMaxRecords;++i) { auto* r=capacity->Find(i,1); assert(r); r->last_seen=1; }
    assert(!capacity->Find(90000,100)); capacity->ResetSession(); assert(capacity->Find(90000,100));
    puts("Nearby-space policy: spherical geometry, overlap escape, swept clearance, reservations, terrain vetoes, immobility, combat gates and bounded episodes passed.");
}

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
    settings=defaults; apply_option(&settings,"<VehicleMovement>NearbySpace"); assert(settings.retry_blocked_paths==2);
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
