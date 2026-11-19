# Diplomacy panel test

Experimental follow-up to the stalled-offensives build, 2026-11-17. Archive: **Better Civ SDK diplomacy panel test.sporemod**. Offline validation passed. The subsequent user playtest confirmed slot replacement after elimination, interactions, no duplicate in the observed case and save/reload. The full-panel and historical-vacancy cases were not tested, and conqueror priority was not isolated. The checklist below records the original test scope; the separate [pagination prototype](DIPLOMACY-PAGINATION.md) is a newer revision requiring its own in-game checks.

## Exact changes

- Free a portrait slot after its nation disappears. The native removal routine hides the portrait and releases its reference, but leaves the slot marked occupied; the new UI update adapter clears that flag after the reference is gone.
- Fill vacant slots with living, non-player, non-neutral rivals that have already triggered their first city-capture notification. Nations do not all appear at the beginning of a game.
- Prefer the eliminated nation's conqueror if the mod observed its last remaining city and can identify that city's new owner. The conqueror must be eligible and not already displayed. Otherwise select another waiting eligible rival by stable object ID. Existing live portraits keep their positions.
- Fill historical vacancies after loading a save. Conqueror history resets on load, so historical vacancies use the waiting-rival fallback.
- Prevent duplicate portraits during restore and repair duplicate entries found during UI updates. Use native registration/removal helpers to maintain the UI's lookup map and references. The existing four-slot overflow guard remains active.
- With **AI Diagnostics = On**, log changed slot assignments as `[Better Civ UI] filled-slot=... civ=... priority=conqueror|waiting-rival`. Startup logs `[Better Civ] Diplomacy panel slot reuse active.` when the required lifecycle registration succeeds. Slot numbers in the log are 0 through 3.

The panel still displays at most four rivals. Scrolling/pagination is not implemented. If the conqueror is the player or already has a portrait, another eligible waiting rival takes the vacancy; if none exists, the slot stays empty. Simultaneous eliminations or a capture before the first observation may prevent identification of the conqueror and use the fallback.

No installer choices or defaults changed. Vehicle prices, firing interval, speed, starting conditions, turret/fleet limits, trade, diplomacy scores, specialty rules and offensive timeout rules are unchanged. All 43 game-data packages and `ModInfo.xml` must be byte-identical to the previous stalled-offensives archive; only `BetterCiv.dll` changes.

## Test setup

Fully exit Spore before replacing the test build. Keep a backup of the working save and use only one Better Civ installation. Use **Hard** difficulty, a **Military** player, and stock city walls without **City_Walls**. A healthy existing test campaign can be resumed; a new campaign is not required for slot reuse. Generation settings only apply when creating a new game.

| Installer setting | Exact value |
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
| Offensive Targets | Rethink Failed Attacks |
| AI Diagnostics | On |
| Stalled Offensives | 3 Minutes |

## In-game checklist

- [ ] The startup log reports `Diplomacy panel slot reuse active.` The campaign loads and land vehicle prices still show 500 in the planner and hover tooltip.
- [ ] If the loaded save has an empty portrait slot and an eligible waiting rival, the rival appears. No historical conqueror priority is expected after reloading.
- [ ] An unrepresented eligible rival eliminates a displayed nation. Its portrait takes the vacant slot when the final-city capture was observed by the mod.
- [ ] If an already displayed nation or the player eliminates a displayed rival, no duplicate/player portrait appears. Another eligible waiting rival takes the slot, or it stays empty.
- [ ] With all four slots full, another rival's first conquest leaves the existing portraits intact and does not crash. That rival can appear after a later vacancy.
- [ ] Hovering and selecting the new portrait shows the correct nation, color and relations, and opens the correct communication panel. Existing portraits still work.
- [ ] Saving, fully exiting Spore and reloading preserves progress and produces valid portraits without duplicates or a loading stall.

Before restarting again, preserve `spore_log.txt` as `spore_log-portrait-test.txt`. Note the names of the eliminated nation and conqueror. Cases that did not occur remain untested, even if no crash occurred.

Continue observing the previous offensive tests when possible: actual deployment to a new target, the two-minute exclusion with no alternative, and whether turret destruction alone leaves the timeout running. The previous playtest confirmed one logged abandonment and a subsequent target change, but did not establish every visible combat outcome.

## Implementation and offline evidence

The supported executable is the same October 2024 Steam Galactic Adventures reference used by the migration branch. The narrow UI-update call at `0xCFE3FC` calls `0xE06200`; it is checked before patching. The adapter reconciles slots before the original update and does not intercept a capture transaction.

| Native helper | Address | Checked body length | FNV-1a hash |
| --- | --- | --- | --- |
| Hide/release portrait | `0xE04930` | 235 | `0x95c8db06` |
| Remove map entry/portrait | `0xE052B0` | 236 | `0xc60c21f9` |
| Restore portrait | `0xE064D0` | 112 | `0x230f7833` |
| Register portrait | `0xE06610` | 108 | `0x1443577b` |

Helper bodies and call sites must match before any patch is written. The native civilization-to-slot map is accessed through those helpers, not reconstructed by the mod. The pinned SDK supplies the civilization list and scoped intrusive references. Only identity values are kept across UI updates; they are never dereferenced and reset on load/mode/lifecycle changes. Reentrant UI updates skip reconciliation.

Release and Debug tests cover full panels, stale occupied flags, conqueror priority over another waiting rival, player/already displayed conquerors, simultaneous vacancies, no eligible replacement, repeated restore calls, duplicate portraits, stale lookup repair, reference ownership, lifecycle reset and reentrant updates. Changing any byte in the checked helper bodies or the new call site rejects the reference executable before patching. Existing price, AI, settings and DLL-load checks also pass. These tests use controlled SDK/native-helper fixtures, not an actual running game UI.

Build-only project changes anchor source paths to the project directory. The local pinned SDK build likewise used absolute project-relative source/include paths to accommodate this build environment; SDK logic was not changed. SDK dependencies, private saves and game executables are not distributed in the test archive.
