# Religious conversion diagnostic 2

Based on diplomacy pagination fix 9, whose five directional-call, communication,
save/reload and price checks passed user playtesting. Diagnostic 2 corrects the starting-specialty inversion found during diagnostic 1
playtesting and retains its read-only conversion observations. It does not repair
conversion, strengthen religious armies,
change visibility, advance the gauge, or issue attack orders.

## Starting-specialty fix

The user's installer settings correctly selected Religious Only and the installed
DLL matched diagnostic 1, but the new rivals were economic. City foundation
records use military/economic/religious IDs 0/1/2. Existing cities and vehicles
use military/religious/economic IDs 0/1/2. The previous foundation hook wrote a
vehicle-purpose ID directly into the foundation record. The native founder at
0xB99C16 calls 0xBEFA70 to convert that record before setting the city's specialty.

The hook now translates only at that boundary. Religious Only, Economic Only,
No Religious and No Economic now match their labels. Military Only, Equal Odds
and the two-specialty distribution of No Military are unchanged. Installer IDs,
defaults, balance packages, RNG draw counts, capture policy and saved cities are
unchanged. **Start a new game:** existing economic rivals are not retroactively
converted. Diagnostic 1's reported game cannot test religious attacks.

The native converter and the founder's use of it are checked before installing
patches. Regression tests exercise all seven choices using the actual converter
copied from the user's compatible reference executable, rather than a mock with
the same numbering assumption. No game executable is distributed.

## Conversion observations

With **AI Diagnostics / On**, the existing simulation lifecycle reads conversion
state once per simulated second. It logs an initial baseline for each city,
then active attacks every ten simulated seconds and on relevant transitions
(view flag, ownership, first positive gauge, active slots, registration starting
or stopping). Paused frames and other game stages do not sample. All stored
history consists of identifiers and scalar values and resets on load or mode
change. No additional save data or native instruction patches are introduced.

Lines start with `[Better Civ Conversion]`. A separate 8,192-line budget per
game launch does not consume the existing 4,096 AI-journal lines. The limit is
reported once; save/reload does not reset the budget. At most 16 cities and
eight slots per city are retained/reported. The current map has ten cities and
the inspected native conversion code uses four slots.

The journal records:

- City ID, city name, political owner, actual city specialty, credited converting nation and gauge.
  `specialty=0` means military, `1` religious, `2` economic (SDK vehicle-purpose IDs).
- `in-view`, view-object presence and engine camera distance. These are engine
  observations, not a guarantee that a human can see the city on screen.
- Living religious vehicles with a recognized attack order toward this city,
  and how many meet the existing city-footprint-plus-weapon-range arrival heuristic.
- Those vehicles' current combat targets: cultural slot, city turret, other or none.
  A target assignment is not proof that a shot was fired.
- Each cultural target's numeric state, registered attackers and political-ID
  reservations, plus its listening/converted population buckets and capacities.
  Registrations may lag actual fighting and are not a count of successful hits.
- Surviving turrets and those with a positive native freeze counter.

The observer uses the pinned SDK fields. It neither forces a model/view to load
nor changes cultural targets or their attacker lists. Missing or mismatched
slot/bucket counts are recorded without indexing beyond either vector.

## Evidence and limits

Read-only disassembly of the local compatible executable shows that the city
conversion update at `0xBEB710` calls the per-slot conversion calculation only
for target state 2. Registration at `0xBE1110` can add an attacker but skips its
state transition while the slot is in state 0. The separate registration at
`0xBDF820` raises state to at least 1. These observations explain which gates to
measure, but do **not** establish that a camera visit is their missing trigger.

The user observed attacks apparently beginning after visiting the city, with
variable delays. This remains an unresolved report. Alternatives include
missing slot activation, an attack still targeting defenses, movement/range,
or a UI gauge that lags simulation. Compare the actual logged state before and
after the visit before selecting a repair. No claim of a known universal game
bug or resolved cause is made.

## Exact playtest setup

Use **Better Civ SDK conversion diagnostic 2.sporemod**, with Spore closed during
replacement and only one Better Civ version installed. Keep the earlier archive as rollback; its starting-specialty inversion is still present.
Start a **new Civilization game, Hard, military player**, using stock city walls
and the same other mods as the previous validated test. A new game is needed for
the starting-specialty selection, not because this diagnostic changes saves.

| Installer setting | Choice |
| --- | --- |
| Vehicle Cost | 50% of Vanilla |
| Firing Interval | Vanilla (2 s) |
| Vehicle Speed | Vanilla (1x) |
| Offensive Units | 16 Units |
| AI Population | 30 per Rival |
| AI Starting Funds | 8,000 per Rival |
| AI Specialties | Religious Only |
| Captured Cities | Keep Specialty |
| Turrets & Fleets | 8 Slots per City |
| Rival Trade | First Rival Route |
| Trade Under Fire | Stay on Course |
| Offensive Targets | Vanilla |
| AI Diagnostics | On |
| Stalled Offensives | Off |

Relative to the recent military test, Religious Only supplies religious sieges;
Vanilla target selection and Stalled Offensives Off prevent Better Civ's timeout
from ending the event being observed. Defense/fleet settings are kept for this
diagnosis; this is not yet a religious-versus-military balance comparison.

1. Confirm ten major cities, nine religious rivals, and the 500 land-vehicle price
   in both interfaces. The log should contain `Diagnostic 2 active` and each rival baseline should
   report `specialty=1` (the military player is `specialty=0`). Stop this test if
   the starting specialties are wrong.
2. Observe a religious AI moving toward another AI city on the minimap. Keep the
   camera elsewhere for about **120 unpaused seconds after apparent arrival**.
   Do not visit the target beforehand if possible. Record attacker/defender names
   and approximate elapsed time; do not assume minimap arrival proves preaching.
3. Visit the target and stay for **30 unpaused seconds**. Note whether units preach,
   freeze turrets, fight defenders or remain stationary, and whether the gauge appears.
4. Move the camera away for **60 unpaused seconds**, then revisit. Record the gauge.
   The journal, rather than the invisible UI, supplies off-camera progression.
5. If possible repeat with another city not yet visited during that attack. A failed
   invasion is useful evidence too; no particular capture time is expected.
6. Save, fully quit, restart and reload once to check regressions. **Copy the first
   log before restarting**. Check 500 prices and diplomacy pagination still work.

The launcher recreates `spore_log.txt`; preserve it as
`spore_log-conversion-test.txt` beside the executable before relaunching.
If the journal reports its cap, preserve that log and start a fresh session for
another observation. No numerical balance changes should be made during the test.

## Validation

Release and Debug checks cover SDK field readings without game-object mutation,
null/mismatched target vectors, turret/aim classification, first-progress and view
transitions, sampling/report cadence, pause/option gating, bounded records/logs,
and the production lifecycle path. The full existing price, fleet, trade,
offensive, pagination, preload, reference-executable and DLL-loader suite is also
required. These are offline checks; runtime overhead and engine view semantics
still need the user playtest. This build has not been tested in Spore by the agent.
