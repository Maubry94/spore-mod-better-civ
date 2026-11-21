#include "PortraitPages.h"
#include "PortraitCallAlerts.h"
#include "Runtime.h"
#include <Spore/ModAPI.h>
#include <Spore/Simulator/SubSystem/CommManager.h>
#include <Spore/Simulator/SubSystem/PlanetModel.h>
#include <Spore/UTFWin/IWindow.h>
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace BetterCiv {
static_assert(offsetof(Simulator::cPlanetModel, mPlanetKey) == 0x2c);

RegisterPortrait original_register_portrait;
RestorePortrait original_restore_portrait;
HidePortrait native_hide_portrait;
RemovePortrait native_remove_portrait;
FindPortraitWindow native_find_portrait_window;
ApplyPortraitMaterial native_apply_portrait_material;
bool portrait_reuse_ready;

uint64_t portrait_identity(const Simulator::cGameData* object)
{
    return object ? (uint64_t(object->mID) << 32) | reinterpret_cast<uintptr_t>(object) : 0;
}
bool eligible_portrait(const Civilization* civ)
{
    return civ && !civ->mbIsDestroyed && !civ->mIsPlayerOwned && !civ->mIsNeutral &&
        civ->mCommNotifiedFirstCityCapture && !civ->mCities.empty();
}
int represented_portrait(const Native::RivalPortraitPanel* panel, const Civilization* civ)
{
    for (int i = 0; i < 4; ++i)
        if (panel->slots[i].civilization == civ && civ) return i;
    return -1;
}
bool listed_portrait(const CivilizationList& civs, const Civilization* civ)
{
    for (const auto& item : civs) if (item.get() == civ) return true;
    return false;
}

PortraitOrder portrait_order;
void reset_portrait_order() { portrait_order = {}; }
static bool sdk_portrait_order_key(const CivilizationList& civs, uint32_t* key)
{
    const Civilization* player = nullptr;
    for (const auto& civ : civs) {
        if (civ && civ->mIsPlayerOwned && !civ->mbIsDestroyed) {
            player = civ.get();
            break;
        }
    }
    if (!player || !player->mID) return false;
    const auto* model = Simulator::cPlanetModel::Get();
    if (!model || !model->mPlanetKey.instanceID) return false;
    key[0] = model->mPlanetKey.instanceID;
    key[1] = model->mPlanetKey.typeID;
    key[2] = model->mPlanetKey.groupID;
    key[3] = player->mID;
    return true;
}
bool (*portrait_order_key)(const CivilizationList&, uint32_t*) = sdk_portrait_order_key;

uint32_t portrait_order_checksum(const PortraitOrderFile& record)
{
    uint32_t hash = 2166136261u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    for (size_t i = 0; i < offsetof(PortraitOrderFile, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
bool valid_portrait_order_file(const PortraitOrderFile& file, const uint32_t* key)
{
    if (file.magic != 0x324f4342 || file.version != 1 || file.count > 9 ||
        memcmp(file.key, key, sizeof file.key) || file.checksum != portrait_order_checksum(file)) return false;
    for (unsigned i = 0; i < 9; ++i) {
        const uint32_t id = file.ids[i];
        if (i >= file.count) {
            if (id != 0) return false;
            continue;
        }
        if (id == 0 || id == UINT32_MAX) return false;
        for (unsigned j = 0; j < i; ++j) {
            if (file.ids[j] == id) return false;
        }
    }
    return true;
}
static bool portrait_order_path(wchar_t* path, size_t capacity, const uint32_t* key, bool create)
{
    wchar_t root[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", root, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    if (swprintf_s(path, capacity, L"%s\\Spore", root) < 0) return false;
    if (create && !CreateDirectoryW(path, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    if (swprintf_s(path, capacity, L"%s\\Spore\\BetterCiv", root) < 0) return false;
    if (create && !CreateDirectoryW(path, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    if (swprintf_s(path, capacity, L"%s\\Spore\\BetterCiv\\PortraitOrder", root) < 0) return false;
    if (create && !CreateDirectoryW(path, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    return swprintf_s(path, capacity, L"%s\\Spore\\BetterCiv\\PortraitOrder\\%08X-%08X-%08X-%08X.bin",
        root, key[0], key[1], key[2], key[3]) >= 0;
}
bool read_portrait_order_file(const wchar_t* path, const uint32_t* key, PortraitOrderFile& result)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"rb") || !file) return false;
    PortraitOrderFile data{};
    const bool complete = fread(&data, 1, sizeof data, file) == sizeof data && fgetc(file) == EOF;
    fclose(file);
    if (!complete || !valid_portrait_order_file(data, key)) return false;
    result = data;
    return true;
}
bool write_portrait_order_file(const wchar_t* path, const PortraitOrderFile& data)
{
    wchar_t temporary[MAX_PATH]{};
    if (swprintf_s(temporary, L"%s.tmp", path) < 0) return false;
    FILE* file = nullptr;
    if (_wfopen_s(&file, temporary, L"wb") || !file) return false;
    const bool complete = fwrite(&data, 1, sizeof data, file) == sizeof data;
    const bool closed = fclose(file) == 0;
    if (complete && closed && MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(temporary); // Only our fixed .tmp sibling, never a save or input file.
    return false;
}
static bool disk_read_portrait_order(const uint32_t* key, PortraitOrderFile& data)
{
    wchar_t path[MAX_PATH]{};
    return portrait_order_path(path, MAX_PATH, key, false) && read_portrait_order_file(path, key, data);
}
static bool disk_write_portrait_order(const uint32_t* key, const PortraitOrderFile& data)
{
    wchar_t path[MAX_PATH]{};
    return portrait_order_path(path, MAX_PATH, key, true) && write_portrait_order_file(path, data);
}
bool (*read_portrait_order)(const uint32_t*, PortraitOrderFile&) = disk_read_portrait_order;
bool (*write_portrait_order)(const uint32_t*, const PortraitOrderFile&) = disk_write_portrait_order;
static bool known_portrait_order(uint32_t id)
{
    for (unsigned i = 0; i < portrait_order.count; ++i) {
        if (portrait_order.ids[i] == id) return true;
    }
    return false;
}
static void append_portrait_order(uint32_t id)
{
    if (id && id != UINT32_MAX && portrait_order.count < 9 && !known_portrait_order(id))
        portrait_order.ids[portrait_order.count++] = id;
}
void note_portrait_qualification(const Civilization* civ)
{
    // Called by the already guarded first-conquest registration hook. Captures
    // happening within the same UI frame retain their native notification order.
    if (portrait_order.initialized && eligible_portrait(civ)) append_portrait_order(civ->mID);
}
static void update_portrait_order(const CivilizationList& civs)
{
    auto& order = portrait_order;
    if (!order.initialized) {
        order.initialized = true;
        order.has_key = portrait_order_key(civs, order.key);
        PortraitOrderFile data{};
        if (order.has_key && read_portrait_order(order.key, data) && valid_portrait_order_file(data, order.key)) {
            for (unsigned i = 0; i < data.count; ++i) {
                // A previous save can predate the first conquest of a cached rival.
                // Keep only IDs that already existed and qualified in this timeline.
                bool present = false;
                bool still_waiting = false;
                for (const auto& civ : civs) {
                    if (!civ || civ->mID != data.ids[i]) continue;
                    present = true;
                    if (!civ->mbIsDestroyed && !civ->mCommNotifiedFirstCityCapture) {
                        still_waiting = true;
                    }
                }
                if (present && !still_waiting) append_portrait_order(data.ids[i]);
            }
            if (settings.ai_diagnostics) ModAPI::Log("[Better Civ UI] arrival-order=restored entries=%u", order.count);
        }
    }
    // A legacy save has no recorded chronology. Use its former ID order once.
    // Later qualifiers append; they never displace previously admitted rivals.
    while (order.count < 9) {
        const Civilization* chosen = nullptr;
        for (const auto& civ : civs) {
            if (!eligible_portrait(civ.get())) continue;
            const uint32_t id = civ->mID;
            if (!id || id == UINT32_MAX || known_portrait_order(id)) continue;
            if (!chosen || id < chosen->mID) chosen = civ.get();
        }
        if (!chosen) break;
        append_portrait_order(chosen->mID);
    }
}
void save_portrait_order()
{
    if (!portrait_order.initialized || !portrait_order.count) return;
    // A freshly generated world can expose its UI before its planet key.
    if (!portrait_order.has_key)
        portrait_order.has_key = portrait_order_key(portrait_civilizations(), portrait_order.key);
    if (!portrait_order.has_key) {
        if (settings.ai_diagnostics) ModAPI::Log("[Better Civ UI] arrival-order=not-saved (planet identity unavailable)");
        return;
    }
    PortraitOrderFile data{};
    memcpy(data.key, portrait_order.key, sizeof data.key);
    data.count = portrait_order.count;
    memcpy(data.ids, portrait_order.ids, sizeof data.ids);
    data.checksum = portrait_order_checksum(data);
    const bool saved = write_portrait_order(data.key, data);
    if (settings.ai_diagnostics || !saved)
        ModAPI::Log("[Better Civ UI] arrival-order=%s entries=%u (UI cache only)", saved ? "saved" : "save-failed", data.count);
}

PortraitPages portrait_pages;
bool pager_owns_panel(const Native::RivalPortraitPanel* panel)
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
int roster_index(uint64_t identity)
{
    if (!identity) return -1;
    for (unsigned i = 0; i < 9; ++i) {
        if (portrait_pages.entries[i].civilization == identity) return int(i);
    }
    return -1;
}
unsigned portrait_page_count() { return (portrait_pages.count + 3) / 4; }

bool sdk_comm_screen_active()
{
    const auto* comm = Simulator::cCommManager::Get();
    return comm && comm->IsCommScreenActive();
}
bool (*portrait_comm_screen_active)() = sdk_comm_screen_active;

void reset_portrait_pagination()
{
    clear_portrait_pager();
    portrait_pages = {};
}

void update_portrait_roster(Native::RivalPortraitPanel* panel, const CivilizationList& civs)
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
        for (const auto& candidate : civs) {
            if (eligible_portrait(candidate.get()) && candidate->mID == portrait_order.ids[rank]) {
                chosen = candidate.get();
            }
        }
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

bool reconcile_paged_portraits(Native::RivalPortraitPanel* panel, const CivilizationList& civs)
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
    // Query each live rival once. Only the four displayed civilizations will be bound to slots.
    Civilization* rivals[9]{};
    uint32_t events[9]{};
    bool visible_call = false;
    for (unsigned i = 0; i < portrait_pages.count; ++i) {
        rivals[i] = resolve_portrait(civs, portrait_pages.entries[i].civilization);
        events[i] = rivals[i] ? rivals[i]->mCurrentCommEventId : UINT32_MAX;
        if (i / 4 == portrait_pages.page && events[i] != UINT32_MAX) visible_call = true;
    }

    // Navigation and an already visible call take precedence over a new hidden call.
    int incoming = -1;
    for (unsigned i = 0; i < portrait_pages.count; ++i) {
        auto& entry = portrait_pages.entries[i];
        const unsigned page = i / 4;
        if (events[i] == UINT32_MAX) {
            entry.noticed_event = UINT32_MAX;
            continue;
        }
        if (!manual && !visible_call && incoming < 0 &&
            page != portrait_pages.page && entry.noticed_event != events[i]) {
            incoming = int(page);
        }
    }
    if (incoming >= 0) portrait_pages.page = unsigned(incoming);

    // Mark other calls as noticed on a manual turn to avoid snapping back, but
    // keep the corresponding arrow lit until the game clears the pending event.
    portrait_pages.hidden_call_before = false;
    portrait_pages.hidden_call_after = false;
    Civilization* wanted[4]{};
    for (unsigned i = 0; i < portrait_pages.count; ++i) {
        const unsigned page = i / 4;
        if (page == portrait_pages.page || manual) portrait_pages.entries[i].noticed_event = events[i];
        if (page == portrait_pages.page) {
            wanted[i % 4] = rivals[i];
        } else if (events[i] != UINT32_MAX) {
            if (page < portrait_pages.page) portrait_pages.hidden_call_before = true;
            else portrait_pages.hidden_call_after = true;
        }
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

PortraitCallAlerts portrait_call_alerts;
void reset_portrait_call_alerts() { portrait_call_alerts = {}; }

void repair_portrait_call_alerts(Native::RivalPortraitPanel* panel,
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
        for (auto& slot : state.windows) {
            for (auto& entry : slot) entry = {};
        }
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
            if (!window) {
                entry = {};
                continue;
            }
            const auto window_identity = reinterpret_cast<uintptr_t>(window);
            if (entry.civilization == identity && entry.window_identity == window_identity &&
                entry.event == event) continue; // Do not restart the animation each frame.

            // Native Update skips a button that still has its old material.
            // Rebind on a new caller, widget or event so a reused slot blinks
            // for the correct nation and clears when that call ends.
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
} // namespace BetterCiv
