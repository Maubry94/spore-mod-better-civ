#pragma once
#include "engine/ReferenceCode.h"
namespace BetterCiv::Fleet {
inline bool MatchNativeSaleCaller(const unsigned char* base) {
    uint32_t hash;
    return base && Native::HashReferenceCode(base + (0xbf74d0 - 0x400000), 271, 0, {}, hash)
        && hash == 0xfe043e9b;
}
inline bool ValidateNative(const unsigned char* base,uint32_t delta) {
    if (!base) return false;
    const struct {uint32_t address,size,hash;} guards[]={
        {0xcaa7b0,741,0x99723fdf},
        {0xc9e950,148,0x91c2da11},
        {0xbdb9d0,165,0xf057ae19},
        {0xb886e0,470,0x64ce6c2f},
    };
    for (const auto& g:guards) {
        uint32_t hash;
        if (!Native::HashReferenceCode(base+g.address-0x400000,g.size,delta,
            Native::CodeRelocations(g.address),hash) || hash!=g.hash) return false;
    }
    return true;
}
}
