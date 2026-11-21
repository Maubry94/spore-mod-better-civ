#include "NativePatches.h"
#include "ReferenceCode.h"
#include <Spore/ModAPI.h>
#include <cstring>

namespace BetterCiv {
void write_branch(BranchSite *site)
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

bool supported_executable(const unsigned char* base)
{
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;

    const auto* headers = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (headers->Signature != IMAGE_NT_SIGNATURE ||
        headers->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        headers->FileHeader.TimeDateStamp != kReferenceTimestamp ||
        (headers->OptionalHeader.SizeOfImage != kReferenceImageSize &&
         headers->OptionalHeader.SizeOfImage != kSteamOctober2024ImageSize))
        return false;
    return true;
}

bool ValidateRuleDependencies(const unsigned char* base, const Settings& settings)
{
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
            return false;
        }
    }
    if (settings.stalled_seconds || settings.retire_stranded) {
        // Verified native order setter, including its ret 0x0c. Validate its
        // entire 212-byte body before enabling calls (FNV-1a of reference code).
        uint32_t hash = 2166136261u;
        const auto* code = base + (0xcac1a0 - kReferenceBase);
        for (unsigned i = 0; i < 212; ++i) hash = (hash ^ code[i]) * 16777619u;
        if (hash != 0x6dc24d07u) return false;
    }
    const unsigned char native_retreat_condition[] = {
        0x8b, 0x44, 0x24, 0x04, 0x80, 0xb8, 0xcc, 0x0b, 0x00, 0x00,
        0x00, 0x74, 0x03, 0xd9, 0xe8, 0xc3, 0xd9, 0xee, 0xc3,
    };
    if (settings.maintain_trade_under_fire &&
        memcmp(base + (kVehicleRetreatCondition - kReferenceBase), native_retreat_condition,
               sizeof native_retreat_condition) != 0)
        return false;

    return true;
}

bool LocateBranchSites(unsigned char* base, BranchSite* sites, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (!sites[i].enabled)
            continue;
        sites[i].location = base + (sites[i].site - kReferenceBase);
        if (memcmp(sites[i].location, sites[i].expected, 5) != 0)
            return false;
    }

    return true;
}

WritableBranches::WritableBranches(BranchSite* sites, size_t count) : sites_(sites), count_(count)
{
    for (; acquired_ < count_; ++acquired_) {
        auto& site = sites_[acquired_];
        if (site.enabled && !VirtualProtect(site.location, 5, PAGE_EXECUTE_READWRITE, &site.protection))
            break;
    }
}

WritableBranches::~WritableBranches()
{
    // Multiple sites may share one page. Only the first acquisition saved its original protection.
    while (acquired_) {
        auto& site = sites_[--acquired_];
        if (site.enabled) {
            DWORD previous;
            VirtualProtect(site.location, 5, site.protection, &previous);
        }
    }
}

void WritableBranches::Apply()
{
    if (!Ready())
        return;
    for (size_t i = 0; i < count_; ++i) {
        if (sites_[i].enabled) {
            write_branch(&sites_[i]);
            FlushInstructionCache(GetCurrentProcess(), sites_[i].location, 5);
        }
    }
}
} // namespace BetterCiv
