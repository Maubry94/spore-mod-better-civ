static unsigned request_passes;
static void __fastcall mock_request_setter(Simulator::cLocomotiveObject* self,int,const Native::LocalPointRequest* request) {
    ++request_passes;
    if(request){self->field_208=request->position;self->field_214=request->stop;self->field_254=request->acceptable;}
}
static void verify_retained_request()
{
    alignas(8) unsigned char bytes[sizeof(Vehicle)]={};auto* u=reinterpret_cast<Vehicle*>(bytes);
    u->mID=22;u->mHealthPoints=100;u->mbInView=true;u->mTimeSinceLastWeaponFireMS=30000;u->mPosition={0,0,500};
    const auto old_setter=original_local_request;original_local_request=reinterpret_cast<LocalRequestSetter>(mock_request_setter);
    const auto old_order=current_vehicle_order;current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    current_mode=kCivilizationMode;game_mode=mock_mode;settings=defaults;settings.retry_blocked_paths=2;
    ai_observer_ready=recovery_native_ready=true;
    auto call=reinterpret_cast<LocalRequestSetter>(retained_local_request);
    Native::LocalPointRequest q{};q.kind=1;q.position={30,0,499};q.stop=1;q.acceptable=2;
    auto reset=[&](){
        placement_state={};request_passes=0;combat_fixture_order={};combat_fixture_order.kind=2;
        Recovery::Sample s;s.key=object_key(u);s.owner=33;s.order=2;s.position=u->mPosition;s.goal=q.position;s.goal_stop=1;s.acceptable_stop=2;
        ai_time=10000;auto* r=placement_state.Find(s.key,ai_time);r->Begin(s,{0,-8,500},2,ai_time);r->last_seen=ai_time;r->hold_native=true;
        u->field_208=r->imposed.goal;return r;
    };
    auto* r=reset();
    // Reproduce the observed 0.5s overwrite, and repeated native refreshes
    // between observer ticks. The validated escape remains in effect.
    for(unsigned t=10000;t<16000;t+=50){ai_time=t;if(t%500==0)r->last_seen=t;call(u,&q);assert(request_passes==0 && u->field_208.y==-8);}
    ai_time=16000;call(u,&q);assert(request_passes==1 && u->field_208.x==30 && !r->active);
    r=reset();q.stop=3;call(u,&q);assert(!request_passes && r->original.goal_stop==3);q.stop=1;
    r=reset();combat_fixture_order.kind=1;call(u,&q);assert(request_passes==1 && !r->active);
    r=reset();combat_fixture_order.target=reinterpret_cast<void*>(0x1234);call(u,&q);assert(request_passes==1);
    r=reset();q.position.x=100;call(u,&q);assert(request_passes==1 && !r->active);q.position.x=30;
    r=reset();q.kind=3;call(u,&q);assert(request_passes==1);q.kind=1;
    r=reset();q.waypoints_end=4;call(u,&q);assert(request_passes==1);q.waypoints_end=0;
    r=reset();q.relative=true;call(u,&q);assert(request_passes==1);q.relative=false;
    r=reset();u->mTimeSinceLastWeaponFireMS=0;call(u,&q);assert(request_passes==1);u->mTimeSinceLastWeaponFireMS=30000;
    r=reset();put_pointer(u,offsetof(Vehicle,mpTarget),u);call(u,&q);assert(request_passes==1);put_pointer(u,offsetof(Vehicle,mpTarget),nullptr);
    r=reset();r->separating=true;u->mTimeSinceLastWeaponFireMS=0;
    call(u,&q);assert(!request_passes && r->active); // Separation also applies while firing.
    q.position.x=99;call(u,&q);assert(request_passes==1 && !r->active);q.position.x=30;u->mTimeSinceLastWeaponFireMS=30000;
    r=reset();r->hold_native=false;call(u,&q);assert(request_passes==1); // Player / idle yielding never held.
    r=reset();ai_time+=2000;call(u,&q);assert(request_passes==1); // Lost observer continuity.
    r=reset();u->mPosition=r->imposed.goal;call(u,&q);assert(request_passes==1);u->mPosition={0,0,500};
    r=reset();settings.retry_blocked_paths=0;call(u,&q);assert(request_passes==1);settings.retry_blocked_paths=2;
    r=reset();ai_lifecycle.Read(nullptr);call(u,&q);assert(request_passes==1);
    original_local_request=old_setter;current_vehicle_order=old_order;placement_state={};settings=defaults;
    ai_observer_ready=recovery_native_ready=false;
    puts("Request retention: native thiscall ABI, repeated refresh, six-second limit, latest destination, new orders, combat, routes, player exclusion and load reset passed.");
}
static bool fleet_target_builds;
static City* fleet_enemy_home;
static bool __fastcall fleet_builder_test(City* city,int,int mode) {
    assert(mode==kVehicleWater);return city==fleet_enemy_home?fleet_target_builds:fleet_mock_port;
}
static void verify_religious_surplus()
{
    alignas(8) unsigned char cb[sizeof(Civilization)]={},eb[sizeof(Civilization)]={},wb[sizeof(Simulator::cPlanetModel)]={};
    alignas(8) unsigned char cities_bytes[sizeof(City)*4]={};
    auto* civ=reinterpret_cast<Civilization*>(cb);auto* enemy=reinterpret_cast<Civilization*>(eb);
    auto* world=reinterpret_cast<Simulator::cPlanetModel*>(wb);fleet_fixture_world=world;
    City* cities[4];for(unsigned i=0;i<4;++i){cities[i]=reinterpret_cast<City*>(cities_bytes+i*sizeof(City));cities[i]->mID=100+i;cities[i]->mPosition={i<3?1.0f:2.0f,0,500};cities[i]->field_32C={0,0,500};put_pointer(cities[i],offsetof(City,mpCivilization),i<3?civ:enemy);}
    cities[0]->mVehicleSpecialty=cities[1]->mVehicleSpecialty=kVehicleCultural;cities[2]->mVehicleSpecialty=kVehicleMilitary;
    fleet_enemy_home=cities[3];fleet_target_builds=false;fleet_mock_port=true;
    auto storage=std::make_unique<unsigned char[]>(sizeof(Vehicle)*35);Vehicle* units[35];
    uintptr_t table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ,0,table);put_pointer(enemy,0,table);civ->mID=1;enemy->mID=2;
    for(unsigned i=0;i<35;++i){auto* u=reinterpret_cast<Vehicle*>(storage.get()+i*sizeof(Vehicle));units[i]=u;put_pointer(u,0,table);u->mID=10+i;
        u->mHealthPoints=100;u->mTimeSinceLastWeaponFireMS=60000;u->mPosition={1,0,500};
        u->mLocomotion=i<26?kVehicleLand:kVehicleWater;u->mPurpose=(i<25 || (i>=26 && i<30))?kVehicleCultural:kVehicleMilitary;}
    put_pointer(civ,kCivCitiesBegin,cities);put_pointer(civ,kCivCitiesEnd,cities+3);
    put_pointer(enemy,kCivCitiesBegin,cities+3);put_pointer(enemy,kCivCitiesEnd,cities+4);
    put_pointer(civ,kCivVehiclesBegin,units);put_pointer(civ,kCivVehiclesEnd,units+35);
    put_pointer(civ,offsetof(Civilization,mReligiousExpansionCity),cities[3]);
    bool land[]={false,true,true};uint32_t map=0;world->mContinentMap=&map;
    put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater),land);put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater)+4,land+3);
    const auto old_order=current_vehicle_order;const auto old_water=fleet_water;const auto old_continent=fleet_continent;const auto old_sell=fleet_sell;
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);combat_fixture_order={};
    fleet_water=reinterpret_cast<FleetWater>(fleet_builder_test);fleet_continent=reinterpret_cast<FleetContinent>(fleet_test_continent);
    fleet_planet=fleet_test_planet;fleet_are_enemies=fleet_test_enemy;fleet_sell=reinterpret_cast<FleetSell>(fleet_mock_sell);
    portrait_fixture_civs.push_back(civ);portrait_fixture_civs.push_back(enemy);portrait_civilizations=mock_portrait_civilizations;
    settings=defaults;settings.rebalance_land=1;fleet_state={};fleet_native_ready=ai_observer_ready=true;fleet_mock_sales=0;
    City* target=nullptr;FleetAudit audit;
    assert(fleet_reason(*civ,*units[0],target,&audit)==Fleet::Reason::LandSurplus && target==cities[3]);
    assert(audit.facts.local_ground==26 && audit.facts.local_cities==3 && audit.facts.sea_units==4);
    // Defender cannot build ships, but its port can still be attacked. Native
    // planner may have no target while reallocating: opportunity stays valid.
    for(unsigned t=0;t<=178000;t+=2000){ai_time=t;manage_unused_fleets(2000,true);}
    assert(!fleet_mock_sales);put_pointer(civ,offsetof(Civilization,mReligiousExpansionCity),nullptr);
    ai_time=180000;manage_unused_fleets(2000,true);assert(fleet_mock_sales==1);
    ai_time=182000;manage_unused_fleets(2000,true);assert(fleet_mock_sales==1);
    // Actual geography, production specialty, pressure and local enemies veto.
    assert(fleet_reason(*civ,*units[1],target,&audit)==Fleet::Reason::LandSurplus);
    cities[3]->field_32C={};assert(fleet_reason(*civ,*units[1],target,&audit)==Fleet::Reason::None && audit.veto==Fleet::Veto::NoEnemyPort);cities[3]->field_32C={0,0,500};
    fleet_mock_port=false;assert(fleet_reason(*civ,*units[1],target,&audit)==Fleet::Reason::None && audit.veto==Fleet::Veto::NoBuilderPort);fleet_mock_port=true;
    cities[3]->mPosition.x=1;assert(fleet_reason(*civ,*units[1],target,&audit)==Fleet::Reason::None && audit.veto==Fleet::Veto::HostileLand);cities[3]->mPosition.x=2;
    Fleet::Record r;assert(!r.Observe(1,100,Fleet::Reason::LandSurplus,0,1));
    for(unsigned t=2000;t<=180000;t+=2000)assert(r.Observe(1,t%4000?100:101,Fleet::Reason::LandSurplus,t,1)==(t==180000));
    assert(!r.Observe(1,101,Fleet::Reason::LandSurplus,182000,2));
    fleet_state={};fleet_native_ready=ai_observer_ready=false;settings=defaults;
    fleet_planet=fleet_sdk_planet;fleet_are_enemies=fleet_hostile;fleet_water=old_water;fleet_continent=old_continent;fleet_sell=old_sell;
    current_vehicle_order=old_order;portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;assert(ai_refs==0);
    puts("Religious surplus: observed 35-unit composition, defender unlock independence, alternate opportunity, three-minute sale, cooldown, port/enemy veto and region reset passed.");
}
