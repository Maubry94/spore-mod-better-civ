# Stalled offensive test

This follow-up addresses continuously reinforced sieges that never completed a wave in the [first targeting experiment](AI-OFFENSIVES.md). It is experimental and requires in-game validation. The previous price, save/lifecycle and portrait safeguards remain included.

## Evidence from the first test

The user confirmed land price display at 500, construction/save/full-exit/reload, continuation of progressing attacks, and no new crash or loading stall. Target rotation was not confirmed. The supplied video and console show several nations attacking the same city; simultaneous attacks are not themselves considered a defect.

A bounded log snapshot contains 26 wave starts, only one failed wave, and no `rethink` event. One rival kept at least 15 units assigned to city 439 across samples spanning about 9 minutes 40 seconds. This exposes a limitation in the old policy: continuous reinforcement prevents the 20-second empty-wave condition. The wealth field is not a complete balance sheet; expansion budgets remained positive even when reported wealth was zero.

The previous observer also replaced the gauge with zero when another attacker held its credit ID. The video shows that credit changing. A later credit change could therefore manufacture apparent progress. The snapshot cannot establish how often this affected the rule. This build reads the shared city gauge and tracks its highest observed value instead.

## Exact changes

- **Stalled Offensives:** a separate installer setting, default **Off**, with **2 Minutes**, **3 Minutes** and **4 Minutes** choices. It works with either value of Offensive Targets and with diagnostics on or off.
- The clock starts after a group has at least **3 assigned attackers** and at least one is near the target. Arrival is approximated using the city's footprint radius plus the vehicle's actual weapon range. Missing or invalid geometry does not start the clock. This is an arrival heuristic, not a discovered native siege-start event.
- Only a **new high in the city's capture/conversion gauge** counts as net progress and restarts the clock. The observer uses the shared military or religious gauge even if another nation is credited. A change of credited attacker, more reinforcements or turret losses alone grants no extra time. A regression and recovery to the same value (20 → 0 → 20) does not restart the clock; reaching 21 does. No building/turret destruction counter is added.
- When the chosen time expires, cancel only the nation's recognized military/religious attack orders for that city, clear that expansion target, and exclude the city for **120 simulated seconds** for that attack type. Religious orders directed at city-owned cultural targets are included. Other-city, economic, unknown, dead-unit and player orders are left alone.
- Spore chooses any alternative through its normal hostility, access, scoring and planning schedule. If there is no alternative, leave the expansion target empty during the exclusion, rather than immediately restoring the stalled city. Units are not teleported or explicitly ordered home; survivors may defend themselves locally. This does not guarantee an immediate visible retreat.
- The rule treats AI attacks on the player and on other AI equally. Forced diplomatic attack assignments are still excluded. Military and religious planners have separate timers and exclusions. Captured targets cease to match their old owner's exclusion.
- The existing **Rethink Failed Attacks** setting still handles 2–4 completed failed waves. Its progress test now also uses shared gauge highs. Continuous attacks are handled by the new setting. The two mechanisms are independent.
- Diagnostics add `nearby`, `engaged`, `net-peak`, `no-advance-ms`, `stalled-abandon`, canceled-order counts and exclusion end times. `source=stalled` identifies the timeout intervention. The existing 4,096-line limit remains.

All timers use simulated time. Pauses do not advance them. Loading or changing modes clears the observation history and exclusions; none is serialized into city/vehicle data. The original 11 balance settings, player money, vehicle prices/stats, starting populations/funds, turret limits and fleet capacity have no additional changes in this follow-up. No cap on simultaneous attackers is introduced.

## Exact next test

Use **Hard**, a **Military** player, stock walls (no City Walls), and only one Better Civ build. Fully exit Spore before replacing the installed build. The current healthy campaign may be resumed with a backup; a new campaign is not required for these runtime rules. Reloading starts fresh timers, so do not count time from before the load. Keep the earlier test archive available for comparison.

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
| Offensive Targets | Rethink Failed Attacks |
| AI Diagnostics | On |
| Stalled Offensives | 3 Minutes |

- [ ] Land vehicles still display and charge 500. Construct/buy, save, fully exit and reload; progress is preserved.
- [ ] An ongoing attack without a new gauge high eventually logs `stalled-abandon`. Check `engaged=1` and `no-advance-ms` before estimating the three minutes; travel time and pause time do not count. One native AI update of delay is possible.
- [ ] Reinforcements to that target stop after abandonment. Existing survivors may still fight locally. When another enemy is reachable, note whether the nation eventually sends forces elsewhere.
- [ ] With no alternative, the attacker does not resume that same expansion target during its two-minute exclusion. Returning after expiry is allowed.
- [ ] A new gauge high restarts the delay. A turret lost without gauge advance does not. Multiple nations may still attack simultaneously, each with its own clock.
- [ ] No new crash, loading stall or purchase/diplomacy regression occurs. Preserve `spore_log.txt` before the next launch, including when behavior is inconclusive.

Religious testing follows this military pass. Off-camera religious conversion, troop strength, trade pace and diplomacy-panel expansion are not changed here.

### Diplomacy bonus — pending in-game verification

Use the exact settings and difficulty listed above. A positive "You fought our enemy" event was found in the inspected campaign, but its visible accumulation and the explanation for the small bonus remain unconfirmed in game. The native local-defense exclusion is a hypothesis to test for this campaign, not a confirmed cause.

- [ ] Before and after fighting, record all surviving rivals' opinions of the player and of each other. For each potential beneficiary, verify that it is at least neutral toward the player and hostile (red) toward the attacked nation. Record the "You fought our enemy" modifier separately from the total relationship score.
- [ ] Compare attacks on the same enemy's vehicles near a player-owned city and outside player-controlled territory. Record the location, nations involved and bonus before/after each encounter. Recheck diplomatic eligibility between encounters; changed relations make the comparison inconclusive. Determine whether local defense actually explains the missing or smaller increases.
- [ ] Check whether repeated eligible encounters increase the displayed bonus toward its +70 cap, and whether the accumulated bonus survives saving, fully exiting and reloading. Do not expect an immediate +70 or assume every shot or destroyed vehicle counts. Record unchanged values and inconclusive results too.

## Native integration and offline checks

The existing three guarded planner/filter call sites are reused. The native order setter is `0xCAC1A0`, a `__thiscall` taking target, order kind and flags, ending in `ret 0x0c`. Cancellation uses `(nullptr, 0, 0)`, as observed in the native dispatcher. Its complete 212-byte body is checked against reference FNV-1a `0x6dc24d07` before any patches are written when the option is enabled. This is a compatibility check, not a security hash. A mismatch disables rule installation. No new function-entry detour is added.

The vehicle list is copied with SDK intrusive references for the synchronous cancellation, protecting objects if a behavior callback modifies the original list. Targets retained for selection/cancellation are also scoped SDK references. Records across updates contain only identifiers, values and times. Candidate exclusion only adapts the narrow native owner lookup; it never changes real ownership.

Offline checks cover all eight combinations of rotation/diagnostics/timeout; all three timeout values; long travel, continuous reinforcements, gauge regression/recovery, credited-attacker changes, real progress, pauses/resets, target-owner changes, cooldown expiry, absence of alternatives, military/religious matching, selective cancellation, player/forced-target exclusions and SDK reference balance. Every byte of the native order setter is individually corrupted in the fixture to verify rejection before patching. Release/Debug and loader-fixture checks do not establish in-game stability or prove a visible retreat; those remain playtest tasks.
