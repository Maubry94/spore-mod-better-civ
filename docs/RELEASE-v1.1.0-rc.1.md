## Changes since v1.0

- Migrate the DLL to C++17 and the Spore ModAPI SDK.
- Correct discounted vehicle prices in the purchase menu and world-view tooltips.
- Recover completed Civilization model preloads to address the tested black-screen/loading failure cases.
- Add diplomacy pagination for eligible rivals beyond four, compact eliminated nations and reuse free slots. Directional `!` markers show calls on other pages.
- Add exact 3/3/3 rival specialty distribution.
- Initialize missing religious conversion capacity so AI conversion can progress outside the camera view.
- Add optional failed-offensive retargeting, stalled-siege abandonment, movement recovery, surplus land-unit retirement and retirement of idle AI ships after all ports are lost.
- Expand optional diagnostics for combat, movement, conversions, fleet decisions and diplomacy.

## Validation and remaining issues

Earlier in-game tests confirmed the tested price, purchase, save/reload,
pagination, portrait-order, call-alert and offscreen-conversion cases. Balanced
3/3/3 generation and specialty persistence were observed. This does not establish
that every planet or interaction is correct.

Known limitations:

- Some naval traffic jams and vehicles circling while actively aiming remain unresolved. The movement option is a bounded recovery attempt, not a replacement pathfinder.
- Large coastal cities can still overlap the water along their walls. No terrain correction is included.
- The latest fleet-retirement eligibility changes need live confirmation. In particular, ships retaining explicit orders are deliberately not sold, even after port loss.
- Alternative-target selection and the stalled-siege cooldown need further isolated tests. Timers and failed-wave history reset on load.
- The historical garbled peace notification for an eliminated nation remains an open report.
- The repaired trade ribbon needs further live checks after city ownership changes.
