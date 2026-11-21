# Better Civ

Better Civ starts Spore's Civilization Stage with ten major cities, including
your capital and nine rivals. It uses the original planet placement system.

See the [installer guide](docs/INSTALLERS.md), [testing status](docs/TESTING.md)
and [development notes](docs/DEVELOPMENT.md). Changes by version are listed on
[GitHub Releases](https://github.com/Maubry94/spore-mod-better-civ/releases).

## Build and test

Use **Visual Studio 2022 / MSVC v143**, its C++ x86/x64 build tools, a Windows SDK,
the [Spore ModAPI SDK](https://github.com/Spore-Community/Spore-ModAPI), and the
ModAPI Launcher Kit. The SDK revision is recorded in `sdk-version.txt`.

```powershell
./tools/build.ps1 -SporeSdkPath "C:/dev/Spore-ModAPI" -SporeLauncherPath "C:/path/to/Spore ModAPI Launcher Kit" -ReferenceExecutable "C:/path/to/SporeApp_ModAPIFix.exe"
```

The reference executable must be a compatible unpacked executable you supply.
The script builds the SDK support library locally, then the DLL, native tests
and loader fixture. Run again with `-Configuration Debug` for Debug checks.
Output goes into `dist/Release` or `dist/Debug`, never the game directory.
Do not install the test fixture from `tests/mLibs` into the game or launcher.
These offline checks are not an in-game stability guarantee.

GitHub Actions also runs the portable installer tests.

```powershell
python -m unittest discover -s tests -p test_installers.py -v
```

## Package the installer

Supply the v1.0 release archive or a later public `.sporemod` for its game data.
The packaging tool combines it with the new DLL and the options in `ModInfo.xml`.

```powershell
python tools/package.py "Better-Civ.sporemod" "dist/Release/BetterCiv.dll" "dist/Better-Civ-test.sporemod"
```

Choose a new output filename. Keep the symbols from the matching DLL build
when diagnosing a playtest. Game resources, SDK libraries and the game
executable are supplied locally.

## Credits

Based on [Civ Insanity by NeDether](https://github.com/NeDether/Civ-Insanity).
The optional eight-turret layout uses turret positions from
[City Walls](https://www.nexusmods.com/spore/mods/10).
The C++ build uses the [Spore ModAPI SDK](https://github.com/Spore-Community/Spore-ModAPI)
and its CppRevEng adapters.
