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

struct Settings {
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
    int rethink_offensives;
    int ai_diagnostics;
    int stalled_seconds;
    int balanced_specialties = 0;
    int retry_blocked_paths = 0;
    int retire_stranded = 0;
    int rebalance_land = 0;
};

inline int matches_choice(const char *line, const char *current, const char *legacy)
{
    // The v1.0 installer writes the original identifiers into ModSettings.
    return strcmp(line, current) == 0 || strcmp(line, legacy) == 0;
}

inline void apply_option(Settings *out, const char *line)
{
    if (strcmp(line, "<TradeBehavior>BetterTrade") == 0 ||
        strcmp(line, "<TradeBehavior>TradeVanilla") == 0) {
        const int enabled = strcmp(line, "<TradeBehavior>BetterTrade") == 0;
        out->assist_rival_trade = out->maintain_trade_under_fire = enabled;
    }
    if (strcmp(line, "<AIBehavior>BetterAI") == 0 ||
        strcmp(line, "<AIBehavior>AIVanilla") == 0) {
        const int enabled = strcmp(line, "<AIBehavior>BetterAI") == 0;
        out->rethink_offensives = enabled;
        out->stalled_seconds = enabled ? 180 : 0;
        out->retry_blocked_paths = enabled ? 2 : 0;
        out->retire_stranded = out->rebalance_land = enabled;
    }
    if (strcmp(line, "<StrandedTraders>StrandedSell") == 0) out->retire_stranded=1;
    if (strcmp(line, "<StrandedTraders>StrandedVanilla") == 0) out->retire_stranded=0;
    if (strcmp(line, "<SurplusLand>LandRebalance") == 0) out->rebalance_land=1;
    if (strcmp(line, "<SurplusLand>LandVanilla") == 0) out->rebalance_land=0;
    if (strcmp(line, "<VehicleMovement>MovementVanilla") == 0) out->retry_blocked_paths = 0;
    if (strcmp(line, "<VehicleMovement>RetryBlockedPaths") == 0) out->retry_blocked_paths = 1;
    if (strcmp(line, "<VehicleMovement>NearbySpace") == 0) out->retry_blocked_paths = 2;
    if (strcmp(line, "<StalledOffensives>StalledOff") == 0) out->stalled_seconds = 0;
    if (strcmp(line, "<StalledOffensives>Stalled120") == 0) out->stalled_seconds = 120;
    if (strcmp(line, "<StalledOffensives>Stalled180") == 0) out->stalled_seconds = 180;
    if (strcmp(line, "<StalledOffensives>Stalled240") == 0) out->stalled_seconds = 240;
    if (strcmp(line, "<OffensiveTargets>RethinkFailedAttacks") == 0) out->rethink_offensives = 1;
    if (strcmp(line, "<OffensiveTargets>TargetsVanilla") == 0) out->rethink_offensives = 0;
    if (strcmp(line, "<AIDiagnostics>DiagnosticsOn") == 0) out->ai_diagnostics = 1;
    if (strcmp(line, "<AIDiagnostics>DiagnosticsOff") == 0) out->ai_diagnostics = 0;
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
    // Reapplying a specialty choice must also leave a previous balanced mode.
    if (strcmp(line, "<AISpecialties>Balanced333") == 0) {
        out->specialty_mask = kAllSpecialties;
        out->balanced_specialties = 1;
    }
    if (matches_choice(line, "<AISpecialties>EqualOdds", "<Specialites>TroisSpecialites")) {
        out->specialty_mask = kAllSpecialties;
        out->balanced_specialties = 0;
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
    if (out->specialty_mask != kAllSpecialties) out->balanced_specialties = 0;
    if (matches_choice(line, "<CapturedCities>KeepSpecialty", "<Capture>CaptureConserve"))
        out->keep_rival_specialty = 1;
    if (matches_choice(line, "<TurretsAndFleets>MaxByTypeOnHard", "<Tourelles>TourellesMaximum"))
        out->turrets = TURRETS_HARD_MAXIMUM;
    if (matches_choice(line, "<TurretsAndFleets>EightSlotsPerCity", "<Tourelles>TourellesTous"))
        out->turrets = TURRETS_ALL_SPECIALTIES;
    if (matches_choice(line, "<RivalTrade>RivalTradeVanilla", "<Commerce>CommerceVanilla"))
        out->assist_rival_trade = 0;
    if (matches_choice(line, "<RivalTrade>FirstRivalRoute", "<Commerce>PremierPartenaire"))
        out->assist_rival_trade = 1;
    if (matches_choice(line, "<TradeUnderFire>RetreatVanilla", "<RepliCommercial>RepliVanilla"))
        out->maintain_trade_under_fire = 0;
    if (matches_choice(line, "<TradeUnderFire>StayOnCourse", "<RepliCommercial>MaintenirTrajet"))
        out->maintain_trade_under_fire = 1;
}

// A public preset is authoritative even if an older installer left granular
// lines in the settings file. Developer manifests never write these two groups.
struct OptionReader {
    Settings* out;
    int trade = -1;
    int ai = -1;

    explicit OptionReader(Settings* value) : out(value) {}
    void consume(const char* line) {
        apply_option(out, line);
        if (strcmp(line, "<TradeBehavior>BetterTrade") == 0) trade = 1;
        if (strcmp(line, "<TradeBehavior>TradeVanilla") == 0) trade = 0;
        if (strcmp(line, "<AIBehavior>BetterAI") == 0) ai = 1;
        if (strcmp(line, "<AIBehavior>AIVanilla") == 0) ai = 0;
    }
    void finish() {
        if (trade >= 0) apply_option(out, trade ? "<TradeBehavior>BetterTrade" : "<TradeBehavior>TradeVanilla");
        if (ai >= 0) apply_option(out, ai ? "<AIBehavior>BetterAI" : "<AIBehavior>AIVanilla");
    }
};

} // namespace BetterCiv
