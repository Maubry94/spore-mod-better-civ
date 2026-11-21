// Included after the placement/fleet/route adapters. No order, cargo, agreement
// or native phase is written. The original delivery update remains the owner.
static TradeTransit::State trade_transit_state;
static uintptr_t trade_update_continue;
static bool trade_detour_phase(Vehicle& unit) {
    if(!trade_transit_native_ready)return false;
    const auto key=object_key(&unit);
    for(const auto& r:trade_transit_state.records)if(r.key==key)
        return ai_time>=r.phase_seen && ai_time-r.phase_seen<=1500 && TradeTransit::Travelling(r.phase);
    return false;
}

static bool trade_transit_log() {
    return settings.ai_diagnostics && trade_transit_state.lines<4096 && ++trade_transit_state.lines;
}
// These two callers fetch an order before any proposal, agreement or cargo
// mutation. Return an empty borrowed view on proven port loss; the original
// behavior tree performs its own cancellation and cleanup. Never sell a player
// unit or invent an air route for a ship whose nation has no port.
static bool trade_ship_impossible(Vehicle& unit,const VehicleOrder& order) {
    if(unit.mLocomotion!=kVehicleWater || unit.mPurpose!=kVehicleEconomic ||
        (order.kind!=5 && order.kind!=9))return false;
    Civilization* owner=nullptr;City* destination=nullptr;
    for(const auto& civ:portrait_civilizations()) {
        if(!civ || civ->mbIsDestroyed)continue;
        for(const auto& u:civ->mVehicles)if(u.get()==&unit)owner=civ.get();
        for(const auto& city:civ->mCities)if(city.get()==order.target && city && !city->mbIsDestroyed)destination=city.get();
    }
    if(!owner || owner->mCities.empty() || !destination)return false; // Native owns invalid-object cases.
    if(!fleet_has_port(*owner) || !fleet_physical_port(destination))return true;
    // A port on another disconnected sea cannot service this ship either.
    auto* planet=fleet_planet();
    if(!planet || !planet->mContinentMap || planet->mContinentMapDirty || !fleet_continent)return false;
    const auto valid_sea=[&](int id){return id>=0 && unsigned(id)<planet->mContinentAboveWater.size() && !planet->mContinentAboveWater[id];};
    if(!Recovery::Finite(unit.mPosition))return false;
    const int sea=fleet_continent(planet,unit.mPosition),target_sea=fleet_continent(planet,destination->field_32C);
    if(!valid_sea(sea) || !valid_sea(target_sea))return false;
    if(sea!=target_sea)return true;
    for(const auto& c:owner->mCities)if(fleet_owned_city(c.get(),owner) && fleet_physical_port(c.get()) && fleet_continent(planet,c->field_32C)==sea)return false;
    return true;
}
member_detour(feasible_trade_order,Vehicle,const VehicleOrder*()) {
    const VehicleOrder* detoured() {
        const auto* order=current_vehicle_order(this);
        if(order && trade_transit_native_ready && fleet_native_ready && ai_observer_ready && in_civilization() &&
            (settings.assist_rival_trade || settings.retire_stranded) && trade_ship_impossible(*this,*order)) {
            static const VehicleOrder unavailable={};
            if(trade_transit_log())ModAPI::Log("[Better Civ Transit] t=%llu event=cancel-impossible-sea-trade unit=%u order=%d",ai_time,mID,order->kind);
            return &unavailable;
        }
        return order;
    }
};


// The agreement drives purchases and diplomacy. Substitute city pairs only
// while the planet renderer draws the arrows; simulation always sees its own lists.
using RoutePair=eastl::pair<City*,City*>;
using RouteList=eastl::vector<RoutePair>;
using DrawPlanetRoutes=uint32_t(__thiscall*)(Simulator::cPlanetModel*,uint32_t,uint32_t,uint32_t);
static DrawPlanetRoutes original_draw_routes;
static uint64_t sea_display_hash, next_sea_display_scan;
static bool drawing_sea_display;

static bool city_agrees_with(City& city,City& other) {
    for (const auto& p:city.mTradePartners) if (Native::TradePartnerCity(p)==&other) return true;
    return false;
}

static City* maritime_display_port(Simulator::cPlanetModel& planet,City& inland,City& destination) {
    if (fleet_physical_port(&inland) || !fleet_physical_port(&destination) ||
        !city_agrees_with(inland,destination) || !city_agrees_with(destination,inland)) return nullptr;
    auto* owner=inland.mpCivilization.get();
    auto* partner=destination.mpCivilization.get();
    if (!owner || !partner || owner==partner) return nullptr;
    const int sea=fleet_continent(&planet,destination.field_32C);
    if (sea<0 || unsigned(sea)>=planet.mContinentAboveWater.size() || planet.mContinentAboveWater[sea]) return nullptr;
    City* chosen=nullptr;
    for (const auto* nation:{owner,partner}) for (const auto& u:nation->mVehicles) {
        if (!u || u->mbDead || u->mbIsDestroyed || u->mPurpose!=kVehicleEconomic) continue;
        const auto* order=current_vehicle_order(u.get());
        if (!order || order->kind!=5 || !order->target) continue;
        const bool outbound=nation==owner && order->target==static_cast<Simulator::cGameData*>(&destination);
        const bool inbound=nation==partner && order->target==static_cast<Simulator::cGameData*>(&inland);
        if (u->mLocomotion==kVehicleAir && (outbound || inbound)) return nullptr;
        if (!outbound || u->mLocomotion!=kVehicleWater || !Recovery::Finite(u->mPosition) ||
            fleet_continent(&planet,u->mPosition)!=sea) continue;
        uint64_t loaded=0;
        for (const auto& rec:trade_transit_state.records) if (rec.key==object_key(u.get()) &&
            rec.target==reinterpret_cast<uintptr_t>(order->target) && rec.owner==order->political_id) {
            loaded=rec.loading_port; break;
        }
        City* port=nullptr;
        unsigned possibilities=0;
        for (const auto& city:owner->mCities) if (fleet_owned_city(city.get(),owner) &&
            fleet_physical_port(city.get()) && fleet_continent(&planet,city->field_32C)==sea) {
            ++possibilities;
            if (loaded==object_key(city.get())) { port=city.get(); possibilities=1; break; }
            port=city.get();
        }
        if (possibilities!=1) continue;
        if (chosen && port!=chosen) return nullptr;
        chosen=port;
    }
    return chosen;
}

static bool sea_display_ready(Simulator::cPlanetModel* planet) {
    return settings.assist_rival_trade && trade_route_native_ready && fleet_native_ready && ai_observer_ready &&
        current_vehicle_order && fleet_continent && planet && planet->mContinentMap && !planet->mContinentMapDirty &&
        planet->mAirTradeRoutes.size()<=64 && planet->mSeaTradeRoutes.size()<=64;
}
static uint64_t sea_display_pairs(Simulator::cPlanetModel& planet,RouteList* air,RouteList* water) {
    uint64_t hash=0;
    for (const auto& native:planet.mAirTradeRoutes) {
        City* home=native.first; City* destination=native.second;
        City* port=nullptr;
        if (trade_live_city(home) && trade_live_city(destination)) {
            if (fleet_physical_port(home)) std::swap(home,destination);
            port=maritime_display_port(planet,*home,*destination);
        }
        if (!port) { if (air) air->push_back(native); continue; }
        const uint64_t keys[]={object_key(home),object_key(port),object_key(destination)};
        hash=TradeRoutes::HashBytes(hash?hash:14695981039346656037ull,keys,sizeof(keys));
        if (water) {
            const auto same=[&](const RoutePair& pair) {return (pair.first==port && pair.second==destination) ||
                (pair.first==destination && pair.second==port);};
            if (std::none_of(water->begin(),water->end(),same)) water->push_back({port,destination});
        }
    }
    return hash;
}

// In the native sea drawing loop only, an empty path is replaced by a
// straight line. Check exactly the points that this loop would consume.
// Retain no borrowed city, path-manager, order or waypoint-vector pointers.
enum class SeaRibbonState { Empty, Check, Native, Plan, Rebuilt, Hidden };
struct SeaRibbonEntry {
    SeaRibbonState state=SeaRibbonState::Empty;
    Math::Vector3 from{},to{};
    uint64_t source_hash=0,seen=0;
    TradeRoutes::Path source,corrected;
};
struct SeaRibbonCache {
    std::array<SeaRibbonEntry,64> entries{};
    SeaRibbon::Planner planner;
    Simulator::cPlanetModel* world=nullptr;
    int active=-1;
    uint64_t next_step=0;
    void Reset() {
        for(auto& e:entries) e={};
        active=-1;world=nullptr;next_step=0;
    }
};
static SeaRibbonCache sea_ribbon_cache;
static Simulator::cPlanetModel* sea_ribbon_drawing_world;

static void set_sea_ribbon_output(const SeaRibbonEntry& entry,TradeNavPath& nodes) {
    if(entry.state==SeaRibbonState::Native || entry.state==SeaRibbonState::Empty) return;
    // One point makes both draw directions exit before allocating any mesh.
    // Zero points would deliberately trigger the native overland shortcut.
    nodes.clear();
    if(entry.state!=SeaRibbonState::Rebuilt) {
        TradeNavPoint only{};only.position=entry.from;nodes.push_back(only);return;
    }
    nodes.reserve(entry.corrected.size());
    for(const auto& p:entry.corrected) {
        TradeNavPoint node{};node.position=p;nodes.push_back(node);
    }
}

static bool __fastcall sea_ribbon_search(void* manager,int,const void* profile,const Math::Vector3& from,
    const Math::Vector3& to,TradeNavPath* nodes,float minimum,float maximum) {
    const bool found=trade_path_search(manager,profile,from,to,nodes,minimum,maximum);
    auto* planet=sea_ribbon_drawing_world;
    if(!planet || !nodes || nodes->size()>TradeRoutes::kMaxPoints ||
        !TradeRoutes::Finite(from) || !TradeRoutes::Finite(to)) return found;
    float sea=0;
    if(!trade_ready(planet,sea)) return found;
    auto& cache=sea_ribbon_cache;
    if(cache.world!=planet) {cache.Reset();cache.world=planet;}
    uint64_t hash=TradeRoutes::HashBytes(14695981039346656037ull,&sea,sizeof(sea));
    for(const auto& n:*nodes) hash=TradeRoutes::HashBytes(hash,&n.position,sizeof(n.position));
    SeaRibbonEntry* slot=nullptr;
    for(auto& e:cache.entries) {
        if(e.state!=SeaRibbonState::Empty && e.source_hash==hash &&
            TradeRoutes::Distance(e.from,from)<0.1f && TradeRoutes::Distance(e.to,to)<0.1f) {
            e.seen=ai_time;set_sea_ribbon_output(e,*nodes);return found;
        }
        if(e.state!=SeaRibbonState::Check && e.state!=SeaRibbonState::Plan &&
            (!slot || (slot->state!=SeaRibbonState::Empty &&
             (e.state==SeaRibbonState::Empty || e.seen<slot->seen)))) slot=&e;
    }
    if(!slot) return found;
    *slot={};slot->seen=ai_time;slot->from=from;slot->to=to;slot->source_hash=hash;
    if(nodes->size()==1) {slot->state=SeaRibbonState::Native;return found;} // already hidden
    slot->state=SeaRibbonState::Check;
    if(nodes->empty()) slot->source={from,to};
    else for(const auto& n:*nodes) slot->source.push_back(n.position);
    set_sea_ribbon_output(*slot,*nodes);
    return found;
}

static void advance_sea_ribbon(Simulator::cPlanetModel& world) {
    auto& cache=sea_ribbon_cache;
    if(cache.world!=&world || ai_time<cache.next_step) return;
    cache.next_step=ai_time+100;
    float sea=0;
    if(!trade_ready(&world,sea)) return;
    const auto height=[&](const Math::Vector3& position){return trade_height(&world,position);};
    // Validate newly published routes first, so long detours do not withhold
    // unrelated, already-correct ribbons. One terrain audit per update.
    for(auto& e:cache.entries) if(e.state==SeaRibbonState::Check) {
        const auto status=SeaRibbon::Inspect(e.source,sea,height);
        e.source.clear();
        e.state=status==SeaRibbon::Surface::Land?SeaRibbonState::Plan:SeaRibbonState::Native;
        if(e.state==SeaRibbonState::Native) world.mSeaTradeRoutesDirty=true;
        return;
    }
    bool begin=cache.active<0;
    if(begin) {
        for(int i=0;i<int(cache.entries.size());++i)
            if(cache.entries[i].state==SeaRibbonState::Plan) {cache.active=i;break;}
        if(cache.active<0) return;
    }
    auto& e=cache.entries[cache.active];
    const auto result=begin ? cache.planner.Begin(e.from,e.to,sea,height,e.corrected) :
                              cache.planner.Step(height,e.corrected);
    if(result==SeaRibbon::Result::Pending) return;
    // The original path was positively observed on land. If the search exhausted
    // its graph or a terrain query failed, retain its one-point display.
    e.state=result==SeaRibbon::Result::Found?SeaRibbonState::Rebuilt:SeaRibbonState::Hidden;
    world.mSeaTradeRoutesDirty=true;
    if(trade_log_allowed()) ModAPI::Log("[Better Civ Routes] t=%llu event=sea-ribbon-%s points=%u endpoints=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)",
        ai_time,e.state==SeaRibbonState::Rebuilt?"rebuilt":"hidden",unsigned(e.corrected.size()),
        e.from.x,e.from.y,e.from.z,e.to.x,e.to.y,e.to.z);
    cache.active=-1;
}

static void update_sea_display(int delta,bool civ_mode) {
    if (!civ_mode || delta<=0 || !settings.assist_rival_trade ||
        !original_draw_routes || !ai_observer_ready || !trade_route_native_ready) return;
    auto* planet=trade_planet();
    if (!planet) return;
    advance_sea_ribbon(*planet);
    if(ai_time<next_sea_display_scan) return;
    next_sea_display_scan=ai_time+1000;
    const uint64_t hash=sea_display_ready(planet)?sea_display_pairs(*planet,nullptr,nullptr):0;
    if (hash!=sea_display_hash) {
        sea_display_hash=hash;
        planet->mAirTradeRoutesDirty=planet->mSeaTradeRoutesDirty=true;
        if (trade_log_allowed()) ModAPI::Log("[Better Civ Routes] t=%llu event=sea-display active=%d",ai_time,hash!=0);
    }
}

static uint32_t __fastcall draw_transport_routes(Simulator::cPlanetModel* planet,int,
    uint32_t a,uint32_t b,uint32_t c) {
    // Set the context only for the duration of a live Civilization planet draw.
    struct DrawScope {
        Simulator::cPlanetModel* previous;
        ~DrawScope() {sea_ribbon_drawing_world=previous;}
    } scope{sea_ribbon_drawing_world};
    sea_ribbon_drawing_world=sea_display_ready(planet) && in_civilization()?planet:nullptr;
    if (!drawing_sea_display && sea_display_hash && sea_display_ready(planet) && in_civilization() &&
        (planet->mAirTradeRoutesDirty || planet->mSeaTradeRoutesDirty)) {
        RouteList air,water=planet->mSeaTradeRoutes;
        if (sea_display_pairs(*planet,&air,&water)) {
            drawing_sea_display=true;
            air.swap(planet->mAirTradeRoutes); water.swap(planet->mSeaTradeRoutes);
            const auto result=original_draw_routes(planet,a,b,c);
            water.swap(planet->mSeaTradeRoutes); air.swap(planet->mAirTradeRoutes);
            drawing_sea_display=false;
            return result;
        }
    }
    return original_draw_routes(planet,a,b,c);
}

using TradeArrival=bool(__thiscall*)(Simulator::cLocomotiveObject*);
static TradeArrival original_trade_arrival;
static bool __fastcall trade_proposal_arrival(Simulator::cLocomotiveObject* locomotive,int) {
    auto* unit=static_cast<Vehicle*>(locomotive);
    if(settings.retry_blocked_paths==2 && trade_transit_native_ready && ai_observer_ready && in_civilization()) {
        const auto* order=current_vehicle_order(unit);
        if(order && order->kind==9)for(const auto& r:placement_state.records)if(r.key==object_key(unit)) {
            auto s=r.imposed;s.order=order->kind;s.target=reinterpret_cast<uintptr_t>(order->target);
            s.goal=unit->field_208;s.goal_stop=unit->field_214;s.acceptable_stop=unit->field_254;
            if((r.active && Placement::OwnsStep(r.imposed,s)) ||
                (ai_time<r.trade_settle_until && Placement::OwnsStep(r.original,s)))return false;
            break;
        }
    }
    return original_trade_arrival(locomotive);
}

static bool trade_delivery_valid(Vehicle& unit,const VehicleOrder& order) {
    if(order.kind!=5 || !order.target)return false;
    Civilization* owner=nullptr;City* partner=nullptr;
    // Compare borrowed pointers against live collections before dereferencing.
    for(const auto& civ:portrait_civilizations()) {
        if(!civ || civ->mbIsDestroyed)continue;
        for(const auto& u:civ->mVehicles)if(u.get()==&unit)owner=civ.get();
        for(const auto& c:civ->mCities)
            if(c.get()==order.target && c && !c->mbIsDestroyed)partner=c.get();
    }
    if(!owner || !partner || owner->mCities.empty())return false;
    const auto own=political_id(owner);
    if(own==UINT32_MAX || order.political_id!=own || partner->mPoliticalID==own)return false;
    // This is the native update's agreement check, expressed through its live
    // partner table. No assumption that the nearest city is the home city.
    bool agreement=false;
    for(const auto& p:partner->mTradePartners)
        if(trade_partner_id(p)==own)agreement=true;
    if(!agreement)return false;
    if(unit.mLocomotion==kVehicleWater && (!fleet_has_port(*owner) || !fleet_physical_port(partner)))return false;
    return true;
}
static bool (*trade_delivery_check)(Vehicle&,const VehicleOrder&)=trade_delivery_valid;

static bool __cdecl trade_delivery_preflight(Vehicle* unit,const int* phase) {
    if(!trade_transit_native_ready || !ai_observer_ready || !unit || !phase || !in_civilization() ||
        !current_vehicle_order || unit->mbIsDestroyed || unit->mbDead)return false;
    const auto* order=current_vehicle_order(unit);
    if(!order || order->kind!=5 || unit->mPurpose!=kVehicleEconomic)return false;
    auto* r=trade_transit_state.Find(object_key(unit),ai_time);
    if(!r)return false;
    r->Observe(reinterpret_cast<uintptr_t>(order->target),order->political_id,unit->mPosition,ai_time);
    r->phase_seen=ai_time;
    if (settings.assist_rival_trade && unit->mLocomotion==kVehicleWater && *phase==2 &&
        unit->mCargoUnits>0 && r->phase!=2) {
        auto* port=unit->mpNearestCity.get();
        if (trade_live_city(port) && port->mPoliticalID==order->political_id && fleet_physical_port(port) &&
            Recovery::Distance(unit->mPosition,port->field_32C)<60) r->loading_port=object_key(port);
    }
    if(r->phase!=*phase || ai_time>=r->next_log) {
        r->phase=*phase;r->next_log=ai_time+30000;
        if(trade_transit_log())ModAPI::Log("[Better Civ Transit] t=%llu event=phase unit=%u phase=%d cargo-type=%u cargo=%.2f request=%d flags=%u attempts=%u",
            ai_time,unit->mID,*phase,unit->mCargoType,unit->mCargoUnits,unit->field_250,unsigned(unit->field_270),r->attempts);
    }
    if(settings.retry_blocked_paths!=2 || !recovery_native_ready || !placement_live(unit) ||
        !unit->mbInView || !TradeTransit::Travelling(*phase) || !trade_delivery_check(*unit,*order) ||
        unit->mpTarget || !unit->mHostileUnits.empty() || !recovery_check_methods(*unit)) {
        r->waiting=false;return false;
    }
    Recovery::Sample s;
    s.key=object_key(unit);s.order=order->kind;s.target=reinterpret_cast<uintptr_t>(order->target);
    s.position=unit->mPosition;s.goal=unit->field_208;s.goal_stop=unit->field_214;s.acceptable_stop=unit->field_254;
    // An escape endpoint is not a port. Suspend delivery only while we still
    // own this exact local request; explicit player commands win immediately.
    for(auto& escape:placement_state.records)if(escape.key==s.key) {
        s.owner=escape.imposed.owner;
        if(escape.active && Placement::OwnsStep(escape.imposed,s)) {
            if(Recovery::Finite(s.position) && Recovery::Distance(s.position,escape.imposed.goal)>2.5f &&
                ai_time<escape.step_until)return true;
            const auto original=escape.original;
            escape.Finish(s,ai_time);
            recovery_apply(*unit,original);
            escape.trade_settle_until=ai_time+500;
            if(trade_transit_log())ModAPI::Log("[Better Civ Transit] t=%llu event=resume-after-detour unit=%u phase=%d",ai_time,unit->mID,*phase);
            return true;
        }
        if(ai_time<escape.trade_settle_until && Placement::OwnsStep(escape.original,s))return true;
        break;
    }
    // Native bit 8 aborts delivery before testing the agreement/phase. Retry
    // only the existing finite local point, never resurrect an empty order.
    if(!(unit->field_270&8)) {r->waiting=false;return false;}
    if(r->waiting) {
        s.owner=r->retried.owner;
        return Placement::OwnsStep(r->retried,s) && ai_time<r->wait_until;
    }
    if(r->attempts>=2 || unit->field_250!=1 || unit->field_240 || !unit->field_1F4.empty() ||
        (unit->field_270&4) || !Recovery::Finite(s.position) || !Recovery::Finite(s.goal) ||
        !std::isfinite(s.goal_stop) || s.goal_stop<0 || s.goal_stop>100 ||
        !std::isfinite(s.acceptable_stop) || s.acceptable_stop<s.goal_stop || s.acceptable_stop>100 ||
        Recovery::Distance(s.position,s.goal)<=s.acceptable_stop+1)return false;
    ++r->attempts;r->waiting=true;r->wait_until=ai_time+1000;r->retried=s;
    recovery_apply(*unit,s);
    if(trade_transit_log())ModAPI::Log("[Better Civ Transit] t=%llu event=retry-path-failure unit=%u phase=%d attempt=%u",ai_time,unit->mID,*phase,r->attempts);
    return true;
}

// DCC7C0's five displaced bytes are: sub esp,0Ch; push ebx; push ebp.
// Preserve its entire caller stack and registers. The sixth stack argument is
// the borrowed phase record. Returning AL=1 defers this update only; no timers
// or phases are advanced. The normal path executes the displaced prologue.
__declspec(naked) static void guarded_trade_update() {
    __asm {
        pushfd
        pushad
        mov eax,[esp+40]
        mov edx,[esp+60]
        push edx
        push eax
        call trade_delivery_preflight
        add esp,8
        test al,al
        jnz deferred
        popad
        popfd
        sub esp,0Ch
        push ebx
        push ebp
        jmp dword ptr [trade_update_continue]
    deferred:
        popad
        popfd
        mov al,1
        ret
    }
}
