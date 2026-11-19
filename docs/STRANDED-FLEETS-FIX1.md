# Stranded fleets fix 1

November 19, 2026. Archive: **Better Civ SDK stranded fleets fix 1.sporemod**.
Based on **Better Civ SDK movement fleet fix 1.sporemod**.

The user clarified that port loss concerns a nation's whole fleet, regardless
of its original specialty or each ship's specialty. The previous implementation
only retired economic ships. This candidate extends that rule to military,
religious and economic ships and renames the visible option to **Stranded
Fleets**. Internal group/choice IDs and option packages are unchanged, retaining
compatibility with previous ModSettings selections.

## Behavior

An AI nation that still owns inland cities but no port may gradually sell ships
that remain idle, stationary and safe for two continuous simulation minutes.
Eligibility uses the individual ship's naval locomotion, not the nation's
reference specialty. This rule is independent of the separate surplus-land
option and its 32-unit threshold.

All explicit orders are protected, including attack, conversion, defense,
movement, route creation and trade delivery. The candidate does not cancel an
order or judge that an assigned mission has become stale. Ships with an aim,
nearby hostile units or weapon activity within the last 30 seconds are also
protected. Player fleets, land vehicles and aircraft are excluded.

A remaining physical port of any specialty cancels retirement. Its production
unlock is irrelevant. Regaining a port, receiving an order, moving, entering
combat, missing observations or reloading resets the eligibility timer.
After two minutes, the native sale operation refunds the owning nation.
At most one ship per nation is sold per 30 seconds, shared across specialties
and the other fleet-retirement rule. Conditions are rechecked before sale.

No additional native hooks, game tuning packages or save fields are added.
The inherited naval movement correction is unchanged: the latest log still
contains unresolved blocked boats. Coastal terrain and active-aim circling
also remain open.

## Installation and setup

Install with Spore fully closed using ModAPI Easy Installer, replacing the
previous Better Civ version. Launch with ModAPI Launcher. Keep a backup of the
save before this experiment. An existing campaign is suitable; no new campaign
is required for the runtime retirement rule.

For comparable tests: Hard difficulty, Military player start if starting a new
campaign, and stock city walls.

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
| Stranded Fleets | Gradual Sale |
| Surplus Land Units | Make Room for Ships |

## Focused test

1. When an AI nation loses its last port but still has cities and idle ships,
   note the nation and the specialties of those ships. Observe for at least
   two unpaused minutes, then allow the 30-second sale spacing for each ship.
2. Ships actually assigned to attack, convert, defend or trade must continue
   their mission; they must not disappear merely because the final port fell.
3. If the nation recaptures a port, remaining ships must stop being retired.
4. Report any new crash, loading failure or unexpected loss of a player ship.
   No repeated price test is required.
5. Before relaunching, copy SporebinEP1/spore_log.txt to
   **spore_log-stranded-fleets-fix1.txt**. Relevant startup marker:
   `[Better Civ Fleet] Stranded fleets fix 1 active`. Sales retain
   `event=sell reason=no-owned-port` with `purpose=0/1/2` for the ship specialty.

Offline Release/Debug checks cover all three specialties, mixed-fleet sale
spacing, active missions, regained ports, combat/player protections, land/air
exclusion, pause, reload and the old option IDs. In-game behavior is unverified.

Rollback: reinstall **Better Civ SDK movement fleet fix 1.sporemod** with the
same options (the old group is called Stranded Traders).
