// Included after MovementRecoveryAdapter.h inside BetterCiv.
static Placement::State placement_state;
static bool placement_terrain_native_ready;
static bool placement_terrain_matches(const Terrain::ITerrain& terrain) {
    return placement_terrain_native_ready && Recovery::MatchingTerrain(terrain,recovery_image_base);
}
static bool (*placement_check_terrain)(const Terrain::ITerrain&) = placement_terrain_matches;
static Terrain::cTerrainMapSet* placement_read_terrain(Simulator::cPlanetModel* planet, Placement::SearchReport& report) {
    if (!planet) { report.reason="no-planet"; return nullptr; }
    if (!planet->mpSphere) { report.reason="no-sphere"; return nullptr; }
    if (!planet->mpTerrain) { report.reason="no-terrain"; return nullptr; }
    auto& terrain=*planet->mpTerrain;
    if (!placement_check_terrain(terrain)) { report.reason="unsupported-terrain-methods"; return nullptr; }
    // Verified against slot 30 at FA0D40 and the load callback at F96C70:
    // true means finished on this native implementation. Do not invert it.
    if (!terrain.IsLoading()) { report.reason="terrain-loading"; return nullptr; }
    auto* map=terrain.GetTerrainMapSet();
    if (!map) report.reason="no-terrain-map";
    return map;
}
static bool placement_live(const Vehicle* u) {
    return u && !u->mbIsDestroyed && !u->mbDead && u->mHealthPoints>0 &&
        !u->mbIsInvalid && !u->mbIsBeingEdited && !u->mbIsGhost && !u->mbFixed &&
        (u->mLocomotion==kVehicleLand || u->mLocomotion==kVehicleWater ||
         (u->mLocomotion==kVehicleAir && u->mPurpose==kVehicleEconomic));
}
static bool placement_contact(const Vehicle& unit, const Math::Vector3& goal) {
    for (const auto& civ:portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& other:civ->mVehicles) {
            if (!other || other.get()==&unit || other->mbIsDestroyed || other->mbDead ||
                other->mLocomotion!=unit.mLocomotion || !Recovery::Finite(other->mVelocity) ||
                Recovery::Distance(other->mVelocity,{})>=0.5f) continue;
            if (Placement::CorridorBlocked(unit.mPosition,goal,unit.mFootprintRadius,
                other->mPosition,other->mFootprintRadius)) return true;
        }
    }
    return false;
}
// Deep overlap is independent of movement and firing. Keep one anchor in each
// friendly cluster; units with larger stable IDs yield through normal MoveTo.
static uint64_t placement_overlap_peer(const Vehicle& unit,const Civilization& civ) {
    if(!Recovery::Finite(unit.mPosition) || !std::isfinite(unit.mFootprintRadius) ||
        unit.mFootprintRadius<=0 || unit.mFootprintRadius>8)return 0;
    for(const auto& other:civ.mVehicles) {
        if(!placement_live(other.get()) || other->mID>=unit.mID ||
            other->mLocomotion!=unit.mLocomotion || !Recovery::Finite(other->mPosition) ||
            !std::isfinite(other->mFootprintRadius) || other->mFootprintRadius<=0 || other->mFootprintRadius>8)continue;
        if(Recovery::Distance(unit.mPosition,other->mPosition)<0.35f*(unit.mFootprintRadius+other->mFootprintRadius))
            return object_key(other.get());
    }
    return 0;
}
static bool placement_watch_log() {
    return settings.ai_diagnostics && placement_state.watch_lines<2048 && (++placement_state.watch_lines!=0);
}
static bool placement_log() {
    return settings.ai_diagnostics && placement_state.lines < 4096 && (++placement_state.lines != 0);
}
static bool placement_find_space(Vehicle& unit, const Recovery::Sample& s,
    Math::Vector3& goal, Placement::SearchReport& report)
{
    namespace P = Placement;
    auto* planet = Simulator::cPlanetModel::Get();
    auto* map = placement_read_terrain(planet,report);
    if (!map) return false;
    const float radius = unit.mFootprintRadius;
    const bool water = unit.mLocomotion == kVehicleWater;
    const bool air = unit.mLocomotion == kVehicleAir;
    const float sea = map->GetWaterHeight();
    const float ground = planet->GetHeightAt(s.position);
    const float surface = water ? sea : ground;
    const float offset = P::Distance(s.position,{})-surface;
    report.reason="invalid-geometry";
    if (!std::isfinite(sea) || !std::isfinite(ground) || sea < 100 || ground < 100 ||
        !std::isfinite(offset) || (!air && std::fabs(offset) > 5) || radius <= 0 || radius > 8) return false;
    P::Space space;
    const float altitude=P::Distance(s.position,{});
    auto structure=[&](const Math::Vector3& p,float r) {
        if (!air) { space.Add(s.position,p,r,false); return; }
        auto up=p;
        if (!P::Normalize(up)) { space.complete=false; return; }
        // Conservatively avoid the whole city/obstacle column at the current
        // flight altitude; permit exiting an existing overlap, never deeper.
        space.Add(s.position,up*altitude,r,true);
    };
    const auto& civs = portrait_civilizations();
    for (const auto& civ : civs) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& other : civ->mVehicles) {
            if (!other || other.get() == &unit || other->mbIsDestroyed || other->mbDead ||
                other->mbIsGhost || other->mbIsInvalid ||
                (air ? other->mLocomotion!=kVehicleAir : other->mLocomotion==kVehicleAir)) continue;
            // Moving, hostile and trading vehicles also occupy physical space.
            space.Add(s.position,other->mPosition,other->mFootprintRadius,true);
        }
        for (const auto& city : civ->mCities) {
            if (!city || city->mbIsDestroyed) continue;
            const auto* walls = city->mpCityWalls.get();
            if (walls && !walls->mbIsDestroyed) {
                structure(walls->mPosition,walls->mOuterRadius);
                const auto* dock = walls->mpDock.get();
                if (dock && !dock->mbIsDestroyed && !dock->mbIsInvalid)
                    structure(dock->mPosition,dock->mFootprintRadius);
            } else structure(city->mPosition,city->mBuildingPlacementRadius+4);
        }
    }
    for (const auto& obstacle : Simulator::GetData<Simulator::cObstacle>().data) {
        if (obstacle && !obstacle->mbIsDestroyed)
            structure(obstacle->mPosition,obstacle->mObstacleBaseCollisionRadius);
    }
    for (const auto& r : placement_state.records) {
        if (r.key != s.key && r.reserved_until > ai_time)
            space.Add(s.position,r.reserved_goal,r.radius,false);
    }
    for(const auto& r:placement_state.records)if(r.key==s.key && r.attempts &&
        P::Distance(s.position,r.step_origin)<1 && P::Finite(r.imposed.goal))
        space.Add(s.position,r.imposed.goal,std::max(2.0f,radius),false);
    report.reason="no-clear-candidate";
    auto project = [&](const Math::Vector3& raw, Math::Vector3& out) {
        if (air) return P::ProjectAir(raw,radius,altitude,sea,
            [&](const Math::Vector3& p) { return planet->GetHeightAt(p); },out);
        return P::ProjectTerrain(raw,water,radius,sea,offset,
            [&](const Math::Vector3& p) { return planet->GetHeightAt(p); },out);
    };
    return P::FindSpace(s,radius,space,project,goal,report);
}
static bool (*placement_plan)(Vehicle&, const Recovery::Sample&, Math::Vector3&, Placement::SearchReport&) = placement_find_space;

// A parked ship can block the only dock approach. Yield only genuinely idle
// units (order 0), never an explicit move, trade, defense or attack order.
static bool placement_yield_idle(Vehicle& blocked,const Recovery::Sample& request) {
    if (blocked.mLocomotion==kVehicleAir) return false;
    for (const auto& civ:portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit:civ->mVehicles) {
            if (!placement_live(unit.get()) || unit.get()==&blocked || !unit->mbInView ||
                unit->mLocomotion!=blocked.mLocomotion || unit->mpTarget || !unit->mHostileUnits.empty() ||
                unit->mTimeSinceLastWeaponFireMS<30000 || unit->mDesiredSpeed>0.1f ||
                !Recovery::Finite(unit->mVelocity) || Recovery::Distance(unit->mVelocity,{})>=0.5f ||
                unit->field_250!=1 || unit->field_240 || !unit->field_1F4.empty() ||
                (unit->field_270&4) || !recovery_check_methods(*unit)) continue;
            const auto* order=current_vehicle_order(unit.get());
            if (!order || order->kind!=0 || !Placement::CorridorBlocked(request.position,request.goal,
                blocked.mFootprintRadius,unit->mPosition,unit->mFootprintRadius)) continue;
            auto* r=placement_state.Find(object_key(unit.get()),ai_time);
            if (!r || r->active || ai_time<r->cooldown || r->attempts>=2) continue;
            Recovery::Sample s;
            s.key=object_key(unit.get()); s.owner=civ->mID; s.order=0;
            s.target=reinterpret_cast<uintptr_t>(order->target); s.order_position=order->position;
            s.position=unit->mPosition; s.goal=unit->field_208;
            s.goal_stop=unit->field_214; s.acceptable_stop=unit->field_254;
            s.eligible=true; s.since_fire=float(unit->mTimeSinceLastWeaponFireMS);
            auto plan=s; plan.goal=request.goal;
            Math::Vector3 goal; Placement::SearchReport report;
            if (!placement_plan(*unit,plan,goal,report)) continue;
            r->Begin(s,goal,unit->mFootprintRadius,ai_time);
            if (placement_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=idle-yield unit=%u civ=%u player=%d for-unit=%u",
                ai_time,unit->mID,civ->mID,civ->mIsPlayerOwned,blocked.mID);
            recovery_apply(*unit,r->imposed); return true;
        }
    }
    return false;
}

static void recover_nearby_space(int deltaGameTime, bool civ_mode)
{
    namespace P = Placement;
    if (!recovery_native_ready || !ai_observer_ready || !civ_mode || deltaGameTime <= 0 ||
        !current_vehicle_order || ai_time < placement_state.next_sample) return;
    placement_state.next_sample = ai_time+500;
    struct Due { Vehicle* unit; P::Record* record; Recovery::Sample sample; bool player; bool separate; };
    Due due[Recovery::kMaxRecords]; unsigned count = 0, observed = 0, stationary = 0, aim_blocked = 0, firing_blocked = 0;
    unsigned traders=0, supported_traders=0, loops=0;
    for (const auto& civ : portrait_civilizations()) {
        if (!civ || civ->mbIsDestroyed) continue;
        for (const auto& unit : civ->mVehicles) {
            if (!placement_live(unit.get()) || !unit->mbInView) continue;
            ++observed;
            Recovery::Sample s;
            s.key=object_key(unit.get()); s.owner=civ->mID; s.position=unit->mPosition;
            s.goal=unit->field_208; s.desired=unit->mDesiredSpeed;
            s.speed=Recovery::Finite(unit->mVelocity) ? Recovery::Distance(unit->mVelocity,{}) : INFINITY;
            s.since_fire=float(unit->mTimeSinceLastWeaponFireMS);
            s.goal_stop=unit->field_214; s.acceptable_stop=unit->field_254;
            const auto* order=current_vehicle_order(unit.get());
            if (order) { s.order=order->kind; s.target=reinterpret_cast<uintptr_t>(order->target); s.order_position=order->position; }
            const bool trader=unit->mPurpose==kVehicleEconomic;
            const bool supported_order=order && ((s.order>=0 && s.order<=3) ||
                (trader && (s.order==5 || s.order==9)) || (!trader && s.order==7));
            s.eligible=supported_order &&
                (!trader || s.order!=5 || trade_detour_check(*unit)) &&
                (unit->mLocomotion!=kVehicleAir || s.order==5 || s.order==9) &&
                (trader || unit->mPurpose==kVehicleMilitary || unit->mPurpose==kVehicleCultural) &&
                unit->field_250==1 && !unit->field_240 && unit->field_1F4.empty() &&
                !(unit->field_270&4) && recovery_check_methods(*unit);
            s.crowded=s.eligible && placement_contact(*unit,s.goal);
            traders+=trader; supported_traders+=trader && s.eligible;
            const bool quiet=!unit->mpTarget;
            if (P::Stationary(s)) {
                ++stationary;
                if (!quiet) ++aim_blocked;
                if (s.since_fire<8000) ++firing_blocked;
            }
            auto* r=placement_state.Find(s.key,ai_time);
            if (!r) continue;
            if (r->followup_at && ai_time>=r->followup_at) {
                if (placement_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=follow-up unit=%u civ=%u displacement=%.3f actual-speed=%.2f same-order=%d step-request=%d crowded=%d aim-present=%d",
                    ai_time,unit->mID,civ->mID,P::Distance(s.position,r->step_origin),s.speed,
                    P::SameOrder(r->original,s),P::OwnsStep(r->imposed,s),s.crowded,!quiet);
                r->followup_at=0;
            }
            if (r->active) {
                r->last_seen=ai_time;
                const bool owned=P::OwnsStep(r->imposed,s);
                const bool safe=s.eligible && P::Finite(s.position) && (r->separating || (quiet && s.since_fire>=8000));
                const bool arrived=P::Finite(s.position) && P::Distance(s.position,r->imposed.goal)<=2.5f;
                const bool expired=ai_time>=r->step_until;
                if (!r->separating && owned && safe && arrived && s.order!=0 && r->attempts<2 &&
                    placement_contact(*unit,r->original.goal)) {
                    // One bounded second leg can go around the remaining queue.
                    // Preserve the original strategic request across both legs.
                    auto next=s; next.goal=r->original.goal;
                    Math::Vector3 goal; P::SearchReport report;
                    if (placement_plan(*unit,next,goal,report)) {
                        const auto original=r->original;
                        r->Begin(next,goal,unit->mFootprintRadius,ai_time); r->original=original;
                        r->hold_native=!civ->mIsPlayerOwned && (s.order==2 || s.order==3 || s.order==5 || s.order==7 || s.order==9);
                        if (placement_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=detour-leg unit=%u civ=%u attempt=%u",
                            ai_time,unit->mID,civ->mID,r->attempts);
                        recovery_apply(*unit,r->imposed); continue;
                    }
                }
                if (!owned || !safe || arrived || expired) {
                    const bool resume=owned && safe && s.order!=0 && (arrived || expired);
                    if (placement_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=step-end unit=%u civ=%u reason=%s resume=%d displacement=%.3f",
                        ai_time,unit->mID,civ->mID,!owned?"native-request-changed":!safe?"combat-or-unsupported":arrived?"arrived":"timeout",
                        resume,P::Distance(s.position,r->step_origin));
                    // A new command, native destination, aim or recent shot
                    // wins immediately. Never restore a stale strategic order.
                    const auto latest_original=r->original;
                    r->Finish(s,ai_time);
                    if (resume) {
                        recovery_apply(*unit,latest_original);
                        if(s.order==5 || s.order==9)r->trade_settle_until=ai_time+500;
                    }
                }
                continue;
            }
            const bool ready=r->Observe(s,quiet,ai_time);
            loops+=r->loop_detected;
            const auto overlap=placement_overlap_peer(*unit,*civ);
            const bool separate=r->Overlap(s.eligible?overlap:0,ai_time);
            if ((ready || separate) && count<Recovery::kMaxRecords) due[count++]={unit.get(),r,s,civ->mIsPlayerOwned,separate};
            // Focused diagnostics survive the general trajectory log's cap.
            // Include unsupported local requests so an exclusion is visible.
            if ((overlap || r->loop_detected || (trader && s.desired>1 && s.speed<0.5f)) && ai_time>=r->next_watch) {
                r->next_watch=ai_time+60000;
                if (placement_watch_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=watch unit=%u civ=%u purpose=%d locomotion=%d order=%d request=%d pending=%d flags=%u supported=%d crowded=%d overlap-peer=%llu loop=%d wait-ms=%llu path=%.2f net=%.2f turns=%.1f position=%.2f,%.2f,%.2f local-goal=%.2f,%.2f,%.2f speed=%.2f desired=%.2f aim=%d since-fire=%.0f",
                    ai_time,unit->mID,civ->mID,unit->mPurpose,unit->mLocomotion,s.order,unit->field_250,!unit->field_1F4.empty(),
                    unsigned(unit->field_270),s.eligible,s.crowded,overlap,r->loop_detected,ai_time-r->start,r->path,
                    P::Distance(s.position,r->anchor),r->turns*57.2957795f,
                    s.position.x,s.position.y,s.position.z,s.goal.x,s.goal.y,s.goal.z,s.speed,s.desired,!quiet,s.since_fire);
            }
        }
    }
    if (ai_time>=placement_state.next_summary) {
        placement_state.next_summary=ai_time+60000;
        if (placement_watch_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=summary visible=%u traders=%u supported-traders=%u stationary-contact=%u loops=%u active-aim=%u recent-fire=%u step-due=%u",
            ai_time,observed,traders,supported_traders,stationary,loops,aim_blocked,firing_blocked,count);
    }
    if (!count || ai_time<placement_state.next_step) return;
    const auto& c=due[placement_state.cursor++%count];
    Math::Vector3 goal; P::SearchReport report;
    placement_state.next_step=ai_time+2000;
    if (!c.separate && placement_yield_idle(*c.unit,c.sample)) { c.record->cooldown=ai_time+10000; return; }
    if (!placement_plan(*c.unit,c.sample,goal,report) || !P::Finite(goal)) {
        c.record->cooldown=ai_time+20000;
        if (placement_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=no-space unit=%u civ=%u reason=%s terrain-rejected=%u occupied-rejected=%u",
            ai_time,c.unit->mID,c.sample.owner,report.reason,report.terrain,report.occupied);
        return;
    }
    const auto waited=ai_time-c.record->start;
    const auto path=c.record->path;
    const auto reason=c.separate?"deep-overlap":c.record->loop_detected?"quiet-loop":"stationary-contact";
    c.record->Begin(c.sample,goal,c.unit->mFootprintRadius,ai_time);
    c.record->separating=c.separate;
    c.record->hold_native=c.separate || (!c.player && (c.sample.order==2 || c.sample.order==3 || c.sample.order==5 || c.sample.order==7 || c.sample.order==9));
    if (placement_log()) ModAPI::Log("[Better Civ Placement] t=%llu event=step unit=%u civ=%u player=%d locomotion=%d order=%d reason=%s attempt=%u wait-ms=%llu path=%.3f from=%.2f,%.2f,%.2f goal=%.2f,%.2f,%.2f old-goal=%.2f,%.2f,%.2f terrain-rejected=%u occupied-rejected=%u",
        ai_time,c.unit->mID,c.sample.owner,c.player,c.unit->mLocomotion,c.sample.order,reason,c.record->attempts,
        waited,path,c.sample.position.x,c.sample.position.y,c.sample.position.z,
        goal.x,goal.y,goal.z,c.sample.goal.x,c.sample.goal.y,c.sample.goal.z,report.terrain,report.occupied);
    recovery_apply(*c.unit,c.record->imposed);
}
static void recover_blocked_movement(int deltaGameTime, bool civ_mode) {
    if (settings.retry_blocked_paths==2) recover_nearby_space(deltaGameTime,civ_mode);
    else if (settings.retry_blocked_paths==1) retry_blocked_movement(deltaGameTime,civ_mode);
}
