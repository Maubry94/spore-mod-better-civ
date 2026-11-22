#pragma once
#include <Spore/MathUtils.h>

namespace Simulator {
class cCity;
class cCivilization;
class cPlanetModel;
class cVehicle;
}

namespace BetterCiv {
void bind_fleet_native(unsigned char* base);
void reset_fleet_session();
void manage_unused_fleets(int delta, bool civ_mode);
void __fastcall guarded_ai_land_sale(Simulator::cVehicle* unit, int, bool refund);

// The trade adapter uses the same ownership and connected-sea checks as fleet retirement.
bool fleet_owned_city(const Simulator::cCity* city, const Simulator::cCivilization* owner);
bool fleet_physical_port(const Simulator::cCity* city);
bool fleet_has_port(Simulator::cCivilization& owner, int purpose = -1);
using FleetContinent = int (__thiscall*)(Simulator::cPlanetModel*, const Math::Vector3&);
extern bool fleet_native_ready;
extern FleetContinent fleet_continent;
extern Simulator::cPlanetModel* (*fleet_planet)();
} // namespace BetterCiv
