#pragma once
#include "NativeExtensions.h"
#include "PortraitOrder.h"

namespace UTFWin { class IWindow; }

namespace BetterCiv {
using RegisterPortrait = void (__thiscall*)(Native::RivalPortraitPanel*, Civilization*);
using RestorePortrait = void (__thiscall*)(Native::RivalPortraitPanel*, int, Civilization*);
using HidePortrait = void (__thiscall*)(Native::RivalPortraitPanel*, int);
using RemovePortrait = void (__thiscall*)(Native::RivalPortraitPanel*, Civilization*);
using FindPortraitWindow = UTFWin::IWindow* (__thiscall*)(Native::RivalPortraitPanel*, uint32_t);

extern RegisterPortrait original_register_portrait;
extern RestorePortrait original_restore_portrait;
extern HidePortrait native_hide_portrait;
extern RemovePortrait native_remove_portrait;
extern FindPortraitWindow native_find_portrait_window;
extern bool portrait_reuse_ready;
extern const CivilizationList& (*portrait_civilizations)();
bool sdk_comm_screen_active();
extern bool (*portrait_comm_screen_active)();
extern bool (*prepare_portrait_pager)(Native::RivalPortraitPanel*);
extern void (*clear_portrait_pager)();
extern void (*refresh_portrait_pager)(Native::RivalPortraitPanel*);

// The roster holds up to nine rivals. The native panel owns four visible slots.
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
extern PortraitPages portrait_pages;

uint64_t portrait_identity(const Simulator::cGameData* object);
bool eligible_portrait(const Civilization* civ);
bool listed_portrait(const CivilizationList& civs, const Civilization* civ);
int represented_portrait(const Native::RivalPortraitPanel* panel, const Civilization* civ);
int roster_index(uint64_t identity);
unsigned portrait_page_count();
bool pager_owns_panel(const Native::RivalPortraitPanel* panel);
void reset_portrait_pagination();
void update_portrait_roster(Native::RivalPortraitPanel* panel, const CivilizationList& civs);
bool reconcile_paged_portraits(Native::RivalPortraitPanel* panel, const CivilizationList& civs);
}
