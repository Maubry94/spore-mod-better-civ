#include "better-civ.cpp"
using namespace BetterCiv;
#include <assert.h>
#include <crtdbg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <memory>

enum {
    kSiteMinorNation=0x25, kSiteSpecialty=0x28, kCityIsPlayerOwned=0x33c,
    kTurretSlotsBegin=0x504, kTurretSlotsEnd=0x508, kCivIsPlayerOwned=0x89,
    kCivCitiesBegin=0x9c, kCivCitiesEnd=0xa0, kCivVehiclesBegin=0xb0, kCivVehiclesEnd=0xb4,
    kCityTradePartnersBegin=0x55c, kCityTradePartnersEnd=0x560,
    kVehiclePurpose=0xb20, kVehicleHitFlag=0xbcc
};
struct TradePartner { uint32_t political_id; void* route; int type; };
using PoliticalId=uint32_t (__thiscall*)(const void*);
static const void* read_pointer(const void* object, size_t offset) {
    const void* value;
    memcpy(&value, static_cast<const unsigned char*>(object)+offset, sizeof value);
    return value;
}


// Call each SDK adapter with the engine's __thiscall ABI, including stack
// cleanup and floating-point returns, rather than its convenient C++ signature.
static const float* call_configured_price(void* object) { return reinterpret_cast<ReadFloatProperty>(configured_price::detoured_function)(static_cast<App::Property*>(object)); }
static const float* call_configured_firing_interval(void* object) { return reinterpret_cast<ReadFloatProperty>(configured_firing_interval::detoured_function)(static_cast<App::Property*>(object)); }
static float call_configured_civ_generation(void* object, const char* name) { return reinterpret_cast<ReadScriptFloat>(configured_civ_generation::detoured_function)(static_cast<App::cArithmeticaResource*>(object), name); }
static float call_configured_starting_vehicles(void* object, const char* name) { return reinterpret_cast<ReadScriptFloat>(configured_starting_vehicles::detoured_function)(static_cast<App::cArithmeticaResource*>(object), name); }
static int call_configured_fleet_capacity(void* object, int specialty) { return reinterpret_cast<FleetCapacityFunction>(configured_fleet_capacity::detoured_function)(static_cast<Civilization*>(object), specialty); }
static int call_defensive_building_count(void* object) { return reinterpret_cast<CityCountFunction>(defensive_building_count::detoured_function)(static_cast<City*>(object)); }
static int call_extended_turret_limit(void* object) { return reinterpret_cast<TurretLimitFunction>(extended_turret_limit::detoured_function)(static_cast<City*>(object)); }
static int call_defensive_capacity_target(void* object) { return reinterpret_cast<CityCountFunction>(defensive_capacity_target::detoured_function)(static_cast<City*>(object)); }
static void call_retain_rival_city_specialty(void* object, int specialty, int walls) { return reinterpret_cast<SetSpecialtyFunction>(retain_rival_city_specialty::detoured_function)(static_cast<City*>(object), specialty, walls); }
static float call_trade_acceptance(void* object, void* city, void* civilization) { return reinterpret_cast<TradeAcceptance>(trade_acceptance::detoured_function)(static_cast<Native::TradeTuning*>(object), static_cast<City*>(city), static_cast<Civilization*>(civilization)); }

static unsigned current_mode;
static unsigned char observed_status;
static int observed_site_specialty;
static unsigned mock_draw;
static unsigned number_of_draws;
static int specialty_changes;
static int last_specialty;
static int difficulty = 2;
static int __fastcall mock_building_count(unsigned char *city, int) { (void)city; return 3; }
static int __fastcall mock_allowed_turret_count(unsigned char *city, int) { (void)city; return 8; }
static void *__cdecl mock_context(void) { return &difficulty; }
static void *__fastcall mock_settings(void *context, int) { return context; }
static int __fastcall mock_difficulty(void *scenario, int) { return *(int *)scenario; }

static uint32_t __cdecl mock_mode(void) { return current_mode; }
static void *__cdecl mock_found(unsigned char *site) {
    if (!site) return nullptr;
    observed_status = site[kSiteMinorNation];
    memcpy(&observed_site_specialty, site + kSiteSpecialty, sizeof observed_site_specialty);
    return site;
}
static unsigned __fastcall mock_random(void *state, int, unsigned bound) {
    assert(state == &mock_draw && bound == 2 && mock_draw < bound);
    ++number_of_draws;
    return mock_draw;
}
static int __fastcall mock_limit(unsigned char *city, int) {
    (void)city;
    return 4;
}
static void __fastcall mock_specialty(unsigned char *city, int, int kind, int walls) {
    (void)city;
    (void)walls;
    ++specialty_changes;
    last_specialty = kind;
}



static const uint32_t optional_sites[] = {
    0xbf2342, 0xca68b1, 0xb96b86, 0xb96ba1, 0xb9da81, 0xb9daa1,
    0xb96bbc, 0xb96bd0, 0xb9dac1, 0xb9dad8, 0xb97142,
    0xe2bb36, 0xe2bdaa, 0xe2c04f,
};

static const uintptr_t optional_targets[] = {
    (uintptr_t)configured_price::detoured_function, (uintptr_t)configured_firing_interval::detoured_function,
    (uintptr_t)configured_civ_generation::detoured_function, (uintptr_t)configured_civ_generation::detoured_function,
    (uintptr_t)configured_civ_generation::detoured_function, (uintptr_t)configured_civ_generation::detoured_function,
    (uintptr_t)configured_civ_generation::detoured_function, (uintptr_t)configured_civ_generation::detoured_function,
    (uintptr_t)configured_civ_generation::detoured_function, (uintptr_t)configured_civ_generation::detoured_function,
    (uintptr_t)configured_starting_vehicles::detoured_function,
    (uintptr_t)configured_hover_text::detoured_function, (uintptr_t)configured_hover_text::detoured_function,
    (uintptr_t)configured_hover_text::detoured_function,
};

static const uint32_t palette_sites[] = {0xd08195, 0xd08333, 0xd088d6, 0xe4950c, 0x5f1d08, 0x5ed8b1, 0x59e840};

static const Settings defaults = {
    100, 100, 8, 15, 15, 4000, 4000, kAllSpecialties, 0, TURRETS_VANILLA, 1, 1, 0, 0, 0,
};
static const Settings mixed = {
    75, 25, 12, 30, 30, 4000, 15000, kMilitaryMask | kEconomicMask, 1, TURRETS_ALL_SPECIALTIES, 0, 0, 0, 0, 0,
};
#include "installer-preset-tests.h"

static const float *__fastcall mock_read_float(void *property, int)
{
    return static_cast<const float*>(property);
}

static int __fastcall mock_fleet_capacity(void *civilization, int, int specialty)
{
    (void)specialty;
    return *(int *)civilization;
}

static float __fastcall mock_read_script(void *script, int, const char *name)
{
    (void)name;
    return *(const float *)script;
}

static void verify_settings(void)
{
    settings = defaults;
    apply_option(&settings, "<VehicleCost>CostVanilla");
    apply_option(&settings, "<OffensiveUnits>OffensiveUnitsVanilla");
    apply_option(&settings, "<UnknownGroup>UnknownOption");
    apply_option(&settings, "<VehicleCost>Cost50Trailing");
    apply_option(&settings, "<CapturedCities>CaptureVanilla");
    assert(memcmp(&settings, &defaults, sizeof settings) == 0);
    apply_option(&settings, "<AIPopulation>Population15To50");
    assert(settings.population_min == 15 && settings.population_max == 50);
    assert(settings.wealth_min == 4000 && settings.wealth_max == 4000);

    const char *legacy_choices[] = {
        "<Prix>PrixModere", "<Tir>TirRapide", "<Vitesse>Vitesse125",
        "<Offensifs>Offensifs12", "<Population>PopulationEgale30",
        "<Tresorerie>TresorerieVariable", "<Specialites>SansReligieuses",
        "<Capture>CaptureConserve", "<Tourelles>TourellesTous",
        "<Commerce>CommerceVanilla", "<RepliCommercial>RepliVanilla",
    };
    Settings legacy = defaults;
    for (size_t i = 0; i < sizeof legacy_choices / sizeof legacy_choices[0]; ++i)
        apply_option(&legacy, legacy_choices[i]);
    assert(memcmp(&legacy, &mixed, sizeof legacy) == 0);

    // Load the real installer settings format using the DLL-relative directory layout.
    settings = defaults;
    mod_instance = GetModuleHandleW(NULL);
    load_options();
    assert(memcmp(&settings, &mixed, sizeof settings) == 0);

    read_float_property = reinterpret_cast<decltype(read_float_property)>(mock_read_float);
    read_script_float = reinterpret_cast<decltype(read_script_float)>(mock_read_script);
    float cost = 1000.0f;
    float firing_interval = 2.0f;
    current_mode = kCivilizationMode;
    assert(*call_configured_price(&cost) == 750.0f);
    assert(*call_configured_firing_interval(&firing_interval) == 0.5f);
    assert(call_configured_starting_vehicles(&cost, "num_offensive_vehicles") == 12.0f);
    assert(call_configured_starting_vehicles(&cost, "num_harvesters") == cost);
    assert(call_configured_civ_generation(&cost, "population_min") == 30.0f);
    assert(call_configured_civ_generation(&cost, "population_max") == 30.0f);
    assert(call_configured_civ_generation(&cost, "wealth_min") == 4000.0f);
    assert(call_configured_civ_generation(&cost, "wealth_max") == 15000.0f);
    settings.wealth_min = settings.wealth_max = 4000;
    assert(call_configured_civ_generation(&cost, "wealth_min") == cost);
    current_mode = 0;
    assert(call_configured_price(&cost) == &cost);
    assert(call_configured_firing_interval(&firing_interval) == &firing_interval);
    settings = defaults;
}

static void verify_fleet_capacity(void)
{
    original_fleet_capacity = reinterpret_cast<decltype(original_fleet_capacity)>(mock_fleet_capacity);
    const TurretMode modes[] = {TURRETS_VANILLA, TURRETS_HARD_MAXIMUM, TURRETS_ALL_SPECIALTIES};
    const int expected[][3] = {{4, 4, 4}, {4, 4, 6}, {6, 5, 7}};
    int native = 4;
    for (size_t mode = 0; mode < sizeof modes / sizeof modes[0]; ++mode) {
        settings.turrets = modes[mode];
        current_mode = kCivilizationMode;
        for (difficulty = kDifficultyEasy; difficulty <= kDifficultyHard; ++difficulty) {
            for (int specialty = kMilitary; specialty <= kEconomic; ++specialty)
                assert(call_configured_fleet_capacity(&native, specialty) == expected[mode][difficulty]);
            assert(call_configured_fleet_capacity(&native, 4) == native);
        }
        current_mode = 0;
        assert(call_configured_fleet_capacity(&native, kMilitary) == native);
    }
    assert(fleet_capacity_with_defenses(0, TURRETS_ALL_SPECIALTIES, kDifficultyHard) == 0);
    assert(fleet_capacity_with_defenses(24, TURRETS_ALL_SPECIALTIES, kDifficultyHard) == 36);
    assert(fleet_capacity_with_defenses(36, TURRETS_ALL_SPECIALTIES, kDifficultyHard) == 36);
    difficulty = kDifficultyHard;
    current_mode = kCivilizationMode;
    settings = defaults;
}

struct TestProperty : App::Property {
    void SetItemType(Palettes::PaletteItem::ItemType type) {
        mValueKey = ResourceKey(static_cast<uint32_t>(type), 0, 0);
        mnType = App::PropertyType::Key;
    }
};

struct TestPropertyList {
    void** table;
    App::Property* cost;
    App::Property* type;
};

static bool __fastcall mock_property_lookup(const TestPropertyList* list, void*, uint32_t id, App::Property*& result)
{
    App::Property* found = id == kPaletteItemCost ? list->cost :
                          id == kPaletteItemType ? list->type : nullptr;
    if (!found) return false;
    result = found;
    return true;
}

static void verify_palette_prices(void)
{
    static_assert(sizeof(App::Property) == 0x14);
    void* table[10] = {};
    table[9] = reinterpret_cast<void*>(mock_property_lookup);
    TestProperty native, type;
    TestPropertyList fixture{table, &native, &type};
    auto* list = reinterpret_cast<App::PropertyList*>(&fixture);
    using Lookup = bool (__thiscall*)(App::PropertyList*, uint32_t, App::Property*&);
    const auto lookup = reinterpret_cast<Lookup>(configured_palette_cost::detoured_function);
    current_mode = kCivilizationMode;
    type.SetItemType(Palettes::PaletteItem::kItemCityVehicle);

    // Land, sea and air base costs, and a non-default compatible palette cost.
    const int32_t base_prices[] = {1000, 1500, 3000, 1200};
    for (int percent : {50, 75, 100}) {
        settings.cost_percent = percent;
        for (int32_t base : base_prices) {
            native.SetValueInt32(base);
            const int expected = base * percent / 100;
            for (int repeat = 0; repeat < 3; ++repeat) {
                App::Property* result = nullptr;
                assert(lookup(list, kPaletteItemCost, result) && result);
                const int displayed = *result->GetValueInt32();
                int32_t tooltip = -1;
                assert(configured_palette_int(list, kPaletteItemCost, tooltip));
                float charged_base = static_cast<float>(base);
                const float charged = *call_configured_price(&charged_base);
                assert(displayed == expected && tooltip == expected && charged == expected);
                // The planner's native affordability check compares funds with
                // this returned cost. The original resource must stay intact.
                for (int funds : {expected - 1, expected, expected + 1, base - 1})
                    assert((funds >= displayed) == (funds >= charged));
                assert(*native.GetValueInt32() == base);
                assert((result == &native) == (percent == 100));
            }
        }
    }
    settings.cost_percent = 50;
    native.SetValueInt32(1000);
    // Execute both patched indirect-call forms, including the native stack
    // argument layout. This catches a wrong register or callee stack cleanup.
    const unsigned char load_opcodes[] = {0xb8, 0xba};
    for (unsigned char opcode : load_opcodes) {
        unsigned char caller[] = {0,0,0,0,0, 0xff,0x74,0x24,0x08,
                                  0xff,0x74,0x24,0x08, 0xff,0xd0, 0xc2,0x08,0x00};
        if (opcode == 0xba) caller[14] = 0xd2;
        auto* executable = static_cast<unsigned char*>(VirtualAlloc(nullptr, sizeof caller,
                                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        assert(executable);
        memcpy(executable, caller, sizeof caller);
        BranchSite target{0, {}, reinterpret_cast<uintptr_t>(configured_palette_cost::detoured_function),
                          executable, 0, 1, opcode};
        write_branch(&target);
        DWORD protection;
        assert(VirtualProtect(executable, sizeof caller, PAGE_EXECUTE_READ, &protection));
        assert(FlushInstructionCache(GetCurrentProcess(), executable, sizeof caller));
        App::Property* actual = nullptr;
        assert(reinterpret_cast<Lookup>(executable)(list, kPaletteItemCost, actual));
        assert(*actual->GetValueInt32() == 500);
        assert(VirtualFree(executable, 0, MEM_RELEASE));
    }
    for (auto item_type : {Palettes::PaletteItem::kItemCityBuilding, Palettes::PaletteItem::kItemCityTurret,
                              Palettes::PaletteItem::kItemTribeTool}) {
        type.SetItemType(item_type);
        App::Property* result = nullptr;
        int32_t value = 0;
        assert(lookup(list, kPaletteItemCost, result) && result == &native);
        assert(configured_palette_int(list, kPaletteItemCost, value) && value == 1000);
    }
    type.SetItemType(Palettes::PaletteItem::kItemCityVehicle);
    current_mode = 0x01654c05; // Space Stage uses the same palette class.
    App::Property* result = nullptr;
    int32_t value = -7;
    assert(lookup(list, kPaletteItemCost, result) && result == &native);
    assert(configured_palette_int(list, kPaletteItemCost, value) && value == 1000);
    current_mode = kCivilizationMode;
    for (int32_t sentinel : {0, -1}) {
        native.SetValueInt32(sentinel);
        assert(lookup(list, kPaletteItemCost, result));
        assert(*result->GetValueInt32() == sentinel);
    }
    fixture.cost = nullptr;
    result = &type;
    value = -7;
    assert(!lookup(list, kPaletteItemCost, result) && result == &type);
    assert(!configured_palette_int(list, kPaletteItemCost, value) && value == -7);
    assert(!configured_palette_int(nullptr, kPaletteItemCost, value) && value == -7);
    fixture.cost = &native;
    fixture.type = nullptr;
    native.SetValueInt32(1000);
    assert(lookup(list, kPaletteItemCost, result) && result == &native);
    assert(configured_palette_int(list, kPaletteItemCost, value) && value == 1000);
    fixture.type = &type;
    assert(lookup(list, kPaletteItemType, result) && result == &type);
    native.mnType = App::PropertyType::Float;
    assert(lookup(list, kPaletteItemCost, result) && result == &native);
    settings = defaults;
}

static const char16_t* __fastcall mock_localized_text(LocalizedString* text, void*)
{
    return reinterpret_cast<const char16_t*>(text);
}

static void verify_hover_prices(void)
{
    read_localized_text = reinterpret_cast<ReadLocalizedText>(mock_localized_text);
    const auto read = reinterpret_cast<ReadLocalizedText>(configured_hover_text::detoured_function);
    current_mode = kCivilizationMode;
    const char16_t* templates[] = {u"Land vehicle (1,000 Sporebucks)", u"V\u00e9hicule terrestre (1 000 Sflouz)",
                                  u"Boat (1\u202f500)", u"Air vehicle (3.000)"};
    const char16_t* half[] = {u"Land vehicle (500 Sporebucks)", u"V\u00e9hicule terrestre (500 Sflouz)",
                             u"Boat (750)", u"Air vehicle (1.500)"};
    const char16_t* reduced[] = {u"Land vehicle (750 Sporebucks)", u"V\u00e9hicule terrestre (750 Sflouz)",
                                u"Boat (1\u202f125)", u"Air vehicle (2.250)"};
    for (int percent : {50, 75, 100}) {
        settings.cost_percent = percent;
        for (size_t i = 0; i < 4; ++i) {
            for (int repeat = 0; repeat < 3; ++repeat) {
                const char16_t* expected = percent == 100 ? templates[i] : percent == 50 ? half[i] : reduced[i];
                const char16_t* actual = read(reinterpret_cast<LocalizedString*>(const_cast<char16_t*>(templates[i])));
                assert(std::u16string(actual) == expected);
            }
        }
    }
    char16_t buffer[512];
    settings.cost_percent = 50;
    for (const char16_t* text : {u"", u"No available vehicle", u"Vehicle (999999999999999999999999)"})
        assert(discount_hover_text(text, buffer) == text);
    const std::u16string long_text(512, u'1');
    assert(discount_hover_text(long_text.c_str(), buffer) == long_text.c_str());
    assert(!discount_hover_text(nullptr, buffer));
    assert(std::u16string(discount_hover_text(u"Vehicle (0)", buffer)) == u"Vehicle (0)");
    current_mode = 0x01654c05;
    assert(discount_hover_text(templates[0], buffer) == templates[0]);
    current_mode = kCivilizationMode;
    settings = defaults;
}

static int portrait_calls;
static void __fastcall mock_restore_portrait(Native::RivalPortraitPanel* panel, void*, int slot, Civilization* civ)
{
    assert(slot >= 0 && slot < 4);
    ++portrait_calls;
    panel->slots[slot].occupied = true;
    panel->slots[slot].civilization = civ;
}
static void __fastcall mock_register_portrait(Native::RivalPortraitPanel* panel, void*, Civilization* civ)
{
    // Emulate the native four-slot search. Entering this with a full panel and
    // a fifth nation is precisely the path observed in the crash dump.
    int slot = 0;
    for (; slot < 4; ++slot)
        if (panel->slots[slot].occupied && panel->slots[slot].civilization == civ) return;
    for (slot = 0; slot < 4 && panel->slots[slot].occupied; ++slot) {}
    assert(slot < 4);
    mock_restore_portrait(panel, nullptr, slot, civ);
    civ->mCommRelPanelSlot = slot;
}
static void verify_portrait_bounds(void)
{
    original_register_portrait = reinterpret_cast<RegisterPortrait>(mock_register_portrait);
    original_restore_portrait = reinterpret_cast<RestorePortrait>(mock_restore_portrait);
    const auto add = reinterpret_cast<RegisterPortrait>(safe_register_portrait::detoured_function);
    const auto restore = reinterpret_cast<RestorePortrait>(safe_restore_portrait::detoured_function);
    Native::RivalPortraitPanel panel{};
    alignas(4) unsigned char data[9][0x2c0] = {};
    Civilization* civs[9];
    for (int i = 0; i < 9; ++i) {
        civs[i] = reinterpret_cast<Civilization*>(data[i]);
        civs[i]->mCommRelPanelSlot = -1;
        add(&panel, civs[i]);
        assert(civs[i]->mCommRelPanelSlot == (i < 4 ? i : -1));
    }
    assert(portrait_calls == 4);
    for (int i = 0; i < 9; ++i) add(&panel, civs[i]);
    assert(portrait_calls == 4);
    panel = {};
    for (int i = 0; i < 9; ++i) restore(&panel, civs[i]->mCommRelPanelSlot, civs[i]);
    assert(portrait_calls == 8);
    for (int bad_slot : {-1, 4, 9, INT32_MAX, INT32_MIN}) {
        restore(&panel, bad_slot, civs[8]);
        assert(civs[8]->mCommRelPanelSlot == -1 && portrait_calls == 8);
    }
    panel.slots[2] = {};
    restore(&panel, 0, civs[8]);
    assert(civs[8]->mCommRelPanelSlot == 2 && portrait_calls == 9);
    add(&panel, nullptr);
    restore(&panel, 4, nullptr);
}

using FoundationDecoder = int (__cdecl*)(int);
#include "balanced-specialty-tests.h"

static void verify_foundation(FoundationDecoder decode)
{
    unsigned char site[0x64] = {};
    number_of_draws = 0;
    const char *options[] = {
        "EqualOdds", "NoMilitary", "NoReligious", "NoEconomic",
        "MilitaryOnly", "ReligiousOnly", "EconomicOnly",
    };
    const int specialties[][2] = {
        {kMilitary, kMilitary}, {kReligious, kEconomic}, {kMilitary, kEconomic},
        {kMilitary, kReligious}, {kMilitary, kMilitary}, {kReligious, kReligious},
        {kEconomic, kEconomic},
    };
    random_below = reinterpret_cast<decltype(random_below)>(mock_random);
    simulation_random = &mock_draw;
    for (size_t i = 0; i < sizeof options / sizeof options[0]; ++i) {
        char line[80];
        snprintf(line, sizeof line, "<AISpecialties>%s", options[i]);
        settings = defaults;
        apply_option(&settings, line);
        for (mock_draw = 0; mock_draw < 2; ++mock_draw) {
            int native = kMilitary;
            memcpy(site + kSiteSpecialty, &native, sizeof native);
            current_mode = 0;
            site[kSiteMinorNation] = 1;
            assert(found_major_city(reinterpret_cast<Native::CityFoundation*>(site)) == reinterpret_cast<City*>(site) && observed_status == 1);
            assert(observed_site_specialty == native);
            current_mode = kCivilizationMode;
            assert(found_major_city(reinterpret_cast<Native::CityFoundation*>(site)) == reinterpret_cast<City*>(site) && observed_status == 0);
            // Interpret the descriptor with the actual reference executable's
            // converter, not a mock repeating the mod's numeric assumptions.
            assert(decode(observed_site_specialty) == specialties[i][mock_draw]);
        }
    }
    assert(number_of_draws == 6);
    settings = defaults;
    // Equal Odds preserves every native descriptor and consumes no extra RNG.
    for (int native : {0, 1, 2, -1}) {
        memcpy(site + kSiteSpecialty, &native, sizeof native);
        found_major_city(reinterpret_cast<Native::CityFoundation*>(site));
        assert(observed_site_specialty == native);
    }
    assert(found_major_city(nullptr) == nullptr && number_of_draws == 6);
}

static void verify_native_foundation_contract(const unsigned char* image)
{
    // This 38-byte pure function has only stack reads, relative branches and
    // constants: no game state, imports, relocations or external calls. Copy
    // it from the user's reference file; never bundle the executable itself.
    constexpr size_t length = 38;
    const auto* source = image + 0xbefa70 - kReferenceBase;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) hash = (hash ^ source[i]) * 16777619u;
    assert(hash == 0xec66066a);
    auto* code = static_cast<unsigned char*>(VirtualAlloc(nullptr, length,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(code);
    memcpy(code, source, length);
    DWORD previous;
    assert(VirtualProtect(code, length, PAGE_EXECUTE_READ, &previous));
    assert(FlushInstructionCache(GetCurrentProcess(), code, length));
    auto decode = reinterpret_cast<FoundationDecoder>(code);
    assert(decode(0) == kVehicleMilitary);
    assert(decode(1) == kVehicleEconomic);
    assert(decode(2) == kVehicleCultural);
    assert(decode(-1) == -1 && decode(3) == -1);
    const auto saved_mode = game_mode;
    const auto saved_found = original_found_city;
    const auto saved_random = random_below;
    const auto saved_state = simulation_random;
    const auto saved_settings = settings;
    const auto saved_current_mode = current_mode;
    game_mode = reinterpret_cast<decltype(game_mode)>(mock_mode);
    original_found_city = reinterpret_cast<decltype(original_found_city)>(mock_found);
    verify_foundation(decode);
    verify_balanced_foundation(decode);
    game_mode = saved_mode; original_found_city = saved_found;
    random_below = saved_random; simulation_random = saved_state;
    settings = saved_settings; current_mode = saved_current_mode;
    assert(VirtualFree(code, 0, MEM_RELEASE));
    puts("All seven specialty choices passed the native foundation-to-vehicle-purpose converter.");
}

static unsigned char trade_player[0xa4];
static int trade_strategy;
static int partner_opinion = 2;
static int origin_opinion = 2;
static float native_trade;

static float __fastcall mock_trade(void *tuning, int, void *city, void *civ)
{ (void)tuning; (void)city; (void)civ; return native_trade; }
static void *__fastcall mock_city_owner(void *city, int)
{ return (void *)read_pointer(city, 0x590); }
static int __fastcall mock_strategy(void *civ, int)
{ (void)civ; return trade_strategy; }
static const VehicleOrder *__fastcall mock_order(void *vehicle, int)
{ return static_cast<const VehicleOrder*>(vehicle); }
static void *__fastcall mock_player(void *context, int)
{ (void)context; return trade_player; }
static uint32_t __fastcall mock_political_id(const void *civ, int)
{ return ((const unsigned char *)civ)[0x24]; }
static int __fastcall mock_relationship(void *manager, int, uint32_t from, uint32_t to, int fresh)
{
    assert(manager && fresh && ((from == 1 && to == 2) || (from == 2 && to == 1)));
    return from == 1 ? origin_opinion : partner_opinion;
}

static void put_pointer(void *object, size_t offset, const void *value)
{
    memcpy((unsigned char *)object + offset, &value, sizeof value);
}

static VehicleOrder trade_order;

static const VehicleOrder *__fastcall mock_trade_order(void *vehicle, int)
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
    game_mode = reinterpret_cast<decltype(game_mode)>(mock_mode);
    current_vehicle_order = reinterpret_cast<decltype(current_vehicle_order)>(mock_trade_order);
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
    PoliticalId getter = reinterpret_cast<decltype(getter)>(mock_political_id);
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

    original_trade_acceptance = reinterpret_cast<decltype(original_trade_acceptance)>(mock_trade);
    city_owner = reinterpret_cast<decltype(city_owner)>(mock_city_owner);
    civilization_strategy = reinterpret_cast<decltype(civilization_strategy)>(mock_strategy);
    relationship_manager = reinterpret_cast<decltype(relationship_manager)>(mock_context);
    relationship_level = reinterpret_cast<decltype(relationship_level)>(mock_relationship);
    find_player_civilization = reinterpret_cast<decltype(find_player_civilization)>(mock_player);
    current_vehicle_order = reinterpret_cast<decltype(current_vehicle_order)>(mock_order);
    trade_strategy = kCivEconomicStrategy;
    current_mode = kCivilizationMode;
    assert(call_trade_acceptance(NULL, target, origin) == 1.0f);
    current_mode = 0;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    current_mode = kCivilizationMode;
    order.kind = kCreateTradeRouteOrder;
    order.target = other;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    order.target = target;
    assert(call_trade_acceptance(NULL, target, origin) == 1.0f);
    route.political_id = 2;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    route.political_id = 3;
    partner_opinion = 1;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    partner_opinion = 2;
    origin_opinion = 0;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    origin_opinion = 2;
    trade_strategy = 0;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    trade_strategy = kCivEconomicStrategy;
    partner[kCivIsPlayerOwned] = 1;
    assert(call_trade_acceptance(NULL, target, origin) == 0.0f);
    native_trade = 0.6f;
    assert(call_trade_acceptance(NULL, target, origin) == 0.6f);
}

#include "conversion-capacity-tests.h"
#include "reference-code-tests.h"

static void verify_reference_writes(const char *path, int variant, int ai_options = 0)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    unsigned char *disk = static_cast<unsigned char*>(HeapAlloc(GetProcessHeap(), 0, (size_t)size));
    assert(disk && fread(disk, 1, (size_t)size, file) == (size_t)size);
    fclose(file);

    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)disk;
    IMAGE_NT_HEADERS32 *header = (IMAGE_NT_HEADERS32 *)(disk + dos->e_lfanew);
    unsigned char *image = static_cast<unsigned char*>(VirtualAlloc(NULL, kSteamOctober2024ImageSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(image);
    memcpy(image, disk, header->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(header);
    for (int i = 0; i < header->FileHeader.NumberOfSections; ++i) {
        assert(section[i].PointerToRawData + section[i].SizeOfRawData <= (size_t)size);
        memcpy(image + section[i].VirtualAddress, disk + section[i].PointerToRawData,
               section[i].SizeOfRawData);
    }
    IMAGE_NT_HEADERS32 *image_header = (IMAGE_NT_HEADERS32 *)(image + dos->e_lfanew);
    prepare_relocated_reference_image(image, variant == 0 && ai_options == 0);
    if (variant == 0 && ai_options == 0) {
        verify_native_foundation_contract(image);
        verify_native_capacity_constants(image);
        // Exact scripts used by native call startup and acknowledgement.
        assert(memcmp(image + 0x147f218 - kReferenceBase, kPortraitCallMaterial, sizeof kPortraitCallMaterial) == 0);
        assert(memcmp(image + 0x13f9ca0 - kReferenceBase, kPortraitIdleMaterial, sizeof kPortraitIdleMaterial) == 0);
    }
    unsigned char *founding = image + 0xb9bda0 - kReferenceBase;
    unsigned char *limit = image + 0xbd8c67 - kReferenceBase;
    unsigned char *editor = image + 0xd07e1a - kReferenceBase;
    unsigned char *capture = image + 0xbe8e51 - kReferenceBase;
    unsigned char *retreat = image + kVehicleRetreatCondition - kReferenceBase;

    image_header->FileHeader.TimeDateStamp ^= 1;
    assert(!install_rules(image) && founding[1] == 0x4b);
    image_header->FileHeader.TimeDateStamp ^= 1;
    image_header->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    assert(!install_rules(image) && founding[1] == 0x4b);
    image_header->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;

    const int use_options = variant == 1;
    // Reject changed native helper bodies before writing even the founding hook.
    for (const auto helper : {std::pair<uint32_t, unsigned>{0xe01280, 27}, {0xe04930, 235},
         {0xe052b0, 236}, {0xe064d0, 112}, {0xe06610, 108}, {0x808b80, 66},
         {0x744700, 29}, {0x751619, 7}, {0x753172, 7}, {0x7531bf, 259},
         {0xb99c16, 22}, {0xbefa70, 38}, {0xb99b74, 23}, {0xbf82f6, 19},
         {0xbeb710, 270}, {0xbeb310, 1024}, {0xbd9ff0, 44},
         {0xd57b00, 719}, {0xafdf60, 955}, {0x13eb950, 4},
         {0x1486054, 4}, {0x13eed94, 4}, {0x145be38, 4}, {0x1567354, 4}}) {
        auto* body = image + helper.first - kReferenceBase;
        for (unsigned i = 0; i < helper.second; ++i) {
            body[i] ^= 1; assert(!install_rules(image) && founding[1] == 0x4b); body[i] ^= 1;
        }
    }
    auto* portrait_update_site = image + 0xcfe3fc - kReferenceBase;
    portrait_update_site[4] ^= 1;
    assert(!install_rules(image) && founding[1] == 0x4b);
    portrait_update_site[4] ^= 1;
    settings = use_options ? mixed : defaults;
    auto* conversion_site = image + 0xae57e3 - kReferenceBase;
    conversion_site[4] ^= 1;
    assert(!install_rules(image) && founding[1] == 0x4b);
    conversion_site[4] ^= 1;
    auto* preload_site = image + 0xcfcf0c - kReferenceBase;
    preload_site[4] ^= 1;
    assert(!install_rules(image) && founding[1] == 0x4b);
    preload_site[4] ^= 1;
    settings.rethink_offensives = ai_options & 1;
    settings.ai_diagnostics = (ai_options >> 1) & 1;
    settings.stalled_seconds = (ai_options & 4) ? 180 : 0;
    settings.retry_blocked_paths=variant==1?2:0;
    auto* request_site=image+0xc9ea20-kReferenceBase;
    unsigned char request_original[5];memcpy(request_original,request_site,5);
    auto* transit_site=image+0xdcc7c0-kReferenceBase;
    unsigned char transit_original[5];memcpy(transit_original,transit_site,5);
    const uint32_t ai_sites[] = {0xbfbfe5, 0xbfbfee, 0xbfaa32};
    const uintptr_t ai_targets[] = {reinterpret_cast<uintptr_t>(observed_expansion_planner::detoured_function),
        reinterpret_cast<uintptr_t>(observed_expansion_planner::detoured_function),
        reinterpret_cast<uintptr_t>(filtered_expansion_owner::detoured_function)};
    const bool ai_enabled[] = {ai_options != 0, ai_options != 0, settings.rethink_offensives || settings.stalled_seconds};
    if (settings.stalled_seconds) {
        auto* setter = image + 0xcac1a0 - kReferenceBase;
        for (unsigned i = 0; i < 212; ++i) {
            setter[i] ^= 1;
            assert(!install_rules(image) && founding[1] == 0x4b);
            setter[i] ^= 1;
        }
    }
    unsigned char original_ai_bytes[3][5];
    for (unsigned i = 0; i < 3; ++i) {
        auto* site = image + ai_sites[i] - kReferenceBase;
        memcpy(original_ai_bytes[i], site, 5);
        if (ai_enabled[i]) {
            site[4] ^= 1;
            assert(!install_rules(image) && founding[1] == 0x4b);
            site[4] ^= 1;
        }
    }
    if (variant == 2)
        apply_option(&settings, "<TurretsAndFleets>MaxByTypeOnHard");
    unsigned char original_option_bytes[sizeof optional_sites / sizeof optional_sites[0]][5];
    for (size_t i = 0; i < sizeof optional_sites / sizeof optional_sites[0]; ++i)
        memcpy(original_option_bytes[i], image + optional_sites[i] - kReferenceBase, 5);
    unsigned char original_palette_bytes[sizeof palette_sites / sizeof palette_sites[0]][5];
    for (size_t i = 0; i < sizeof palette_sites / sizeof palette_sites[0]; ++i) {
        auto* site = image + palette_sites[i] - kReferenceBase;
        memcpy(original_palette_bytes[i], site, 5);
        if (use_options) {
            site[4] ^= 1;
            assert(!install_rules(image) && founding[1] == 0x4b);
            site[4] ^= 1;
        }
    }
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
      image + 0xbe8c2c - kReferenceBase, image + 0xbe5c45 - kReferenceBase, image + 0xbe5cc4 - kReferenceBase,
      image + 0xbf468d - kReferenceBase, image + 0xe065de - kReferenceBase, image + 0xcfe3fc - kReferenceBase,
      image + 0xcfcf0c - kReferenceBase, image + 0xae57e3 - kReferenceBase};
    uintptr_t targets[] = {(uintptr_t)found_major_city, (uintptr_t)extended_turret_limit::detoured_function,
                           (uintptr_t)extended_turret_limit::detoured_function, (uintptr_t)retain_rival_city_specialty::detoured_function,
                           (uintptr_t)retain_rival_city_specialty::detoured_function, (uintptr_t)defensive_capacity_target::detoured_function, (uintptr_t)defensive_building_count::detoured_function,
                           (uintptr_t)safe_register_portrait::detoured_function, (uintptr_t)safe_restore_portrait::detoured_function,
                           (uintptr_t)reuse_portrait_slots::detoured_function, (uintptr_t)rechecked_civ_model_world,
                           (uintptr_t)initialized_city_conversion::detoured_function};
    int enabled[] = {1, variant == 1, variant == 1, variant == 1,
                     variant == 1, variant != 0, variant != 0, 1, 1, 1, 1, 1};
    uint32_t fleet_sites[] = {0xbd7325, 0xbf228a, 0xbf686f, 0xbf6afa, 0xd07c73,
                             0xd1df57, 0xd1df73, 0xd1df93};
    unsigned char original_base_bytes[sizeof sites / sizeof sites[0]][5];
    for (size_t i = 0; i < sizeof sites / sizeof sites[0]; ++i)
        memcpy(original_base_bytes[i], sites[i], 5);
    for (size_t i = 7; i < 9; ++i) {
        sites[i][4] ^= 1;
        assert(!install_rules(image) && memcmp(founding, original_base_bytes[0], 5) == 0);
        sites[i][4] ^= 1;
    }
    if (enabled[3]) {
        capture[4] ^= 1;
        assert(!install_rules(image) && memcmp(founding, original_base_bytes[0], 5) == 0);
        capture[4] ^= 1;
    }
    DWORD old;
    assert(VirtualProtect(founding, 5, PAGE_EXECUTE_READ, &old));
    assert(VirtualProtect(retreat, sizeof native_retreat, PAGE_EXECUTE_READ, &old));
    for (uint32_t address : palette_sites)
        assert(VirtualProtect(image + address - kReferenceBase, 5, PAGE_EXECUTE_READ, &old));
    assert(install_rules(image));
    if(settings.retry_blocked_paths==2 || settings.ai_diagnostics) {
        uint32_t displacement;memcpy(&displacement,transit_site+1,4);
        assert(transit_site[0]==0xe9 && reinterpret_cast<uintptr_t>(transit_site)+5+displacement==reinterpret_cast<uintptr_t>(guarded_trade_update));
        assert(trade_transit_native_ready && trade_update_continue==reinterpret_cast<uintptr_t>(transit_site)+5);
    } else assert(!memcmp(transit_site,transit_original,5));
    if(settings.retry_blocked_paths==2) {
        uint32_t displacement;memcpy(&displacement,request_site+1,4);
        assert(request_site[0]==0xe9 && reinterpret_cast<uintptr_t>(request_site)+5+displacement==reinterpret_cast<uintptr_t>(retained_local_request));
        assert(recovery_native_ready && reinterpret_cast<uintptr_t>(original_local_request)==reinterpret_cast<uintptr_t>(image)+0xc43fa0-kReferenceBase);
    } else assert(!memcmp(request_site,request_original,5));
    for (unsigned i = 0; i < 3; ++i) {
        auto* site = image + ai_sites[i] - kReferenceBase;
        if (!ai_enabled[i]) assert(memcmp(site, original_ai_bytes[i], 5) == 0);
        else {
            uint32_t displacement;
            memcpy(&displacement, site + 1, 4);
            assert(site[0] == 0xe8 && reinterpret_cast<uintptr_t>(site) + 5 + displacement == ai_targets[i]);
        }
    }
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
                                        : (uintptr_t)configured_fleet_capacity::detoured_function;
        assert(site[0] == 0xe8 && (uintptr_t)site + 5 + relative == target);
    }
    for (size_t i = 0; i < sizeof palette_sites / sizeof palette_sites[0]; ++i) {
        const auto* site = image + palette_sites[i] - kReferenceBase;
        if (!use_options) {
            assert(memcmp(site, original_palette_bytes[i], 5) == 0);
            continue;
        }
        uint32_t address;
        memcpy(&address, site + 1, sizeof address);
        if (i < 5) {
            assert(site[0] == (i == 4 ? 0xba : 0xb8));
            assert(address == reinterpret_cast<uintptr_t>(configured_palette_cost::detoured_function));
        } else {
            assert(site[0] == 0xe8 && reinterpret_cast<uintptr_t>(site) + 5 + address ==
                   reinterpret_cast<uintptr_t>(configured_palette_int));
        }
    }
    const uint32_t commerce_sites[] = {0xbfb446, 0xbfb7bc, 0xdca391};
    for (size_t i = 0; i < sizeof commerce_sites / sizeof commerce_sites[0]; ++i) {
        unsigned char *site = image + commerce_sites[i] - kReferenceBase;
        uint32_t relative;
        memcpy(&relative, site + 1, 4);
        uintptr_t target = use_options ? (uintptr_t)(image + 0xceef90 - kReferenceBase)
                                       : (uintptr_t)trade_acceptance::detoured_function;
        assert(site[0] == 0xe8 && (uintptr_t)site + 5 + relative == target);
    }
    MEMORY_BASIC_INFORMATION page;
    assert(VirtualQuery(founding, &page, sizeof page) && page.Protect == PAGE_EXECUTE_READ);
    assert(VirtualQuery(retreat, &page, sizeof page) && page.Protect == PAGE_EXECUTE_READ);
    for (uint32_t address : palette_sites)
        assert(VirtualQuery(image + address - kReferenceBase, &page, sizeof page) && page.Protect == PAGE_EXECUTE_READ);
    VirtualFree(image, 0, MEM_RELEASE);
    HeapFree(GetProcessHeap(), 0, disk);
}

#include "offensive-tests.h"
#include "portrait-tests.h"
#include "pagination-tests.h"
#include "pager-widget-tests.h"
#include "portrait-call-tests.h"
#include "portrait-order-tests.h"
#include "preload-tests.h"
#include "conversion-tests.h"
#include "combat-tests.h"
#include "movement-tests.h"
#include "recovery-tests.h"
#include "placement-tests.h"
#include "traffic-tests.h"
#include "fleet-tests.h"
#include "trade-route-tests.h"
#include "movement-fleet-fix-tests.h"
#include "trade-transit-tests.h"

int main(int argc, char *argv[])
{
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc == 3 && strcmp(argv[1], "--reference-live") == 0) {
        verify_captured_reference_guards(argv[2]);
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "--loader") == 0) {
        // Load the shim explicitly from an isolated test directory. The actual
        // mod's DllMain must register with it through its real DLL imports.
        HMODULE shim = LoadLibraryExA(argv[2], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        assert(shim);
        HMODULE mod = LoadLibraryExA(argv[3], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        assert(mod);
        auto run = reinterpret_cast<int (__cdecl*)()>(GetProcAddress(shim, "RunRegisteredInitialization"));
        assert(run && run() == 1);
        // The host is this test executable, so activation must fail cleanly.
        assert(FreeLibrary(mod));
        assert(FreeLibrary(shim));
        puts("DLL load, SDK initialization registration and unsupported-host rejection passed.");
        return 0;
    }
    assert(argc == 3);
    // Keep the fixture loaded until process exit: SDK addresses also back the
    // thread-local scalar destructor. Nothing is copied into the game folders.
    assert(LoadLibraryExA(argv[2], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
    unsigned char city[0x818] = {0};
    game_mode = reinterpret_cast<decltype(game_mode)>(mock_mode);
    original_found_city = reinterpret_cast<decltype(original_found_city)>(mock_found);
    original_turret_limit = reinterpret_cast<decltype(original_turret_limit)>(mock_limit);
    original_set_specialty = reinterpret_cast<decltype(original_set_specialty)>(mock_specialty);

    current_mode = kCivilizationMode;

    uintptr_t start = 0x1000;
    uintptr_t end = start + 8 * kTurretSlotSize;
    memcpy(city + kTurretSlotsBegin, &start, sizeof start);
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(call_extended_turret_limit(city) == 8);
    end += kTurretSlotSize;
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(call_extended_turret_limit(city) == 8);
    end = start + 4 * kTurretSlotSize;
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(call_extended_turret_limit(city) == 4);
    end = start + 6 * kTurretSlotSize;
    memcpy(city + kTurretSlotsEnd, &end, sizeof end);
    assert(call_extended_turret_limit(city) == 6);

    call_retain_rival_city_specialty(city, 1, 0);
    assert(specialty_changes == 0);
    city[kCityIsPlayerOwned] = 1;
    call_retain_rival_city_specialty(city, 1, 0);
    assert(specialty_changes == 1 && last_specialty == 1);
    city[kCityIsPlayerOwned] = 0;
    current_mode = 0;
    call_retain_rival_city_specialty(city, 2, 0);
    assert(specialty_changes == 2 && last_specialty == 2);

    original_building_count = reinterpret_cast<decltype(original_building_count)>(mock_building_count);
    allowed_turret_count = reinterpret_cast<decltype(allowed_turret_count)>(mock_allowed_turret_count);
    simulation_context = reinterpret_cast<decltype(simulation_context)>(mock_context);
    scenario_settings = reinterpret_cast<decltype(scenario_settings)>(mock_settings);
    game_difficulty = reinterpret_cast<decltype(game_difficulty)>(mock_difficulty);
    assert(call_defensive_building_count(city) == 3);
    current_mode = kCivilizationMode;
    assert(call_defensive_building_count(city) == 15);
    assert(call_defensive_capacity_target(city) == 10);
    difficulty = 1;
    assert(call_defensive_building_count(city) == 3);
    assert(call_defensive_capacity_target(city) == 8);
    assert(install_rules(NULL) == 0);
    verify_settings();
    verify_installer_presets();
    puts("Settings checks passed; checking palette prices...");
    verify_palette_prices();
    puts("Palette price and affordability checks passed.");
    verify_hover_prices();
    verify_portrait_bounds();
    puts("Localized hover prices and nine-rival portrait bounds passed.");
    verify_fleet_capacity();
    verify_rival_trade();
    verify_offensive_policy();
    verify_offensive_adapters();
    puts("Offensive wave, SDK adapter, lifecycle and optional log checks passed.");
    verify_portrait_reuse();
    verify_portrait_pagination();
    verify_pager_widget_setup();
    verify_portrait_call_alerts();
    verify_portrait_arrival_order();
    verify_portrait_specialty_refresh();
    verify_preload_recheck();
    verify_conversion_diagnostics();
    verify_combat_diagnostics();
    verify_movement_diagnostics();
    verify_movement_recovery();
    verify_placement_policy();
    verify_placement_adapter();
    verify_placement_loops();
    verify_placement_terrain();
    verify_traffic_geometry();
    verify_traffic_adapter();
    verify_fleet_policy();
    verify_fleet_adapter();
    verify_land_fleet_adapter();
    verify_retained_request();
    verify_religious_surplus();
    verify_trade_geometry();
    verify_trade_adapter();
    verify_trade_transit();
    verify_conversion_capacity();
    puts("Diplomacy slot reuse, priority, map/reference ownership and lifecycle checks passed.");
    verify_reference_writes(argv[1], 0);
    verify_reference_writes(argv[1], 1);
    verify_reference_writes(argv[1], 2);
    for (int ai_options = 1; ai_options <= 7; ++ai_options)
        verify_reference_writes(argv[1], 0, ai_options);
    puts("Logic and reference executable checks passed.");
    return 0;
}
