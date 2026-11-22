#pragma once
#include "MovementPlacement.h"

namespace Simulator {
class cLocomotiveObject;
class cPlanetModel;
class cVehicle;
}
namespace Terrain {
class ITerrain;
class cTerrainMapSet;
}
namespace BetterCiv::Native {
struct LocalPointRequest;
}

namespace BetterCiv {

void bind_movement_native(const unsigned char* base);
void reset_movement_session();
void recover_blocked_movement(int deltaGameTime, bool civ_mode);

// The vehicle request hook may defer a repeated local destination during an escape step.
void __fastcall retained_local_request(Simulator::cLocomotiveObject*, int, const Native::LocalPointRequest*);

// Trade and movement share placement reservations, terrain validation and native movement calls.
extern Placement::State placement_state;
extern uintptr_t recovery_image_base;
extern bool recovery_native_ready;
extern bool placement_terrain_native_ready;
bool placement_live(const Simulator::cVehicle* unit);
Terrain::cTerrainMapSet* placement_read_terrain(Simulator::cPlanetModel* planet,
                                                Placement::SearchReport& report);
void recovery_apply(Simulator::cVehicle& unit, const Recovery::Sample& sample);
extern bool (*trade_detour_check)(Simulator::cVehicle&);
extern bool (*recovery_check_methods)(const Simulator::cVehicle&);

} // namespace BetterCiv
