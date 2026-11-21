#include "Settings.h"
#include <Spore/ModAPI.h>
#include <Spore/Resource/IResourceManager.h>

namespace BetterCiv {
namespace {
constexpr uint32_t kPropertyResourceType = 0x00B1B104;
constexpr uint32_t kSettingsResourceGroup = 0xE2C5417D;

struct ChoiceEntry {
    SettingsGroup group;
    PublicChoice choice;
    const char* name;
};
constexpr ChoiceEntry choices[] = {
#define BETTER_CIV_CHOICE(group, name, instance) {SettingsGroup::group, PublicChoice::name, #name},
#include "SettingsChoices.inc"
#undef BETTER_CIV_CHOICE
};

struct GroupEntry {
    const char* name;
    PublicChoice default_choice;
};
// Same order as SettingsGroup. Vehicle Speed is applied by its native script.
constexpr GroupEntry groups[] = {
    {"VehicleCost", PublicChoice::CostVanilla}, {"FiringInterval", PublicChoice::FiringVanilla},
    {"OffensiveUnits", PublicChoice::OffensiveUnitsVanilla}, {"AIPopulation", PublicChoice::PopulationVanilla},
    {"AIStartingFunds", PublicChoice::FundsVanilla}, {"AISpecialties", PublicChoice::EqualOdds},
    {"CapturedCities", PublicChoice::CaptureVanilla}, {"TurretsAndFleets", PublicChoice::TurretsVanilla},
    {"TradeBehavior", PublicChoice::BetterTrade}, {"AIBehavior", PublicChoice::AIVanilla},
    {"AIDiagnostics", PublicChoice::DiagnosticsOff}
};
constexpr unsigned group_count = sizeof groups / sizeof *groups;
static_assert(group_count == static_cast<unsigned>(SettingsGroup::Count));
static_assert(sizeof choices / sizeof *choices == 34);

const char* ChoiceName(PublicChoice which) {
    for (const auto& entry : choices) {
        if (entry.choice == which) return entry.name;
    }
    return "unknown";
}

bool LoadedMarker(void* context, uint32_t instance, uint32_t type, uint32_t group) {
    auto& manager = *static_cast<Resource::IResourceManager*>(context);
    // FindDatabase matches the exact record key in the game's mounted DBPFs.
    // Looking up an empty PROP this way does not instantiate a property list.
    return manager.FindDatabase(ResourceKey(instance, type, group)) != nullptr;
}

void SetSpecialties(Settings& settings, unsigned mask, bool balanced = false) {
    settings.specialty_mask = mask;
    settings.balanced_specialties = balanced;
}
} // namespace

void ApplyPublicChoice(Settings& settings, PublicChoice choice) {
    using C = PublicChoice;
    switch (choice) {
    case C::CostVanilla: settings.cost_percent = 100; break;
    case C::Cost75: settings.cost_percent = 75; break;
    case C::Cost50: settings.cost_percent = 50; break;
    case C::FiringVanilla: settings.firing_interval_percent = 100; break;
    case C::Firing1500ms: settings.firing_interval_percent = 75; break;
    case C::Firing500ms: settings.firing_interval_percent = 25; break;
    case C::OffensiveUnitsVanilla: settings.offensive_vehicles = 8; break;
    case C::OffensiveUnits12: settings.offensive_vehicles = 12; break;
    case C::OffensiveUnits16: settings.offensive_vehicles = 16; break;
    case C::PopulationVanilla: settings.population_min = settings.population_max = 15; break;
    case C::Population30: settings.population_min = settings.population_max = 30; break;
    case C::Population15To50: settings.population_min = 15; settings.population_max = 50; break;
    case C::FundsVanilla: settings.wealth_min = settings.wealth_max = 4000; break;
    case C::Funds8000: settings.wealth_min = settings.wealth_max = 8000; break;
    case C::Funds4000To15000: settings.wealth_min = 4000; settings.wealth_max = 15000; break;
    case C::EqualOdds: SetSpecialties(settings, kAllSpecialties); break;
    case C::Balanced333: SetSpecialties(settings, kAllSpecialties, true); break;
    case C::NoMilitary: SetSpecialties(settings, kReligiousMask | kEconomicMask); break;
    case C::NoReligious: SetSpecialties(settings, kMilitaryMask | kEconomicMask); break;
    case C::NoEconomic: SetSpecialties(settings, kMilitaryMask | kReligiousMask); break;
    case C::MilitaryOnly: SetSpecialties(settings, kMilitaryMask); break;
    case C::ReligiousOnly: SetSpecialties(settings, kReligiousMask); break;
    case C::EconomicOnly: SetSpecialties(settings, kEconomicMask); break;
    case C::CaptureVanilla: settings.keep_rival_specialty = 0; break;
    case C::KeepSpecialty: settings.keep_rival_specialty = 1; break;
    case C::TurretsVanilla: settings.turrets = TURRETS_VANILLA; break;
    case C::MaxByTypeOnHard: settings.turrets = TURRETS_HARD_MAXIMUM; break;
    case C::EightSlotsPerCity: settings.turrets = TURRETS_ALL_SPECIALTIES; break;
    case C::TradeVanilla:
    case C::BetterTrade:
        settings.assist_rival_trade = settings.maintain_trade_under_fire = choice == C::BetterTrade;
        break;
    case C::AIVanilla:
    case C::BetterAI: {
        const bool enabled = choice == C::BetterAI;
        settings.rethink_offensives = settings.retire_stranded = settings.rebalance_land = enabled;
        settings.stalled_seconds = enabled ? 180 : 0;
        settings.retry_blocked_paths = enabled ? 2 : 0;
        break;
    }
    case C::DiagnosticsOff:
    case C::DiagnosticsOn: settings.ai_diagnostics = choice == C::DiagnosticsOn; break;
    }
}

ResolvedSettings ResolvePublicSettings(MarkerLookup lookup, void* context) {
    ResolvedSettings result;
    for (unsigned i = 0; i < sizeof choices / sizeof *choices; ++i) {
        const auto& entry = choices[i];
        if (!lookup || !lookup(context, static_cast<uint32_t>(entry.choice), kPropertyResourceType, kSettingsResourceGroup))
            continue;
        result.loaded_markers |= uint64_t{1} << i;
        auto& match = result.groups[static_cast<unsigned>(entry.group)];
        ++match.matches;
        match.choice = entry.choice;
    }
    // A missing or ambiguous group uses its public default; other independent
    // groups retain their choices. Filename or package traversal order is irrelevant.
    for (unsigned i = 0; i < group_count; ++i) {
        auto& match = result.groups[i];
        if (match.matches != 1) match.choice = groups[i].default_choice;
        ApplyPublicChoice(result.values, match.choice);
    }
    return result;
}

Settings ReadInstalledSettings() {
    auto* manager = Resource::IResourceManager::Get();
    if (!manager) {
        ModAPI::Log("[Better Civ Settings] Resource manager unavailable at startup; using public defaults.");
        return kPublicDefaults;
    }
    const auto loaded = ResolvePublicSettings(LoadedMarker, manager);
    for (unsigned i = 0; i < group_count; ++i) {
        const auto& match = loaded.groups[i];
        const char* source = "marker";
        if (match.matches == 0) source = "missing-default";
        if (match.matches > 1) source = "conflict-default";
        ModAPI::Log("[Better Civ Settings] %s=%s source=%s markers=%u",
            groups[i].name, ChoiceName(match.choice), source, match.matches);
        if (match.matches > 1) {
            for (unsigned j = 0; j < sizeof choices / sizeof *choices; ++j) {
                if (static_cast<unsigned>(choices[j].group) == i && (loaded.loaded_markers & (uint64_t{1} << j))) {
                    ModAPI::Log("[Better Civ Settings] Conflicting marker: %s=%s", groups[i].name, choices[j].name);
                }
            }
        }
    }
    ModAPI::Log("[Better Civ Settings] VehicleSpeed is applied by the installed game-data script.");
    return loaded.values;
}
} // namespace BetterCiv
