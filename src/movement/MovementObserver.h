#pragma once
#include "diplomacy/PortraitOrder.h"
#include "engine/SimulationRuntime.h"

namespace BetterCiv::Movement {

struct Journal;

void Observe(Journal& journal, const CivilizationList& civilizations, CurrentVehicleOrder read_order, uint64_t now);

} // namespace BetterCiv::Movement
