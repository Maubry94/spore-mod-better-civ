static unsigned recovery_mock_calls;
static Simulator::cLocomotiveObject* recovery_mock_this;
static Math::Vector3 recovery_mock_goal;
static void __fastcall recovery_mock_move(Simulator::cLocomotiveObject* self, int,
    const Math::Vector3& goal, float stop, float acceptable, bool direct)
{
    ++recovery_mock_calls; recovery_mock_this = self; recovery_mock_goal = goal;
    assert(stop == 1 && acceptable == 2 && !direct);
}
static bool recovery_mock_methods(const Vehicle&) { return true; }
static void verify_movement_recovery()
{
    namespace Q = Recovery;
    auto option = defaults;
    apply_option(&option,"<VehicleMovement>RetryBlockedPaths"); assert(option.retry_blocked_paths == 1);
    apply_option(&option,"<VehicleMovement>MovementVanilla"); assert(option.retry_blocked_paths == 0);
    Q::Sample s;
    s.key=1; s.owner=10; s.order=2; s.target=20; s.goal={20,0,0};
    s.desired=13.7f; s.since_fire=12000; s.goal_stop=1; s.acceptable_stop=2;
    s.eligible=s.crowded=true; assert(s.Valid());
    Q::Record r;
    for (unsigned t=0;t<8000;t+=500) assert(!r.Observe(s,t));
    assert(r.Observe(s,8000)); r.Retried(s,8000);
    for (unsigned t=8500;t<28000;t+=500) assert(!r.Observe(s,t));
    assert(r.Observe(s,28000)); r.Retried(s,28000);
    for (unsigned t=28500;t<=60000;t+=500) assert(!r.Observe(s,t));
    // Loss of view/time continuity cannot immediately earn another retry.
    assert(!r.Observe(s,70000) && r.attempts==2);
    s.position={3,0,0}; assert(!r.Observe(s,70500) && r.attempts==0);
    for (unsigned t=71000;t<78500;t+=500) assert(!r.Observe(s,t));
    assert(r.Observe(s,78500));
    s.target=21; assert(!r.Observe(s,79000));
    s.goal.x+=1; assert(!r.Observe(s,79500));
    s.order_position.x+=1; assert(!r.Observe(s,80000));
    s.speed=0.5f; assert(!s.Valid()); s.speed=0;
    s.since_fire=7999; assert(!s.Valid()); s.since_fire=12000;
    s.eligible=false; assert(!s.Valid()); s.eligible=true;
    s.crowded=false; assert(!s.Valid()); s.crowded=true;
    s.position=s.goal; assert(!s.Valid()); s.position={};
    s.goal_stop=-1; assert(!s.Valid()); s.goal_stop=1;
    s.position.x=std::numeric_limits<float>::quiet_NaN(); assert(!s.Valid()); s.position={};
    s.goal.x=std::numeric_limits<float>::infinity(); assert(!s.Valid()); s.goal={20,0,0};
    r={};
    // A unit making steady progress, or moving in a loop, is not stationary.
    for (unsigned t=0;t<=30000;t+=500) {
        s.position.x=(t/500)%2 ? 0.2f : 0; assert(!r.Observe(s,t));
    }
    Q::State capacity;
    for (unsigned i=1;i<=Q::kMaxRecords;++i) { auto* p=capacity.Find(i,1); assert(p); p->last_seen=1; }
    assert(!capacity.Find(10000,100)); capacity.ResetSession(); assert(capacity.Find(10000,100));

    alignas(8) unsigned char cb[sizeof(Civilization)]={}, ub[sizeof(Vehicle)]={}, nb[sizeof(Vehicle)]={};
    auto* civ=reinterpret_cast<Civilization*>(cb);
    auto* u=reinterpret_cast<Vehicle*>(ub); auto* n=reinterpret_cast<Vehicle*>(nb);
    uintptr_t civ_table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    uintptr_t move_table[64]={}; move_table[0xe0/4]=reinterpret_cast<uintptr_t>(recovery_mock_move);
    put_pointer(civ,0,civ_table); put_pointer(u,0x34,move_table);
    assert(!Q::MatchingMethods(*u,0x400000));
    move_table[0xe0/4]=0xca35a0; move_table[0xdc/4]=0xc9ea20; move_table[0xc4/4]=0xc9ee70;
    assert(Q::MatchingMethods(*u,0x400000)); move_table[0xdc/4]^=1;
    assert(!Q::MatchingMethods(*u,0x400000)); move_table[0xdc/4]^=1;
    move_table[0xe0/4]=reinterpret_cast<uintptr_t>(recovery_mock_move);
    civ->mID=10; civ->mIsPlayerOwned=true; u->mID=30; n->mID=31;
    Vehicle* units[]={u,n}; put_pointer(civ,kCivVehiclesBegin,units); put_pointer(civ,kCivVehiclesEnd,units+2);
    u->mHealthPoints=n->mHealthPoints=100; u->mFootprintRadius=n->mFootprintRadius=3;
    u->mbInView=true; u->field_250=1; u->field_208={20,0,0}; u->field_214=1; u->field_254=2;
    u->mDesiredSpeed=13.7f; u->mTimeSinceLastWeaponFireMS=15000; n->mPosition={1,0,0};
    combat_fixture_order={}; combat_fixture_order.kind=2;
    const auto saved_order=current_vehicle_order;
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    portrait_fixture_civs.push_back(civ); portrait_civilizations=mock_portrait_civilizations;
    recovery_check_methods=recovery_mock_methods; recovery_native_ready=true; recovery_state={};
    current_mode=kCivilizationMode; game_mode=mock_mode; ai_observer_ready=true;
    settings=defaults; settings.retry_blocked_paths=1; recovery_mock_calls=0;
    auto run=[&](unsigned start,unsigned end) { for (unsigned t=start;t<=end;t+=500) { ai_time=t; recover_blocked_movement(500,true); } };
    // Both player and AI, land and water use the same adapter. No diagnostics
    // setting is required to recover, and no event log is written while off.
    for (int water=0;water<=1;++water) {
        u->mLocomotion=n->mLocomotion=static_cast<VehicleLocomotion>(water);
        civ->mIsPlayerOwned=water==0;
        recovery_state={}; recovery_mock_calls=0;
        const auto before=std::string(reinterpret_cast<char*>(ub),sizeof ub);
        run(0,8000); assert(recovery_mock_calls==1 && recovery_state.lines==0);
        assert(recovery_mock_this==static_cast<Simulator::cLocomotiveObject*>(u) && recovery_mock_goal.x==20);
        assert(!memcmp(ub,before.data(),sizeof ub)); // Adapter only invokes native MoveTo.
        run(8500,28000); assert(recovery_mock_calls==2);
        run(28500,70000); assert(recovery_mock_calls==2);
    }
    // Unknown/active aim, economic/aircraft, path-in-progress, unsupported
    // request, no contact, offscreen/dead, disabled setting and pause all abstain.
    auto excluded=[&]() { recovery_state={}; recovery_mock_calls=0; run(0,20000); assert(!recovery_mock_calls); };
    const uintptr_t unknown=1; memcpy(&u->mpTarget,&unknown,sizeof unknown); excluded();
    const uintptr_t zero=0; memcpy(&u->mpTarget,&zero,sizeof zero);
    u->mPurpose=kVehicleEconomic; excluded(); u->mPurpose=kVehicleMilitary;
    u->mLocomotion=kVehicleAir; excluded(); u->mLocomotion=kVehicleWater;
    u->field_270=4; excluded(); u->field_270=0;
    u->field_250=2; excluded(); u->field_250=1;
    u->field_240=true; excluded(); u->field_240=false;
    u->mTimeSinceLastWeaponFireMS=7999; excluded(); u->mTimeSinceLastWeaponFireMS=15000;
    n->mPosition={100,0,0}; excluded(); n->mPosition={1,0,0};
    u->mbInView=false; excluded(); u->mbInView=true;
    u->mbDead=true; excluded(); u->mbDead=false;
    settings.retry_blocked_paths=0; excluded(); settings.retry_blocked_paths=1;
    recovery_native_ready=false; excluded(); recovery_native_ready=true;
    recovery_state={}; ai_time=9000;
    recover_blocked_movement(0,true); recover_blocked_movement(500,false); assert(!recovery_state.records[0].key);
    run(10000,18000); assert(recovery_mock_calls==1);
    ai_lifecycle.Read(nullptr); assert(!recovery_state.records[0].key && recovery_state.next_retry==0);
    portrait_fixture_civs.clear(); portrait_civilizations=sdk_portrait_civilizations;
    current_vehicle_order=saved_order; recovery_check_methods=recovery_methods_match;
    recovery_state={}; recovery_native_ready=false; settings=defaults; ai_observer_ready=false;
    assert(ai_refs==0);
    puts("Movement recovery: stationary contact, exclusions, native MoveTo adapter, unchanged orders, cooldown, retry budget, pause and lifecycle passed.");
}
