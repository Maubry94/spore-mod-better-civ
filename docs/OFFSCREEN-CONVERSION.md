# Offscreen conversion fix 2

Based on conversion diagnostic 2. The user's religious-only foundation, prices,
save/reload and pagination checks passed. This experiment addresses missing
conversion capacities, not religious army strength or conversion speed.

## Fix 1 activation failure and fix 2

Both user logs from the first fix report `Rules inactive: unsupported executable
or conflicting patch.` The installed DLL hash matches fix 1. Neither playtest
therefore exercised the capacity repair, and the empty diagnostic output is not
evidence that the repair was called but failed.

A read-only inspection at the main menu found the game loaded at `0x000E0000`,
below the reference image base `0x00400000`. Seventeen guard ranges match exactly;
the other four differ only in 32 absolute address operands relocated by Windows.
Fix 1 incorrectly hashed those operands as if the game always used its preferred
base. No other modification was found in the inspected ranges.

Fix 2 normalizes only those exact HIGHLOW operands before comparison, using the
actual image base. It still checks every instruction and every address byte;
the operands are not ignored. The allowlist is checked against the reference
executable's relocation directory. A rejection now logs the failing range and
expected/actual hashes. Capacity initialization, conversion rates, presets and
all game-data packages are unchanged from fix 1.

## Evidence

The November 18 diagnostic-2 journal contains nine religious rival cities. Seven
never acquire nonzero conversion bucket capacities and remain at zero conversion
throughout the recorded session, despite registered religious attackers during
the test. For example, Leveroni ends with ten registered attackers and 0%.

Milleu is the sole captured city in that journal. Its capacities change from
0/0 to 10 listening / 2 converted per slot at log time 00:09:52. Its first
positive conversion sample appears at 00:10:08, and its owner changes at
00:14:22. This matches the user's report of visiting the successfully converted
city. Bodon also later acquires capacities and reaches 20%, without being
captured. Do not claim that every city reported as unvisited stayed at 0%.

The diagnostic's engine view flags stay true and camera distances stay zero;
they are not a reliable measure of what the player actually viewed in this run.
The exact camera transition and origin of the game's visual dependency are not
proven by those flags. This is not a claim that every installation has this bug.

Native executable inspection provides a concrete missing dependency:

- The city-view routine at `0xD57B00` generates crowd positions and is the sole
  direct caller of the capacity setter `0xBD9FF0` at `0xD57D16`.
- The view uses radii 7 and 3 with spacing 1.5. The generator at `0xAFDF60`
  produces floor((pi * radius^2) / (pi * spacing^2) * 0.5) positions: 10 and 2.
  These counts match the visited cities in the journal.
- Native conversion update `0xBEB710` selects active cultural targets (state 2).
  Its calculation at `0xBEB310` clamps new listeners to remaining capacity. An
  empty capacity therefore prevents that stage from receiving new listeners.

## Exact change

One additional caller hook, at `0xAE57E3`, checks the city immediately before
its normal conversion update. Only in Civilization, it initializes the four
capacity pairs to 10/2 when all these conditions hold:

- The city has a positive robot population and zero conversion progress.
- All four targets and their culture objects exist; target states are valid.
- At least one active target has a registered attacker from another nation.
- Every capacity, current bucket population and pending migration count is zero.

Partial initialization, existing progress and populated buckets are left intact.
Only eight capacity integers are written. The native update still runs exactly
once. The mod does not write the gauge, ownership, actual populations, timers,
attack orders or target states, and does not load views or spawn visual crowds.
Repeated calls do nothing after initialization. The normal view setup can later
initialize its visuals. Diagnostics are optional and do not enable the repair.

The hook, producer/consumer routines, setter and constants are checked against the
supported executable before any patch is written. No balance packages, installer
choices, army sizes, attack rates, turret rules or save format change. The repair
works on eligible stalled cities in an existing save as well as new games.

## Exact playtest setup

Install **Better Civ SDK offscreen conversion fix 2.sporemod** with Spore closed;
keep only one Better Civ version installed. Preserve conversion diagnostic 2 as
rollback. Resume the saved religious test, **Hard, military player, stock walls**.
Do not start a new game for the first comparison. Keep the same other mods.

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

1. Start Spore with ModAPI. Before spending time on another siege test, confirm
   the log reports `Offscreen capacity fix 2 active` and `C++/SDK city rules active`.
   Stop and retain the log if it reports `Rules inactive` instead. Then resume
   the current saved test planet; a new game is not needed.
2. Keep the camera at the player's city for **three unpaused minutes**. Do not
   visit the other sieges. Record captures seen on the minimap, if any. A capture
   is not required: sufficient surviving attackers may still be missing.
3. Save and quit before visiting the targets. Copy `spore_log.txt` to
   `spore_log-conversion-fix2.txt` before restarting. The key check is nonzero
   conversion progression on previously zero-capacity cities while unvisited;
   it can be verified in the journal even if no city has yet fallen.
4. Reload once more, then inspect an ongoing siege. Check that the gauge and
   visual effects work and no progression was reset on visiting the city.
5. Report any new crash or loading problem. Existing price checks are already
   validated and should not be repeated unless a later change affects them.
   Preserve a separate log from the second session.

With AI Diagnostics On, each repair logs `capacity-initialized city=...` (at most
64 such lines per process). Existing ten-second conversion snapshots remain.
The installed archive is separate from diagnostic 2; local diagnostic evidence
and the user's save backup are not included in distributable source archives.

## Validation and limits

Release and Debug tests cover the exact stalled-save shape, already initialized
and partly populated cities, invalid/missing targets, unchanged non-capacity
bytes, repeated calls, phase gating, diagnostic independence and the production
calling convention. Tests independently calculate the capacities from constants
in the compatible reference executable and verify the new hook and guards.
The existing specialties, prices, trade, offensives, pagination, preload and DLL
loader suite remains required. Fix 2 additionally tests the guards at the
preferred base, below it (the user's observed base) and above it. Every changed
byte, including relocated operands, must still be rejected. All four captured
live code ranges pass the production validator in Release and Debug; these
local binary captures are not distributed. Full game activation and offscreen
progression on the resumed diagnostic campaign passed the user playtest below.

Once offscreen progression is verified, religious/military balance can be
compared with matching settings. This experiment does not promise a capture
within three minutes or equal siege speed between specialties.


## User playtest: November 18, 15:14-15:18 (Europe/Paris)

The installed fix 2 activated successfully. The user stayed at the player city
throughout this resumed-save observation and did not visit the AI sieges. The
renamed journal, `spore_log-conversion-fix2.txt`, ends with normal shutdown at
15:18. Its engine view fields are not independent proof of camera position;
offscreen coverage is supplied by the user's report.

Five cities logged `capacity-initialized`: Ricky, Tabeauste, Orimmyot, Rhesa and
Daquild. All five subsequently logged positive conversion progress. Ricky was
captured. Tabeauste reached 81.1321%; Daquild reached 5.7143%. Thus a successful
capture is not the only evidence of working conversion simulation.

Two actual religious captures were recorded for civilization 1919: Ricky at
journal time 00:03:41 (owner change sampled at 00:03:42), then Murellon at
00:06:19. Both capture-counter events are type 1 (religious). Murellon did not
need capacity initialization in this session. The journal also shows a transient
100% for Orimmyot without an owner change; that is not counted as a capture.

Outcome: offscreen progress and completed religious captures are validated on
this resumed campaign. This is not a blanket stability guarantee or a measured
comparison of religious and military army balance. Keep fix 2 as the current
conversion test build; no new binary or balance changes follow from this result.
