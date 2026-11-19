# Offensive targeting experiment

This document records the **first** test build. Its user feedback, continuous-siege fix and current test settings are documented in [Stalled offensive test](STALLED-OFFENSIVES.md). The original rules below are retained as the comparison baseline.

This test build adds two independent installer options. Both default to off/native behavior. The previous price and portrait fixes remain included. Vehicle stats, starting conditions, trade rules, turret limits and specialty rules have no new balance changes.

## What changed

- **Offensive Targets / Rethink Failed Attacks:** track military and religious offensives separately. After 2–4 unsuccessful observed waves against the same city, exclude that city from one native target-selection pass. The threshold varies by civilization, specialty and target; this does not consume Spore's random-number generator.
- A wave must first have at least **3 assigned attackers**. It ends only after **20 simulated seconds** with no assigned attackers and no ambiguous attack orders. Religious units working on a city's cultural targets count as assigned. A reinforcing siege remains one wave, however long it lasts.
- An increase in that nation's capture/conversion progress resets the failure count. A wave that made progress is not counted as failed. Changes of target, target owner or owned cities also reset the observation. This deliberately favors leaving a potentially viable siege alone.
- Spore still checks diplomacy, geography, available vehicle types and its native priorities. Selection runs on the native schedule (normally gated by 40/50-second military/religious timers), not immediately when the twentieth quiet second passes. Forced diplomatic attack targets are left alone.
- If no alternative is selected, retain the current target and wait at least **60 simulated seconds** before another exclusion attempt. Existing vehicle orders are never canceled by this feature. Native target changes and defensive responses can still occur. There is no permanent ban on returning to a city.
- **AI Diagnostics / On:** write bounded observations to `spore_log.txt`: targets, wave sizes, failure counts, city gains/losses, the native military/religious/buyout capture counters, and budget snapshots every 30 simulated seconds per attack type. Logging alone does not filter candidates. Limit: 4,096 lines per game launch.

This is an observation-based policy, not a discovered engine "offensive finished" event. Groups of one or two units, continuously reinforced sieges and unknown suborders deliberately do not trigger rotation. Missing a very short wave between native AI updates is possible. This version does not raise troop counts or force an AI to defend/rebuild.

The status labels describe observed facts (`forces-assigned`, `other-attack-orders`, `no-city-of-type`, `no-hostile-target`, `no-assigned-force`). Wealth and expansion budget help investigate passivity; they do not prove a single cause. City gains can precede capture-counter updates. A counter delta is not a timestamped purchase event, and the initial snapshot is not logged as a capture.

## Save and lifecycle behavior

The SDK simulation strategy supplies simulated time and resets observation state on loading or changing modes. Pauses do not advance the quiet period. Runtime records contain identifiers and values, not retained game pointers. A city's SDK reference is held only across the synchronous native selection call, so it stays alive if the selector drops its reference. History is not serialized. ModAPI may include an empty strategy entry in its own subsystem record; no city or vehicle save format is extended.

Copy `spore_log.txt` **before launching Spore again**, because ModAPI recreates it at startup. On this test installation it is beside the game executable in `SporebinEP1`. Filter for `[Better Civ AI]`; `source=rethink` identifies a mod-induced target change, whereas `source=native` identifies a change made during an unfiltered native pass. `rethink=no-alternative` means the exclusion did not produce another selected city. Times are simulated milliseconds since the latest observation reset. City/civilization numbers are internal object IDs, not names.

## Exact test settings

Use stock walls (no City Walls), one Better Civ build, and a new Hard campaign. Keep the previous test campaign available separately. Start with **Military test**; the religious test follows after the military feedback. All settings are listed, including unchanged ones.

| Setting | Military test | Religious test |
| --- | --- | --- |
| Difficulty | Hard | Hard |
| Player specialty | Military | Military |
| Vehicle Cost | 50% of Vanilla | 50% of Vanilla |
| Firing Interval | Vanilla (2 s) | Vanilla (2 s) |
| Vehicle Speed | Vanilla (1x) | Vanilla (1x) |
| Offensive Units | 16 Units | 16 Units |
| AI Population | 30 per Rival | 30 per Rival |
| AI Starting Funds | 8,000 per Rival | 8,000 per Rival |
| AI Specialties | Military Only | Religious Only |
| Captured Cities | Keep Specialty | Keep Specialty |
| Turrets & Fleets | 8 Slots per City | 8 Slots per City |
| Rival Trade | First Rival Route | First Rival Route |
| Trade Under Fire | Stay on Course | Stay on Course |
| Offensive Targets | Rethink Failed Attacks | Rethink Failed Attacks |
| AI Diagnostics | On | On |

For either test:

- [ ] The log contains `[Better Civ] Optional AI observer active.` and AI snapshot times increase while the game runs.
- [ ] Ten major cities start, including the player. All nine rivals have the selected specialty. Land vehicles display and charge 500 in both purchase interfaces.
- [ ] Build/buy a few items, save, fully exit, relaunch and reload. Recorded buildings/progress remain present. Check this before a long playthrough.
- [ ] Repel repeated attacks while the attacker has another enemy reachable with its available units. Note the attacker, old target, any newly chosen target, and the approximate time. Preserve the log even if no change occurs; absence alone cannot distinguish an ongoing wave, lack of alternatives or a defect.
- [ ] A progressing siege is not redirected by the new rule; examine `source=rethink` separately from native changes, peace or recapture of a lost home city.
- [ ] Pausing does not accumulate failed waves. Reloading starts a new observation history without changing city ownership or vehicle prices.

For the religious test also observe whether vehicles disabling defenses are counted and whether conversion starts off-camera. The off-camera conversion issue is **not fixed or bypassed** by this build.

## Native integration and verification

Reference executable: October 2024 Steam Galactic Adventures, same executable gate as the migration branch. Three optional call sites are verified before any patches are written:

| Call site | Native callee | Role |
| --- | --- | --- |
| `0xBFBFE5` | `0xBFA7A0` | Military expansion planning |
| `0xBFBFEE` | `0xBFA7A0` | Religious expansion planning |
| `0xBFAA32` | `0xBD9D40` | Owner lookup solely during candidate enumeration |

The selector rejects its own cities immediately after `0xBFAA32`. For the one excluded candidate, a scoped adapter returns the selecting civilization; all actual ownership fields and other owner queries are unchanged. Other candidates continue through native hostility, transport access, cultural-target and scoring checks. If the selector produces no target, the previous live target is restored with an SDK intrusive pointer. The native forced-target path is excluded from this feature.

The dispatcher at `0xBF5DF0` uses order 2 for military attacks and order 3 for religious attacks. The latter also accepts a `cCulturalTarget` whose game-data owner is the target city (`0xBF6A60..0xBF6AD6`). The observer mirrors that checked cast. Other subtarget orders are a conservative veto.

The SDK's `cSimulatorSystem::AddStrategy` is used from post-init, registering both live callbacks and the ModAPI serialization registry. Its static support library is built from the pinned SDK checkout with MSVC v143; no replacement ModAPI runtime DLL is distributed.

Offline tests cover wave completion, reinforcing/ambiguous orders, progress, ownership changes, no-alternative fallback, military/religious adapters with the native calling convention, SDK reference balance, player/friendly/forced-target exclusions, logging-only mode, reset/pause behavior, option parsing and all four feature-toggle combinations on the reference executable. Modified call bytes must cause rejection before patching. In-game validation remains pending.

## Planned improvements

The numbered items below retain the original five improvement areas and add the diplomacy panel as a sixth; numbering is not implementation order. The next offensive follow-up is described in [Stalled offensive test](STALLED-OFFENSIVES.md).

1. **Offensive management:** the current experiment reconsiders targets after observed failed waves. Validate it in game before extending defensive priorities or rebuilding behavior.
2. **Army and turret balance:** compare military and religious forces, especially attacks against fully defended cities. [Offscreen conversion fix 2](OFFSCREEN-CONVERSION.md) passed the resumed-campaign test. Army strength and conversion rates have not been rebalanced.
3. **Economic expansion:** review buyout pace, partner selection and replacement of lost trade partners.
4. **Balanced starting specialties:** the [balanced starts test](BALANCED-STARTS.md) adds exactly three military, three religious and three economic rivals, independent of the player's specialty. Offline checks passed; in-game validation is pending.
5. **Optional AI diagnostics:** implemented in the current experiment; validate the journal against observed gameplay and refine the evidence used to explain passive nations.
6. **Diplomacy panel:** two successive stages:
   - Slot reuse is implemented in the [diplomacy panel test](DIPLOMACY-PANEL.md). The user confirmed replacement after elimination, no duplicate in the observed case, working communications and save/reload. Full-panel overflow and historical vacancies on load remain untested in that playtest; conqueror priority was not isolated from the waiting-rival fallback.
    - [Pagination fix 9](DIPLOMACY-PAGINATION.md) retains four portraits per page for up to nine eligible rivals. The user subsequently validated appearance, navigation, order after reload, elimination compaction, directional hidden-call markers, communication access without interrupting an open conversation, 500 prices and observed session stability. The middle-of-three-pages simultaneous two-direction marker case has offline coverage only.
