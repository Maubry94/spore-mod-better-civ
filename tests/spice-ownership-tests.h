#pragma once

static unsigned spice_owner_writes;
static void __fastcall spice_test_set_owner(Simulator::cCommodityNode* node, int, uint32_t owner)
{
    ++spice_owner_writes;
    node->mPoliticalID = owner;
}
static uint32_t __fastcall spice_test_get_owner(const Civilization* civilization, int)
{
    return civilization->mPoliticalID;
}

static void verify_spice_ownership()
{
    alignas(8) unsigned char civ_bytes[2][sizeof(Civilization)] = {};
    alignas(8) unsigned char city_bytes[10][sizeof(City)] = {};
    alignas(8) unsigned char node_bytes[sizeof(Simulator::cCommodityNode)] = {};
    uintptr_t table[20] = {};
    table[0] = reinterpret_cast<uintptr_t>(ai_mock_addref);
    table[1] = reinterpret_cast<uintptr_t>(ai_mock_release);
    table[0x48/4] = reinterpret_cast<uintptr_t>(spice_test_set_owner);
    table[0x4c/4] = reinterpret_cast<uintptr_t>(spice_test_get_owner);
    auto* player = reinterpret_cast<Civilization*>(civ_bytes[0]);
    auto* rival = reinterpret_cast<Civilization*>(civ_bytes[1]);
    auto* node = reinterpret_cast<Simulator::cCommodityNode*>(node_bytes);
    for (auto* object : {static_cast<void*>(player), static_cast<void*>(rival), static_cast<void*>(node)})
        put_pointer(object, 0, table);
    player->mIsPlayerOwned = true;
    player->mPoliticalID = 335;
    rival->mPoliticalID = 11752;
    CivilizationList civilizations;
    civilizations.push_back(player);
    eastl::vector<cCityPtr> cities;
    City* city_pointers[10];
    for (unsigned i = 0; i < 10; ++i) {
        auto* city = reinterpret_cast<City*>(city_bytes[i]);
        put_pointer(city, 0, table);
        put_pointer(city, offsetof(City, mpCivilization), player);
        city_pointers[i] = city;
        cities.push_back(city);
    }
    put_pointer(player, kCivCitiesBegin, city_pointers);
    put_pointer(player, kCivCitiesEnd, city_pointers+9);
    assert(!spice_planet_winner(civilizations));
    put_pointer(player, kCivCitiesEnd, city_pointers+10);
    assert(spice_planet_winner(civilizations) == player && spice_victory_confirmed(*player, cities));
    cities[9] = cities[8];
    assert(!spice_victory_confirmed(*player, cities));
    cities[9] = city_pointers[9];
    put_pointer(city_pointers[9], offsetof(City, mpCivilization), rival);
    assert(!spice_victory_confirmed(*player, cities));
    put_pointer(city_pointers[9], offsetof(City, mpCivilization), player);
    civilizations.push_back(rival);
    put_pointer(rival, kCivCitiesBegin, city_pointers+9);
    put_pointer(rival, kCivCitiesEnd, city_pointers+10);
    assert(!spice_planet_winner(civilizations));
    put_pointer(rival, kCivCitiesEnd, city_pointers+9);
    assert(spice_planet_winner(civilizations) == player);

    node->mID = 684;
    node->mPoliticalID = rival->mPoliticalID;
    node->mbMineInitialized = true;
    node->mConstructingPoliticalID = node->mCapturePoliticalID =
        node->mConvertPoliticalID = node->mBuyPoliticalID = UINT32_MAX;
    node->mHealthPoints = node->mMaxHealthPoints = 10;
    eastl::vector<cCommodityNodePtr> nodes;
    nodes.push_back(node);
    const auto sample = [&]() { inspect_orphan_spice_nodes(civilizations, nodes); };
    spice_ownership.Reset(); spice_owner_writes = 0;
    sample(); sample();
    assert(!spice_owner_writes && spice_ownership.previous.empty()); // The cityless owner still exists.
    civilizations.pop_back();
    sample();
    assert(!spice_owner_writes && spice_ownership.previous.size() == 1);
    alignas(8) unsigned char header_bytes[sizeof(Simulator::cSavedGameHeader)] = {};
    ai_lifecycle.OnLoad(*reinterpret_cast<const Simulator::cSavedGameHeader*>(header_bytes));
    sample();
    assert(!spice_owner_writes && spice_ownership.previous.size() == 1);
    for (auto* active : {&node->mConstructingPoliticalID, &node->mCapturePoliticalID,
                         &node->mConvertPoliticalID, &node->mBuyPoliticalID}) {
        *active = player->mPoliticalID;
        sample(); assert(spice_ownership.previous.empty() && !spice_owner_writes);
        *active = UINT32_MAX;
    }
    for (auto* progress : {&node->mCapturePercent, &node->mConvertPercent, &node->mBuyPercent}) {
        *progress = 0.01f; assert(!spice_node_is_idle(*node)); *progress = 0;
    }
    node->mMineState = 1; assert(!spice_node_is_idle(*node)); node->mMineState = 0;
    node->mbMineInitialized = false; assert(!spice_node_is_idle(*node)); node->mbMineInitialized = true;
    put_pointer(node, offsetof(Simulator::cCommodityNode, mpGameDataOwner), player);
    assert(!spice_node_is_idle(*node));
    put_pointer(node, offsetof(Simulator::cCommodityNode, mpGameDataOwner), nullptr);
    put_pointer(node, offsetof(Simulator::cCommodityNode, mConstructingVehicle), player);
    assert(!spice_node_is_idle(*node));
    put_pointer(node, offsetof(Simulator::cCommodityNode, mConstructingVehicle), nullptr);
    node->mHealthPoints = 9; assert(!spice_node_is_idle(*node)); node->mHealthPoints = 10;
    node->mPoliticalID = player->mPoliticalID; sample(); sample();
    assert(!spice_owner_writes && spice_ownership.previous.empty());
    node->mPoliticalID = 11752; sample(); sample();
    assert(spice_owner_writes == 1 && node->mPoliticalID == UINT32_MAX);
    sample(); sample(); assert(spice_owner_writes == 1);
    spice_ownership.Reset();
}
