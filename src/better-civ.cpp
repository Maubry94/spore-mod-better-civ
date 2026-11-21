#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <Spore/ModAPI.h>
#include <Spore/App/Property.h>
#include <Spore/App/PropertyList.h>
#include <Spore/Palettes/PaletteItem.h>
#include <Spore/App/cArithmeticaResource.h>
#include <Spore/Simulator/cCulturalTarget.h>
#include <Spore/Simulator/SubSystem/cStrategy.h>
#include <Spore/Simulator/SubSystem/SimulatorSystem.h>
// The SDK's GetData template uses an unqualified intrusive_ptr internally.
namespace Simulator { using eastl::intrusive_ptr; }
#include <Spore/Simulator/SubSystem/GameNounManager.h>
#include <Spore/Simulator/SubSystem/CommManager.h>
#include <Spore/Simulator/SubSystem/PlanetModel.h>
#include <Spore/Simulator/cObstacle.h>
#include <Spore/Simulator/cCityWalls.h>
#include <Spore/UTFWin/IButton.h>
#include <Spore/UTFWin/IWinProc.h>
#include <Spore/UTFWin/StdDrawable.h>
#include "Settings.h"
#include "BalancedSpecialties.h"
#include "OffensivePolicy.h"
#include "NativeExtensions.h"
#include "BetterCiv.h"
#include "ConversionDiagnostics.h"
#include "CombatDiagnostics.h"
#include "MovementDiagnostics.h"
#include "ConversionCapacity.h"
#include "ReferenceCode.h"
#include "MovementRecovery.h"
#include "MovementPlacement.h"
#include "MovementNative.h"
#include "FleetPolicy.h"
#include "FleetNative.h"
#include "TradeRouteGeometry.h"
#include "TradeRouteNative.h"
#include "TradeTransit.h"

namespace BetterCiv {
using City = Simulator::cCity;
using Civilization = Simulator::cCivilization;
using Vehicle = Simulator::cVehicle;

static_assert(kMilitary == kVehicleMilitary && kReligious == kVehicleCultural &&
    kEconomic == kVehicleEconomic, "Settings use SDK vehicle-purpose IDs.");

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
    100, 100, 8, 15, 15, 4000, 4000, kAllSpecialties, 0, TURRETS_VANILLA, 1, 1, 0, 0, 0,
};
static BalancedSpecialties starting_specialties;
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
using RegisterPortrait = void (__thiscall*)(Native::RivalPortraitPanel*, Civilization*);
using RestorePortrait = void (__thiscall*)(Native::RivalPortraitPanel*, int, Civilization*);
using ReadLocalizedText = const char16_t* (__thiscall*)(LocalizedString*);
static RegisterPortrait original_register_portrait;
static RestorePortrait original_restore_portrait;
static ReadLocalizedText read_localized_text;
static bool portrait_pagination_enabled;
static bool pager_owns_panel(const Native::RivalPortraitPanel* panel);

using CivModelWorld = Graphics::IModelWorld* (__cdecl*)();
static CivModelWorld original_civ_model_world;
static Graphics::IModelWorld* __cdecl rechecked_civ_model_world()
{
    auto* world = original_civ_model_world();
    if (game_mode() == kCivilizationMode && Native::RecheckCompletedCivPreloads(world))
        ModAPI::Log("[Better Civ] Rechecking completed Civilization model preloads.");
    return world;
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
    OptionReader options(&settings);
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
        options.consume(start);
    }
    options.finish();
    fclose(file);
}

static int in_civilization(void)
{
    return game_mode() == kCivilizationMode;
}

using UpdateConversion = void (__thiscall*)(City*);
static UpdateConversion original_update_conversion;
static unsigned conversion_capacity_repairs_logged;
member_detour(initialized_city_conversion, City, void()) {
    void detoured()
    {
        if (in_civilization() && Conversion::InitializeMissingCapacity(*this) &&
            settings.ai_diagnostics && conversion_capacity_repairs_logged < 64) {
            ++conversion_capacity_repairs_logged;
            ModAPI::Log("[Better Civ Conversion] capacity-initialized city=%u owner=%u slots=4 listening=10 converted=2; native conversion follows.",
                mID, mPoliticalID);
        }
        original_update_conversion(this);
    }
};

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

// The planner and tooltips read paletteItemCost, not cCivilization's float
// vehicle cost. Keep both paths on the same multiplier without editing the
// shared property list (which can also be used outside Civilization).
static constexpr uint32_t kPaletteItemCost = 0x02166464;
static constexpr uint32_t kPaletteItemType = 0x02196ad5;

static bool is_discounted_vehicle_item(const App::PropertyList* list, uint32_t property_id)
{
    if (property_id != kPaletteItemCost || settings.cost_percent == 100 || !in_civilization())
        return false;
    uint32_t type = 0;
    return App::Property::GetKeyInstanceID(list, kPaletteItemType, type) &&
        type == Palettes::PaletteItem::kItemCityVehicle;
}

static int32_t palette_vehicle_price(int32_t native)
{
    // Zero/negative values are UI sentinels; the offered percentages are <= 100.
    return native > 0 ? static_cast<int32_t>(static_cast<int64_t>(native) * settings.cost_percent / 100)
                      : native;
}

member_detour(configured_palette_cost, App::PropertyList, bool(uint32_t property_id, App::Property*& result)) {
    bool detoured(uint32_t property_id, App::Property*& result)
    {
        const bool found = this->GetProperty(property_id, result);
        if (found && result && result->mnType == App::PropertyType::Int32 &&
            is_discounted_vehicle_item(this, property_id)) {
            // Each checked caller consumes the scalar immediately. Never give
            // the engine a stack pointer or modify the original property.
            static thread_local App::Property adjusted;
            adjusted.SetValueInt32(palette_vehicle_price(*result->GetValueInt32()));
            result = &adjusted;
        }
        return found;
    }
};

static bool __cdecl configured_palette_int(const App::PropertyList* list, uint32_t property_id, int32_t& result)
{
    const bool found = App::Property::GetInt32(list, property_id, result);
    if (found && is_discounted_vehicle_item(list, property_id))
        result = palette_vehicle_price(result);
    return found;
}

static const char16_t* discount_hover_text(const char16_t* native, char16_t (&buffer)[512])
{
    if (!native || settings.cost_percent == 100 || !in_civilization()) return native;
    size_t length = 0;
    while (length < 512 && native[length]) ++length;
    if (length == 512) return native;
    size_t begin = 0;
    while (begin < length && (native[begin] < u'0' || native[begin] > u'9')) ++begin;
    if (begin == length) return native;
    size_t end = begin;
    int64_t price = 0;
    char16_t separator = 0;
    while (end < length) {
        const char16_t ch = native[end];
        if (ch >= u'0' && ch <= u'9') {
            price = price * 10 + ch - u'0';
            if (price > INT32_MAX) return native;
        } else if ((ch == u' ' || ch == u'\u00a0' || ch == u'\u202f' || ch == u',' || ch == u'.' || ch == u'\'') &&
                   end + 1 < length && native[end + 1] >= u'0' && native[end + 1] <= u'9') {
            separator = ch;
        } else break;
        ++end;
    }
    int32_t discounted = palette_vehicle_price(static_cast<int32_t>(price));
    char16_t reverse[24];
    size_t digits = 0, count = 0;
    do {
        if (separator && digits && digits % 3 == 0) reverse[count++] = separator;
        reverse[count++] = static_cast<char16_t>(u'0' + discounted % 10);
        discounted /= 10;
        ++digits;
    } while (discounted);
    if (begin + count + length - end >= 512) return native;
    memcpy(buffer, native, begin * sizeof(char16_t));
    for (size_t i = 0; i < count; ++i) buffer[begin + i] = reverse[count - i - 1];
    memcpy(buffer + begin + count, native + end, (length - end + 1) * sizeof(char16_t));
    return buffer;
}

// The three world-view purchase buttons use localized text with a literal
// vanilla price. These exact GetText calls are separate from paletteItemCost.
member_detour(configured_hover_text, LocalizedString, const char16_t*()) {
    const char16_t* detoured()
    {
        static thread_local char16_t buffer[512];
        return discount_hover_text(read_localized_text(this), buffer);
    }
};

static int available_portrait(const Native::RivalPortraitPanel* panel, const Civilization* civilization)
{
    int free_slot = -1;
    for (int i = 0; i < 4; ++i) {
        const auto& slot = panel->slots[i];
        if (slot.occupied && slot.civilization == civilization) return i;
        if (!slot.occupied && free_slot < 0) free_slot = i;
    }
    return free_slot;
}

static void note_portrait_qualification(const Civilization*);
member_detour(safe_register_portrait, Native::RivalPortraitPanel, void(Civilization* civilization)) {
    void detoured(Civilization* civilization)
    {
        if (!civilization) return;
        if (portrait_pagination_enabled && pager_owns_panel(this)) {
            note_portrait_qualification(civilization);
            // First-conquest eligibility is set by the caller. The next UI
            // update places this rival in the roster without changing pages.
            return;
        }
        if (available_portrait(this, civilization) >= 0) {
            original_register_portrait(this, civilization);
        } else {
            civilization->mCommRelPanelSlot = -1;
            ModAPI::Log("[Better Civ] Rival portrait panel full; extra nation remains active.");
        }
    }
};

member_detour(safe_restore_portrait, Native::RivalPortraitPanel, void(int slot, Civilization* civilization)) {
    void detoured(int slot, Civilization* civilization)
    {
        if (!civilization) return;
        if (portrait_pagination_enabled && (slot < 0 || slot >= 4)) {
            // Hidden-page rivals have no native slot. Do not let an early
            // hidden rival steal a later rival's valid saved visible slot.
            civilization->mCommRelPanelSlot = -1;
            return;
        }
        for (int i = 0; i < 4; ++i)
            if (this->slots[i].civilization == civilization) { slot = i; break; }
        if (slot < 0 || slot >= 4 ||
            (this->slots[slot].occupied && this->slots[slot].civilization != civilization))
            slot = available_portrait(this, civilization);
        civilization->mCommRelPanelSlot = slot;
        if (slot >= 0) original_restore_portrait(this, slot, civilization);
    }
};

#include "Portraits.h"

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

static uint32_t live_trade_partner_id(const Simulator::cCityPartner& partner)
{
    const auto* wanted=Native::TradePartnerCity(partner);
    if(!wanted)return UINT32_MAX;
    for(const auto& civ:portrait_civilizations()) {
        if(!civ || civ->mbIsDestroyed)continue;
        for(const auto& city:civ->mCities)
            if(city.get()==wanted && !city->mbIsDestroyed && city->mpCivilization.get()==civ.get())
                return political_id(civ.get());
    }
    return UINT32_MAX;
}
static auto trade_partner_id=live_trade_partner_id;
static int has_rival_trade(Civilization* civilization, uint32_t own_id, uint32_t player_id)
{
    for (const auto& city : civilization->mCities) {
        if (!city) continue;
        for (const auto& partner : city->mTradePartners) {
            const auto id = trade_partner_id(partner);
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

// Optional observer: only identifiers and values survive a simulation callback.
// Borrowed city/vehicle pointers are never kept across ticks or serialized.
using ExpansionPlanner = void (__thiscall*)(Civilization*, int);
static ExpansionPlanner original_expansion_planner;
using SetVehicleOrder = void (__thiscall*)(Vehicle*, void*, int, int);
static SetVehicleOrder set_vehicle_order;
static uint64_t ai_time;
static bool ai_observer_ready;
static unsigned ai_log_lines;
static Conversion::Journal conversion_journal;
static void observe_conversion();
static CombatDiagnostics::Journal combat_journal;
static void observe_combat();
static Movement::Journal movement_journal;
static void observe_movement();
struct AIRecord {
    uint64_t key = 0, last_seen = 0, next_report[2] = {};
    Offensive::State attacks[2];
    Offensive::Cooldown avoided[2];
    uint32_t cities[16] = {};
    unsigned city_count = 0;
    int captures[3] = {};
    bool seen = false;
};
static AIRecord ai_records[32];

static uint64_t object_key(const Simulator::cGameData* object)
{
    return object ? (uint64_t(object->mID) << 32) | reinterpret_cast<uintptr_t>(object) : 0;
}

static bool trade_transit_native_ready;
static bool trade_detour_phase(Vehicle&);
static bool (*trade_detour_check)(Vehicle&)=trade_detour_phase;
#include "MovementRecoveryAdapter.h"
#include "MovementPlacementAdapter.h"
#include "MovementRequestAdapter.h"
#include "FleetAdapter.h"
#include "TradeRouteAdapter.h"
#include "TradeTransitAdapter.h"
#include "HarborDiagnosticsAdapter.h"

static bool ai_log_allowed()
{
    if (!settings.ai_diagnostics) return false;
    if (ai_log_lines >= 4096) return false;
    if (++ai_log_lines == 4096) {
        ModAPI::Log("[Better Civ AI] Log limit reached; further diagnostics suppressed until restart.");
        return false;
    }
    return true;
}

static void reset_ai_observations()
{
    starting_specialties.Reset();
    conversion_journal.ResetSession();
    combat_journal.ResetSession();
    movement_journal.ResetSession();
    recovery_state.ResetSession();
    placement_state.ResetSession();
    fleet_state.ResetSession();
    trade_route_state.ResetSession();
    trade_transit_state.ResetSession();
    harbor_next_sample=harbor_route_hash=0;
    reset_portrait_history();
    reset_portrait_call_alerts();
    reset_portrait_specialties();
    reset_portrait_pagination();
    reset_portrait_order();
    for (auto& record : ai_records) record = {};
    ai_time = 0;
    if (ai_log_allowed()) ModAPI::Log("[Better Civ AI] Observation state reset; no attack history loaded from saves.");
}

// SDK lifecycle callbacks supply simulated time and discard state on load/mode
// transitions. Nothing is written to the strategy's serialization stream.
class AIObservationLifecycle final : public Simulator::cStrategy {
public:
    int AddRef() override { return 1; } // Static object: lives as long as the DLL.
    int Release() override { return 1; }
    void Initialize() override { reset_ai_observations(); }
    void Dispose() override {
        ai_observer_ready = false; portrait_reuse_ready = false; portrait_pagination_enabled = false;
        reset_ai_observations();
    }
    const char* GetName() const override { return "BetterCivAIObserver"; }
    void OnModeExited(uint32_t oldMode, uint32_t newMode) override {
        Simulator::cStrategy::OnModeExited(oldMode, newMode); reset_ai_observations();
    }
    void OnModeEntered(uint32_t oldMode, uint32_t newMode) override {
        Simulator::cStrategy::OnModeEntered(oldMode, newMode); reset_ai_observations();
    }
    bool Write(Simulator::ISerializerStream*) override { save_portrait_order(); return true; }
    bool Read(Simulator::ISerializerStream*) override { reset_ai_observations(); return true; }
    void OnLoad(const Simulator::cSavedGameHeader&) override { reset_ai_observations(); }
    bool WriteToXML(Simulator::XmlSerializer*) override { return false; }
    void Update(int, int deltaGameTime) override {
        const bool civ_mode = in_civilization();
        if (civ_mode && deltaGameTime > 0) ai_time += uint64_t(deltaGameTime);
        if (conversion_journal.SampleDue(settings.ai_diagnostics != 0,
            ai_observer_ready, civ_mode, deltaGameTime, ai_time)) observe_conversion();
        if (combat_journal.SampleDue(settings.ai_diagnostics != 0,
            ai_observer_ready, civ_mode, deltaGameTime, ai_time)) observe_combat();
        if (movement_journal.SampleDue(settings.ai_diagnostics != 0,
            ai_observer_ready, civ_mode, deltaGameTime, ai_time)) observe_movement();
        recover_blocked_movement(deltaGameTime, civ_mode);
        manage_unused_fleets(deltaGameTime,civ_mode);
        repair_trade_routes(deltaGameTime,civ_mode);
        observe_harbors(deltaGameTime,civ_mode);
    }
};
static AIObservationLifecycle ai_lifecycle;

static AIRecord& record_for(Civilization* civilization)
{
    const auto key = object_key(civilization);
    AIRecord* oldest = &ai_records[0];
    for (auto& record : ai_records) {
        if (record.key == key) { record.last_seen = ai_time; return record; }
        if (!record.key || record.last_seen < oldest->last_seen) oldest = &record;
    }
    *oldest = {};
    oldest->key = key; oldest->last_seen = ai_time;
    return *oldest;
}

static void observe_territory(Civilization* civilization, AIRecord& record)
{
    uint32_t cities[16] = {};
    unsigned count = 0;
    for (const auto& city : civilization->mCities)
        if (city && count < 16) cities[count++] = city->mID;
    const int captures[] = {civilization->mMilitaryCaptureCount,
        civilization->mReligiousCaptureCount, civilization->mEconomicCaptureCount};
    if (record.seen) {
        for (unsigned i = 0; i < count; ++i) {
            bool known = false;
            for (unsigned j = 0; j < record.city_count; ++j) known |= cities[i] == record.cities[j];
            if (!known && ai_log_allowed()) ModAPI::Log("[Better Civ AI] civ=%u gained-city=%u", civilization->mID, cities[i]);
        }
        for (unsigned i = 0; i < record.city_count; ++i) {
            bool present = false;
            for (unsigned j = 0; j < count; ++j) present |= record.cities[i] == cities[j];
            if (!present && ai_log_allowed()) ModAPI::Log("[Better Civ AI] civ=%u lost-city=%u", civilization->mID, record.cities[i]);
        }
        for (unsigned i = 0; i < 3; ++i)
            if (captures[i] > record.captures[i] && ai_log_allowed())
                ModAPI::Log("[Better Civ AI] civ=%u capture-counter type=%u delta=%d (0=military 1=religious 2=buyout)",
                    civilization->mID, i, captures[i] - record.captures[i]);
    }
    memcpy(record.cities, cities, sizeof cities);
    memcpy(record.captures, captures, sizeof captures);
    record.city_count = count; record.seen = true;
}

static bool order_attacks_city(const VehicleOrder* order, City* target, int specialty)
{
    if (!order || !target || order->kind != specialty + 2) return false;
    if (order->target == target) return true;
    if (specialty == kReligious && order->target) {
        auto* object = static_cast<Simulator::cGameData*>(order->target);
        auto* cultural = static_cast<Simulator::cCulturalTarget*>(object->Cast(Simulator::cCulturalTarget::TYPE));
        return cultural && cultural->GetGameDataOwner() == static_cast<Simulator::cGameData*>(target);
    }
    return false;
}

static bool near_attack_city(const Vehicle* vehicle, const City* target)
{
    // Arrival heuristic, not proof of firing: city footprint plus this unit's
    // actual weapon range. Missing/invalid geometry never starts the timer.
    const auto* weapon = vehicle->GetWeapon();
    if (!weapon) return false;
    const float footprint = target->mFootprintRadius;
    const float range = weapon->GetMaxRange();
    if (!std::isfinite(footprint) || !std::isfinite(range) || footprint <= 0 || range <= 0) return false;
    const auto delta = vehicle->mPosition - target->mPosition;
    const float distance2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    const float radius = footprint + range;
    return std::isfinite(distance2) && std::isfinite(radius * radius) && distance2 <= radius * radius;
}

static void observe_conversion()
{
    // Reuse the SDK civilization enumeration. All pointers are borrowed for
    // this synchronous callback; this observer never issues an order or writes
    // a game object, and never calls visibility/model creation methods.
    const auto& civs = portrait_civilizations();
    for (const auto& defender : civs) {
        if (!defender || defender->mbIsDestroyed) continue;
        for (const auto& city : defender->mCities) {
            if (!city || city->mbIsDestroyed) continue;
            auto snapshot = Conversion::ReadCity(*city);
            for (const auto& attacker : civs) {
                if (!attacker || attacker->mbIsDestroyed || attacker.get() == defender.get()) continue;
                for (const auto& unit : attacker->mVehicles) {
                    if (!unit || unit->mbDead || unit->mbIsDestroyed || unit->mPurpose != kReligious) continue;
                    if (!order_attacks_city(current_vehicle_order(unit.get()), city.get(), kReligious)) continue;
                    ++snapshot.ordered;
                    snapshot.nearby += near_attack_city(unit.get(), city.get());
                    Conversion::CountAim(snapshot, *city, *unit);
                }
            }
            auto& record = conversion_journal.For(object_key(city.get()), ai_time);
            if (!record.Observe(snapshot, ai_time)) continue;
            const unsigned count = (snapshot.targets < Conversion::kMaxSlots ? snapshot.targets : Conversion::kMaxSlots);
            if (!conversion_journal.Reserve(1 + count)) {
                conversion_journal.lines = Conversion::kMaxLines;
                break;
            }
            char name[192] = {};
            WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(city->mName.c_str()),
                -1, name, sizeof name, nullptr, nullptr);
            ModAPI::Log("[Better Civ Conversion] t=%llu city=%u name=\"%s\" owner=%u specialty=%d in-view=%d view-object=%d camera-distance=%.1f progress=%.4f credited=%u ordered=%u nearby=%u aim-culture=%u aim-turret=%u aim-other=%u aim-none=%u targets=%u buckets=%u state0=%u state1=%u state2=%u registered=%u turrets=%u frozen=%u",
                ai_time, snapshot.city, name, snapshot.owner, snapshot.specialty, snapshot.in_view, snapshot.has_view, snapshot.distance,
                snapshot.progress, snapshot.credited, snapshot.ordered, snapshot.nearby, snapshot.cultural_aim,
                snapshot.turret_aim, snapshot.other_aim, snapshot.no_aim, snapshot.targets, snapshot.buckets,
                snapshot.disabled, snapshot.ready, snapshot.active, snapshot.registered, snapshot.turrets, snapshot.frozen);
            for (unsigned i = 0; i < count; ++i) {
                const auto& s = snapshot.slots[i];
                ModAPI::Log("[Better Civ Conversion] t=%llu city=%u slot=%u target=%u state=%d attacker=%u registered=%u reservations=%u listening=%d/%d converted=%d/%d",
                    ai_time, snapshot.city, i, s.id, s.state, s.attacker, s.registered, s.reservations,
                    s.listening, s.listening_capacity, s.converted, s.converted_capacity);
            }
        }
    }
    if (conversion_journal.lines >= Conversion::kMaxLines && !conversion_journal.limit_notified) {
        conversion_journal.limit_notified = true;
        ModAPI::Log("[Better Civ Conversion] Log limit reached; restart Spore to start a new diagnostic session.");
    }
}

#include "CombatObserver.h"
#include "MovementObserver.h"

static unsigned cancel_city_attack(Civilization* civilization, City* target, int specialty)
{
    if (!set_vehicle_order || civilization->mIsPlayerOwned || !target || specialty < 0 || specialty > 1) return 0;
    // SDK references keep units alive if a native behavior callback changes the
    // civilization's vector. No snapshot survives this synchronous call.
    const auto vehicles = civilization->mVehicles;
    unsigned canceled = 0;
    for (const auto& vehicle : vehicles) {
        if (!vehicle || vehicle->mbDead || vehicle->mbIsDestroyed || vehicle->mPurpose != specialty) continue;
        if (!order_attacks_city(current_vehicle_order(vehicle.get()), target, specialty)) continue;
        set_vehicle_order(vehicle.get(), nullptr, 0, 0);
        ++canceled;
    }
    return canceled;
}

static Offensive::Observation observe_attack(Civilization* civilization, City* target, int specialty)
{
    Offensive::Observation observation;
    observation.now = ai_time;
    for (const auto& city : civilization->mCities)
        if (city) observation.territory += object_key(city.get());
    auto* owner = target ? city_owner(target) : nullptr;
    if (!owner || owner == civilization || target->mbIsDestroyed) return observation;
    const auto own_id = political_id(civilization);
    const auto owner_id = political_id(owner);
    auto* manager = relationship_manager();
    if (!manager || relationship_level(manager, own_id, owner_id, 1) > 1) return observation;
    observation.target = object_key(target);
    observation.owner = owner_id;
    // Read the shared gauge even when another attacker is credited. Otherwise
    // changing mCaptureID manufactures a zero followed by false progress.
    observation.progress = specialty == kMilitary ? target->mCapturePercent : target->mConvertPercent;
    for (const auto& vehicle : civilization->mVehicles) {
        if (!vehicle || vehicle->mbDead || vehicle->mbIsDestroyed ||
            vehicle->mPurpose != specialty) continue;
        const auto* order = current_vehicle_order(vehicle.get());
        if (!order || order->kind != specialty + 2) continue;
        if (order_attacks_city(order, target, specialty)) {
            ++observation.committed;
            if (settings.stalled_seconds && near_attack_city(vehicle.get(), target)) ++observation.nearby;
            continue;
        }
        // Unknown subtarget/defense orders are a veto, never proof of defeat.
        ++observation.uncertain;
    }
    return observation;
}

struct SelectionFilter { Civilization* origin; City* excluded; const Offensive::Cooldown* avoided; bool used; };
static thread_local SelectionFilter* selection_filter;
member_detour(filtered_expansion_owner, City, Civilization*()) {
    Civilization* detoured() {
        auto* owner = city_owner(this);
        if (selection_filter && (selection_filter->excluded == this ||
            (selection_filter->avoided && owner && selection_filter->avoided->Blocks(
                object_key(this), political_id(owner), ai_time)))) {
            selection_filter->used = true;
            // Only the candidate-enumeration call is adapted: the native
            // selector skips its own cities. Actual ownership is never changed.
            return selection_filter->origin;
        }
        return owner;
    }
};

member_detour(observed_expansion_planner, Civilization, void(int specialty)) {
    void detoured(int specialty) {
        auto* civilization = this;
        if (!ai_observer_ready || !in_civilization() || civilization->mIsPlayerOwned ||
            civilization->mbIsDestroyed || civilization->mCities.empty() || specialty < 0 || specialty > 1) {
            original_expansion_planner(civilization, specialty); return;
        }
        auto& record = record_for(civilization);
        auto& state = record.attacks[specialty];
        auto& avoided = record.avoided[specialty];
        auto& target_slot = specialty == kMilitary ? civilization->mMilitaryExpansionCity : civilization->mReligiousExpansionCity;
        observe_territory(civilization, record);
        const auto observation = observe_attack(civilization, target_slot.get(), specialty);
        const auto event = state.Observe(observation, civilization->mID ^ uint32_t(specialty));
        const char* events[] = {"none", "target-observed", "wave-started", "progress", "wave-failed", "wave-ended-with-progress"};
        if (event != Offensive::Event::None && event != Offensive::Event::Progress && ai_log_allowed())
            ModAPI::Log("[Better Civ AI] t=%llu civ=%u type=%d event=%s target=%u units=%u uncertain=%u peak=%u failures=%u/%u progress=%.4f",
                ai_time, civilization->mID, specialty, events[int(event)], uint32_t(observation.target >> 32),
                observation.committed, observation.uncertain, state.peak_units, state.failures, state.threshold, observation.progress);
        if (ai_time >= record.next_report[specialty]) {
            record.next_report[specialty] = ai_time + 30000;
            unsigned production_cities = 0;
            for (const auto& city : civilization->mCities)
                if (city && city->mVehicleSpecialty == specialty) ++production_cities;
            const char* status = observation.committed ? "forces-assigned" : observation.uncertain ? "other-attack-orders" :
                !production_cities ? "no-city-of-type" : !observation.target ? "no-hostile-target" : "no-assigned-force";
            if (ai_log_allowed()) ModAPI::Log("[Better Civ AI] t=%llu civ=%u type=%d status=%s cities=%u vehicles=%u wealth=%.0f expansion-budget=%.0f units=%u uncertain=%u progress=%.4f failures=%u/%u military-target=%u religious-target=%u forced-target=%u",
                ai_time, civilization->mID, specialty, status, unsigned(civilization->mCities.size()), unsigned(civilization->mVehicles.size()),
                civilization->mWealth, civilization->mExpansionBudget,
                observation.committed, observation.uncertain, observation.progress, state.failures, state.threshold,
                civilization->mMilitaryExpansionCity ? civilization->mMilitaryExpansionCity->mID : UINT32_MAX,
                civilization->mReligiousExpansionCity ? civilization->mReligiousExpansionCity->mID : UINT32_MAX,
                civilization->mDiploAttackCity ? civilization->mDiploAttackCity->mID : UINT32_MAX);
            if (settings.stalled_seconds && ai_log_allowed())
                ModAPI::Log("[Better Civ AI] t=%llu civ=%u type=%d siege target=%u nearby=%u engaged=%d net-peak=%.4f no-advance-ms=%llu",
                    ai_time, civilization->mID, specialty, uint32_t(observation.target >> 32), observation.nearby,
                    state.engaged, state.progress, state.engaged ? ai_time - state.last_advance : 0);
        }
        const uint64_t old_target = object_key(target_slot.get());
        const bool stalled = !civilization->mDiploAttackCity &&
            state.IsStalled(observation, uint64_t(settings.stalled_seconds) * 1000);
        if (stalled) {
            cCityPtr retained_target = target_slot;
            avoided = {observation.target, ai_time + Offensive::kAvoidMs, observation.owner};
            const unsigned canceled = cancel_city_attack(civilization, retained_target.get(), specialty);
            target_slot = nullptr;
            if (ai_log_allowed()) ModAPI::Log("[Better Civ AI] t=%llu civ=%u type=%d stalled-abandon city=%u canceled=%u progress=%.4f net-peak=%.4f avoid-until=%llu",
                ai_time, civilization->mID, specialty, retained_target->mID, canceled,
                observation.progress, state.progress, avoided.until);
            state = {};
        }
        const bool reconsider = settings.rethink_offensives && !civilization->mDiploAttackCity && state.CanReconsider(observation);
        // Keep the old target alive only during this call, using SDK ownership.
        cCityPtr retained = reconsider ? target_slot : nullptr;
        const bool cooling = settings.stalled_seconds && !civilization->mDiploAttackCity && avoided.target && ai_time < avoided.until;
        SelectionFilter filter{civilization, retained.get(), cooling ? &avoided : nullptr, false};
        auto* previous_filter = selection_filter;
        selection_filter = reconsider || cooling ? &filter : nullptr;
        original_expansion_planner(civilization, specialty);
        selection_filter = previous_filter;
        if (filter.used && reconsider) {
            if (!target_slot && retained && !retained->mbIsDestroyed) target_slot = retained;
            const bool changed = target_slot && target_slot != retained;
            if (ai_log_allowed()) ModAPI::Log("[Better Civ AI] t=%llu civ=%u type=%d rethink=%s old-city=%u new-city=%u",
                ai_time, civilization->mID, specialty, changed ? "alternative-selected" : "no-alternative",
                uint32_t(old_target >> 32), target_slot ? target_slot->mID : UINT32_MAX);
            if (!changed) state.NoAlternative(ai_time);
        }
        if (old_target != object_key(target_slot.get())) {
            if (ai_log_allowed()) ModAPI::Log("[Better Civ AI] t=%llu civ=%u type=%d target-change source=%s from=%u to=%u",
                ai_time, civilization->mID, specialty, stalled ? "stalled" : filter.used ? "rethink" : "native",
                uint32_t(old_target >> 32), target_slot ? target_slot->mID : UINT32_MAX);
            // Do not count an order reassignment as a defeated offensive.
            state = {};
        }
    }
};

struct BranchSite {
    uintptr_t site;
    unsigned char expected[5];
    uintptr_t replacement;
    unsigned char *location;
    DWORD protection;
    int enabled;
    // Zero: relative call/jump. Otherwise: opcode for mov eax/edx, imm32.
    unsigned char load_target_opcode = 0;
};

static void write_branch(BranchSite *site)
{
    if (site->load_target_opcode) {
        const uint32_t address = static_cast<uint32_t>(site->replacement);
        memcpy(site->location + 1, &address, sizeof address);
        site->location[0] = site->load_target_opcode;
        return;
    }
    uint32_t displacement = (uint32_t)(site->replacement - (uintptr_t)site->location - 5);
    memcpy(site->location + 1, &displacement, sizeof displacement);
    // Use a tail jump for the predicate entry, keeping its cdecl caller and float result.
    if (site->site == kVehicleRetreatCondition || site->site == 0xc9ea20 || site->site == 0xdcc7c0)
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
    const bool request_guard=Recovery::ValidateNative(base,uint32_t(reinterpret_cast<uintptr_t>(base))-kReferenceBase);
    const bool transit_guard=TradeTransit::ValidateNative(base,uint32_t(reinterpret_cast<uintptr_t>(base))-kReferenceBase);
    BranchSite sites[] = {
        {0xbf75d2,{0xe8,0xd9,0x31,0x0b,0x00},(uintptr_t)guarded_ai_land_sale,NULL,0,
            settings.rebalance_land && Fleet::ValidateNative(base,uint32_t(reinterpret_cast<uintptr_t>(base))-kReferenceBase) &&
            Fleet::MatchNativeSaleCaller(base)},
        {0xdca6b9,{0xe8,0xc2,0x88,0xe7,0xff},(uintptr_t)trade_proposal_arrival,NULL,0,transit_guard && settings.retry_blocked_paths==2},
        {0xdca0b0,{0xe8,0xab,0xd2,0xed,0xff},(uintptr_t)feasible_trade_order::detoured_function,NULL,0,transit_guard && (settings.assist_rival_trade || settings.retire_stranded)},
        {0xdcc7f1,{0xe8,0x6a,0xab,0xed,0xff},(uintptr_t)feasible_trade_order::detoured_function,NULL,0,transit_guard && (settings.assist_rival_trade || settings.retire_stranded)},
        {0xdcc7c0, {0x83,0xec,0x0c,0x53,0x55}, (uintptr_t)guarded_trade_update, NULL, 0,
            transit_guard && (settings.retry_blocked_paths==2 || settings.ai_diagnostics)},
        {0xc9ea20, {0xe9,0x7b,0x55,0xfa,0xff}, (uintptr_t)retained_local_request, NULL, 0, settings.retry_blocked_paths==2 && request_guard},
        // Before the normal per-city conversion simulation, independently of
        // camera, AI diagnostics and offensive target options.
        {0xae57e3, {0xe8, 0x28, 0x5f, 0x10, 0x00}, (uintptr_t)initialized_city_conversion::detoured_function, NULL, 0, 1},
        // Only the Civilization loading-state caller, before its native
        // ModelsHavePreloaded(2) check. Other worlds/stages are untouched.
        {0xcfcf0c, {0xe8, 0x0f, 0x07, 0xe4, 0xff}, (uintptr_t)rechecked_civ_model_world, NULL, 0, 1},
        {0xbfbfe5, {0xe8, 0xb6, 0xe7, 0xff, 0xff}, (uintptr_t)observed_expansion_planner::detoured_function, NULL, 0, settings.rethink_offensives || settings.ai_diagnostics || settings.stalled_seconds},
        {0xbfbfee, {0xe8, 0xad, 0xe7, 0xff, 0xff}, (uintptr_t)observed_expansion_planner::detoured_function, NULL, 0, settings.rethink_offensives || settings.ai_diagnostics || settings.stalled_seconds},
        {0xbfaa32, {0xe8, 0x09, 0xf3, 0xfd, 0xff}, (uintptr_t)filtered_expansion_owner::detoured_function, NULL, 0, settings.rethink_offensives || settings.stalled_seconds},
        {0xb9bda0, {0xe8, 0x4b, 0xdc, 0xff, 0xff}, (uintptr_t)found_major_city, NULL, 0, 1},
        {0xbf468d, {0xe8, 0x7e, 0x1f, 0x21, 0x00}, (uintptr_t)safe_register_portrait::detoured_function, NULL, 0, 1},
        {0xe065de, {0xe8, 0xed, 0xfe, 0xff, 0xff}, (uintptr_t)safe_restore_portrait::detoured_function, NULL, 0, 1},
        {0xcfe3fc, {0xe8, 0xff, 0x7d, 0x10, 0x00}, (uintptr_t)reuse_portrait_slots::detoured_function, NULL, 0, 1},
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
        // Replace only the five-byte virtual target load, retaining the native
        // arguments, indirect call, result test and register/stack convention.
        {0xd08195, {0x8b, 0x01, 0x8b, 0x40, 0x24}, (uintptr_t)configured_palette_cost::detoured_function, NULL, 0, settings.cost_percent != 100, 0xb8},
        {0xd08333, {0x8b, 0x01, 0x8b, 0x40, 0x24}, (uintptr_t)configured_palette_cost::detoured_function, NULL, 0, settings.cost_percent != 100, 0xb8},
        {0xd088d6, {0x8b, 0x01, 0x8b, 0x40, 0x24}, (uintptr_t)configured_palette_cost::detoured_function, NULL, 0, settings.cost_percent != 100, 0xb8},
        {0xe4950c, {0x8b, 0x01, 0x8b, 0x40, 0x24}, (uintptr_t)configured_palette_cost::detoured_function, NULL, 0, settings.cost_percent != 100, 0xb8},
        {0x5f1d08, {0x8b, 0x11, 0x8b, 0x52, 0x24}, (uintptr_t)configured_palette_cost::detoured_function, NULL, 0, settings.cost_percent != 100, 0xba},
        {0x5ed8b1, {0xe8, 0x7a, 0x2a, 0xe2, 0xff}, (uintptr_t)configured_palette_int, NULL, 0, settings.cost_percent != 100},
        {0x59e840, {0xe8, 0xeb, 0x1a, 0xe7, 0xff}, (uintptr_t)configured_palette_int, NULL, 0, settings.cost_percent != 100},
        {0xe2bb36, {0xe8, 0x15, 0x9a, 0x88, 0xff}, (uintptr_t)configured_hover_text::detoured_function, NULL, 0, settings.cost_percent != 100},
        {0xe2bdaa, {0xe8, 0xa1, 0x97, 0x88, 0xff}, (uintptr_t)configured_hover_text::detoured_function, NULL, 0, settings.cost_percent != 100},
        {0xe2c04f, {0xe8, 0xfc, 0x94, 0x88, 0xff}, (uintptr_t)configured_hover_text::detoured_function, NULL, 0, settings.cost_percent != 100},
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
    // Validate foundation specialty conversion, native preload layout/cleanup and portrait ownership helpers
    // before enabling any caller or writing patches.
    const struct { uint32_t address, size, hash; } native_helpers[] = {
        {0xbdb350,128,0xf30f9e7c}, // Live partner-city lookup and owner political ID.
        // Capacity setter, conversion consumer, view producer, crowd count
        // generator and its constants. Reject changed semantics before writes.
        {0xbeb710, 270, 0xdf53aaa9}, {0xbeb310, 1024, 0x21f45f84},
        {0xbd9ff0, 44, 0x810e6be0}, {0xd57b00, 719, 0xdd9fbf3b},
        {0xafdf60, 955, 0x91eccdd3},
        {0x13eb950, 4, 0x8af6f615}, {0x1486054, 4, 0x4a090eb5},
        {0x13eed94, 4, 0x9ab9dc58}, {0x145be38, 4, 0x1c95ab18},
        {0x1567354, 4, 0xad8203d6},
        {0xb99c16, 22, 0x6c19fab1}, {0xbefa70, 38, 0xec66066a},
        // Rival founder's player=false argument, and Initialize's store into
        // cCivilization::mIsPlayerOwned. Neither range has relocated operands.
        {0xb99b74, 23, 0xa2312b3e}, {0xbf82f6, 19, 0xece952d5},
        {0x744700, 29, 0xc330f438}, {0x751619, 7, 0x677c05e6},
        {0x753172, 7, 0x78a7044c}, {0x7531bf, 259, 0x12262fd4},
        {0xe01280, 27, 0xce9baa2e},
        {0xe04930, 235, 0x95c8db06}, {0xe052b0, 236, 0xc60c21f9},
        {0xe064d0, 112, 0x230f7833}, {0xe06610, 108, 0x1443577b},
        // Native portrait material parser entry, cdecl; no relocated operands.
        {0x808b80, 66, 0x374443f4},
    };
    for (const auto& helper : native_helpers) {
        uint32_t hash = 0;
        const auto* code = base + (helper.address - kReferenceBase);
        const auto image_delta = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base)) - kReferenceBase;
        if (!Native::HashReferenceCode(code, helper.size, image_delta,
                Native::CodeRelocations(helper.address), hash) || hash != helper.hash) {
            ModAPI::Log("[Better Civ] Reference check failed: address=%08X bytes=%u expected=%08X actual=%08X image-delta=%08X.",
                helper.address, helper.size, helper.hash, hash, image_delta);
            return 0;
        }
    }
    if (settings.stalled_seconds || settings.retire_stranded) {
        // Verified native order setter, including its ret 0x0c. Validate its
        // entire 212-byte body before enabling calls (FNV-1a of reference code).
        uint32_t hash = 2166136261u;
        const auto* code = base + (0xcac1a0 - kReferenceBase);
        for (unsigned i = 0; i < 212; ++i) hash = (hash ^ code[i]) * 16777619u;
        if (hash != 0x6dc24d07u) return 0;
    }
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
    original_register_portrait = reinterpret_cast<RegisterPortrait>(base + (0xe06610 - kReferenceBase));
    original_restore_portrait = reinterpret_cast<RestorePortrait>(base + (0xe064d0 - kReferenceBase));
    original_update_portraits = reinterpret_cast<UpdatePortraits>(base + (0xe06200 - kReferenceBase));
    native_hide_portrait = reinterpret_cast<HidePortrait>(base + (0xe04930 - kReferenceBase));
    native_remove_portrait = reinterpret_cast<RemovePortrait>(base + (0xe052b0 - kReferenceBase));
    native_find_portrait_window = reinterpret_cast<FindPortraitWindow>(base + (0xe01280 - kReferenceBase));
    native_apply_portrait_material = reinterpret_cast<ApplyPortraitMaterial>(base + (0x808b80 - kReferenceBase));
    read_localized_text = reinterpret_cast<ReadLocalizedText>(base + (0x6b5550 - kReferenceBase));
    read_float_property = (ReadFloatProperty)(base + (0x41ea30 - kReferenceBase));
    random_below = (RandomBelow)(base + (0xa68f70 - kReferenceBase));
    simulation_random = base + (0x1601760 - kReferenceBase);
    original_found_city = (FoundCityFunction)(base + (0xb999f0 - kReferenceBase));
    original_update_conversion = reinterpret_cast<UpdateConversion>(base + (0xbeb710 - kReferenceBase));
    original_expansion_planner = reinterpret_cast<ExpansionPlanner>(base + (0xbfa7a0 - kReferenceBase));
    set_vehicle_order = reinterpret_cast<SetVehicleOrder>(base + (0xcac1a0 - kReferenceBase));
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
    original_civ_model_world = reinterpret_cast<CivModelWorld>(base + (0xb3d620 - kReferenceBase));

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
        recovery_image_base = reinterpret_cast<uintptr_t>(base);
        trade_transit_native_ready=transit_guard;
        original_trade_arrival=reinterpret_cast<TradeArrival>(base+0xc42f80-kReferenceBase);
        trade_update_continue=reinterpret_cast<uintptr_t>(base)+0xdcc7c5-kReferenceBase;
        recovery_native_ready = Recovery::ValidateNative(base, uint32_t(recovery_image_base)-kReferenceBase);
        original_local_request=reinterpret_cast<LocalRequestSetter>(base+0xc43fa0-kReferenceBase);
        placement_terrain_native_ready = Recovery::ValidateTerrain(base, uint32_t(recovery_image_base)-kReferenceBase);
        fleet_native_ready=Fleet::ValidateNative(base,uint32_t(recovery_image_base)-kReferenceBase);
        fleet_sell=reinterpret_cast<FleetSell>(base+0xcaa7b0-kReferenceBase);
        fleet_water=reinterpret_cast<FleetWater>(base+0xbdb9d0-kReferenceBase);
        fleet_continent=reinterpret_cast<FleetContinent>(base+0xb886e0-kReferenceBase);
        trade_route_native_ready=placement_terrain_native_ready && TradeRoutes::ValidateNative(base,uint32_t(recovery_image_base)-kReferenceBase);
        trade_path_manager=reinterpret_cast<TradePathManager>(base+0xb3d390-kReferenceBase);
        trade_path_search=reinterpret_cast<TradePathSearch>(base+0xac68a0-kReferenceBase);
        trade_land_profile=base+0x1565b54-kReferenceBase;
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
    if (active) {
        auto* system = Simulator::cSimulatorSystem::Get();
        // Initialization is post-init: register with the live simulation as well
        // as ModAPI's save registry, so Update/OnLoad callbacks actually run.
        ai_observer_ready = system && system->AddStrategy(&ai_lifecycle, 0xbec1a110);
        if (settings.balanced_specialties) {
            ModAPI::Log(ai_observer_ready ? "[Better Civ Start] Balanced 3/3/3 enabled for new rival foundations."
                : "[Better Civ Start] Balanced 3/3/3 unavailable: lifecycle missing; keeping native specialties.");
            if (!ai_observer_ready) settings.balanced_specialties = 0;
        }
        portrait_reuse_ready = ai_observer_ready;
        portrait_pagination_enabled = ai_observer_ready;
        if(settings.assist_rival_trade || settings.retire_stranded)
            ModAPI::Log(trade_transit_native_ready && fleet_native_ready && ai_observer_ready
                ? "[Better Civ Transit] Sea transport eligibility active: proposals and deliveries rechecked before native processing."
                : "[Better Civ Transit] Sea transport eligibility disabled: unsupported code or lifecycle.");
        if(settings.retry_blocked_paths==2 || settings.ai_diagnostics)
            ModAPI::Log(trade_transit_native_ready && ai_observer_ready
                ? (settings.retry_blocked_paths==2
                    ? "[Better Civ Transit] Harbor test 2 active: verified partner cities; delivery phases protected during detours; bounded retries before native path-failure cancellation."
                    : "[Better Civ Transit] Harbor diagnostics active; movement recovery disabled.")
                : "[Better Civ Transit] Disabled: unsupported native code or lifecycle; delivery detours disabled.");
        ModAPI::Log(trade_route_native_ready && ai_observer_ready
            ? "[Better Civ Routes] Fix 1 active: guarded land ribbon repair; trade orders unchanged."
            : "[Better Civ Routes] Disabled: unsupported native code or lifecycle.");
        if (settings.retry_blocked_paths)
            ModAPI::Log(recovery_native_ready && ai_observer_ready
                ? (settings.retry_blocked_paths == 2
                    ? "[Better Civ Placement] Test 2 active: bounded local detours and deep-overlap separation for player and AI."
                    : "[Better Civ Recovery] Test 1 active: guarded same-goal retries for crowded, stationary visible land/sea units; orders retained.")
                : "[Better Civ Recovery] Unavailable: native or lifecycle check failed; keeping vanilla movement.");
        if (settings.retire_stranded || settings.rebalance_land)
            ModAPI::Log(fleet_native_ready && ai_observer_ready ? "[Better Civ Fleet] Fix 1 active: reachable enemy ports, stable local grace and retirement veto diagnostics."
                : "[Better Civ Fleet] Disabled: unsupported native code or lifecycle.");
        if(settings.retire_stranded && fleet_native_ready && ai_observer_ready)
            ModAPI::Log("[Better Civ Fleet] Stranded fleets test 2 active: idle combat ships try enemy ports; bounded retries; assigned missions and player fleets protected.");
        if (settings.rethink_offensives || settings.ai_diagnostics || settings.stalled_seconds)
            ModAPI::Log(ai_observer_ready ? "[Better Civ] Optional AI observer active."
                                         : "[Better Civ] AI observer unavailable; target selection remains native.");
        if (settings.ai_diagnostics && ai_observer_ready)
            ModAPI::Log("[Better Civ Conversion] Diagnostic 2 observer active: specialty=0 military/1 religious/2 economic; 1s samples, 10s reports and state/view transitions.");
        if (settings.ai_diagnostics && ai_observer_ready)
            ModAPI::Log("[Better Civ Combat] Diagnostic 1 active: read-only orders, movement, turret aims and communication transitions; stationary does not mean stuck.");
        if (settings.ai_diagnostics && ai_observer_ready)
            ModAPI::Log("[Better Civ Movement] Diagnostic test 1 active: visible combat, 250ms samples, building targets, bounded trajectory and turret reports; observation only.");
        ModAPI::Log("[Better Civ Conversion] Offscreen capacity fix 2 active; relocation-aware checks, native conversion rates and progress preserved.");
        ModAPI::Log(portrait_reuse_ready ? "[Better Civ] Diplomacy panel slot reuse active."
                                       : "[Better Civ] Diplomacy panel slot reuse unavailable.");
        ModAPI::Log(portrait_pagination_enabled ? "[Better Civ] Diplomacy pagination ready."
                                              : "[Better Civ] Diplomacy pagination unavailable.");
        if (portrait_reuse_ready)
            ModAPI::Log("[Better Civ UI] Portrait call alert test 1 active: native flash rearmed on binding/event changes; audio unchanged.");
        if (portrait_reuse_ready)
            ModAPI::Log("[Better Civ UI] Portrait order test 1 active: append by qualification, UI cache on save, live specialty refresh.");
    }
    ModAPI::Log(active ? "[Better Civ] C++/SDK city rules active."
                      : "[Better Civ] Rules inactive: unsupported executable or conflicting patch.");
}

} // namespace BetterCiv
