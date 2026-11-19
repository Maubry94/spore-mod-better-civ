# Portrait call alerts - test 1

This experimental build targets a missing portrait flash after one rival replaces
another. It does not fix the separate garbled peace-notification report.

## Evidence and change

The previous combat journal confirms that Nation Orange replaced Nation Citron
in slot 0 and had a pending communication. Clicking Orange opened the expected
first-conquest message. The journal did not record the rendered material or sound,
so it does not prove the cause of the absent alert.

The native portrait binder resets its call-active byte but reuses the UI buttons.
The native blink helper skips buttons with an existing material of type
0x05234B49. This candidate explicitly reapplies the native blink script on a
portrait, widget or pending-event transition, in both panel sizes. A replacement
without a pending event receives the native idle material, avoiding an inherited
false alert. The effect is not restarted every frame.

An open conversation defers the repair. After it closes, only a still-pending
message is signaled. The original update continues to handle acknowledgements.
No communication ID, relationship, vehicle order, balance value or saved state
is changed. Native audio is unchanged: visual recovery and sound must be reported
separately. No extra sound is played on page navigation.

The material helper at 0x808B80 is checked byte-for-byte before hooks are enabled.
No new native patch site is added. Diagnostic lines beginning
`[Better Civ UI] call-alert=` record the rearm/clear, slot, nation ID, event and
panel variant. They are limited to 512 transitions per lifecycle session. A
`rearm` line proves the script was requested, not that a flash or sound was heard.

## Install and settings

Replace the previous test with **Better Civ SDK portrait call alert test 1.sporemod**
using the ModAPI installer while Spore is closed. Keep the previous combat
diagnostics archive for rollback. Do not install both variants together.
An existing Civilization save can be used; no new planet is required for this fix.
Messages already read are not recreated by loading this build.

Keep Hard difficulty, your existing player's specialty and other mods unchanged.
For a fresh comparison use a Military player and stock walls without City Walls.

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

- [ ] When a new conqueror replaces an eliminated rival and has a message,
      the correct portrait flashes. Record both nations and approximate time.
- [ ] Report separately whether the usual call sound plays.
- [ ] Open the conversation: the expected message appears and the alert stops
      after it is handled, with no repeated false call.
- [ ] If pagination is available, change page and panel size. Pending calls remain
      visible on their page and directional markers still identify hidden callers.
- [ ] A replacement without a pending message does not inherit an old flash.
- [ ] Report any new crash, hang, continuous flashing or audio repetition.

If practical, save before an expected replacement. After observing it, quit
normally without saving to retain that earlier game state. Copy `spore_log.txt`
to `spore_log-call-alert-test1.txt` before relaunching Spore. Logs are written
during play independently of saving; a forced termination may lose the last
buffered lines. Replaying a save does not guarantee identical AI decisions.

Offline tests cover stale button reuse, the two native panel variants, identical
event IDs belonging to different nations, acknowledgement, conversation deferral,
page changes, missing widgets, invalid pointers, load/dispose and diagnostics Off.
They also check that the repair leaves fixture game objects and slot ownership
unchanged, and verify the native material strings against the reference executable.
Visual appearance and audio remain pending an in-game user test.
