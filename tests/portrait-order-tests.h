static PortraitOrderFile order_fixture_disk;
static bool order_fixture_available, order_fixture_writable;
static unsigned order_fixture_writes;
static uint32_t order_fixture_key[4] = {11, 22, 33, 44};
static bool mock_order_key(const CivilizationList&, uint32_t* key) {
    memcpy(key, order_fixture_key, sizeof order_fixture_key); return true;
}
static bool mock_order_read(const uint32_t* key, PortraitOrderFile& data) {
    if (!order_fixture_available || !valid_portrait_order_file(order_fixture_disk, key)) return false;
    data = order_fixture_disk; return true;
}
static bool mock_order_write(const uint32_t* key, const PortraitOrderFile& data) {
    assert(valid_portrait_order_file(data, key)); ++order_fixture_writes;
    if (!order_fixture_writable) return false;
    order_fixture_available = true; order_fixture_disk = data; return true;
}
static void verify_portrait_arrival_order()
{
    const auto old_key = portrait_order_key;
    const auto old_read = read_portrait_order;
    const auto old_write = write_portrait_order;
    const bool old_enabled = portrait_pagination_enabled, old_ready = portrait_reuse_ready;
    portrait_pagination_enabled = portrait_reuse_ready = true;
    portrait_order_key = mock_order_key; read_portrait_order = mock_order_read; write_portrait_order = mock_order_write;
    order_fixture_available = false; order_fixture_writable = true; order_fixture_writes = 0;
    reset_ai_observations(); settings = defaults;
    alignas(8) unsigned char civ_bytes[9][sizeof(Civilization)]{};
    alignas(8) unsigned char city_bytes[9][sizeof(City)]{};
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    Civilization* civs[9]; City* cities[9]; City* owned[9][1]{};
    CivilizationList live;
    for (unsigned i = 0; i < 9; ++i) {
        civs[i] = reinterpret_cast<Civilization*>(civ_bytes[i]); cities[i] = reinterpret_cast<City*>(city_bytes[i]);
        put_pointer(civs[i], 0, vtable); put_pointer(cities[i], 0, vtable);
        civs[i]->mID = 900 - i; cities[i]->mID = 1000 + i;
        civs[i]->mCurrentCommEventId = UINT32_MAX; owned[i][0] = cities[i];
        put_pointer(civs[i], kCivCitiesBegin, owned[i]); put_pointer(civs[i], kCivCitiesEnd, owned[i] + 1);
        live.push_back(civs[i]);
    }
    Native::RivalPortraitPanel panel{};
    update_portrait_roster(&panel, live); assert(!portrait_pages.count);
    const auto register_rival = reinterpret_cast<RegisterPortrait>(safe_register_portrait::detoured_function);
    for (unsigned i = 0; i < 9; ++i) {
        civs[i]->mCommNotifiedFirstCityCapture = true;
        register_rival(&panel, civs[i]);
        update_portrait_roster(&panel, live);
        assert(portrait_pages.count == i + 1);
        for (unsigned j = 0; j <= i; ++j)
            assert(portrait_pages.entries[j].civilization == portrait_identity(civs[j]));
    }
    // Descending object IDs must never move a later conqueror ahead of Anis.
    // Eliminating position 2 pulls up every following nation across page edges.
    civs[1]->mbIsDestroyed = true;
    update_portrait_roster(&panel, live);
    assert(portrait_pages.count == 8 && portrait_page_count() == 2);
    for (unsigned i = 1; i < 8; ++i)
        assert(portrait_pages.entries[i].civilization == portrait_identity(civs[i + 1]));
    portrait_pages.page = 1;
    assert(ai_lifecycle.Write(nullptr)); assert(order_fixture_writes == 1 && portrait_pages.page == 1);
    const auto saved = order_fixture_disk;
    // Full lifecycle reset, fresh panel address and reversed engine enumeration.
    ai_lifecycle.Read(nullptr);
    live.clear(); for (int i = 8; i >= 0; --i) live.push_back(civs[i]);
    Native::RivalPortraitPanel reloaded{};
    update_portrait_roster(&reloaded, live);
    assert(portrait_pages.page == 0 && portrait_pages.count == 8);
    assert(portrait_pages.entries[0].civilization == portrait_identity(civs[0]));
    for (unsigned i = 1; i < 8; ++i)
        assert(portrait_pages.entries[i].civilization == portrait_identity(civs[i + 1]));
    // Roll back to an older save: extinct rivals can be restored in their rank;
    // not-yet-qualified rivals must wait for their new actual admission order.
    civs[1]->mbIsDestroyed = false;
    civs[7]->mCommNotifiedFirstCityCapture = civs[8]->mCommNotifiedFirstCityCapture = false;
    ai_lifecycle.Read(nullptr); update_portrait_roster(&reloaded, live);
    assert(portrait_pages.count == 7 && portrait_order.count == 7);
    civs[8]->mCommNotifiedFirstCityCapture = true; note_portrait_qualification(civs[8]);
    civs[7]->mCommNotifiedFirstCityCapture = true; note_portrait_qualification(civs[7]);
    update_portrait_roster(&reloaded, live);
    assert(portrait_pages.entries[7].civilization == portrait_identity(civs[8]));
    assert(portrait_pages.entries[8].civilization == portrait_identity(civs[7]));
    // Failed UI cache writes cannot make the game save callback fail.
    order_fixture_writable = false; assert(ai_lifecycle.Write(nullptr));
    assert(memcmp(&saved, &order_fixture_disk, sizeof saved) == 0);
    // Another planet and missing/corrupt caches start deterministically, then
    // remember future additions; no foreign order is accepted.
    order_fixture_key[0]++; ai_lifecycle.Read(nullptr); update_portrait_roster(&reloaded, live);
    assert(portrait_pages.entries[0].civilization == portrait_identity(civs[8]));
    order_fixture_key[0]--;
    order_fixture_disk.checksum ^= 1; ai_lifecycle.Read(nullptr); update_portrait_roster(&reloaded, live);
    assert(portrait_pages.entries[0].civilization == portrait_identity(civs[8]));
    order_fixture_disk = saved;
    for (auto& id : order_fixture_disk.ids) id += 10000;
    order_fixture_disk.checksum = portrait_order_checksum(order_fixture_disk);
    ai_lifecycle.Read(nullptr); update_portrait_roster(&reloaded, live);
    assert(portrait_pages.count == 9 && portrait_order.count == 9);
    assert(portrait_pages.entries[0].civilization == portrait_identity(civs[8]));
    // Independent binary-format and real temporary-file round trip, not AppData.
    assert(valid_portrait_order_file(saved, saved.key));
    for (size_t i = 0; i < sizeof saved; ++i) {
        auto corrupt = saved; reinterpret_cast<unsigned char*>(&corrupt)[i] ^= 1;
        assert(!valid_portrait_order_file(corrupt, saved.key));
    }
    auto duplicate = saved; duplicate.ids[1] = duplicate.ids[0];
    duplicate.checksum = portrait_order_checksum(duplicate); assert(!valid_portrait_order_file(duplicate, saved.key));
    wchar_t temporary_root[MAX_PATH]{}, file[MAX_PATH]{};
    assert(GetTempPathW(MAX_PATH, temporary_root));
    assert(GetTempFileNameW(temporary_root, L"bco", 0, file));
    assert(write_portrait_order_file(file, saved));
    PortraitOrderFile round_trip{};
    assert(read_portrait_order_file(file, saved.key, round_trip) && !memcmp(&saved, &round_trip, sizeof saved));
    FILE* extra = nullptr; assert(!_wfopen_s(&extra, file, L"ab")); fputc(0, extra); fclose(extra);
    assert(!read_portrait_order_file(file, saved.key, round_trip));
    assert(DeleteFileW(file));
    live.clear(); assert(ai_refs == 0);
    reset_ai_observations(); portrait_order_key = old_key;
    read_portrait_order = old_read; write_portrait_order = old_write;
    portrait_pagination_enabled = old_enabled; portrait_reuse_ready = old_ready;
    puts("Portrait arrival order: descending IDs, compaction, same-frame admissions, UI cache reload, rollback, isolation and corruption checks passed.");
}

static unsigned specialty_rebinds;
static void __fastcall mock_specialty_restore(Native::RivalPortraitPanel* panel, void*, int index, Civilization* civ)
{
    assert(panel->slots[index].civilization == civ);
    ++specialty_rebinds;
}
static void verify_portrait_specialty_refresh()
{
    const auto old_restore = original_restore_portrait;
    const auto old_talking = portrait_comm_screen_active;
    original_restore_portrait = reinterpret_cast<RestorePortrait>(mock_specialty_restore);
    portrait_comm_screen_active = mock_call_conversation;
    call_conversation = false; specialty_rebinds = 0; reset_portrait_specialties();
    alignas(8) unsigned char civ_bytes[sizeof(Civilization)]{}, city_bytes[2][sizeof(City)]{};
    auto* civ = reinterpret_cast<Civilization*>(civ_bytes);
    auto* religious = reinterpret_cast<City*>(city_bytes[0]); auto* military = reinterpret_cast<City*>(city_bytes[1]);
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    put_pointer(civ, 0, vtable); civ->mID = 1710; civ->mCommNotifiedFirstCityCapture = true;
    religious->mID = 1709; religious->mVehicleSpecialty = kReligious;
    military->mID = 1824; military->mVehicleSpecialty = kMilitary;
    City* owned[] = {religious, military};
    put_pointer(civ, kCivCitiesBegin, owned); put_pointer(civ, kCivCitiesEnd, owned + 2);
    CivilizationList live; live.push_back(civ);
    Native::RivalPortraitPanel panel{};
    panel.slots[0].civilization = civ; panel.slots[0].occupied = true;
    refresh_portrait_specialties(&panel, live); assert(!specialty_rebinds);
    assert(portrait_specialties.slots[0].purpose == kReligious);
    // Lavande loses its first (religious) city, retaining the military capture.
    put_pointer(civ, kCivCitiesBegin, owned + 1);
    portrait_call_alerts.windows[0][0].civilization = portrait_identity(civ);
    refresh_portrait_specialties(&panel, live);
    assert(specialty_rebinds == 1 && !portrait_call_alerts.windows[0][0].civilization);
    assert(portrait_specialties.slots[0].purpose == kMilitary);
    for (unsigned i = 0; i < 500; ++i) refresh_portrait_specialties(&panel, live);
    assert(specialty_rebinds == 1);
    call_conversation = true; military->mVehicleSpecialty = kEconomic;
    refresh_portrait_specialties(&panel, live); assert(specialty_rebinds == 1);
    call_conversation = false;
    refresh_portrait_specialties(&panel, live); assert(specialty_rebinds == 2);
    assert(military->mVehicleSpecialty == kEconomic && religious->mVehicleSpecialty == kReligious);
    panel.slots[0].civilization = reinterpret_cast<Civilization*>(uintptr_t(1));
    refresh_portrait_specialties(&panel, live); assert(specialty_rebinds == 2);
    live.clear(); assert(ai_refs == 0); reset_portrait_specialties();
    original_restore_portrait = old_restore; portrait_comm_screen_active = old_talking;
    puts("Portrait specialty refresh: first-city loss, unchanged frames, open conversations and borrowed-pointer safety passed.");
}
