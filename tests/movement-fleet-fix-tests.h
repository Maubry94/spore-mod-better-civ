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
    return mode==kVehicleWater && (city==fleet_enemy_home?fleet_target_builds:fleet_mock_port);
}
static bool __fastcall fleet_air_only(City*,int,int mode) {return mode==kVehicleAir;}
static bool __fastcall fleet_merchant_unlock(City*,int,int mode) {return mode==kVehicleLand;}
static int __fastcall fleet_merchant_specialty_limit(Civilization*,int,int) {return 12;}
static void verify_religious_surplus()
{
    alignas(8) unsigned char cb[sizeof(Civilization)]={},eb[sizeof(Civilization)]={},wb[sizeof(Simulator::cPlanetModel)]={};
    alignas(8) unsigned char cities_bytes[sizeof(City)*4]={};
    auto* civ=reinterpret_cast<Civilization*>(cb);auto* enemy=reinterpret_cast<Civilization*>(eb);
    auto* world=reinterpret_cast<Simulator::cPlanetModel*>(wb);fleet_fixture_world=world;
    City* cities[4];for(unsigned i=0;i<4;++i){cities[i]=reinterpret_cast<City*>(cities_bytes+i*sizeof(City));cities[i]->mID=100+i;cities[i]->mPosition={i<3?1.0f:2.0f,0,500};cities[i]->field_32C={0,0,500};put_pointer(cities[i],offsetof(City,mpCivilization),i<3?civ:enemy);}
    cities[0]->mVehicleSpecialty=cities[1]->mVehicleSpecialty=kVehicleCultural;cities[2]->mVehicleSpecialty=kVehicleMilitary;
    fleet_enemy_home=cities[3];fleet_target_builds=false;fleet_mock_port=true;
    auto storage=std::make_unique<unsigned char[]>(sizeof(Vehicle)*36);Vehicle* units[36];
    uintptr_t table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ,0,table);put_pointer(enemy,0,table);civ->mID=1;enemy->mID=2;
    for(unsigned i=0;i<36;++i){auto* u=reinterpret_cast<Vehicle*>(storage.get()+i*sizeof(Vehicle));units[i]=u;put_pointer(u,0,table);u->mID=10+i;
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
    // Mathy: a hostile fleet is still offshore after the city changes hands.
    // An empty native hostile list alone must not authorize either seller.
    alignas(8) unsigned char ship_bytes[sizeof(Vehicle)]={};
    auto* ship=reinterpret_cast<Vehicle*>(ship_bytes);
    ship->mHealthPoints=100;ship->mLocomotion=kVehicleWater;ship->mPosition={30,0,500};
    Vehicle* fleet[]={ship};
    put_pointer(enemy,kCivVehiclesBegin,fleet);put_pointer(enemy,kCivVehiclesEnd,fleet+1);
    auto* ground=units[0];
    const auto old_mode=game_mode; const auto old_stage=current_mode;
    game_mode=mock_mode; current_mode=kCivilizationMode;
    for (int order : {1,2,3,7}) {
        combat_fixture_order.kind=order;
        guarded_ai_land_sale(ground,0,true);
        assert(!fleet_mock_sales && !ground->mbDead);
    }
    assert(fleet_reason(*civ,*ground,target,&audit)==Fleet::Reason::None && audit.veto==Fleet::Veto::Busy);
    ship->mPosition={400,0,500};
    ground->mTimeSinceLastWeaponFireMS=100;
    guarded_ai_land_sale(ground,0,true);
    assert(!fleet_mock_sales);
    ground->mTimeSinceLastWeaponFireMS=60000;
    combat_fixture_order={};
    guarded_ai_land_sale(ground,0,true);
    assert(fleet_mock_sales==1 && ground->mbDead);
    ground->mbDead=false;fleet_mock_sales=0;game_mode=old_mode;current_mode=old_stage;
    put_pointer(enemy,kCivVehiclesBegin,nullptr);put_pointer(enemy,kCivVehiclesEnd,nullptr);
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
    // A temporary unknown region must not turn a home garrison into a surplus.
    cities[1]->mPosition.x=-1;
    assert(fleet_reason(*civ,*units[1],target,&audit)==Fleet::Reason::None && audit.veto==Fleet::Veto::MapUnavailable);
    cities[1]->mPosition.x=1;
    cities[3]->mPosition.x=0;
    assert(fleet_reason(*civ,*units[1],target,&audit)==Fleet::Reason::None && audit.veto==Fleet::Veto::MapUnavailable);
    cities[3]->mPosition.x=2;
    fleet_water=reinterpret_cast<FleetWater>(fleet_air_only);
    assert(fleet_reason(*civ,*units[1],target)==Fleet::Reason::LandSurplus && target==cities[3]);
    fleet_water=reinterpret_cast<FleetWater>(fleet_builder_test);
    Fleet::Record r;assert(!r.Observe(1,100,Fleet::Reason::LandSurplus,0,1));
    for(unsigned t=2000;t<=180000;t+=2000)assert(r.Observe(1,t%4000?100:101,Fleet::Reason::LandSurplus,t,1)==(t==180000));
    assert(!r.Observe(1,101,Fleet::Reason::LandSurplus,182000,2));
    // The 35th place is still usable by other specialties. The idle seller
    // acts only at 36 and must leave orders and loaded merchants alone.
    const auto old_capacity=original_fleet_capacity;
    fleet_water=reinterpret_cast<FleetWater>(fleet_merchant_unlock);
    // A loaded trader on a landmass with no local partner has no delivery.
    units[0]->mPurpose=kVehicleEconomic;units[0]->mbDead=false;units[0]->mCargoUnits=245;
    units[0]->mDesiredSpeed=15;
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::StrandedMerchant);
    combat_fixture_order.kind=5;
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    combat_fixture_order={};units[0]->mDesiredSpeed=0;
    cities[3]->mPosition.x=1;
    fleet_are_enemies=[](Civilization&,Civilization&){return false;};
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    units[0]->mPosition.x=2;
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::StrandedMerchant);
    units[0]->mPosition.x=1;units[0]->mCargoUnits=0;
    fleet_are_enemies=fleet_test_enemy;
    cities[1]->mVehicleSpecialty=kVehicleEconomic;
    assert(merchant_capacity(*civ,32)==5 && merchant_capacity(*civ,4)==4);
    civ->mIsPlayerOwned=true;assert(merchant_capacity(*civ,32)==32);civ->mIsPlayerOwned=false;
    cities[1]->mVehicleSpecialty=kVehicleCultural;
    for (unsigned i=0;i<28;++i) {
        units[i]->mPurpose=kVehicleEconomic;units[i]->mbDead=false;units[i]->mLocomotion=kVehicleWater;
    }
    put_pointer(civ,offsetof(Civilization,mMilitaryExpansionCity),cities[3]);
    cities[3]->mPosition.x=1;
    original_fleet_capacity=reinterpret_cast<decltype(original_fleet_capacity)>(fleet_merchant_specialty_limit);
    // A mixed nation can free a slot for its religious planner without a military city.
    cities[2]->mVehicleSpecialty=kVehicleEconomic;
    put_pointer(civ,offsetof(Civilization,mReligiousExpansionCity),cities[3]);
    put_pointer(civ,kCivVehiclesEnd,units+36);
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::MerchantSurplus && target==cities[3]);
    put_pointer(civ,offsetof(Civilization,mReligiousExpansionCity),nullptr);
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    cities[2]->mVehicleSpecialty=kVehicleMilitary;put_pointer(civ,kCivVehiclesEnd,units+35);
    assert(merchant_capacity(*civ,32)==28 && merchant_capacity(*civ,27)==27);
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    put_pointer(civ,kCivVehiclesEnd,units+36);
    assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::MerchantSurplus);
    combat_fixture_order.kind=5;assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    combat_fixture_order={};units[0]->mCargoUnits=5;assert(fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    units[0]->mCargoUnits=0; fleet_state={};fleet_mock_sales=0;
    for (unsigned t=0;t<120000;t+=2000) {ai_time=t;manage_unused_fleets(2000,true);}
    assert(!fleet_mock_sales);
    ai_time=120000;manage_unused_fleets(2000,true);assert(fleet_mock_sales==1);
    put_pointer(civ,kCivVehiclesEnd,units+35);
    ai_time=150000;manage_unused_fleets(2000,true);assert(fleet_mock_sales==1);
    civ->mIsPlayerOwned=true;
    assert(merchant_capacity(*civ,32)==32 && fleet_reason(*civ,*units[0],target)==Fleet::Reason::None);
    civ->mIsPlayerOwned=false;cities[3]->mPosition.x=2;
    assert(merchant_capacity(*civ,32)==32);
    cities[3]->mPosition.x=1;put_pointer(civ,offsetof(Civilization,mMilitaryExpansionCity),nullptr);
    assert(merchant_capacity(*civ,32)==32);
    original_fleet_capacity=old_capacity;
    puts("Merchant reinforcement: shared slot, minority combat fleet, reachable objective, active trade, cargo and player protections passed.");
    fleet_state={};fleet_native_ready=ai_observer_ready=false;settings=defaults;
    fleet_planet=fleet_sdk_planet;fleet_are_enemies=fleet_hostile;fleet_water=old_water;fleet_continent=old_continent;fleet_sell=old_sell;
    current_vehicle_order=old_order;portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;assert(ai_refs==0);
    puts("Religious surplus: observed 35-unit composition, defender unlock independence, alternate opportunity, three-minute sale, cooldown, port/enemy veto and region reset passed.");
}
