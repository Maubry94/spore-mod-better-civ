# Diplomacy pagination prototype

Current experiment: [portrait order test 1](PORTRAIT-ORDER.md) supersedes the
fixed-ID ordering described below with first-qualification order and a separate
UI cache. The earlier fixed-ID behavior remains documented as version history.

Archive: **Better Civ SDK diplomacy pagination fix 9.sporemod**. Built on the separate pagination prototype, 2026-11-18. Includes the startup correction below and [Civilization preload recovery](CIV-PRELOAD-RECOVERY.md).

## Directional call alerts (fix 9, optional trial)

- Move the hidden-call marker from the counter to the corresponding existing arrow button: `< !` for at least one pending caller on an earlier page, `! >` for one on a later page. The counter stays `1/2`, with no suffix.
- On the middle of three pages, both markers can appear. The direction is recomputed after automatic/manual page changes and roster compaction. Calls on the displayed page do not create a pager alert; cleared calls and eliminated callers stop contributing.
- Keep the four existing controls and their 24/44/24-unit regions, font, rounded background, spacing and click behavior. The marker is part of its arrow's caption/click area; no new window, native call or hook is introduced.
- Retain existing call surfacing and conversation blocking. The mod does not acknowledge or change a communication event to show an alert. Offline tests cover both directions, simultaneous callers on three pages, on-page calls, clearing/elimination, lifecycle reset and all four arrow-caption combinations. In-game readability and received-call handling still need playtesting.

### User playtest report for fix 8 and rollback

The user confirmed appearance in both panel states, navigation, full-exit save/reload, 500 land-vehicle prices and no observed crash/loading stall. First-page elimination correctly compacts the roster and hides pagination at four eligible rivals. The user also saw the old counter marker for hidden calls in either direction. Accessing a received hidden-page caller while preserving an open conversation is still **not tested in game**.

The original **Better Civ SDK diplomacy pagination fix 8.sporemod** is retained unchanged (SHA-256 `0df88564ce059697b0249a042b1d81c8201a4f99a6aa0e9678691f2c5f38ac04`). To revert, fully exit Spore, replace fix 9 with that archive and use the same installer choices and healthy save. Neither variant changes the save format. Preserve the next log as **spore_log-pagination-fix9-test.txt**.

## Native image layout correction (fix 8)

- The user's fix 7 screenshot shows the flat rectangular fallback; `spore_log.txt` confirms `texture=0`. The native atlas exists at the expected key and its PNG header is 512 x 512. This is a rendering setup regression, not evidence of a damaged save.
- The pinned SDK declares `Image::mDimensions` as two floats. The supported engine constructor at `0x957950` actually stores two integers at `+0x1C` and `+0x20`; the sliced renderer at `0x95CF83` and `0x95CFA9` reads them using integer-to-float conversion. The old check therefore rejected a correctly loaded 512-square image. Fix 8 uses a local byte-copy adapter to read native integer dimensions and stores the cropped image dimensions as integers. It does not alter the SDK, original image or game files.
- Correct the crop UV representation too: the native loader at `0x833E28` subtracts left from right (and top from bottom) before constructing an image. The owned crop now uses `(242/512, 0, 133/512, 116/512)` as origin plus extent, selecting the intended rounded panel rather than extending beyond it.
- Record one explicit background diagnostic: `pager-background ready: rounded-native atlas=512x512 crop=133x116`, or a specific load/size failure. The layout log should then report `texture=1`.
- The previous fixture used the SDK's float setter and repeated the same mistake as production. It now supplies independently checked native integer bytes, reproduces the old guard failure, verifies crop dimensions and UV bytes, rejects float-encoded or unexpected dimensions, and checks that the source image stays unchanged. These checks validate the compatibility adapter; they still do not render the live game HUD.

Spacing, centered captions, navigation rules and all installer settings remain as in fix 7. Continue an existing healthy save. Check the texture and rounded corners in both panel states first, then navigation and save/reload. Preserve the next log as **spore_log-pagination-fix8-test.txt**.

## Rounded background and restored gap (fix 7)

- Remove fix 6's six-unit downward inset, restoring the earlier spacing above the first visible communication row. Retain the centered counter, separate arrow regions, foreground ordering, viewport limits and compact/expanded tracking.
- Replace the empty-slot background with the blank rounded gold panel from the same Civilization HUD atlas. Inspection of the native assets confirms that the old empty-slot artwork includes an embossed telephone: these are the stray marks beside the previous arrow, not a pagination status indicator.
- Load native atlas `0x03996401!0x31A44893` (PNG type `0x2F7D0004`) through the SDK. An owned `Image` wrapper uses the native panel UV rectangle `(242,0)-(375,116)` in the 512-square atlas (Civ HUD SPUI `0x6A6CE68B`, image object 849). A standard SDK drawable stretches the center while preserving six-unit corner slices. Dim only this background to keep the cream text readable. Do not edit the shared atlas, native row drawable or game files; no game artwork is included in the archive.
- Load once per pager lifecycle, only when pagination becomes visible. Release the owned drawable with its window. If the atlas is unavailable or has unexpected dimensions, keep readable flat controls without retrying every frame.
- Extend the offline fixture for the SDK image/drawable allocator and fail on unexpected rendering calls. Check that the source atlas is unchanged, background creation is not repeated, missing-atlas fallback works, and cleanup resets the lifecycle. Release and Debug checks pass; actual rounded rendering and perceived spacing still require an in-game check.

The latest user screenshot confirms the centered counter. The requested change is to restore a gap, round the background and remove its stray marks. Do not infer new stability, elimination or incoming-call results from that screenshot. Existing healthy saves and all 14 installer choices remain compatible with this presentation-only change. Preserve the next log as **spore_log-pagination-fix7-test.txt**.

## Centered counter and closer join (fix 6)

- Give the counter its own 44 x 22 central region between two 24 x 22 arrow regions. Use a separate 92 x 22 background control with an empty caption and no mouse input. The native texture is retained without making the text window span the arrow regions.
- Explicitly select Spore's existing `DefaultStyleCentered` (`0x00AEBB70`) for all captions before setting their text. The native UI stylesheet in `Spore_Graphics.package` (type `0x0248F226`, instance `0x94DA47E5`) defines horizontal center and vertical middle alignment. The previous inherited panel style was left-aligned. No font or stylesheet is redistributed or overwritten.
- Clip each control to its own region so the counter cannot paint over an arrow, including the optional hidden-call marker (`1/2!`). Retain the explicit foreground ordering from fix 5. The background is behind all three caption controls.
- Lower the above-row pager by 6 UI units to reduce the visible gap caused by edge padding in the native artwork. Do this only when using a visible row; retain the root/below-panel fallbacks and viewport bounds. Final visual contact needs in-game review.
- Update the widget fixtures for four-control cleanup, a failed background allocation, centered style assignment, non-overlapping caption regions and normal/hidden-call captions through three pages. No new SDK method, native hook or unsafe caption-margin call is introduced.

### User playtest report for fix 5

Save loading, 500 land-vehicle prices, both directions of pagination, stable full-roster order after menu/full-restart reload and overall stability passed. The pager was above the panel, but its left-aligned page text overlapped the previous arrow and a small visible gap remained. Forward compaction after elimination and a received call from a hidden page remain **untested in game**.

The inspected `spore_log.txt` additionally records page 1's IDs `1431,1459,1485,1556`, page 2's ID `1598`, and the same page-1 group after reload. Fix 6 changes presentation only. Preserve the next log as **spore_log-pagination-fix6-test.txt** before launching the game again.

## Visible-row anchoring and arrow stacking (fix 5)

- Use the visible occupied communication row for placement. The empty placeholder still supplies its correct native background, but its hidden layout/transform is no longer used as a position reference. If the occupied row is unavailable, use a visible empty row, then the visible panel root. Retain direct edge alignment and the viewport fallback.
- Explicitly bring both arrow windows in front of the full-width textured counter. The supported executable's `AddWindow` inserts ordinary children at the front (`0x9627CD`), despite the SDK prose describing insertion at the back. Fix 4's counter was therefore in front of both arrows. `BringToFront` uses the normal SDK interface; its native implementation at `0x9611E0` accepts one pointer and returns with `ret 4`.
- Add one `pager-layout` log record when the visible panel variant changes, including root/row IDs, texture availability and final position. This helps diagnose compact/expanded placement without per-frame log spam.
- The fixed nation order, forward compaction, incoming-call handling and all gameplay rules are unchanged from fix 4.

### User playtest report for fix 4

Save loading, 500 land-vehicle prices, the native-looking background, portrait communications and session stability passed. The screenshot shows the pager overlapping the planet-name bar and its arrows hidden. Stable order after reload, forward compaction and incoming hidden-page calls could not be assessed; they remain pending, not passed.

The previous widget fixture incorrectly treated insertion order as proof of render order and supplied valid geometry for a hidden row. Fix 5 models native front insertion, checks the final stack, and gives the hidden placeholder stale geometry distinct from the visible occupied row. It also exercises visible-empty/root fallback. These offline checks do not render Spore's actual HUD. Start the next in-game test with placement and both arrows before attempting order/call tests. Preserve the log as **spore_log-pagination-fix5-test.txt**.

## Native background and stable order (fix 4)

- Reuse the native empty communication row's fill drawable for the whole pager. Transparent arrow controls stay above it; the counter ignores mouse input. The drawable is reference-counted without changing its game-owned window, handlers or contents. No game texture is copied into the mod. A replacement HUD without that drawable retains the olive flat-color fallback.
- Intended alignment: pager bottom directly against the first row's top, and right edges aligned, with no 2-unit gap/inset. Fix 4 incorrectly chose the hidden placeholder's geometry; fix 5 corrects this. Keep 92 x 22 dimensions, 24 x 22 arrow hit areas, compact/expanded tracking, viewport clamping and the below-panel fallback.
- Use each civilization's existing saved object ID to define a fixed order across all pages. Save on any page without changing it; reload on page 1 in that same fixed order. No new save field, sidecar file or loading hook is needed. First migration from fix 3 may reorder portraits once. Newly eligible rivals enter their fixed rank rather than replacing a victim or appending by notification time.
- Removing a nation shifts every following nation one position forward. For example, `A B C D / E F` becomes `A C D E / F` after B disappears. The last page disappears when empty, and navigation hides once four or fewer rivals remain eligible.
- No installer settings, gameplay values or game-data packages change.

### User playtest report for fix 3

Loading, 500 land-vehicle prices, readable controls in both panel states, navigation, saving on page 2 and returning through the main menu passed. The user reported no crash or stalled load. Reload retained working pagination but changed the order, and the pager was still visually detached. A received call from a hidden page remains **untested in game**. This report does not substitute for a full-restart test of fix 4.

Fix 4's offline checks exercised real control setup and visible layout calls through SDK-interface fixtures, including drawable ownership, fallback and viewport edges. Their geometry and stacking assumptions did not catch the reported regression; fix 5 extends those checks as described above.

## Compact navigation (fix 3)

- Reduce the pager from 116 x 24 to 92 x 22 UI units. Arrow hit areas are 24 x 22; the counter is 44 x 22.
- Right-align the pager to the visible native panel with a 2-unit inset. Retain the above-panel placement and below-panel fallback, viewport clamping, and compact/expanded panel tracking.
- Replace the blue background with muted olive-brown arrow buttons and a dark counter. Warm light captions, white hover captions and muted disabled arrows match the Civilization palette more closely.
- Remove trailing spaces from the counter so `1/2` is centered. An existing hidden-call indicator reads `1/2!` without widening the control.
- Use the same three SDK buttons and already-used interface methods. No new drawable, texture, native hook or caption-margin call is introduced.
- Only the DLL differs in the mod archive. Keep all installer settings and continue the existing campaign; no new game is required for this visual change.

### User playtest report for fix 2

The user confirmed a fresh game's startup, ten major cities with military rivals, correct 500 vehicle prices, manual save/menu reload, full-restart reload, normal four-portrait behavior, five-rival navigation, elimination handling and overall stability. The arrows worked but looked detached from the native HUD in both supplied screenshots. Calls from a hidden page and save/reload while showing page 2 were **not tested**. This report validates those observed cases, not every possible loading or UI state.

These fix 3 changes are historical; fix 4 replaces its spacing, background and reload-order rules as described above.

## Startup crash correction (fix 1)

The 2026-11-17 15:56:41 crash report points to `BetterCiv.dll+0x7423`, in `update_portrait_roster`, immediately after the log message `Pagination controls created.` The panel pointer had become the button identifier `0xbec1b002`.

The cause is the prototype's call to `IButton::SetCaptionBorders`. The pinned SDK declares a single `Rectangle` reference (4 bytes), while the supported Steam executable's vtable slot `0x54` resolves to `0x964A10`, which reads four float arguments and returns with `ret 0x10` (16 bytes). Each button therefore unbalanced the stack by 12 bytes; three buttons displaced the saved registers by 36 bytes. The disassembled prototype's epilogue and the crash registers match this failure.

Fix 1 removes this optional decorative call and retains the engine's default caption margins. Pagination, gameplay settings, save format and the SDK itself are unchanged. No new native hook or workaround calling convention is added.

A new regression fixture exercises the production widget creation, styling, first hidden frame, idempotent reuse, cleanup and partial-allocation failure. Its incompatible margin slot is deliberately fatal, so reintroducing the call fails the test. This addresses the gap in the original fixtures, which substituted the entire widget creation step. These are SDK-interface simulations, not live engine widgets.

Fix 1's fresh-game startup and vehicle prices were confirmed in game, but resuming a saved campaign could still hang. Fix 2 addresses a reproduced preload-completion condition; the user's fresh-game save/reload tests now pass. Retain those checks when testing fix 9.

## Exact changes

- Up to nine eligible living rivals are accessible through four portraits per page, with at most three pages. A rival still qualifies after its first city conquest; this does not display all nine at the start of a new campaign.
- SDK-created **<** and **>** buttons and a page indicator appear near the native portrait panel when more than four rivals qualify. Boundary arrows are disabled. Controls adapt to the visible compact/expanded panel and are hidden during a conversation or outside Civilization mode.
- First-conquest notifications make rivals eligible for their fixed rank in the roster. This can shift later portraits to the next slot or page. Hidden-page rivals remain fully active in the game.
- A new pending communication on another page can surface that page once, unless a call is already pending on the visible page or the player just chose a page manually. A **!** beside the previous/next arrow points toward pages with pending callers. Both arrows may be marked on the middle of three pages. Opening a conversation blocks page changes. The mod does not accept, decline, clear or generate communication events.
- Eliminated nations leave the roster; every following entry shifts forward in order. A conqueror does not override that order. An empty last page disappears. This can move a surviving rival between pages after an elimination.
- Native registration, reference ownership, lookup mappings and saved slot indices always refer to the four currently displayed portraits. Hidden rivals have slot -1. No engine array is expanded beyond four entries.
- Saving itself leaves the current page unchanged. The native loading adapter still restores valid saved visible slots safely and ignores hidden -1 indices. The subsequent roster update restores the fixed all-page order on **page 1**, regardless of the page saved. A new pending hidden call may then surface its page under the normal call rules. Neither page nor order is added to the save format; existing civilization IDs supply the order.
- If the SDK navigation controls cannot be created, the previous four-slot reuse behavior is retained.

The installer still has the same 14 groups, labels and defaults. All 43 game-data packages and `ModInfo.xml` are unchanged from the previous four-slot test. Only the DLL changes. Prices, specialties, diplomacy scores, offensive logic, starting conditions and turret/fleet balance are unchanged.

## Installation and exact configuration

Fully exit Spore before replacing the previous Better Civ test. Keep a copy of a healthy save and only one installed Better Civ version. Use **Hard** difficulty, a **Military** player and stock city walls without **City_Walls**. An existing healthy campaign can be resumed; a new campaign is not required. Starting-condition settings only apply when generating a new game.

| Installer setting | Exact value |
| --- | --- |
| Vehicle Cost | 50% of Vanilla |
| Firing Interval | Vanilla (2 s) |
| Vehicle Speed | Vanilla (1x) |
| Offensive Units | 16 Units |
| AI Population | 30 per Rival |
| AI Starting Funds | 8,000 per Rival |
| AI Specialties | Military Only |
| Captured Cities | Keep Specialty |
| Turrets & Fleets | 8 Slots per City |
| Rival Trade | First Rival Route |
| Trade Under Fire | Stay on Course |
| Offensive Targets | Rethink Failed Attacks |
| AI Diagnostics | On |
| Stalled Offensives | 3 Minutes |

## In-game checklist

- [ ] The campaign loads. Land vehicle prices still display and charge 500 in both purchase interfaces.
- [ ] The log reports `Diplomacy pagination ready.` and `Pagination controls created.` With four or fewer eligible rivals, the arrows are hidden and native portraits work.
- [ ] With at least five eligible rivals, check the restored small gap, rounded gold background and absence of telephone marks beside the previous arrow. The page indicator stays centered between both unobscured arrows on pages 1 and 2 (also page 3 if available). Check compact and expanded panels; neither the planet name nor the first communication row is covered.
- [ ] Previous/next navigate to different nations and return correctly. All eligible rivals appear exactly once across the pages; the arrows stop at the first/last page. A newly eligible rival enters its fixed rank and later rivals shift forward.
- [ ] Names, colors, relationship faces, hover text and communications match each displayed nation, especially immediately after a page change.
- [ ] With a pending caller on page 2, manually return to page 1 if needed: only the next button reads `! >`; the counter remains `1/2`. Reverse the situation: page 2 shows `< !` for a pending caller on page 1. The arrows and marker remain legible in both panel states.
- [ ] Viewing the caller's page removes that directional marker if no other caller remains farther in that direction. Clearing an event or eliminating its nation removes its marker. With three pages, calls on both sides of page 2 produce both markers.
- [ ] A call from a hidden page remains accessible. Check the existing automatic page change. A conversation already open is not disrupted; manually changing page does not immediately snap back to an already noticed pending call.
- [ ] Note the order across all pages, then observe an elimination in page 1. Every following nation moves up: the first nation of page 2 fills page 1's final place. No duplicate or empty last page remains; at four eligible rivals, navigation disappears.
- [ ] Note the order across all pages and save on page 2 or 3. Saving does not immediately change the page. Return through the main menu and reload, then repeat after fully exiting Spore. The same complete order returns starting at page 1 (unless a new pending call surfaces another page), with progress intact and no loading stall.

If only four or fewer rivals qualify during this session, pagination is **not tested** even if normal play is stable. The previous historical-vacancy-on-load and full-panel-first-conquest cases remain useful when they occur naturally. Preserve the log as **spore_log-pagination-fix9-test.txt** before launching the game again. Report any overlap or unreadable arrows with a screenshot.

## Verification and limits

Release and Debug checks exercise the real pagination logic through SDK-typed fixtures: 0-9 eligible rivals, three-page traversal, boundary commands, full-panel first conquest, reentrant UI updates, hidden calls, simultaneous calls, conversation blocking, manual navigation priority, roster shrinkage, forward compaction across pages, duplicate/map repair, fixed order after saving page 2 and reversing engine enumeration, fallback without navigation, mode/lifecycle reset and balanced references. Previous price, gameplay, executable-guard and DLL-load tests remain included.

The production widget setup and visible drawing path are additionally exercised against mock SDK interfaces: hidden first frame, creation/reuse, partial-failure cleanup including the fourth/background control, drawable reference ownership, native front insertion and final background-behind-captions stack, centered style assignment, separate clipped arrow/counter regions, normal and hidden-call captions, stale hidden-row geometry versus visible occupied-row placement, compact/expanded movement, visible-empty/root fallback, below-panel fallback, viewport clamping and flat-color fallback. The incompatible caption-margin interface slot remains fatal in the fixture. Native widget rendering and the actual game save pipeline are not reproduced. Successful offline checks are not a guarantee of in-game stability or correct on-screen placement. No installation, save modification or publication is part of this build.

The native update call site remains `0xCFE3FC`. The new UI lookup helper at `0xE01280` is a 27-byte routine with checked FNV-1a hash `0xce9baa2e`; the previous portrait helper checks remain. The official SDK supplies button creation, window events, civilization references, communication-screen state and the current communication-event field. Navigation binds four native records; it does not enlarge the array or reconstruct its private map.

SDK revision: the existing pinned revision in `sdk-version.txt`. The local SDK project additionally anchors its resource compiler input path to the project directory for this build environment; no SDK runtime logic is modified. External SDK libraries, the test-only ModAPI fixture, private game data and executables are not included in the mod.
