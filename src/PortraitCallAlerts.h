#pragma once
// Included after PortraitPages.h. All window and civilization pointers are
// resolved afresh within the UI callback; only scalar identities are retained.
using ApplyPortraitMaterial = void (__cdecl*)(UTFWin::IWindow*, const char16_t*, int);
static ApplyPortraitMaterial native_apply_portrait_material;
static constexpr char16_t kPortraitCallMaterial[] =
    u"ui_material_blink -color (2,2,2) -frequency 1 -intensity 1 -func 1 -func_inv 1";
static constexpr char16_t kPortraitIdleMaterial[] = u"ui_material_default";

struct PortraitCallAlerts {
    uintptr_t panel_identity = 0;
    struct Entry {
        uint64_t civilization = 0;
        uintptr_t window_identity = 0;
        uint32_t event = UINT32_MAX;
    } windows[4][2];
    unsigned log_lines = 0;
};
static PortraitCallAlerts portrait_call_alerts;
static void reset_portrait_call_alerts() { portrait_call_alerts = {}; }

static void repair_portrait_call_alerts(Native::RivalPortraitPanel* panel,
    const CivilizationList& civs)
{
    if (!panel || !native_find_portrait_window || !native_apply_portrait_material) return;
    auto& state = portrait_call_alerts;
    if (state.panel_identity != reinterpret_cast<uintptr_t>(panel)) {
        state = {};
        state.panel_identity = reinterpret_cast<uintptr_t>(panel);
    }
    if (portrait_comm_screen_active()) {
        // The native update clears the caller's material during conversation.
        // Defer until it closes, then use the still-current pending event.
        for (auto& slot : state.windows) for (auto& entry : slot) entry = {};
        return;
    }
    const uint32_t containers[2] = {0x046977ff, 0x055d626f};
    for (unsigned i = 0; i < 4; ++i) {
        const auto& slot = panel->slots[i];
        const auto* civ = slot.civilization;
        // Membership is checked before dereferencing a borrowed slot pointer.
        if (!slot.occupied || !listed_portrait(civs, civ) || !eligible_portrait(civ)) {
            for (auto& entry : state.windows[i]) entry = {};
            continue;
        }
        const auto identity = portrait_identity(civ);
        const auto event = civ->mCurrentCommEventId;
        for (unsigned variant = 0; variant < 2; ++variant) {
            auto& entry = state.windows[i][variant];
            auto* container = native_find_portrait_window(panel, containers[variant]);
            auto* window = container ? container->FindWindowByID(slot.window_id, false) : nullptr;
            if (!window) { entry = {}; continue; }
            const auto window_identity = reinterpret_cast<uintptr_t>(window);
            if (entry.civilization == identity && entry.window_identity == window_identity &&
                entry.event == event) continue; // Do not restart the animation each frame.

            // Native 0xE04A20 skips any existing 0x05234B49 material. A reused
            // button can retain its previous material after 0xE064D0 resets
            // slot byte +5. Explicitly apply the same native script on a
            // binding/event transition, including clearing a stale call effect.
            // 0x808B80 is cdecl (window, UTF-16 script, 0), checked before hooks.
            native_apply_portrait_material(window,
                event == UINT32_MAX ? kPortraitIdleMaterial : kPortraitCallMaterial, 0);
            if (settings.ai_diagnostics && state.log_lines < 512) {
                ModAPI::Log("[Better Civ UI] call-alert=%s slot=%u civ=%u event=%u view=%s reason=%s",
                    event == UINT32_MAX ? "clear" : "rearm", i, civ->mID, event,
                    variant ? "compact" : "expanded",
                    entry.civilization != identity ? "portrait-binding" :
                    entry.window_identity != window_identity ? "widget-binding" : "event-change");
                if (++state.log_lines == 512)
                    ModAPI::Log("[Better Civ UI] Call-alert log limit reached until next lifecycle reset.");
            }
            entry = {identity, window_identity, event};
        }
    }
    // No event ID, relationship, save data, conversation or sound is changed.
    // The original portrait update still owns call acknowledgement and cleanup.
}
