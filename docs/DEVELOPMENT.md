# Development

The [README](../README.md) covers building and packaging. The
[installer guide](INSTALLERS.md) describes the options. Evidence from actual
campaigns and outstanding checks are in [TESTING.md](TESTING.md). Release notes live on
[GitHub Releases](https://github.com/Maubry94/spore-mod-better-civ/releases).

## Integration and safety

The DLL is Win32 C++17, built with MSVC v143 and the SDK revision pinned in
`sdk-version.txt`. The supported native reference is Steam Galactic Adventures
3.1.0.29; SDK migration does not imply compatibility with other executables.
Initialization uses `ModAPI::AddPostInitFunction`. SDK member adapters preserve
the x86 calling convention, and native patches remain scoped to verified callers.

`src/NativeExtensions.h` records SDK gaps and layout assertions.
`src/ReferenceCode.h` validates native instructions, including relocation
operands at the actual image base. Check every required native path before
writing patches; do not weaken a guard just to make a feature activate. A
startup activation message must be checked separately from a successful build.

Use live SDK containers by reference. Do not retain game-owned pointers across
updates or treat unidentified addresses as objects. Runtime observers and
policy histories reset on load and mode transitions. The game serialization
stream is unchanged; the portrait-order cache described below is separate.

## Source map

| Area | Main sources | Focused checks |
| --- | --- | --- |
| Game startup, native checks and patch registration | `src/dllmain.cpp`, `src/better-civ.cpp`, `src/NativePatches.cpp`, `src/NativeExtensions.h` | `tests/test-better-civ.cpp`, `tests/reference-code-tests.h` |
| City rules and vehicle prices | `src/CityRules.cpp`, `src/VehiclePrices.cpp` | `tests/test-better-civ.cpp` |
| Starting specialties | `src/BalancedSpecialties.h` | `tests/balanced-specialty-tests.h` |
| Conversion | `src/ConversionCapacity.h`, `src/ConversionDiagnostics.h` | `tests/conversion-capacity-tests.h`, `tests/conversion-tests.h` |
| Diplomacy UI | `src/DiplomacyRoster.cpp`, `src/Portraits.h`, `src/PortraitPages.h`, `src/PortraitPagerUI.h`, `src/PortraitOrder.h`, `src/PortraitCallAlerts.h`, `src/PortraitSpecialties.h` | `tests/portrait*-tests.h`, `tests/pagination-tests.h`, `tests/pager-widget-tests.h` |
| Offensives | `src/OffensivePolicy.h` and adapters in `src/better-civ.cpp` | `tests/offensive-tests.h` |
| Movement | `src/MovementPlacement.cpp`, `src/MovementRecovery.h`, `src/MovementRequestAdapter.h` | `tests/simulation-policy-tests.cpp`, `tests/recovery-tests.h`, `tests/placement-tests.h`, `tests/movement-fleet-fix-tests.h` |
| Fleet retirement | `src/FleetPolicy.cpp`, `src/FleetAdapter.h`, `src/FleetNative.h` | `tests/simulation-policy-tests.cpp`, `tests/fleet-tests.h`, `tests/movement-fleet-fix-tests.h` |
| Trade arrows and painted roads | `src/TradeRoutes.cpp`, `src/GroundTradeRoad.h`, `src/SeaRibbon.h`, `src/TradeTransitAdapter.h` | `tests/trade-route-tests.h`, `tests/sea-ribbon-tests.h` |
| Trade transit | `src/TradeTransit.h`, `src/TradeTransitAdapter.h`, `src/HarborDiagnosticsAdapter.h` | `tests/trade-transit-tests.h` |
| Diagnostic observation | `src/CombatDiagnostics.h`, `src/CombatObserver.h`, `src/MovementDiagnostics.h`, `src/MovementObserver.h` | `tests/combat-tests.h`, `tests/movement-tests.h` |
| Installer and runtime settings | `ModInfo.xml`, `src/Settings.cpp`, `src/SettingsChoices.inc`, `tools/package.py` | `tests/settings-tests.cpp`, `tests/test_installers.py` |

### Adding an installer option

Edit `ModInfo.xml` for the label, description and default. Keep published mod,
group and choice IDs stable so upgrades can reuse them. For a setting consumed by
the DLL, assign the choice a unique PROP instance in `src/SettingsChoices.inc`
and map it to its behavior in `src/Settings.cpp`. For a new option group, also
add the group and its default in `src/Settings.h` and `src/Settings.cpp`.
Cover the new choice in `tests/settings-tests.cpp` and `tests/test_installers.py`.
The packaging tool checks the installer choices and their package resources
against the same source catalog used to build the DLL.

The post-initialization callback resolves choices through Spore's resource
manager. Exactly one marker selects an option in each group. Missing or multiple
markers select the published default for that group and produce a startup log
entry. Vehicle speed comes directly from the selected native script. The
eight-slot package carries both a marker and its twelve wall resources.

The DLL and the native tests compile the gameplay sources listed in
`BetterCiv.Sources.props`. New rule tests should call the corresponding module.
The low-level integration fixture also exercises private callbacks against a
reference executable and substitutes engine functions for that purpose.

## Persistent design constraints

### Cities, specialties and prices

Keep native planet placement. Promoting rivals to major cities does not establish
that the coastal terrain fits the larger city; no terrain correction is included.
Captured-city specialty retention and vehicle production use the city's specialty,
not a forced uniform specialty for its owning nation.

Balanced uses a finite bag of three tickets per rival specialty. Reserve before
the synchronous native founding call and return the ticket if founding fails.
Reset the bag through the lifecycle; do not refill it merely because it is empty.
The player is excluded, and loading a campaign does not redistribute existing
cities. Random assignment does not balance specialties between continents.

Keep displayed prices, affordability checks and actual payments consistent.
Price text changes must not alter game-data packages or save layouts.

### Completed model preloads

The Civilization load-state caller at `0xCFCF0C` retains the native world accessor
and return value. It requests native cleanup only when the bounded preload list
2 is nonempty and every entry belongs to that world and is already loaded.
Never empty the list manually, mark incomplete models as loaded, or bypass native
readiness checks. Recovery demonstrates a mechanism, not the original cause of
every black screen or loading stall. See `tests/preload-tests.h`.

### Religious conversion outside the camera

The caller at `0xAE57E3` checks capacity before the normal conversion update.
Initialize four listening/converted capacity pairs to 10/2 only for an eligible
populated city with a registered foreign attacker, valid targets, zero progress,
and completely empty capacities, buckets and pending migrations. Preserve partial
initialization and existing progress. Only eight capacity integers are written;
native conversion still runs once. Do not write ownership, progress or real
populations, and do not load camera views to make the simulation advance.

### Diplomacy order and calls

Four native portrait slots display a page of up to nine eligible rivals. First
conquest notifications append rivals; elimination compacts the list while keeping
survivors' order. Defer rebinding during an open conversation. Rearm native alert
materials when bindings or pending events change, and use directional markers
for calls on other pages. The icon follows the nation's live reference city;
refreshing it must not change any city specialty.

On save, write the bounded 68-byte order record under
`%APPDATA%\Spore\BetterCiv\PortraitOrder\`, keyed by planet resource and player
civilization. It contains IDs, version and checksum, never pointers or names.
Replace a temporary sibling atomically; cache failure must not fail the game
save. Ignore invalid/foreign records and filter stale rivals when loading an
earlier save. Missing history uses the existing fallback order, then appends new
arrivals. Loading starts on page 1, subject to incoming calls. Preserve this cache
when transferring a campaign if portrait order matters.

### Offensives, movement and fleets

Measure stalled progress using a new high of the actual conquest/conversion
gauge. Destroying only a turret or recovering an earlier gauge value is not new
progress. Travel, pauses and forced diplomatic targets must not be treated as
ordinary stalled sieges. Reset transient histories on load.

Movement recovery uses checked local native move requests, not teleportation or
collision bypass. Respect changes of orders and player input. Ordinary jam recovery protects active
aiming/recent fire; deep same-nation overlap is a separate bounded case for player
and AI units, including firing units. Leave the lowest-ID unit as an anchor, require
1.5 seconds of observed overlap and reserve checked distinct escape endpoints. Bound candidate searches, detour legs, observation and retries.
The request thunk at `0xC9EA20` can hold an eligible AI escape against same-goal
refreshes for at most six simulation seconds per leg; ordinary requests retain
the original setter at `0xC43FA0`. Do not construct/copy the incomplete SDK request
record. This is not a replacement pathfinder or a fix for all active-aim circling.

Fleet policy separates physical enemy ports from their owners' production unlock.
An enemy coast can be an opportunity even before that nation can produce ships;
our own production still needs a suitable matching-specialty port. Same-sea
membership is a conservative opportunity check, not proof of a navigable approach.
Keep local idle grace across eligible overseas target changes. Do not infer
successful retirement from disappearance alone: the native AI can sell units too.

Portless retirement protects explicit combat missions. Idle military/religious
ships first seek a hostile physical port on the same connected sea after 30 seconds;
use normal attack/conversion orders, at most two attempts per ship, two minutes
apart. Regaining a port renews this budget. Missing map data defers the decision;
a known absence of an enemy port permits ordinary retirement. Idle economic
ships retain the two-minute retirement grace. Use the native refund and shared per-nation sale spacing; never sell
player units. Numerical eligibility and timing rules are in [INSTALLERS.md](INSTALLERS.md).

### Trade ribbons

Land arrows use `cPlanetModel::mLandTradeRoutes[].path`. Check live city
membership before reading borrowed pointers. For an invalid ribbon, use the
verified native land search, require complete endpoints and sample terrain
between them. The land search tries up to nine gate pairs and spaces attempts
by at least two simulation seconds.

The painted road is a separate terrain layer. Match the city's road identity to
exactly one existing terrain stroke before replacing its paint with the verified
arrow coordinates. Ambiguous or missing matches leave the terrain alone.

Better Trade checks the native sea renderer's waypoints, including the direct
port-to-port fallback it draws after an empty search. A land crossing queues a
bounded water-route search. A single display point hides the rejected segment
until a verified detour is ready. The renderer then redraws both directions.
The sea search cache resets on reload and world changes. These paths are
display data. Agreements, payments, cargo and vehicle navigation belong to
the game simulation.

### Trade transit

The first word of `cCityPartner` is a borrowed city pointer, despite the SDK's
road-pointer label. `0xBDB350` is guarded and executed by the reference tests:
read the live city's owner political ID only after validating membership, not
the pointer bits or the SDK's second word. This also serves rival-trade checks.

At the proposal/delivery order-getter callers (`0xDCA0B0`, `0xDCC7F1`), reject a
ship's trade request if its nation has no usable port, its destination is inland,
or a valid continent map proves the seas disconnected. Return a borrowed empty
order view: the native behavior tree performs its normal cancellation. No air
fallback, agreement deletion, cargo write or player-unit sale is introduced.
The proposal arrival caller (`0xDCA6B9`) also rejects a temporary detour endpoint
as arrival, until the original local request is restored.

The guarded delivery update entry at `0xDCC7C0` keeps its native stack, registers
and phase record. Its phase dispatch, arrival helper and diagnostic-string
layout have separate relocation-aware byte checks. An owned temporary detour is
not an arrival at a port: defer the delivery update until the original local
request is restored, then allow 500 simulation milliseconds for locomotion to
consume it. Loading/unloading phases cannot start a new detour. A new order,
changed local request, invalid agreement or combat vetoes intervention.

Native path-failure bit 8 otherwise clears the trade order. For a live, visible,
quiet trader with an active agreement and supported finite local point request,
retry that point with native MoveTo and defer at most one simulation second.
Do not clear native flags or modify orders, cargo, phase, payments or agreements.
Two retries exhaust an episode; a different partner/owner or more than 100 units
of net displacement starts another. An unsuccessful retry falls through to the
native failure. No resurrection of an order that is already missing. Unsupported
trade code disables delivery detours as well as this hook. State resets on load.

Focused harbor diagnostics record the **last native message** per vehicle.
Compare it with later positions, partner snapshots, orders and cargo to determine
whether a delivery resumed.

## Maintaining checks and documentation

Run the checks appropriate to a change. Native changes need the local Release and
Debug suites and compatible reference executable; GitHub-hosted checks validate
the portable installer/package logic only. No offline suite proves in-game
pathfinding, rendering, performance or universal save compatibility.

Keep current behavior here and in INSTALLERS, and evidence/open tests in TESTING.
Put release summaries on GitHub Releases. Avoid adding one document per trial.
The detailed investigation history is preserved in
[the pre-consolidation docs snapshot](https://github.com/Maubry94/spore-mod-better-civ/tree/faa99f379e38fea7cf6a65aee40c0d3c45fbf94d/docs).
Those historical instructions and validation statuses describe earlier candidates.
