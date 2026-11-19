# Civilization preload recovery (pagination fix 2)

## Observed failure

An existing Civilization save remained in native load state 5 after both a cold
restart and a return to the main menu. Rolling back to the four-slot diplomacy
DLL did not make this same save load. That comparison alone cannot establish
which version or mod originally triggered the condition.

The Civilization model world (`0x00EB9968`) contained eight entries in preload
list 2. All eight SDK `Graphics::Model` objects already had `kModelFlagIsLoaded`
set, while the native preload-completion dirty flag was false. The native
`ModelsHavePreloaded(2)` check only tests whether this pending list is empty.
The normal cleanup that moves loaded entries to the completed list was skipped.

Re-arming the native dirty flag in the stuck session allowed the original save
to reach the world. The list was not manually emptied, incomplete models were
not marked as loaded, and the load-state completion condition was not bypassed.
This confirms a recovery mechanism, not the original source of the lost signal.

## Scope of the change

- Hook only the Civilization load-state call at `0xCFCF0C` to the native world
  accessor at `0xB3D620`, retaining its calling convention and returned world.
- Request native cleanup only for a nonempty, bounded preload list 2 whose
  entries all belong to that world and are already loaded.
- Leave incomplete, empty, foreign, or already-scheduled lists untouched.
- Leave ownership, list contents, model flags, and native readiness checks to
  Spore. The request is transient and is not serialized into saves.
- Verify the supported executable, call bytes, and the relevant native model
  layout/cleanup instructions before installing any hooks.
- No installer settings, simulation packages, prices, combat rules, or diplomacy
  behavior change in this fix.

Log on recovery: `[Better Civ] Rechecking completed Civilization model preloads.`

## Checks

Release and Debug builds test the reproduced eight-model state, incomplete
entries, null/foreign entries, empty lists, duplicate notifications, ownership
preservation, call-site routing, and rejection of modified native instructions.
Private save files and process diagnostics are excluded from source archives.

Further playtesting must cover fresh-game save/reload and diplomacy pagination
with five or more eligible rivals. Loading one recovered save does not validate
all possible loading failures or the complete pagination feature.
