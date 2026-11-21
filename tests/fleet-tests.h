static unsigned fleet_mock_sales;
static bool fleet_mock_port;
static bool __fastcall fleet_mock_water(City*,int,int mode) { return mode==kVehicleWater && fleet_mock_port; }
static int __fastcall fleet_mock_continent(Simulator::cPlanetModel*,int,const Math::Vector3&) {return 0;}
static void __fastcall fleet_mock_sell(Vehicle* u,int,bool refund) {
    assert(refund); ++fleet_mock_sales; u->mbDead=true;
}
static Simulator::cPlanetModel* fleet_fixture_world;
static Simulator::cPlanetModel* fleet_test_planet(){return fleet_fixture_world;}
static void verify_fleet_adapter()
{
    alignas(8) unsigned char cb[sizeof(Civilization)]={},ub[sizeof(Vehicle)]={},nb[sizeof(Vehicle)]={},ct[sizeof(City)]={};
    auto* civ=reinterpret_cast<Civilization*>(cb); auto* u=reinterpret_cast<Vehicle*>(ub);
    auto* n=reinterpret_cast<Vehicle*>(nb); auto* city=reinterpret_cast<City*>(ct);
    uintptr_t table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ,0,table); put_pointer(u,0,table); put_pointer(n,0,table);
    civ->mID=10; u->mID=20; n->mID=21; city->mID=30;
    Vehicle* units[]={u,n}; City* cities[]={city};
    put_pointer(civ,kCivVehiclesBegin,units); put_pointer(civ,kCivVehiclesEnd,units+2);
    put_pointer(civ,kCivCitiesBegin,cities); put_pointer(civ,kCivCitiesEnd,cities+1);
    put_pointer(city,offsetof(City,mpCivilization),civ);
    for(auto* v:units) {v->mHealthPoints=100;v->mLocomotion=kVehicleWater;v->mPurpose=kVehicleEconomic;v->mTimeSinceLastWeaponFireMS=60000;}
    alignas(8) unsigned char world_bytes[sizeof(Simulator::cPlanetModel)]={};
    auto* world=reinterpret_cast<Simulator::cPlanetModel*>(world_bytes);fleet_fixture_world=world;
    bool land[]={false};uint32_t map=0;world->mContinentMap=&map;
    put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater),land);
    put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater)+4,land+1);
    const auto old_planet=fleet_planet;fleet_planet=fleet_test_planet;
    const auto old_order=current_vehicle_order; current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    combat_fixture_order={}; portrait_fixture_civs.push_back(civ); portrait_civilizations=mock_portrait_civilizations;
    const auto old_sell=fleet_sell; const auto old_water=fleet_water; const auto old_continent=fleet_continent;
    fleet_sell=reinterpret_cast<FleetSell>(fleet_mock_sell); fleet_water=reinterpret_cast<FleetWater>(fleet_mock_water);
    fleet_continent=reinterpret_cast<FleetContinent>(fleet_mock_continent); fleet_native_ready=ai_observer_ready=true;
    settings=defaults; settings.retire_stranded=1;
    auto reset=[&](){fleet_state={};fleet_mock_sales=0;fleet_mock_port=false;u->mbDead=n->mbDead=false;civ->mIsPlayerOwned=false;combat_fixture_order={};};
    auto run=[&](unsigned start,unsigned end){for(unsigned t=start;t<=end;t+=2000){city->field_32C=fleet_mock_port?Math::Vector3{0,0,500}:Math::Vector3{};ai_time=t;manage_unused_fleets(2000,true);}};
    reset();run(0,118000);assert(!fleet_mock_sales);run(120000,120000);assert(fleet_mock_sales==1);
    run(122000,148000);assert(fleet_mock_sales==1);run(150000,150000);assert(fleet_mock_sales==2);
    // All ship specialties use the same grace and refundable transaction.
    // The nation's reference specialty is deliberately different from some
    // of its ships: eligibility is about owned ports, never nation type.
    for(auto purpose:{kVehicleMilitary,kVehicleCultural,kVehicleEconomic}) {
        u->mPurpose=n->mPurpose=purpose;
        reset();run(0,118000);assert(!fleet_mock_sales);run(120000,120000);assert(fleet_mock_sales==1);
        for(int kind:{1,2,3,5,7,8,9}) {
            reset();combat_fixture_order.kind=kind;run(0,200000);assert(!fleet_mock_sales);
        }
        reset();run(0,60000);fleet_mock_port=true;run(62000,200000);assert(!fleet_mock_sales);
        reset();civ->mIsPlayerOwned=true;run(0,200000);assert(!fleet_mock_sales);
        reset();u->mDesiredSpeed=n->mDesiredSpeed=15;run(0,200000);assert(fleet_mock_sales==(purpose==kVehicleEconomic ? 2u : 0u));u->mDesiredSpeed=n->mDesiredSpeed=0;
        reset();u->mTimeSinceLastWeaponFireMS=n->mTimeSinceLastWeaponFireMS=0;run(0,200000);assert(!fleet_mock_sales);
        u->mTimeSinceLastWeaponFireMS=n->mTimeSinceLastWeaponFireMS=60000;
        reset();put_pointer(u,offsetof(Vehicle,mpTarget),u);put_pointer(n,offsetof(Vehicle,mpTarget),n);
        run(0,200000);assert(!fleet_mock_sales);put_pointer(u,offsetof(Vehicle,mpTarget),nullptr);put_pointer(n,offsetof(Vehicle,mpTarget),nullptr);
        reset();put_pointer(u,offsetof(Vehicle,mHostileUnits),units);put_pointer(u,offsetof(Vehicle,mHostileUnits)+4,units+2);
        put_pointer(n,offsetof(Vehicle,mHostileUnits),units);put_pointer(n,offsetof(Vehicle,mHostileUnits)+4,units+2);
        run(0,200000);assert(!fleet_mock_sales);
        put_pointer(u,offsetof(Vehicle,mHostileUnits),nullptr);put_pointer(u,offsetof(Vehicle,mHostileUnits)+4,nullptr);
        put_pointer(n,offsetof(Vehicle,mHostileUnits),nullptr);put_pointer(n,offsetof(Vehicle,mHostileUnits)+4,nullptr);
        reset();settings.retire_stranded=0;run(0,200000);assert(!fleet_mock_sales);settings.retire_stranded=1;
        for(auto locomotion:{kVehicleLand,kVehicleAir}) {
            reset();u->mLocomotion=n->mLocomotion=locomotion;run(0,200000);assert(!fleet_mock_sales);
        }
        u->mLocomotion=n->mLocomotion=kVehicleWater;
    }
    // Mixed fleets share one per-nation sale cooldown, not one per specialty.
    reset();u->mPurpose=kVehicleMilitary;n->mPurpose=kVehicleCultural;
    run(0,120000);assert(fleet_mock_sales==1);run(122000,148000);assert(fleet_mock_sales==1);
    run(150000,150000);assert(fleet_mock_sales==2);
    u->mPurpose=n->mPurpose=kVehicleEconomic;
    reset();run(0,60000);fleet_mock_port=true;run(62000,180000);assert(!fleet_mock_sales);
    fleet_mock_port=false;run(182000,300000);assert(!fleet_mock_sales);run(302000,302000);assert(fleet_mock_sales==1);
    reset();civ->mIsPlayerOwned=true;run(0,200000);assert(!fleet_mock_sales);
    reset();combat_fixture_order.kind=5;run(0,200000);assert(!fleet_mock_sales);
    reset();u->mDesiredSpeed=n->mDesiredSpeed=15;run(0,200000);assert(fleet_mock_sales==2);u->mDesiredSpeed=n->mDesiredSpeed=0;
    reset();u->mTimeSinceLastWeaponFireMS=n->mTimeSinceLastWeaponFireMS=0;run(0,200000);assert(!fleet_mock_sales);
    u->mTimeSinceLastWeaponFireMS=n->mTimeSinceLastWeaponFireMS=60000;
    reset();settings.retire_stranded=0;run(0,200000);assert(!fleet_mock_sales);settings.retire_stranded=1;
    reset();run(0,60000);ai_lifecycle.Read(nullptr);run(62000,180000);assert(!fleet_mock_sales);
    ai_time=182000;manage_unused_fleets(0,true);manage_unused_fleets(2000,false);assert(!fleet_mock_sales);
    run(182000,182000);assert(fleet_mock_sales==1);
    portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;current_vehicle_order=old_order;
    fleet_planet=old_planet;fleet_sell=old_sell;fleet_water=old_water;fleet_continent=old_continent;fleet_native_ready=ai_observer_ready=false;
    fleet_state={};settings=defaults;assert(ai_refs==0);
    puts("Fleet adapter: native refund call, one sale per 30s, usable-port cancellation, trade/player/movement protection, pause and load reset passed.");
    puts("Stranded fleets: all three ship specialties, mixed-fleet cooldown, retained missions, regained port, player/combat protection, land/air exclusion and legacy setting IDs passed.");
}
static bool fleet_test_enemy(Civilization&,Civilization&){return true;}
static int __fastcall fleet_test_continent(Simulator::cPlanetModel*,int,const Math::Vector3& p){return int(p.x);}
static void verify_land_fleet_adapter()
{
    alignas(8) unsigned char cb[sizeof(Civilization)]={},eb[sizeof(Civilization)]={},
        own_bytes[sizeof(City)]={},target_bytes[sizeof(City)]={},world_bytes[sizeof(Simulator::cPlanetModel)]={};
    auto* civ=reinterpret_cast<Civilization*>(cb);auto* enemy=reinterpret_cast<Civilization*>(eb);
    auto* home=reinterpret_cast<City*>(own_bytes);auto* target=reinterpret_cast<City*>(target_bytes);
    auto* world=reinterpret_cast<Simulator::cPlanetModel*>(world_bytes);fleet_fixture_world=world;
    auto buffers=std::make_unique<unsigned char[]>(sizeof(Vehicle)*36);Vehicle* units[36];
    for(unsigned i=0;i<36;++i){auto* v=reinterpret_cast<Vehicle*>(buffers.get()+i*sizeof(Vehicle));units[i]=v;
        v->mHealthPoints=100;v->mLocomotion=kVehicleLand;v->mPurpose=kVehicleMilitary;v->mPosition={1,0,500};v->mTimeSinceLastWeaponFireMS=60000;}
    City* own_cities[]={home};City* enemy_cities[]={target};
    put_pointer(civ,kCivCitiesBegin,own_cities);put_pointer(civ,kCivCitiesEnd,own_cities+1);
    put_pointer(enemy,kCivCitiesBegin,enemy_cities);put_pointer(enemy,kCivCitiesEnd,enemy_cities+1);
    put_pointer(home,offsetof(City,mpCivilization),civ);put_pointer(target,offsetof(City,mpCivilization),enemy);
    put_pointer(civ,kCivVehiclesBegin,units);put_pointer(civ,kCivVehiclesEnd,units+36);
    put_pointer(civ,offsetof(Civilization,mMilitaryExpansionCity),target);
    home->mPosition={1,0,500};target->mPosition={2,0,500};
    home->field_32C=target->field_32C={0,0,500};
    bool land[]={false,true,true};uint32_t map=0;world->mContinentMap=&map;
    put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater),land);
    put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater)+4,land+3);
    uintptr_t table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ,0,table);put_pointer(enemy,0,table);
    portrait_fixture_civs.push_back(civ);portrait_fixture_civs.push_back(enemy);portrait_civilizations=mock_portrait_civilizations;
    const auto old_order=current_vehicle_order;const auto old_water=fleet_water;const auto old_continent=fleet_continent;
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);combat_fixture_order={};combat_fixture_order.kind=7;
    fleet_water=reinterpret_cast<FleetWater>(fleet_mock_water);fleet_mock_port=true;
    fleet_continent=reinterpret_cast<FleetContinent>(fleet_test_continent);fleet_planet=fleet_test_planet;fleet_are_enemies=fleet_test_enemy;
    settings=defaults;settings.rebalance_land=1;City* observed=nullptr;
    assert(fleet_reason(*civ,*units[0],observed)==Fleet::Reason::LandSurplus && observed==target);
    home->mVehicleSpecialty=kReligious;assert(fleet_reason(*civ,*units[0],observed)==Fleet::Reason::None);home->mVehicleSpecialty=kMilitary;
    target->mPosition.x=1;assert(fleet_reason(*civ,*units[0],observed)==Fleet::Reason::None);target->mPosition.x=2;
    target->field_32C.x=2;assert(fleet_reason(*civ,*units[0],observed)==Fleet::Reason::None);target->field_32C.x=0;
    for(unsigned i=0;i<6;++i)units[i]->mLocomotion=kVehicleWater;
    assert(fleet_reason(*civ,*units[6],observed)==Fleet::Reason::None);
    for(unsigned i=0;i<36;++i)units[i]->mLocomotion=i<3?kVehicleLand:kVehicleAir;
    assert(fleet_reason(*civ,*units[0],observed)==Fleet::Reason::None); // Only the three defenders remain.
    world->mContinentMapDirty=true;assert(fleet_reason(*civ,*units[0],observed)==Fleet::Reason::None);
    fleet_planet=fleet_sdk_planet;fleet_are_enemies=fleet_hostile;fleet_water=old_water;fleet_continent=old_continent;
    current_vehicle_order=old_order;portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;
    settings=defaults;assert(ai_refs==0);
    puts("Land fleet adapter: native target, same sea, matching specialty, hostile land veto, fleet pressure and retained garrison passed.");
}
