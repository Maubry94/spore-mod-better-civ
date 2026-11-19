# Better Civ v1.1.0-rc.1

Public prerelease, November 19, 2026. Targets Steam Galactic Adventures 3.1.0.29
with the Spore ModAPI Launcher Kit.

## Changes since v1.0

- Migrate the DLL to C++17 and the Spore ModAPI SDK, with executable guards, x86 calling-convention tests and a loader fixture.
- Correct discounted vehicle prices in the purchase menu, world-view tooltips, actual payment and affordability checks.
- Recover completed Civilization model preloads to address the tested black-screen/loading failure cases.
- Add diplomacy pagination for eligible rivals beyond four, preserve arrival order across saves, compact eliminated nations, reuse free slots, refresh specialty icons and retain incoming-call blinking/audio. Directional `!` markers show calls on other pages.
- Add exact 3/3/3 rival specialty distribution. Preserve captured city specialties when selected and restrict production to the producing city's specialty.
- Initialize missing religious conversion capacity so AI conversion can progress outside the camera view.
- Add optional failed-offensive retargeting, stalled-siege abandonment, movement recovery, surplus land-unit retirement and retirement of idle AI ships after all ports are lost. The last rule covers military, religious and economic ships.
- Repair invalid land trade-route display paths after endpoint changes, hiding an unvalidated ribbon when no valid path is found.
- Expand optional diagnostics for combat, movement, conversions, fleet decisions and diplomacy.
- Simplify the public installer to 12 settings with **Trade: Vanilla / Better Trade**, **AI Behavior: Vanilla / Better AI**, and **AI Diagnostics (Dev)**. Keep a 17-setting developer installer built from the same sources and DLL.

See [installer profiles](INSTALLERS.md) for exact behavior and defaults. Better
Trade defaults on, Better AI is opt-in, and diagnostics default off. The earlier
nine balance settings keep their choices and defaults.

## Installation

Download **Better-Civ-v1.1.0-rc.1.sporemod** and install with the Spore ModAPI
Easy Installer. Close Spore, back up saves and uninstall the previous Better Civ
version first. Launch with ModAPI; create a new Civilization game for starting
population, funds, specialties and generation settings. Use stock city walls for
the eight-slot option. Only one Better Civ installer may be installed at a time.

`SHA256SUMS.txt` identifies the published binary. GitHub's source archives refer
to this release tag. The separate developer installer is kept for development;
its packaging command is documented in the repository.

## Validation and remaining issues

Earlier in-game tests confirmed the tested price, purchase, save/reload,
pagination, portrait-order, call-alert and offscreen-conversion cases. Balanced
3/3/3 generation and specialty persistence were observed. This does not establish
that every planet or interaction is correct.

The release is checked with MSVC Release and Debug builds, the native-reference
and DLL-loader tests, and portable installer/package tests. The final regrouped
installer and all-specialty portless-fleet retirement still need community
playtesting. Automated checks are not an in-game stability guarantee.

Known limitations:

- Some naval traffic jams and vehicles circling while actively aiming remain unresolved. The movement option is a bounded recovery attempt, not a replacement pathfinder.
- Large coastal cities can still overlap the water along their walls. No terrain correction is included.
- The latest fleet-retirement eligibility changes need live confirmation. In particular, ships retaining explicit orders are deliberately not sold, even after port loss.
- Alternative-target selection and the stalled-siege cooldown need further isolated tests. Timers and failed-wave history reset on load.
- The historical garbled peace notification for an eliminated nation remains an open report.
- The repaired trade ribbon needs further live checks after city ownership changes.

For a report, include your installer choices, affected nations/cities, approximate
time and a copy of `spore_log.txt` from `SporebinEP1`. Enable **AI Diagnostics
(Dev)** for a targeted reproduction, then copy the log before launching Spore
again. Logs do not require saving the game, but an earlier save helps replay the
problem.
