# C++ / ModAPI SDK migration

This is a behavior-preserving migration from the C implementation at `84a2baf`. It does not add gameplay features, change balance, or fix the previously reported city-planner crash.

## Implementation

- Compile a Win32 C++17 DLL with MSVC v143 and the official Spore ModAPI SDK, pinned in `sdk-version.txt`.
- Register initialization through `ModAPI::AddPostInitFunction` and report activation through `ModAPI::Log`. There is no manual lookup of the decorated initialization export.
- Use SDK `cCity`, `cCivilization`, `cVehicle`, `cCommunityLayout`, `Property` and `cArithmeticaResource` types. Iterate the SDK's EASTL containers by reference; do not copy or retain game-owned objects.
- Use SDK `member_detour` adapters for the engine's x86 member calling convention and the SDK virtual `GetPoliticalID` call.
- Keep the original, narrowly scoped call-site patches. Installing function-wide detours instead would affect callers that v1.0 deliberately leaves alone, including capture choices and other game stages.
- Keep executable identity checks and expected instruction bytes before any writes. This still targets Steam Galactic Adventures 3.1.0.29; the migration does not add support for other executables.
- Preserve installer options, defaults, v1.0 settings aliases, all game-data packages and fleet-cap calculations.

## Native gaps

The SDK does not yet provide every function or record used here. `NativeExtensions.h` documents the remaining records and compile-time layout checks:

- The city-foundation record's minor-nation flag and specialty.
- The active vehicle order, for which the SDK currently has a placeholder.
- The first word of a trade-partner entry. The supported executable uses a political ID here; the pinned SDK labels it as a road pointer. This word is copied, never dereferenced or reference-counted.
- An opaque receiver for the native trade-acceptance function.

Native call bindings and their checked addresses remain in `better-civ.cpp`. They are retained deliberately, rather than substituting superficially similar SDK methods without confirming their behavior.

## Regression checks

The C reference implementation and the C++ build must pass the same offline behavior/reference-executable checks. The C++ tests call the SDK adapters through the engine's `__thiscall` signatures. Reference-executable tests check enabled patches, unchanged disabled sites, rejected mismatches, and restored memory protection.

The build script also loads the real compiled DLL against an isolated test-only ModAPI shim. This verifies import resolution, initialization registration and clean rejection of the test executable. The shim in `tests/loader-bin` is never packaged or installed in the game.

For an in-game comparison, install only one Better Civ build at a time. Use the same exact settings when comparing the reference and migration builds. Keep existing saves backed up and use a new Civilization game for generation checks.

### Test A: mixed nations, hard difficulty, military player

| Setting | Value |
| --- | --- |
| Vehicle Cost | Vanilla (1x) |
| Firing Interval | Vanilla (2 s) |
| Vehicle Speed | Vanilla (1x) |
| Offensive Units | Vanilla (8) |
| AI Population | Vanilla (15) |
| AI Starting Funds | Vanilla (4,000) |
| AI Specialties | Equal Odds |
| Captured Cities | Vanilla |
| Turrets & Fleets | Vanilla |
| Rival Trade | First Rival Route |
| Trade Under Fire | Stay on Course |

- [ ] The launcher loads the DLL and `spore_log.txt` reports `[Better Civ] C++/SDK city rules active.`
- [ ] There are ten major cities in total, including the player's city.
- [ ] Economic rivals can trade with another eligible AI and buy its city.
- [ ] Traders keep their trade order under fire; damage still applies.
- [ ] Normal city editing, vehicle purchases and captures work.
- [ ] Saving and reloading works.

Equal odds do not guarantee that all three specialties appear. A map without economic rivals cannot validate rival trade.

### Test B: previous high-population configuration

Use a new game, **Hard** difficulty and a **Military** player.

| Setting | Value |
| --- | --- |
| Vehicle Cost | 50% of Vanilla |
| Firing Interval | Vanilla (2 s) |
| Vehicle Speed | Vanilla (1x) |
| Offensive Units | 16 Units |
| AI Population | 30 per Rival |
| AI Starting Funds | 8,000 per Rival |
| AI Specialties | Military Only |
| Captured Cities | Keep Specialty |
| Turrets & Fleets | 8 Slots per City |
| Rival Trade | First Rival Route |
| Trade Under Fire | Stay on Course |

- [ ] All nine rivals start as military nations.
- [ ] Starting conditions, vehicle prices and fleet limits match the C reference with these settings.
- [ ] City editing, turrets and repeated captures work through the late game.

This all-military test cannot distinguish the two capture-specialty options or exercise economic trade. Check specialty preservation in a separate mixed-nation game: use **every Test A setting**, except **Captured Cities = Keep Specialty**.
