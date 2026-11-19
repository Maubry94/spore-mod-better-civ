// Behavior tests exercise the real pager with SDK-typed objects and native-map
// fixtures. Widget creation is substituted; no claim of in-game visual QA.
static bool pagination_talking, pagination_ui_available = true;
static bool fixture_prepare_pager(Native::RivalPortraitPanel*) { return pagination_ui_available; }
static void fixture_clear_pager() {}
static void fixture_refresh_pager(Native::RivalPortraitPanel*) {}
static bool fixture_talking() { return pagination_talking; }

static void assert_page_consistent(Native::RivalPortraitPanel& panel)
{
    unsigned shown = 0;
    for (int i = 0; i < 4; ++i) {
        auto* civ = panel.slots[i].civilization;
        const unsigned index = portrait_pages.page * 4 + unsigned(i);
        if (index >= portrait_pages.count) {
            assert(!civ && !panel.slots[i].occupied);
            continue;
        }
        assert(civ && panel.slots[i].occupied && civ->mCommRelPanelSlot == i);
        assert(portrait_identity(civ) == portrait_pages.entries[index].civilization);
        assert(portrait_fixture_map.at(civ) == i);
        for (int j = 0; j < i; ++j) assert(panel.slots[j].civilization != civ);
        ++shown;
    }
    assert(portrait_fixture_map.size() == shown);
    for (const auto& civ : portrait_fixture_civs)
        if (represented_portrait(&panel, civ.get()) < 0) assert(civ->mCommRelPanelSlot == -1);
}

static void verify_portrait_pagination()
{
    alignas(8) unsigned char civ_bytes[9][sizeof(Civilization)] = {};
    alignas(8) unsigned char city_bytes[9][sizeof(City)] = {};
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    Civilization* civs[9]; City* cities[9]; City* owned[9][2] = {};
    for (unsigned i = 0; i < 9; ++i) {
        civs[i] = reinterpret_cast<Civilization*>(civ_bytes[i]);
        cities[i] = reinterpret_cast<City*>(city_bytes[i]);
        put_pointer(civs[i], 0, vtable); put_pointer(cities[i], 0, vtable);
        civs[i]->mID = 300 + i; cities[i]->mID = 400 + i;
        civs[i]->mCommNotifiedFirstCityCapture = i != 8;
        civs[i]->mCommRelPanelSlot = -1; civs[i]->mCurrentCommEventId = UINT32_MAX;
        owned[i][0] = cities[i];
        put_pointer(civs[i], kCivCitiesBegin, owned[i]); put_pointer(civs[i], kCivCitiesEnd, owned[i] + 1);
        portrait_fixture_civs.push_back(civs[i]);
    }
    portrait_civilizations = mock_portrait_civilizations;
    native_hide_portrait = reinterpret_cast<HidePortrait>(fixture_hide_portrait);
    native_remove_portrait = reinterpret_cast<RemovePortrait>(fixture_remove_portrait);
    original_restore_portrait = reinterpret_cast<RestorePortrait>(fixture_restore_portrait);
    original_register_portrait = reinterpret_cast<RegisterPortrait>(fixture_register_portrait);
    original_update_portraits = reinterpret_cast<UpdatePortraits>(fixture_update_portraits);
    prepare_portrait_pager = fixture_prepare_pager;
    clear_portrait_pager = fixture_clear_pager;
    refresh_portrait_pager = fixture_refresh_pager;
    portrait_comm_screen_active = fixture_talking;
    portrait_pagination_enabled = portrait_reuse_ready = true;
    current_mode = kCivilizationMode; settings = defaults;
    reset_ai_observations();
    Native::RivalPortraitPanel panel{};
    for (int i = 0; i < 4; ++i) fixture_register_portrait(&panel, nullptr, civs[i]);
    const auto update = reinterpret_cast<UpdatePortraits>(reuse_portrait_slots::detoured_function);
    const auto register_rival = reinterpret_cast<RegisterPortrait>(safe_register_portrait::detoured_function);
    const auto restore = reinterpret_cast<RestorePortrait>(safe_restore_portrait::detoured_function);
    update(&panel, 16);
    assert(portrait_pages.count == 8 && portrait_page_count() == 2 && portrait_pages.page == 0);
    assert_page_consistent(panel);
    assert(!handle_pager_command(kPortraitPrevious) && !handle_pager_command(123));

    // First conquest while all physical slots are full: append, do not evict.
    const auto before_first_capture = panel;
    civs[8]->mCommNotifiedFirstCityCapture = true;
    register_rival(&panel, civs[8]);
    assert(memcmp(&panel, &before_first_capture, sizeof panel) == 0);
    update(&panel, 16);
    assert(portrait_pages.count == 9 && portrait_page_count() == 3 && portrait_pages.page == 0);
    assert_page_consistent(panel);

    assert(handle_pager_command(kPortraitNext));
    portrait_reenter = true; update(&panel, 16);
    assert(!portrait_reenter && !repairing_portraits && portrait_pages.page == 1);
    for (int i = 0; i < 4; ++i) assert(panel.slots[i].civilization == civs[i + 4]);
    assert_page_consistent(panel);
    assert(handle_pager_command(kPortraitNext)); update(&panel, 16);
    assert(portrait_pages.page == 2 && panel.slots[0].civilization == civs[8]);
    assert(!handle_pager_command(kPortraitNext));
    assert_page_consistent(panel);
    assert(handle_pager_command(kPortraitPrevious)); update(&panel, 16);
    assert(portrait_pages.page == 1); assert_page_consistent(panel);

    // Saving leaves the current page untouched. A reload rebuilds the SAME
    // complete order, independent of the saved page and engine enumeration.
    const auto before_save = panel;
    assert(ai_lifecycle.Write(nullptr));
    assert(memcmp(&panel, &before_save, sizeof panel) == 0 && portrait_pages.page == 1);
    int saved_slots[9];
    for (int i = 0; i < 9; ++i) saved_slots[i] = civs[i]->mCommRelPanelSlot;
    for (int i = 0; i < 4; ++i) fixture_hide_portrait(&panel, nullptr, i);
    panel = {}; portrait_fixture_map.clear();
    assert(ai_lifecycle.Read(nullptr));
    portrait_fixture_civs.clear();
    for (int i = 8; i >= 0; --i) {
        portrait_fixture_civs.push_back(civs[i]);
        restore(&panel, saved_slots[i], civs[i]);
    }
    update(&panel, 16);
    assert(portrait_pages.page == 0 && portrait_pages.count == 9);
    for (int i = 0; i < 4; ++i) assert(panel.slots[i].civilization == civs[i]);
    for (int i = 0; i < 9; ++i) assert(roster_index(portrait_identity(civs[i])) == i);
    assert_page_consistent(panel);

    // Two simultaneous hidden calls: show one, retain it while pending, then
    // show the other. No frame-by-frame page cycling or caller-event mutation.
    civs[8]->mCurrentCommEventId = 123;
    civs[4]->mCurrentCommEventId = 456;
    update(&panel, 16);
    const auto first_call_page = portrait_pages.page;
    assert(first_call_page == 1 && !portrait_pages.hidden_call_before && portrait_pages.hidden_call_after);
    update(&panel, 16); assert(portrait_pages.page == first_call_page);
    pagination_talking = true;
    assert(!handle_pager_command(kPortraitNext));
    const auto during_conversation = panel;
    update(&panel, 16); assert(memcmp(&panel, &during_conversation, sizeof panel) == 0);
    pagination_talking = false;
    civs[4]->mCurrentCommEventId = UINT32_MAX;
    update(&panel, 16); assert(portrait_pages.page == 2);
    assert(civs[8]->mCurrentCommEventId == 123);
    assert(handle_pager_command(kPortraitPrevious)); update(&panel, 16);
    assert(portrait_pages.page == 1 && !portrait_pages.hidden_call_before && portrait_pages.hidden_call_after);
    update(&panel, 16); assert(portrait_pages.page == 1); // No snap-back after manual navigation.
    civs[8]->mCurrentCommEventId = UINT32_MAX;
    update(&panel, 16); assert(!portrait_pages.hidden_call_before && !portrait_pages.hidden_call_after);

    // The middle page can have callers on both sides. An on-page call itself
    // needs no pager alert; manual navigation must recalculate the direction.
    civs[0]->mCurrentCommEventId = 100;
    civs[4]->mCurrentCommEventId = 101;
    civs[8]->mCurrentCommEventId = 102;
    update(&panel, 16);
    assert(portrait_pages.page == 1 && portrait_pages.hidden_call_before && portrait_pages.hidden_call_after);
    assert(handle_pager_command(kPortraitNext)); update(&panel, 16);
    assert(portrait_pages.page == 2 && portrait_pages.hidden_call_before && !portrait_pages.hidden_call_after);
    assert(handle_pager_command(kPortraitPrevious)); update(&panel, 16);
    assert(portrait_pages.page == 1 && portrait_pages.hidden_call_before && portrait_pages.hidden_call_after);
    assert(handle_pager_command(kPortraitPrevious)); update(&panel, 16);
    assert(portrait_pages.page == 0 && !portrait_pages.hidden_call_before && portrait_pages.hidden_call_after);
    assert(civs[0]->mCurrentCommEventId == 100 && civs[4]->mCurrentCommEventId == 101 && civs[8]->mCurrentCommEventId == 102);
    civs[4]->mCurrentCommEventId = civs[8]->mCurrentCommEventId = UINT32_MAX;
    update(&panel, 16);
    assert(!portrait_pages.hidden_call_before && !portrait_pages.hidden_call_after);
    assert(handle_pager_command(kPortraitNext)); update(&panel, 16);
    assert(portrait_pages.hidden_call_before && !portrait_pages.hidden_call_after);
    // An eliminated caller must not leave a stale directional marker.
    put_pointer(civs[0], kCivCitiesEnd, owned[0]); update(&panel, 16);
    assert(!portrait_pages.hidden_call_before && !portrait_pages.hidden_call_after);
    civs[0]->mCurrentCommEventId = UINT32_MAX;
    put_pointer(civs[0], kCivCitiesEnd, owned[0] + 1); update(&panel, 16);
    assert_page_consistent(panel);

    // All roster sizes, last-page shrinkage, and exhaustion. Survivors never
    // leak references or leave a stale hidden map entry behind.
    for (int remaining = 8; remaining >= 0; --remaining) {
        put_pointer(civs[remaining], kCivCitiesEnd, owned[remaining]);
        update(&panel, 16);
        assert(portrait_pages.count == unsigned(remaining));
        assert(portrait_page_count() == unsigned((remaining + 3) / 4));
        assert_page_consistent(panel);
    }
    // With no navigation available, fall back to working four-slot allocation.
    for (int i = 0; i < 5; ++i) put_pointer(civs[i], kCivCitiesEnd, owned[i] + 1);
    pagination_ui_available = false;
    update(&panel, 16);
    for (int i = 0; i < 4; ++i) assert(panel.slots[i].civilization);
    assert(civs[4]->mCommRelPanelSlot == -1);
    pagination_ui_available = true;
    update(&panel, 16); assert(portrait_pages.count == 5); assert_page_consistent(panel);
    assert(handle_pager_command(kPortraitNext)); update(&panel, 16); assert_page_consistent(panel);

    // Interior elimination: pull the FIRST nation of page 2 into page 1;
    // every later survivor moves forward, including new qualifiers by rank.
    for (int i = 0; i < 4; ++i) fixture_hide_portrait(&panel, nullptr, i);
    panel = {}; portrait_fixture_map.clear(); reset_ai_observations();
    for (int i = 0; i < 9; ++i) {
        civs[i]->mCommRelPanelSlot = -1;
        civs[i]->mCommNotifiedFirstCityCapture = i < 5;
        put_pointer(civs[i], kCivCitiesEnd, owned[i] + 1);
    }
    update(&panel, 16); assert(portrait_pages.count == 5); assert_page_consistent(panel);
    owned[8][1] = cities[3]; put_pointer(civs[8], kCivCitiesEnd, owned[8] + 2);
    put_pointer(civs[3], kCivCitiesEnd, owned[3]);
    civs[8]->mCommNotifiedFirstCityCapture = true;
    civs[5]->mCommNotifiedFirstCityCapture = true;
    update(&panel, 16);
    assert(portrait_pages.count == 6 && panel.slots[3].civilization == civs[4]);
    assert(roster_index(portrait_identity(civs[5])) == 4);
    assert(roster_index(portrait_identity(civs[8])) == 5); assert_page_consistent(panel);

    // Duplicate native slots and stale displaced map entries are repaired
    // before another page is bound, without releasing the retained SDK object.
    fixture_restore_portrait(&panel, nullptr, 3, civs[0]);
    update(&panel, 16);
    assert(panel.slots[0].civilization == civs[0] && panel.slots[3].civilization == civs[4]);
    assert_page_consistent(panel);
    civs[4]->mIsPlayerOwned = true;
    civs[5]->mIsNeutral = true;
    update(&panel, 16); assert(portrait_pages.count == 4); assert_page_consistent(panel);
    assert(!handle_pager_command(kPortraitNext));

    // An earlier-ID qualifier rejoins in its fixed rank. Removing an interior
    // first-page nation shifts the following three entries, not just the tail.
    civs[4]->mIsPlayerOwned = false; civs[5]->mIsNeutral = false;
    update(&panel, 16);
    assert(handle_pager_command(kPortraitNext)); update(&panel, 16);
    assert(panel.slots[0].civilization == civs[5] && panel.slots[1].civilization == civs[8]);
    put_pointer(civs[1], kCivCitiesEnd, owned[1]);
    update(&panel, 16);
    assert(panel.slots[0].civilization == civs[8] && !panel.slots[1].civilization);
    assert(handle_pager_command(kPortraitPrevious)); update(&panel, 16);
    assert(panel.slots[0].civilization == civs[0] && panel.slots[1].civilization == civs[2]);
    assert(panel.slots[2].civilization == civs[4] && panel.slots[3].civilization == civs[5]);
    put_pointer(civs[2], kCivCitiesEnd, owned[2]); update(&panel, 16);
    assert(portrait_pages.count == 4 && portrait_page_count() == 1);
    assert(panel.slots[1].civilization == civs[4] && panel.slots[2].civilization == civs[5]);
    assert(panel.slots[3].civilization == civs[8]); assert_page_consistent(panel);

    const auto outside_mode = panel;
    current_mode = 0; update(&panel, 16);
    assert(memcmp(&panel, &outside_mode, sizeof panel) == 0 && !handle_pager_command(kPortraitPrevious));
    current_mode = kCivilizationMode;
    ai_lifecycle.Dispose(); assert(portrait_pages.panel_identity == 0 && !portrait_reuse_ready);
    assert(!portrait_pages.hidden_call_before && !portrait_pages.hidden_call_after);
    for (int i = 0; i < 4; ++i) fixture_hide_portrait(&panel, nullptr, i);
    portrait_fixture_map.clear(); portrait_fixture_civs.clear();
    assert(ai_refs == 0);
    portrait_pagination_enabled = false;
    portrait_civilizations = sdk_portrait_civilizations;
    prepare_portrait_pager = ensure_portrait_pager;
    clear_portrait_pager = dispose_portrait_pager;
    refresh_portrait_pager = draw_portrait_pager;
    portrait_comm_screen_active = sdk_comm_screen_active;
    puts("Diplomacy pages, navigation, hidden calls, save/restore, fallback and ownership checks passed.");
}
