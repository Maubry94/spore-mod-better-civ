#pragma once
#include <cstdint>

namespace BetterCiv {
enum {
    kMilitary = 0, kReligious = 1, kEconomic = 2,
    kMilitaryMask = 1 << kMilitary, kReligiousMask = 1 << kReligious,
    kEconomicMask = 1 << kEconomic,
    kAllSpecialties = kMilitaryMask | kReligiousMask | kEconomicMask
};
enum TurretMode {
    TURRETS_VANILLA,
    TURRETS_HARD_MAXIMUM,
    TURRETS_ALL_SPECIALTIES,
};

struct Settings {
    int cost_percent = 100;
    int firing_interval_percent = 100;
    int offensive_vehicles = 8;
    int population_min = 15;
    int population_max = 15;
    int wealth_min = 4000;
    int wealth_max = 4000;
    unsigned specialty_mask = kAllSpecialties;
    int keep_rival_specialty = 0;
    TurretMode turrets = TURRETS_VANILLA;

    int assist_rival_trade = 1;
    int maintain_trade_under_fire = 1;
    int rethink_offensives = 0;
    int ai_diagnostics = 0;
    int stalled_seconds = 0;
    int balanced_specialties = 0;
    int retry_blocked_paths = 0;
    int retire_stranded = 0;
    int rebalance_land = 0;
};

inline constexpr Settings kPublicDefaults{};

enum class PublicChoice : uint32_t {
#define BETTER_CIV_CHOICE(group, name, instance) name = instance,
#include "SettingsChoices.inc"
#undef BETTER_CIV_CHOICE
};

enum class SettingsGroup {
    VehicleCost, FiringInterval, OffensiveUnits, AIPopulation, AIStartingFunds,
    AISpecialties, CapturedCities, TurretsAndFleets, TradeBehavior, AIBehavior,
    AIDiagnostics, Count
};

struct ResolvedChoice {
    PublicChoice choice{};
    unsigned matches = 0;
};

struct ResolvedSettings {
    Settings values = kPublicDefaults;
    ResolvedChoice groups[static_cast<unsigned>(SettingsGroup::Count)]{};
    uint64_t loaded_markers = 0;
};

using MarkerLookup = bool (*)(void* context, uint32_t instance, uint32_t type, uint32_t group);
void ApplyPublicChoice(Settings& settings, PublicChoice choice);
ResolvedSettings ResolvePublicSettings(MarkerLookup lookup, void* context);
Settings ReadInstalledSettings();
} // namespace BetterCiv
