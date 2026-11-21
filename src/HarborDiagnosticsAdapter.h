// Read-only evidence, separate from the capped detailed trajectory journal.
static uint64_t harbor_next_sample,harbor_route_hash;
static unsigned harbor_lines;
static_assert(offsetof(Simulator::cPlanetModel,mSeaTradeRoutes)==0xac);
static_assert(offsetof(Simulator::cPlanetModel,mAirTradeRoutes)==0xc0);
static bool harbor_log() {return settings.ai_diagnostics && harbor_lines<4096 && ++harbor_lines;}
static uint32_t harbor_city_id(const City* wanted) {
    return trade_live_city(const_cast<City*>(wanted))?wanted->mID:UINT32_MAX;
}
static void observe_harbors(int delta,bool civ_mode) {
    if(!settings.ai_diagnostics || !ai_observer_ready || !civ_mode || delta<=0 ||
        ai_time<harbor_next_sample)return;
    harbor_next_sample=ai_time+1000;
    for(const auto& civ:portrait_civilizations()) {
        if(!civ || civ->mbIsDestroyed)continue;
        for(const auto& u:civ->mVehicles) {
            if(!u || u->mbIsDestroyed || u->mbDead ||
                (u->mPurpose!=kVehicleEconomic && !(u->mPurpose==kVehicleCultural && u->mLocomotion==kVehicleAir)))continue;
            auto* r=trade_transit_state.Find(object_key(u.get()),ai_time);if(!r)continue;
            r->last_seen=ai_time;
            const auto* o=current_vehicle_order?current_vehicle_order(u.get()):nullptr;
            const int order=o?o->kind:-1;
            const auto target=CombatDiagnostics::Resolve(o?o->target:nullptr,false,portrait_civilizations());
            const auto aim=CombatDiagnostics::Resolve(u->mpTarget.get(),true,portrait_civilizations());
            char reason[128]={};
            if(trade_transit_native_ready) {
                const auto length=std::min<size_t>(u->field_D14.size(),sizeof(reason)-1);
                for(size_t i=0;i<length;++i) {
                    const auto c=static_cast<unsigned char>(u->field_D14[i]);
                    reason[i]=(c>=32 && c<127)?char(c):'?';
                }
            }
            uint64_t hash=14695981039346656037ull;
            hash=TradeRoutes::HashBytes(hash,&order,sizeof(order));
            hash=TradeRoutes::HashBytes(hash,&target.id,sizeof(target.id));
            hash=TradeRoutes::HashBytes(hash,reason,strlen(reason));
            hash=TradeRoutes::HashBytes(hash,&u->mCargoType,sizeof(u->mCargoType));
            hash=TradeRoutes::HashBytes(hash,&u->mCargoUnits,sizeof(u->mCargoUnits));
            if(hash==r->diagnostic_hash && ai_time<r->next_diagnostic)continue;
            r->diagnostic_hash=hash;r->next_diagnostic=ai_time+30000;
            if(harbor_log())ModAPI::Log("[Better Civ Harbor] t=%llu unit=%u civ=%u player=%d purpose=%d locomotion=%d order=%d partner=%u near-city=%u phase-last=%d cargo-type=%u cargo=%.2f request=%d flags=%u pending=%u async=%d aim=%u aim-distance=%.2f acceptable=%.2f,%.2f,%.2f/%.2f/%.2f last-native-message=\"%s\"",
                ai_time,u->mID,civ->mID,civ->mIsPlayerOwned,u->mPurpose,u->mLocomotion,order,target.id,
                harbor_city_id(u->mpNearestCity.get()),r->phase,u->mCargoType,u->mCargoUnits,u->field_250,unsigned(u->field_270),
                unsigned(u->field_1F4.size()),bool(u->field_240),aim.id,
                aim.Known()?CombatDiagnostics::Distance(u->mPosition,aim.position):-1,
                u->mAcceptableCenter.x,u->mAcceptableCenter.y,u->mAcceptableCenter.z,u->mAcceptableMinDist,u->mAcceptableMaxDist,reason);
        }
    }
    // Do not infer the boats' destination from a ribbon. Log the native pairs
    // and the city partner tables separately, with live IDs for comparison.
    if(!trade_route_native_ready)return;
    auto* planet=trade_planet();
    if(!planet || planet->mSeaTradeRoutes.size()>64 ||
        planet->mAirTradeRoutes.size()>64 || planet->mLandTradeRoutes.size()>64)return;
    uint64_t hash=14695981039346656037ull;
    const auto pair_hash=[&](const City* a,const City* b,unsigned kind) {
        const uint32_t ids[]={kind,harbor_city_id(a),harbor_city_id(b)};
        hash=TradeRoutes::HashBytes(hash,ids,sizeof(ids));
    };
    for(const auto& p:planet->mSeaTradeRoutes)pair_hash(p.first,p.second,1);
    for(const auto& p:planet->mAirTradeRoutes)pair_hash(p.first,p.second,2);
    for(const auto& p:planet->mLandTradeRoutes)pair_hash(p.city1,p.city2,0);
    for(const auto& civ:portrait_civilizations())if(civ && !civ->mbIsDestroyed)
        for(const auto& city:civ->mCities)if(city && !city->mbIsDestroyed)
            for(const auto& p:city->mTradePartners) {
                pair_hash(city.get(),Native::TradePartnerCity(p),3);
                const uint32_t id=trade_partner_id(p);
                hash=TradeRoutes::HashBytes(hash,&id,sizeof(id));
            }
    if(hash==harbor_route_hash)return;
    harbor_route_hash=hash;
    const auto log_pair=[&](const City* a,const City* b,const char* mode) {
        if(harbor_log())ModAPI::Log("[Better Civ Harbor] t=%llu event=ribbon mode=%s cities=%u,%u",ai_time,mode,harbor_city_id(a),harbor_city_id(b));
    };
    if(harbor_log())ModAPI::Log("[Better Civ Harbor] t=%llu event=route-snapshot land=%u sea=%u air=%u",ai_time,
        unsigned(planet->mLandTradeRoutes.size()),unsigned(planet->mSeaTradeRoutes.size()),unsigned(planet->mAirTradeRoutes.size()));
    for(const auto& p:planet->mSeaTradeRoutes)log_pair(p.first,p.second,"sea");
    for(const auto& p:planet->mAirTradeRoutes)log_pair(p.first,p.second,"air");
    for(const auto& p:planet->mLandTradeRoutes)log_pair(p.city1,p.city2,"land");
    for(const auto& civ:portrait_civilizations())if(civ && !civ->mbIsDestroyed)
        for(const auto& city:civ->mCities)if(city && !city->mbIsDestroyed)
            for(const auto& p:city->mTradePartners)if(harbor_log())
                ModAPI::Log("[Better Civ Harbor] t=%llu event=agreement city=%u owner=%u partner-city=%u partner-owner=%u",
                    ai_time,city->mID,city->mPoliticalID,harbor_city_id(Native::TradePartnerCity(p)),trade_partner_id(p));
}
