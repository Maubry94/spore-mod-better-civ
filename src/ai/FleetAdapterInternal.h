#pragma once
#include "FleetAdapter.h"
#include "FleetPolicy.h"

namespace BetterCiv {
// The native fixture supplies a stand-in planet, sale callback and relationship table.
using FleetSell = void (__thiscall*)(Simulator::cVehicle*, bool);
using FleetWater = bool (__thiscall*)(Simulator::cCity*, int);
extern FleetSell fleet_sell;
extern FleetWater fleet_water;
extern Fleet::State fleet_state;
Simulator::cPlanetModel* fleet_sdk_planet();
bool fleet_hostile(Simulator::cCivilization& owner, Simulator::cCivilization& other);
extern bool (*fleet_are_enemies)(Simulator::cCivilization&, Simulator::cCivilization&);

struct FleetAudit {
    Fleet::Facts facts;
    Fleet::Veto veto = Fleet::Veto::NotCandidate;
    int region = -1;
};

Fleet::Reason fleet_reason(Simulator::cCivilization& owner, Simulator::cVehicle& unit,
                           Simulator::cCity*& target, FleetAudit* audit = nullptr);
} // namespace BetterCiv
