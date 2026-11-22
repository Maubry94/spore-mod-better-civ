#pragma once
#include "settings/Settings.h"
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace BetterCiv {
inline constexpr uint32_t kReferenceBase = 0x400000;
inline constexpr uint32_t kReferenceTimestamp = 1727737552;
inline constexpr uint32_t kReferenceImageSize = 26271744;
inline constexpr uint32_t kSteamOctober2024ImageSize = 26443776;
inline constexpr uint32_t kVehicleRetreatCondition = 0xdc47e0;

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

bool supported_executable(const unsigned char* base);
bool ValidateRuleDependencies(const unsigned char* base, const Settings& settings);
bool LocateBranchSites(unsigned char* base, BranchSite* sites, size_t count);
void write_branch(BranchSite* site);

// Acquire every enabled site before changing the first instruction. Destruction
// restores the original page permissions even after a partial acquisition.
class WritableBranches {
public:
    WritableBranches(BranchSite* sites, size_t count);
    ~WritableBranches();
    WritableBranches(const WritableBranches&) = delete;
    WritableBranches& operator=(const WritableBranches&) = delete;
    bool Ready() const { return acquired_ == count_; }
    void Apply();

private:
    BranchSite* sites_;
    size_t count_;
    size_t acquired_ = 0;
};
} // namespace BetterCiv
