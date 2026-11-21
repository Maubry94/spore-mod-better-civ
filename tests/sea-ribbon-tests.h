static TradeRoutes::Path sea_test_drawn;
static unsigned sea_test_searches;
static bool sea_test_open_water;
static float sea_test_terrain(Simulator::cPlanetModel*,const Math::Vector3& p) {
    return !sea_test_open_water && p.x>421 && std::fabs(p.y)<72 && std::fabs(p.z)<80?485.f:462.f;
}
static bool __fastcall sea_test_no_native_path(void* manager,int,const void* profile,const Math::Vector3&,
    const Math::Vector3&,TradeNavPath* nodes,float near_limit,float far_limit) {
    assert(manager==reinterpret_cast<void*>(0x1234) && profile==reinterpret_cast<void*>(0x5678));
    assert(nodes && nodes->empty() && near_limit==0.f && far_limit==1.f);
    ++sea_test_searches;
    return false;
}
static uint32_t __fastcall sea_test_renderer(Simulator::cPlanetModel* planet,int,uint32_t,uint32_t,uint32_t) {
    assert(sea_ribbon_drawing_world==planet && planet->mSeaTradeRoutes.size()==1);
    const auto pair=planet->mSeaTradeRoutes.front();
    const auto& from=pair.first->field_32C;
    const auto& to=pair.second->field_32C;
    TradeNavPath points;
    assert(!sea_ribbon_search(reinterpret_cast<void*>(0x1234),0,reinterpret_cast<void*>(0x5678),
        from,to,&points,0,1));
    sea_test_drawn.clear();
    for(const auto& n:points) sea_test_drawn.push_back(n.position);
    if(points.empty()) sea_test_drawn={from,to}; // native fallback consumed by this renderer
    planet->mSeaTradeRoutesDirty=false;
    return 19;
}
static void verify_sea_ribbon_lifecycle() {
    auto first=std::make_unique<unsigned char[]>(sizeof(Simulator::cPlanetModel));
    auto second=std::make_unique<unsigned char[]>(sizeof(Simulator::cPlanetModel));
    auto* world=reinterpret_cast<Simulator::cPlanetModel*>(first.get());
    auto* ocean=reinterpret_cast<Simulator::cPlanetModel*>(second.get());
    alignas(8) unsigned char city_bytes[2][sizeof(City)]={};
    auto* home=reinterpret_cast<City*>(city_bytes[0]);auto* dest=reinterpret_cast<City*>(city_bytes[1]);
    home->field_32C=SeaRibbon::Direction({1,-0.47f,0})*480;
    dest->field_32C=SeaRibbon::Direction({1,0.47f,0})*480;
    uint32_t map=0;
    for(auto* p:{world,ocean}) {p->mContinentMap=&map;p->mSeaTradeRoutes.push_back({home,dest});}
    const auto old_settings=settings;
    const auto old_game=game_mode;const auto old_stage=current_mode;
    const auto old_planet=trade_planet;const auto old_ready=trade_ready;const auto old_height=trade_height;
    const auto old_search=trade_path_search;const auto old_draw=original_draw_routes;
    const auto old_continent=fleet_continent;const auto old_order=current_vehicle_order;
    const bool old_fleet=fleet_native_ready,old_routes=trade_route_native_ready,old_observer=ai_observer_ready;
    const bool old_fixture_ready=route_fixture_ready;auto* old_world=route_fixture_world;
    const auto old_time=ai_time,old_scan=next_sea_display_scan,old_hash=sea_display_hash;
    settings.assist_rival_trade=1;settings.ai_diagnostics=0;
    game_mode=mock_mode;current_mode=kCivilizationMode;
    trade_planet=route_mock_planet;trade_ready=route_mock_ready;trade_height=sea_test_terrain;
    route_fixture_ready=true;route_fixture_world=world;
    trade_path_search=reinterpret_cast<TradePathSearch>(sea_test_no_native_path);
    original_draw_routes=reinterpret_cast<DrawPlanetRoutes>(sea_test_renderer);
    fleet_continent=reinterpret_cast<FleetContinent>(fleet_test_continent);
    current_vehicle_order=reinterpret_cast<CurrentVehicleOrder>(combat_fixture_order_getter);
    fleet_native_ready=trade_route_native_ready=ai_observer_ready=true;
    sea_ribbon_cache.Reset();sea_display_hash=next_sea_display_scan=0;ai_time=1;
    sea_test_searches=0;sea_test_open_water=false;
    const auto draw=[](Simulator::cPlanetModel* p) {
        assert(draw_transport_routes(p,0,0,0,0)==19);
        assert(!sea_ribbon_drawing_world && !p->mSeaTradeRoutesDirty && p->mSeaTradeRoutes.size()==1);
    };
    const auto wait_redraw=[](Simulator::cPlanetModel* p) {
        for(unsigned n=0;!p->mSeaTradeRoutesDirty && n<=SeaRibbon::vertices;++n) {
            ai_time+=100;
            update_sea_display(100,true);
        }
        assert(p->mSeaTradeRoutesDirty);
    };
    draw(world);assert(sea_test_drawn.size()==1 && sea_test_searches==1);
    wait_redraw(world);
    draw(world);assert(sea_test_drawn.size()>2 && sea_test_searches==2);
    assert(SeaRibbon::Inspect(sea_test_drawn,480,[&](const Math::Vector3& p){return sea_test_terrain(world,p);})
        ==SeaRibbon::Surface::Water);
    assert(TradeRoutes::Distance(sea_test_drawn.front(),home->field_32C)<0.01f);
    assert(TradeRoutes::Distance(sea_test_drawn.back(),dest->field_32C)<0.01f);

    // Same coordinates on a different planet have no island. One check later
    // the renderer should be allowed to use its original direct segment.
    route_fixture_world=ocean;sea_test_open_water=true;
    draw(ocean);assert(sea_test_drawn.size()==1);
    wait_redraw(ocean);draw(ocean);assert(sea_test_drawn.size()==2);
    // The strategy uses this reset on mode entry, save read and load. Reuse
    // the same world address to verify a reload cannot keep the cached result.
    reset_ai_observations();
    assert(!sea_ribbon_cache.world && sea_ribbon_cache.active==-1);
    sea_test_open_water=false;draw(ocean);assert(sea_test_drawn.size()==1);
    wait_redraw(ocean);draw(ocean);assert(sea_test_drawn.size()>2);
    for(auto* p:{world,ocean}) RouteList().swap(p->mSeaTradeRoutes);
    sea_ribbon_cache.Reset();sea_test_drawn.clear();sea_display_hash=old_hash;next_sea_display_scan=old_scan;
    ai_time=old_time;settings=old_settings;game_mode=old_game;current_mode=old_stage;
    trade_planet=old_planet;trade_ready=old_ready;trade_height=old_height;trade_path_search=old_search;
    original_draw_routes=old_draw;fleet_continent=old_continent;current_vehicle_order=old_order;
    route_fixture_ready=old_fixture_ready;route_fixture_world=old_world;
    fleet_native_ready=old_fleet;trade_route_native_ready=old_routes;ai_observer_ready=old_observer;
    puts("Sea ribbon: deferred terrain check, redraw, native fallback, planet change and reload exercised.");
}

static void verify_sea_ribbon() {
    using namespace SeaRibbon;
    using TradeRoutes::Path;
    constexpr float sea=500.f;
    const auto a=Direction({1,-0.47f,0})*sea,b=Direction({1,0.47f,0})*sea;
    auto island=[](const SeaRibbon::Point& p){return p.x>440 && std::fabs(p.y)<75 && std::fabs(p.z)<83?505.f:485.f;};
    auto water=[](const SeaRibbon::Point&){return 485.f;};
    assert(Inspect(Path{a,b},sea,island)==Surface::Land);
    assert(Inspect(Path{a,b},sea,water)==Surface::Water);
    assert(Inspect(Path{a,b},sea,[](const SeaRibbon::Point&){return NAN;})==Surface::Unknown);
    Planner planner;
    Path out;
    auto result=planner.Begin(a,b,sea,island,out);
    for(int step=0;result==Result::Pending && step<vertices;++step) result=planner.Step(island,out);
    assert(result==Result::Found && out.size()>2 && Inspect(out,sea,island)==Surface::Water);
    assert(Distance(a,out.front())<0.1f && Distance(b,out.back())<0.1f);
    for(size_t i=1;i<out.size();++i) assert(Distance(out[i-1],out[i])<=sample_spacing+0.02f);
    auto isolated=[&](const SeaRibbon::Point& p){return Distance(p,a)<7 || Distance(p,b)<7?485.f:505.f;};
    result=planner.Begin(a,b,sea,isolated,out);
    for(int step=0;result==Result::Pending && step<vertices;++step) result=planner.Step(isolated,out);
    assert(result==Result::NoPath);
    result=planner.Begin(a,b,sea,water,out);
    assert(result==Result::Found && Distance(out.front(),a)<0.01f && Distance(out.back(),b)<0.01f);

    SeaRibbonEntry e;
    e.from=a;e.to=b;e.state=SeaRibbonState::Hidden;
    TradeNavPath native;
    set_sea_ribbon_output(e,native);
    assert(native.size()==1 && Distance(native.front().position,a)<0.01f);
    e.state=SeaRibbonState::Check;native.clear();set_sea_ribbon_output(e,native);assert(native.size()==1);
    e.state=SeaRibbonState::Rebuilt;e.corrected=out;set_sea_ribbon_output(e,native);
    assert(native.size()==out.size() && Distance(native.back().position,b)<0.01f);
    e.state=SeaRibbonState::Native;native.clear();set_sea_ribbon_output(e,native);assert(native.empty());
    verify_sea_ribbon_lifecycle();
}
