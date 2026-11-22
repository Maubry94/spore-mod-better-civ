#include "ai/FleetPolicy.h"
#include "movement/MovementPlacement.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
using namespace BetterCiv;

static void verify_placement_policy()
{
    namespace P=Placement;
    Recovery::Sample s;
    s.key=1; s.owner=10; s.order=2; s.target=20;
    s.position={0,0,500}; s.goal={25,0,499}; s.goal_stop=1; s.acceptable_stop=2;
    s.eligible=s.crowded=true; s.desired=20; s.since_fire=12000;
    auto flat=[](const Math::Vector3& raw,Math::Vector3& out) { out=raw; return P::Normalize(out) ? (out=out*500,true) : false; };
    P::Space space;
    space.Add(s.position,{3,0,500},2,true);
    space.Add(s.position,{0,3,500},2,true);
    Math::Vector3 goal;
    P::SearchReport report;
    assert(P::FindSpace(s,2,space,flat,goal,report));
    assert(P::Distance(s.position,goal)>=4 && P::Distance(s.position,goal)<=28);
    for(unsigned i=0;i<space.count;++i) assert(P::ClearSegment(s.position,s.position,goal,2,space.blockers[i],true));
    for (unsigned i=0;i<space.count;++i) assert(P::Distance(goal,space.blockers[i].position)>=4.75f);
    // A reservation prevents a second unit from taking the same destination.
    const auto first=goal; space.Add(s.position,first,2,false);
    assert(P::FindSpace(s,2,space,flat,goal,report)); assert(P::Distance(first,goal)>=4.75f);
    // Eight coincident vehicles can reserve separate endpoints on open ground.
    P::Space stacked;for(unsigned i=0;i<8;++i)stacked.Add(s.position,s.position,2,true);
    Math::Vector3 separated[7];
    for(unsigned i=0;i<7;++i) {
        assert(P::FindSpace(s,2,stacked,flat,separated[i],report));
        for(unsigned j=0;j<i;++j)assert(P::Distance(separated[i],separated[j])>=4.75f);
        stacked.Add(s.position,separated[i],2,false);
    }
    // Starting overlapped is allowed only for vehicles, and the segment may
    // not deepen that overlap or cross an obstacle between its endpoints.
    assert(!P::ClearSegment(s.position,s.position,{10,0,500},1,{{5,0,500},0.1f,false},true));
    assert(!P::ClearSegment(s.position,s.position,{10,0,500},2,{{3,0,500},2,true},true));
    assert(P::ClearSegment(s.position,s.position,{-10,0,500},2,{{3,0,500},2,true},true));
    P::Space closed; closed.Add(s.position,s.position,50,true);
    assert(!P::FindSpace(s,2,closed,flat,goal,report));
    closed={}; closed.Add(s.position,s.position,2,false);
    assert(!P::FindSpace(s,2,closed,flat,goal,report));
    closed={}; closed.complete=false;
    assert(!P::FindSpace(s,2,closed,flat,goal,report));
    closed={};
    assert(!P::FindSpace(s,2,closed,[](const Math::Vector3&,Math::Vector3&){return false;},goal,report));
    // Terrain adapter vetoes the wrong medium along the path, not just the
    // endpoint. Here a shoreline excludes the positive-X half of the search.
    auto shore=[&](const Math::Vector3& raw,Math::Vector3& out) { return raw.x<=0 && flat(raw,out); };
    assert(P::FindSpace(s,2,closed,shore,goal,report) && goal.x<=0);
    auto cliff=[&](const Math::Vector3& raw,Math::Vector3& out) {
        if (!flat(raw,out)) return false;
        if (raw.x>1) out=out*(520.0f/500.0f);
        return true;
    };
    assert(P::FindSpace(s,2,closed,cliff,goal,report) && goal.x<=1);
    assert(!P::FindSpace(s,9,closed,flat,goal,report));
    assert(!P::FindSpace(s,NAN,closed,flat,goal,report));
    auto bad=s; bad.position={}; assert(!P::FindSpace(bad,2,closed,flat,goal,report));
    bad=s; bad.goal={INFINITY,0,0}; assert(!P::FindSpace(bad,2,closed,flat,goal,report));
    s.goal={0,0,500}; // Degenerate tangent direction still produces finite geometry.
    assert(P::FindSpace(s,2,closed,flat,goal,report)); s.goal={25,0,499};
    // Exercise the same terrain projection used by the production adapter.
    auto sea_floor=[](const Math::Vector3&){return 490.0f;};
    auto land=[](const Math::Vector3&){return 510.0f;};
    Math::Vector3 projected;
    assert(P::ProjectTerrain(s.position,true,2,500,1,sea_floor,projected) && std::fabs(P::Distance(projected,{})-501)<0.01f);
    assert(!P::ProjectTerrain(s.position,false,2,500,0,sea_floor,projected));
    assert(!P::ProjectTerrain(s.position,true,2,500,0,land,projected));
    assert(P::ProjectTerrain(s.position,false,2,500,0,land,projected) && std::fabs(P::Distance(projected,{})-510)<0.01f);
    assert(!P::ProjectTerrain(s.position,true,2,500,0,[](const Math::Vector3& p){return p.x>1?501.0f:490.0f;},projected));
    assert(!P::ProjectTerrain(s.position,false,2,500,0,[](const Math::Vector3& p){return p.y>1?520.0f:510.0f;},projected));
    assert(!P::ProjectTerrain(s.position,true,2,500,0,[](const Math::Vector3&){return NAN;},projected));
    assert(!P::ProjectTerrain(s.position,true,2,500,10,sea_floor,projected));

    P::Record record;
    for (unsigned t=0;t<8000;t+=500) {
        s.goal.x+=0.2f; s.since_fire=float(t);
        assert(!record.Observe(s,true,t));
    }
    s.since_fire=8000;
    assert(!record.Observe(s,false,8000)); // Active aim protects suppression/targeting.
    assert(record.Observe(s,true,8500));
    record.Begin(s,{-8,0,500},2,8500);
    assert(record.active && record.attempts==1 && record.reserved_until==16500);
    s.goal=record.imposed.goal; record.Finish(s,14500);
    for (unsigned t=15000;t<38500;t+=500) assert(!record.Observe(s,true,t));
    assert(record.Observe(s,true,38500)); record.Begin(s,{0,-8,500},2,38500);
    record.Finish(s,44500);
    for (unsigned t=45000;t<=80000;t+=500) assert(!record.Observe(s,true,t));
    assert(record.attempts==2);
    s.goal={30,0,499}; assert(!record.Observe(s,true,80500) && record.attempts==2);
    s.target=21; assert(!record.Observe(s,true,81000) && record.attempts==0);
    record={}; s.position={0,0,500};
    for (unsigned t=0;t<=30000;t+=500) {
        s.position.x=(t/500)%2?0.2f:0; assert(!record.Observe(s,true,t));
    }
    auto capacity=std::make_unique<P::State>();
    for (unsigned i=1;i<=Recovery::kMaxRecords;++i) { auto* r=capacity->Find(i,1); assert(r); r->last_seen=1; }
    assert(!capacity->Find(90000,100)); capacity->ResetSession(); assert(capacity->Find(90000,100));
    std::puts("Nearby-space policy: spherical geometry, overlap escape, swept clearance, reservations, terrain vetoes, immobility, combat gates and bounded episodes passed.");
}

static void verify_traffic_geometry()
{
    namespace P=Placement;
    // Previously missed: a blocker ahead but well outside physical contact.
    assert(P::CorridorBlocked({0,0,500},{30,0,500},2,{12,0,500},2));
    assert(!P::CorridorBlocked({0,0,500},{30,0,500},2,{12,20,500},2));
    assert(!P::CorridorBlocked({0,0,500},{30,0,500},2,{-12,0,500},2));
    Math::Vector3 out;
    auto flat=[](const Math::Vector3&){return 500.0f;};
    assert(P::ProjectAir({10,0,550},2,550,490,flat,out));
    assert(std::fabs(P::Distance(out,{})-550)<0.01f); // Preserve flight altitude.
    assert(!P::ProjectAir({0,0,550},2,550,549,flat,out));
    assert(!P::ProjectAir({0,0,550},2,550,490,[](const Math::Vector3&){return 546.0f;},out));
    assert(!P::ProjectAir({0,0,550},2,550,490,[](const Math::Vector3& p){return p.x>1?549.0f:500.0f;},out));
    std::puts("Traffic geometry: pre-contact corridor detection, fixed-altitude aircraft clearance and terrain/footprint vetoes passed.");
}

static void verify_fleet_policy()
{
    namespace F=Fleet;
    F::Facts f; f.idle=f.safe=f.no_port=f.ship=true;
    assert(F::Choose(f,true,false)==F::Reason::NoPort);
    f.player=true; assert(F::Choose(f,true,true)==F::Reason::None); f.player=false;
    f.no_port=false; assert(F::Choose(f,true,true)==F::Reason::None); f.no_port=true;
    f.safe=false; assert(F::Choose(f,true,true)==F::Reason::None); f.safe=true;
    f.ship=false; f.combat_land=f.suitable_port=f.sea_target=true;
    f.hostile_land=false; f.total=36; f.local_ground=15; f.local_cities=2;
    assert(F::Choose(f,false,true)==F::Reason::LandSurplus);
    f.local_ground=6; assert(F::Choose(f,false,true)==F::Reason::None); f.local_ground=15;
    f.hostile_land=true; assert(F::Choose(f,false,true)==F::Reason::None); f.hostile_land=false;
    f.sea_units=6; assert(F::Choose(f,false,true)==F::Reason::None); f.sea_units=0;
    f.suitable_port=false; assert(F::Choose(f,false,true)==F::Reason::None); f.suitable_port=true;
    f.sea_target=false; assert(F::Choose(f,false,true)==F::Reason::None); f.sea_target=true;
    f.total=31; assert(F::Choose(f,false,true)==F::Reason::None);
    F::Record r;
    for(unsigned t=0;t<120000;t+=2000)assert(!r.Observe(1,0,F::Reason::NoPort,t));
    assert(r.Observe(1,0,F::Reason::NoPort,120000));
    assert(!r.Observe(1,0,F::Reason::None,122000));
    assert(!r.Observe(1,0,F::Reason::NoPort,124000));
    r={};
    for(unsigned t=0;t<=180000;t+=2000)assert(r.Observe(1,9,F::Reason::LandSurplus,t)==(t==180000));
    assert(r.Observe(1,10,F::Reason::LandSurplus,182000)); // New overseas opportunity preserves local idle grace.
    assert(!r.Observe(2,10,F::Reason::LandSurplus,184000));
    assert(!r.Observe(2,10,F::Reason::LandSurplus,200000)); // Missing observations do not count.
    std::puts("Fleet policy: grace periods, player/combat protection, port return, target changes, garrison and sea supply passed.");
}

} // namespace

void VerifySimulationPolicies()
{
    verify_placement_policy();
    verify_traffic_geometry();
    verify_fleet_policy();
}
