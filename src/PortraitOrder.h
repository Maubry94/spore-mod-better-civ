#pragma once
static_assert(offsetof(Simulator::cPlanetModel, mPlanetKey) == 0x2c);
// UI-only history. Native save records and their four physical slot indices
// remain unchanged. Persist a small optional cache when the game saves.
struct PortraitOrder {
    bool initialized = false, has_key = false;
    uint32_t key[4]{};
    uint32_t ids[9]{};
    unsigned count = 0;
};
static PortraitOrder portrait_order;
static void reset_portrait_order() { portrait_order = {}; }
static bool sdk_portrait_order_key(const CivilizationList& civs, uint32_t* key)
{
    const Civilization* player = nullptr;
    for (const auto& civ : civs)
        if (civ && civ->mIsPlayerOwned && !civ->mbIsDestroyed) { player = civ.get(); break; }
    if (!player || !player->mID) return false;
    const auto* model = Simulator::cPlanetModel::Get();
    if (!model || !model->mPlanetKey.instanceID) return false;
    key[0] = model->mPlanetKey.instanceID; key[1] = model->mPlanetKey.typeID;
    key[2] = model->mPlanetKey.groupID; key[3] = player->mID;
    return true;
}
static bool (*portrait_order_key)(const CivilizationList&, uint32_t*) = sdk_portrait_order_key;

struct PortraitOrderFile {
    uint32_t magic = 0x324f4342, version = 1, key[4]{}, count = 0, ids[9]{}, checksum = 0;
};
static_assert(sizeof(PortraitOrderFile) == 68);
static uint32_t portrait_order_checksum(const PortraitOrderFile& record)
{
    uint32_t hash = 2166136261u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&record);
    for (size_t i = 0; i < offsetof(PortraitOrderFile, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
static bool valid_portrait_order_file(const PortraitOrderFile& file, const uint32_t* key)
{
    if (file.magic != 0x324f4342 || file.version != 1 || file.count > 9 ||
        memcmp(file.key, key, sizeof file.key) || file.checksum != portrait_order_checksum(file)) return false;
    for (unsigned i = 0; i < 9; ++i) {
        if (i >= file.count) { if (file.ids[i]) return false; continue; }
        if (!file.ids[i] || file.ids[i] == UINT32_MAX) return false;
        for (unsigned j = 0; j < i; ++j) if (file.ids[j] == file.ids[i]) return false;
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
static bool read_portrait_order_file(const wchar_t* path, const uint32_t* key, PortraitOrderFile& result)
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
static bool write_portrait_order_file(const wchar_t* path, const PortraitOrderFile& data)
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
static bool (*read_portrait_order)(const uint32_t*, PortraitOrderFile&) = disk_read_portrait_order;
static bool (*write_portrait_order)(const uint32_t*, const PortraitOrderFile&) = disk_write_portrait_order;
static bool known_portrait_order(uint32_t id)
{
    for (unsigned i = 0; i < portrait_order.count; ++i) if (portrait_order.ids[i] == id) return true;
    return false;
}
static void append_portrait_order(uint32_t id)
{
    if (id && id != UINT32_MAX && portrait_order.count < 9 && !known_portrait_order(id))
        portrait_order.ids[portrait_order.count++] = id;
}
static void note_portrait_qualification(const Civilization* civ)
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
                bool present = false, future = false;
                // Loading an earlier save must not reserve ranks for rivals
                // whose first conquest has not happened in that save.
                for (const auto& civ : civs)
                    if (civ && civ->mID == data.ids[i]) {
                        present = true;
                        if (!civ->mbIsDestroyed && !civ->mCommNotifiedFirstCityCapture) future = true;
                    }
                // IDs absent from this timeline must not consume all nine
                // places when resuming before those civilizations existed.
                if (present && !future) append_portrait_order(data.ids[i]);
            }
            if (settings.ai_diagnostics) ModAPI::Log("[Better Civ UI] arrival-order=restored entries=%u", order.count);
        }
    }
    // A legacy save has no recorded chronology. Use its former ID order once.
    // Later qualifiers append; they never displace previously admitted rivals.
    for (;;) {
        const Civilization* chosen = nullptr;
        for (const auto& civ : civs)
            if (eligible_portrait(civ.get()) && civ->mID && civ->mID != UINT32_MAX && !known_portrait_order(civ->mID) &&
                (!chosen || civ->mID < chosen->mID)) chosen = civ.get();
        if (!chosen || order.count >= 9) break;
        append_portrait_order(chosen->mID);
    }
}
static void save_portrait_order()
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
    data.count = portrait_order.count; memcpy(data.ids, portrait_order.ids, sizeof data.ids);
    data.checksum = portrait_order_checksum(data);
    const bool saved = write_portrait_order(data.key, data);
    if (settings.ai_diagnostics || !saved)
        ModAPI::Log("[Better Civ UI] arrival-order=%s entries=%u (UI cache only)", saved ? "saved" : "save-failed", data.count);
}
