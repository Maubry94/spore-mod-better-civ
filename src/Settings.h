#pragma once
#include <cstring>

namespace BetterCiv {
enum {
    kMilitary = 0, kReligious = 1, kEconomic = 2,
    kMilitaryMask = 1 << kMilitary, kReligiousMask = 1 << kReligious,
    kEconomicMask = 1 << kEconomic,
    kAllSpecialties = kMilitaryMask | kReligiousMask | kEconomicMask
};
typedef enum {
    TURRETS_VANILLA,
    TURRETS_HARD_MAXIMUM,
    TURRETS_ALL_SPECIALTIES,
} TurretMode;

typedef struct {
    int cost_percent;
    int firing_interval_percent;
    int offensive_vehicles;
    int population_min;
    int population_max;
    int wealth_min;
    int wealth_max;
    unsigned specialty_mask;
    int keep_rival_specialty;
    TurretMode turrets;
    int assist_rival_trade;
    int maintain_trade_under_fire;
} Settings;

inline int matches_choice(const char *line, const char *current, const char *legacy)
{
    // The v1.0 installer writes the original identifiers into ModSettings.
    return strcmp(line, current) == 0 || strcmp(line, legacy) == 0;
}

inline void apply_option(Settings *out, const char *line)
{
    if (matches_choice(line, "<VehicleCost>Cost75", "<Prix>PrixModere")) out->cost_percent = 75;
    if (matches_choice(line, "<VehicleCost>Cost50", "<Prix>PrixReduits")) out->cost_percent = 50;
    if (matches_choice(line, "<FiringInterval>Firing1500ms", "<Tir>TirModere"))
        out->firing_interval_percent = 75;
    if (matches_choice(line, "<FiringInterval>Firing500ms", "<Tir>TirRapide"))
        out->firing_interval_percent = 25;
    if (matches_choice(line, "<OffensiveUnits>OffensiveUnits12", "<Offensifs>Offensifs12"))
        out->offensive_vehicles = 12;
    if (matches_choice(line, "<OffensiveUnits>OffensiveUnits16", "<Offensifs>Offensifs16"))
        out->offensive_vehicles = 16;
    if (matches_choice(line, "<AIPopulation>Population30", "<Population>PopulationEgale30"))
        out->population_min = out->population_max = 30;
    if (matches_choice(line, "<AIPopulation>Population15To50", "<Population>PopulationVariable")) {
        out->population_min = 15;
        out->population_max = 50;
    }
    if (matches_choice(line, "<AIStartingFunds>Funds8000", "<Tresorerie>TresorerieEgale8000"))
        out->wealth_min = out->wealth_max = 8000;
    if (matches_choice(line, "<AIStartingFunds>Funds4000To15000", "<Tresorerie>TresorerieVariable")) {
        out->wealth_min = 4000;
        out->wealth_max = 15000;
    }
    if (matches_choice(line, "<AISpecialties>NoMilitary", "<Specialites>SansMilitaires"))
        out->specialty_mask = kReligiousMask | kEconomicMask;
    if (matches_choice(line, "<AISpecialties>NoReligious", "<Specialites>SansReligieuses"))
        out->specialty_mask = kMilitaryMask | kEconomicMask;
    if (matches_choice(line, "<AISpecialties>NoEconomic", "<Specialites>SansEconomiques"))
        out->specialty_mask = kMilitaryMask | kReligiousMask;
    if (matches_choice(line, "<AISpecialties>MilitaryOnly", "<Specialites>MilitairesSeulement"))
        out->specialty_mask = kMilitaryMask;
    if (matches_choice(line, "<AISpecialties>ReligiousOnly", "<Specialites>ReligieusesSeulement"))
        out->specialty_mask = kReligiousMask;
    if (matches_choice(line, "<AISpecialties>EconomicOnly", "<Specialites>EconomiquesSeulement"))
        out->specialty_mask = kEconomicMask;
    if (matches_choice(line, "<CapturedCities>KeepSpecialty", "<Capture>CaptureConserve"))
        out->keep_rival_specialty = 1;
    if (matches_choice(line, "<TurretsAndFleets>MaxByTypeOnHard", "<Tourelles>TourellesMaximum"))
        out->turrets = TURRETS_HARD_MAXIMUM;
    if (matches_choice(line, "<TurretsAndFleets>EightSlotsPerCity", "<Tourelles>TourellesTous"))
        out->turrets = TURRETS_ALL_SPECIALTIES;
    if (matches_choice(line, "<RivalTrade>RivalTradeVanilla", "<Commerce>CommerceVanilla"))
        out->assist_rival_trade = 0;
    if (matches_choice(line, "<TradeUnderFire>RetreatVanilla", "<RepliCommercial>RepliVanilla"))
        out->maintain_trade_under_fire = 0;
}


} // namespace BetterCiv
