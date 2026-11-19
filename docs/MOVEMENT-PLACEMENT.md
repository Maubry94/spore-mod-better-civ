# Traffic and fleet recovery - test 4

Artifact: `Better Civ SDK movement recovery test 4.sporemod`.
This is a playtest candidate. Offline validation is not proof that every
congestion case is fixed in the running game. Test 3 remains available.

## Changes driven by the November 19 campaign

Test 3 recovered trader 6326, but later ships 7749 and 11464 waited outside the
contact threshold. Trader 11039 completed a clearance step and then stalled
again on its original approach. Six Framboise aircraft and one Orange aircraft
were also stationary; aircraft had been excluded. Moving a player ship freed
naval traffic. Loss of Framboise's ports left economic ships without orders.
The preserved evidence is in `work/playtests/movement-recovery-test3-live-20261119`
in the parent workspace. Those observations do not establish a universal native
pathfinding cause.

### Vehicle Movement: Find Nearby Space

- Detect a stationary blocker along the next 24 units of the requested corridor,
  including a safety margin before physical contact. Wait eight simulated
  seconds of sustained immobility. Quiet-loop detection remains at 20 seconds.
- Prefer a detour whose onward corridor is clear. If that corridor remains
  blocked at arrival, allow one additional checked leg instead of immediately
  returning into the queue. At most two legs per episode; a separate later
  attempt retains the 30-second cooldown. Each leg has a six-second limit.
- A visible, genuinely idle land/sea vehicle obstructing the corridor may yield
  to a nearby free point, **including an idle player vehicle**. It must have
  native order 0, no combat target or nearby hostile units, no recent firing,
  zero requested speed and no pending path job. Explicit move, trade, attack
  and defense orders are protected. This does not move aircraft with zero speed
  requests or interrupt their delivery animations.
- Economic aircraft with supported trade orders (5 or 9) can take a local
  detour at their existing altitude. Terrain/sea and footprint clearance are
  checked at each sample; city/obstacle columns and other aircraft are avoided.
  This is separate from land/water terrain projection. Combat aircraft remain
  outside the experiment.
- Preserve cargo, trade routes and strategic orders. New manual/native orders,
  active combat aims and recent firing take priority. Unsupported requests,
  pending waypoints, unseen vehicles and a failed search cause no intervention.
- Keep bounded action logs separate from watch/summary logs, so a long trade
  observation cannot exhaust the action budget. Watch summaries use 60 seconds.

Movement still uses the validated native MoveTo method. No teleportation,
collision bypass, artificial trade delivery or speed override. Search stays
local (16 candidates, 12 segment samples, eight footprint-edge samples,
footprint radius up to eight). It cannot guarantee a route through a fully
occupied port or repair every circling unit while it is actively fighting.

### Stranded Traders: Gradual Sale

AI only. An economic ship with no usable owned port must remain idle, without
combat or nearby hostile units, for two simulated minutes. Then the normal
native sale operation refunds its owning nation according to price and health.
One sale per nation per 30 seconds, rechecked immediately before selling.
An existing port of any specialty vetoes this rule. Moving/trading ships,
player ships and military/religious ships are excluded. A temporary traffic jam
or hostile diplomacy alone never qualifies. A regained usable port cancels the
condition; a later loss starts the grace period again.

### Surplus Land Units: Make Room for Ships

A conservative AI-only trial. A stationary military/religious land unit may be
sold after three continuously eligible minutes if all of these hold:

- The nation has at least 32 vehicles, and fewer than six ships of this unit's
  specialty. It has a port capable of producing that same specialty.
- No hostile city lies on the unit's continent. The native offensive target is
  a hostile coastal city on another continent, reachable from the matching
  port through the same mapped sea.
- More than six combat land units per owned city on that continent remain;
  retain at least six even if the nation has no city on that continent.
- The candidate is idle or quietly defending (native order 0/7), without an
  active target, hostile units nearby, movement or firing in the last 30 seconds.

The same one-sale-per-nation-per-30-second limit applies across both sale rules.
Freed funds and capacity return to native AI purchasing: this does not force a
ship spawn or guarantee that every free slot is immediately used for a ship.
City production restrictions, starting balance, diplomacy and portrait rules
are unchanged. Ordinary cities still produce only their own specialty.

Both sale settings are independent and default to Vanilla. Native transaction,
refund, port eligibility and continent lookup code is checked with normalized
relocation-aware hashes. An unsupported helper disables the sale features.
No new executable patch site is added. Histories reset on load and are not
written into game saves. This also restarts the two/three-minute grace periods.

## Exact playtest setup

Install only one Better Civ build with Spore closed. The rules also work with a
healthy existing save; a new game is useful to recreate mixed fleets and port
losses. Use **Hard**, **Military** player, stock walls, the same other mods and
no City Walls.

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
| Vehicle Movement | Find Nearby Space |
| Stranded Traders | Gradual Sale |
| Surplus Land Units | Make Room for Ships |

## Focused checklist

1. **Busy port:** establish incoming trade and park some of your ships near the
   approach. Observe for 45-60 unpaused seconds without intervening. A genuinely
   idle blocker may shift aside; deliveries should resume. Record nations,
   city, time, and whether cargo actually moves after a clearance step.
2. **Air traffic:** if trade aircraft bunch up, observe for 45-60 seconds. Note
   whether waiting aircraft take a short detour and deliveries resume. Ordinary
   delivery/turn animations must continue. No occurrence means untested.
3. **Sea/land combat and manual orders:** watch a congested attack or quiet
   circle for 45-60 seconds. Active firing/preaching must continue. Give a new
   destination to one of your units during a clearance move; it must obey it.
4. **Lost ports:** when an AI loses every port, note its idle economic ships.
   After two safe unpaused minutes, watch for gradual sales. Returning ports,
   active trade or nearby enemies can postpone/cancel them. Combat ships and
   all player ships must remain outside this sale rule.
5. **Land surplus:** if a nearly full AI fleet has idle combat land units on a
   continent without hostile cities, observe for three minutes. It should keep
   a garrison, gradually free slots, and may build ships if native AI chooses.
   Log evidence is needed to distinguish a protected case from a failed rule.
6. **Save and stability:** save, quit completely and reload once. Report any
   crash, slowdown, cancelled trade or repeated side-step loop. No price retest
   is needed. Preserve `spore_log.txt` as
   **`spore_log-movement-recovery-test4.txt`** before the next launch.

One useful occurrence per scenario is enough; no need to finish the campaign.
Rollback: Vehicle Movement > Vanilla, Stranded Traders > Vanilla and Surplus
Land Units > Vanilla; or reinstall the unchanged test 3 archive.

## Offline validation

Release and Debug builds run the prior suite plus corridor geometry, airborne
terrain clearance, idle-player yielding, explicit command protection, two-leg
resume ownership, delivery-state protection, fleet grace/cooldown rules and the
native SDK fleet adapter. Land tests cover mixed-specialty ports, different sea
basins, native targets, local hostile cities and retained garrisons. Reference
checks cover relocation operands and byte tampering, native helper signatures,
DLL loading and required ModAPI exports. In-game behavior remains pending.
