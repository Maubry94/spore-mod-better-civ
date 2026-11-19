// Included inside BetterCiv, after the guarded terrain adapter.
// The planet ribbon cache is separate from cInterCityRoad and vehicle orders.
static TradeRoutes::State trade_route_state;
static bool trade_route_native_ready;
static_assert(sizeof(Simulator::cPlanetModel::cLandTradeRoute)==0x1c,"Native ribbon record stride");
static_assert(offsetof(Simulator::cPlanetModel,mLandTradeRoutes)==0xd4,"Native ribbon cache offset");
static_assert(offsetof(Simulator::cPlanetModel,mLandTradeRoutesDirty)==0xea,"Native ribbon dirty flag offset");
struct TradeNavPoint { Math::Vector3 position; unsigned char opaque[48]; };
static_assert(sizeof(TradeNavPoint)==60,"Native pathfinder waypoint stride");
using TradeNavPath=eastl::vector<TradeNavPoint>;
using TradePathManager=void* (__cdecl*)();
using TradePathSearch=bool (__thiscall*)(void*,const void*,const Math::Vector3&,
    const Math::Vector3&,TradeNavPath*,float,float);
static TradePathManager trade_path_manager;
static TradePathSearch trade_path_search;
static const void* trade_land_profile;
static Simulator::cPlanetModel* trade_sdk_planet() {return Simulator::cPlanetModel::Get();}
static auto trade_planet=trade_sdk_planet;
static bool trade_terrain_ready(Simulator::cPlanetModel* planet,float& sea) {
    Placement::SearchReport report;
    auto* terrain=placement_read_terrain(planet,report);
    if(!terrain) return false;
    sea=terrain->GetWaterHeight();return std::isfinite(sea)&&sea>=100;
}
static auto trade_ready=trade_terrain_ready;
static float trade_terrain_height(Simulator::cPlanetModel* planet,const Math::Vector3& point) {
    return planet->GetHeightAt(point);
}
static auto trade_height=trade_terrain_height;
static bool trade_log_allowed() {
    return settings.ai_diagnostics && trade_route_state.lines<512 && ++trade_route_state.lines;
}
static bool trade_live_city(City* wanted) {
    // Check membership before dereferencing a non-owning pointer in the cache.
    if(!wanted) return false;
    for(const auto& civ:portrait_civilizations()) {
        if(!civ || civ->mbIsDestroyed) continue;
        for(const auto& city:civ->mCities)
            if(city.get()==wanted) return !city->mbIsDestroyed;
    }
    return false;
}
static bool trade_city_geometry(City& city,TradeRoutes::Ends& end,uint64_t& hash) {
    using namespace TradeRoutes;
    const auto* walls=city.mpCityWalls.get();
    if(!walls || walls->mbIsDestroyed || !Finite(city.mPosition) || Length(city.mPosition)<100 ||
        !std::isfinite(walls->mInnerRadius) || !std::isfinite(walls->mOuterRadius) ||
        walls->mInnerRadius<5 || walls->mOuterRadius<walls->mInnerRadius || walls->mOuterRadius>100 ||
        walls->mGatesTransformed.size()<3 || walls->mGatesTransformed.size()>16) return false;
    end={city.mPosition,walls->mInnerRadius,walls->mOuterRadius};
    hash=HashBytes(hash,&city.mPoliticalID,sizeof(city.mPoliticalID));
    hash=HashBytes(hash,&end,sizeof(end));
    hash=HashBytes(hash,walls->mGatesTransformed.data(),3*sizeof(Point));
    return true;
}
struct TradeGatePair {Math::Vector3 a,b;float distance;};
static unsigned trade_gate_pairs(const City& a,const City& b,const TradeRoutes::Ends& ea,
    const TradeRoutes::Ends& eb,TradeGatePair (&pairs)[9]) {
    using namespace TradeRoutes;
    unsigned count=0;
    // Gates 0..2 are land approaches. Gate 3 is the dock, 5 freight.
    for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) {
        const auto& p=a.mpCityWalls->mGatesTransformed[i];
        const auto& q=b.mpCityWalls->mGatesTransformed[j];
        if(!Finite(p)||!Finite(q)||!NearEdge(p,ea)||!NearEdge(q,eb)) continue;
        pairs[count++]={p,q,Distance(p,q)};
    }
    std::sort(pairs,pairs+count,[](const auto& p,const auto& q){return p.distance<q.distance;});
    return count;
}
static void repair_trade_routes(int deltaGameTime,bool civ_mode) {
    using namespace TradeRoutes;
    if(!trade_route_native_ready || !ai_observer_ready || !civ_mode || deltaGameTime<=0 ||
        ai_time<10000 || ai_time<trade_route_state.next_scan || !trade_path_manager ||
        !trade_path_search || !trade_land_profile) return;
    trade_route_state.next_scan=ai_time+1000;
    auto* planet=trade_planet();float sea=0;
    if(!planet || planet->mLandTradeRoutes.size()>64 || !trade_ready(planet,sea)) return;
    for(auto& route:planet->mLandTradeRoutes) {
        if(route.city1==route.city2 || !trade_live_city(route.city1) || !trade_live_city(route.city2) ||
            route.path.size()>kMaxPoints) continue;
        auto& a=*route.city1;auto& b=*route.city2;
        Ends ea,eb;uint64_t topology=14695981039346656037ull;
        if(!trade_city_geometry(a,ea,topology)||!trade_city_geometry(b,eb,topology)) continue;
        auto& record=trade_route_state.Get(object_key(&a),object_key(&b),ai_time);
        const auto source=HashPath(route.path);
        const auto height=[&](const Point& direction){return trade_height(planet,direction*sea);};
        if(record.topology!=topology || (source!=record.path_hash && source!=record.source_hash)) {
            // Empty native ribbons have no misleading geometry to repair.
            if(route.path.empty()) continue;
            record.topology=topology;record.source_hash=source;record.path_hash=source;
            record.pending=false;record.attempt=0;record.replacement.clear();
            Path projected;const char* reason=nullptr;float surface_error=0;
            const bool grounded=GroundPath(route.path,sea,height,projected,reason,&surface_error);
            if(!grounded && !std::strcmp(reason,"invalid-terrain")) {
                record.topology=0;continue; // Terrain not usable: defer without changing the ribbon.
            }
            if(grounded && surface_error<=3.0f && HasCityEnds(route.path,ea,eb)) continue;
            if(grounded) reason=surface_error>3.0f?"off-terrain":"city-endpoint";
            record.pending=true;
            if(trade_log_allowed()) ModAPI::Log("[Better Civ Routes] t=%llu event=invalid-ribbon cities=%u,%u owners=%u,%u points=%u reason=%s",
                ai_time,a.mID,b.mID,a.mPoliticalID,b.mPoliticalID,unsigned(route.path.size()),reason);
            route.path.clear();record.path_hash=HashPath(route.path);planet->mLandTradeRoutesDirty=true;
        } else if(source==record.source_hash && source!=record.path_hash) {
            // The engine re-published the same rejected source. Reuse the result;
            // never restart searches every frame or change the native road itself.
            route.path=record.replacement;planet->mLandTradeRoutesDirty=true;
        }
        if(!record.pending || ai_time<trade_route_state.next_search) continue;
        auto* manager=trade_path_manager();if(!manager) continue;
        TradeGatePair pairs[9];const unsigned count=trade_gate_pairs(a,b,ea,eb,pairs);
        if(record.attempt>=count) {
            record.pending=false;
            if(trade_log_allowed()) ModAPI::Log("[Better Civ Routes] t=%llu event=ribbon-hidden cities=%u,%u reason=no-verified-land-path attempts=%u",
                ai_time,a.mID,b.mID,record.attempt);
            continue;
        }
        const auto& pair=pairs[record.attempt++];
        trade_route_state.next_search=ai_time+2000;
        // Same synchronous call/profile/range as native C414C0. Read-only result:
        // no city partner, cInterCityRoad, delivery or vehicle-order mutation.
        TradeNavPath nodes;
        const bool found=trade_path_search(manager,trade_land_profile,pair.a,pair.b,&nodes,0.0f,1.0f);
        Path input,projected;const char* reason="native-path-unavailable";
        bool valid=found && nodes.size()>=2 && nodes.size()<=kMaxPoints;
        if(valid) {
            for(const auto& node:nodes) input.push_back(node.position);
            valid=Distance(input.front(),pair.a)<=8 && Distance(input.back(),pair.b)<=8;
            reason="partial-path";
            if(valid) valid=GroundPath(input,sea,height,projected,reason)&&HasCityEnds(projected,ea,eb);
        }
        if(valid) {
            route.path=projected;record.replacement=projected;record.path_hash=HashPath(route.path);
            record.pending=false;planet->mLandTradeRoutesDirty=true;
        }
        if(trade_log_allowed()) ModAPI::Log("[Better Civ Routes] t=%llu event=%s cities=%u,%u attempt=%u nodes=%u points=%u reason=%s",
            ai_time,valid?"ribbon-rebuilt":"path-rejected",a.mID,b.mID,record.attempt,
            unsigned(nodes.size()),unsigned(projected.size()),reason);
    }
}
