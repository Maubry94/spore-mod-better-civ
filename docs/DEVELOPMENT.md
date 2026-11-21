# Development

For build and packaging commands, see the [README](../README.md). For option
semantics, see [INSTALLERS.md](INSTALLERS.md); current evidence and remaining
checks belong in [TESTING.md](TESTING.md). Release notes live on
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
| Native integration, prices and city rules | `src/better-civ.cpp`, `src/NativeExtensions.h` | `tests/test-better-civ.cpp`, `tests/reference-code-tests.h` |
| Starting specialties | `src/BalancedSpecialties.h` | `tests/balanced-specialty-tests.h` |
| Conversion | `src/ConversionCapacity.h`, `src/ConversionDiagnostics.h` | `tests/conversion-capacity-tests.h`, `tests/conversion-tests.h` |
| Diplomacy UI | `src/Portraits.h`, `src/PortraitPages.h`, `src/PortraitPagerUI.h`, `src/PortraitOrder.h`, `src/PortraitCallAlerts.h`, `src/PortraitSpecialties.h` | `tests/portrait*-tests.h`, `tests/pagination-tests.h`, `tests/pager-widget-tests.h` |
| Offensives | `src/OffensivePolicy.h` and adapters in `src/better-civ.cpp` | `tests/offensive-tests.h` |
| Movement | `src/MovementRecovery.h`, `src/MovementPlacement.h`, their adapters, `src/MovementRequestAdapter.h` | `tests/recovery-tests.h`, `tests/placement-tests.h`, `tests/movement-fleet-fix-tests.h` |
| Fleet retirement | `src/FleetPolicy.h`, `src/FleetAdapter.h`, `src/FleetNative.h` | `tests/fleet-tests.h`, `tests/movement-fleet-fix-tests.h` |
| Trade display | `src/TradeRouteGeometry.h`, `src/TradeRouteAdapter.h`, `src/TradeRouteNative.h` | `tests/trade-route-tests.h` |
| Trade transit | `src/TradeTransit.h`, `src/TradeTransitAdapter.h`, `src/HarborDiagnosticsAdapter.h` | `tests/trade-transit-tests.h` |
| Diagnostic observation | `src/CombatDiagnostics.h`, `src/CombatObserver.h`, `src/MovementDiagnostics.h`, `src/MovementObserver.h` | `tests/combat-tests.h`, `tests/movement-tests.h` |
| Settings and profiles | `src/Settings.h`, `installer/ModInfo.dev.xml`, `tools/installers.py` | `tests/installer-preset-tests.h`, `tests/test_installers.py` |

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
collision bypass. Respect changes of orders, active aiming, recent firing and
player input. Bound candidate searches, detour legs, observation and retries.
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

Portless retirement covers military, religious and economic AI ships, with every
explicit mission protected. It does not cancel a stale mission to make a ship
eligible. Use the native refund and shared per-nation sale spacing; never sell
player units. Numerical eligibility and timing rules are in [INSTALLERS.md](INSTALLERS.md).

### Trade ribbons

Repair only `cPlanetModel::mLandTradeRoutes[].path` and its dirty flag. Check live
city membership before reading the cache's non-owning pointers. Preserve native
road data, trade agreements, orders and cargo; sea/air ribbons are unaffected.

Use the verified native land search (`0xAC68A0`, land profile `0x1565B54`), require
complete endpoints and validate terrain before publishing. An unverifiable line
stays hidden; the native renderer accepts fewer than two points. Keep the 10-second
initial grace, one-second scans, at most one search per two simulation seconds,
nine gate pairs per revision, 64 records and 4,096 sampled points. Reset transient
caches on load/mode changes. A repaired ribbon does not prove a delivery occurred.

### Trade transit

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

Focused harbor diagnostics record native messages as **last messages**, not
proven causes of a later stop. The route/partner snapshots are read-only; they
do not resolve the reported sea-ribbon mismatch by themselves.

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
