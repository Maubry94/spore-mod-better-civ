# Testing and current status

Baseline: v1.1.0-rc.1. Installer description edits did not change the DLL or
game-data resources. [INSTALLERS.md](INSTALLERS.md) defines current presets;
[DEVELOPMENT.md](DEVELOPMENT.md) records implementation constraints. Build/test
commands are in the [README](../README.md), and patch notes are on
[GitHub Releases](https://github.com/Maubry94/spore-mod-better-civ/releases).

## Evidence already obtained

These are observations in the reported campaigns, not universal guarantees.

| Area | Observed result |
| --- | --- |
| Prices and campaign persistence | Tested terrestrial price display/payment, menu reload and full-restart reload retained progress. Completed-preload recovery reached the world in the reproduced loading case. |
| Diplomacy | Pagination, vacant-slot reuse, elimination compaction, saved ordering, observed specialty icons, call blinking/audio and directional alerts worked in reported tests. |
| Conversion | Capacity initialization and subsequent religious progress/captures were logged outside the camera view. |
| Starting specialties | Ten major cities, nine rivals in a 3/3/3 mix, and specialty persistence through save/reload were observed. |
| Diplomacy bonus and commerce | The fought-enemy bonus accumulated, including spice-mine defense; AI routes and buyouts were observed. |
| Offensives and retirement | Apparent abandonment and some land-unit cleanup were observed. They do not isolate every mod policy: native AI retirement is a possible explanation for cleanup without a mod sale event. |

Release/Debug native checks and portable installer checks passed for the published
baseline. Offline checks cover preset mapping and lifecycle resets. Subsequent
text-only packaging checks confirmed unchanged DLL/resources. Do not turn an old
candidate's unchecked checklist into a new regression claim.

## Focused checks before stable release

Use the public prerelease installer, launch with ModAPI and record the exact
settings. For observing optional features, select **Better AI**, **Better Trade**
and **AI Diagnostics (Dev): On**. Choose **Balanced (3/3/3)** only when checking
starting distribution. Keep other settings fixed for comparisons. Diagnostics
observe behavior; enabling them alone does not enable Better AI.

Normal play can supply most of the following situations. A case that never occurs
remains untested. Preserve a useful save when possible; logs do not require saving.

| Check | Required observation/evidence |
| --- | --- |
| Surplus land units | On a peaceful landmass, observe eligible military/religious surplus, gradual `reason=surplus-land` sales, retained garrison and continued native fleet decisions. The religious Citron report remains unconfirmed after the latest eligibility changes. A sale need not immediately produce a ship. |
| Lost final port | For idle military, religious and economic AI ships, observe `reason=no-owned-port` after two safe idle minutes, with at most one sale per nation per 30 seconds. Active missions/player fleets stay protected; regaining a port stops eligibility. Explicitly assigned ships are intentionally not sold even if their mission seems useless. |
| Stalled offensive | After engagement, three unpaused minutes without a new gauge high can abandon the siege. Check no immediate retry during the two-minute cooldown and an alternative target when eligible. A progressing attack continues. Survivors may still fight locally. |
| Movement and commerce | If a queue occurs, watch for about 30 seconds without intervention; correlate requests with follow-up displacement, resumed attacks/deliveries and your own new orders. A clearance request alone is not successful recovery. |
| Trade after ownership changes | Check the land ribbon is plausible, or hidden if no verified route exists, while agreements and deliveries still behave correctly. Compare with the on-screen state. |
| Balanced lifecycle (secondary check) | After one fresh Balanced game, return to the galaxy and create a second fresh game without closing Spore. Its nine rivals must again be 3/3/3. This tests in-process quota reset, not whether another ordinary game launches; only the initial founding is needed. Startup/foundation logs can establish the counts. |

Retain existing core validation rather than repeating the full price checklist
after every text edit. Report any new crash, loading stall, lost save progress,
unintended sale or serious slowdown during these sessions. Such regressions block
promotion until understood. A bounded successful playtest is evidence, not proof
that every planet is safe. Better AI remains optional and experimental.

## Reading logs while playing

The tested Windows installation writes `spore_log.txt` beside the game executable
in `SporebinEP1`. Inspect the current installation rather than assuming every
machine uses that location. Check the startup build/activation messages and
selected settings first; a feature can refuse activation on unsupported code.

A reader can follow the file with shared access while Spore continues running.
No pause or save is required. For example, in PowerShell (adjust the path):

```powershell
Get-Content -LiteralPath 'C:/path/to/Spore/SporebinEP1/spore_log.txt' -Tail 0 -Wait
```

This prints appended lines until stopped. For incremental analysis, keep a file
offset and handle truncation/replacement, incomplete lines and a temporarily
missing file. Relaunching recreates the log: preserve a copy before restarting,
then reopen the new stream. A copied log is a snapshot, not a live feed.

| Marker or field | What it can establish |
| --- | --- |
| `[Better Civ Start] balanced rival=... specialty=...` | Starting rival allocation: 0 military, 1 religious, 2 economic. Separate founding groups across lifecycle resets. |
| `[Better Civ AI]`, `stalled-abandon`, `source=rethink` | Observed offensives and mod-triggered abandonment/retargeting; `source=native` identifies native target changes. |
| `[Better Civ Fleet] event=sell`, `reason=...` | Actual mod sale attempt through the native transaction, reason and unit specialty. `event=review` records eligibility/veto evidence. |
| `[Better Civ Placement]`, `[Better Civ Recovery]`, `[Better Civ Movement]` | Recovery requests, follow-ups, trajectories and sampled motion. Follow-up displacement matters more than request count. |
| `[Better Civ Routes]` | Rejected, repaired or hidden display ribbons; not proof of delivery. |
| `[Better Civ Conversion] capacity-initialized`, `progress=...` | Capacity repair and sampled conversion progress. |
| `[Better Civ UI]`, `[Better Civ Combat]` | Recorded UI/communication/combat transitions; not proof of visible blinking, sound or correct rendered text. |

`t=` uses simulation milliseconds, not the Windows clock. Observation histories
reset on load; pauses do not count toward policy timers. For correlation, note the
nation, city and approximate real time, plus whether the game was paused/reloaded.
Some detailed movement/combat observations are limited to visible units. Sampling,
rate limits and per-launch output caps can omit events; absence of a line does
not alone prove that nothing happened. Copying the log does not reset those caps.

During an assisted live session, inspect new blocks regularly and keep a short
list of confirmed events, unresolved reports and missing situations. This needs
an active reader; it is not automatic unattended monitoring between sessions.

## Known limitations and open reports

- Naval queues, trade congestion and active-aim circling remain possible. Local
  recovery is bounded and does not replace the native pathfinder.
- Large coastal cities can overlap water along their walls. No terrain repair is
  included, and a causal link with blocked ships has not been established.
- Latest fleet eligibility changes still need the targeted in-game observations
  above. Offensive alternative selection/cooldown and trade-ribbon repair also
  need isolated evidence.
- The historical garbled peace notification after elimination remains an open
  report, separate from the repaired portrait-call alerts.
- Equal 3/3/3 totals do not imply equal distribution between continents. Relative
  conquest speed needs comparable geography/settings before balance changes.
- Three-page simultaneous alerts and every possible reference-specialty
  transition are not established by the reported UI cases.

Historical test recipes and raw investigation chronology remain in
[Git history](https://github.com/Maubry94/spore-mod-better-civ/tree/faa99f379e38fea7cf6a65aee40c0d3c45fbf94d/docs).
Keep new evidence here without duplicating the old trial-by-trial documents.
