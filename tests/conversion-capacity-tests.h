static int conversion_update_calls;
static bool expect_conversion_capacity;
static void __fastcall mock_conversion_update(City* city, int)
{
    ++conversion_update_calls;
    for (const auto& info : city->mCultureTargetInfo) {
        assert(info.mBucketMaxPopulation[0] == (expect_conversion_capacity ? 10 : 0));
        assert(info.mBucketMaxPopulation[1] == (expect_conversion_capacity ? 2 : 0));
    }
}

static void verify_conversion_capacity()
{
    using namespace Conversion;
    alignas(8) unsigned char city_bytes[sizeof(City)] = {};
    alignas(8) unsigned char bucket_bytes[4 * sizeof(Simulator::tCultureTargetInfo)] = {};
    alignas(8) unsigned char target_bytes[4][sizeof(Simulator::cCulturalTarget)] = {};
    auto* city = reinterpret_cast<City*>(city_bytes);
    auto* buckets = reinterpret_cast<Simulator::tCultureTargetInfo*>(bucket_bytes);
    Simulator::cCulturalTarget* targets[4] = {};
    void* attackers[] = {reinterpret_cast<void*>(uintptr_t(0x1234))};
    city->mID = 12; city->mPoliticalID = 7; city->mRoboPopulation = 30;
    put_pointer(city, 0x618, buckets); put_pointer(city, 0x61c, buckets + 4);
    put_pointer(city, 0x62c, targets); put_pointer(city, 0x630, targets + 4);
    for (unsigned i = 0; i < 4; ++i) {
        targets[i] = reinterpret_cast<Simulator::cCulturalTarget*>(target_bytes[i]);
        put_pointer(buckets + i, 0x78, target_bytes[i] + 0x100);
    }
    auto unchanged = [&]() {
        const auto before = std::string(reinterpret_cast<char*>(bucket_bytes), sizeof bucket_bytes);
        assert(!InitializeMissingCapacity(*city));
        assert(!memcmp(bucket_bytes, before.data(), sizeof bucket_bytes));
    };
    unchanged(); // No active preaching: untouched.
    targets[0]->mAttackState = 2; targets[0]->mAttackerPoliticalID = 8;
    unchanged(); // Active state alone is insufficient without a registered unit.
    put_pointer(targets[0], 0x1f4, attackers); put_pointer(targets[0], 0x1f8, attackers + 1);
    targets[0]->mAttackerPoliticalID = 7; unchanged();
    targets[0]->mAttackerPoliticalID = UINT32_MAX; unchanged();
    targets[0]->mAttackerPoliticalID = 8;
    city->mRoboPopulation = 0; unchanged(); city->mRoboPopulation = 30;
    city->mConvertPercent = 0.25f; unchanged(); city->mConvertPercent = 0;
    put_pointer(city, 0x61c, buckets + 3); unchanged(); put_pointer(city, 0x61c, buckets + 4);
    put_pointer(city, 0x630, targets + 3); unchanged(); put_pointer(city, 0x630, targets + 4);
    auto* fourth = targets[3]; targets[3] = nullptr; unchanged(); targets[3] = fourth;
    put_pointer(buckets + 3, 0x78, nullptr); unchanged();
    put_pointer(buckets + 3, 0x78, target_bytes[3] + 0x100);
    targets[3]->mAttackState = 3; unchanged(); targets[3]->mAttackState = 0;
    for (unsigned slot = 0; slot < 4; ++slot) {
        auto& info = buckets[slot];
        for (unsigned b = 0; b < 2; ++b) {
            for (auto* field : {&info.mBucketMaxPopulation[b], &info.mBucketPopulation[b],
                 &info.mBucketEmigration[b], &info.mBucketImmigration[b]}) {
                *field = 1; unchanged(); *field = -1; unchanged(); *field = 0;
            }
        }
    }
    const auto city_before = std::string(reinterpret_cast<char*>(city_bytes), sizeof city_bytes);
    const auto targets_before = std::string(reinterpret_cast<char*>(target_bytes), sizeof target_bytes);
    unsigned char expected[sizeof bucket_bytes]; memcpy(expected, bucket_bytes, sizeof expected);
    const int capacity[] = {10, 2};
    for (unsigned i = 0; i < 4; ++i) memcpy(expected + i * sizeof(*buckets), capacity, sizeof capacity);
    assert(InitializeMissingCapacity(*city));
    assert(!memcmp(bucket_bytes, expected, sizeof expected)); // Only eight capacity integers change.
    assert(!memcmp(city_bytes, city_before.data(), sizeof city_bytes));
    assert(!memcmp(target_bytes, targets_before.data(), sizeof target_bytes));
    unchanged(); // Repeat calls preserve existing initialization.

    // Recreate the stalled-save shape and call the production adapter with
    // the engine's __thiscall ABI. The native update always runs exactly once.
    for (auto& info : city->mCultureTargetInfo) info.mBucketMaxPopulation[0] = info.mBucketMaxPopulation[1] = 0;
    const auto saved_mode = game_mode; const auto saved_current_mode = current_mode;
    const auto saved_update = original_update_conversion; const auto saved_settings = settings;
    const auto saved_logs = conversion_capacity_repairs_logged;
    game_mode = reinterpret_cast<decltype(game_mode)>(mock_mode);
    original_update_conversion = reinterpret_cast<UpdateConversion>(mock_conversion_update);
    auto update = reinterpret_cast<UpdateConversion>(initialized_city_conversion::detoured_function);
    conversion_update_calls = 0; expect_conversion_capacity = false;
    current_mode = 0; update(city); assert(conversion_update_calls == 1);
    current_mode = kCivilizationMode; settings.ai_diagnostics = 0;
    expect_conversion_capacity = true; update(city); assert(conversion_update_calls == 2);
    update(city); assert(conversion_update_calls == 3);
    assert(conversion_capacity_repairs_logged == saved_logs);
    assert(!memcmp(bucket_bytes, expected, sizeof expected));
    assert(!memcmp(city_bytes, city_before.data(), sizeof city_bytes));
    game_mode = saved_mode; current_mode = saved_current_mode;
    original_update_conversion = saved_update; settings = saved_settings;
    puts("Offscreen capacity: stalled-save repair, unchanged progress, idempotence, phase and diagnostic independence passed.");
}

static void verify_native_capacity_constants(const unsigned char* image)
{
    auto value = [&](uintptr_t address) { float f; memcpy(&f, image + address - kReferenceBase, sizeof f); return f; };
    const float pi = value(0x1567354), spacing = value(0x13eed94), factor = value(0x145be38);
    auto count = [&](uintptr_t address) {
        const float radius = value(address);
        float area = radius * radius; area *= pi;
        float point_area = pi * spacing; point_area *= spacing;
        area /= point_area; area *= factor;
        return static_cast<int>(area); // Native cvttss2si truncates.
    };
    assert(count(0x1486054) == Conversion::kNativeListeningCapacity);
    assert(count(0x13eb950) == Conversion::kNativeConvertedCapacity);
    puts("Native crowd radii and spacing independently confirm 10 listening / 2 converted capacity.");
}
