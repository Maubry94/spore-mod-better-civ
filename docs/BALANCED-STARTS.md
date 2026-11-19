# Balanced starting specialties test

Based on offscreen conversion fix 2. Its resumed-campaign playtest confirmed
five capacity initializations followed by progress and two religious captures
while the user kept the camera away from AI sieges.

## Changes

- Add **AI Specialties / Balanced (3/3/3)**: exactly three military, three
  religious and three economic rivals among the nine starting rivals. The
  player's specialty is separate. Assignments are random draws without
  replacement from remaining quotas, not independent 33% probabilities.
- Keep **Equal Odds** as the default, with all seven previous choices available.
- With **AI Diagnostics / On**, log each balanced foundation under
  `[Better Civ Start]`, using IDs 0 military, 1 religious, 2 economic.
- Correct the diagnostics description: logging is read-only, but the separate
  offscreen conversion fix remains active even when diagnostics are off.

This is a starting-distribution feature, not a combat or economy rebalance.
All 43 existing game-data packages remain byte-identical. The archive adds one
empty-property installer marker. Turrets, fleet limits, prices, damage, firing
intervals, starting resources, trade/buyout rules and conversion rates retain
their prior behavior. The conversion repair and validated diplomacy pagination
are included. Existing saves are not rebalanced to 3/3/3.

## Implementation and verification

The existing rival foundation caller at `0xB9BDA0` is reused. Its native founder
passes false for the player flag to civilization initialization (verified at
`0xB99B74` and `0xBF82F6`). Those ranges and the native specialty converter are
checked before any patches. No new instruction patch sites are added.

A nine-ticket bag stores only the three remaining quotas. Reserve a ticket
before the synchronous native call; return it and restore the original specialty
if founding fails. Exhaustion leaves any extra caller's specialty unchanged.
The existing SDK lifecycle resets the bag on initialization, load and mode
transitions. No additional save fields, stored game pointers, or scans rewriting
existing cities are introduced. If the SDK lifecycle is unavailable, this choice
falls back to native specialties and reports the reason at startup.

Offline checks cover all 362,880 ticket paths, 256 complete foundations through
the actual native specialty converter, previous choices, null/out-of-stage calls,
failed founding, exhaustion, mode/load resets and save callbacks. Reference-byte
checks include the native non-player argument contract. The complete existing
Release/Debug suite also covers prices, fleets, trade, offensives, pagination,
preloads, conversion, relocated code validation and DLL loading. These checks do
not replace the following in-game test. No installer or game session was run by
the agent for this build.

## Exact playtest setup

Archive: **Better Civ SDK balanced start test.sporemod**.
Rollback: **Better Civ SDK offscreen conversion fix 2.sporemod**.
Close Spore for replacement and keep only one Better Civ version installed.
Start a **new Civilization game, Hard, military player**, using stock walls
(no City Walls) and the same other mods as the preceding test.

Only **AI Specialties** changes from the conversion test. Keep offensive target
rotation and the stalled-siege timeout off for this baseline. Once distribution
and resuming are confirmed, mixed-army observations can inform the next balance
pass; this build does not claim equal conquest speed for the three specialties.

| Installer setting | Choice |
| --- | --- |
| Vehicle Cost | 50% of Vanilla |
| Firing Interval | Vanilla (2 s) |
| Vehicle Speed | Vanilla (1x) |
| Offensive Units | 16 Units |
| AI Population | 30 per Rival |
| AI Starting Funds | 8,000 per Rival |
| AI Specialties | Balanced (3/3/3) |
| Captured Cities | Keep Specialty |
| Turrets & Fleets | 8 Slots per City |
| Rival Trade | First Rival Route |
| Trade Under Fire | Stay on Course |
| Offensive Targets | Vanilla |
| AI Diagnostics | On |
| Stalled Offensives | Off |

1. Before any capture, count ten large cities including yours, with exactly
   three military, three religious and three economic **rivals**. Your military
   city makes four military cities in total; it is not part of the rival quota.
   Record names/types or take a screenshot of the minimap.
2. Save, return to the main menu and reload. Nations retain their specialties
   and territory; loading does not redistribute them. Then fully exit. Before
   relaunching, copy `spore_log.txt` to `spore_log-balanced-start.txt`.
3. After relaunch, reload the same save once. To check a second fresh draw in the
   same game process, return to the menu and create a separate test planet with
   the same setup. Its nine rivals must again be 3/3/3. Preserve this session as
   `spore_log-balanced-second-start.txt` before another launch. Keep the first
   planet for the next balance observations.
4. If you continue playing the first planet, note the first military, religious
   and economic conquests, attacker/defender names, approximate times and whether
   attacks are on land or by sea. Report a stalled attack too. One random planet
   is not a controlled comparison; no conquest within a fixed time is required.

Logs are beside the game executable in `SporebinEP1`. No repeat vehicle-price
check is requested for this build. Preserve any crash or loading failure report.
