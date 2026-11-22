#include "CityRules.h"
#include "engine/Runtime.h"
#include "VehiclePrices.h"
#include <cstring>

namespace BetterCiv {
BalancedSpecialties starting_specialties;
FoundCityFunction original_found_city;
TurretLimitFunction original_turret_limit;
SetSpecialtyFunction original_set_specialty;
CityCountFunction original_building_count;
CityCountFunction allowed_turret_count;
FleetCapacityFunction original_fleet_capacity;
SimulationContextFunction simulation_context;
ScenarioSettingsFunction scenario_settings;
DifficultyFunction game_difficulty;
ReadScriptFloat read_script_float;
RandomBelow random_below;
void* simulation_random;

const float* configured_firing_interval::detoured()
{
    const float* vanilla = read_float_property(this);
    static thread_local float value;
    if (!vanilla || settings.firing_interval_percent == 100 || !in_civilization())
        return vanilla;
    value = *vanilla * static_cast<float>(settings.firing_interval_percent) / 100.0f;
    return &value;
}

int custom_population()
{
    return settings.population_min != 15 || settings.population_max != 15;
}

int custom_wealth()
{
    return settings.wealth_min != 4000 || settings.wealth_max != 4000;
}

float configured_civ_generation::detoured(const char* name)
{
    if (custom_population()) {
        if (strcmp(name, "population_min") == 0) return static_cast<float>(settings.population_min);
        if (strcmp(name, "population_max") == 0) return static_cast<float>(settings.population_max);
    }
    if (custom_wealth()) {
        if (strcmp(name, "wealth_min") == 0) return static_cast<float>(settings.wealth_min);
        if (strcmp(name, "wealth_max") == 0) return static_cast<float>(settings.wealth_max);
    }
    return read_script_float(this, name);
}

float configured_starting_vehicles::detoured(const char* name)
{
    if (strcmp(name, "num_offensive_vehicles") == 0) return static_cast<float>(settings.offensive_vehicles);
    return read_script_float(this, name);
}

int civilization_difficulty()
{
    if (!in_civilization())
        return -1;
    void* context = simulation_context();
    void* scenario = context ? scenario_settings(context) : nullptr;
    return scenario ? game_difficulty(scenario) : -1;
}

int fleet_capacity_with_defenses(int capacity, TurretMode mode, int difficulty)
{
    // Ratios use the sum of the three large-city turret targets before the AI's budget checks.
    int vanilla = 13;
    int configured = 18;
    if (mode == TURRETS_ALL_SPECIALTIES) {
        switch (difficulty) {
        case kDifficultyEasy:
            vanilla = 6;
            configured = 9;
            break;
        case kDifficultyMedium:
            vanilla = 9;
            configured = 12;
            break;
        case kDifficultyHard:
            configured = 24;
            break;
        default:
            return capacity;
        }
    } else if (mode != TURRETS_HARD_MAXIMUM || difficulty != kDifficultyHard) {
        return capacity;
    }
    if (capacity <= 0 || capacity >= kNativeFleetLimit)
        return capacity;
    const int adjusted = (capacity * configured + vanilla / 2) / vanilla;
    return adjusted < kNativeFleetLimit ? adjusted : kNativeFleetLimit;
}

int specialty_capacity(Civilization& civ, int specialty)
{
    const int native = original_fleet_capacity(&civ, specialty);
    if (specialty < kMilitary || specialty > kEconomic)
        return native;
    return fleet_capacity_with_defenses(native, settings.turrets, civilization_difficulty());
}

int configured_fleet_capacity::detoured(int specialty)
{
    return specialty_capacity(*this, specialty);
}

// Only the native remaining-slots query reserves the last place. Other
// capacity readers still see the configured specialty capacity.
int recruitment_capacity::detoured(int specialty)
{
    const int limit = specialty_capacity(*this, specialty);
    return specialty == kEconomic && in_civilization() ? merchant_capacity(*this, limit) : limit;
}

int defensive_building_count::detoured()
{
    auto* city = this;
    if (civilization_difficulty() == kDifficultyHard) {
        // The AI uses (count + 1) / 2 for this independent construction target.
        int capacity = allowed_turret_count(city);
        if (capacity > 0 && capacity <= kMilitaryLargeTurretCapacity)
            return capacity * 2 - 1;
    }
    return original_building_count(city);
}

int capacity_in_layout(uintptr_t begin, uintptr_t end, int original_limit)
{
    if (!begin || end < begin || (end - begin) % kTurretSlotSize)
        return original_limit;

    uintptr_t slots = (end - begin) / kTurretSlotSize;
    if (slots <= static_cast<uintptr_t>(original_limit))
        return original_limit;
    return slots < kMilitaryLargeTurretCapacity ? static_cast<int>(slots) : kMilitaryLargeTurretCapacity;
}

City* __cdecl found_major_city(Native::CityFoundation* site)
{
    // The guarded native founder passes false as Initialize's player flag.
    // The player's city and save restoration do not use this rival call site.
    int reserved_specialty = -1;
    const int old_specialty = site ? site->specialty : -1;
    if (site && in_civilization()) {
        site->minorNation = false;
        if (settings.balanced_specialties) {
            const unsigned remaining = starting_specialties.Remaining();
            if (remaining) {
                const unsigned ticket = remaining == 1 ? 0 : random_below(simulation_random, remaining);
                reserved_specialty = starting_specialties.Take(ticket);
                if (reserved_specialty >= 0)
                    site->specialty = Native::FoundationSpecialtyForVehiclePurpose(reserved_specialty);
            }
        } else if (settings.specialty_mask != kAllSpecialties) {
            int candidates[3];
            unsigned count = 0;
            for (int specialty = kMilitary; specialty <= kEconomic; ++specialty) {
                if (settings.specialty_mask & (1u << specialty))
                    candidates[count++] = specialty;
            }
            if (count) {
                unsigned index = count == 1 ? 0 : random_below(simulation_random, count);
                site->specialty = Native::FoundationSpecialtyForVehiclePurpose(candidates[index]);
            }
        }
    }
    City* city = original_found_city(site);
    if (reserved_specialty >= 0) {
        if (!city) {
            starting_specialties.Return(reserved_specialty);
            site->specialty = old_specialty;
        } else if (settings.ai_diagnostics) {
            ModAPI::Log("[Better Civ Start] balanced rival=%u specialty=%d (0=military 1=religious 2=economic).",
                9u - starting_specialties.Remaining(), reserved_specialty);
        }
    }
    return city;
}

int extended_turret_limit::detoured()
{
    auto* city = this;
    int limit = original_turret_limit(city);
    if (!in_civilization() || limit <= 0)
        return limit;

    const auto& slots = city->mTurretsLayout.mSlots;
    const auto begin = reinterpret_cast<uintptr_t>(slots.begin());
    const auto end = reinterpret_cast<uintptr_t>(slots.end());
    return capacity_in_layout(begin, end, limit);
}

int defensive_capacity_target::detoured()
{
    auto* city = this;
    int capacity = allowed_turret_count(city);
    // The Hard branch takes floor(4 * target / 5) before checking its remaining budget.
    if (capacity > 0 && capacity <= kMilitaryLargeTurretCapacity &&
        civilization_difficulty() == kDifficultyHard)
        return (capacity * 5 + 3) / 4;
    return capacity;
}

void retain_rival_city_specialty::detoured(int specialty, int update_walls)
{
    auto* city = this;
    if (!in_civilization() || city->mbIsPlayerCity)
        original_set_specialty(city, specialty, update_walls);
}
} // namespace BetterCiv
