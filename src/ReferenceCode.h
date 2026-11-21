#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace BetterCiv::Native {
struct ReferenceRelocations {
    const uint16_t* offsets = nullptr;
    size_t count = 0;
};

// Exact IMAGE_REL_BASED_HIGHLOW operands from the supported executable's
// relocation directory for the guarded routines and profile data. Do not mask
// their bytes: normalize their entire 32-bit values.
inline ReferenceRelocations CodeRelocations(uint32_t reference_address)
{
    static constexpr uint16_t update[] = {0xb5};
    static constexpr uint16_t calculate[] = {
        0xae, 0xd2, 0x10a, 0x136, 0x163, 0x192, 0x1b4,
        0x22e, 0x2b0, 0x304, 0x375, 0x3ae, 0x3c8};
    static constexpr uint16_t view[] = {
        0xcc, 0xe7, 0x103, 0x14d, 0x160, 0x1a0, 0x1cc, 0x27d, 0x29e};
    static constexpr uint16_t crowd[] = {
        0x37, 0x67, 0xa0, 0xed, 0x174, 0x19b, 0x1a3, 0x1b8, 0x1c2};
    static constexpr uint16_t movement[] = {0x7e, 0x121, 0x13b, 0x1dd, 0x1f8};
    static constexpr uint16_t request[] = {0x1f, 0x34, 0x46, 0xb1, 0xb9, 0xbf, 0xc9};
    static constexpr uint16_t fleet_sell[] = {0x8e,0x19f};
    static constexpr uint16_t fleet_refund[] = {0x32,0x89};
    static constexpr uint16_t fleet_water[] = {0x3e,0x53,0x68};
    static constexpr uint16_t fleet_continent[] = {0x66,0x6e,0xad,0xdf,0xe7,0x125,0x149,0x151,0x18f};
    static constexpr uint16_t trade_ac68a0[] = {0x13,0x7d};
    static constexpr uint16_t trade_ac6510[] = {0x31,0x3f,0x49,0x57,0xe1,0x153,0x172,0x1b1,0x1b7,0x1bc,0x1c6,0x1d0,0x216,0x226,0x256,0x25f,0x269,0x273,0x279,0x34b};
    static constexpr uint16_t trade_b3d390[] = {0x1};
    static constexpr uint16_t trade_1565b54[] = {0x0,0x4,0x8,0xc,0x10};
    static constexpr uint16_t trade_c414c0[] = {0xfd,0x146,0x1a4,0x242,0x334,0x3d5,0x404,0x4e7,0x4f5,0x507,0x516,0x5cd,0x606};
    static constexpr uint16_t trade_b8cfd4[] = {0x12c,0x134,0x15d,0x165,0x18e,0x196,0x1ff,0x207,0x22a,0x232,0x255,0x25d,0x2a7};
    static constexpr uint16_t ground_road_publisher[] =
        {0x53,0xa7,0x101,0x156,0x15e,0x1dc,0x213,0x21b,0x223,0x22b,0x2d4,0x2dc,0x2e4,0x2ec};
    static constexpr uint16_t transit_update[] = {0x17,0x93,0xc5,0x119,0x143,0x1f8,0x26c,0x294,0x2f5,0x328,0x34c,0x376,0x3b3,0x46c,0x4dd,0x543,0x560,0x57d};
    static constexpr uint16_t transit_dispatch[] = {0,4,8,12,16,20};
    static constexpr uint16_t transit_message[] = {0x2e,0x33};
    static constexpr uint16_t transit_arrival[] = {0x262,0x26a,0x2e7,0x370,0x374,0x378,0x37c};
    static constexpr uint16_t trade_proposal[] = {0x17,0x89,0x178,0x1dc,0x223,0x2fd,0x320,0x41a,0x4de,0x5a8,0x652,0x685};
    switch (reference_address) {
    case 0xdca090: return {trade_proposal,sizeof trade_proposal/sizeof *trade_proposal};
    case 0xdcc7c0: return {transit_update,sizeof transit_update/sizeof *transit_update};
    case 0xdccd64: return {transit_dispatch,sizeof transit_dispatch/sizeof *transit_dispatch};
    case 0xca73e0: return {transit_message,sizeof transit_message/sizeof *transit_message};
    case 0xc42f80: return {transit_arrival,sizeof transit_arrival/sizeof *transit_arrival};
    case 0xac68a0: return {trade_ac68a0,sizeof trade_ac68a0/sizeof *trade_ac68a0};
    case 0xac6510: return {trade_ac6510,sizeof trade_ac6510/sizeof *trade_ac6510};
    case 0xb3d390: return {trade_b3d390,sizeof trade_b3d390/sizeof *trade_b3d390};
    case 0x1565b54: return {trade_1565b54,sizeof trade_1565b54/sizeof *trade_1565b54};
    case 0xc414c0: return {trade_c414c0,sizeof trade_c414c0/sizeof *trade_c414c0};
    case 0xb8cfd4: return {trade_b8cfd4,sizeof trade_b8cfd4/sizeof *trade_b8cfd4};
    case 0xb8d570: return {ground_road_publisher,sizeof ground_road_publisher/sizeof *ground_road_publisher};

    case 0xcaa7b0: return {fleet_sell, sizeof fleet_sell/sizeof *fleet_sell};
    case 0xc9e950: return {fleet_refund, sizeof fleet_refund/sizeof *fleet_refund};
    case 0xbdb9d0: return {fleet_water, sizeof fleet_water/sizeof *fleet_water};
    case 0xb886e0: return {fleet_continent, sizeof fleet_continent/sizeof *fleet_continent};
    case 0xca35a0: return {movement, sizeof movement / sizeof *movement};
    case 0xc43fa0: return {request, sizeof request / sizeof *request};
    case 0xbeb710: return {update, sizeof update / sizeof *update};
    case 0xbeb310: return {calculate, sizeof calculate / sizeof *calculate};
    case 0xd57b00: return {view, sizeof view / sizeof *view};
    case 0xafdf60: return {crowd, sizeof crowd / sizeof *crowd};
    default: return {};
    }
}

inline bool HashReferenceCode(const unsigned char* code, size_t size,
    uint32_t image_delta, ReferenceRelocations relocations, uint32_t& hash)
{
    hash = 2166136261u;
    if (!code || (relocations.count && !relocations.offsets)) return false;
    for (size_t r = 0; r < relocations.count; ++r) {
        const size_t offset = relocations.offsets[r];
        if (offset > size || size - offset < sizeof(uint32_t) ||
            (r && offset < size_t(relocations.offsets[r - 1]) + sizeof(uint32_t))) return false;
    }
    size_t r = 0;
    for (size_t i = 0; i < size;) {
        if (r < relocations.count && i == relocations.offsets[r]) {
            uint32_t word;
            std::memcpy(&word, code + i, sizeof word);
            word -= image_delta; // Unsigned arithmetic also handles bases below 0x400000.
            for (unsigned byte = 0; byte < 4; ++byte)
                hash = (hash ^ ((word >> (8 * byte)) & 255u)) * 16777619u;
            i += sizeof word;
            ++r;
        } else {
            hash = (hash ^ code[i++]) * 16777619u;
        }
    }
    return true;
}
}
