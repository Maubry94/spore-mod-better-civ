# Better Civ

Ten major cities and configurable rules for Spore's Civilization Stage. The player starts with nine major rival nations on a map with vanilla city placement.

- Choose the rivals' starting specialties and whether cities keep their specialty after an AI capture.
- Economic rivals can trade with other AI nations and buy their partner cities.
- Land, sea, and air traders can stay on course under fire.
- Adjust turrets, fleet limits, vehicle stats, and AI starting conditions separately.

## Install

Download the `.sporemod` from [Releases](https://github.com/Maubry94/spore-mod-better-civ/releases). This build targets Steam Galactic Adventures 3.1.0.29 with the Spore ModAPI Launcher Kit.

Remove earlier Civ Insanity and Better Civ test builds, then install the archive with the Spore ModAPI Easy Installer. Start a new Civilization game to apply city generation and starting settings. The **8 Slots per City** mode is designed for the stock city walls.

## Build the DLL

Use a Windows x86 MinGW-w64 GCC toolchain and Python 3.10 or later. Run these commands from the repository root in PowerShell.

```powershell
New-Item -ItemType Directory -Force dist | Out-Null
i686-w64-mingw32-gcc -std=c17 -O2 -Wall -Wextra -Werror -shared -static-libgcc "-Wl,--nxcompat" "-Wl,--dynamicbase" "-Wl,--image-base,0x6d280000" -o "dist/BetterCiv.dll" "src/better-civ.c"
```

With the x86 edition of w64devkit, the compiler is named `gcc`.

## Build a .sporemod

`ModInfo.xml` contains the installer text, option IDs, and package names. The release archive supplies the packaged game data, including speed presets and turret layouts. Open those packages in SporeModder FX to inspect or edit their resources. The packaging script combines those packages with the manifest and rebuilt DLL.

```powershell
python tools/package.py "./Better Civ.sporemod" "dist/BetterCiv.dll" "dist/Better Civ rebuilt.sporemod"
```

Use the v1.0 release as the game-data input for this revision.

## Run the tests

The tests cover option loading and the DLL's changes against a compatible executable built from the October 2024 Steam version. Supply your own compatible unpacked executable for this test. The retail executable can be packed on disk.

```powershell
New-Item -ItemType Directory -Force tests/mLibs | Out-Null
i686-w64-mingw32-gcc -std=c17 -O2 -Wall -Wextra -Werror -static-libgcc -I src -o tests/mLibs/test-better-civ.exe tests/test-better-civ.c
& ./tests/mLibs/test-better-civ.exe "C:/path/to/SporeApp_ModAPIFix.exe"
```

## Credits

Based on [Civ Insanity by NeDether](https://github.com/NeDether/Civ-Insanity). The optional eight-turret layout uses turret positions from [City Walls](https://www.nexusmods.com/spore/mods/10).
