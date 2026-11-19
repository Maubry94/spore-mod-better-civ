# Better Civ

Ten major cities and configurable rules for Spore's Civilization Stage: your city
and nine major rivals, using the original planet placement system.

**v1.1.0-rc.1 is a public prerelease.** It brings together the SDK migration,
diplomacy pagination, conversion fixes and optional AI improvements tested during
development. Some movement and fleet-retirement cases still need in-game testing.
See the [release notes](docs/RELEASE-v1.1.0-rc.1.md) for coverage and known issues.

- Choose rival starting specialties, including an exact **3 military / 3 religious / 3 economic** distribution.
- Keep captured cities' specialties; vehicle production follows the producing city's specialty.
- Browse every eligible rival through a paginated diplomacy panel, with ordered portraits and directional incoming-call markers.
- Apply offscreen religious conversion and Civilization reload fixes.
- Adjust prices, firing interval, speed, fleet limits and starting conditions independently.
- Choose **Better Trade** and **Better AI** presets, or keep their original behavior.

## Install

Download **Better-Civ-v1.1.0-rc.1.sporemod** from
[Releases](https://github.com/Maubry94/spore-mod-better-civ/releases).
This build targets **Steam Galactic Adventures 3.1.0.29** with the Spore ModAPI
Launcher Kit. Other executable revisions have not been validated.

1. Close Spore and back up saves you want to keep.
2. If upgrading from another Better Civ build, remove it with the **Spore ModAPI Easy Uninstaller** first. This clears obsolete option packages and settings.
3. Install the new `.sporemod` using the **Spore ModAPI Easy Installer**.
4. Launch through **Spore ModAPI Launcher**. Start a new Civilization game to apply city generation and starting settings.

Install one Better Civ build at a time. Public and developer installers use the
same mod identity and replace each other. The **8 Slots per City** option is
designed for stock city walls.

## Public installer

The first nine settings remain independent: **Vehicle Cost**, **Firing Interval**,
**Vehicle Speed**, **Offensive Units**, **AI Population**, **AI Starting Funds**,
**AI Specialties**, **Captured Cities**, and **Turrets & Fleets**.

| Setting | Choices and behavior | Default |
| --- | --- | --- |
| Trade | **Vanilla**, or **Better Trade**: help an economic rival open its first reachable rival route when both nations are at least neutral; keep land, sea and air traders on course under fire. Traders still take damage. | Better Trade |
| AI Behavior | **Vanilla**, or **Better AI**: reconsider failed attacks, abandon stalled sieges after 3 minutes with a 2-minute retry cooldown, help blocked vehicles find nearby space, and gradually retire idle surplus land units and ships without an owned port. | Vanilla |
| AI Diagnostics (Dev) | **Off / On**. Optional diagnostic journal for development and bug reports. | Off |

The original defaults of the first nine settings are retained. **Better AI is
opt-in in this prerelease** because movement and fleet retirement still have open
playtest cases. Vanilla in these two groups disables those optional rules; the
core city, price-display, conversion, diplomacy and reload fixes remain active.

A stalled siege needs a new maximum of the conquest/conversion gauge to reset its
timer. Destroying only a turret or recovering an earlier gauge value does not
count. Travel and paused time do not count. Sales apply only to eligible AI units,
protect active missions, and are limited to one unit per nation every 30 seconds.
See [installer profiles](docs/INSTALLERS.md) for the exact preset mapping.

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
