# Better Civ

Ten major cities and configurable rules for Spore's Civilization Stage. The player starts with nine major rival nations on a map with vanilla city placement.

- Choose the rivals' starting specialties and whether cities keep their specialty after an AI capture.
- Economic rivals can trade with other AI nations and buy their partner cities.
- Land, sea, and air traders can stay on course under fire.
- Adjust turrets, fleet limits, vehicle stats, and AI starting conditions separately.

## Install

Download the `.sporemod` from [Releases](https://github.com/Maubry94/spore-mod-better-civ/releases). This build targets Steam Galactic Adventures 3.1.0.29 with the Spore ModAPI Launcher Kit.

Install the archive with the Spore ModAPI Easy Installer. Start a new Civilization game to apply city generation and starting settings. The **8 Slots per City** mode is designed for the stock city walls.

The native diplomacy portrait panel still has four slots. This branch prevents additional rivals from overflowing that panel; all nine rivals remain active, but extra portraits are not displayed there.

## Build the DLL

This branch migrates the DLL to C++17, MSVC and the Spore ModAPI SDK. Follow-up fixes address vehicle prices in the planner and world-view purchase tooltips, and a crash when a fifth rival qualifies for the four-slot diplomacy portrait panel. Gameplay settings and packaged game data are unchanged. The published v1.0 release remains the reference build until in-game regression testing is complete.

Install Visual Studio 2022 (or its Build Tools) with **MSVC v143 C++ x86/x64 build tools** and a **Windows 10 or 11 SDK**. Get the [Spore ModAPI SDK](https://github.com/Spore-Community/Spore-ModAPI) and the ModAPI Launcher Kit. The tested SDK revision is recorded in `sdk-version.txt`.

Run these commands from the repository root in PowerShell. Paths are examples; use your own SDK and launcher directories. The build writes to `dist`, never into the game or the installed mod directory.

```powershell
./tools/build.ps1 -SporeSdkPath "C:/dev/Spore-ModAPI" -SporeLauncherPath "C:/path/to/Spore ModAPI Launcher Kit"
```

The output is `dist/Release/BetterCiv.dll`. Keep the accompanying `BetterCiv.pdb` for debugging this exact build. To use Visual Studio directly, copy `SdkPaths.example.props` to `SdkPaths.props`, fill in the two paths, and open `BetterCiv.vcxproj`. Build **Release / Win32**. The CRT is linked statically; the DLL imports the installed `SporeModAPI.dll`.

## Build a .sporemod

`ModInfo.xml` contains the installer text, option IDs, and package names. The release archive supplies the packaged game data, including speed presets and turret layouts. Open those packages in SporeModder FX to inspect or edit their resources. The packaging script combines those packages with the manifest and rebuilt DLL.

```powershell
python tools/package.py "./Better Civ.sporemod" "dist/Release/BetterCiv.dll" "dist/Better Civ SDK test.sporemod"
```

Use Python 3.10 or later and the v1.0 release as the game-data input for this revision. The destination must not already exist. Internal `.package` filenames and legacy settings IDs remain compatible with v1.0.

## Run the tests

The tests cover option loading, specialty selection, capture rules, rival trade, trader retreat, fleet scaling, vehicle price display and affordability, x86 calling conventions, and instruction checks against a compatible executable built from the October 2024 Steam version. Supply your own compatible unpacked executable. The retail executable can be packed on disk. Assertions remain enabled in both test configurations. The test-only SDK fixture in `tests/mLibs` must never be installed in the game or Launcher Kit.

```powershell
./tools/build.ps1 -SporeSdkPath "C:/dev/Spore-ModAPI" -SporeLauncherPath "C:/path/to/Spore ModAPI Launcher Kit" -ReferenceExecutable "C:/path/to/SporeApp_ModAPIFix.exe"
```

These are offline checks, not an in-game stability guarantee. See [the migration notes](docs/MIGRATION.md) for the retained native hooks and the playtest checklist.

## Credits

Based on [Civ Insanity by NeDether](https://github.com/NeDether/Civ-Insanity). The optional eight-turret layout uses turret positions from [City Walls](https://www.nexusmods.com/spore/mods/10).

The C++ build uses the [Spore ModAPI SDK](https://github.com/Spore-Community/Spore-ModAPI) and its CppRevEng adapters. SDK headers and libraries are external dependencies and are not bundled in the mod archive.
