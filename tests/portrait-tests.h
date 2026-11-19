#include <map>

static CivilizationList portrait_fixture_civs;
static std::map<Civilization*, int> portrait_fixture_map;
static unsigned portrait_updates, portrait_assignments;
static bool portrait_reenter;
static bool fixture_no_conversation() { return false; }
static const CivilizationList& mock_portrait_civilizations() { return portrait_fixture_civs; }
static void __fastcall fixture_hide_portrait(Native::RivalPortraitPanel* panel, void*, int slot)
{
    assert(slot >= 0 && slot < 4);
    auto* civ = panel->slots[slot].civilization;
    panel->slots[slot].civilization = nullptr;
    if (civ) civ->Release();
    // Deliberately preserve occupied: this is the native bug under test.
}
static void __fastcall fixture_remove_portrait(Native::RivalPortraitPanel* panel, void*, Civilization* civ)
{
    auto found = portrait_fixture_map.find(civ);
    if (found != portrait_fixture_map.end()) {
        assert(panel->slots[found->second].civilization == civ);
        fixture_hide_portrait(panel, nullptr, found->second);
        portrait_fixture_map.erase(found);
    }
}
static void __fastcall fixture_restore_portrait(Native::RivalPortraitPanel* panel, void*, int slot, Civilization* civ)
{
    assert(slot >= 0 && slot < 4 && civ);
    auto* old = panel->slots[slot].civilization;
    if (old != civ) {
        civ->AddRef(); panel->slots[slot].civilization = civ;
        if (old) old->Release();
    }
    panel->slots[slot].occupied = true;
    portrait_fixture_map[civ] = slot;
}
static void __fastcall fixture_register_portrait(Native::RivalPortraitPanel* panel, void*, Civilization* civ)
{
    if (portrait_fixture_map.count(civ)) return;
    int slot = 0;
    while (slot < 4 && panel->slots[slot].occupied) ++slot;
    assert(slot < 4); // A full panel must never reach the unsafe allocator.
    fixture_restore_portrait(panel, nullptr, slot, civ);
    civ->mCommRelPanelSlot = slot;
    ++portrait_assignments;
    if (portrait_reenter) {
        portrait_reenter = false;
        reinterpret_cast<UpdatePortraits>(reuse_portrait_slots::detoured_function)(panel, 0);
    }
}
static void __fastcall fixture_update_portraits(Native::RivalPortraitPanel*, void*, int) { ++portrait_updates; }

static void verify_portrait_reuse()
{
    const auto previous_comm_check = portrait_comm_screen_active;
    portrait_comm_screen_active = fixture_no_conversation;
    alignas(8) unsigned char civ_bytes[9][sizeof(Civilization)] = {};
    alignas(8) unsigned char city_bytes[9][sizeof(City)] = {};
    uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(ai_mock_addref), reinterpret_cast<uintptr_t>(ai_mock_release)};
    Civilization* civs[9]; City* cities[9]; City* owned[9][3] = {};
    for (unsigned i = 0; i < 9; ++i) {
        civs[i] = reinterpret_cast<Civilization*>(civ_bytes[i]);
        cities[i] = reinterpret_cast<City*>(city_bytes[i]);
        put_pointer(civs[i], 0, vtable); put_pointer(cities[i], 0, vtable);
        civs[i]->mID = 100 + i; cities[i]->mID = 200 + i;
        civs[i]->mCommNotifiedFirstCityCapture = true; civs[i]->mCommRelPanelSlot = -1;
        owned[i][0] = cities[i];
        put_pointer(civs[i], kCivCitiesBegin, owned[i]); put_pointer(civs[i], kCivCitiesEnd, owned[i] + 1);
        portrait_fixture_civs.push_back(civs[i]);
    }
    civs[6]->mCommNotifiedFirstCityCapture = false;
    civs[7]->mIsNeutral = true;
    civs[8]->mIsPlayerOwned = true;
    portrait_civilizations = mock_portrait_civilizations;
    native_hide_portrait = reinterpret_cast<HidePortrait>(fixture_hide_portrait);
    native_remove_portrait = reinterpret_cast<RemovePortrait>(fixture_remove_portrait);
    original_restore_portrait = reinterpret_cast<RestorePortrait>(fixture_restore_portrait);
    original_register_portrait = reinterpret_cast<RegisterPortrait>(fixture_register_portrait);
    original_update_portraits = reinterpret_cast<UpdatePortraits>(fixture_update_portraits);
    const auto update = reinterpret_cast<UpdatePortraits>(reuse_portrait_slots::detoured_function);
    const auto restore = reinterpret_cast<RestorePortrait>(safe_restore_portrait::detoured_function);
    Native::RivalPortraitPanel panel{};
    settings = defaults; current_mode = kCivilizationMode; portrait_reuse_ready = true;
    reset_ai_observations();
    for (int i = 0; i < 4; ++i) fixture_register_portrait(&panel, nullptr, civs[i]);
    update(&panel, 16); assert(portrait_assignments == 4);
    for (unsigned i = 4; i < 9; ++i) assert(civs[i]->mCommRelPanelSlot == -1);

    // A higher-ID conqueror must beat a lower-ID waiting rival. Native removal
    // frees the reference/map entry but leaves a visibly empty reserved slot.
    owned[5][1] = cities[2]; put_pointer(civs[5], kCivCitiesEnd, owned[5] + 2);
    put_pointer(civs[2], kCivCitiesEnd, owned[2]);
    fixture_remove_portrait(&panel, nullptr, civs[2]);
    assert(panel.slots[2].occupied && !panel.slots[2].civilization);
    portrait_reenter = true;
    update(&panel, 16);
    assert(!portrait_reenter && !repairing_portraits);
    assert(panel.slots[2].civilization == civs[5] && civs[5]->mCommRelPanelSlot == 2);
    assert(civs[4]->mCommRelPanelSlot == -1 && !portrait_fixture_map.count(civs[2]));
    assert(portrait_fixture_map.at(civs[5]) == 2 && portrait_assignments == 5);
    update(&panel, 16); assert(portrait_assignments == 5);

    // The player conquering a displayed nation is not eligible for its slot.
    owned[8][1] = cities[1]; put_pointer(civs[8], kCivCitiesEnd, owned[8] + 2);
    put_pointer(civs[1], kCivCitiesEnd, owned[1]);
    update(&panel, 16); // Also exercises removal of an eliminated retained portrait.
    assert(panel.slots[1].civilization == civs[4] && civs[1]->mCommRelPanelSlot == -1);
    assert(!portrait_fixture_map.count(civs[1]) && civs[8]->mCommRelPanelSlot == -1);

    // Repeated restore requests, including bad indices, must not create duplicates.
    for (int slot : {-1, 0, 3, 4, INT32_MIN, INT32_MAX}) restore(&panel, slot, civs[5]);
    assert(panel.slots[2].civilization == civs[5] && portrait_fixture_map.at(civs[5]) == 2);
    assert(panel.slots[0].civilization == civs[0] && panel.slots[3].civilization == civs[3]);

    // Historical vacancy on load: no invented conqueror, fill with a waiting
    // eligible nation. A destroyed candidate and neutral/player nations stay out.
    put_pointer(civs[0], kCivCitiesEnd, owned[0]);
    fixture_remove_portrait(&panel, nullptr, civs[0]);
    civs[6]->mCommNotifiedFirstCityCapture = true; civs[6]->mbIsDestroyed = true;
    assert(ai_lifecycle.Read(nullptr));
    assert(portrait_history.panel_identity == 0);
    update(&panel, 16); assert(!panel.slots[0].occupied && !panel.slots[0].civilization);
    civs[6]->mbIsDestroyed = false;
    update(&panel, 16); assert(panel.slots[0].civilization == civs[6]);

    // Repair a duplicate from historical state while retaining the earlier slot
    // and rebuilding its lookup. The displaced nation remains eligible to return.
    fixture_restore_portrait(&panel, nullptr, 3, civs[4]);
    update(&panel, 16);
    assert(panel.slots[1].civilization == civs[4] && portrait_fixture_map.at(civs[4]) == 1);
    // The overwritten portrait still had a stale lookup. It must actually be
    // restored, not merely skipped because registration found that old key.
    assert(panel.slots[3].civilization == civs[3] && portrait_fixture_map.at(civs[3]) == 3);

    // Two vacancies in one frame, a conqueror already represented, and only
    // one waiting eligible rival: never duplicate the conqueror or the player.
    owned[5][2] = cities[6]; put_pointer(civs[5], kCivCitiesEnd, owned[5] + 3);
    owned[8][2] = cities[3]; put_pointer(civs[8], kCivCitiesEnd, owned[8] + 3);
    put_pointer(civs[6], kCivCitiesEnd, owned[6]);
    put_pointer(civs[3], kCivCitiesEnd, owned[3]);
    civs[7]->mIsNeutral = false;
    update(&panel, 16);
    assert(panel.slots[0].civilization == civs[7] && panel.slots[2].civilization == civs[5]);
    assert(!panel.slots[3].occupied && !panel.slots[3].civilization);
    assert(portrait_fixture_map.count(civs[5]) == 1 && !portrait_fixture_map.count(civs[8]));

    const auto saved = panel;
    current_mode = 0; update(&panel, 16);
    assert(memcmp(&saved, &panel, sizeof panel) == 0);
    current_mode = kCivilizationMode;
    ai_lifecycle.Dispose(); assert(!portrait_reuse_ready && portrait_history.panel_identity == 0);
    update(&panel, 16); assert(memcmp(&saved, &panel, sizeof panel) == 0);
    for (int i = 0; i < 4; ++i) fixture_hide_portrait(&panel, nullptr, i);
    portrait_fixture_map.clear(); portrait_fixture_civs.clear();
    assert(ai_refs == 0 && portrait_updates > 0);
    portrait_civilizations = sdk_portrait_civilizations;
    portrait_comm_screen_active = previous_comm_check;
    settings = defaults;
}
