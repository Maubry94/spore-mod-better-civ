#pragma once

// Included inside BetterCiv. A diplomatic victory can leave the political ID
// set on a bare spice node after its civilization has left the simulation.
struct SpiceOwnershipSession {
    struct Candidate { uint64_t key; uint32_t owner; };
    eastl::vector<Candidate> previous;
    static constexpr uint64_t sample_interval = 2000;
    uint64_t next_sample = sample_interval;
    void Reset() { previous.clear(); next_sample = sample_interval; }
};
static SpiceOwnershipSession spice_ownership;

static Civilization* spice_planet_winner(const CivilizationList& civilizations)
{
    Civilization* player = nullptr;
    for (const auto& civilization : civilizations) {
        if (!civilization || civilization->mbIsDestroyed || civilization->mCities.empty()) continue;
        if (!civilization->mIsPlayerOwned || player || civilization->mCities.size() < 10) return nullptr;
        player = civilization.get();
    }
    return player;
}

static bool spice_victory_confirmed(const Civilization& player, const eastl::vector<cCityPtr>& cities)
{
    if (player.mCities.size() != cities.size()) return false;
    for (const auto& city : cities) {
        if (!city || city->mbIsDestroyed || city->mpCivilization.get() != &player ||
            std::count(player.mCities.begin(), player.mCities.end(), city) != 1 ||
            std::count(cities.begin(), cities.end(), city) != 1) return false;
    }
    return true;
}

static bool spice_node_is_idle(const Simulator::cCommodityNode& node)
{
    return !node.mbIsDestroyed && node.mID != UINT32_MAX && node.mbMineInitialized &&
        node.mMineState == 0 && !node.mpGameDataOwner && !node.mConstructingVehicle &&
        node.mPoliticalID != UINT32_MAX && node.mPoliticalID != 0 &&
        node.mConstructingPoliticalID == UINT32_MAX && node.mCapturePoliticalID == UINT32_MAX &&
        node.mConvertPoliticalID == UINT32_MAX && node.mBuyPoliticalID == UINT32_MAX &&
        node.mCapturePercent == 0 && node.mConvertPercent == 0 && node.mBuyPercent == 0 &&
        node.mMaxHealthPoints > 0 && std::isfinite(node.mMaxHealthPoints) &&
        node.mHealthPoints == node.mMaxHealthPoints;
}

static void inspect_orphan_spice_nodes(const CivilizationList& civilizations,
    const eastl::vector<cCommodityNodePtr>& nodes)
{
    eastl::vector<SpiceOwnershipSession::Candidate> pending;
    for (const auto& node : nodes) {
        if (!node || !spice_node_is_idle(*node)) continue;
        const uint32_t owner = node->mPoliticalID;
        // Even a cityless civilization can still be present during a handover.
        const auto has_owner = [owner](const cCivilizationPtr& civilization) {
            return civilization && !civilization->mbIsDestroyed && political_id(civilization.get()) == owner;
        };
        if (std::any_of(civilizations.begin(), civilizations.end(), has_owner)) continue;
        const auto key = object_key(node.get());
        const auto same_candidate = [key, owner](const SpiceOwnershipSession::Candidate& entry) {
            return entry.key == key && entry.owner == owner;
        };
        if (!std::any_of(spice_ownership.previous.begin(), spice_ownership.previous.end(), same_candidate)) {
            pending.push_back({key, owner});
            continue;
        }
        node->SetPoliticalID(UINT32_MAX);
        if (settings.ai_diagnostics && node->mPoliticalID == UINT32_MAX)
            ModAPI::Log("[Better Civ Spice] t=%llu event=clear-orphan node=%u former-owner=%u",
                ai_time, node->mID, owner);
    }
    spice_ownership.previous.swap(pending);
}

static void repair_orphan_spice_nodes(int delta, bool civilization_mode)
{
    if (!ai_observer_ready || !civilization_mode || delta <= 0 || ai_time < spice_ownership.next_sample) return;
    spice_ownership.next_sample = ai_time + SpiceOwnershipSession::sample_interval;
    const auto& civilizations = portrait_civilizations();
    const auto* player = spice_planet_winner(civilizations);
    if (!player || !spice_victory_confirmed(*player, Simulator::GetData<City>().data)) {
        spice_ownership.previous.clear();
        return;
    }
    inspect_orphan_spice_nodes(civilizations, Simulator::GetData<Simulator::cCommodityNode>().data);
}
