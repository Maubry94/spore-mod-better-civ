#pragma once
#include "Settings.h"

namespace BetterCiv {
// A finite bag for the nine starting rivals. No game pointers or save data.
// Tickets are reserved before the synchronous native founder and returned if
// it fails. Exhaustion never silently starts another bag in the same session.
class BalancedSpecialties {
    unsigned remaining_[3] = {3, 3, 3};
public:
    void Reset() { for (auto& count : remaining_) count = 3; }
    unsigned Remaining() const { return remaining_[0] + remaining_[1] + remaining_[2]; }
    int Take(unsigned ticket) {
        if (ticket >= Remaining()) return -1;
        for (int purpose = kMilitary; purpose <= kEconomic; ++purpose) {
            if (ticket < remaining_[purpose]) {
                --remaining_[purpose];
                return purpose;
            }
            ticket -= remaining_[purpose];
        }
        return -1;
    }
    void Return(int purpose) {
        if (purpose >= kMilitary && purpose <= kEconomic && remaining_[purpose] < 3)
            ++remaining_[purpose];
    }
};
}
