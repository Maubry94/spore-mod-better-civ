#pragma once
#include "engine/ReferenceCode.h"

namespace BetterCiv::Recovery {
// SDK MoveTo's vehicle override: false creates a type-1 local request at the
// supplied point, resets transit time and invalidates the locomotion strategy.
// It neither edits mOrders nor mAcceptableCenter/mIdlePosition. Do not use the
// SDK's incompletely described E4/E8 route methods or copy native requests.
inline bool ValidateNative(const unsigned char* base, uint32_t delta) {
    const struct { uint32_t address, size, hash; } guards[] = {
        {0xca35a0,587,0x82439220}, {0xc43fa0,218,0xca990f65},
        {0xc43f40,96,0x285647ac}, {0xc9ea20,5,0x41be670b},
        {0xc9ee70,21,0x5925ffa9}, {0xc41e80,32,0xaace0ab2},
    };
    if (!base) return false;
    for (const auto& g : guards) {
        uint32_t hash;
        if (!Native::HashReferenceCode(base+g.address-0x400000, g.size, delta,
            Native::CodeRelocations(g.address), hash) || hash != g.hash) return false;
    }
    return true;
}
inline bool MatchingMethods(const Simulator::cVehicle& unit, uintptr_t base) {
    const auto* locomotive = static_cast<const Simulator::cLocomotiveObject*>(&unit);
    const auto* table = *reinterpret_cast<const uintptr_t* const*>(locomotive);
    return table && table[0xe0/4] == base+0xca35a0-0x400000 &&
        table[0xdc/4] == base+0xc9ea20-0x400000 && table[0xc4/4] == base+0xc9ee70-0x400000;
}
// This supported ITerrain implementation's slot 30 returns true when its
// active load-job pointer is null (finished), despite the SDK name IsLoading.
// The native SetOnLoadFinish immediately invokes the callback in that state.
// Keep these guards separate so unknown terrain code disables only clearance.
inline bool ValidateTerrain(const unsigned char* base, uint32_t delta) {
    if (!base) return false;
    uint32_t hash;
    return Native::HashReferenceCode(base+0xfa0d40-0x400000,12,delta,{},hash) && hash==0x5f095bfe &&
        Native::HashReferenceCode(base+0x967d90-0x400000,4,delta,{},hash) && hash==0x51abeecc;
}
inline bool MatchingTerrain(const Terrain::ITerrain& terrain, uintptr_t base) {
    const auto* table=*reinterpret_cast<const uintptr_t* const*>(&terrain);
    return table && table[0x30/4]==base+0xfa0d40-0x400000 && table[0x0c/4]==base+0x967d90-0x400000;
}
}
