// Included inside BetterCiv, after the movement adapter. Runtime history only.
static Fleet::State fleet_state;
static bool fleet_native_ready;
using FleetSell=void(__thiscall*)(Vehicle*,bool);
using FleetWater=bool(__thiscall*)(City*,int);
using FleetContinent=int(__thiscall*)(Simulator::cPlanetModel*,const Math::Vector3&);
static FleetSell fleet_sell;
static FleetWater fleet_water;
static FleetContinent fleet_continent;
static Simulator::cPlanetModel* fleet_sdk_planet() {return Simulator::cPlanetModel::Get();}
static Simulator::cPlanetModel* (*fleet_planet)()=fleet_sdk_planet;
static bool fleet_live(const Vehicle* u) { return placement_live(u); }
static bool fleet_owned_city(const City* c,const Civilization* civ) {
    return c && !c->mbIsDestroyed && c->mpCivilization.get()==civ;
}
static bool fleet_physical_port(const City* city) {
    return city && Recovery::Finite(city->field_32C) && Recovery::Distance(city->field_32C,{})>10;
}
static bool fleet_has_port(Civilization& civ,int purpose=-1) {
    for (const auto& city:civ.mCities) if(fleet_owned_city(city.get(),&civ) &&
        (purpose<0 || city->mVehicleSpecialty==purpose) && fleet_physical_port(city.get())) return true;
    return false;
}
static bool fleet_hostile(Civilization& a,Civilization& b) {
    auto* manager=relationship_manager();
    if (!manager || &a==&b) return false;
    const auto own=political_id(&a),other=political_id(&b);
    if (own==UINT32_MAX || other==UINT32_MAX) return false;
    const int relation=relationship_level(manager,own,other,1);
    return relation>=0 && relation<=1;
}
static bool (*fleet_are_enemies)(Civilization&,Civilization&)=fleet_hostile;
static bool fleet_in_combat(const Vehicle& unit) {
    return unit.mpTarget || !unit.mHostileUnits.empty() || unit.mTimeSinceLastWeaponFireMS < 30000;
}

static bool fleet_enemy_in_reach(Civilization& owner, const Vehicle& unit) {
    const float own_range = unit.mpWeapon ? unit.mpWeapon->GetMaxRange() : 0.0f;
    for (const auto& other : portrait_civilizations()) {
        if (!other || other.get() == &owner || other->mbIsDestroyed || !fleet_are_enemies(owner, *other)) continue;
        for (const auto& enemy : other->mVehicles) {
            if (!enemy || enemy->mbDead || enemy->mbIsDestroyed || enemy->mbIsGhost || enemy->mHealthPoints <= 0) continue;
            const float enemy_range = enemy->mpWeapon ? enemy->mpWeapon->GetMaxRange() : 0.0f;
            // Keep a local buffer even when a unit has no weapon data yet.
            const float reach = (std::max)(85.0f, (std::max)(own_range, enemy_range));
            if (Recovery::Distance(unit.mPosition, enemy->mPosition) <= reach) return true;
        }
    }
    return false;
}

// Only the AI's land-vehicle retirement call. The native caller returns
// immediately afterwards, so vetoing the transaction leaves ownership intact.
static void __fastcall guarded_ai_land_sale(Vehicle* unit, int, bool refund) {
    if (unit && refund && settings.rebalance_land && ai_observer_ready && in_civilization()) {
        for (const auto& civ : portrait_civilizations()) {
            if (!civ || civ->mbIsDestroyed || civ->mIsPlayerOwned || civ->mCities.empty()) continue;
            auto owned = std::find_if(civ->mVehicles.begin(), civ->mVehicles.end(),
                [unit](const cVehiclePtr& v) { return v.get() == unit; });
            if (owned == civ->mVehicles.end()) continue;
            const bool blocked = fleet_in_combat(*unit) || fleet_enemy_in_reach(*civ, *unit);
            auto* nation = settings.ai_diagnostics ? fleet_state.FindNation(object_key(civ.get())) : nullptr;
            if (nation && fleet_state.lines < 4096 && (!blocked || ai_time >= nation->next_veto_log)) {
                if (blocked) nation->next_veto_log = ai_time + 5000;
                ++fleet_state.lines;
                ModAPI::Log("[Better Civ Fleet] t=%llu event=%s civ=%u unit=%u source=native-ai",
                    ai_time, blocked ? "sale-veto" : "sale-allowed", civ->mID, unit->mID);
            }
            if (blocked) return;
            break;
        }
    }
    fleet_sell(unit, refund);
}
// Find an enemy port on the ship's own connected sea. Geography is checked
// without requiring a builder city, since the nation just lost its ports.
static bool fleet_reclaim_target(Civilization& civ,Vehicle& unit,City*& target) {
    target=nullptr;
    auto* planet=fleet_planet();
    if(!planet || !planet->mContinentMap || planet->mContinentMapDirty || !Recovery::Finite(unit.mPosition))return false;
    const int sea=fleet_continent(planet,unit.mPosition);
    if(sea<0 || unsigned(sea)>=planet->mContinentAboveWater.size() || planet->mContinentAboveWater[sea])return false;
    float closest=INFINITY;
    for(const auto& other:portrait_civilizations()) {
        if(!other || other->mbIsDestroyed || other.get()==&civ || !fleet_are_enemies(civ,*other))continue;
        for(const auto& city:other->mCities)if(fleet_owned_city(city.get(),other.get()) && fleet_physical_port(city.get()) &&
            fleet_continent(planet,city->field_32C)==sea) {
            const float distance=Recovery::Distance(unit.mPosition,city->field_32C);
            if(distance<closest){target=city.get();closest=distance;}
        }
    }
    return true;
}
// A merchant only competes for reinforcements when the native shared limit
// is near, the military specialty still has room, and its planner has a target.
static bool mixed_economy(Civilization& civ) {
    bool economic=false, combat=false;
    for (const auto& city:civ.mCities) if (fleet_owned_city(city.get(),&civ)) {
        economic |= city->mVehicleSpecialty==kVehicleEconomic;
        combat |= city->mVehicleSpecialty==kVehicleMilitary || city->mVehicleSpecialty==kVehicleCultural;
    }
    return economic && combat;
}
static City* fleet_reinforcement(Civilization& civ, int& traders) {
    traders=0;
    if (!fleet_native_ready || !ai_observer_ready || civ.mIsPlayerOwned || civ.mbIsDestroyed ||
        civ.mVehicles.size()<kNativeFleetLimit-1 || !fleet_continent || !fleet_water) return nullptr;
    int soldiers=0, preachers=0;
    for (const auto& unit:civ.mVehicles) if (unit && !unit->mbDead && !unit->mbIsDestroyed) {
        traders+=unit->mPurpose==kVehicleEconomic;
        soldiers+=unit->mPurpose==kVehicleMilitary;
        preachers+=unit->mPurpose==kVehicleCultural;
    }
    const bool mixed=mixed_economy(civ);
    if (traders<=(mixed ? 5 : int(civ.mVehicles.size()/2))) return nullptr;
    auto* planet=fleet_planet();
    if (!planet || !planet->mContinentMap || planet->mContinentMapDirty) return nullptr;
    const auto connected=[&](const Math::Vector3& a,const Math::Vector3& b,bool land) {
        if (!Recovery::Finite(a) || !Recovery::Finite(b)) return false;
        const int id=fleet_continent(planet,a);
        return id>=0 && unsigned(id)<planet->mContinentAboveWater.size() &&
            planet->mContinentAboveWater[id]==land && fleet_continent(planet,b)==id;
    };
    for (const auto& city:civ.mCities) {
        if (!fleet_owned_city(city.get(),&civ)) continue;
        const auto specialty=city->mVehicleSpecialty;
        City* target=nullptr;
        if (specialty==kVehicleMilitary) target=civ.mMilitaryExpansionCity.get();
        else if (mixed && specialty==kVehicleCultural) target=civ.mReligiousExpansionCity.get();
        if (!target) continue;
        bool hostile=false;
        for (const auto& other:portrait_civilizations()) if (other && !other->mbIsDestroyed && other.get()!=&civ)
            for (const auto& c:other->mCities) if (c.get()==target && fleet_owned_city(target,other.get()))
                hostile=fleet_are_enemies(civ,*other);
        if (!hostile || (specialty==kVehicleMilitary?soldiers:preachers)>=specialty_capacity(civ,specialty)) continue;
        if ((connected(city->mPosition,target->mPosition,true) && fleet_water(city.get(),kVehicleLand)) ||
            (fleet_physical_port(city.get()) && fleet_physical_port(target) && fleet_water(city.get(),kVehicleWater) &&
             connected(city->field_32C,target->field_32C,false)) || fleet_water(city.get(),kVehicleAir)) return target;
    }
    return nullptr;
}
int merchant_capacity(Civilization& civ,int capacity) {
    if (civ.mIsPlayerOwned || civ.mbIsDestroyed || !settings.rebalance_land) return capacity;
    if (mixed_economy(civ)) return (std::min)(capacity,5);
    int traders=0;
    City* target=fleet_reinforcement(civ,traders);
    if (!target || capacity<=traders) return capacity;
    auto* n=settings.ai_diagnostics?fleet_state.FindNation(object_key(&civ)):nullptr;
    if (n && ai_time>=n->next_reserve_log && fleet_state.lines<4096) {
        ++fleet_state.lines; n->next_reserve_log=ai_time+30000;
        ModAPI::Log("[Better Civ Fleet] t=%llu event=reserve-reinforcement civ=%u total=%u traders=%d target=%u",
            ai_time,civ.mID,unsigned(civ.mVehicles.size()),traders,target->mID);
    }
    return traders;
}
static int fleet_land_region(Simulator::cPlanetModel& planet, const Math::Vector3& position) {
    if (!Recovery::Finite(position)) return -1;
    const int region = fleet_continent(&planet, position);
    return region >= 0 && unsigned(region) < planet.mContinentAboveWater.size() &&
        planet.mContinentAboveWater[region] ? region : -1;
}
// Only classify a land merchant when every city can be assigned to known land.
static bool fleet_isolated_merchant(Civilization& owner, Vehicle& unit, int& region) {
    auto* planet=fleet_planet();
    if (!planet || !planet->mContinentMap || planet->mContinentMapDirty || !fleet_continent) return false;
    region=fleet_land_region(*planet,unit.mPosition);
    if (region<0) return false;
    bool home=false, partner=false;
    for (const auto& civ:portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& city:civ->mCities) if (fleet_owned_city(city.get(),civ.get())) {
            const int city_region=fleet_land_region(*planet,city->mPosition);
            if (city_region<0) return false;
            if (city_region!=region) continue;
            if (civ.get()==&owner) home=true;
            else if (!fleet_are_enemies(owner,*civ)) partner=true;
        }
    }
    return !home || !partner;
}

struct FleetAudit { Fleet::Facts facts; Fleet::Veto veto=Fleet::Veto::NotCandidate; int region=-1; };
static Fleet::Reason fleet_reason(Civilization& civ,Vehicle& u,City*& target,FleetAudit* audit=nullptr) {
    namespace F=Fleet;
    F::Facts f; target=nullptr;
    const auto finish=[&](F::Veto why,F::Reason result=F::Reason::None) {
        if(audit){audit->facts=f;audit->veto=why;}return result;
    };
    f.player=civ.mIsPlayerOwned; f.alive=!civ.mbIsDestroyed && !civ.mCities.empty() && fleet_live(&u);
    if (!f.alive || f.player)return finish(F::Veto::PlayerOrInvalid);
    const auto* order=current_vehicle_order(&u);
    f.safe=!fleet_in_combat(u);
    f.idle=order && (order->kind==0 || (order->kind==7 && u.mPurpose!=kVehicleEconomic)) &&
        Recovery::Finite(u.mVelocity) && Recovery::Distance(u.mVelocity,{})<0.5f &&
        std::isfinite(u.mDesiredSpeed) && u.mDesiredSpeed<=0.1f;
    f.ship=u.mLocomotion==kVehicleWater && (u.mPurpose==kVehicleMilitary ||
        u.mPurpose==kVehicleCultural || u.mPurpose==kVehicleEconomic);
    f.combat_land=u.mLocomotion==kVehicleLand && (u.mPurpose==kVehicleMilitary || u.mPurpose==kVehicleCultural);
    if (!f.safe)return finish(F::Veto::Busy);
    // A local navigation request can persist after trade has become impossible.
    // A strategic order, combat or a restored trading opportunity still wins.
    if (u.mPurpose==kVehicleEconomic && order && order->kind==0 && !order->target) {
        if (settings.retire_stranded && f.ship && !fleet_has_port(civ) && !fleet_enemy_in_reach(civ,u))
            return finish(F::Veto::Eligible,F::Reason::NoPort);
        int region=-1;
        if (settings.rebalance_land && u.mLocomotion==kVehicleLand &&
            fleet_isolated_merchant(civ,u,region) && !fleet_enemy_in_reach(civ,u)) {
            if (audit) audit->region=region;
            return finish(F::Veto::Eligible,F::Reason::StrandedMerchant);
        }
    }
    if (!f.idle)return finish(F::Veto::Moving);
    if (settings.retire_stranded && f.ship) {
        // Losing ports alone does not make an assigned combat ship useless.
        // Protect every explicit mission, including a quiet defense order.
        // Idle defense remains eligible only for the separate land-surplus rule.
        if(order->kind!=0)return finish(F::Veto::Moving);
        // A remaining port of any specialty vetoes portless retirement.
        f.no_port=!fleet_has_port(civ);
        if(f.no_port && u.mPurpose!=kVehicleEconomic) {
            if(!fleet_reclaim_target(civ,u,target))return finish(F::Veto::MapUnavailable);
            if(target)return finish(F::Veto::Eligible,F::Reason::ReclaimPort);
        }
        if (f.no_port && fleet_enemy_in_reach(civ, u)) return finish(F::Veto::Busy);
        if (f.no_port) return finish(F::Veto::Eligible,F::Choose(f,true,false));
    }
    if (settings.rebalance_land && u.mPurpose==kVehicleEconomic && civ.mVehicles.size()>=kNativeFleetLimit &&
        order->kind==0 && !order->target && u.mCargoUnits==0 && u.field_1F4.empty() && !u.field_240) {
        int traders=0;
        target=fleet_reinforcement(civ,traders);
        if (target && !fleet_enemy_in_reach(civ,u)) return finish(F::Veto::Eligible,F::Reason::MerchantSurplus);
    }
    if (!settings.rebalance_land || !f.combat_land)return finish(F::Veto::NotCandidate);
    f.total=unsigned(civ.mVehicles.size());
    if(f.total<32)return finish(F::Veto::LowPressure);
    auto* planet=fleet_planet();
    if (!planet || !planet->mContinentMap || planet->mContinentMapDirty || !Recovery::Finite(u.mPosition))return finish(F::Veto::MapUnavailable);
    const int continent=fleet_continent(planet,u.mPosition);
    if (continent<0 || unsigned(continent)>=planet->mContinentAboveWater.size() ||
        !planet->mContinentAboveWater[continent])return finish(F::Veto::NotLand);
    if(audit)audit->region=continent;
    f.total=unsigned(civ.mVehicles.size()); f.hostile_land=false;
    bool air_builder=false;
    unsigned aircraft=0;
    for(const auto& own:civ.mCities) if(fleet_owned_city(own.get(),&civ)) {
        const int home_region=fleet_land_region(*planet,own->mPosition);
        if (home_region<0) return finish(F::Veto::MapUnavailable);
        if (home_region==continent) ++f.local_cities;
        if(own->mVehicleSpecialty==u.mPurpose && fleet_water(own.get(),kVehicleAir)) air_builder=true;
        if(own->mVehicleSpecialty==u.mPurpose && fleet_water(own.get(),kVehicleWater))f.suitable_port=true;
    }
    for(const auto& v:civ.mVehicles) if(fleet_live(v.get())) {
        if(v->mLocomotion==kVehicleLand && v->mPurpose!=kVehicleEconomic &&
            fleet_continent(planet,v->mPosition)==continent)++f.local_ground;
        if(v->mLocomotion==kVehicleWater && v->mPurpose==u.mPurpose)++f.sea_units;
    }
    if (air_builder) for (const auto& v:civ.mVehicles) {
        if (v && !v->mbDead && !v->mbIsDestroyed && v->mHealthPoints>0 &&
            v->mLocomotion==kVehicleAir && v->mPurpose==u.mPurpose) ++aircraft;
    }
    City* air_target=nullptr;
    // Prefer the current offensive target, but a different reachable hostile
    // port also establishes a need for ships. This does not issue an attack,
    // change the native target, declare war or create a trade route.
    auto* preferred=(u.mPurpose==kVehicleMilitary?civ.mMilitaryExpansionCity:civ.mReligiousExpansionCity).get();
    for(const auto& other:portrait_civilizations()) {
        if(!other || other->mbIsDestroyed || other.get()==&civ || !fleet_are_enemies(civ,*other))continue;
        for(const auto& city:other->mCities)if(fleet_owned_city(city.get(),other.get())) {
            const int land=fleet_land_region(*planet,city->mPosition);
            if (land<0) return finish(F::Veto::MapUnavailable);
            if(land==continent)f.hostile_land=true;
            if (air_builder && land!=continent && (!air_target || city.get()==preferred))
                air_target=city.get();
            if(land==continent || !fleet_physical_port(city.get()))continue;
            // fleet_water is a production-unlock test: the defender's tech
            // level must not prevent attacking its geographically valid port.
            const int sea=fleet_continent(planet,city->field_32C);
            if(sea<0 || unsigned(sea)>=planet->mContinentAboveWater.size() || planet->mContinentAboveWater[sea])continue;
            for(const auto& own:civ.mCities)if(fleet_owned_city(own.get(),&civ) &&
                own->mVehicleSpecialty==u.mPurpose && fleet_physical_port(own.get()) &&
                fleet_water(own.get(),kVehicleWater) && fleet_continent(planet,own->field_32C)==sea) {
                f.sea_target=true;
                if(!target || city.get()==preferred)target=city.get();
            }
        }
    }
    if(f.hostile_land)return finish(F::Veto::HostileLand);
    if (air_target && aircraft<6 && (!f.sea_target || f.sea_units>=6)) {
        target=air_target;
        f.suitable_port=true; f.sea_target=true; f.sea_units=aircraft;
    }
    if(!f.suitable_port)return finish(F::Veto::NoBuilderPort);
    if(!f.sea_target)return finish(F::Veto::NoEnemyPort);
    if(f.sea_units>=6)return finish(F::Veto::EnoughShips);
    if(f.local_ground<=F::GroundReserve(f.local_cities))return finish(F::Veto::Garrison);
    if (fleet_enemy_in_reach(civ, u)) return finish(F::Veto::Busy);
    return finish(F::Veto::Eligible,F::Choose(f,false,true));
}
static void manage_unused_fleets(int delta,bool civ_mode) {
    if(!(settings.retire_stranded || settings.rebalance_land) || !fleet_native_ready || !ai_observer_ready ||
        !civ_mode || delta<=0 || !current_vehicle_order || !fleet_sell || !fleet_water || !fleet_continent ||
        ai_time<fleet_state.next_sample) return;
    fleet_state.next_sample=ai_time+2000;
    Vehicle* chosen=nullptr; Civilization* owner=nullptr; Fleet::Reason reason=Fleet::Reason::None;
    for(const auto& civ:portrait_civilizations()) {
        if(!civ || civ->mbIsDestroyed || civ->mIsPlayerOwned || civ->mCities.empty())continue;
        auto* nation=fleet_state.FindNation(object_key(civ.get()));
        if(!nation)continue;
        const bool owns_port=fleet_has_port(*civ);
        unsigned counts[unsigned(Fleet::Veto::Count)]={};uint64_t oldest=0;
        FleetAudit example;unsigned example_id=0,example_target=0;
        for(const auto& unit:civ->mVehicles) {
            if(!unit)continue;
            City* target=nullptr;FleetAudit audit;
            const auto why=fleet_reason(*civ,*unit,target,&audit);++counts[unsigned(audit.veto)];
            if(audit.facts.combat_land && audit.facts.idle && (!example_id || audit.veto==Fleet::Veto::Eligible)) {
                example=audit;example_id=unit->mID;example_target=target?target->mID:0;
            }
            if(!fleet_live(unit.get()))continue;
            auto* r=fleet_state.Find(object_key(unit.get()),ai_time); if(!r)continue;
            if(owns_port){r->reclaim_attempts=0;r->next_reclaim=0;}
            if(r->Observe(object_key(civ.get()),object_key(target),why,ai_time,audit.region) &&
                ai_time>=nation->next_sale && !chosen &&
                ((why!=Fleet::Reason::StrandedMerchant && why!=Fleet::Reason::NoPort) ||
                 (Recovery::Finite(unit->mVelocity) && Recovery::Distance(unit->mVelocity,{})<0.5f)) &&
                (why!=Fleet::Reason::ReclaimPort || (r->reclaim_attempts<2 && ai_time>=r->next_reclaim))) {chosen=unit.get();owner=civ.get();reason=why;}
            if(why!=Fleet::Reason::None)oldest=std::max(oldest,ai_time-r->since);
        }
        if(settings.ai_diagnostics && ai_time>=nation->next_review && fleet_state.lines<4096) {
            nation->next_review=ai_time+30000;++fleet_state.lines;
            ModAPI::Log("[Better Civ Fleet] t=%llu event=review civ=%u vehicles=%u eligible=%u invalid=%u combat=%u assigned=%u other=%u pressure=%u map=%u land=%u hostile-land=%u builder-port=%u enemy-port=%u ships=%u garrison=%u grace-ms=%llu sample-unit=%u veto=%s region=%d local-ground=%u local-cities=%u same-purpose-ships=%u opportunity=%u",
                ai_time,civ->mID,unsigned(civ->mVehicles.size()),counts[0],counts[1],counts[2],counts[3],counts[4],counts[5],counts[6],counts[7],counts[8],counts[9],counts[10],counts[11],counts[12],oldest,
                example_id,Fleet::VetoName(example.veto),example.region,example.facts.local_ground,example.facts.local_cities,example.facts.sea_units,example_target);
        }
    }
    if(!chosen || !owner)return;
    City* target=nullptr;
    if(fleet_reason(*owner,*chosen,target)!=reason)return;
    auto* nation=fleet_state.FindNation(object_key(owner));
    if(reason==Fleet::Reason::ReclaimPort) {
        auto* r=fleet_state.Find(object_key(chosen),ai_time);
        if(!target || !r || !set_vehicle_order || r->reclaim_attempts>=2 || ai_time<r->next_reclaim)return;
        ++r->reclaim_attempts;r->next_reclaim=ai_time+120000;
        nation->next_sale=ai_time+2000;
        if(settings.ai_diagnostics && fleet_state.lines++<4096)
            ModAPI::Log("[Better Civ Fleet] t=%llu event=reclaim-port civ=%u unit=%u target=%u purpose=%d attempt=%u",
                ai_time,owner->mID,chosen->mID,target->mID,chosen->mPurpose,r->reclaim_attempts);
        // Only an idle AI combat ship. Use the normal attack/conversion order;
        // the native navigator and combat still decide how the mission proceeds.
        set_vehicle_order(chosen,target,chosen->mPurpose==kVehicleMilitary?2:3,0);
        return;
    }
    nation->next_sale=ai_time+30000;
    if(settings.ai_diagnostics && fleet_state.lines++<4096)
        ModAPI::Log("[Better Civ Fleet] t=%llu event=sell civ=%u unit=%u reason=%s purpose=%d locomotion=%d vehicles-before=%u",
            ai_time,owner->mID,chosen->mID,reason==Fleet::Reason::NoPort?"no-owned-port":reason==Fleet::Reason::MerchantSurplus?"surplus-merchant":reason==Fleet::Reason::StrandedMerchant?"isolated-land-merchant":"surplus-land",
            chosen->mPurpose,chosen->mLocomotion,unsigned(owner->mVehicles.size()));
    // All enumeration has ended. The native transaction refunds according to
    // price/health and removes the unit from its owning nation's vehicle list.
    cVehiclePtr retained=chosen;
    fleet_sell(chosen,true);
}
