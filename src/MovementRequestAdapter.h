// Only the verified vehicle request thunk is redirected. All other locomotive
// classes keep their original request setter. Active, AI-only escape steps may
// defer an automatic refresh of the same blocked destination for at most 6 s.
using LocalRequestSetter=void(__thiscall*)(Simulator::cLocomotiveObject*,const Native::LocalPointRequest*);
static LocalRequestSetter original_local_request;
static bool placement_hold_request(Vehicle& unit,const Native::LocalPointRequest* request) {
    namespace P=Placement;
    if(settings.retry_blocked_paths!=2 || !recovery_native_ready || !ai_observer_ready ||
        !current_vehicle_order || !in_civilization())return false;
    // No allocation, reference retention or record creation in this hot path.
    P::Record* r=nullptr;
    const auto key=object_key(&unit);
    for(auto& item:placement_state.records)if(item.key==key && item.active && item.hold_native){r=&item;break;}
    if(!r)return false;
    auto s=r->imposed; s.position=unit.mPosition;
    const auto* order=current_vehicle_order(&unit);
    if(order){s.order=order->kind;s.target=reinterpret_cast<uintptr_t>(order->target);s.order_position=order->position;}
    const bool supported=request && request->kind==1 && !request->relative &&
        request->waypoints_begin==request->waypoints_end && P::Finite(request->position) &&
        std::isfinite(request->stop) && request->stop>=0 && request->stop<=100 &&
        std::isfinite(request->acceptable) && request->acceptable>=0 && request->acceptable<=100;
    if(!order || !P::SameOrder(r->imposed,s) || !placement_live(&unit) || !unit.mbInView ||
        (!r->separating && ((unit.mpTarget && unit.mpTarget.get()!=r->siege_aim) || unit.mTimeSinceLastWeaponFireMS<8000)) || !supported ||
        ai_time>=r->step_until || ai_time<r->last_seen || ai_time-r->last_seen>1500 ||
        !P::Finite(s.position) || !r->StillApproaching(s.position) || P::Distance(s.position,r->imposed.goal)<=2.5f) {
        r->Finish(s,ai_time);return false;
    }
    // Our own MoveTo must pass through unchanged. A genuinely different local
    // destination also wins immediately, even if its strategic order is equal.
    if(P::Distance(request->position,r->imposed.goal)<0.1f &&
        request->stop==r->imposed.goal_stop && request->acceptable==r->imposed.acceptable_stop)return false;
    if(P::Distance(request->position,r->original.goal)>4) {r->Finish(s,ai_time);return false;}
    r->original.goal=request->position;r->original.goal_stop=request->stop;
    r->original.acceptable_stop=request->acceptable;
    if(++r->blocked_resumes==1 && placement_log())
        ModAPI::Log("[Better Civ Placement] t=%llu event=defer-native-refresh unit=%u order=%d remaining-ms=%llu",
            ai_time,unit.mID,s.order,r->step_until-ai_time);
    return true;
}
static void __fastcall retained_local_request(Simulator::cLocomotiveObject* locomotive,int,
    const Native::LocalPointRequest* request) {
    // C9EA20 is exclusively cVehicle's locomotive subobject thunk.
    auto* unit=static_cast<Vehicle*>(locomotive);
    if(!placement_hold_request(*unit,request))original_local_request(locomotive,request);
}
