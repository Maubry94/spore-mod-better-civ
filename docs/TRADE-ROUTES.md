# Trade route fix 1

Experimental follow-up to movement recovery test 4, November 19, 2026.

## Observed issue

After Orange lost Alieuith, the player observed a land trade ribbon connecting
military Polaine to Ville Plok through a mountain and water. The video shows
the displayed line while paused. The player confirmed that no trader was
observed following that impossible line. The retained log confirms that
Polaine kept its military specialty after Orange bought it; this is not evidence
of a failed specialty-retention rule. The original cause of the bad ribbon is
still unconfirmed.

## Changes

- Inspect the land trade display cache after terrain loading has finished.
- Keep valid existing ribbons unchanged. Reject water/cliff crossings,
  substantial terrain-height errors and endpoints inside city centers.
- For a rejected ribbon, ask the game's land pathfinder for a complete path
  between land gates, then validate and project its points onto the terrain.
- Hide the rejected line while searching. If no validated path is available,
  leave the line hidden rather than draw a false crossing.
- Preserve agreements, city partners, purchases, vehicle orders and cargo.
  Sea and air ribbon caches are not edited. All movement/fleet options from
  test 4 remain unchanged.

This is a display correction, not a new trade-partner policy or a fix for
vehicle congestion. A reconstructed line does not prove that vehicles can
complete a delivery; the playtest checks that separately.

## Implementation and bounds

Only `cPlanetModel::mLandTradeRoutes[].path` and its dirty flag are written.
The native road object and its path data are not edited. Current city membership
is checked before dereferencing the display cache's non-owning city pointers.
Records retain keys and geometry, not owned city references. They reset on
load and mode transitions and add no fields to save data.

On the supported executable, native road builder `0xC414C0` calls `0xAC68A0`
with manager getter `0xB3D390`, land profile `0x1565B54`, a vector of 60-byte
waypoints, and floats 0/1. The adapter uses the same calling convention and
arguments. It accepts only success with complete endpoints. The SDK's
`cInterCityRoad::mPathData` declaration is not used. Native function/profile
bytes and exact relocation operands are checked before activation.

`0xB8D2E0` copies paths into the separate 28-byte display records; the consumer
at `0xB8CFD4` draws them. Renderer `0xB87410` returns for fewer than two points,
so an empty ribbon can be hidden. The guard includes that early return.

After a 10-second simulation grace period, scan at most once per second.
Permit one native search per two simulation seconds, at most nine gate pairs
per route revision, 64 records and 4,096 sampled points per path. Loading,
pause and non-Civilization modes do not advance this work. Repeat publication
of the same rejected source reuses the cached result. Ownership, gate or path
changes permit reevaluation. Diagnostics are limited to 512 lines per session.
The native search is synchronous, as in the road builder; in-game performance
and compatibility remain to be tested.

## Exact playtest setup

Install **Better Civ SDK trade route fix 1.sporemod** with Spore closed,
replacing the previous Better Civ build. Keep a backup of the current save.
Use the existing campaign containing the Polaine/Plok route if available;
no new campaign is required for this runtime correction. Keep other mods
unchanged and use the stock city walls, without City_Walls.

Difficulty: **Hard**. Player starting specialty: **Military**.

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

Starting settings do not redistribute or refund an existing campaign.

1. Resume the save and let 30 seconds pass unpaused. Inspect Polaine/Plok:
   the line should follow a validated land path or disappear if none is found.
   A hidden line does not mean the agreement was cancelled. With several bad
   routes, processing can take longer because searches are globally spaced.
2. Observe actual traders separately: movement, delivery and trade progress.
   Report a visual line problem separately from a stationary vehicle.
3. Check one ordinary land route and any sea/air routes: no lost agreements,
   wrong endpoints, persistent flickering or new freeze.
4. If another partner city changes ownership, check the updated line.
5. Save, exit completely and reload; verify the route and agreement again.

After exiting, copy `spore_log.txt` to `spore_log-trade-route-fix1.txt`
before relaunching. Look for `[Better Civ Routes]` activation, `invalid-ribbon`,
`ribbon-rebuilt`, `path-rejected` or `ribbon-hidden`.

Rollback: close Spore and reinstall **Better Civ SDK movement recovery test 4.sporemod**
with the same options. The previous installer and source archive are retained.

## Validation status

Release and Debug offline tests cover terrain geometry, the path-call adapter,
unchanged valid ribbons, complete/failed/partial paths, alternate gates, retry
limits, stale city pointers, pause/loading and lifecycle reset. Existing logic,
DLL loader, supported-executable and relocation/tampering checks also run.
These checks do not establish live rendering, delivery success or game stability.
In-game validation is pending. No installation or publication was performed.
