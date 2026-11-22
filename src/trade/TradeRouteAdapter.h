// Main simulation binding. The land and sea displays use the same guarded native
// interfaces and current-planet queries. Route computation lives in TradeRoutes.cpp.
using TradeNavPoint = TradeRoutes::NavPoint;
using TradeNavPath = TradeRoutes::NavPath;
using TradePathManager = TradeRoutes::PathManager;
using TradePathSearch = TradeRoutes::PathSearch;

static TradeRoutes::State trade_route_state;
static bool trade_route_native_ready;
static bool trade_ground_native_ready;
static TradePathManager trade_path_manager;
static TradePathSearch trade_path_search;
static const void* trade_land_profile;
static auto trade_planet = TradeRoutes::CurrentPlanet;
static auto trade_height = TradeRoutes::TerrainHeight;
static auto trade_exclusions = TradeRoutes::SpiceNodes;

static bool trade_terrain_ready(Simulator::cPlanetModel* planet, float& sea)
{
    Placement::SearchReport report;
    auto* terrain = placement_read_terrain(planet, report);
    if (!terrain) return false;
    sea = terrain->GetWaterHeight();
    return std::isfinite(sea) && sea >= 100;
}
static auto trade_ready = trade_terrain_ready;

static bool trade_log_allowed()
{
    return settings.ai_diagnostics && trade_route_state.lines < 512 && ++trade_route_state.lines;
}

static bool trade_live_city(City* wanted)
{
    // Route endpoints are borrowed. Match live membership before reading the city.
    if (!wanted) return false;
    for (const auto& civilization : portrait_civilizations()) {
        if (!civilization || civilization->mbIsDestroyed) continue;
        for (const auto& city : civilization->mCities) {
            if (city.get() == wanted) return !city->mbIsDestroyed;
        }
    }
    return false;
}

static void repair_trade_routes(int delta, bool civilization_mode)
{
    if (!trade_route_native_ready || !ai_observer_ready || !civilization_mode || delta <= 0 ||
        ai_time < 10000 || ai_time < trade_route_state.next_scan || !trade_path_manager ||
        !trade_path_search || !trade_land_profile) return;

    trade_route_state.next_scan = ai_time + 1000;
    auto* planet = trade_planet();
    if (!planet) return;
    const TradeRoutes::Services services{
        trade_live_city, trade_ready, trade_height, trade_exclusions, trade_log_allowed,
        trade_path_manager, trade_path_search, trade_land_profile, recovery_image_base,
        trade_ground_native_ready
    };
    TradeRoutes::Repair(trade_route_state, *planet, ai_time, services);
}
