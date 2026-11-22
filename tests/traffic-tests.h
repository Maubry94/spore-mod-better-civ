static Vehicle* traffic_idle_unit;
static Native::VehicleOrder traffic_idle_order;
static const Native::VehicleOrder* __fastcall traffic_order(Vehicle* unit,int) {
    return unit==traffic_idle_unit?&traffic_idle_order:&combat_fixture_order;
}
static void verify_traffic_adapter()
{
    const auto old_phase=trade_detour_check;trade_detour_check=placement_trade_phase;
    alignas(8) unsigned char cb[sizeof(Civilization)]={},ub[sizeof(Vehicle)]={},nb[sizeof(Vehicle)]={};
    auto* civ=reinterpret_cast<Civilization*>(cb);auto* u=reinterpret_cast<Vehicle*>(ub);auto* n=reinterpret_cast<Vehicle*>(nb);
    uintptr_t table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    uintptr_t moves[64]={};moves[0xe0/4]=reinterpret_cast<uintptr_t>(placement_mock_move);
    put_pointer(civ,0,table);put_pointer(u,0x34,moves);put_pointer(n,0x34,moves);
    civ->mID=10;civ->mIsPlayerOwned=true;u->mID=20;n->mID=21;Vehicle* units[]={u,n};
    put_pointer(civ,kCivVehiclesBegin,units);put_pointer(civ,kCivVehiclesEnd,units+2);
    for(auto* v:units){v->mHealthPoints=100;v->mbInView=true;v->mFootprintRadius=2;v->field_250=1;
        v->field_214=1;v->field_254=2;v->mTimeSinceLastWeaponFireMS=60000;v->mLocomotion=kVehicleWater;}
    u->mPurpose=kVehicleEconomic;
    const auto old_order=current_vehicle_order;current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(traffic_order);
    traffic_idle_unit=n;portrait_fixture_civs.push_back(civ);portrait_civilizations=mock_portrait_civilizations;
    recovery_check_methods=recovery_mock_methods;recovery_native_ready=ai_observer_ready=true;
    placement_plan=placement_mock_plan;placement_mock_has_space=true;settings=defaults;settings.retry_blocked_paths=2;
    auto reset=[&](){placement_state={};placement_mock_moves=0;u->mPosition={0,0,500};u->field_208={30,0,500};
        n->mPosition={12,0,500};n->field_208=n->mPosition;u->mDesiredSpeed=20;n->mDesiredSpeed=0;
        combat_fixture_order={};combat_fixture_order.kind=5;traffic_idle_order={};};
    auto run=[&](unsigned start,unsigned end){for(unsigned t=start;t<=end;t+=500){ai_time=t;recover_blocked_movement(500,true);}};
    reset();run(0,8000);assert(placement_mock_moves==1 && n->field_208.y==-8 && u->field_208.x==30);
    n->mPosition=n->field_208;run(8500,9000);assert(placement_mock_moves==1); // Idle parker stays in its new free spot.
    reset();traffic_idle_order.kind=1;run(0,8000);
    assert(placement_mock_moves==1 && n->field_208.y==0 && u->field_208.y==-8); // Explicit command protected.
    u->mPosition=u->field_208;run(8500,8500);
    assert(placement_mock_moves==2 && u->field_208.y==-16); // Second leg instead of immediate blocked return.
    u->mPosition=u->field_208;run(9000,9000);assert(placement_mock_moves==3 && u->field_208.x==30 && u->field_208.y==0);
    reset();u->mLocomotion=n->mLocomotion=kVehicleAir;run(0,8000);
    assert(placement_mock_moves==1 && u->field_208.y==-8 && n->field_208.y==0);
    combat_fixture_order.kind=1;u->field_208={100,0,500};run(8500,14000);
    assert(placement_mock_moves==1 && u->field_208.x==100);
    reset();u->mDesiredSpeed=0;run(0,30000);assert(!placement_mock_moves); // Do not disturb a stationary delivery animation.
    reset();combat_fixture_order.kind=11;run(0,30000);assert(!placement_mock_moves);
    portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;current_vehicle_order=old_order;
    recovery_check_methods=recovery_methods_match;placement_plan=placement_find_space;trade_detour_check=old_phase;
    placement_state={};recovery_native_ready=ai_observer_ready=false;settings=defaults;assert(ai_refs==0);
    puts("Traffic adapter: idle-player yielding, explicit command protection, bounded two-leg detour, air traders and delivery-state protection passed.");
}
