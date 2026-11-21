#pragma once
#include "BalancedSpecialties.h"
#include "NativeExtensions.h"
#include "Settings.h"
#include "SimulationRuntime.h"
#include <Spore/ModAPI.h>
#include <Spore/App/Property.h>
#include <Spore/App/cArithmeticaResource.h>

namespace BetterCiv {
using City = Simulator::cCity;
using Civilization = Simulator::cCivilization;
inline constexpr int kNativeFleetLimit = 36;
inline constexpr int kMilitaryLargeTurretCapacity = 8;
inline constexpr int kTurretSlotSize = 0x18;

typedef City* (__cdecl *FoundCityFunction)(Native::CityFoundation* site);
typedef int (__thiscall *TurretLimitFunction)(City* city);
typedef void (__thiscall *SetSpecialtyFunction)(City* city, int specialty, int updateWalls);
typedef int (__thiscall *CityCountFunction)(City* city);
typedef int (__thiscall *FleetCapacityFunction)(Civilization* civilization, int specialty);
typedef void *(__thiscall *ScenarioSettingsFunction)(void *context);
typedef int (__thiscall *DifficultyFunction)(void *settings);
typedef float (__thiscall *ReadScriptFloat)(App::cArithmeticaResource*, const char*);
typedef unsigned (__thiscall *RandomBelow)(void *, unsigned);

extern FoundCityFunction original_found_city;
extern TurretLimitFunction original_turret_limit;
extern SetSpecialtyFunction original_set_specialty;
extern CityCountFunction original_building_count;
extern CityCountFunction allowed_turret_count;
extern FleetCapacityFunction original_fleet_capacity;
extern SimulationContextFunction simulation_context;
extern ScenarioSettingsFunction scenario_settings;
extern DifficultyFunction game_difficulty;
extern ReadScriptFloat read_script_float;
extern RandomBelow random_below;
extern void *simulation_random;
extern BalancedSpecialties starting_specialties;

int custom_population();
int custom_wealth();
int civilization_difficulty();
int specialty_capacity(Civilization& civ, int specialty);
int fleet_capacity_with_defenses(int capacity, TurretMode mode, int difficulty);
int capacity_in_layout(uintptr_t begin, uintptr_t end, int original_limit);
City* __cdecl found_major_city(Native::CityFoundation* site);
int merchant_capacity(Civilization& civ, int capacity);

member_detour(configured_firing_interval, App::Property, const float *()) {
    const float* detoured();
};

member_detour(configured_civ_generation, App::cArithmeticaResource, float(const char *name)) {
    float detoured(const char *name);
};

member_detour(configured_starting_vehicles, App::cArithmeticaResource, float(const char *name)) {
    float detoured(const char *name);
};

member_detour(configured_fleet_capacity, Civilization, int(int specialty)) {
    int detoured(int specialty);
};

member_detour(recruitment_capacity, Civilization, int(int specialty)) {
    int detoured(int specialty);
};

member_detour(defensive_building_count, City, int()) {
    int detoured();
};

member_detour(extended_turret_limit, City, int()) {
    int detoured();
};

member_detour(defensive_capacity_target, City, int()) {
    int detoured();
};

member_detour(retain_rival_city_specialty, City, void(int specialty, int update_walls)) {
    void detoured(int specialty, int update_walls);
};
} // namespace BetterCiv
