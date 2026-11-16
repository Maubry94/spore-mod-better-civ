#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <Spore/ModAPI.h>
#include <Spore/App/Property.h>
#include <Spore/App/cArithmeticaResource.h>
#include "Settings.h"
#include "NativeExtensions.h"
#include "BetterCiv.h"

namespace BetterCiv {
using City = Simulator::cCity;
using Civilization = Simulator::cCivilization;
using Vehicle = Simulator::cVehicle;

enum {
    kReferenceBase = 0x400000,
    kReferenceTimestamp = 1727737552,
    kReferenceImageSize = 26271744,
    kSteamOctober2024ImageSize = 26443776,
    kCivilizationMode = 0x01654c04,
    kTurretSlotSize = 0x18,
    kMilitaryLargeTurretCapacity = 8,
    kNativeFleetLimit = 36,
    kRelationshipNeutral = 2,
    kTradeDeliveryOrder = 5,
    kCreateTradeRouteOrder = 9,
    kCivEconomicStrategy = 1,
    kVehicleRetreatCondition = 0xdc47e0,
};

typedef uint32_t (__cdecl *ModeFunction)(void);
typedef City* (__cdecl *FoundCityFunction)(Native::CityFoundation* site);
typedef int (__thiscall *TurretLimitFunction)(City* city);
typedef void (__thiscall *SetSpecialtyFunction)(City* city, int specialty, int updateWalls);

static ModeFunction game_mode;
static FoundCityFunction original_found_city;
static TurretLimitFunction original_turret_limit;
static SetSpecialtyFunction original_set_specialty;
typedef int (__thiscall *CityCountFunction)(City* city);
typedef int (__thiscall *FleetCapacityFunction)(Civilization* civilization, int specialty);
typedef void *(__cdecl *SimulationContextFunction)(void);
typedef void *(__thiscall *ScenarioSettingsFunction)(void *context);
typedef int (__thiscall *DifficultyFunction)(void *settings);

static CityCountFunction original_building_count;
static CityCountFunction allowed_turret_count;
static FleetCapacityFunction original_fleet_capacity;
static SimulationContextFunction simulation_context;
static ScenarioSettingsFunction scenario_settings;
static DifficultyFunction game_difficulty;

typedef float (__thiscall *ReadScriptFloat)(App::cArithmeticaResource*, const char*);
typedef const float *(__thiscall *ReadFloatProperty)(App::Property*);
typedef unsigned (__thiscall *RandomBelow)(void *, unsigned);
typedef float (__thiscall *TradeAcceptance)(Native::TradeTuning*, City* city, Civilization* civilization);
typedef Civilization* (__thiscall *CityOwner)(City* city);
typedef int (__thiscall *CivilizationStrategy)(Civilization* civilization);
typedef int (__thiscall *RelationshipLevel)(void *, uint32_t, uint32_t, int);
typedef Civilization* (__thiscall *FindPlayerCivilization)(void*);

using CurrentVehicleOrder = const Native::VehicleOrder* (__thiscall*)(Vehicle*);
using Native::VehicleOrder;

static Settings settings = {
    100, 100, 8, 15, 15, 4000, 4000, kAllSpecialties, 0, TURRETS_VANILLA, 1, 1,
};
static HINSTANCE mod_instance;
static ReadScriptFloat read_script_float;
static ReadFloatProperty read_float_property;
static RandomBelow random_below;
static void *simulation_random;
static TradeAcceptance original_trade_acceptance;
static CityOwner city_owner;
static CivilizationStrategy civilization_strategy;
static SimulationContextFunction relationship_manager;
static RelationshipLevel relationship_level;
static FindPlayerCivilization find_player_civilization;
static CurrentVehicleOrder current_vehicle_order;

static void load_options(void)
{
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(mod_instance, path, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return;
    wchar_t *file_separator = wcsrchr(path, L'\\');
    if (!file_separator)
        return;
    *file_separator = L'\0';
    wchar_t *parent_separator = wcsrchr(path, L'\\');
    if (parent_separator && _wcsicmp(parent_separator + 1, L"mLibs") == 0)
        *parent_separator = L'\0';
    const wchar_t suffix[] = L"\\ModSettings\\BetterCiv";
    if (wcslen(path) + sizeof suffix / sizeof *suffix > MAX_PATH)
        return;
    wcscat(path, suffix);
    FILE *file = _wfopen(path, L"rb");
    if (!file)
        return;
    char line[128];
    int first = 1;
    while (fgets(line, sizeof line, file)) {
        char *start = line;
        if (first && strncmp(start, "\xef\xbb\xbf", 3) == 0)
            start += 3;
        first = 0;
        char *end = strpbrk(start, "\r\n");
        if (!end && !feof(file)) {
            int byte;
            while ((byte = fgetc(file)) != '\n' && byte != EOF) {}
            continue;
        }
        if (end)
            *end = '\0';
        apply_option(&settings, start);
    }
    fclose(file);
}

static int in_civilization(void)
{
    return game_mode() == kCivilizationMode;
}

member_detour(configured_price, App::Property, const float *()) {
    const float* detoured()
    {
        auto* property = this;
        const float *vanilla = read_float_property(property);
        static thread_local float value;
        if (!vanilla || settings.cost_percent == 100 || !in_civilization())
            return vanilla;
        value = *vanilla * (float)settings.cost_percent / 100.0f;
        return &value;
    }
};

member_detour(configured_firing_interval, App::Property, const float *()) {
    const float* detoured()
    {
        auto* property = this;
        const float *vanilla = read_float_property(property);
        static thread_local float value;
        if (!vanilla || settings.firing_interval_percent == 100 || !in_civilization())
            return vanilla;
        value = *vanilla * (float)settings.firing_interval_percent / 100.0f;
        return &value;
    }
};

static int custom_population(void)
{
    return settings.population_min != 15 || settings.population_max != 15;
}

static int custom_wealth(void)
{
    return settings.wealth_min != 4000 || settings.wealth_max != 4000;
}

member_detour(configured_civ_generation, App::cArithmeticaResource, float(const char *name)) {
    float detoured(const char *name)
    {
        auto* script = this;
        if (custom_population()) {
            if (strcmp(name, "population_min") == 0) return (float)settings.population_min;
            if (strcmp(name, "population_max") == 0) return (float)settings.population_max;
        }
        if (custom_wealth()) {
            if (strcmp(name, "wealth_min") == 0) return (float)settings.wealth_min;
            if (strcmp(name, "wealth_max") == 0) return (float)settings.wealth_max;
        }
        return read_script_float(script, name);
    }
};

member_detour(configured_starting_vehicles, App::cArithmeticaResource, float(const char *name)) {
    float detoured(const char *name)
    {
        auto* script = this;
        if (strcmp(name, "num_offensive_vehicles") == 0) return (float)settings.offensive_vehicles;
        return read_script_float(script, name);
    }
};

static int civilization_difficulty(void)
{
    if (!in_civilization())
        return -1;
    void *context = simulation_context();
    void *scenario = context ? scenario_settings(context) : NULL;
    return scenario ? game_difficulty(scenario) : -1;
}

static int fleet_capacity_with_defenses(int capacity, TurretMode mode, int difficulty)
{
    // Ratios use the sum of the three large-city turret targets before the AI's budget checks.
    int vanilla = 13;
    int configured = 18;
    if (mode == TURRETS_ALL_SPECIALTIES) {
        switch (difficulty) {
            case kDifficultyEasy:   vanilla = 6; configured = 9; break;
            case kDifficultyMedium: vanilla = 9; configured = 12; break;
            case kDifficultyHard:   configured = 24; break;
            default: return capacity;
        }
    } else if (mode != TURRETS_HARD_MAXIMUM || difficulty != kDifficultyHard) {
        return capacity;
    }
    if (capacity <= 0 || capacity >= kNativeFleetLimit)
        return capacity;
    int adjusted = (capacity * configured + vanilla / 2) / vanilla;
    return adjusted < kNativeFleetLimit ? adjusted : kNativeFleetLimit;
}

member_detour(configured_fleet_capacity, Civilization, int(int specialty)) {
    int detoured(int specialty)
    {
        auto* civilization = this;
        int native = original_fleet_capacity(civilization, specialty);
        if (specialty < kMilitary || specialty > kEconomic)
            return native;
        return fleet_capacity_with_defenses(native, settings.turrets, civilization_difficulty());
    }
};

member_detour(defensive_building_count, City, int()) {
    int detoured()
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
};

static int capacity_in_layout(uintptr_t begin, uintptr_t end, int original_limit)
{
    if (!begin || end < begin || (end - begin) % kTurretSlotSize)
        return original_limit;

    uintptr_t slots = (end - begin) / kTurretSlotSize;
    if (slots <= (uintptr_t)original_limit)
        return original_limit;
    return slots < kMilitaryLargeTurretCapacity ? (int)slots : kMilitaryLargeTurretCapacity;
}

static City* __cdecl found_major_city(Native::CityFoundation* site)
{
    if (site && in_civilization()) {
        site->minorNation = false;
        if (settings.specialty_mask != kAllSpecialties) {
            int candidates[3];
            unsigned count = 0;
            for (int specialty = kMilitary; specialty <= kEconomic; ++specialty) {
                if (settings.specialty_mask & (1u << specialty))
                    candidates[count++] = specialty;
            }
            if (count) {
                unsigned index = count == 1 ? 0 : random_below(simulation_random, count);
                site->specialty = candidates[index];
            }
        }
    }
    return original_found_city(site);
}

member_detour(extended_turret_limit, City, int()) {
    int detoured()
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
};

member_detour(defensive_capacity_target, City, int()) {
    int detoured()
    {
        auto* city = this;
        int capacity = allowed_turret_count(city);
        // The Hard branch takes floor(4 * target / 5) before checking its remaining budget.
        if (capacity > 0 && capacity <= kMilitaryLargeTurretCapacity &&
            civilization_difficulty() == kDifficultyHard)
            return (capacity * 5 + 3) / 4;
        return capacity;
    }
};

member_detour(retain_rival_city_specialty, City, void(int specialty, int update_walls)) {
    void detoured(int specialty, int update_walls)
    {
        auto* city = this;
        if (game_mode() != kCivilizationMode || city->mbIsPlayerCity)
            original_set_specialty(city, specialty, update_walls);
    }
};

static uint32_t political_id(Civilization* entity)
{
    return entity->GetPoliticalID();
}

static int has_rival_trade(Civilization* civilization, uint32_t own_id, uint32_t player_id)
{
    for (const auto& city : civilization->mCities) {
        if (!city) continue;
        for (const auto& partner : city->mTradePartners) {
            const auto id = Native::TradePartnerPoliticalId(partner);
            if (id != own_id && id != player_id && id != UINT32_MAX)
                return 1;
        }
    }
    return 0;
}

static int has_other_rival_trade_order(Civilization* civilization, City* city)
{
    for (const auto& vehicle : civilization->mVehicles) {
        if (!vehicle) continue;
        const VehicleOrder* order = current_vehicle_order(vehicle.get());
        if (!order || order->kind != kCreateTradeRouteOrder || !order->target ||
            order->target == city)
            continue;
        // Order 9 targets a city. A route to the player does not reserve a rival partner.
        const auto* owner = city_owner(static_cast<City*>(order->target));
        if (owner && owner != civilization && !owner->mIsPlayerOwned)
            return 1;
    }
    return 0;
}

member_detour(trade_acceptance, Native::TradeTuning, float(City* city, Civilization* civilization)) {
    float detoured(City* city, Civilization* civilization)
    {
        auto* tuning = this;
        float vanilla = original_trade_acceptance(tuning, city, civilization);
        if (vanilla > 0.0f || !in_civilization())
            return vanilla;
        auto* origin = civilization;
        auto* partner = city_owner(city);
        if (!partner || partner == origin || origin->mIsPlayerOwned ||
            partner->mIsPlayerOwned || civilization_strategy(civilization) != kCivEconomicStrategy)
            return vanilla;

        void *context = simulation_context();
        void *manager = relationship_manager();
        Civilization* player = context ? find_player_civilization(context) : NULL;
        if (!manager || !player)
            return vanilla;

        uint32_t origin_id = political_id(origin);
        uint32_t partner_id = political_id(partner);
        if (origin_id == UINT32_MAX || partner_id == UINT32_MAX ||
            relationship_level(manager, origin_id, partner_id, 1) < kRelationshipNeutral ||
            relationship_level(manager, partner_id, origin_id, 1) < kRelationshipNeutral ||
            has_rival_trade(civilization, origin_id, political_id(player)) ||
            has_other_rival_trade_order(civilization, city))
            return vanilla;

        // The native selector and vehicle approach still require physical reachability.
        return 1.0f;
    }
};

static float __cdecl retreat_on_hit(Vehicle* vehicle)
{
    if (!vehicle->field_BCC)
        return 0.0f;

    if (vehicle->GetPurpose() == kVehicleEconomic && in_civilization()) {
        const VehicleOrder *order = current_vehicle_order(vehicle);
        if (order && order->target &&
            (order->kind == kTradeDeliveryOrder || order->kind == kCreateTradeRouteOrder))
            return 0.0f;
    }
    return 1.0f;
}

typedef struct {
    uintptr_t site;
    unsigned char expected[5];
    uintptr_t replacement;
    unsigned char *location;
    DWORD protection;
    int enabled;
} BranchSite;

static void write_branch(BranchSite *site)
{
    uint32_t displacement = (uint32_t)(site->replacement - (uintptr_t)site->location - 5);
    memcpy(site->location + 1, &displacement, sizeof displacement);
    // Use a tail jump for the predicate entry, keeping its cdecl caller and float result.
    if (site->site == kVehicleRetreatCondition)
        site->location[0] = 0xe9;
}

// These call sites are intentionally kept narrow. A function-wide Detours hook
// would affect additional callers and change v1.0 behavior (notably captures).
static int install_rules(unsigned char *base)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    IMAGE_NT_HEADERS32 *headers = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (headers->Signature != IMAGE_NT_SIGNATURE ||
        headers->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        headers->FileHeader.TimeDateStamp != kReferenceTimestamp ||
        (headers->OptionalHeader.SizeOfImage != kReferenceImageSize &&
         headers->OptionalHeader.SizeOfImage != kSteamOctober2024ImageSize))
        return 0;

    const int adjust_defenses = settings.turrets != TURRETS_VANILLA;
    const int extend_layout = settings.turrets == TURRETS_ALL_SPECIALTIES;
    BranchSite sites[] = {
        {0xb9bda0, {0xe8, 0x4b, 0xdc, 0xff, 0xff}, (uintptr_t)found_major_city, NULL, 0, 1},
        {0xbfb446, {0xe8, 0x45, 0x3b, 0x0f, 0x00}, (uintptr_t)trade_acceptance::detoured_function, NULL, 0, settings.assist_rival_trade},
        {0xbfb7bc, {0xe8, 0xcf, 0x37, 0x0f, 0x00}, (uintptr_t)trade_acceptance::detoured_function, NULL, 0, settings.assist_rival_trade},
        {0xdca391, {0xe8, 0xfa, 0x4b, 0xf2, 0xff}, (uintptr_t)trade_acceptance::detoured_function, NULL, 0, settings.assist_rival_trade},
        {kVehicleRetreatCondition, {0x8b, 0x44, 0x24, 0x04, 0x80}, (uintptr_t)retreat_on_hit, NULL, 0, settings.maintain_trade_under_fire},
        {0xbd8c67, {0xe8, 0x84, 0xff, 0xff, 0xff}, (uintptr_t)extended_turret_limit::detoured_function, NULL, 0, extend_layout},
        {0xd07e1a, {0xe8, 0xd1, 0x0d, 0xed, 0xff}, (uintptr_t)extended_turret_limit::detoured_function, NULL, 0, extend_layout},
        {0xbe8c2c, {0xe8, 0xef, 0xe8, 0xff, 0xff}, (uintptr_t)retain_rival_city_specialty::detoured_function, NULL, 0, settings.keep_rival_specialty},
        {0xbe5c45, {0xe8, 0x16, 0x30, 0xff, 0xff}, (uintptr_t)defensive_capacity_target::detoured_function, NULL, 0, adjust_defenses},
        {0xbe5cc4, {0xe8, 0x37, 0x40, 0xff, 0xff}, (uintptr_t)defensive_building_count::detoured_function, NULL, 0, adjust_defenses},
        {0xbe8e51, {0xe8, 0xca, 0xe6, 0xff, 0xff}, (uintptr_t)retain_rival_city_specialty::detoured_function, NULL, 0, settings.keep_rival_specialty},
        // Use the same fleet limit for AI production, player purchases, and UI reads.
        {0xbd7325, {0xe8, 0xe6, 0xad, 0x01, 0x00}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xbf228a, {0xe8, 0x81, 0xfe, 0xff, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xbf686f, {0xe8, 0x9c, 0xb8, 0xff, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xbf6afa, {0xe8, 0x11, 0xb6, 0xff, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xd07c73, {0xe8, 0x98, 0xa4, 0xee, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xd1df57, {0xe8, 0xb4, 0x41, 0xed, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xd1df73, {0xe8, 0x98, 0x41, 0xed, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xd1df93, {0xe8, 0x78, 0x41, 0xed, 0xff}, (uintptr_t)configured_fleet_capacity::detoured_function, NULL, 0, adjust_defenses},
        {0xbf2342, {0xe8, 0xe9, 0xc6, 0x82, 0xff}, (uintptr_t)configured_price::detoured_function, NULL, 0, settings.cost_percent != 100},
        {0xca68b1, {0xe8, 0x7a, 0x81, 0x77, 0xff}, (uintptr_t)configured_firing_interval::detoured_function, NULL, 0, settings.firing_interval_percent != 100},
        // Loading a scenario and founding a fresh Civilization planet read the same ranges.
        {0xb96b86, {0xe8, 0xc5, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_population()},
        {0xb96ba1, {0xe8, 0xaa, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_population()},
        {0xb9da81, {0xe8, 0xca, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_population()},
        {0xb9daa1, {0xe8, 0xaa, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_population()},
        {0xb96bbc, {0xe8, 0x8f, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_wealth()},
        {0xb96bd0, {0xe8, 0x7b, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_wealth()},
        {0xb9dac1, {0xe8, 0x8a, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_wealth()},
        {0xb9dad8, {0xe8, 0x73, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation::detoured_function, NULL, 0, custom_wealth()},
        {0xb97142, {0xe8, 0x09, 0xb5, 0xc5, 0xff}, (uintptr_t)configured_starting_vehicles::detoured_function, NULL, 0, settings.offensive_vehicles != 8},
    };
    const size_t count = sizeof sites / sizeof sites[0];
    const unsigned char native_retreat_condition[] = {
        0x8b, 0x44, 0x24, 0x04, 0x80, 0xb8, 0xcc, 0x0b, 0x00, 0x00,
        0x00, 0x74, 0x03, 0xd9, 0xe8, 0xc3, 0xd9, 0xee, 0xc3,
    };
    if (settings.maintain_trade_under_fire &&
        memcmp(base + (kVehicleRetreatCondition - kReferenceBase), native_retreat_condition,
               sizeof native_retreat_condition) != 0)
        return 0;

    for (size_t i = 0; i < count; ++i) {
        if (!sites[i].enabled)
            continue;
        sites[i].location = base + (sites[i].site - kReferenceBase);
        if (memcmp(sites[i].location, sites[i].expected, 5) != 0)
            return 0;
    }

    read_script_float = (ReadScriptFloat)(base + (0x7f2650 - kReferenceBase));
    read_float_property = (ReadFloatProperty)(base + (0x41ea30 - kReferenceBase));
    random_below = (RandomBelow)(base + (0xa68f70 - kReferenceBase));
    simulation_random = base + (0x1601760 - kReferenceBase);
    original_found_city = (FoundCityFunction)(base + (0xb999f0 - kReferenceBase));
    original_turret_limit = (TurretLimitFunction)(base + (0xbd8bf0 - kReferenceBase));
    original_set_specialty = (SetSpecialtyFunction)(base + (0xbe7520 - kReferenceBase));
    original_building_count = (CityCountFunction)(base + (0xbd9d00 - kReferenceBase));
    allowed_turret_count = (CityCountFunction)(base + (0xbd8c60 - kReferenceBase));
    original_fleet_capacity = (FleetCapacityFunction)(base + (0xbf2110 - kReferenceBase));
    simulation_context = (SimulationContextFunction)(base + (0xb3d400 - kReferenceBase));
    scenario_settings = (ScenarioSettingsFunction)(base + (0xf67d40 - kReferenceBase));
    game_difficulty = (DifficultyFunction)(base + (0xc755c0 - kReferenceBase));
    original_trade_acceptance = (TradeAcceptance)(base + (0xceef90 - kReferenceBase));
    city_owner = (CityOwner)(base + (0xbd9d40 - kReferenceBase));
    civilization_strategy = (CivilizationStrategy)(base + (0xbf1080 - kReferenceBase));
    relationship_manager = (SimulationContextFunction)(base + (0xb3d3c0 - kReferenceBase));
    relationship_level = (RelationshipLevel)(base + (0xd00bd0 - kReferenceBase));
    find_player_civilization = (FindPlayerCivilization)(base + (0xb25f90 - kReferenceBase));
    current_vehicle_order = (CurrentVehicleOrder)(base + (0xca7360 - kReferenceBase));
    game_mode = (ModeFunction)(base + (0xb5b910 - kReferenceBase));

    size_t writable = 0;
    while (writable < count) {
        BranchSite *site = &sites[writable];
        if (!site->enabled) {
            ++writable;
            continue;
        }
        if (!VirtualProtect(site->location, 5, PAGE_EXECUTE_READWRITE, &site->protection))
            break;
        ++writable;
    }

    const int ready = writable == count;
    if (ready) {
        for (size_t i = 0; i < count; ++i) {
            if (!sites[i].enabled)
                continue;
            write_branch(&sites[i]);
            FlushInstructionCache(GetCurrentProcess(), sites[i].location, 5);
        }
    }

    // Restore in reverse order, including when multiple targets share a page.
    while (writable) {
        BranchSite *site = &sites[--writable];
        if (!site->enabled)
            continue;
        DWORD previous;
        VirtualProtect(site->location, 5, site->protection, &previous);
    }
    return ready;
}

void Initialize(HINSTANCE module)
{
    mod_instance = module;
    load_options();
    const bool active = install_rules(reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))) != 0;
    ModAPI::Log(active ? "[Better Civ] C++/SDK city rules active."
                      : "[Better Civ] Rules inactive: unsupported executable or conflicting patch.");
}

} // namespace BetterCiv
