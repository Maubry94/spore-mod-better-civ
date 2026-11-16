#pragma once
#include <Spore/Simulator/cCity.h>
#include <Spore/Simulator/cCivilization.h>
#include <Spore/Simulator/cVehicle.h>
#include <cstddef>
#include <cstring>

// Only gaps in the pinned SDK are described here. These are borrowed engine
// records, never constructed, destroyed, or reference-counted by Better Civ.
// All access is gated by the executable and instruction checks in install_rules.
namespace BetterCiv::Native {
struct CityFoundation {
    unsigned char unknown[0x25];
    bool minorNation;
    unsigned char padding[2];
    int specialty;
};
static_assert(offsetof(CityFoundation, minorNation) == 0x25);
static_assert(offsetof(CityFoundation, specialty) == 0x28);

// cVehicle::mOrders is explicitly a placeholder in the SDK. Order 9 has a
// cCity* target; other order kinds can target other simulation objects.
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

inline uint32_t TradePartnerPoliticalId(const Simulator::cCityPartner& partner)
{
    // The current SDK labels this first word as mpInterCityRoad. The supported
    // executable and v1.0 use it as a political ID. Copy the word without
    // interpreting/dereferencing an intrusive_ptr or changing its refcount.
    uint32_t id;
    std::memcpy(&id, &partner, sizeof id);
    return id;
}

static_assert(sizeof(Simulator::cCityPartner) == 12);
static_assert(sizeof(Simulator::cLayoutSlot) == 0x18);
static_assert(offsetof(Simulator::cCity, mbIsPlayerCity) == 0x33c);
static_assert(offsetof(Simulator::cCity, mTradePartners) == 0x55c);
static_assert(offsetof(Simulator::cCity, mTurretsLayout) == 0x4b4);
static_assert(offsetof(Simulator::cCivilization, mCities) == 0x9c);
static_assert(offsetof(Simulator::cCivilization, mVehicles) == 0xb0);
static_assert(offsetof(Simulator::cCivilization, mIsPlayerOwned) == 0x89);
static_assert(offsetof(Simulator::cVehicle, mPurpose) == 0xb20);
static_assert(offsetof(Simulator::cVehicle, field_BCC) == 0xbcc);
}
