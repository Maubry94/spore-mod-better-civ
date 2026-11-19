# Movement diagnostics test 1

Use `Better Civ SDK movement diagnostics test 1.sporemod`. This candidate is
based on portrait order test 1, whose archive remains the rollback version.
The user confirmed chronological admission, elimination compaction, observed
specialty icons, call alerts and no crash in that test. Save/reload order and a
live reference-specialty transition remain unreported for that specific build.

## Exact changes

- Recognize building order/combat targets from each live city's SDK collection,
  in addition to cities, turrets, cultural targets and vehicles. Unknown
  addresses are compared only and remain `unresolved`; they are never followed.
- When AI Diagnostics is On, sample visible combat every 250 simulated ms.
  Include player and rival attackers plus idle defenders near an attacked city.
  Observe at most 128 eligible visible units per callback; overflow is reported.
- Report each unit normally every two seconds, or after a sampled order/aim
  change with a minimum 500 ms between reports. Record positions, velocity,
  angular velocity, order position, desired speed, neighbor count, hostiles,
  firing timer, weapon range, identified aim and straight-line aim distance.
- Summarize sampled path length, net displacement, velocity-heading changes and
  order/aim transitions over each report window. These are measurements, not
  automatic stuck/circling diagnoses. The path is a sampled lower bound; short
  transitions between samples can still be missed. Order position need not be
  the native steering destination, and neighbor count does not prove collision.
- Report nearby city capture/conversion state and live turret aims, health and
  freeze counts every two seconds, subject to the shared output budget. A target
  or distance alone does not prove firing, reachability or line of sight.
- Bound the added journal to 32 lines/second with a 64-line burst and 32,768
  data lines per launch. Rate-limited reports preserve accumulated unit data;
  reported suppression counts expose missing detail. Unit reporting order
  rotates. Log caps survive save/load; copying the log and restarting resets
  them. The older combat/conversion journals have their own existing budgets.
- Reset trajectory windows after a sample gap over one simulated second, time
  rollback or lifecycle reset. No samples while paused, outside Civilization,
  with diagnostics disabled or before the simulation is ready. Loading writes
  no diagnostic data into the save. Only scalar observation state is retained.
- Clarify the English AI Diagnostics description. All 14 choices and defaults,
  44 game packages, movement rules, strengths, prices and UI behavior are retained.

This build does not change vehicle orders, stop vehicles, alter targeting or
fix movement. It adds no native patch site. Visibility is observed, never forced.
No game installation or automated in-game validation was performed.

## Setup

Close Spore and replace the previous Better Civ variant with this archive.
Use one Better Civ installation. Keep stock walls (no City Walls) and other
mods unchanged from the previous comparison. Use Hard difficulty. A healthy
existing save with enemies is sufficient; no new foundations are needed. If
the previous campaign is finished, start a new Military Civilization test.

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

## Focused test

No routine price or full-campaign regression is requested.

- [ ] Observe one city battle with the camera nearby, unpaused, for 30 seconds
      without issuing replacement orders. Player or AI vehicles are both useful.
      Note the planet, city, nations and approximate real-world time. Report
      whether units circle, stay stationary, fire or suppress turrets.
- [ ] If abnormal movement occurs, save that state if practical before trying
      another order. Preserve that save if it can reproduce the event. A short
      recording helps match visible units with the diagnostic interval.
- [ ] Optional religious comparison only if available: manually send one
      affected religious vehicle to a turret that is still shooting. Watch for
      20 seconds and report whether it moves and freezes the turret. Do not
      assume that all firing turrets must be reachable by all attackers.
- [ ] Report any new slowdown, crash or loading issue. No need to manufacture a
      second combat if the first already captures useful movement.
- [ ] Close Spore and copy `spore_log.txt` beside `SporeApp.exe` to
      `spore_log-movement-test1.txt` before launching the game again. The new
      startup line is `[Better Civ Movement] Diagnostic test 1 active`.

If the phenomenon does not recur, report that and keep the log. Do not treat a
successful battle as proof that the earlier movement issue is fixed.

## Validation

Release and Debug checks exercise building pointer resolution, destroyed and
foreign object exclusion, visible/idle-defender gating, a straight trajectory
versus a closed loop, sampled aim/order transitions, rate/line caps, invalid
geometry, time gaps/rollback and lifecycle resets. The SDK observer is executed
on fixtures and checked for byte-for-byte immutability of civilizations, cities,
vehicles, buildings and turrets. Existing gameplay, loader, UI, conversion,
reference executable and save-lifecycle tests remain required.

## User session reviewed November 18

The supplied `spore_log-movement-test1.txt` was copied to the local
`work/movement-review-20261118` evidence folder before any relaunch. SHA-256:
`209fcb7996c9ab1de16a3d0499d0288488d8ec2563339839a7b4f1f002e962dd`.
Its startup confirms this candidate and the installed options match the table.
An approximate local start at 21:14:40 is inferred from the final shutdown
timestamp and original file modification time. Citron's capture of Orange's
Aberry aligns with the user's report at approximately 21:22:45.

During the player's attack on Sabilde (city 1496), seven distinct units have
reports combining positive desired speed, less than one world unit net movement
over at least two simulated seconds, and more than eight seconds since weapon
fire. For example unit 6373 moves only 0.25 units over a 21.639-second sampled
window, with desired speed 13.70 and a 20.262-second firing timer. Units 6314 and
6373 have practically overlapping sampled positions around 21:31:06-07.
Subsequent records show movement and identified targets again; unit 6314 targets
building 1509 with a reset firing timer around 21:31:36. Exact on-screen identity
and causation by another particular unit cannot be established from these data.

Near Delia (445), units including 11008 and 11034 show order 0, no combat aim,
zero hostiles and zero or very small displacement despite desired speed 13.70.
Some local neighbor counts exceed 40; this includes spatial objects and is not
a count of colliding vehicles. The user's observation that manually moving one
vehicle released the others strengthens the local placement/congestion
hypothesis. Neither a specific engine defect nor mod causality is established.

The detailed movement cap was reached at log 00:31:08, approximately 21:45:48.
The 22:25 naval event has only the older five-second samples/thirty-second
reports, which do not establish the same blockage. Combat and conversion logs
also hit their separate caps around 22:44. Later absence is not evidence of
normal behavior. Future targeted capture should not spend its budget on a full
campaign; rate suppression also lengthened some unit reporting windows.

Starting around 21:49:55, multiple player units with order 0 have combat aims at
units belonging to Foret, including economic boat 2030 and military boat 5846.
This supports automatic engagement without an explicit attack order. The journal
does not record individual damage or relationship-event deltas, so it cannot
attribute the reported -31 penalty, establish which side initiated hostilities,
or reconstruct the full diplomacy sequence.

The AI-to-AI trade-acceptance override explicitly returns the native result if
either party is the player. It therefore does not directly force Framboise's
reported offer to the player at orange relations. Relationship scores at the
offer time are not logged, so the reason for that offer remains unresolved.

The city-interior vehicle screenshot and the blue patch at the port/ground join
are kept as separate visual reports. A still frame does not establish the
traversal route, terrain/water cause or a capture-related regression. No movement,
diplomacy, terrain, installed files or saves were changed during this review.
