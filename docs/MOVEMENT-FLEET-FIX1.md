# Movement and fleet fix 1

Experimental candidate, November 19, 2026. Archive:
**Better Civ SDK movement fleet fix 1.sporemod**.

## Evidence and scope

The paused Chria log shows ship 8322 receiving a checked clearance destination
at simulation time 619667, then losing that request 502 ms later. Five seconds
later displacement was only 0.004 units. This supports fixing premature native
request replacement; it does not prove that every naval siege has this cause.

Citron's read-only snapshot at 18:43:57 contains 35 vehicles: 25 religious land,
one military land, four religious ships and five military ships. Twenty-three
religious land units had idle orders. Balbere and overseas Rusarg connect to
the same sea. No Better Civ sale occurred in the final preserved journal.
The old journal lacks rejection reasons, so the exact historical veto remains
unknown. The new rules fix independently verified policy limitations and add
the missing evidence for this case.

The prior land-ribbon correction, price/UI behavior, offscreen conversion,
diplomacy panel, specialties and all installer choices remain included.
Game resource packages and save serialization are unchanged.

## Changes

### Local movement

- An AI unit's existing, terrain-checked escape can defer refreshes of the same
  blocked local destination for up to six simulation seconds per leg. It may
  still use at most two legs, with the existing spacing/collision checks.
- Apply retention only to AI attack, defense and supported trade orders.
  Player requests and idle blocker yielding retain their immediate precedence.
- A changed strategic order, a substantially different local destination,
  active weapon/conversion aim, recent weapon activity, arrival, timeout,
  unsupported route request or interrupted observation releases the hold.
- Keep the latest deferred destination and stop distances for safe resumption.
  No strategic order, weapon target, cargo, collision list or position is set.
- Quiet defense order 7 can now be considered by the existing obstruction
  detector. Active-aim defenders remain excluded; this does not force a boat
  to shoot a land target outside its valid firing position.

Only the verified vehicle request thunk at C9EA20 is redirected to a guarded
adapter; ordinary requests call the original C43FA0 setter unchanged. Its full
body, request builder and related native methods must pass the existing exact,
relocation-aware checks. The incomplete SDK request struct is not constructed
or copied: a checked borrowed view reads only documented native offsets.
Runtime records reset on load and mode transitions.

### Fleet retirement

- Distinguish a physical enemy port from the ability of its owner to produce
  ships. The native BDB9D0 water predicate checks both port and owner tech
  level >= 2; enemy technology is irrelevant to attacking that coast.
- Own ship production still requires a usable port of the unit's specialty.
- Prefer the native offensive target as an opportunity, but allow another
  already-hostile port on the same sea to establish demand for ships. This
  does not select a new offensive target, issue attacks or change relations.
- Measure three continuous idle minutes on the same peaceful landmass.
  Changing between eligible overseas opportunities no longer resets this
  timer. A local enemy, combat, movement, missing opportunity, ownership or
  landmass change, lost observations or reload still resets it.
- Retain the existing thresholds: at least 32 total vehicles, fewer than six
  ships of the relevant specialty, and more than six combat land vehicles
  per owned city on that landmass (minimum six). Sell at most one vehicle per
  nation per 30 seconds through the native refundable transaction.
- Player fleets are excluded. Economic orphan-ship retirement keeps its
  separate two-minute delay; a remaining physical port vetoes that retirement
  even if its owner cannot currently produce new ships.
- With AI Diagnostics enabled, a bounded review every 30 simulation seconds
  reports rejection counts, a representative idle land unit, local troops,
  matching ships, eligible idle duration and the overseas opportunity.

## Known limits

- Active-aim circling, including Anis at Chria, remains open. Avoiding movement
  overrides during genuine fighting is deliberate; this candidate does not
  claim to solve that separate case.
- Water inside port-side city walls remains open. Inspection of foundation
  and initialization code has not established a safe terrain correction.
  The major-city hook does not directly edit site geometry, but native city
  setup depends on nation state. A causal mismatch is plausible, unproven.
  No saved terrain, coast, city position or dock is altered by this candidate.
- Same-sea membership is a conservative opportunity check, not a proof that
  every shore approach is navigable. Naval construction and target selection
  still belong to the native AI; a sale does not guarantee an immediate ship.
- Offline tests prove the guarded logic and calling convention, not in-game
  pathfinding, rendering, performance, save compatibility or absence of crash.

## Exact test setup

Install with Spore fully closed using ModAPI Easy Installer, replacing the
previous Better Civ build. Keep the current campaign as the primary test;
these runtime changes do not require starting over. Back up the save first.
Use Hard difficulty, Military player start for a new test, and stock city walls.

| Setting | Choice |
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
| Vehicle Movement | Find Nearby Space |
| Stranded Traders | Gradual Sale |
| Surplus Land Units | Make Room for Ships |

## Focused checklist

1. Citron or another qualifying nation: let the game run for four to five
   unpaused minutes on a peaceful landmass with idle combat land units. Note
   the nation and whether troops are sold progressively and ships appear.
   Do not expect every land unit to disappear; a garrison is retained.
2. Naval siege: observe a queue for 30 seconds without intervening. Note
   whether a blocked ship completes its sideways escape and rejoins the
   attack, or returns immediately to the queue. Record city, nation and time.
3. Naval trade: if a queue occurs, observe for 30 seconds. Check that ships
   keep their route and can resume deliveries. Give your own ship a new move
   order and confirm it responds normally.
4. Report any new slowdown, crash or load failure. No repeated 500-price test
   is required unless an anomaly is noticed.
5. Before relaunching, copy spore_log.txt to
   spore_log-movement-fleet-fix1.txt in SporebinEP1. A save is useful to reproduce
   a stuck state, but saving is not required for the journal to contain events.

Useful markers: `[Better Civ Placement] Fix 1 active`, `event=defer-native-refresh`,
`event=step-end`, `[Better Civ Fleet] Fix 1 active`, `event=review`, `event=sell`.

Rollback: reinstall **Better Civ SDK trade route fix 1.sporemod** with the same
settings. Its archive and matching sources/symbols are preserved separately.
