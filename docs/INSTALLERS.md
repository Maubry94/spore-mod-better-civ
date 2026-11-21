# Installer and options

Install the complete `.sporemod` with the Spore ModAPI Easy Installer. One
installer serves players and contributors. Its twelve option groups, their
labels and defaults match the public v1.1.0-rc.1 installer. When upgrading from
RC1, check your selections in the installer and apply them. Installing the
complete archive also updates the data for **8 Slots per City**.

## Presets

| Choice | Behavior |
| --- | --- |
| Trade / Vanilla | Use the original rules for choosing partners and retreating under fire. |
| Trade / Better Trade | Help traders establish a first rival route, stay on course under fire and show the ports serving an agreement. |
| AI Behavior / Vanilla | Use the original targeting, movement recovery and vehicle retirement rules. |
| AI Behavior / Better AI | Reconsider failed attacks, abandon stalled sieges, help blocked vehicles move and gradually retire eligible surplus units. |

Trade defaults to **Better Trade**. AI Behavior defaults to **Vanilla**.
**AI Diagnostics (Dev)** is a separate logging switch and defaults to **Off**.

## What Better AI does

- Defers the native AI's land-vehicle sale while the unit has a target, detects hostiles, has fired in the last 30 seconds, or has an enemy vehicle within either unit's weapon range (at least 85 world units). The same proximity check applies to optional surplus land and portless ship sales.
- Reconsiders a target after 2-4 failed waves, subject to native eligibility and an available alternative.
- Cancels identified city-attack orders after 3 simulated minutes without a new maximum of the conquest/conversion gauge. Observation starts near the city with at least three assigned attackers. A 2-minute cooldown prevents immediately retrying that city. Forced diplomatic targets are excluded; survivors can still fight locally.
- Tries bounded nearby detours for blocked approaches and quiet loops. Explicit new orders take priority. A separate deep-overlap check can separate firing or moving friendly units, for player and AI. An actually idle player unit can yield space; player fleets are never sold by these rules. Some jams and circling remain unresolved.
- Prevents a temporary detour endpoint from being treated as arrival for a proposal or delivery. Eligible native trade-path failures get at most two same-point retries per episode, with a one-second grace each; cancelled or already missing trade orders are never recreated.
- At a congested city siege, a stationary ship outside the active attack or conversion can try a clear approach along the coast. Active defenders and registered preachers retain their targets.
- An AI with both economic and combat cities can recruit up to five merchants across land, sea and air. Existing deliveries can finish. At the shared fleet limit, a safely idle, empty surplus merchant can free a slot for a reachable military or religious objective.
- After three safe idle minutes, a nation with at least 32 vehicles can retire surplus ground troops from a continent without hostile cities. It keeps three ground defenders per local city and checks for nearby enemies before each sale. A matching city must be able to reach an overseas target with ships or unlocked aircraft. The native AI chooses what to buy.
- A land merchant with no orders can retire after two minutes on a continent without an owned city or a potential trade partner. A local navigation request or leftover cargo can persist after the last port or land partner disappears. Sales still require a slow or stopped vehicle.
- After 2 safe idle minutes, may sell unassigned military, religious or economic AI ships when their nation still has cities but owns no port. Combat ships first try a hostile port on the same sea after 30 idle seconds, using at most two attempts two minutes apart; regaining a port renews this budget. Missing map data defers the decision. Explicit combat missions remain protected.

Sales use native refunds and a shared limit of one unit per nation every
30 seconds. Timers exclude pauses and reset on load. These rules do not declare
new wars, teleport units, or change city terrain.

## Trade routes on the map

For a land agreement, the mod checks the arrows against the terrain and the city
gates. When it can match the painted road to that agreement, it aligns the
paint with the verified arrows. The adjustment can also follow a detour around
a spice node.

With Better Trade, ships serving an agreement made with an inland city can show
arrows between the actual ports. The observed loading port or the only
connected home port identifies the endpoint. An active air delivery or conflicting
port evidence keeps the original display. The mod also checks sea arrows for
land crossings. It searches for a water route around the coast and hides the
arrows if it cannot verify a detour around a known crossing. The agreement,
purchase progress and ship orders continue independently of the map display.

Sea trade proposals and deliveries are revalidated when Better Trade or portless-fleet management is enabled. A missing home/destination port, or a proven disconnected sea, lets the native behavior cancel the impossible ship request. Agreements are not deleted and player ships are not sold.

## How settings reach Spore

Each option the DLL uses has a small identifying resource in the selected
package. At startup the DLL asks Spore which resources are loaded. If a group
has a missing marker or conflicting choices, it logs the reason and applies
that group's installer default. The three **Vehicle Speed** choices use Spore's
original game-data mechanism directly.

For a startup check, look for `[Better Civ Settings]` in `spore_log.txt`.
An installation upgraded by replacing the RC1 DLL alone can still have the old
eight-slot wall package, which lacks the marker. Use the complete installer to
apply that option. The player-facing choices are listed in
[ModInfo.xml](../ModInfo.xml). The [testing guide](TESTING.md) covers playtests
and [development notes](DEVELOPMENT.md) cover how to extend the catalog.
