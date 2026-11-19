"""Package a rebuilt Better Civ DLL with the game data from the v1.0 release."""

import argparse
from pathlib import Path
from struct import pack_into, unpack_from
from xml.etree import ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile
from installers import DEVELOPER, PUBLIC, check_public


AI_MARKERS = ('TargetsVanilla', 'RethinkFailedAttacks', 'DiagnosticsOff', 'DiagnosticsOn',
              'StalledOff', 'Stalled120', 'Stalled180', 'Stalled240', 'Balanced333',
              'MovementVanilla', 'RetryBlockedPaths', 'NearbySpace',
              'StrandedVanilla', 'StrandedSell', 'LandVanilla', 'LandRebalance',
              'TradeVanilla', 'BetterTrade', 'AIVanilla', 'BetterAI')


def ai_marker(template, choice):
    # Same empty PROP marker format as the existing installer options, with a
    # separate resource instance for every new choice. No simulation properties.
    if (len(template) != 136 or template[:4] != b'DBPF' or
            unpack_from('<I', template, 36)[0] != 1 or
            unpack_from('<I', template, 64)[0] != 100 or
            template[96:100] != b'\0' * 4 or
            unpack_from('<4I', template, 120) != (96, 0x80000004, 4, 0x10000)):
        raise ValueError('Unexpected installer marker template.')
    marker = bytearray(template)
    instance = 0x811c9dc5
    for character in ('bettercivai_' + choice.lower()).encode('ascii'):
        instance = ((instance * 0x01000193) ^ character) & 0xffffffff
    pack_into('<I', marker, 116, instance)
    return bytes(marker)


def package(game_data, library, destination, profile='public'):
    if profile not in ('public', 'dev'):
        raise ValueError('Unknown installer profile.')
    check_public()
    manifest = DEVELOPER if profile == 'dev' else PUBLIC
    root = ET.parse(manifest).getroot()
    packages = {
        entry.text
        for entry in root.findall("prerequisite") + root.findall("componentGroup/component")
        if entry.text.endswith(".package")
    }
    dll = library.read_bytes()
    pe_offset = unpack_from("<I", dll, 0x3C)[0]
    if dll[:2] != b"MZ" or dll[pe_offset:pe_offset + 6] != b"PE\0\0\x4c\x01":
        raise ValueError("BetterCiv.dll must be a 32-bit Windows DLL.")

    with ZipFile(game_data) as source:
        generated = {f'BetterCiv-Option-{choice}.package': ai_marker(
            source.read('CivInsanity-Option-PrixVanilla.package'), choice) for choice in AI_MARKERS}
        missing = packages.difference(source.namelist()).difference(generated)
        if missing:
            raise ValueError(f"Missing game packages: {', '.join(sorted(missing))}")
        game_packages = {name: generated[name] if name in generated else source.read(name) for name in sorted(packages)}
    with ZipFile(destination, "x", ZIP_DEFLATED) as output:
        output.write(manifest, "ModInfo.xml")
        output.writestr("BetterCiv.dll", dll)
        for name, contents in game_packages.items():
            output.writestr(name, contents)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_data", type=Path, help="Better Civ v1.0 .sporemod")
    parser.add_argument("library", type=Path, help="Rebuilt BetterCiv.dll")
    parser.add_argument("destination", type=Path, help="Path to the new .sporemod")
    parser.add_argument('--profile', choices=('public', 'dev'), default='public',
                        help='Public presets or separate developer options; same DLL.')
    args = parser.parse_args()
    package(args.game_data, args.library, args.destination, args.profile)
