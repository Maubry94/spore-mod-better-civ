#pragma once
#include <cstdint>
#include <Spore/Simulator/cCity.h>

namespace BetterCiv::Conversion {
// The supported executable's city-view setup uses radii 7 and 3, spacing 1.5,
// and floor((pi * radius^2) / (pi * spacing^2) * 0.5) crowd positions.
// Its sole capacity setter copies those counts into the simulation buckets.
// All relevant native routines and constants are checked before enabling this
// fallback. The native view may later initialize its visuals normally.
constexpr int kNativeListeningCapacity = 10;
constexpr int kNativeConvertedCapacity = 2;
constexpr unsigned kNativeCultureSlots = 4;

inline bool InitializeMissingCapacity(Simulator::cCity& city)
{
    if (city.mRoboPopulation <= 0 || city.mConvertPercent != 0 ||
        city.mCultureTargetInfo.size() != kNativeCultureSlots ||
        city.mCulturalTargets.size() != kNativeCultureSlots) return false;
    bool preaching = false;
    for (unsigned i = 0; i < kNativeCultureSlots; ++i) {
        const auto& info = city.mCultureTargetInfo[i];
        const auto* target = city.mCulturalTargets[i].get();
        if (!target || !info.mCultureObject || target->mAttackState < 0 ||
            target->mAttackState > 2) return false;
        for (unsigned bucket = 0; bucket < 2; ++bucket) {
            // Do not overwrite initialized, partial, inconsistent or active
            // conversion data. No population, timer or progress is reset.
            if (info.mBucketMaxPopulation[bucket] != 0 ||
                info.mBucketPopulation[bucket] != 0 ||
                info.mBucketEmigration[bucket] != 0 ||
                info.mBucketImmigration[bucket] != 0) return false;
        }
        preaching |= target->mAttackState == 2 && !target->mAttackers.empty() &&
            target->mAttackerPoliticalID != UINT32_MAX &&
            target->mAttackerPoliticalID != city.mPoliticalID;
    }
    if (!preaching) return false;
    for (auto& info : city.mCultureTargetInfo) {
        info.mBucketMaxPopulation[0] = kNativeListeningCapacity;
        info.mBucketMaxPopulation[1] = kNativeConvertedCapacity;
    }
    return true;
}
}
