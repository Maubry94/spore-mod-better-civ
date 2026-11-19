# Combat diagnostics test - November 18, 2026

This build extends **AI Diagnostics: On**. It does not change orders, pathfinding,
turret assignment, fleet size, attack strength, trade, gifts or notifications.
All existing gameplay options and their defaults are retained.

## Evidence from the balanced-start campaign

- The user confirmed nine major rivals with a 3/3/3 composition, and preservation
  of specialties and territory after menu and full-process reloads. A second
  newly generated planet without restarting Spore is still untested.
- The log contains nine founding events, exactly three of each purpose. Their
  order was economic, economic, economic, religious, military, military,
  religious, military, religious. The draw does not balance continents.
- The first logged AI captures were an economic purchase, a religious
  conversion and a military capture. All AI capture counters together recorded
  3 purchases, 9 conversions and 10 military captures. These are capture methods,
  not the founding specialties of mixed nations. Log timestamps include menus
  and pauses and are not measurements of siege durations.
- The conversion journal recorded eight simultaneously frozen turrets in 28
  samples, across three cities. This disproves a universal six-turret paralysis
  ceiling, but does not establish correct local assignment or adequate armies.
- The video shows apparently stationary defenders while a city is attacked;
  it does not reveal their current orders, weapon range or navigation state.
- The installed settings had Offensive Targets: Vanilla and Stalled Offensives:
  Off. Better Civ's optional cancellation and target rotation were inactive.
- The user observed a completed buyout progress bar followed by the purchase.
- The first-rival-route helper permits an eligible AI partner when both
  directional relations are at least neutral. It does not rank partners by
  friendship. Native candidate selection still matters.
- In the supported reference executable, the native purchase loop at 0xBF1310
  skips player-owned civilizations (test at 0xBF1319; skip branch at 0xBF1320).
  It calls the buyout eligibility function at 0xBD8110 and the purchase routine
  at 0xBE99A0 for eligible rival cities. Better Civ does not patch this loop.

## Unresolved reports

1. Player and AI vehicles sometimes rock in place despite expected movement.
2. Religious city-attack orders sometimes leave firing turrets unsuppressed;
   manual assignment can work. Range, competing tasks and navigation remain
   possible causes; no fixed paralysis-count limit has been demonstrated.
3. A peace notification after a rival's last city was purchased displayed a
   garbled name. There is no captured notification payload or reproduction save.
   A stale object/name is a hypothesis, not a confirmed memory-lifetime bug.
4. A portrait apparently failed to flash after an embargo refusal. A hostile
   relationship icon or dialogue available on manual opening is not by itself
   proof of an unanswered incoming call.
5. Economic partner replacement and diplomacy after expansion need separate
   investigation. This build adds no AI gift policy and does not establish the
   complete native AI gift behavior.

Do not rebalance strength based on this single campaign. Geography, income,
aircraft and retained city specialties confound the comparison.

## New journal entries

`[Better Civ Combat]` samples every five simulated seconds. Interesting units
are reported on entry, after ten seconds near the same sampled position with
the same order/aim, and at thirty-second intervals. Position drift over 0.25
world units resets the stationary observation. Sampled positions cannot exclude
motion between samples. Stationary does not mean stuck: firing, preaching or
waiting can be legitimate.

The report includes player/AI ownership, purpose, locomotion, order kind, known
order target, combat aim, position, velocity, desired speed, health, hostile
count, time since last weapon fire, weapon range and straight-line aim distance.
It also reports active cities' turret freeze counts and current combat aims.
An assigned aim does not prove firing; straight-line distance does not establish
line of sight, navigability or the engine's full range test.

Targets are resolved by comparing addresses with current live collections.
Unknown addresses are logged as `unresolved`, never dereferenced. Unknown types
include buildings or other objects not covered by the resolver. No engine pointer
is retained between samples. The existing guarded order getter is read-only.

Civilization names, pending communication IDs, portrait slots and city counts
are logged on changes; disappearance from the live list is logged once. This
does not capture the final toast text or short-lived events between samples.

The additional journal is capped at 16,384 lines per launch. Sampling, records
and line budgets are bounded; no diagnostics run while paused or outside the
Civilization stage. State resets on load without resetting the per-launch cap.
No diagnostic state is serialized, no game object is changed, and no additional
native patch is installed. Diagnostics Off disables the new observer too.

## Test setup

Use Hard difficulty, a Military player, stock walls without City Walls, and
keep other mods unchanged for comparison. Replace the previous test build with
`Better Civ SDK combat diagnostics test.sporemod`; do not install two variants
at once. Keep the previous archive for rollback. Existing saves can be used;
Balanced (3/3/3) only affects new foundations.

| Setting | Selection |
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

## Focused checklist

- [ ] Optional pending check: create two new planets in one Spore session.
      Before conquests, both have nine rivals split 3/3/3. Save the test planet.
- [ ] If vehicles rock in place, leave them alone for about 20 unpaused seconds.
      Note planet, nation, city, vehicle type and approximate clock time. Save
      before manually redirecting them, if practical; a reproduction is valuable.
- [ ] For a religious siege, note which turrets keep firing. After the initial
      observation, assign a stationary unit to one such turret and wait 20
      seconds. Note whether it moves, freezes the turret or remains stationary.
- [ ] If defenders also stay stationary, note their nation/city and time; do
      not assume the same cause as the player's units.
- [ ] If a portrait does not flash, note the nation, time and whether a pending
      call marker exists. If the garbled peace toast recurs, capture it and note
      which nation disappeared and who bought its final city.
- [ ] Save and close Spore. Before restarting, copy `spore_log.txt` to
      `spore_log-combat-diagnostics.txt`. Report any crash, hang or new slowdown.

Stop once a useful event is captured; a full campaign is not required. No
routine vehicle-price retest is requested because price code is unchanged.

Offline validation covers diagnostics gating/cadence, timer rollback, invalid
geometry, bounded record replacement and line limits, unresolved pointer safety,
dead/destroyed filtering, SDK field reads, lifecycle resets and byte-for-byte
immutability of fixture game objects. Existing behavior/patch tests are also
run. This diagnostic build still requires an in-game user test.

## November 18 follow-up: circling attackers

The user tested `Better Civ SDK portrait order test 1.sporemod` and supplied
`20261118-1945-13.7590444.mp4` (132.70 seconds, creation 19:45:17 UTC).
The video was reviewed through an overview sequence, one-second frames from
63-82 seconds, quarter-second frames from 67-71 seconds and a full frame at 71
seconds. After the pause ends around 65 seconds, several player ground vehicles
make tight repeated turns beside the wall while the attack and firing continue.
This establishes visible repeated movement, not its cause or gameplay cost.

The current log was preserved before any relaunch in the local review folder
`work/vehicle-circling-20261118/spore_log-portrait-order-test1.txt`, alongside
frame evidence, the parser and `review.json`. Log SHA-256:
`2db628c371fd8edee6d8c35d78729dc8f2ed577ba1398765f272a0f18e13c4af`.
The startup line confirms portrait order test 1. Matching file/video times
approximately puts the moving section at log time 00:21:18 onward; this is not
an exact synchronized trace.

In that log window, 71 reports for 22 player ground military vehicles all show
order 2 targeting city 1752 (Dupane). Fifty-eight combat aims are unresolved by
the current resolver; do not interpret those as invalid pointers or absent
targets. The resolver does not cover building targets. The five-second samples
and normally thirty-second reports cannot exclude intermediate order changes
or identify which visible vehicle corresponds to each ID.

Installed settings retain Vanilla speed, Vanilla Offensive Targets and Stalled
Offensives Off. The custom offensive controller also excludes player forces;
the stay-on-course hook is restricted to economic trade orders. The latest
portrait change did not introduce a movement correction. Possible firing-position
selection or local collision avoidance remains a hypothesis, not a confirmed
native bug or mod regression. No movement code, installed file, save or release
archive was changed during this review.

Before attempting a movement fix, capture finer bounded movement/aim transitions
and include live building targets if the issue can be reproduced. Do not freeze
vehicles or repeatedly reissue attack orders based on these sparse reports.
