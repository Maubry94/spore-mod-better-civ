static void verify_trade_agreement_guard() {
    alignas(8) unsigned char cb[2][sizeof(Civilization)]={},city_bytes[2][sizeof(City)]={},unit_bytes[sizeof(Vehicle)]={};
    auto* own=reinterpret_cast<Civilization*>(cb[0]);auto* other=reinterpret_cast<Civilization*>(cb[1]);
    auto* home=reinterpret_cast<City*>(city_bytes[0]);auto* partner=reinterpret_cast<City*>(city_bytes[1]);
    auto* u=reinterpret_cast<Vehicle*>(unit_bytes);
    uintptr_t table[20]={};table[0]=reinterpret_cast<uintptr_t>(ai_mock_addref);table[1]=reinterpret_cast<uintptr_t>(ai_mock_release);
    table[0x4c/4]=reinterpret_cast<uintptr_t>(mock_political_id);
    put_pointer(own,0,table);put_pointer(other,0,table);cb[0][0x24]=7;cb[1][0x24]=8;
    own->mID=7;other->mID=8;home->mPoliticalID=7;partner->mPoliticalID=8;
    home->field_32C=partner->field_32C={0,0,500};u->mLocomotion=kVehicleWater;
    put_pointer(home,offsetof(City,mpCivilization),own);put_pointer(partner,offsetof(City,mpCivilization),other);
    City* own_cities[]={home};City* other_cities[]={partner};Vehicle* units[]={u};
    put_pointer(own,kCivCitiesBegin,own_cities);put_pointer(own,kCivCitiesEnd,own_cities+1);
    put_pointer(other,kCivCitiesBegin,other_cities);put_pointer(other,kCivCitiesEnd,other_cities+1);
    put_pointer(own,kCivVehiclesBegin,units);put_pointer(own,kCivVehiclesEnd,units+1);
    TradePartner agreement={7,home,0};
    put_pointer(partner,kCityTradePartnersBegin,&agreement);put_pointer(partner,kCityTradePartnersEnd,&agreement+1);
    portrait_fixture_civs.push_back(own);portrait_fixture_civs.push_back(other);portrait_civilizations=mock_portrait_civilizations;
    VehicleOrder order={};order.kind=5;order.target=partner;order.political_id=7;
    assert(trade_delivery_valid(*u,order));
    order.target=reinterpret_cast<void*>(0x1234);assert(!trade_delivery_valid(*u,order));order.target=partner;
    agreement.political_id=9;assert(!trade_delivery_valid(*u,order));agreement.political_id=7;
    order.political_id=9;assert(!trade_delivery_valid(*u,order));order.political_id=7;
    home->field_32C={};assert(!trade_delivery_valid(*u,order));home->field_32C={0,0,500};
    partner->field_32C={};assert(!trade_delivery_valid(*u,order));partner->field_32C={0,0,500};
    partner->mbIsDestroyed=true;assert(!trade_delivery_valid(*u,order));partner->mbIsDestroyed=false;
    partner->mPoliticalID=7;assert(!trade_delivery_valid(*u,order));partner->mPoliticalID=8;
    order.kind=9;assert(!trade_delivery_valid(*u,order));order.kind=5;
    assert(trade_delivery_valid(*u,order));
    portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;assert(ai_refs==0);
    puts("Trade agreement guard: live membership, partner cancellation, owner changes, both ports and order type passed.");
}
static bool transit_agreement;
static bool transit_mock_agreement(Vehicle&,const VehicleOrder&){return transit_agreement;}
static unsigned transit_native_calls;
static Vehicle* transit_expected_unit;
static const int* transit_expected_phase;
static bool __cdecl transit_native_probe(Vehicle* unit,unsigned b,unsigned c,unsigned d,unsigned e,
    const int* phase,unsigned g,float delta) {
    assert(unit==transit_expected_unit && phase==transit_expected_phase);
    assert(b==2 && c==3 && d==4 && e==5 && g==7 && delta==0.125f);
    ++transit_native_calls;return false;
}
__declspec(naked) static void transit_probe_continuation() {
    __asm {
        pop ebp
        pop ebx
        add esp,0Ch
        jmp transit_native_probe
    }
}
static void verify_trade_transit() {
    verify_trade_agreement_guard();
    alignas(8) unsigned char bytes[sizeof(Vehicle)]={};auto* u=reinterpret_cast<Vehicle*>(bytes);
    uintptr_t moves[64]={};moves[0xe0/4]=reinterpret_cast<uintptr_t>(placement_mock_move);put_pointer(u,0x34,moves);
    u->mID=20179;u->mPurpose=kVehicleEconomic;u->mLocomotion=kVehicleWater;u->mHealthPoints=100;u->mbInView=true;
    u->field_250=1;u->field_214=1;u->field_254=2;u->mPosition={0,0,500};u->field_208={30,0,499};
    u->mCargoType=1;u->mCargoUnits=7;
    const auto old_order=current_vehicle_order;const auto old_check=trade_delivery_check;
    const auto old_continue=trade_update_continue;
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    trade_delivery_check=transit_mock_agreement;recovery_check_methods=recovery_mock_methods;
    current_mode=kCivilizationMode;game_mode=mock_mode;ai_observer_ready=recovery_native_ready=trade_transit_native_ready=true;
    settings=defaults;settings.retry_blocked_paths=2;
    const auto reset=[&](){
        trade_transit_state={};placement_state={};ai_time=10000;placement_mock_moves=0;transit_agreement=true;
        combat_fixture_order={};combat_fixture_order.kind=5;combat_fixture_order.target=reinterpret_cast<void*>(0x1234);
        combat_fixture_order.political_id=7;u->field_270=8;u->field_208={30,0,499};u->mPosition={0,0,500};
    };
    int phase=3;reset();
    assert(trade_delivery_preflight(u,&phase) && placement_mock_moves==1);
    assert(u->mCargoType==1 && u->mCargoUnits==7 && phase==3 && combat_fixture_order.kind==5);
    // Give the native locomotive one second to consume the queued request.
    ai_time=10999;assert(trade_delivery_preflight(u,&phase) && placement_mock_moves==1);
    ai_time=11000;assert(!trade_delivery_preflight(u,&phase) && placement_mock_moves==1);
    u->field_270=0;assert(!trade_delivery_preflight(u,&phase));
    u->field_270=8;assert(trade_delivery_preflight(u,&phase) && placement_mock_moves==2);
    u->field_270=0;assert(!trade_delivery_preflight(u,&phase));
    u->field_270=8;assert(!trade_delivery_preflight(u,&phase) && placement_mock_moves==2);
    // A real journey, not repeated small detours, renews the retry budget.
    u->mPosition={0,150,500};assert(trade_delivery_preflight(u,&phase) && placement_mock_moves==3);
    reset();transit_agreement=false;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);
    reset();combat_fixture_order.kind=0;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);
    reset();combat_fixture_order.kind=1;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);
    reset();phase=4;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);phase=3;
    reset();settings.retry_blocked_paths=0;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);settings.retry_blocked_paths=2;
    reset();u->field_208.x=NAN;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);
    reset();u->field_270=12;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);
    reset();u->field_250=3;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);u->field_250=1;
    reset();u->mbInView=false;assert(!trade_delivery_preflight(u,&phase) && !placement_mock_moves);u->mbInView=true;
    reset();assert(trade_delivery_preflight(u,&phase));u->field_208.x=100;
    assert(!trade_delivery_preflight(u,&phase) && placement_mock_moves==1); // New local request wins.
    reset();u->field_270=0;
    Recovery::Sample s;s.key=object_key(u);s.owner=7;s.order=5;s.target=0x1234;s.position=u->mPosition;s.goal=u->field_208;s.goal_stop=1;s.acceptable_stop=2;
    auto* escape=placement_state.Find(s.key,ai_time);escape->Begin(s,{0,-8,500},2,ai_time);
    u->field_208=escape->imposed.goal;
    assert(trade_delivery_preflight(u,&phase) && !placement_mock_moves && phase==3);
    u->mPosition=u->field_208;ai_time+=500;
    assert(trade_delivery_preflight(u,&phase) && placement_mock_moves==1 && u->field_208.x==30 && !escape->active);
    assert(u->mCargoUnits==7 && u->mCargoType==1 && phase==3);
    ai_time+=499;assert(trade_delivery_preflight(u,&phase));
    u->field_208.x=100;assert(!trade_delivery_preflight(u,&phase));u->field_208.x=30;
    ai_time+=1;assert(!trade_delivery_preflight(u,&phase)); // Native arrival is checked only on the restored request.
    assert(trade_detour_phase(*u));phase=1;trade_delivery_preflight(u,&phase);assert(!trade_detour_phase(*u));
    phase=3;trade_delivery_preflight(u,&phase);ai_time+=1501;assert(!trade_detour_phase(*u));
    // Run the actual naked entry in both paths, with every stack argument.
    using Entry=bool(__cdecl*)(Vehicle*,unsigned,unsigned,unsigned,unsigned,const int*,unsigned,float);
    auto entry=reinterpret_cast<Entry>(guarded_trade_update);
    trade_update_continue=reinterpret_cast<uintptr_t>(transit_probe_continuation);
    transit_expected_unit=u;transit_expected_phase=&phase;transit_native_calls=0;
    reset();u->field_270=0;
    assert(!entry(u,2,3,4,5,&phase,7,0.125f) && transit_native_calls==1);
    u->field_270=8;
    assert(entry(u,2,3,4,5,&phase,7,0.125f) && transit_native_calls==1);
    combat_fixture_order.kind=1;
    assert(!entry(u,2,3,4,5,&phase,7,0.125f) && transit_native_calls==2);
    ai_lifecycle.Read(nullptr);assert(!trade_transit_state.records[0].key && !placement_state.records[0].key);
    current_vehicle_order=old_order;trade_delivery_check=old_check;trade_update_continue=old_continue;
    recovery_check_methods=recovery_methods_match;settings=defaults;
    ai_observer_ready=recovery_native_ready=trade_transit_native_ready=false;
    puts("Trade transit: native entry ABI, detour arrival isolation, restored-request grace, unchanged cargo/orders, bounded retries, cancellation/loading/off/unsupported gates and load reset passed.");
}
