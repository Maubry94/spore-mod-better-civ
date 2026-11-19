# Blocked movement recovery - test 1

Artifact: `Better Civ SDK movement recovery test 1.sporemod`.
Rollback: `Better Civ SDK movement diagnostics test 1.sporemod`, or select
**Vehicle Movement > Vanilla** in this build. Do not install both builds.

## Purpose and limits

The November 18 log showed tightly overlapping ground vehicles with positive
desired speed but virtually no displacement, sometimes over 20 seconds. The
user also freed several idle vehicles near Delia by manually moving one unit.
This supports congestion; it does not identify a particular defective steering
routine. Naval congestion was reported but detailed recording had already capped.

This candidate tries a **native retry to the same local destination**. It does
not invent a new parking point, force a side step or separate units physically.
If their shared destination is inherently unusable, a retry may not help.
Improvement must be measured in game; successful offline checks do not prove it.

The new **Vehicle Movement** setting defaults to **Vanilla**. With
**Retry Blocked Paths**, player and AI military/religious land and sea vehicles
are examined twice per simulated second, while visible. A retry requires:

- A live vehicle with idle, movement or attack order (0-3), no active combat
  aim, and at least eight seconds on its last-fire timer.
- An unchanged native point request, no waypoint list or pending path job,
  finite coordinates and valid stopping distances; destination still unreached.
- Desired speed above 1, actual speed below 0.5, and less than one world unit
  of sampled travel throughout an eight-second observation window.
- Contact with another stationary vehicle of the same nation and locomotion,
  using footprint radii plus a small tolerance.

The DLL calls the SDK `cLocomotiveObject::MoveTo` virtual method with the same
destination and stop distances, using its normal point-request mode. Native
collision processing remains active. It neither assigns nor clears vehicle
orders or targets, changes health/damage, teleports units, moves blockers, nor
edits diplomatic rules. Trade vehicles and aircraft are excluded from this trial.

There are at most two retries per unchanged episode, at least 20 seconds apart,
and one retry globally per two simulated seconds. Order/goal changes or clear
movement start a new episode. Pauses do not count. Temporary exclusion or a
sampling gap does not reset the retry budget. Tracking capacity is 512 units;
live records are not evicted on overflow. Transient state resets on load and
mode change and is not serialized into the saved game.

## Native checks and diagnostics

The supported vehicle `MoveTo` routine (0xCA35A0, 587 bytes), request setter,
request copy, dispatch thunk and speed setter are checked against the local
reference executable with PE-derived ASLR normalization. Each candidate also
requires the expected vehicle virtual methods. No new code patch sites are
added. If these checks fail, this feature remains inactive.

The native point request copies local movement parameters, resets transit time
and marks the locomotion strategy dirty. The vehicle order queue, acceptable
combat area and idle anchor are not written by the selected method. The native
E4/E8 route methods are not called: their additional side effects are outside
this experiment. Exact semantic checks are in `MovementNative.h` and
`ReferenceCode.h`; actual runtime locomotion still needs the user's playtest.

Recovery works independently of **AI Diagnostics**. With diagnostics On,
`[Better Civ Recovery]` writes retries, five-second follow-ups and a short
30-second coverage summary. This has its own 4,096-line process budget, so the
old detailed-movement cap does not silence these reports. A follow-up records
displacement, aim, firing timer and whether the request stayed the same; it does
not automatically label movement as a successful repair.

## Exact test setup

Close Spore before replacing the previous build. Use Hard difficulty and stock
walls, with the other mods unchanged. Resume a healthy save if possible; this
rule does not require new city generation. If starting another planet, choose
a Military player. Starting choices below affect new games only.

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
| Vehicle Movement | Retry Blocked Paths |

## Focused checklist

- [ ] If ground units get stuck together, keep the camera nearby and observe
  for 45 unpaused seconds without replacement orders. Note city, nations and
  approximate local time; distinguish movement, firing and continued wiggling.
- [ ] If available, repeat with ships or idle units outside a city. One useful
  occurrence is enough; do not prolong a whole campaign solely for this test.
- [ ] Units already firing or suppressing turrets keep doing so. A new manual
  move/attack order works normally; no repeated forced return to an old order.
- [ ] Save and quit. Preserve `spore_log.txt` as
  `spore_log-movement-recovery-test1.txt` **before restarting**. Then check that
  this save reloads. Report any crash, freeze or new movement regression.

If nothing jams, report that; it does not establish effectiveness. If jamming
persists, the retry/follow-up and coverage entries determine whether the feature
ran and whether its deliberately narrow conditions excluded the case. For a
comparison, switch only **Vehicle Movement** to **Vanilla** and preserve the
second log under a different name.

## Validation

Release and Debug offline checks cover the old gameplay/UI rules plus stationary
contact, moving/looping exclusions, native virtual-call arguments, absence of
order writes, player/AI and land/sea parity, cooldown and attempt budget,
unsupported aims/requests, pause, load reset, native-method identity and relocated
reference code. The original 44 data packages are preserved; two empty installer
markers are added. No game installation or publication is performed by packaging.

## November 19 playtest result: naval congestion persists

The user reported Lavande attacking Pervenche's city "Orache" around 00:56
Paris time. The journal identifies the city as **Horach (1393)** and records
Pervenche taking it before Lavande recaptures it. The preserved source is
`spore_log-movement-recovery-test1.txt`, SHA-256
`a74b1fdd60bb6b18d0aa157fadd956897ec331c0722d1dab1f53615cb76e897a`.
The recovery startup line confirms that test 1 was active. The installed
settings matched the table above, including `Retry Blocked Paths`.

During the assault, several ships had positive desired speed and negligible
sampled travel. Some switched between city buildings and defending ships;
several temporarily had no combat aim. Two ships occupied the same rounded
position. No recovery retry was recorded for those assault ships before the
capture. The current diagnostics do not identify the individual exclusion
responsible: renewed targeting/firing, request changes and the continuous
eligibility window are possible explanations, not established causes.

After capture, nine of these ships (2924, 3615, 5038, 5042, 2920, 5040, 3619,
3613 and 5046) had idle orders and each received two retries. Every five-second
follow-up retained the same request and stationary contact; displacement was
only 0.000-0.019 world units. Ships 3615 and 3619 also shared the same retry
destination. Later summaries still counted nine stationary contacts after the
retry budget was exhausted. **The candidate did not resolve this naval case.**

An earlier Anis ship moved after one retry, but Anis captured its target between
the retry and follow-up and the movement request changed. That observation
cannot establish that the retry caused the movement. There were 22 retries and
22 follow-ups across 12 ships in the session. No ground recurrence was reported
by the user; this does not establish ground recovery effectiveness.

The capture is logged at elapsed 00:15:16 and the nine-ship retry sequence starts
at elapsed 00:15:49. Mapping these to approximately 00:57:06 and 00:57:39 uses
the original log's last-write time and its final shutdown timestamp; local times
are estimates, not an independently recorded wall clock. The journal does not
encode camera left/right or prove that the apparently empty port side is a
reachable firing position.

Next investigation: finding a distinct reachable local position for persistently
crowded units while preserving the strategic order. Simply increasing the number
of same-goal retries is not supported by this result. No additional movement
change or replacement build was made as part of this log review.
