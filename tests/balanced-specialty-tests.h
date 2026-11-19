// Exercise every ticket path, including uneven remaining counts. Each completed
// draw must give three of each purpose; position is not a specialty quota.
static unsigned long balanced_paths;
static void visit_balanced_paths(BalancedSpecialties bag, unsigned military,
    unsigned religious, unsigned economic)
{
    const unsigned remaining = bag.Remaining();
    if (!remaining) {
        assert(military == 3 && religious == 3 && economic == 3);
        assert(bag.Take(0) == -1);
        ++balanced_paths;
        return;
    }
    assert(bag.Take(remaining) == -1 && bag.Remaining() == remaining);
    assert(bag.Take(UINT32_MAX) == -1 && bag.Remaining() == remaining);
    for (unsigned ticket = 0; ticket < remaining; ++ticket) {
        auto next = bag;
        const int purpose = next.Take(ticket);
        assert(next.Remaining() == remaining - 1);
        assert(purpose >= kMilitary && purpose <= kEconomic);
        visit_balanced_paths(next, military + (purpose == kMilitary),
            religious + (purpose == kReligious), economic + (purpose == kEconomic));
    }
}

static uint32_t balanced_seed;
static unsigned balanced_rng_calls;
static unsigned __fastcall balanced_random(void* state, int, unsigned bound)
{
    assert(state == &balanced_seed && bound >= 2 && bound <= 9);
    balanced_seed = balanced_seed * 1664525u + 1013904223u;
    ++balanced_rng_calls;
    return (balanced_seed >> 16) % bound;
}
static City* __cdecl failed_founder(Native::CityFoundation* site)
{
    assert(site && !site->minorNation);
    observed_site_specialty = site->specialty;
    return nullptr;
}

static void verify_balanced_foundation(FoundationDecoder decode)
{
    visit_balanced_paths({}, 0, 0, 0);
    assert(balanced_paths == 362880); // 9! possible ticket sequences.
    const auto saved_random = random_below;
    const auto saved_state = simulation_random;
    const auto saved_founder = original_found_city;
    const auto saved_mode = current_mode;
    const auto saved_settings = settings;
    const auto saved_bag = starting_specialties;
    random_below = reinterpret_cast<RandomBelow>(balanced_random);
    simulation_random = &balanced_seed;
    current_mode = kCivilizationMode;
    settings = defaults;
    apply_option(&settings, "<AISpecialties>Balanced333");
    assert(settings.balanced_specialties && settings.specialty_mask == kAllSpecialties);

    Native::CityFoundation site{};
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        balanced_seed = seed;
        balanced_rng_calls = 0;
        ai_lifecycle.Initialize();
        unsigned purposes[3] = {};
        for (int i = 0; i < 9; ++i) {
            site.minorNation = true; site.specialty = -1;
            assert(found_major_city(&site));
            // Use Spore's converter, which caught the previous religious /
            // economic inversion. Check the descriptor seen by the founder.
            const int purpose = decode(observed_site_specialty);
            assert(purpose >= 0 && purpose < 3);
            assert(++purposes[purpose] <= 3 && !site.minorNation);
        }
        assert(purposes[0] == 3 && purposes[1] == 3 && purposes[2] == 3);
        assert(balanced_rng_calls == 8 && starting_specialties.Remaining() == 0);
        site.specialty = 1;
        assert(found_major_city(&site));
        assert(observed_site_specialty == 1 && balanced_rng_calls == 8);
    }

    ai_lifecycle.Initialize();
    current_mode = 0;
    site.specialty = 1; site.minorNation = true;
    assert(found_major_city(&site) && site.specialty == 1 && site.minorNation);
    current_mode = kCivilizationMode;
    assert(found_major_city(nullptr) == nullptr && starting_specialties.Remaining() == 9);
    original_found_city = failed_founder;
    assert(!found_major_city(&site) && site.specialty == 1);
    assert(starting_specialties.Remaining() == 9);
    original_found_city = saved_founder;
    assert(found_major_city(&site) && starting_specialties.Remaining() == 8);

    // No stream writes, stale bag on reload, or changes to a saved descriptor.
    const auto stored_site = site;
    assert(ai_lifecycle.Write(nullptr) && starting_specialties.Remaining() == 8);
    assert(ai_lifecycle.Read(nullptr) && starting_specialties.Remaining() == 9);
    assert(memcmp(&stored_site, &site, sizeof site) == 0);
    alignas(8) unsigned char header_bytes[sizeof(Simulator::cSavedGameHeader)] = {};
    assert(found_major_city(&site) && starting_specialties.Remaining() == 8);
    ai_lifecycle.OnLoad(*reinterpret_cast<const Simulator::cSavedGameHeader*>(header_bytes));
    assert(starting_specialties.Remaining() == 9);
    assert(found_major_city(&site));
    ai_lifecycle.OnModeExited(kCivilizationMode, 0);
    assert(starting_specialties.Remaining() == 9);
    assert(found_major_city(&site));
    ai_lifecycle.OnModeEntered(0, kCivilizationMode);
    assert(starting_specialties.Remaining() == 9);

    // A choice switch must not retain the balanced flag or its full mask.
    for (const char* choice : {"EqualOdds", "NoMilitary", "NoReligious", "NoEconomic",
            "MilitaryOnly", "ReligiousOnly", "EconomicOnly"}) {
        apply_option(&settings, "<AISpecialties>Balanced333");
        char line[80]; snprintf(line, sizeof line, "<AISpecialties>%s", choice);
        apply_option(&settings, line);
        assert(!settings.balanced_specialties);
    }
    apply_option(&settings, "<AISpecialties>Balanced333");
    apply_option(&settings, "<AISpecialties>Balanced333Trailing");
    apply_option(&settings, "<AIDiagnostics>DiagnosticsOff");
    assert(settings.balanced_specialties && settings.specialty_mask == kAllSpecialties);
    apply_option(&settings, "<Specialites>TroisSpecialites");
    assert(!settings.balanced_specialties && settings.specialty_mask == kAllSpecialties);

    random_below = saved_random; simulation_random = saved_state;
    original_found_city = saved_founder; current_mode = saved_mode;
    settings = saved_settings; starting_specialties = saved_bag;
    puts("Balanced starts: 362880 draw paths, native specialty mapping, failure rollback, exhaustion and lifecycle resets passed.");
}
