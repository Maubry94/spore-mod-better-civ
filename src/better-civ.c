#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

enum {
    kReferenceBase = 0x400000,
    kReferenceTimestamp = 1727737552,
    kReferenceImageSize = 26271744,
    kSteamOctober2024ImageSize = 26443776,
    kCivilizationMode = 0x01654c04,
    kSiteMinorNation = 0x25,
    kSiteSpecialty = 0x28,
    kCityIsPlayerOwned = 0x33c,
    kMilitary = 0,
    kReligious = 1,
    kEconomic = 2,
    kMilitaryMask = 1 << kMilitary,
    kReligiousMask = 1 << kReligious,
    kEconomicMask = 1 << kEconomic,
    kAllSpecialties = kMilitaryMask | kReligiousMask | kEconomicMask,
    kTurretSlotsBegin = 0x504,
    kTurretSlotsEnd = 0x508,
    kTurretSlotSize = 0x18,
    kMilitaryLargeTurretCapacity = 8,
    kNativeFleetLimit = 36,
    kDifficultyEasy = 0,
    kDifficultyNormal = 1,
    kDifficultyHard = 2,
    kCivIsPlayerOwned = 0x89,
    kCivCitiesBegin = 0x9c,
    kCivCitiesEnd = 0xa0,
    kCivVehiclesBegin = 0xb0,
    kCivVehiclesEnd = 0xb4,
    kCityTradePartnersBegin = 0x55c,
    kCityTradePartnersEnd = 0x560,
    kRelationshipNeutral = 2,
    kTradeDeliveryOrder = 5,
    kCreateTradeRouteOrder = 9,
    kCivEconomicStrategy = 1,
    kVehiclePurpose = 0xb20,
    kVehicleHitFlag = 0xbcc,
    kVehicleRetreatCondition = 0xdc47e0,
};

typedef uint32_t (__cdecl *ModeFunction)(void);
typedef void *(__cdecl *FoundCityFunction)(unsigned char *site);
typedef int (__attribute__((thiscall)) *TurretLimitFunction)(unsigned char *city);
typedef void (__attribute__((thiscall)) *SetSpecialtyFunction)(unsigned char *city, int specialty, int updateWalls);

static ModeFunction game_mode;
static FoundCityFunction original_found_city;
static TurretLimitFunction original_turret_limit;
static SetSpecialtyFunction original_set_specialty;
typedef int (__attribute__((thiscall)) *CityCountFunction)(unsigned char *city);
typedef int (__attribute__((thiscall)) *FleetCapacityFunction)(void *civilization, int specialty);
typedef void *(__cdecl *SimulationContextFunction)(void);
typedef void *(__attribute__((thiscall)) *ScenarioSettingsFunction)(void *context);
typedef int (__attribute__((thiscall)) *DifficultyFunction)(void *settings);

static CityCountFunction original_building_count;
static CityCountFunction allowed_turret_count;
static FleetCapacityFunction original_fleet_capacity;
static SimulationContextFunction simulation_context;
static ScenarioSettingsFunction scenario_settings;
static DifficultyFunction game_difficulty;

typedef float (__attribute__((thiscall)) *ReadScriptFloat)(void *, const char *);
typedef const float *(__attribute__((thiscall)) *ReadFloatProperty)(void *);
typedef unsigned (__attribute__((thiscall)) *RandomBelow)(void *, unsigned);
typedef float (__attribute__((thiscall)) *TradeAcceptance)(void *, void *city, void *civilization);
typedef void *(__attribute__((thiscall)) *CityOwner)(void *city);
typedef int (__attribute__((thiscall)) *CivilizationStrategy)(void *civilization);
typedef uint32_t (__attribute__((thiscall)) *PoliticalId)(const void *entity);
typedef int (__attribute__((thiscall)) *RelationshipLevel)(void *, uint32_t, uint32_t, int);
typedef void *(__attribute__((thiscall)) *FindPlayerCivilization)(void *);

typedef struct {
    uint32_t political_id;
    void *route;
    int type;
} TradePartner;

typedef struct {
    void *target;
    unsigned char position[12];
    int kind;
    uint32_t political_id;
} VehicleOrder;

typedef const VehicleOrder *(__attribute__((thiscall)) *CurrentVehicleOrder)(void *);
_Static_assert(sizeof(TradePartner) == 12 && sizeof(VehicleOrder) == 24, "32-bit game layout");

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

static int matches_choice(const char *line, const char *current, const char *legacy)
{
    // The v1.0 installer writes the original identifiers into ModSettings.
    return strcmp(line, current) == 0 || strcmp(line, legacy) == 0;
}

static void apply_option(Settings *out, const char *line)
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

static const float *__attribute__((thiscall)) configured_price(void *property)
{
    const float *vanilla = read_float_property(property);
    static __thread float value;
    if (!vanilla || settings.cost_percent == 100 || !in_civilization())
        return vanilla;
    value = *vanilla * (float)settings.cost_percent / 100.0f;
    return &value;
}

static const float *__attribute__((thiscall)) configured_firing_interval(void *property)
{
    const float *vanilla = read_float_property(property);
    static __thread float value;
    if (!vanilla || settings.firing_interval_percent == 100 || !in_civilization())
        return vanilla;
    value = *vanilla * (float)settings.firing_interval_percent / 100.0f;
    return &value;
}

static int custom_population(void)
{
    return settings.population_min != 15 || settings.population_max != 15;
}

static int custom_wealth(void)
{
    return settings.wealth_min != 4000 || settings.wealth_max != 4000;
}

static float __attribute__((thiscall)) configured_civ_generation(void *script, const char *name)
{
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

static float __attribute__((thiscall)) configured_starting_vehicles(void *script, const char *name)
{
    if (strcmp(name, "num_offensive_vehicles") == 0) return (float)settings.offensive_vehicles;
    return read_script_float(script, name);
}



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
            case kDifficultyNormal: vanilla = 9; configured = 12; break;
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

static int __attribute__((thiscall)) configured_fleet_capacity(void *civilization, int specialty)
{
    int native = original_fleet_capacity(civilization, specialty);
    if (specialty < kMilitary || specialty > kEconomic)
        return native;
    return fleet_capacity_with_defenses(native, settings.turrets, civilization_difficulty());
}

static int __attribute__((thiscall)) defensive_building_count(unsigned char *city)
{
    if (civilization_difficulty() == kDifficultyHard) {
        // The AI uses (count + 1) / 2 for this independent construction target.
        int capacity = allowed_turret_count(city);
        if (capacity > 0 && capacity <= kMilitaryLargeTurretCapacity)
            return capacity * 2 - 1;
    }
    return original_building_count(city);
}

static int capacity_in_layout(uintptr_t begin, uintptr_t end, int original_limit)
{
    if (!begin || end < begin || (end - begin) % kTurretSlotSize)
        return original_limit;

    uintptr_t slots = (end - begin) / kTurretSlotSize;
    if (slots <= (uintptr_t)original_limit)
        return original_limit;
    return slots < kMilitaryLargeTurretCapacity ? (int)slots : kMilitaryLargeTurretCapacity;
}

static void *__cdecl found_major_city(unsigned char *site)
{
    if (site && in_civilization()) {
        site[kSiteMinorNation] = 0;
        if (settings.specialty_mask != kAllSpecialties) {
            int candidates[3];
            unsigned count = 0;
            for (int specialty = kMilitary; specialty <= kEconomic; ++specialty) {
                if (settings.specialty_mask & (1u << specialty))
                    candidates[count++] = specialty;
            }
            if (count) {
                unsigned index = count == 1 ? 0 : random_below(simulation_random, count);
                memcpy(site + kSiteSpecialty, &candidates[index], sizeof candidates[index]);
            }
        }
    }
    return original_found_city(site);
}

static int __attribute__((thiscall)) extended_turret_limit(unsigned char *city)
{
    int limit = original_turret_limit(city);
    if (!in_civilization() || limit <= 0)
        return limit;

    uintptr_t begin, end;
    memcpy(&begin, city + kTurretSlotsBegin, sizeof begin);
    memcpy(&end, city + kTurretSlotsEnd, sizeof end);
    return capacity_in_layout(begin, end, limit);
}

static int __attribute__((thiscall)) defensive_capacity_target(unsigned char *city)
{
    int capacity = allowed_turret_count(city);
    // The Hard branch takes floor(4 * target / 5) before checking its remaining budget.
    if (capacity > 0 && capacity <= kMilitaryLargeTurretCapacity &&
        civilization_difficulty() == kDifficultyHard)
        return (capacity * 5 + 3) / 4;
    return capacity;
}

static void __attribute__((thiscall)) retain_rival_city_specialty(
    unsigned char *city, int specialty, int update_walls)
{
    if (game_mode() != kCivilizationMode || city[kCityIsPlayerOwned])
        original_set_specialty(city, specialty, update_walls);
}

static const void *read_pointer(const void *object, size_t offset)
{
    const void *value;
    memcpy(&value, (const unsigned char *)object + offset, sizeof value);
    return value;
}

static uint32_t political_id(const void *entity)
{
    const void *vtable = read_pointer(entity, 0);
    PoliticalId function;
    memcpy(&function, (const unsigned char *)vtable + 0x4c, sizeof function);
    return function(entity);
}

static int has_rival_trade(void *civilization, uint32_t own_id, uint32_t player_id)
{
    void *const *cities = read_pointer(civilization, kCivCitiesBegin);
    void *const *end = read_pointer(civilization, kCivCitiesEnd);
    for (; cities != end; ++cities) {
        if (!*cities)
            continue;
        const TradePartner *partner = read_pointer(*cities, kCityTradePartnersBegin);
        const TradePartner *limit = read_pointer(*cities, kCityTradePartnersEnd);
        for (; partner != limit; ++partner) {
            if (partner->political_id != own_id && partner->political_id != player_id &&
                partner->political_id != UINT32_MAX)
                return 1;
        }
    }
    return 0;
}

static int has_other_rival_trade_order(void *civilization, void *city)
{
    void *const *vehicles = read_pointer(civilization, kCivVehiclesBegin);
    void *const *end = read_pointer(civilization, kCivVehiclesEnd);
    for (; vehicles != end; ++vehicles) {
        if (!*vehicles)
            continue;
        const VehicleOrder *order = current_vehicle_order(*vehicles);
        if (!order || order->kind != kCreateTradeRouteOrder || !order->target ||
            order->target == city)
            continue;
        // For order 9 the target is a cCity. Player orders do not reserve a rival partner.
        const unsigned char *owner = city_owner(order->target);
        if (owner && owner != civilization && !owner[kCivIsPlayerOwned])
            return 1;
    }
    return 0;
}

static float __attribute__((thiscall)) trade_acceptance(
    void *tuning, void *city, void *civilization)
{
    float vanilla = original_trade_acceptance(tuning, city, civilization);
    if (vanilla > 0.0f || !in_civilization())
        return vanilla;
    const unsigned char *origin = civilization;
    const unsigned char *partner = city_owner(city);
    if (!partner || partner == origin || origin[kCivIsPlayerOwned] ||
        partner[kCivIsPlayerOwned] || civilization_strategy(civilization) != kCivEconomicStrategy)
        return vanilla;

    void *context = simulation_context();
    void *manager = relationship_manager();
    const void *player = context ? find_player_civilization(context) : NULL;
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

static float __cdecl retreat_on_hit(void *vehicle)
{
    const unsigned char *bytes = vehicle;
    if (!bytes[kVehicleHitFlag])
        return 0.0f;

    uint32_t purpose;
    memcpy(&purpose, bytes + kVehiclePurpose, sizeof purpose);
    if (purpose == kEconomic && in_civilization()) {
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
        {0xbfb446, {0xe8, 0x45, 0x3b, 0x0f, 0x00}, (uintptr_t)trade_acceptance, NULL, 0, settings.assist_rival_trade},
        {0xbfb7bc, {0xe8, 0xcf, 0x37, 0x0f, 0x00}, (uintptr_t)trade_acceptance, NULL, 0, settings.assist_rival_trade},
        {0xdca391, {0xe8, 0xfa, 0x4b, 0xf2, 0xff}, (uintptr_t)trade_acceptance, NULL, 0, settings.assist_rival_trade},
        {kVehicleRetreatCondition, {0x8b, 0x44, 0x24, 0x04, 0x80}, (uintptr_t)retreat_on_hit, NULL, 0, settings.maintain_trade_under_fire},
        {0xbd8c67, {0xe8, 0x84, 0xff, 0xff, 0xff}, (uintptr_t)extended_turret_limit, NULL, 0, extend_layout},
        {0xd07e1a, {0xe8, 0xd1, 0x0d, 0xed, 0xff}, (uintptr_t)extended_turret_limit, NULL, 0, extend_layout},
        {0xbe8c2c, {0xe8, 0xef, 0xe8, 0xff, 0xff}, (uintptr_t)retain_rival_city_specialty, NULL, 0, settings.keep_rival_specialty},
        {0xbe5c45, {0xe8, 0x16, 0x30, 0xff, 0xff}, (uintptr_t)defensive_capacity_target, NULL, 0, adjust_defenses},
        {0xbe5cc4, {0xe8, 0x37, 0x40, 0xff, 0xff}, (uintptr_t)defensive_building_count, NULL, 0, adjust_defenses},
        {0xbe8e51, {0xe8, 0xca, 0xe6, 0xff, 0xff}, (uintptr_t)retain_rival_city_specialty, NULL, 0, settings.keep_rival_specialty},
        // Use the same fleet limit for AI production, player purchases, and UI reads.
        {0xbd7325, {0xe8, 0xe6, 0xad, 0x01, 0x00}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xbf228a, {0xe8, 0x81, 0xfe, 0xff, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xbf686f, {0xe8, 0x9c, 0xb8, 0xff, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xbf6afa, {0xe8, 0x11, 0xb6, 0xff, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xd07c73, {0xe8, 0x98, 0xa4, 0xee, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xd1df57, {0xe8, 0xb4, 0x41, 0xed, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xd1df73, {0xe8, 0x98, 0x41, 0xed, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xd1df93, {0xe8, 0x78, 0x41, 0xed, 0xff}, (uintptr_t)configured_fleet_capacity, NULL, 0, adjust_defenses},
        {0xbf2342, {0xe8, 0xe9, 0xc6, 0x82, 0xff}, (uintptr_t)configured_price, NULL, 0, settings.cost_percent != 100},
        {0xca68b1, {0xe8, 0x7a, 0x81, 0x77, 0xff}, (uintptr_t)configured_firing_interval, NULL, 0, settings.firing_interval_percent != 100},
        // Loading a scenario and founding a fresh Civilization planet read the same ranges.
        {0xb96b86, {0xe8, 0xc5, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_population()},
        {0xb96ba1, {0xe8, 0xaa, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_population()},
        {0xb9da81, {0xe8, 0xca, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_population()},
        {0xb9daa1, {0xe8, 0xaa, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_population()},
        {0xb96bbc, {0xe8, 0x8f, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_wealth()},
        {0xb96bd0, {0xe8, 0x7b, 0xba, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_wealth()},
        {0xb9dac1, {0xe8, 0x8a, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_wealth()},
        {0xb9dad8, {0xe8, 0x73, 0x4b, 0xc5, 0xff}, (uintptr_t)configured_civ_generation, NULL, 0, custom_wealth()},
        {0xb97142, {0xe8, 0x09, 0xb5, 0xc5, 0xff}, (uintptr_t)configured_starting_vehicles, NULL, 0, settings.offensive_vehicles != 8},
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

static void __cdecl initialize(void)
{
    load_options();
    int active = install_rules((unsigned char *)GetModuleHandleW(NULL));
    OutputDebugStringA(active ? "[Better Civ] Configurable city rules active."
                              : "[Better Civ] Major city rules inactive: game build or existing patch differs.");
}

typedef void (__cdecl *RegisterPostInit)(void (__cdecl *initialize_callback)(void));

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        mod_instance = instance;
        HMODULE api = GetModuleHandleW(L"SporeModAPI.dll");
        FARPROC entry = api ? GetProcAddress(api, "?AddPostInitFunction@ModAPI@@YAXP6AXXZ@Z") : NULL;
        RegisterPostInit register_post_init;
        memcpy(&register_post_init, &entry, sizeof register_post_init);
        if (register_post_init)
            register_post_init(initialize);
        else
            OutputDebugStringA("[Better Civ] ModAPI post-init callback is unavailable.");
    }
    return TRUE;
}
