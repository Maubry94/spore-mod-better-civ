#pragma once
#include <cstdint>

namespace Simulator {
class cCivilization;
class cGameData;
class cVehicle;
}
namespace BetterCiv::Native { struct VehicleOrder; }

namespace BetterCiv {
using Vehicle = Simulator::cVehicle;
using CurrentVehicleOrder = const Native::VehicleOrder* (__thiscall*)(Vehicle*);
using SetVehicleOrder = void (__thiscall*)(Vehicle*, void*, int, int);
using SimulationContextFunction = void* (__cdecl*)();
using RelationshipLevel = int (__thiscall*)(void*, uint32_t, uint32_t, int);

void bind_simulation_runtime(unsigned char* base);
extern CurrentVehicleOrder current_vehicle_order;
extern SetVehicleOrder set_vehicle_order;
extern SimulationContextFunction relationship_manager;
extern RelationshipLevel relationship_level;

extern uint64_t ai_time;
extern bool ai_observer_ready;
uint32_t political_id(Simulator::cCivilization* entity);
uint64_t object_key(const Simulator::cGameData* object);
} // namespace BetterCiv
