#pragma once
// Included inside BetterCiv after the existing bounded portrait adapters.
using UpdatePortraits = void (__thiscall*)(Native::RivalPortraitPanel*, int);
static UpdatePortraits original_update_portraits;
static bool repairing_portraits;

static const CivilizationList& sdk_portrait_civilizations()
{
    return Simulator::GetData<Civilization>().data;
}
const CivilizationList& (*portrait_civilizations)() = sdk_portrait_civilizations;

// No borrowed object/UI pointer survives a callback. Identity values are only
// compared, never dereferenced, and are cleared by the save/mode lifecycle.
struct PortraitHistory {
    uintptr_t panel_identity = 0;
    struct Entry { uint64_t civilization = 0, last_city = 0; } slots[4];
};
static PortraitHistory portrait_history;
static void reset_portrait_history() { portrait_history = {}; }
static Civilization* last_city_conqueror(const CivilizationList& civs, const PortraitHistory::Entry& old)
{
    if (!old.last_city) return nullptr;
    for (const auto& civ : civs) {
        if (!eligible_portrait(civ.get()) || portrait_identity(civ.get()) == old.civilization) continue;
        for (const auto& city : civ->mCities)
            if (city && !city->mbIsDestroyed && portrait_identity(city.get()) == old.last_city) return civ.get();
    }
    return nullptr;
}

static void reconcile_portraits(Native::RivalPortraitPanel* panel, const CivilizationList& civs)
{
    if (portrait_history.panel_identity != reinterpret_cast<uintptr_t>(panel)) reset_portrait_history();
    Civilization* preferred[4] = {};
    for (int i = 0; i < 4; ++i) {
        auto& slot = panel->slots[i];
        // A valid live portrait keeps its position. Priority is only relevant
        // to a vacancy, never a reason to evict a surviving nation.
        if (!eligible_portrait(slot.civilization) || !listed_portrait(civs, slot.civilization))
            preferred[i] = last_city_conqueror(civs, portrait_history.slots[i]);
        if (slot.civilization) {
            cCivilizationPtr retained = slot.civilization;
            const int first = represented_portrait(panel, retained.get());
            if (first != i) {
                // Hide releases this slot's reference; restoring the retained
                // first slot also repairs the native civilization-to-slot map.
                native_hide_portrait(panel, i);
                slot.occupied = false;
                original_restore_portrait(panel, first, retained.get());
                retained->mCommRelPanelSlot = first;
            } else if (!eligible_portrait(retained.get()) || !listed_portrait(civs, retained.get())) {
                // Native removal erases the lookup map and hides/releases the
                // portrait, but does NOT clear its occupied byte.
                native_remove_portrait(panel, retained.get());
                for (int j = 0; j < 4; ++j) {
                    if (panel->slots[j].civilization == retained.get()) native_hide_portrait(panel, j);
                    if (!panel->slots[j].civilization) panel->slots[j].occupied = false;
                }
                retained->mCommRelPanelSlot = -1;
            } else if (!slot.occupied) {
                original_restore_portrait(panel, i, retained.get());
            }
        }
        if (!slot.civilization && slot.occupied) {
            slot.occupied = false;
            if (settings.ai_diagnostics) ModAPI::Log("[Better Civ UI] freed-slot=%d", i);
        }
    }
    for (const auto& civ : civs)
        if (eligible_portrait(civ.get())) civ->mCommRelPanelSlot = represented_portrait(panel, civ.get());

    for (int i = 0; i < 4; ++i) {
        if (panel->slots[i].civilization || panel->slots[i].occupied) continue;
        auto* chosen = preferred[i];
        if (chosen && represented_portrait(panel, chosen) >= 0) chosen = nullptr;
        if (!chosen) {
            for (auto* conqueror : preferred)
                if (conqueror && represented_portrait(panel, conqueror) < 0) { chosen = conqueror; break; }
        }
        const bool is_conqueror = chosen != nullptr;
        if (!chosen) {
            for (const auto& candidate : civs) {
                auto* civ = candidate.get();
                if (eligible_portrait(civ) && represented_portrait(panel, civ) < 0 &&
                    (!chosen || civ->mID < chosen->mID)) chosen = civ;
            }
        }
        if (!chosen) continue;
        // Native registration updates the lookup map, both panel variants,
        // intrusive references, saved index and first-slot visibility.
        original_register_portrait(panel, chosen);
        if (represented_portrait(panel, chosen) < 0 && !panel->slots[i].civilization) {
            // A legacy map entry can make native registration return early.
            // Rebind only this verified empty slot; do not erase an unrelated
            // slot through the stale map's saved index.
            original_restore_portrait(panel, i, chosen);
            chosen->mCommRelPanelSlot = i;
        }
        if (settings.ai_diagnostics && panel->slots[i].civilization == chosen)
            ModAPI::Log("[Better Civ UI] filled-slot=%d civ=%u priority=%s", i, chosen->mID,
                is_conqueror ? "conqueror" : "waiting-rival");
    }
    portrait_history = {};
    portrait_history.panel_identity = reinterpret_cast<uintptr_t>(panel);
    for (int i = 0; i < 4; ++i) {
        const auto* civ = panel->slots[i].civilization;
        if (!eligible_portrait(civ)) continue;
        portrait_history.slots[i].civilization = portrait_identity(civ);
        if (civ->mCities.size() == 1 && civ->mCities[0] && !civ->mCities[0]->mbIsDestroyed)
            portrait_history.slots[i].last_city = portrait_identity(civ->mCities[0].get());
    }
}

#include "PortraitPagerUI.h"
bool (*prepare_portrait_pager)(Native::RivalPortraitPanel*) = ensure_portrait_pager;
void (*clear_portrait_pager)() = dispose_portrait_pager;
void (*refresh_portrait_pager)(Native::RivalPortraitPanel*) = draw_portrait_pager;
#include "PortraitSpecialties.h"

member_detour(reuse_portrait_slots, Native::RivalPortraitPanel, void(int deltaTime)) {
    void detoured(int deltaTime) {
        if (portrait_reuse_ready && in_civilization() && !repairing_portraits) {
            repairing_portraits = true;
            // Hold SDK references only during this UI callback. Re-entrant UI
            // updates skip reconciliation; no capture transaction is intercepted.
            const auto civs = portrait_civilizations();
            if (!portrait_pagination_enabled || !reconcile_paged_portraits(this, civs))
                reconcile_portraits(this, civs);
            refresh_portrait_specialties(this, civs);
            repair_portrait_call_alerts(this, civs);
            repairing_portraits = false;
        }
        original_update_portraits(this, deltaTime);
        if (portrait_pagination_enabled && portrait_reuse_ready && in_civilization() && !repairing_portraits)
            refresh_portrait_pager(this);
    }
};
