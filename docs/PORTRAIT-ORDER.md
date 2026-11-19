# Portrait order and specialty refresh - test 1

## Observed result

The user heard the call signal and saw flashing when rivals qualified in call
alert test 1. Its previous native-material repair is retained. The supplied log
also confirms that Anis (ID 1737) qualified first, then Lavande (ID 1710) was
inserted ahead of it. That was Better Civ's fixed-ID sorting policy, not a native
four-slot restriction.

Lavande's recorded original city, Esphiene (1709), was religious. At 00:06:28
Lavande converted Leane (1824), which retained its military specialty. At 00:10:05
Lavande lost Esphiene and retained Leane. The native portrait's reference
specialty comes from the first currently owned city, so a religious-to-military
icon change is expected in that sequence. The old log did not record the rendered
icon; an on-screen refresh delay is not proven by the log alone.

## Live reference-city check - November 19, 2026

Preserved live log:
`../../../playtests/movement-recovery-test3-live-20261119/spore_log-portrait-specialties-152744.txt`.
Times below are log elapsed time, not wall-clock time.

- Orange (4302) began with religious Mironoly (4301), conquered military
  Lamelia (4283) at 00:10:01, then lost Mironoly at 00:12:13. At 00:12:23 the
  portrait reference was Lamelia, purpose 0, still in slot 0. Its military icon
  is consistent with the current native reference rather than a different
  nation's portrait leaking into the slot.
- Framboise (4446) began with economic Fauve (4445). Its reference changed to
  military Basilivia (4489) at 00:25:13 around the loss of Fauve. At 01:00:31,
  losing Basilivia changed its reference to religious Gastalmone (4527), purpose
  1. Both transitions kept slot 1 and requested a native portrait refresh.
- At 01:14:57, Lamelia still belonged to Orange and was military; Gastalmone
  still belonged to Framboise and was religious. The user's reported swords and
  eye match those references. Mixed-specialty ownership does not mean every
  city or vehicle changed specialty.

Rechecked native function 0xBF1080: it reads the first owned city from the
civilization city list and obtains its current specialty. The existing mod
refreshes the native portrait to follow this value; it does not pin the icon to
the founding specialty. No runtime change was made for this report. The journal
alone still does not capture rendered pixels or prove every possible icon case.

## Changes

- Qualifying rivals append in their first-conquest notification order:
  positions 1, 2, 3, 4, then the next page. A lower object ID no longer moves a
  new rival ahead of an existing one. Simultaneous native notifications preserve
  their callback order; legacy/unobserved admissions use an ID tie-break.
- Eliminations compact the entire list without changing surviving rivals'
  relative order. The first rival from page 2 moves up when page 1 loses a rival.
- Save/load retains that order through a separate UI cache. Saving does not
  change the displayed page. Loading starts on page 1, subject to the existing
  incoming-call navigation rules.
- Refresh the native portrait when its reference city's specialty changes,
  instead of waiting for page navigation to bind it again. Never change a city's
  actual specialty. Defer while a conversation is open.
- With AI Diagnostics On, record the reference city, expected specialty and
  whether a refresh was requested. A log entry is not proof of rendered output.

The call-alert repair, directional markers, artwork and all balance options
remain unchanged. The garbled peace notification and stationary-vehicle reports
are separate unresolved issues.

## UI order cache

When Spore calls the mod's save callback, Better Civ writes a 68-byte record in
`%APPDATA%\Spore\BetterCiv\PortraitOrder\`. The filename identifies the planet's
resource key and player civilization ID. It contains up to nine civilization
IDs, a format version and checksum; no pointers or names.

The game serialization stream is left untouched. A temporary sibling is replaced
atomically; a failed UI-cache write does not fail the game save. Missing,
truncated, foreign or invalid records are ignored. The previous fixed-ID order
is the initial fallback for an existing save without this cache; historical
first-conquest order cannot be recovered from that save alone. Later arrivals
append normally. Preserve the cache when moving a save to another computer if
the UI order matters. Removing it affects ordering, not the campaign.

Loading an earlier save discards cached ranks for rivals that are absent or have
not yet qualified in that save. Moving between older backups can require the
fallback order if their rivals are no longer in the current UI cache. UI history
is bounded to the nine native rivals.

## Install and exact settings

With Spore closed, replace the previous test with
**Better Civ SDK portrait order test 1.sporemod**. Do not install both variants.
Keep **Better Civ SDK portrait call alert test 1.sporemod** for rollback.

An existing healthy game works for future qualifications and specialty changes.
For the full chronological-order check, use a game from before the first rival
qualifies. A new planet is optional. Keep Hard difficulty and the same other mods.
For a fresh test, use a Military player and stock walls without City Walls.

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

## Focused checklist

- [ ] Note the first two qualifiers. The first remains in position 1 when the
      second appears in position 2. Subsequent arrivals append across pages.
- [ ] When a first-page rival is eliminated, all following rivals move forward
      in the same order. Pagination disappears at four remaining qualifiers.
- [ ] Save with page 2 displayed, reload from the menu, then repeat after a full
      exit. The same overall order returns, initially on page 1.
- [ ] If a nation loses its reference city and retains a different specialty,
      the icon updates without changing pages. Note the nation and cities.
- [ ] Pending calls still flash and sound, the correct conversation opens, and
      directional markers continue to indicate hidden callers.
- [ ] Report any crash, loading stall, repeated flash or incorrect portrait.

Copy `spore_log.txt` to **spore_log-portrait-order-test1.txt** before relaunch.
Look for `arrival-order=saved/restored` and `portrait-specialty` entries.
No routine vehicle-price retest is needed.

## Validation limits

Release and Debug offline checks cover reverse-ID admissions, same-frame
notifications, cross-page compaction, cache reload with reversed enumeration,
earlier-save rollback, foreign/corrupt cache rejection, atomic temporary-file
round trips, save failure, specialty changes, conversation deferral and existing
pager/call-alert regressions. The actual game save callback, rendered icon and
sound require the user's in-game test. No game installation or publication is
performed as part of packaging.
