#include "SimulationRuntime.h"
#include "NativePatches.h"
#include <Spore/Simulator/cCivilization.h>

namespace BetterCiv {
CurrentVehicleOrder current_vehicle_order;
SetVehicleOrder set_vehicle_order;
SimulationContextFunction relationship_manager;
RelationshipLevel relationship_level;
uint64_t ai_time;
bool ai_observer_ready;

void bind_simulation_runtime(unsigned char* base)
{
    current_vehicle_order = reinterpret_cast<CurrentVehicleOrder>(base + (0xca7360 - kReferenceBase));
    set_vehicle_order = reinterpret_cast<SetVehicleOrder>(base + (0xcac1a0 - kReferenceBase));
    relationship_manager = reinterpret_cast<SimulationContextFunction>(base + (0xb3d3c0 - kReferenceBase));
    relationship_level = reinterpret_cast<RelationshipLevel>(base + (0xd00bd0 - kReferenceBase));
}

uint32_t political_id(Simulator::cCivilization* entity)
{
    return entity->GetPoliticalID();
}

uint64_t object_key(const Simulator::cGameData* object)
{
    return object ? (uint64_t(object->mID) << 32) | reinterpret_cast<uintptr_t>(object) : 0;
}
} // namespace BetterCiv
