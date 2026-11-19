# Development status - November 19, 2026

For the public v1.1.0-rc.1 baseline, see [the release notes](RELEASE-v1.1.0-rc.1.md)
and [the installer profiles](INSTALLERS.md). The observations below retain the
chronology of earlier tests. Stranded-fleet retirement now includes all three
ship specialties; its latest revision has offline coverage but still needs a
live playtest. Public presets do not turn the unresolved movement and terrain
reports into completed fixes.

These are the original five improvement areas plus the diplomacy panel. User
observations apply to those tests, not every possible game state.

| Area | Current status | Remaining work |
| --- | --- | --- |
| Offensive management | Optional failed-wave rotation and stalled-siege timeout implemented. User observed apparent abandonment and reduced harassment. | Isolate alternative-target selection and the two-minute retry cooldown; consider defensive priorities with evidence. |
| Army and turret balance | Fleet caps scale with turret mode. Offscreen religious conversion fix 2 passed the resumed-campaign test. | Compare military/religious attacks with matching settings and geography before adjusting strength or reinforcements. |
| Land/sea fleet allocation | Anis's cleanup around 17:52 and the Pertide/Dollebel and Hubin/Rusarg observations remain successful cases, likely native retirement rather than the optional rule. Later, Citron retained 25 religious ground vehicles, 23 without orders, with only four religious ships and 35 vehicles overall; the read-only live snapshot confirms the user's surplus report. The installed `LandRebalance` option is enabled and no Better Civ sale event appears in the preserved log. | Reopened: religious units are supported, and the 32-total/fewer-than-six-ships thresholds do not explain this snapshot. Citron's religious port Balbere and target Rusarg map to the same sea, on different land regions. Inspect the remaining eligibility gates and three-minute history before changing the policy; the exact veto has not been identified. Evidence: `work/playtests/trade-route-fix1-live-20261119/citron-live-fleet.json` and `citron-land-surplus-assessment.json` in the parent workspace. |
| Economic expansion | AI-to-AI routes and buyouts observed; stay-on-course option implemented. | Measure buyout pace, partner selection and replacement after a route is lost. |
| Stranded economic ships | At 15:17 on November 19, the user reported Framboise had lost every port to Orange. The preserved log shows economic ships 7749 and 11039 idle, with no target and zero desired speed. The inspected native sale operation credits the owning nation without checking for a port; the previously inspected automatic retirement routine selects land vehicles only. | Test 4 adds an independent option: two safe idle minutes without any usable owned port, native refund, and at most one sale per nation per 30 seconds. Trade orders, player fleets and nearby combat are protected. In-game validation pending. |
| Starting specialties | Previous seven choices retained. Balanced (3/3/3) first-game composition and save/reload preservation passed the user's test. | Second new planet within the same process remains untested. No continent-balancing rule. |
| Coastal city terrain | On November 19, the user supplied four screenshots from Camenia showing the shoreline/water intersecting city ground along the inside of the port-side walls. The foundation hook promotes rivals to major nations while preserving the supplied site geometry; it adds no coastal-fit check. | Investigate the native site selection, physical city size and terrain stamp together. A mismatch caused by major-city promotion is plausible, not established. Check collision/navigation separately: the screenshots do not prove a connection to blocked ships. No terrain or save modification has been made. Evidence: `work/playtests/coastal-city-overlap-20261119` in the parent workspace. |
| AI diagnostics | Ground congestion was recorded during the Sabilde attack and near Delia. Recovery test 1 also captured Lavande's naval attack on Horach and persistent congestion after capture: nine ships received two same-goal retries each without meaningful movement. | Same-goal recovery did not resolve this naval case. Investigate local placement and occupied destinations; no specific native routine or mod regression is proven. The garbled elimination notification remains separate. |
| Diplomacy panel | Portrait order test 1 passed the user's arrival-order, elimination compaction, observed specialty icons, call flashing/sound/marker and no-crash checks. | Save/reload order with this candidate and a live reference-specialty transition remain unreported. Three-page simultaneous alerts have offline coverage only. |

The "You fought our enemy" bonus and its accumulation were confirmed in the
user's campaign, including defense of a spice mine. The earlier suspicion that
Better Civ removed the bonus was not confirmed.

Current candidate: [stranded fleets fix 1](STRANDED-FLEETS-FIX1.md). Following
the user's clarification, portless retirement now covers idle military,
religious and economic AI ships. All assigned missions remain protected,
including quiet defense; losing a port alone does not discard an active fleet.
The visible installer group is Stranded Fleets; old setting IDs are retained.
The 18-minute movement/fleet fix 1 log ends normally without an explicit
critical error, but its four clearance attempts do not confirm recovery, and
no automatic sale occurred. Movement and sale effectiveness remain open.

The inherited [movement and fleet fix 1](MOVEMENT-FLEET-FIX1.md) is based on
trade route fix 1. It retains bounded AI clearance against same-goal native
refreshes, admits quiet defense orders, separates enemy port geography from
production unlock, and preserves local idle grace across eligible overseas
opportunities. Fleet rejection reviews are now available. These changes pass
offline checks; the exact historical Citron veto and in-game effectiveness
remain unconfirmed. The coastal terrain issue and active-aim circling remain
open. No terrain mutation is included.

The paused Polaine/Plok video shows a false land ribbon through
mountain and water; no vehicle following that line was observed. The candidate
repairs only the display path or hides it if no validated land path is found.
Live rendering and trade delivery checks remain pending.

Test 4 adds approach-corridor detection, idle blocker yielding, bounded second
detour legs and economic-aircraft clearance. Native orders and cargo are retained.
Both fleet sale experiments are independent options, defaulting to Vanilla.
Test 2 executed no clearance steps in the supplied November 19 logs. Its terrain
readiness gate was inverted relative to the supported native implementation.
The revised gate is checked against the actual executable, including execution
of its ready predicate with finished and active load-job states.
Test 3 includes supported trade orders, cross-nation blockers and quiet-loop
detection. Eight tracked traders left their congested positions after the user's
menu/reload workaround; that does not identify the transient native state that
was reset. Test 3 does not invoke a partial reload or clear collision lists.
Native local movement is used to request a checked clearance point. Test 3 showed both successful recovery and unresolved queues;
Test 4 effectiveness and stability remain pending.
Balance settings and native audio are unchanged. The latest user's observations confirm
arrival order and compaction. Anis/Lavande reordering was the previous fixed-ID policy.
Lavande's reference specialty legitimately changed after losing its religious
city; the old journal does not prove the displayed icon's refresh time.
Combat diagnostics remain available for stationary/circling vehicles and the
separate garbled peace notification. Eight-turret paralysis was observed in the log;
a universal six-turret paralysis ceiling is not the explanation.
