# C++ / ModAPI SDK migration

The initial build was a behavior-preserving migration from the C implementation at `84a2baf`. The follow-up price fix below corrects an interface discrepancy reported during testing. It does not change balance or fix the previously reported city-planner crash.

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

The build script also loads the real compiled DLL against an isolated test-only ModAPI shim. This verifies import resolution, initialization registration and clean rejection of the test executable. The shim in `tests/mLibs` also supplies scalar-property SDK calls for offline tests. It is never packaged or installed in the game.

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

- [x] The launcher loads the DLL and `spore_log.txt` reports `[Better Civ] C++/SDK city rules active.`
- [x] There are ten major cities in total, including the player's city.
- [x] Economic rivals can trade with another eligible AI and buy its city.
- [ ] Traders keep their trade order under fire; damage still applies.
- [x] Normal city editing, vehicle purchases and captures work.
- [x] Saving and reloading works.

Results reported by the user on 2026-11-16 for the initial migration build. Trade under fire was not verified.

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

- [x] All nine rivals start as military nations.
- [ ] Starting conditions, vehicle prices and fleet limits match the C reference with these settings.
- [ ] City editing, turrets and repeated captures work through the late game.

This all-military test cannot distinguish the two capture-specialty options or exercise economic trade. Check specialty preservation in a separate mixed-nation game: use **every Test A setting**, except **Captured Cities = Keep Specialty**.

The user confirmed the 50% charge but found that the planner still displayed and required the full price. Hover purchases worked at the reduced amount, with an incorrect label. Other starting conditions/fleet limits and the final late-game checklist item were not confirmed in that report.

## Vehicle price follow-up

The simulation reads a float vehicle cost at `0xBF2342`. The planner and tooltips instead read the integer `paletteItemCost` (`0x02166464`). The initial migration and C reference only adjusted the simulation path.

The fix scales the UI cost at seven narrowly checked read sites. It requires `paletteItemType` (`0x02196AD5`) to identify `PaletteItem::kItemCityVehicle`, and only runs in Civilization with a discounted setting. It leaves the source property intact and does not apply a second multiplier to the amount charged. Buildings, turrets, other item types, vanilla cost mode and Space Stage retain their original values.

| Read sites in the supported Steam executable | Purpose |
| --- | --- |
| `0xD08195`, `0xD088D6` | Planner item availability / affordability |
| `0xD08333` | Planner item cost |
| `0xE4950C` | Simulation tooltip cost |
| `0x5F1D08`, `0x5ED8B1` | Palette cost presentation |
| `0x59E840` | Shared palette affordability |

Five sites replace the virtual-function target load with an adapter address in the same register, leaving the existing call and arguments intact. Two replace a direct `Property::GetInt32` call. The adapters use SDK `PropertyList` and `Property` operations. Expected instruction bytes are checked before installing any rule; a conflicting enabled site rejects the entire installation.

Offline checks cover 50%, 75% and vanilla prices, repeat reads without cumulative discounts, funds just below/at/above the price, non-vehicle items, missing/wrong-type properties, Space Stage, both x86 indirect-call forms, and reference-executable patch validation. In-game confirmation of this fix remains pending.

### Test B1: discounted prices

Use **Hard** difficulty and a **Military** player. An existing backed-up Test B save is suitable; this fix does not need city regeneration. Exit the game before replacing the test build.

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

Keep room in the fleet and use an unlocked vehicle type with the required port/air access when testing affordability.

- [ ] The city vehicle menu and city-hover tooltips display the discounted land and sea prices (500 instead of 1,000 for the reported land vehicle).
- [ ] With funds between the discounted and original prices, the planner allows a purchase and deducts only the discounted price.
- [ ] Below the discounted price, the planner refuses the purchase; at the exact price, it allows it.
- [ ] Buying through the city-hover shortcut still works at the same price.
- [ ] Air vehicle labels and purchases also match once available.
- [ ] Building and turret prices are unchanged; opening the planner repeatedly does not reduce any price again.
- [ ] Saving/reloading and the remaining late-game Test B checks work.

For a subsequent price-only comparison, keep **every Test B1 setting** and change only **Vehicle Cost = 75% of Vanilla** (land example: 750). Repeat once with **Vehicle Cost = Vanilla (1x)** (land example: 1,000). Restart the game after each configuration change. These comparisons do not require starting new games.
