#pragma once
#include "GroundTradeRoad.h"
#include "TradeRouteNative.h"

namespace Simulator { class cPlanetModel; }

namespace BetterCiv::TradeRoutes {

struct Record {
    uint64_t city1 = 0, city2 = 0, topology = 0, source_hash = 0, path_hash = 0, seen = 0;
    uint64_t next_ground_check = 0;
    unsigned attempt = 0;
    bool pending = false;
    Path replacement;
};
struct State {
    Record records[64];
    uint64_t next_scan = 0, next_search = 0, next_ground_paint = 0;
    unsigned lines = 0;
    void ResetSession();
    Record& Get(uint64_t a, uint64_t b, uint64_t now);
};

// The supported executable returns 60-byte waypoints from a synchronous search.
struct NavPoint { Point position; unsigned char opaque[48]; };
static_assert(sizeof(NavPoint) == 60);
using NavPath = eastl::vector<NavPoint>;
using PathManager = void* (__cdecl*)();
using PathSearch = bool (__thiscall*)(void*, const void*, const Point&, const Point&, NavPath*, float, float);

struct Services {
    bool (*live_city)(Simulator::cCity*);
    bool (*terrain_ready)(Simulator::cPlanetModel*, float&);
    float (*height)(Simulator::cPlanetModel*, const Point&);
    Exclusions (*spice_nodes)();
    bool (*log_allowed)();
    PathManager path_manager;
    PathSearch path_search;
    const void* land_profile;
    uintptr_t image_base;
    bool paint_available;
};

Simulator::cPlanetModel* CurrentPlanet();
float TerrainHeight(Simulator::cPlanetModel* planet, const Point& position);
Exclusions SpiceNodes();
void Repair(State& state, Simulator::cPlanetModel& planet, uint64_t now, const Services& services);

} // namespace BetterCiv::TradeRoutes
