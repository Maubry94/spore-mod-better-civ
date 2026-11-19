# Better Civ

Ten major cities and configurable rules for Spore's Civilization Stage: your city
and nine major rivals, using the original planet placement system.

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

Portable installer checks, also run by GitHub Actions:

```powershell
python tools/installers.py --check
python -m unittest discover -s tests -p test_installers.py -v
```

## Package public and developer installers

Both installers contain **the same DLL** and come from the same source branch.
`installer/ModInfo.dev.xml` is the shared option catalog; `tools/installers.py`
generates the public `ModInfo.xml` from it. Run the generator after editing the
catalog. Packaging refuses a stale public manifest.

Use the **v1.0 release archive** (or a later developer archive) as game-data input:

```powershell
python tools/package.py "Better-Civ.sporemod" "dist/Release/BetterCiv.dll" "dist/Better-Civ-v1.1.0-rc.1.sporemod"
python tools/package.py "Better-Civ.sporemod" "dist/Release/BetterCiv.dll" "dist/Better-Civ-v1.1.0-rc.1-dev.sporemod" --profile dev
```

Destinations must not already exist. Public archives intentionally omit the
separate developer option packages, so use the original v1.0 game-data archive
when rebuilding both profiles. The [developer profile](docs/INSTALLERS.md)
retains all 17 settings. Native game resources, SDK libraries and game executables
are not committed to this repository. Keep symbols locally for the exact DLL
being tested.

## Credits

Based on [Civ Insanity by NeDether](https://github.com/NeDether/Civ-Insanity).
The optional eight-turret layout uses turret positions from
[City Walls](https://www.nexusmods.com/spore/mods/10).
The C++ build uses the [Spore ModAPI SDK](https://github.com/Spore-Community/Spore-ModAPI)
and its CppRevEng adapters.
