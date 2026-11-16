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
- The four entries in the Civilization diplomacy portrait panel, used to prevent overflow when additional major rivals make their first conquest.

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

Offline checks cover 50%, 75% and vanilla prices, repeat reads without cumulative discounts, funds just below/at/above the price, non-vehicle items, missing/wrong-type properties, Space Stage, both x86 indirect-call forms, and reference-executable patch validation. The user subsequently confirmed planner land pricing and affordability, unchanged building/turret prices, and no cumulative discount. World-view purchase text still showed the original price; sea and air checks were not reported.

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

- [x] The city vehicle menu displays 500 instead of 1,000 for the land vehicle.
- [ ] City-hover tooltips and sea/air prices display the discounted amount.
- [x] With funds between the discounted and original prices, the planner allows a land purchase and deducts only 500.
- [x] Below 500, the planner refuses the land purchase.
- [ ] At exactly 500, the planner allows the land purchase.
- [x] Buying through the city-hover shortcut still works at the reduced price.
- [ ] Air vehicle labels and purchases also match once available.
- [x] Building and turret prices are unchanged; opening the planner repeatedly does not reduce any price again.
- [ ] Saving/reloading and the remaining late-game Test B checks work.

For a subsequent price-only comparison, keep **every Test B1 setting** and change only **Vehicle Cost = 75% of Vanilla** (land example: 750). Repeat once with **Vehicle Cost = Vanilla (1x)** (land example: 1,000). Restart the game after each configuration change. These comparisons do not require starting new games.

## Crash and world-view tooltip follow-up

The 2026-11-16 20:33:44 crash report and minidump identify a null UI-window read at reference address `0xE047E5` (loaded executable base `0x7D0000`, fault `0x11D47E5`). The stack contains returns to `0xE06539`, `0xE0665B`, and `0xBF4692`. At the portrait-registration caller, the saved slot argument is **4**, outside the native valid range **0..3**.

The first conquest notification at `0xBF468D` calls a routine that scans four occupied slots, then passes index 4 to the portrait setup routine when all four are full. That routine writes outside the portrait array and tries to access a nonexistent window. This explains a crash while observing wars: an AI conquest triggers the notification independently of camera movement. It is a different fault location from the earlier city-editor crash; that earlier failure remains unresolved. Private crash dumps and user paths are not included in the repository or mod archive.

The follow-up guards registration at `0xBF468D` and restoration at `0xE065DE`. A full panel leaves the additional rival's portrait slot at -1 instead of calling the overflowing allocator. Loading a saved invalid/conflicting slot uses an available valid slot or leaves that portrait unassigned. All rivals keep their simulation and cities; this is **not** a nine-portrait UI redesign. The four-slot panel remains a visible limitation. A blocked extra portrait logs `[Better Civ] Rival portrait panel full; extra nation remains active.`

The three city-hover purchase tooltips instead use localized strings containing literal vanilla costs. Verified French game resources contain land 1,000, sea 1,500 and air 3,000. Scoped `LocalizedString::GetText` adapters at `0xE2BB36`, `0xE2BDAA`, and `0xE2C04F` scale the numeric price, preserving the game language and thousands separator. The purchase charge is unchanged. Unrecognized or oversized text falls back to the original string.

Offline Release and Debug checks cover nine registration attempts into four slots, duplicate notifications, reloads, invalid slot indices, reuse of a vacant slot, localized 50%/75%/vanilla price text, and reference-executable instruction checks. These do not establish in-game crash freedom.

### Test B2: world-view price and portrait overflow

Use a backed-up Test B save if available, **Hard** difficulty and a **Military** player. Exit the game before replacing the test build.

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

- [ ] Hovering the world-view land purchase button displays 500; buying still deducts 500.
- [ ] When unlocked, sea and air purchase buttons display and charge 750 and 1,500 respectively with stock vehicle costs.
- [ ] Land purchases in the city planner still use the 500 threshold, with fleet space available.
- [ ] Resume from before the crash and observe conquests. If five different AI nations have each captured at least one city, the game continues rather than overflowing the portrait panel.
- [ ] If the overflow guard log appears, the extra nation remains active and can still be selected/contacted through its city.
- [ ] Save and reload after that event, then check city selection, diplomacy and continued simulation.

If fewer than five different rivals have made their first conquest, absence of a crash does not validate the overflow scenario. Trader-under-fire behavior and the remaining long-game checks are still pending.
