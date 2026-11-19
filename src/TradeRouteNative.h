#pragma once
#include "ReferenceCode.h"
namespace BetterCiv::TradeRoutes {
inline bool ValidateNative(const unsigned char* base,uint32_t delta) {
    if(!base) return false;
    const struct {uint32_t address,size,hash;} guards[]={
        {0xb87410,48,0xa763fd81},
    {0xb87cea,10,0xe8215410},
    {0xac68a0,171,0x0ff6a7f4}, // path-search
        {0xac6510,912,0xe95ead5c}, // path-request
        {0xac53c0,10,0x0aabdfd2}, // path-completion
        {0xb3d390,6,0x32e5eb8f}, // path-manager
        {0x1565b54,20,0x83747623}, // land-profile
        {0xc414c0,1943,0xe2ee0198}, // road-path-layout
        {0xb8d2e0,263,0x2ed294c4}, // ribbon-cache-layout
        {0xb8cfd4,767,0x8f586850}, // ribbon-consumer
    };
    for(const auto& g:guards) {
        uint32_t hash;
        if(!Native::HashReferenceCode(base+g.address-0x400000,g.size,delta,
            Native::CodeRelocations(g.address),hash)||hash!=g.hash) return false;
    }
    return true;
}
}
