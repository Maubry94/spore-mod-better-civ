#pragma once
static_assert(offsetof(City, mVehicleSpecialty) == 0x540);
// The native portrait binder derives the icon from the first owned city's
// current specialty (0xBF1080 -> 0xBEFA70). Losing that city can change the icon.
struct PortraitSpecialties {
    uintptr_t panel_identity = 0;
    struct Entry { uint64_t civ = 0; uint32_t city = 0; int purpose = -1; } slots[4];
};
static PortraitSpecialties portrait_specialties;
static void reset_portrait_specialties() { portrait_specialties = {}; }
static void refresh_portrait_specialties(Native::RivalPortraitPanel* panel, const CivilizationList& civs)
{
    if (!panel || !original_restore_portrait || portrait_comm_screen_active()) return;
    auto& state = portrait_specialties;
    if (state.panel_identity != reinterpret_cast<uintptr_t>(panel)) {
        state = {}; state.panel_identity = reinterpret_cast<uintptr_t>(panel);
    }
    for (int i = 0; i < 4; ++i) {
        auto& before = state.slots[i];
        const auto& slot = panel->slots[i];
        auto* civ = slot.civilization;
        if (!slot.occupied || !listed_portrait(civs, civ) || !eligible_portrait(civ) ||
            !civ->mCities[0] || civ->mCities[0]->mbIsDestroyed) { before = {}; continue; }
        const auto* first = civ->mCities[0].get();
        const int purpose = first->mVehicleSpecialty;
        if (purpose < kMilitary || purpose > kEconomic) { before = {}; continue; }
        const auto identity = portrait_identity(civ);
        const bool changed = before.civ == identity && before.purpose != purpose;
        if (changed) {
            // Reuse the already guarded native binder, never guess a widget
            // interface or modify the civilization's actual specialty.
            original_restore_portrait(panel, i, civ);
            for (auto& entry : portrait_call_alerts.windows[i]) entry = {};
        }
        if (settings.ai_diagnostics &&
            (before.civ != identity || before.city != first->mID || before.purpose != purpose))
            ModAPI::Log("[Better Civ UI] portrait-specialty slot=%d civ=%u first-city=%u purpose=%d previous=%d refresh=%d (0=military 1=religious 2=economic)",
                i, civ->mID, first->mID, purpose, before.civ == identity ? before.purpose : -1, changed);
        before = {identity, first->mID, purpose};
    }
}
