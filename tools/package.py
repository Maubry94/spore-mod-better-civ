"""Build the Better Civ installer using game data from an existing release."""

import argparse
import re
from pathlib import Path
from struct import Struct, pack, pack_into, unpack_from
from xml.etree import ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "ModInfo.xml"
CATALOG = ROOT / "src/SettingsChoices.inc"
PROP = 0x00B1B104
MARKER_GROUP = 0xE2C5417D
SPEED_SCRIPT = (0x0472329B, 0x024A4F5A, 0xB3A05C62)
WALL_GROUP = 0x40828100
WALL_INSTANCES = {
    0x36386464, 0x47E05700, 0x6332837D, 0x67E2B4DC, 0x74025F48, 0x74025F49,
    0x9EFE3EEA, 0x9EFE3EEB, 0xB8B6D0B6, 0xBFC61837, 0xEC69E20D, 0xED0063A9,
}
ENTRY = Struct("<7I")
# The shipped DBPF v3 indices share just the zero instance-high word.
INDEX_PREFIX = pack("<2I", 4, 0)
CHOICE = re.compile(r"BETTER_CIV_CHOICE\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)\s*\)")


def read_catalog():
    catalog = {}
    for line in CATALOG.read_text(encoding="utf-8").splitlines():
        line = line.partition("//")[0].strip()
        if not line:
            continue
        match = CHOICE.fullmatch(line)
        if not match:
            raise ValueError(f"Invalid settings catalog entry: {line}")
        group, choice, value = match.groups()
        if (group, choice) in catalog:
            raise ValueError(f"Duplicate settings choice: {choice}")
        catalog[group, choice] = int(value, 16)
    if len(catalog) != len(set(catalog.values())):
        raise ValueError("Multiple settings choices use the same resource.")
    return catalog


def read_index(data):
    """Read the fixed index layout used by the release data we accept."""
    if len(data) < 96 or data[:8] != b"DBPF\x03\0\0\0":
        raise ValueError("Invalid input game package.")
    count, _, size = unpack_from("<3I", data, 36)
    offset = unpack_from("<I", data, 64)[0]
    if not count or offset < 96 or size != 8 + ENTRY.size * count or offset + size != len(data):
        raise ValueError("Invalid DBPF index bounds.")
    if data[offset:offset + 8] != INDEX_PREFIX:
        raise ValueError("Unsupported DBPF index layout.")
    rows = {}
    for row in ENTRY.iter_unpack(data[offset + 8:]):
        key, start, length = row[:3], row[3], row[4] & 0x7FFFFFFF
        if key in rows or start < 96 or start + length > offset:
            raise ValueError("Invalid or duplicate DBPF record.")
        rows[key] = row
    return offset, rows


def marker_key(instance):
    return PROP, MARKER_GROUP, instance


def empty_marker(data, row):
    return row[4:] == (0x80000004, 4, 0x10000) and data[row[3]:row[3] + 4] == bytes(4)


def marker_package(template, instance):
    # All public marker-only options use the same empty property record.
    offset, rows = read_index(template)
    if len(rows) != 1:
        raise ValueError("Expected one settings marker in the template.")
    key, row = next(iter(rows.items()))
    if key[:2] != (PROP, MARKER_GROUP) or not empty_marker(template, row):
        raise ValueError("Invalid settings marker template.")
    result = bytearray(template)
    pack_into("<I", result, offset + len(INDEX_PREFIX) + 8, instance)
    return bytes(result)


def mark_eight_slots(data, instance):
    # Keep all twelve wall resources and their offsets intact. The display
    # layout also needs a private marker so the DLL can select its fleet rules.
    offset, rows = read_index(data)
    key = marker_key(instance)
    walls = {(PROP, WALL_GROUP, wall) for wall in WALL_INSTANCES}
    if rows.keys() - {key} != walls:
        raise ValueError("Eight Slots per City must contain the twelve supported walls.")
    if key in rows:
        if not empty_marker(data, rows[key]):
            raise ValueError("Invalid Eight Slots per City settings marker.")
        return data

    # The old index starts immediately after all original resource payloads.
    # Put the new payload there and advance the table start by four bytes.
    result = bytearray(data[:offset]) + bytes(4)
    rows[key] = (*key, offset, 0x80000004, 4, 0x10000)
    pack_into("<I", result, 36, len(rows))
    pack_into("<I", result, 44, len(INDEX_PREFIX) + ENTRY.size * len(rows))
    pack_into("<I", result, 64, len(result))
    result.extend(INDEX_PREFIX)
    for record_key in sorted(rows):
        result.extend(ENTRY.pack(*rows[record_key]))
    return bytes(result)


def game_packages(source, manifest, catalog):
    assets = {entry.text: source.read(entry.text) for entry in manifest.findall("prerequisite")
              if entry.text.endswith(".package")}
    template = source.read("CivInsanity-Option-PrixVanilla.package")
    available = set(source.namelist())
    used = set()
    for group in manifest.findall("componentGroup"):
        group_id = group.attrib["unique"]
        for component in group.findall("component"):
            choice = component.attrib["unique"]
            name = component.text
            if name in assets:
                raise ValueError(f"Duplicate manifest package: {name}")
            if group_id == "VehicleSpeed":
                data = source.read(name)
                if set(read_index(data)[1]) != {SPEED_SCRIPT}:
                    raise ValueError(f"Invalid native vehicle-speed package: {name}")
            else:
                option = (group_id, choice)
                if option in used or option not in catalog:
                    raise ValueError(f"Unrecognized or repeated manifest choice: {option}")
                used.add(option)
                instance = catalog[option]
                if choice == "EightSlotsPerCity":
                    data = mark_eight_slots(source.read(name), instance)
                elif name not in available and name == f"BetterCiv-Option-{choice}.package":
                    # v1.0 predates these public choices. Their only payload is the marker.
                    data = marker_package(template, instance)
                else:
                    data = source.read(name)
                    _, rows = read_index(data)
                    key = marker_key(instance)
                    if set(rows) != {key} or not empty_marker(data, rows[key]):
                        raise ValueError(f"Invalid settings marker in {name}.")
            assets[name] = data
    if used != set(catalog):
        raise ValueError("The settings catalog and installer choices disagree.")
    return assets


def package(game_data, library, destination):
    manifest = MANIFEST.read_bytes()
    catalog = read_catalog()
    dll = library.read_bytes()
    if len(dll) < 64 or dll[:2] != b"MZ":
        raise ValueError("BetterCiv.dll must be a 32-bit Windows DLL.")
    pe_offset = unpack_from("<I", dll, 0x3C)[0]
    if dll[pe_offset:pe_offset + 6] != b"PE\0\0\x4c\x01":
        raise ValueError("BetterCiv.dll must be a 32-bit Windows DLL.")
    with ZipFile(game_data) as source:
        if len(source.namelist()) != len(set(source.namelist())):
            raise ValueError("The source installer contains duplicate files.")
        assets = game_packages(source, ET.fromstring(manifest), catalog)
    # An incomplete archive must neither replace an existing build nor prevent
    # retrying after a failed write.
    stream = destination.open("xb")
    try:
        with stream, ZipFile(stream, "w", ZIP_DEFLATED) as output:
            output.writestr("ModInfo.xml", manifest)
            output.writestr("BetterCiv.dll", dll)
            for name in sorted(assets):
                output.writestr(name, assets[name])
    except BaseException:
        destination.unlink()
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_data", type=Path, help="Better Civ v1.0 or a later public .sporemod")
    parser.add_argument("library", type=Path, help="Rebuilt BetterCiv.dll")
    parser.add_argument("destination", type=Path, help="Path to the new .sporemod")
    args = parser.parse_args()
    package(args.game_data, args.library, args.destination)
