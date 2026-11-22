#include "settings/Settings.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

using namespace BetterCiv;

namespace {

struct MarkerFixture {
    std::initializer_list<uint32_t> loaded;
    unsigned lookups = 0;
};

static bool fixture_marker(void* context, uint32_t instance, uint32_t type, uint32_t group) {
    auto& fixture = *static_cast<MarkerFixture*>(context);
    assert(type == 0x00B1B104 && group == 0xE2C5417D);
    ++fixture.lookups;
    for (auto expected : fixture.loaded) if (instance == expected) return true;
    return false;
}

static ResolvedSettings fixture_options(std::initializer_list<uint32_t> instances) {
    MarkerFixture fixture{instances};
    auto result = ResolvePublicSettings(fixture_marker, &fixture);
    assert(fixture.lookups == 34);
    return result;
}

} // namespace

void VerifyPublicSettings() {
    constexpr Settings defaults{
        100, 100, 8, 15, 15, 4000, 4000, kAllSpecialties, 0, TURRETS_VANILLA, 1, 1, 0, 0, 0
    };
    constexpr Settings mixed{
        75, 25, 12, 30, 30, 4000, 15000, kMilitaryMask | kEconomicMask, 1, TURRETS_ALL_SPECIALTIES, 0, 0, 0, 0, 0
    };
    const auto empty = fixture_options({});
    assert(memcmp(&empty.values, &defaults, sizeof defaults) == 0);
    for (const auto& group : empty.groups) assert(group.matches == 0);

    // Physical marker IDs from the shipping packages, independent of the C++
    // enum. This exercises the cost setting before the gameplay hooks read it.
    const auto standard = fixture_options({0x6AA00001, 0x6AA00007, 0x6AA0000D, 0x6AA00010, 0x6AA00013,
        0xF3C37190, 0x57A36CFE, 0xEFCCBFDD, 0x494223B4, 0xB7556A1F, 0xE3C52A97});
    assert(memcmp(&standard.values, &defaults, sizeof defaults) == 0);
    for (const auto& group : standard.groups) assert(group.matches == 1);
    const auto custom = fixture_options({0x6AA00002, 0x6AA00009, 0x6AA0000E, 0x6AA00011, 0x6AA00015,
        0x55A73C3B, 0x8A27B0D8, 0x0720E488, 0x858899DF, 0xB7556A1F, 0xE3C52A97});
    assert(memcmp(&custom.values, &mixed, sizeof mixed) == 0);
    for (const auto& group : custom.groups) assert(group.matches == 1);

    const auto more = fixture_options({0x6AA00003, 0x6AA00008, 0x6AA0000F, 0x6AA00012, 0x6AA00014,
        0xE202EFA9, 0xABAC982A, 0x40193DB4, 0x57A503E3});
    const auto& s = more.values;
    assert(s.cost_percent == 50 && s.firing_interval_percent == 75 && s.offensive_vehicles == 16);
    assert(s.population_min == 15 && s.population_max == 50 && s.wealth_min == 8000 && s.wealth_max == 8000);
    assert(s.balanced_specialties && s.specialty_mask == kAllSpecialties && s.turrets == TURRETS_HARD_MAXIMUM);
    assert(s.rethink_offensives && s.stalled_seconds == 180 && s.retry_blocked_paths == 2 &&
        s.retire_stranded && s.rebalance_land && s.ai_diagnostics);

    for (auto entry : {std::pair<uint32_t,unsigned>{0xAF7C2E5B, kReligiousMask | kEconomicMask},
            {0x55A73C3B, kMilitaryMask | kEconomicMask}, {0x4653D07A, kMilitaryMask | kReligiousMask},
            {0xA927F152, kMilitaryMask}, {0x0D97BEC0, kReligiousMask}, {0xE29495DF, kEconomicMask}}) {
        auto only = fixture_options({entry.first});
        assert(only.values.specialty_mask == entry.second && !only.values.balanced_specialties);
    }
    // Ambiguous choices revert only their group; an unrelated valid marker
    // still applies. Unknown records and the former granular IDs are ignored.
    auto conflict = fixture_options({0x6AA00002, 0x6AA00003, 0x40193DB4, 0xB7556A1F, 0x57A503E3, 0xFFFFFFFF});
    assert(conflict.groups[unsigned(SettingsGroup::VehicleCost)].matches == 2 &&
        conflict.groups[unsigned(SettingsGroup::AIBehavior)].matches == 2);
    assert(conflict.values.cost_percent == 100 && conflict.values.ai_diagnostics && !conflict.values.retry_blocked_paths);
    const auto old = fixture_options({0xDEADBEEF, 0xCD8EE9E9, 0x99456596, 0x1EA8A041, 0xD4D2BC8F, 0x42254490});
    assert(memcmp(&old.values, &defaults, sizeof defaults) == 0);
    for (const auto& group : old.groups) assert(group.matches == 0);
    puts("Public options: loaded resource keys, defaults, prices, presets, fleet slots, stale granular markers and conflicts passed.");
}
