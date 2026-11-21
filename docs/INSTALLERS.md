# Public and developer installers

One branch and one DLL serve both installers. Feature branches are merged into
`main` after review and checks; a release tag identifies the exact source.
There is no separate long-lived public/developer code fork.

`installer/ModInfo.dev.xml` contains the full 17-group catalog. Run
`python tools/installers.py` to regenerate the 12-group public `ModInfo.xml`.
`tools/package.py --profile public` is the default; `--profile dev` preserves
granular choices. Both use the identity `BetterCiv` and the same settings file.
Uninstall the previous build before switching profiles, then reinstall your
choices. Do not install both profiles together.

## Presets

| Public choice | Equivalent developer choices |
| --- | --- |
| Trade / Vanilla | Rival Trade: Vanilla; Trade Under Fire: Vanilla |
| Trade / Better Trade | Rival Trade: First Rival Route; Trade Under Fire: Stay on Course |
| AI Behavior / Vanilla | Offensive Targets: Vanilla; Stalled Offensives: Off; Vehicle Movement: Vanilla; Stranded Fleets: Vanilla; Surplus Land Units: Vanilla |
| AI Behavior / Better AI | Offensive Targets: Rethink Failed Attacks; Stalled Offensives: 3 Minutes; Vehicle Movement: Find Nearby Space; Stranded Fleets: Gradual Sale; Surplus Land Units: Make Room for Ships |

AI Diagnostics (Dev) remains independent, default Off. Better Trade is the
default as in the previous installer. AI Behavior defaults to Vanilla; selecting
Better AI enables every rule in its row. The nine earlier balance settings and
their defaults are copied unchanged to the public installer.

## What Better AI does

- Reconsiders a target after 2-4 failed waves, subject to native eligibility and an available alternative.
- Cancels identified city-attack orders after 3 simulated minutes without a new maximum of the conquest/conversion gauge. Observation starts near the city with at least three assigned attackers. A 2-minute cooldown prevents immediately retrying that city. Forced diplomatic targets are excluded; survivors can still fight locally.
- Tries bounded nearby detours for blocked approaches and quiet loops. Combat and explicit orders take priority. An actually idle player unit can yield space; player fleets are never sold by these rules. Some jams and circling remain unresolved.
- Keeps commercial loading/delivery phases suspended during its temporary detours. Eligible native trade-path failures get at most two same-point retries per episode, with a one-second grace each; cancelled or already missing trade orders are never recreated.
- After 3 safe idle minutes, may sell surplus combat land units when an AI has at least 32 vehicles, no hostile city on that continent, a matching-specialty producing port and a reachable overseas enemy port. Retains at least six ground defenders per owned local city; stops when six ships of that specialty exist. The native AI decides what to buy.
- After 2 safe idle minutes, may sell unassigned military, religious or economic AI ships when their nation still has cities but owns no port. Any regained port or new mission resets eligibility. All explicit missions, including defense and trade, are protected.

Sales use native refunds and a shared limit of one unit per nation every
30 seconds. Timers exclude pauses and reset on load. These rules do not declare
new wars, teleport units, or change city terrain.

## Compatibility and validation

Existing granular and v1.0 French settings identifiers remain readable. If stale
granular lines coexist with public presets, the public choice takes precedence
regardless of line order. The developer manifest does not write public presets;
uninstalling before a profile switch clears old choices.

The C++ tests compare bundled and granular settings, verify resets and mixed-file
precedence, and retain developer independence. Python tests verify manifests,
defaults, distinct empty-property marker resources and archive contents. GitHub
Actions runs the Python checks. Full x86 builds and native-reference checks need
local MSVC, SDK and game files and are not run on GitHub-hosted runners.

See [TESTING.md](TESTING.md) for current evidence, remaining checks and live log
reading, and [DEVELOPMENT.md](DEVELOPMENT.md) for implementation constraints.
Release notes are published on
[GitHub Releases](https://github.com/Maubry94/spore-mod-better-civ/releases).
