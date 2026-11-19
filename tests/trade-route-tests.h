static Math::Vector3 route_point(float x,float y=0) {
    Math::Vector3 p{x,y,500};return p*(500.0f/TradeRoutes::Length(p));
}
static float route_test_height(Simulator::cPlanetModel*,const Math::Vector3& p) {
    auto q=p*(500.0f/TradeRoutes::Length(p));
    return std::fabs(q.x)<15 && std::fabs(q.y)<25 ? 470.0f:500.0f;
}
static void verify_trade_geometry() {
    using namespace TradeRoutes;
    Path source={route_point(-50),route_point(50)},out;
    const char* reason=nullptr;
    auto height=[](const TradeRoutes::Point& p){return route_test_height(nullptr,p);};
    assert(!GroundPath(source,480,height,out,reason) && !strcmp(reason,"water-crossing"));
    source={route_point(-50),route_point(-50,60),route_point(50,60),route_point(50)};
    assert(GroundPath(source,480,height,out,reason) && out.size()>source.size());
    float error=0;
    assert(GroundPath(source,480,[](const TradeRoutes::Point&){return 510.0f;},out,reason,&error) && error>9);
    assert(GroundPath(source,480,height,out,reason));
    Ends a{route_point(-90),29,37},b{route_point(90),29,37};
    assert(HasCityEnds(out,a,b));std::reverse(out.begin(),out.end());assert(HasCityEnds(out,a,b));
    source={a.center,b.center};assert(!HasCityEnds(source,a,b));
    source={route_point(-50),route_point(50)};
    assert(!GroundPath(source,480,[](const TradeRoutes::Point& p){return p.x>0?540.0f:500.0f;},out,reason));
    assert(!strcmp(reason,"steep-crossing"));
    assert(!GroundPath(source,480,[](const TradeRoutes::Point&){return NAN;},out,reason));
    assert(!strcmp(reason,"invalid-terrain"));
    source[0].x=INFINITY;assert(!GroundPath(source,480,height,out,reason));
    source.clear();assert(!GroundPath(source,480,height,out,reason));
    source.resize(kMaxPoints+1);assert(!GroundPath(source,480,height,out,reason));
    State state;for(unsigned i=1;i<=64;++i)state.Get(i,i+64,i);
    assert(state.Get(65,129,65).city1==65);assert(state.records[0].city1==65);
    state.ResetSession();assert(state.records[0].city1==0);
    puts("Trade geometry: water and cliff sampling, bounded paths, city-edge endpoints, reversed paths and finite terrain checks passed.");
}
static Simulator::cPlanetModel* route_fixture_world;
static bool route_fixture_ready,route_fixture_manager;
static unsigned route_search_count,route_search_mode;
static Simulator::cPlanetModel* route_mock_planet(){return route_fixture_world;}
static bool route_mock_ready(Simulator::cPlanetModel*,float& sea){sea=480;return route_fixture_ready;}
static void* __cdecl route_mock_manager(){return route_fixture_manager?reinterpret_cast<void*>(0x1234):nullptr;}
static bool __fastcall route_mock_search(void* manager,int,const void* profile,const Math::Vector3& a,
    const Math::Vector3& b,TradeNavPath* output,float min,float max) {
    assert(manager==reinterpret_cast<void*>(0x1234) && profile==reinterpret_cast<void*>(0x5678));
    assert(min==0.0f && max==1.0f && output && output->empty());++route_search_count;
    output->push_back({a,{}});
    output->push_back({route_point(-50,60),{}});output->push_back({route_point(50,60),{}});
    output->push_back({b,{}});
    if(route_search_mode==1) return false; // Even a populated failure result is rejected.
    if(route_search_mode==2 && route_search_count==1) return false;
    if(route_search_mode==3) output->back().position=route_point(0,60); // Incomplete.
    if(route_search_mode==4) output->clear();
    return true;
}
static void verify_trade_adapter() {
    using namespace TradeRoutes;
    alignas(8) unsigned char cb[sizeof(Civilization)]={},ab[sizeof(City)]={},bb[sizeof(City)]={},
        aw[sizeof(Simulator::cCityWalls)]={},bw[sizeof(Simulator::cCityWalls)]={};
    auto world_bytes=std::make_unique<unsigned char[]>(sizeof(Simulator::cPlanetModel));
    auto* world=reinterpret_cast<Simulator::cPlanetModel*>(world_bytes.get());
    auto* civ=reinterpret_cast<Civilization*>(cb);auto* a=reinterpret_cast<City*>(ab);auto* b=reinterpret_cast<City*>(bb);
    auto* wa=reinterpret_cast<Simulator::cCityWalls*>(aw);auto* wb=reinterpret_cast<Simulator::cCityWalls*>(bw);
    uintptr_t table[]={reinterpret_cast<uintptr_t>(ai_mock_addref),reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ,0,table);City* cities[]={a,b};
    put_pointer(civ,kCivCitiesBegin,cities);put_pointer(civ,kCivCitiesEnd,cities+2);
    a->mID=1;b->mID=2;a->mPoliticalID=100;b->mPoliticalID=200;
    a->mPosition=route_point(-90);b->mPosition=route_point(90);
    put_pointer(a,offsetof(City,mpCityWalls),wa);put_pointer(b,offsetof(City,mpCityWalls),wb);
    wa->mInnerRadius=wb->mInnerRadius=29;wa->mOuterRadius=wb->mOuterRadius=37;
    Path ag={route_point(-50),route_point(-55,10),route_point(-55,-10)};
    Path bg={route_point(50),route_point(55,10),route_point(55,-10)};
    wa->mGatesTransformed.swap(ag);wb->mGatesTransformed.swap(bg);
    eastl::vector<Simulator::cPlanetModel::cLandTradeRoute> routes;
    routes.push_back({a,b,{route_point(-50),route_point(50)}});world->mLandTradeRoutes.swap(routes);
    auto& path=world->mLandTradeRoutes[0].path;const Path bad=path;
    portrait_fixture_civs.push_back(civ);portrait_civilizations=mock_portrait_civilizations;
    const auto old_manager=trade_path_manager;const auto old_search=trade_path_search;const auto old_profile=trade_land_profile;
    trade_planet=route_mock_planet;trade_ready=route_mock_ready;trade_height=route_test_height;
    trade_path_manager=route_mock_manager;trade_path_search=reinterpret_cast<TradePathSearch>(route_mock_search);
    trade_land_profile=reinterpret_cast<void*>(0x5678);route_fixture_world=world;
    trade_route_native_ready=ai_observer_ready=true;settings=defaults;
    auto reset=[&](){trade_route_state.ResetSession();path=bad;world->mLandTradeRoutesDirty=false;
        route_fixture_ready=route_fixture_manager=true;route_search_count=route_search_mode=0;ai_time=10000;};
    reset();repair_trade_routes(0,true);repair_trade_routes(1000,false);assert(!route_search_count && path==bad);
    ai_time=9999;repair_trade_routes(1000,true);assert(!route_search_count);
    ai_time=10000;route_fixture_ready=false;repair_trade_routes(1000,true);assert(!route_search_count && path==bad);
    reset();trade_route_native_ready=false;repair_trade_routes(1000,true);assert(path==bad);trade_route_native_ready=true;
    reset();repair_trade_routes(1000,true);assert(route_search_count==1 && path!=bad && !path.empty() && world->mLandTradeRoutesDirty);
    const Path repaired=path;world->mLandTradeRoutesDirty=false;ai_time+=3000;repair_trade_routes(1000,true);
    assert(route_search_count==1 && path==repaired && !world->mLandTradeRoutesDirty);
    path=bad;ai_time+=3000;repair_trade_routes(1000,true);assert(route_search_count==1 && path==repaired);
    // A normal cache entry remains byte-for-byte identical; no path search.
    reset();path=repaired;repair_trade_routes(1000,true);assert(!route_search_count && path==repaired && !world->mLandTradeRoutesDirty);
    // Full search failure removes only the misleading display line, at most 9 attempts.
    reset();route_search_mode=1;
    for(unsigned t=10000;t<=40000;t+=1000){ai_time=t;repair_trade_routes(1000,true);}
    assert(route_search_count==9 && path.empty());
    // Ownership/native geometry change allows another bounded attempt.
    ++a->mPoliticalID;path=bad;route_search_mode=0;ai_time+=2000;repair_trade_routes(1000,true);
    assert(route_search_count==10 && !path.empty());
    reset();route_search_mode=2;repair_trade_routes(1000,true);assert(route_search_count==1 && path.empty());
    ai_time+=1000;repair_trade_routes(1000,true);assert(route_search_count==1);
    ai_time+=1000;repair_trade_routes(1000,true);assert(route_search_count==2 && !path.empty());
    for(unsigned mode:{3u,4u}) {reset();route_search_mode=mode;repair_trade_routes(1000,true);assert(path.empty());}
    // No manager: no native call. The visually invalid ribbon can still be hidden.
    reset();route_fixture_manager=false;repair_trade_routes(1000,true);assert(!route_search_count && path.empty());
    route_fixture_manager=true;ai_time+=2000;repair_trade_routes(1000,true);assert(route_search_count==1);
    reset();world->mLandTradeRoutes[0].city2=reinterpret_cast<City*>(1);repair_trade_routes(1000,true);
    assert(!route_search_count && path==bad);world->mLandTradeRoutes[0].city2=b;
    reset();path.clear();repair_trade_routes(1000,true);assert(!route_search_count && !world->mLandTradeRoutesDirty);
    reset();b->mbIsDestroyed=true;repair_trade_routes(1000,true);assert(!route_search_count && path==bad);b->mbIsDestroyed=false;
    reset();repair_trade_routes(1000,true);ai_lifecycle.Read(nullptr);
    assert(!trade_route_state.records[0].city1 && ai_time==0);
    world->mLandTradeRoutes.swap(routes);wa->mGatesTransformed.swap(ag);wb->mGatesTransformed.swap(bg);
    trade_planet=trade_sdk_planet;trade_ready=trade_terrain_ready;trade_height=trade_terrain_height;
    trade_path_manager=old_manager;trade_path_search=old_search;trade_land_profile=old_profile;
    trade_route_native_ready=ai_observer_ready=false;trade_route_state.ResetSession();
    portrait_fixture_civs.clear();portrait_civilizations=sdk_portrait_civilizations;assert(ai_refs==0);settings=defaults;
    puts("Trade adapter: native ABI, guarded lifecycle, valid-cache preservation, repair/failure, alternate gates, bounded retries, stale cities and reload reset passed.");
}
