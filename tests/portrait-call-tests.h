// Reused native portrait buttons can keep a material from their former nation.
// Model the button/material state, not just the repair's internal counters.
struct CallWindowFixture {
    uintptr_t* vtable = nullptr;
    unsigned view = 0, slot = 0;
    bool available = true, blinking = false;
    unsigned applications = 0;
};
static CallWindowFixture call_roots[2], call_windows[2][4];
static bool call_conversation;
static unsigned call_applications;
static bool mock_call_conversation() { return call_conversation; }
static UTFWin::IWindow* __fastcall mock_call_find_panel(void*, void*, uint32_t id)
{
    assert(id == 0x046977ff || id == 0x055d626f);
    auto& root = call_roots[id == 0x055d626f ? 1 : 0];
    return root.available ? reinterpret_cast<UTFWin::IWindow*>(&root) : nullptr;
}
static UTFWin::IWindow* __fastcall mock_call_find_window(CallWindowFixture* root, void*, uint32_t id, bool recursive)
{
    assert(!recursive && id >= 1000 && id < 1004);
    auto& window = call_windows[root->view][id - 1000];
    return window.available ? reinterpret_cast<UTFWin::IWindow*>(&window) : nullptr;
}
static void __cdecl mock_call_material(UTFWin::IWindow* window, const char16_t* script, int flags)
{
    assert(flags == 0 && window && script);
    auto& target = *reinterpret_cast<CallWindowFixture*>(window);
    const std::u16string text(script);
    assert(text == kPortraitCallMaterial || text == kPortraitIdleMaterial);
    target.blinking = text == kPortraitCallMaterial;
    ++target.applications;
    ++call_applications;
}

static void verify_portrait_call_alerts()
{
    const auto old_find = native_find_portrait_window;
    const auto old_material = native_apply_portrait_material;
    const auto old_comm = portrait_comm_screen_active;
    const auto old_pagination = portrait_pagination_enabled;
    native_find_portrait_window = reinterpret_cast<FindPortraitWindow>(mock_call_find_panel);
    native_apply_portrait_material = mock_call_material;
    portrait_comm_screen_active = mock_call_conversation;
    portrait_pagination_enabled = false;
    settings = defaults; settings.ai_diagnostics = 1;
    current_mode = kCivilizationMode; portrait_reuse_ready = true;
    call_conversation = false; call_applications = 0;
    reset_ai_observations();

    uintptr_t window_table[0xf4 / 4];
    for (auto& slot : window_table) slot = reinterpret_cast<uintptr_t>(unexpected_widget_call);
    window_table[0xf0 / 4] = reinterpret_cast<uintptr_t>(mock_call_find_window);
    for (unsigned view = 0; view < 2; ++view) {
        call_roots[view] = {window_table, view};
        for (unsigned slot = 0; slot < 4; ++slot)
            call_windows[view][slot] = {window_table, view, slot};
    }

    alignas(8) unsigned char civ_bytes[3][sizeof(Civilization)]{};
    alignas(8) unsigned char city_bytes[3][sizeof(City)]{};
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    Civilization* civs[3]; City* cities[3]; City* owned[3][2]{};
    CivilizationList live;
    for (unsigned i = 0; i < 3; ++i) {
        civs[i] = reinterpret_cast<Civilization*>(civ_bytes[i]);
        cities[i] = reinterpret_cast<City*>(city_bytes[i]);
        put_pointer(civs[i], 0, vtable); put_pointer(cities[i], 0, vtable);
        civs[i]->mID = 100 + i; cities[i]->mID = 200 + i;
        civs[i]->mCommNotifiedFirstCityCapture = true;
        civs[i]->mCurrentCommEventId = UINT32_MAX;
        owned[i][0] = cities[i];
        put_pointer(civs[i], kCivCitiesBegin, owned[i]);
        put_pointer(civs[i], kCivCitiesEnd, owned[i] + 1);
        live.push_back(civs[i]);
    }
    Native::RivalPortraitPanel panel{};
    for (unsigned i = 0; i < 4; ++i) panel.slots[i].window_id = 1000 + i;
    panel.slots[0].civilization = civs[0]; panel.slots[0].occupied = true;
    auto tick = [&]() {
        // No game data or native slot ownership is modified by this UI repair.
        const auto before = panel;
        unsigned char before_civs[sizeof civ_bytes]; memcpy(before_civs, civ_bytes, sizeof civ_bytes);
        repair_portrait_call_alerts(&panel, live);
        assert(memcmp(&before, &panel, sizeof panel) == 0);
        assert(memcmp(before_civs, civ_bytes, sizeof civ_bytes) == 0);
    };
    auto assert_blink = [&](unsigned slot, bool expected) {
        assert(call_windows[0][slot].blinking == expected && call_windows[1][slot].blinking == expected);
    };
    tick(); assert_blink(0, false); assert(call_applications == 2);
    // Citron leaves a non-blinking material; Orange uses exactly the same button.
    panel.slots[0].civilization = civs[1]; civs[1]->mCurrentCommEventId = 1;
    tick(); assert_blink(0, true); assert(call_applications == 4);
    for (unsigned i = 0; i < 1000; ++i) tick();
    assert(call_applications == 4); // No frame-by-frame restart or parser churn.
    // A different caller with the same event number still needs its own signal.
    panel.slots[0].civilization = civs[2]; civs[2]->mCurrentCommEventId = 1;
    tick(); assert_blink(0, true); assert(call_applications == 6);
    // Acknowledgement clears the effect; it never recreates the pending event.
    civs[2]->mCurrentCommEventId = UINT32_MAX;
    tick(); assert_blink(0, false); assert(call_applications == 8);
    tick(); assert(call_applications == 8);
    civs[2]->mCurrentCommEventId = 1;
    tick(); assert_blink(0, true); assert(call_applications == 10);
    // A replacement with no call must not inherit the old nation's flash.
    panel.slots[0].civilization = civs[0];
    tick(); assert_blink(0, false); assert(call_applications == 12);

    // Conversation veto: native acknowledgement may clear the material/event
    // while it is open. Do not interrupt it or restart an acknowledged call.
    call_conversation = true;
    panel.slots[0].civilization = civs[1];
    tick(); assert(call_applications == 12);
    civs[1]->mCurrentCommEventId = UINT32_MAX;
    call_conversation = false; tick(); assert_blink(0, false);
    const auto acknowledged = call_applications;
    tick(); assert(call_applications == acknowledged);
    // Still-pending calls are rearmed after a conversation closes.
    civs[1]->mCurrentCommEventId = 1;
    call_conversation = true; tick(); assert(call_applications == acknowledged);
    call_conversation = false; tick(); assert_blink(0, true);

    // Switching pages reuses the same widgets. Each is resolved anew.
    panel.slots[0].civilization = civs[0]; tick(); assert_blink(0, false);
    panel.slots[0].civilization = civs[1]; tick(); assert_blink(0, true);
    // Missing compact widgets are retried only when available, without
    // disturbing the expanded portrait or dereferencing a missing control.
    call_roots[1].available = false;
    tick();
    const auto expanded = call_windows[0][0].applications;
    call_windows[1][0].blinking = false; call_roots[1].available = true;
    tick(); assert_blink(0, true);
    assert(call_windows[0][0].applications == expanded);
    call_windows[0][0].available = false; tick();
    call_windows[0][0].blinking = false; call_windows[0][0].available = true;
    tick(); assert_blink(0, true);

    // Removed/foreign pointers are never dereferenced; invalid slots and
    // ineligible nations do not start any visual effect.
    const auto valid = panel.slots[0];
    const auto calls_before_invalid = call_applications;
    panel.slots[0].civilization = reinterpret_cast<Civilization*>(uintptr_t(1));
    tick(); assert(call_applications == calls_before_invalid);
    panel.slots[0] = valid; panel.slots[0].occupied = false; tick();
    assert(call_applications == calls_before_invalid);
    panel.slots[0] = valid; civs[1]->mbIsDestroyed = true; tick();
    assert(call_applications == calls_before_invalid);
    civs[1]->mbIsDestroyed = false;
    settings.ai_diagnostics = 0;
    tick(); assert_blink(0, true); // Feature independent of diagnostics toggle.

    // New UI/load lifecycle re-resolves widgets and re-arms only pending calls.
    assert(ai_lifecycle.Read(nullptr));
    assert(portrait_call_alerts.panel_identity == 0);
    call_windows[0][0].blinking = call_windows[1][0].blinking = false;
    tick(); assert_blink(0, true);
    assert(portrait_call_alerts.log_lines == 0);
    civs[1]->mCurrentCommEventId = UINT32_MAX;
    assert(ai_lifecycle.Read(nullptr)); tick(); assert_blink(0, false);

    // Integration: the existing four-slot callback fills a vacancy, then repairs
    // its alert in that same frame, even when pagination is disabled.
    panel.slots[0].civilization = nullptr; panel.slots[0].occupied = false;
    portrait_fixture_civs.push_back(civs[1]);
    civs[1]->mCurrentCommEventId = 1;
    portrait_civilizations = mock_portrait_civilizations;
    native_hide_portrait = reinterpret_cast<HidePortrait>(fixture_hide_portrait);
    native_remove_portrait = reinterpret_cast<RemovePortrait>(fixture_remove_portrait);
    original_register_portrait = reinterpret_cast<RegisterPortrait>(fixture_register_portrait);
    original_restore_portrait = reinterpret_cast<RestorePortrait>(fixture_restore_portrait);
    original_update_portraits = reinterpret_cast<UpdatePortraits>(fixture_update_portraits);
    reset_ai_observations();
    const auto update = reinterpret_cast<UpdatePortraits>(reuse_portrait_slots::detoured_function);
    update(&panel, 16);
    assert(panel.slots[0].civilization == civs[1]); assert_blink(0, true);
    const auto after_update = call_applications;
    current_mode = 0; update(&panel, 16); assert(call_applications == after_update);
    current_mode = kCivilizationMode;
    ai_lifecycle.Dispose(); update(&panel, 16); assert(call_applications == after_update);
    assert(portrait_call_alerts.panel_identity == 0);
    fixture_remove_portrait(&panel, nullptr, civs[1]);
    portrait_fixture_civs.clear(); portrait_fixture_map.clear(); live.clear();
    assert(ai_refs == 0);
    portrait_civilizations = sdk_portrait_civilizations;
    native_find_portrait_window = old_find;
    native_apply_portrait_material = old_material;
    portrait_comm_screen_active = old_comm;
    portrait_pagination_enabled = old_pagination;
    settings = defaults;
    puts("Portrait call alerts: reused widgets, both panel sizes, acknowledgement, conversations, page changes, missing widgets and lifecycle passed.");
}
