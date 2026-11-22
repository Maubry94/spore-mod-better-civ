#pragma once
#include "MovementAdapter.h"
#include "engine/NativeExtensions.h"

namespace BetterCiv {

Recovery::Sample sample_movement(Simulator::cVehicle& unit, uint32_t owner, const Native::VehicleOrder*& order);
void recover_nearby_space(int deltaGameTime, bool civ_mode);

using LocalRequestSetter = void(__thiscall*)(Simulator::cLocomotiveObject*, const Native::LocalPointRequest*);
extern LocalRequestSetter original_local_request;

// Native queries and placement plans are replaceable in the offline vehicle fixtures.
extern Recovery::State recovery_state;
bool recovery_methods_match(const Simulator::cVehicle& unit);
extern bool (*placement_check_terrain)(const Terrain::ITerrain&);
bool placement_find_space(Simulator::cVehicle& unit, const Recovery::Sample& sample, Math::Vector3& goal,
                          Placement::SearchReport& report);
extern bool (*placement_plan)(Simulator::cVehicle&, const Recovery::Sample&, Math::Vector3&,
                              Placement::SearchReport&);

} // namespace BetterCiv
