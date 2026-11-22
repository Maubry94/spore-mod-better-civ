#pragma once
#include <Spore/Simulator/cCity.h>
#include <Spore/Simulator/cCivilization.h>
#include <Spore/Simulator/cVehicle.h>
#include <Spore/Graphics/IModelWorld.h>
#include <cstddef>
#include <cstring>

// Only gaps in the pinned SDK are described here. These are borrowed engine
// records, never constructed, destroyed, or reference-counted by Better Civ.
// All access is gated by the executable and instruction checks in install_rules.
namespace BetterCiv::Native {
// Borrowed Civilization preload list 2. The SDK exposes Model and IModelWorld,
// but not the native completion notification or pending vectors. No ownership
// operations are performed here; the native update moves completed entries.
// Verified against ModelsHavePreloaded (0x744700), completion (0x751619), and
// cleanup (0x753172..0x7532c2), all guarded before installing the caller hook.
struct CivModelPreloads {
    unsigned char unknown[0x5cc];
    bool completion_dirty;
    unsigned char before_list2[0x2b];
    Graphics::Model** begin;
    Graphics::Model** end;
    Graphics::Model** capacity;
};
static_assert(offsetof(CivModelPreloads, completion_dirty) == 0x5cc);
static_assert(offsetof(CivModelPreloads, begin) == 0x5f8);
static_assert(offsetof(CivModelPreloads, end) == 0x5fc);

inline bool RecheckCompletedCivPreloads(Graphics::IModelWorld* world)
{
    if (!world) return false;
    auto& state = *reinterpret_cast<CivModelPreloads*>(world);
    if (state.completion_dirty) return false;
    const auto begin = reinterpret_cast<uintptr_t>(state.begin);
    const auto end = reinterpret_cast<uintptr_t>(state.end);
    const auto capacity = reinterpret_cast<uintptr_t>(state.capacity);
    if (!begin || end <= begin || end > capacity ||
        (end - begin) % sizeof(Graphics::Model*) ||
        end - begin > 4096 * sizeof(Graphics::Model*)) return false;
    for (auto p = state.begin; p != state.end; ++p) {
        const auto* model = *p;
        if (!model || model->mpWorld != world ||
            !(model->mFlags & Graphics::kModelFlagIsLoaded)) return false;
    }
    // Request normal native cleanup next frame. Do not empty the vector, mark
    // incomplete models as ready, or return true from ModelsHavePreloaded.
    state.completion_dirty = true;
    return true;
}

struct CityFoundation {
    unsigned char unknown[0x25];
    bool minorNation;
    unsigned char padding[2];
    // Foundation IDs differ from VehiclePurpose: 0 military, 1 economic,
    // 2 religious. The native founder converts them at 0xBEFA70 before setting
    // cCity::mVehicleSpecialty. Do not use these IDs for cities or vehicles.
    int specialty;
};
static_assert(offsetof(CityFoundation, minorNation) == 0x25);
static_assert(offsetof(CityFoundation, specialty) == 0x28);

// Borrowed request passed through cVehicle's DCh thunk (C9EA20) to C43FA0.
// The SDK declaration omits the orientation area. Read only; never copy its
// owning waypoint vector or construct a native request from this view.
struct LocalPointRequest {
    uintptr_t waypoints_begin, waypoints_end;
    unsigned char before_position[0x0c];
    Math::Vector3 position;
    float stop;
    unsigned char before_relative[0x28];
    bool relative;
    unsigned char before_kind[0x0f];
    int kind;
    float acceptable;
};
static_assert(offsetof(LocalPointRequest, position)==0x14);
static_assert(offsetof(LocalPointRequest, relative)==0x4c);
static_assert(offsetof(LocalPointRequest, kind)==0x5c);
static_assert(offsetof(LocalPointRequest, acceptable)==0x60);

inline int FoundationSpecialtyForVehiclePurpose(int purpose)
{
    switch (purpose) {
    case kVehicleMilitary: return 0;
    case kVehicleCultural: return 2;
    case kVehicleEconomic: return 1;
    default: return -1;
    }
}

// cVehicle::mOrders is explicitly a placeholder in the SDK. Order 9 has a
// cCity* target. Attack orders 2/3 can target a city or a checked subtarget;
// only cast non-city religious targets through cCulturalTarget's SDK TYPE.
struct VehicleOrder {
    void* target;
    Math::Vector3 position;
    int kind;
    uint32_t political_id;
};
static_assert(sizeof(VehicleOrder) == 24);
static_assert(offsetof(VehicleOrder, kind) == 0x10);

// Only passed back to the native acceptance method; its layout is unknown.
struct TradeTuning {};

// Civilization diplomacy UI: four native portrait entries, not one per major
// nation. The allocator at 0xE06610 otherwise falls through with index 4.
struct RivalPortraitSlot {
    uint32_t window_id;
    bool occupied;
    unsigned char unknown[3];
    Simulator::cCivilization* civilization;
};
struct RivalPortraitPanel {
    unsigned char unknown[0x8c];
    RivalPortraitSlot slots[4];
};
static_assert(sizeof(RivalPortraitSlot) == 12);
static_assert(offsetof(RivalPortraitPanel, slots) == 0x8c);
static_assert(offsetof(RivalPortraitSlot, occupied) == 4);
static_assert(offsetof(RivalPortraitSlot, civilization) == 8);
static_assert(offsetof(Simulator::cCivilization, mCommRelPanelSlot) == 0x29c);
static_assert(offsetof(Simulator::cCivilization, mCurrentCommEventId) == 0x2a0);

inline Simulator::cCity* TradePartnerCity(const Simulator::cCityPartner& partner)
{
    // BDB350 loads word 0, then city+590h, then the owner's GetPoliticalID.
    // Both SDK member labels are misleading here. This is a borrowed CITY
    // pointer, not an ID or a road. Validate live membership before using it.
    Simulator::cCity* city;
    std::memcpy(&city, &partner, sizeof city);
    return city;
}

static_assert(sizeof(Simulator::cCityPartner) == 12);
static_assert(sizeof(Simulator::cLayoutSlot) == 0x18);
static_assert(offsetof(Simulator::cCity, mbIsPlayerCity) == 0x33c);
static_assert(offsetof(Simulator::cCity, mBuildings) == 0x340);
static_assert(offsetof(Simulator::cVehicle, mAngularVelocity) == 0x208);
static_assert(offsetof(Simulator::cVehicle, mNeighbors) == 0x2ac);
static_assert(offsetof(Simulator::cVehicle, field_208) == 0x23c);
static_assert(offsetof(Simulator::cVehicle, field_214) == 0x248);
static_assert(offsetof(Simulator::cVehicle, field_240) == 0x274);
static_assert(offsetof(Simulator::cVehicle, field_250) == 0x284);
static_assert(offsetof(Simulator::cVehicle, field_254) == 0x288);
static_assert(offsetof(Simulator::cVehicle, field_270) == 0x2a4);
static_assert(offsetof(Simulator::cCity, mTradePartners) == 0x55c);
static_assert(offsetof(Simulator::cCity, mTurretsLayout) == 0x4b4);
static_assert(offsetof(Simulator::cCivilization, mCities) == 0x9c);
static_assert(offsetof(Simulator::cCivilization, mVehicles) == 0xb0);
static_assert(offsetof(Simulator::cCivilization, mIsPlayerOwned) == 0x89);
static_assert(offsetof(Simulator::cCivilization, mMilitaryExpansionCity) == 0x458);
static_assert(offsetof(Simulator::cCivilization, mReligiousExpansionCity) == 0x45c);
static_assert(offsetof(Simulator::cCity, mCapturePercent) == 0x748);
static_assert(offsetof(Simulator::cCity, mConvertPercent) == 0x750);
static_assert(offsetof(Simulator::cCity, mCultureTargetInfo) == 0x618);
static_assert(offsetof(Simulator::cCity, mCulturalTargets) == 0x62c);
static_assert(offsetof(Simulator::cCulturalTarget, mAttackState) == 0x1d8);
static_assert(offsetof(Simulator::cCulturalTarget, mAttackers) == 0x1f4);
static_assert(offsetof(Simulator::cTurret, mFreezeCount) == 0x684);
static_assert(offsetof(Simulator::cVehicle, mPurpose) == 0xb20);
static_assert(offsetof(Simulator::cVehicle, mCargoType) == 0xb28);
static_assert(offsetof(Simulator::cVehicle, mCargoUnits) == 0xb58);
static_assert(offsetof(Simulator::cVehicle, field_D14) == 0xd14);
static_assert(offsetof(Simulator::cVehicle, field_BCC) == 0xbcc);
}
