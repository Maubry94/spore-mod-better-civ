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
        if(Native::TradePartnerPoliticalId(p)==own)agreement=true;
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
