static unsigned reclaim_orders,proposal_arrivals;
static bool __fastcall proposal_mock_arrival(Simulator::cLocomotiveObject*,int){++proposal_arrivals;return true;}
static void __fastcall reclaim_mock_order(Vehicle*,int,void* target,int kind,int) {
    ++reclaim_orders;combat_fixture_order.kind=kind;combat_fixture_order.target=target;
}
static void verify_harbor_fix2() {
    alignas(8) unsigned char cb[2][sizeof(Civilization)]={},ct[2][sizeof(City)]={},ub[sizeof(Vehicle)]={},wb[sizeof(Simulator::cPlanetModel)]={};
    auto* own=reinterpret_cast<Civilization*>(cb[0]);auto* enemy=reinterpret_cast<Civilization*>(cb[1]);
    auto* home=reinterpret_cast<City*>(ct[0]);auto* port=reinterpret_cast<City*>(ct[1]);
    auto* unit=reinterpret_cast<Vehicle*>(ub);auto* world=reinterpret_cast<Simulator::cPlanetModel*>(wb);
    uintptr_t table[20]={};table[0]=reinterpret_cast<uintptr_t>(ai_mock_addref);table[1]=reinterpret_cast<uintptr_t>(ai_mock_release);table[0x4c/4]=reinterpret_cast<uintptr_t>(mock_political_id);
    put_pointer(own,0,table);put_pointer(enemy,0,table);put_pointer(unit,0,table);
    own->mID=7;enemy->mID=8;cb[0][0x24]=7;cb[1][0x24]=8;
    home->mID=10;port->mID=11;home->mPosition=port->mPosition={1,0,500};port->field_32C={0,0,500};
    put_pointer(home,offsetof(City,mpCivilization),own);put_pointer(port,offsetof(City,mpCivilization),enemy);
    City* a[]={home};City* b[]={port};Vehicle* units[]={unit};
    put_pointer(own,kCivCitiesBegin,a);put_pointer(own,kCivCitiesEnd,a+1);
    put_pointer(enemy,kCivCitiesBegin,b);put_pointer(enemy,kCivCitiesEnd,b+1);
    put_pointer(own,kCivVehiclesBegin,units);put_pointer(own,kCivVehiclesEnd,units+1);
    unit->mID=20;unit->mPosition={0,0,500};unit->mHealthPoints=100;unit->mTimeSinceLastWeaponFireMS=60000;unit->mLocomotion=kVehicleWater;
    bool land[]={false,true,false};uint32_t map=0;world->mContinentMap=&map;
    put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater),land);put_pointer(world,offsetof(Simulator::cPlanetModel,mContinentAboveWater)+4,land+3);
    const auto old_order=current_vehicle_order;const auto old_set=set_vehicle_order;const auto old_continent=fleet_continent;const auto old_water=fleet_water;const auto old_sell=fleet_sell;
    fleet_fixture_world=world;fleet_planet=fleet_test_planet;fleet_are_enemies=fleet_test_enemy;
    fleet_continent=reinterpret_cast<FleetContinent>(fleet_test_continent);fleet_water=reinterpret_cast<FleetWater>(fleet_mock_water);fleet_sell=reinterpret_cast<FleetSell>(fleet_mock_sell);
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);set_vehicle_order=reinterpret_cast<SetVehicleOrder>(reclaim_mock_order);
    portrait_fixture_civs.push_back(own);portrait_fixture_civs.push_back(enemy);portrait_civilizations=mock_portrait_civilizations;
    settings=defaults;settings.retire_stranded=1;fleet_native_ready=trade_transit_native_ready=ai_observer_ready=true;current_mode=kCivilizationMode;game_mode=mock_mode;
    auto getter=reinterpret_cast<CurrentVehicleOrder>(feasible_trade_order::detoured_function);
    unit->mPurpose=kVehicleEconomic;combat_fixture_order={};combat_fixture_order.kind=9;combat_fixture_order.target=port;
    assert(trade_ship_impossible(*unit,combat_fixture_order));
    assert(getter(unit)->target==nullptr && combat_fixture_order.kind==9); // Native cancels; adapter doesn't edit the order.
    home->field_32C={0,0,500};assert(!trade_ship_impossible(*unit,combat_fixture_order) && getter(unit)==&combat_fixture_order);
    port->field_32C={2,0,500};assert(trade_ship_impossible(*unit,combat_fixture_order));port->field_32C={0,0,500};
    combat_fixture_order.kind=5;home->field_32C={};assert(getter(unit)->target==nullptr);
    combat_fixture_order.kind=1;assert(getter(unit)==&combat_fixture_order);
    combat_fixture_order.kind=9;unit->mLocomotion=kVehicleAir;assert(getter(unit)==&combat_fixture_order);unit->mLocomotion=kVehicleWater;
    settings.assist_rival_trade=settings.retire_stranded=0;assert(getter(unit)==&combat_fixture_order);settings.retire_stranded=1;
    const auto old_arrival=original_trade_arrival;original_trade_arrival=reinterpret_cast<TradeArrival>(proposal_mock_arrival);
    settings.retry_blocked_paths=2;placement_state={};proposal_arrivals=0;ai_time=10000;
    Recovery::Sample sample;sample.key=object_key(unit);sample.order=9;sample.target=reinterpret_cast<uintptr_t>(port);sample.goal={30,0,500};sample.position=unit->mPosition;sample.goal_stop=1;sample.acceptable_stop=2;
    auto* step=placement_state.Find(sample.key,ai_time);step->Begin(sample,{0,8,500},2,ai_time);
    unit->field_208=step->imposed.goal;unit->field_214=1;unit->field_254=2;
    assert(!trade_proposal_arrival(unit,0) && !proposal_arrivals); // A detour endpoint isn't the foreign port.
    combat_fixture_order.kind=1;assert(trade_proposal_arrival(unit,0) && proposal_arrivals==1);
    placement_state={};original_trade_arrival=old_arrival;
    City* target=nullptr;
    for(auto purpose:{kVehicleMilitary,kVehicleCultural}) {
        unit->mPurpose=purpose;combat_fixture_order={};fleet_state={};fleet_mock_sales=reclaim_orders=0;
        assert(fleet_reason(*own,*unit,target)==Fleet::Reason::ReclaimPort && target==port);
        for(unsigned t=0;t<30000;t+=2000){ai_time=t;manage_unused_fleets(2000,true);}assert(!reclaim_orders);
        ai_time=30000;manage_unused_fleets(2000,true);assert(reclaim_orders==1 && !fleet_mock_sales && combat_fixture_order.target==port && combat_fixture_order.kind==(purpose==kVehicleMilitary?2:3));
        // Lost mission may retry once after two minutes. It may not loop or sell useful combat ships.
        combat_fixture_order={};
        for(unsigned t=32000;t<=148000;t+=2000){ai_time=t;manage_unused_fleets(2000,true);}assert(reclaim_orders==1);
        ai_time=150000;manage_unused_fleets(2000,true);assert(reclaim_orders==2);
        combat_fixture_order={};
        for(unsigned t=152000;t<=400000;t+=2000){ai_time=t;manage_unused_fleets(2000,true);}assert(reclaim_orders==2 && !fleet_mock_sales);
        port->field_32C={2,0,500};assert(fleet_reason(*own,*unit,target)==Fleet::Reason::NoPort && !target);port->field_32C={0,0,500};
        world->mContinentMapDirty=true;assert(fleet_reason(*own,*unit,target)==Fleet::Reason::None);world->mContinentMapDirty=false;
        home->field_32C={0,0,500};assert(fleet_reason(*own,*unit,target)==Fleet::Reason::None);home->field_32C={};
        own->mIsPlayerOwned=true;assert(fleet_reason(*own,*unit,target)==Fleet::Reason::None);own->mIsPlayerOwned=false;
        combat_fixture_order.kind=7;assert(fleet_reason(*own,*unit,target)==Fleet::Reason::None);combat_fixture_order={};
    }
    portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;
    fleet_planet=fleet_sdk_planet;fleet_are_enemies=fleet_hostile;fleet_continent=old_continent;fleet_water=old_water;fleet_sell=old_sell;current_vehicle_order=old_order;set_vehicle_order=old_set;
    fleet_native_ready=trade_transit_native_ready=ai_observer_ready=false;fleet_state={};settings=defaults;assert(ai_refs==0);
    puts("Harbor test 2: revalidated sea proposals/deliveries, disconnected seas, native cancellation view, combat port reclaim, both specialties, bounded retry, retained missions and player/off gates passed.");
}
