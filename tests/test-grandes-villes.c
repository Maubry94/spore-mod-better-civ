#include "grandes-villes.c"
#include <assert.h>
#include <stdio.h>

static unsigned current_mode;
static unsigned char observed_status;
static int observed_site_specialty;
static unsigned mock_draw;
static unsigned number_of_draws;
static int specialty_changes;
static int last_specialty;
static int difficulty = 2;
static int __attribute__((thiscall)) mock_building_count(unsigned char *city) { (void)city; return 3; }
static int __attribute__((thiscall)) mock_allowed_turret_count(unsigned char *city) { (void)city; return 8; }
static void *__cdecl mock_context(void) { return &difficulty; }
static void *__attribute__((thiscall)) mock_settings(void *context) { return context; }
static int __attribute__((thiscall)) mock_difficulty(void *settings) { return *(int *)settings; }

static uint32_t __cdecl mock_mode(void) { return current_mode; }
static void *__cdecl mock_found(unsigned char *site) {
    observed_status = site[kSiteMinorNation];
    memcpy(&observed_site_specialty, site + kSiteSpecialty, sizeof observed_site_specialty);
    return site;
}
static unsigned __attribute__((thiscall)) mock_random(void *state, unsigned bound) {
    assert(state == &mock_draw && bound == 2 && mock_draw < bound);
    ++number_of_draws;
    return mock_draw;
}
static int __attribute__((thiscall)) mock_limit(unsigned char *city) {
    (void)city;
    return 4;
}
static void __attribute__((thiscall)) mock_specialty(unsigned char *city, int kind, int walls) {
    (void)city;
    (void)walls;
    ++specialty_changes;
    last_specialty = kind;
}



static const uint32_t optional_sites[] = {
    0xbf2342, 0xca68b1, 0xb96b86, 0xb96ba1, 0xb9da81, 0xb9daa1,
    0xb96bbc, 0xb96bd0, 0xb9dac1, 0xb9dad8, 0xb97142,
};

static const uintptr_t optional_targets[] = {
    (uintptr_t)configured_price, (uintptr_t)configured_firing_interval,
    (uintptr_t)configured_civ_generation, (uintptr_t)configured_civ_generation,
    (uintptr_t)configured_civ_generation, (uintptr_t)configured_civ_generation,
    (uintptr_t)configured_civ_generation, (uintptr_t)configured_civ_generation,
    (uintptr_t)configured_civ_generation, (uintptr_t)configured_civ_generation,
    (uintptr_t)configured_starting_vehicles,
};

static const Settings defaults = {
    100, 100, 8, 15, 15, 4000, 4000, kAllSpecialties, 0, TURRETS_VANILLA, 1, 1,
};
static const Settings mixed = {
    75, 25, 12, 30, 30, 4000, 15000, kMilitaryMask | kEconomicMask, 1, TURRETS_ALL_SPECIALTIES, 0, 0,
};

static const float *__attribute__((thiscall)) mock_read_float(void *property)
{
    return property;
}

static int __attribute__((thiscall)) mock_fleet_capacity(void *civilization, int specialty)
{
    (void)specialty;
    return *(int *)civilization;
}

static float __attribute__((thiscall)) mock_read_script(void *script, const char *name)
{
    (void)name;
    return *(const float *)script;
}

static void verify_settings(void)
{
    settings = defaults;
    apply_option(&settings, "<Prix>PrixVanilla");
    apply_option(&settings, "<Offensifs>OffensifsVanilla");
    apply_option(&settings, "<Flotte>FlotteInconnu");
    apply_option(&settings, "<Prix>PrixReduitsTrailing");
    apply_option(&settings, "<Capture>CaptureVanilla");
    assert(memcmp(&settings, &defaults, sizeof settings) == 0);
    apply_option(&settings, "<Population>PopulationVariable");
    assert(settings.population_min == 15 && settings.population_max == 50);
    assert(settings.wealth_min == 4000 && settings.wealth_max == 4000);

    // Load the real installer settings format using the DLL-relative directory layout.
    settings = defaults;
    mod_instance = GetModuleHandleW(NULL);
    load_options();
    assert(memcmp(&settings, &mixed, sizeof settings) == 0);

    read_float_property = mock_read_float;
    read_script_float = mock_read_script;
    float cost = 1000.0f;
    float firing_interval = 2.0f;
    current_mode = kCivilizationMode;
    assert(*configured_price(&cost) == 750.0f);
    assert(*configured_firing_interval(&firing_interval) == 0.5f);
    assert(configured_starting_vehicles(&cost, "num_offensive_vehicles") == 12.0f);
    assert(configured_starting_vehicles(&cost, "num_harvesters") == cost);
    assert(configured_civ_generation(&cost, "population_min") == 30.0f);
    assert(configured_civ_generation(&cost, "population_max") == 30.0f);
    assert(configured_civ_generation(&cost, "wealth_min") == 4000.0f);
    assert(configured_civ_generation(&cost, "wealth_max") == 15000.0f);
    settings.wealth_min = settings.wealth_max = 4000;
    assert(configured_civ_generation(&cost, "wealth_min") == cost);
    current_mode = 0;
    assert(configured_price(&cost) == &cost);
    assert(configured_firing_interval(&firing_interval) == &firing_interval);
    settings = defaults;
}

static void verify_fleet_capacity(void)
{
    original_fleet_capacity = mock_fleet_capacity;
    const TurretMode modes[] = {TURRETS_VANILLA, TURRETS_HARD_MAXIMUM, TURRETS_ALL_SPECIALTIES};
    const int expected[][3] = {{4, 4, 4}, {4, 4, 6}, {6, 5, 7}};
    int native = 4;
    for (size_t mode = 0; mode < sizeof modes / sizeof modes[0]; ++mode) {
        settings.turrets = modes[mode];
        current_mode = kCivilizationMode;
        for (difficulty = kDifficultyEasy; difficulty <= kDifficultyHard; ++difficulty) {
            for (int specialty = kMilitary; specialty <= kEconomic; ++specialty)
                assert(configured_fleet_capacity(&native, specialty) == expected[mode][difficulty]);
            assert(configured_fleet_capacity(&native, 4) == native);
        }
        current_mode = 0;
        assert(configured_fleet_capacity(&native, kMilitary) == native);
    }
    assert(fleet_capacity_with_defenses(0, TURRETS_ALL_SPECIALTIES, kDifficultyHard) == 0);
    assert(fleet_capacity_with_defenses(24, TURRETS_ALL_SPECIALTIES, kDifficultyHard) == 36);
    assert(fleet_capacity_with_defenses(36, TURRETS_ALL_SPECIALTIES, kDifficultyHard) == 36);
    difficulty = kDifficultyHard;
    current_mode = kCivilizationMode;
    settings = defaults;
}

static void verify_foundation(unsigned char *site)
{
    const char *options[] = {
        "TroisSpecialites", "SansMilitaires", "SansReligieuses", "SansEconomiques",
        "MilitairesSeulement", "ReligieusesSeulement", "EconomiquesSeulement",
    };
    const int specialties[][2] = {
        {kMilitary, kMilitary}, {kReligious, kEconomic}, {kMilitary, kEconomic},
        {kMilitary, kReligious}, {kMilitary, kMilitary}, {kReligious, kReligious},
        {kEconomic, kEconomic},
    };
    random_below = mock_random;
    simulation_random = &mock_draw;
    for (size_t i = 0; i < sizeof options / sizeof options[0]; ++i) {
        char line[80];
        snprintf(line, sizeof line, "<Specialites>%s", options[i]);
        settings = defaults;
        apply_option(&settings, line);
        for (mock_draw = 0; mock_draw < 2; ++mock_draw) {
            int native = kMilitary;
            memcpy(site + kSiteSpecialty, &native, sizeof native);
            current_mode = 0;
            site[kSiteMinorNation] = 1;
            assert(found_major_city(site) == site && observed_status == 1);
            assert(observed_site_specialty == native);
            current_mode = kCivilizationMode;
            assert(found_major_city(site) == site && observed_status == 0);
            assert(observed_site_specialty == specialties[i][mock_draw]);
        }
    }
    assert(number_of_draws == 6);
    settings = defaults;
}

static unsigned char trade_player[0xa4];
static int trade_strategy;
static int partner_opinion = 2;
static int origin_opinion = 2;
static float native_trade;

static float __attribute__((thiscall)) mock_trade(void *tuning, void *city, void *civ)
{ (void)tuning; (void)city; (void)civ; return native_trade; }
static void *__attribute__((thiscall)) mock_city_owner(void *city)
{ return (void *)read_pointer(city, 0x590); }
static int __attribute__((thiscall)) mock_strategy(void *civ)
{ (void)civ; return trade_strategy; }
static const VehicleOrder *__attribute__((thiscall)) mock_order(void *vehicle)
{ return vehicle; }
static void *__attribute__((thiscall)) mock_player(void *context)
{ (void)context; return trade_player; }
static uint32_t __attribute__((thiscall)) mock_political_id(const void *civ)
{ return ((const unsigned char *)civ)[0x24]; }
static int __attribute__((thiscall)) mock_relationship(void *manager, uint32_t from, uint32_t to, int fresh)
{
    assert(manager && fresh && ((from == 1 && to == 2) || (from == 2 && to == 1)));
    return from == 1 ? origin_opinion : partner_opinion;
}

static void put_pointer(void *object, size_t offset, const void *value)
{
    memcpy((unsigned char *)object + offset, &value, sizeof value);
}

static VehicleOrder trade_order;

static const VehicleOrder *__attribute__((thiscall)) mock_trade_order(void *vehicle)
{
    (void)vehicle;
    return &trade_order;
}

static void verify_trade_under_fire(float (__cdecl *predicate)(void *))
{
    unsigned char vehicle[kVehicleHitFlag + 1] = {0};
    const size_t locomotion_offset = 0xb1c;
    const uint32_t locomotions[] = {0, 1, 2};  // land, water, air
    const uint32_t economic = kEconomic;
    const uint32_t military = kMilitary;
    game_mode = mock_mode;
    current_vehicle_order = mock_trade_order;
    current_mode = kCivilizationMode;
    trade_order.target = vehicle;
    trade_order.kind = kTradeDeliveryOrder;
    memcpy(vehicle + kVehiclePurpose, &economic, sizeof economic);
    assert(predicate(vehicle) == 0.0f);
    vehicle[kVehicleHitFlag] = 1;
    for (size_t i = 0; i < sizeof locomotions / sizeof locomotions[0]; ++i) {
        memcpy(vehicle + locomotion_offset, &locomotions[i], sizeof locomotions[i]);
        trade_order.kind = kTradeDeliveryOrder;
        assert(predicate(vehicle) == 0.0f);
        trade_order.kind = kCreateTradeRouteOrder;
        assert(predicate(vehicle) == 0.0f);
    }
    trade_order.target = NULL;
    assert(predicate(vehicle) == 1.0f);
    trade_order.target = vehicle;
    trade_order.kind = 1;  // movement without a trade order
    assert(predicate(vehicle) == 1.0f);
    trade_order.kind = kTradeDeliveryOrder;
    memcpy(vehicle + kVehiclePurpose, &military, sizeof military);
    assert(predicate(vehicle) == 1.0f);
    memcpy(vehicle + kVehiclePurpose, &economic, sizeof economic);
    current_mode = 0x01654c05;
    assert(predicate(vehicle) == 1.0f);
}

static void verify_rival_trade(void)
{
    unsigned char origin[0xb8] = {0}, partner[0xa4] = {0};
    unsigned char home[0x594] = {0}, target[0x594] = {0}, other[0x594] = {0};
    unsigned char vtable[0x50] = {0};
    PoliticalId getter = mock_political_id;
    memcpy(vtable + 0x4c, &getter, sizeof getter);
    put_pointer(origin, 0, vtable);
    put_pointer(partner, 0, vtable);
    put_pointer(trade_player, 0, vtable);
    put_pointer(target, 0x590, partner);
    put_pointer(other, 0x590, partner);
    origin[0x24] = 1;
    partner[0x24] = 2;
    trade_player[0x24] = 3;
    trade_player[kCivIsPlayerOwned] = 1;
    TradePartner route = {3, NULL, 0};
    put_pointer(home, kCityTradePartnersBegin, &route);
    put_pointer(home, kCityTradePartnersEnd, &route + 1);
    void *cities[] = {home};
    put_pointer(origin, kCivCitiesBegin, cities);
    put_pointer(origin, kCivCitiesEnd, cities + 1);
    VehicleOrder order = {0};
    void *vehicles[] = {&order};
    put_pointer(origin, kCivVehiclesBegin, vehicles);
    put_pointer(origin, kCivVehiclesEnd, vehicles + 1);

    original_trade_acceptance = mock_trade;
    city_owner = mock_city_owner;
    civilization_strategy = mock_strategy;
    relationship_manager = mock_context;
    relationship_level = mock_relationship;
    find_player_civilization = mock_player;
    current_vehicle_order = mock_order;
    trade_strategy = kCivEconomicStrategy;
    current_mode = kCivilizationMode;
    assert(trade_acceptance(NULL, target, origin) == 1.0f);
    current_mode = 0;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    current_mode = kCivilizationMode;
    order.kind = kCreateTradeRouteOrder;
    order.target = other;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    order.target = target;
    assert(trade_acceptance(NULL, target, origin) == 1.0f);
    route.political_id = 2;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    route.political_id = 3;
    partner_opinion = 1;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    partner_opinion = 2;
    origin_opinion = 0;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    origin_opinion = 2;
    trade_strategy = 0;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    trade_strategy = kCivEconomicStrategy;
    partner[kCivIsPlayerOwned] = 1;
    assert(trade_acceptance(NULL, target, origin) == 0.0f);
    native_trade = 0.6f;
    assert(trade_acceptance(NULL, target, origin) == 0.6f);
}

static void verify_reference_writes(const char *path, int variant)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    unsigned char *disk = HeapAlloc(GetProcessHeap(), 0, (size_t)size);
    assert(disk && fread(disk, 1, (size_t)size, file) == (size_t)size);
    fclose(file);

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)disk;
    IMAGE_NT_HEADERS32 *header = (IMAGE_NT_HEADERS32 *)(disk + dos->e_lfanew);
    unsigned char *image = VirtualAlloc(NULL, kSteamOctober2024ImageSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(image);
    memcpy(image, disk, header->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(header);
    for (int i = 0; i < header->FileHeader.NumberOfSections; ++i) {
        assert(section[i].PointerToRawData + section[i].SizeOfRawData <= (size_t)size);
        memcpy(image + section[i].VirtualAddress, disk + section[i].PointerToRawData,
               section[i].SizeOfRawData);
    }
    IMAGE_NT_HEADERS32 *image_header = (IMAGE_NT_HEADERS32 *)(image + dos->e_lfanew);
    unsigned char *founding = image + 0xb9bda0 - kReferenceBase;
    unsigned char *limit = image + 0xbd8c67 - kReferenceBase;
    unsigned char *editor = image + 0xd07e1a - kReferenceBase;
    unsigned char *capture = image + 0xbe8e51 - kReferenceBase;
    unsigned char *retreat = image + kVehicleRetreatCondition - kReferenceBase;

    const int use_options = variant == 1;
    settings = use_options ? mixed : defaults;
    if (variant == 2)
        apply_option(&settings, "<Tourelles>TourellesMaximum");
    unsigned char original_option_bytes[sizeof optional_sites / sizeof optional_sites[0]][5];
    for (size_t i = 0; i < sizeof optional_sites / sizeof optional_sites[0]; ++i)
        memcpy(original_option_bytes[i], image + optional_sites[i] - kReferenceBase, 5);
    unsigned char native_retreat[19];
    memcpy(native_retreat, retreat, sizeof native_retreat);
    if (settings.maintain_trade_under_fire) {
        retreat[6] ^= 1;
        assert(!install_rules(image) && founding[1] == 0x4b);
        retreat[6] ^= 1;
    }

    if (use_options) {
        unsigned char *last = image + optional_sites[sizeof optional_sites / sizeof optional_sites[0] - 1] -
                              kReferenceBase;
        last[4] ^= 1;
        assert(!install_rules(image) && founding[1] == 0x4b);
        last[4] ^= 1;
    }

    // Emulate the Steam PE header around the same code after initialization.
    image_header->OptionalHeader.SizeOfImage = kSteamOctober2024ImageSize;
    unsigned char *sites[] = {founding, limit, editor, capture,
      image + 0xbe8c2c - kReferenceBase, image + 0xbe5c45 - kReferenceBase, image + 0xbe5cc4 - kReferenceBase};
    uintptr_t targets[] = {(uintptr_t)found_major_city, (uintptr_t)extended_turret_limit,
                           (uintptr_t)extended_turret_limit, (uintptr_t)retain_rival_city_specialty,
                           (uintptr_t)retain_rival_city_specialty, (uintptr_t)defensive_capacity_target, (uintptr_t)defensive_building_count};
    int enabled[] = {1, variant == 1, variant == 1, variant == 1,
                     variant == 1, variant != 0, variant != 0};
    uint32_t fleet_sites[] = {0xbd7325, 0xbf228a, 0xbf686f, 0xbf6afa, 0xd07c73,
                             0xd1df57, 0xd1df73, 0xd1df93};
    unsigned char original_base_bytes[sizeof sites / sizeof sites[0]][5];
    for (size_t i = 0; i < sizeof sites / sizeof sites[0]; ++i)
        memcpy(original_base_bytes[i], sites[i], 5);
    if (enabled[3]) {
        capture[4] ^= 1;
        assert(!install_rules(image) && memcmp(founding, original_base_bytes[0], 5) == 0);
        capture[4] ^= 1;
    }
    DWORD old;
    assert(VirtualProtect(founding, 5, PAGE_EXECUTE_READ, &old));
    assert(VirtualProtect(retreat, sizeof native_retreat, PAGE_EXECUTE_READ, &old));
    assert(install_rules(image));
    if (settings.maintain_trade_under_fire) {
        uint32_t relative;
        memcpy(&relative, retreat + 1, sizeof relative);
        assert(retreat[0] == 0xe9 && (uintptr_t)retreat + 5 + relative == (uintptr_t)retreat_on_hit);
        assert(memcmp(retreat + 5, native_retreat + 5, sizeof native_retreat - 5) == 0);
        verify_trade_under_fire((float (__cdecl *)(void *))retreat);
    } else {
        assert(memcmp(retreat, native_retreat, sizeof native_retreat) == 0);
    }
    for (size_t i = 0; i != sizeof sites / sizeof sites[0]; ++i) {
        if (!enabled[i]) {
            assert(memcmp(sites[i], original_base_bytes[i], 5) == 0);
            continue;
        }
        int32_t relative;
        memcpy(&relative, sites[i] + 1, sizeof relative);
        assert(sites[i][0] == 0xe8 && (uintptr_t)sites[i] + 5 + (uint32_t)relative == targets[i]);
    }
    for (size_t i = 0; i < sizeof optional_sites / sizeof optional_sites[0]; ++i) {
        unsigned char *site = image + optional_sites[i] - kReferenceBase;
        if (use_options) {
            uint32_t relative;
            memcpy(&relative, site + 1, 4);
            assert(site[0] == 0xe8 && (uintptr_t)site + 5 + relative == optional_targets[i]);
        } else {
            assert(memcmp(site, original_option_bytes[i], 5) == 0);
        }
    }
    for (size_t i = 0; i < sizeof fleet_sites / sizeof fleet_sites[0]; ++i) {
        unsigned char *site = image + fleet_sites[i] - kReferenceBase;
        uint32_t relative;
        memcpy(&relative, site + 1, 4);
        uintptr_t target = variant == 0 ? (uintptr_t)(image + 0xbf2110 - kReferenceBase)
                                        : (uintptr_t)configured_fleet_capacity;
        assert(site[0] == 0xe8 && (uintptr_t)site + 5 + relative == target);
    }
    const uint32_t commerce_sites[] = {0xbfb446, 0xbfb7bc, 0xdca391};
    for (size_t i = 0; i < sizeof commerce_sites / sizeof commerce_sites[0]; ++i) {
        unsigned char *site = image + commerce_sites[i] - kReferenceBase;
        uint32_t relative;
        memcpy(&relative, site + 1, 4);
        uintptr_t target = use_options ? (uintptr_t)(image + 0xceef90 - kReferenceBase)
                                       : (uintptr_t)trade_acceptance;
        assert(site[0] == 0xe8 && (uintptr_t)site + 5 + relative == target);
    }
    MEMORY_BASIC_INFORMATION page;
    assert(VirtualQuery(founding, &page, sizeof page) && page.Protect == PAGE_EXECUTE_READ);
    assert(VirtualQuery(retreat, &page, sizeof page) && page.Protect == PAGE_EXECUTE_READ);
    VirtualFree(image, 0, MEM_RELEASE);
    HeapFree(GetProcessHeap(), 0, disk);
}

int main(int argc, char *argv[])
{
    unsigned char site[0x64] = {0};
    unsigned char city[0x818] = {0};
    game_mode = mock_mode;
    original_found_city = mock_found;
    original_turret_limit = mock_limit;
    original_set_specialty = mock_specialty;

    verify_foundation(site);

    uintptr_t start = 0x1000;
    uintptr_t end = start + 8 * kTurretSlotSize;
    memcpy(city + kTurretSlotsBegin, &start, sizeof start);
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(extended_turret_limit(city) == 8);
    end += kTurretSlotSize;
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(extended_turret_limit(city) == 8);
    end = start + 4 * kTurretSlotSize;
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(extended_turret_limit(city) == 4);
    end = start + 6 * kTurretSlotSize;
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(extended_turret_limit(city) == 6);

    retain_rival_city_specialty(city, 1, 0);
    assert(specialty_changes == 0);
    city[kCityIsPlayerOwned] = 1;
    retain_rival_city_specialty(city, 1, 0);
    assert(specialty_changes == 1 && last_specialty == 1);
    city[kCityIsPlayerOwned] = 0;
    current_mode = 0;
    retain_rival_city_specialty(city, 2, 0);
    assert(specialty_changes == 2 && last_specialty == 2);

    original_building_count = mock_building_count;
    allowed_turret_count = mock_allowed_turret_count;
    simulation_context = mock_context;
    scenario_settings = mock_settings;
    game_difficulty = mock_difficulty;
    assert(defensive_building_count(city) == 3);
    current_mode = kCivilizationMode;
    assert(defensive_building_count(city) == 15);
    assert(defensive_capacity_target(city) == 10);
    difficulty = 1;
    assert(defensive_building_count(city) == 3);
    assert(defensive_capacity_target(city) == 8);
    assert(install_rules(NULL) == 0);
    assert(argc == 2);
    verify_settings();
    verify_fleet_capacity();
    verify_rival_trade();
    verify_reference_writes(argv[1], 0);
    verify_reference_writes(argv[1], 1);
    verify_reference_writes(argv[1], 2);
    puts("Logic and reference executable checks passed.");
    return 0;
}
