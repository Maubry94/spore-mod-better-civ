// Regression for Vaudicoge: completed SDK models remain in native preload
// list 2 after the completion notification has been cleared.
static Graphics::IModelWorld* preload_test_world;
static Graphics::IModelWorld* __cdecl mock_civ_world() { return preload_test_world; }

static void verify_preload_recheck()
{
    Native::CivModelPreloads storage{};
    auto* world = reinterpret_cast<Graphics::IModelWorld*>(&storage);
    Graphics::Model models[8]{};
    Graphics::Model* pending[8];
    for (unsigned i = 0; i < 8; ++i) {
        models[i].mpWorld = world;
        models[i].mFlags = 0x84001;
        models[i].mnRefCount = 1;
        pending[i] = &models[i];
    }
    storage.begin = pending;
    storage.end = storage.capacity = pending + 8;
    const auto before = storage;
    original_civ_model_world = mock_civ_world;
    preload_test_world = world;
    game_mode = mock_mode;
    current_mode = kCivilizationMode;
    assert(rechecked_civ_model_world() == world && storage.completion_dirty);
    assert(storage.begin == before.begin && storage.end == before.end && storage.capacity == before.capacity);
    for (const auto& model : models) assert(model.mFlags == 0x84001 && model.mnRefCount == 1);
    assert(!Native::RecheckCompletedCivPreloads(world)); // already scheduled

    storage.completion_dirty = false;
    current_mode = 0;
    assert(rechecked_civ_model_world() == world && !storage.completion_dirty);
    current_mode = kCivilizationMode;
    for (unsigned i = 0; i < 8; ++i) {
        models[i].mFlags &= ~Graphics::kModelFlagIsLoaded;
        assert(rechecked_civ_model_world() == world && !storage.completion_dirty);
        models[i].mFlags |= Graphics::kModelFlagIsLoaded;
    }
    pending[3] = nullptr;
    assert(!Native::RecheckCompletedCivPreloads(world));
    pending[3] = &models[3];
    models[3].mpWorld = nullptr;
    assert(!Native::RecheckCompletedCivPreloads(world));
    models[3].mpWorld = world;
    storage.end = storage.begin;
    assert(!Native::RecheckCompletedCivPreloads(world));
    storage.end = pending + 8; storage.capacity = pending + 7;
    assert(!Native::RecheckCompletedCivPreloads(world));
    preload_test_world = nullptr;
    assert(rechecked_civ_model_world() == nullptr);
    puts("Preload recheck: completed models preserved; incomplete, foreign and empty lists untouched.");
}
