#pragma once
// Included inside BetterCiv. The engine still owns exactly four portrait slots.
using FindPortraitWindow = UTFWin::IWindow* (__thiscall*)(Native::RivalPortraitPanel*, uint32_t);
static FindPortraitWindow native_find_portrait_window;

struct PortraitPages {
    uintptr_t panel_identity = 0;
    struct Entry {
        uint64_t civilization = 0, last_city = 0;
        uint32_t noticed_event = UINT32_MAX;
    } entries[9];
    unsigned page = 0, count = 0;
    int requested_step = 0;
    bool hidden_call_before = false, hidden_call_after = false;
    uint64_t displayed[4] = {};
    unsigned reported_count = UINT32_MAX, reported_page = UINT32_MAX;
};
static PortraitPages portrait_pages;
static bool pager_owns_panel(const Native::RivalPortraitPanel* panel)
{
    return portrait_reuse_ready && portrait_pages.panel_identity == reinterpret_cast<uintptr_t>(panel);
}
static Civilization* resolve_portrait(const CivilizationList& civs, uint64_t identity)
{
    if (!identity) return nullptr;
    for (const auto& civ : civs)
        if (portrait_identity(civ.get()) == identity && eligible_portrait(civ.get())) return civ.get();
    return nullptr;
}
static int roster_index(uint64_t identity)
{
    for (unsigned i = 0; i < 9; ++i)
        if (identity && portrait_pages.entries[i].civilization == identity) return int(i);
    return -1;
}
static unsigned portrait_page_count() { return (portrait_pages.count + 3) / 4; }

static bool sdk_comm_screen_active()
{
    const auto* comm = Simulator::cCommManager::Get();
    return comm && comm->IsCommScreenActive();
}
static bool (*portrait_comm_screen_active)() = sdk_comm_screen_active;
static bool ensure_portrait_pager(Native::RivalPortraitPanel*);
static void dispose_portrait_pager();
static void draw_portrait_pager(Native::RivalPortraitPanel*);
static bool (*prepare_portrait_pager)(Native::RivalPortraitPanel*) = ensure_portrait_pager;
static void (*clear_portrait_pager)() = dispose_portrait_pager;
static void (*refresh_portrait_pager)(Native::RivalPortraitPanel*) = draw_portrait_pager;
static void reset_portrait_pagination()
{
    clear_portrait_pager();
    portrait_pages = {};
}

#include "PortraitOrder.h"

static void update_portrait_roster(Native::RivalPortraitPanel* panel, const CivilizationList& civs)
{
    if (portrait_pages.panel_identity != reinterpret_cast<uintptr_t>(panel)) {
        portrait_pages = {};
        portrait_pages.panel_identity = reinterpret_cast<uintptr_t>(panel);
    }
    update_portrait_order(civs);
    // Surviving qualifiers keep their admission order. Eliminations compact
    // the list; new arrivals append even when their object ID is lower.
    PortraitPages::Entry ordered[9]{};
    unsigned count = 0;
    for (unsigned rank = 0; rank < portrait_order.count; ++rank) {
        Civilization* chosen = nullptr;
        for (const auto& candidate : civs)
            if (eligible_portrait(candidate.get()) && candidate->mID == portrait_order.ids[rank])
                chosen = candidate.get();
        if (!chosen) continue;
        const auto identity = portrait_identity(chosen);
        const int old_index = roster_index(identity);
        if (old_index >= 0) ordered[count] = portrait_pages.entries[old_index];
        ordered[count].civilization = identity;
        ordered[count].last_city = chosen->mCities.size() == 1 ? portrait_identity(chosen->mCities[0].get()) : 0;
        ++count;
    }
    for (unsigned i = 0; i < 9; ++i) portrait_pages.entries[i] = ordered[i];
    portrait_pages.count = count;
    const auto pages = portrait_page_count();
    if (!pages) portrait_pages.page = 0;
    else if (portrait_pages.page >= pages) portrait_pages.page = pages - 1;
}

static bool reconcile_paged_portraits(Native::RivalPortraitPanel* panel, const CivilizationList& civs)
{
    // Without working navigation, retain the previous four-slot behavior.
    if (!prepare_portrait_pager(panel)) return false;
    update_portrait_roster(panel, civs);
    const bool talking = portrait_comm_screen_active();
    const int step = portrait_pages.requested_step;
    portrait_pages.requested_step = 0;
    if (talking) return true; // Never swap a portrait beneath an open conversation.
    const auto pages = portrait_page_count();
    const bool manual = step != 0;
    if (manual && pages) {
        const int desired = int(portrait_pages.page) + step;
        if (desired >= 0 && desired < int(pages)) portrait_pages.page = unsigned(desired);
    }
    // A new incoming call on a hidden page is surfaced once. A deliberate page
    // change wins; pending calls remain indicated until the game clears them.
    int incoming = -1;
    bool visible_call = false;
    for (unsigned i = 0; i < portrait_pages.count; ++i)
        if (i / 4 == portrait_pages.page &&
            resolve_portrait(civs, portrait_pages.entries[i].civilization)->mCurrentCommEventId != UINT32_MAX)
            visible_call = true;
    for (unsigned i = 0; i < portrait_pages.count; ++i) {
        auto& entry = portrait_pages.entries[i];
        const auto* civ = resolve_portrait(civs, entry.civilization);
        const auto event = civ->mCurrentCommEventId;
        if (event == UINT32_MAX) entry.noticed_event = UINT32_MAX;
        else if (event != entry.noticed_event) {
            if (!manual && !visible_call && i / 4 != portrait_pages.page && incoming < 0) incoming = int(i / 4);
            // Mark only the page actually shown below, so simultaneous calls
            // on another page remain pending instead of silently disappearing.
        }
    }
    if (incoming >= 0) portrait_pages.page = unsigned(incoming);
    portrait_pages.hidden_call_before = portrait_pages.hidden_call_after = false;
    for (unsigned i = 0; i < portrait_pages.count; ++i) {
        auto& entry = portrait_pages.entries[i];
        const auto event = resolve_portrait(civs, entry.civilization)->mCurrentCommEventId;
        if (i / 4 == portrait_pages.page || manual) entry.noticed_event = event;
        if (event != UINT32_MAX) {
            if (i / 4 < portrait_pages.page) portrait_pages.hidden_call_before = true;
            else if (i / 4 > portrait_pages.page) portrait_pages.hidden_call_after = true;
        }
    }
    Civilization* wanted[4] = {};
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned index = portrait_pages.page * 4 + i;
        if (index < portrait_pages.count) wanted[i] = resolve_portrait(civs, portrait_pages.entries[index].civilization);
    }
    for (int i = 0; i < 4; ++i) {
        auto& slot = panel->slots[i];
        if (slot.civilization && slot.civilization != wanted[i]) {
            cCivilizationPtr retained = slot.civilization;
            native_remove_portrait(panel, retained.get());
            // Native removal may already have cleared this slot. It preserves
            // occupied, and that byte must be cleared before reuse.
            for (int j = 0; j < 4; ++j) {
                if (panel->slots[j].civilization == retained.get()) native_hide_portrait(panel, j);
                if (!panel->slots[j].civilization) panel->slots[j].occupied = false;
            }
            retained->mCommRelPanelSlot = -1;
        }
        if (!slot.civilization) slot.occupied = false;
    }
    for (int i = 0; i < 4; ++i) {
        if (!wanted[i]) continue;
        if (panel->slots[i].civilization != wanted[i]) {
            original_register_portrait(panel, wanted[i]);
            if (panel->slots[i].civilization != wanted[i]) original_restore_portrait(panel, i, wanted[i]);
        } else if (!panel->slots[i].occupied) original_restore_portrait(panel, i, wanted[i]);
    }
    for (const auto& civ : civs)
        if (eligible_portrait(civ.get())) civ->mCommRelPanelSlot = represented_portrait(panel, civ.get());
    bool changed = portrait_pages.reported_count != portrait_pages.count || portrait_pages.reported_page != portrait_pages.page;
    for (int i = 0; i < 4; ++i) {
        const auto identity = portrait_identity(panel->slots[i].civilization);
        changed = changed || portrait_pages.displayed[i] != identity;
        portrait_pages.displayed[i] = identity;
    }
    if (changed && settings.ai_diagnostics)
        ModAPI::Log("[Better Civ UI] page=%u/%u rivals=%u civs=%u,%u,%u,%u reason=%s",
            pages ? portrait_pages.page + 1 : 0, pages, portrait_pages.count,
            unsigned(portrait_pages.displayed[0] >> 32), unsigned(portrait_pages.displayed[1] >> 32),
            unsigned(portrait_pages.displayed[2] >> 32), unsigned(portrait_pages.displayed[3] >> 32),
            manual ? "navigation" : incoming >= 0 ? "incoming-call" : "roster-update");
    portrait_pages.reported_count = portrait_pages.count;
    portrait_pages.reported_page = portrait_pages.page;
    return true;
}

#include "PortraitPagerUI.h"
