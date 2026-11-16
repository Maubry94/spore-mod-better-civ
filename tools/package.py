"""Package a rebuilt Better Civ DLL with the game data from the v1.0 release."""

import argparse
from pathlib import Path
from struct import unpack_from
from xml.etree import ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile


def package(game_data, library, destination):
    manifest = Path(__file__).resolve().parents[1] / "ModInfo.xml"
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
        missing = packages.difference(source.namelist())
        if missing:
            raise ValueError(f"Missing game packages: {', '.join(sorted(missing))}")
        game_packages = {name: source.read(name) for name in sorted(packages)}
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
    args = parser.parse_args()
    package(args.game_data, args.library, args.destination)
