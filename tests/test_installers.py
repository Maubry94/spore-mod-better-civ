"""Check the published installer format without distributing game assets."""

import sys
import tempfile
import unittest
from pathlib import Path
from struct import pack, pack_into
from xml.etree import ElementTree as ET
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package


def dbpf(resources):
    header = bytearray(96)
    header[:4] = b"DBPF"
    pack_into("<I", header, 4, 3)
    pack_into("<I", header, 60, 3)
    body = bytearray()
    index = bytearray(pack("<2I", 4, 0))
    for key, payload in sorted(resources.items()):
        pointer = len(header) + len(body)
        index.extend(pack("<7I", *key, pointer, 0x80000000 + len(payload), len(payload), 0x10000))
        body.extend(payload)
    pack_into("<I", header, 36, len(resources))
    pack_into("<I", header, 44, len(index))
    pack_into("<I", header, 64, len(header) + len(body))
    return bytes(header + body + index)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)
        self.manifest = ET.parse(package.MANIFEST).getroot()
        self.catalog = package.read_catalog()
        self.base = self.dir / "original.sporemod"
        self.dll = self.dir / "BetterCiv.dll"
        image = bytearray(128)
        image[:2] = b"MZ"
        pack_into("<I", image, 0x3C, 64)
        image[64:70] = b"PE\0\0\x4c\x01"
        self.dll.write_bytes(image)
        self.walls = dbpf({(package.PROP, package.WALL_GROUP, i): pack("<2I", i, i)
                           for i in package.WALL_INSTANCES})
        self.walls_name = "CivInsanity-Option-TourellesTous.package"
        with ZipFile(self.base, "w") as z:
            z.writestr("CivInsanity-Specialites.package", b"unchanged prerequisite")
            for group in self.manifest.findall("componentGroup"):
                for option in group.findall("component"):
                    name, choice = option.text, option.attrib["unique"]
                    if name.startswith("BetterCiv-Option-"):
                        continue  # These choices were introduced after v1.0.
                    if choice == "EightSlotsPerCity":
                        data = self.walls
                    elif group.get("unique") == "VehicleSpeed":
                        data = dbpf({package.SPEED_SCRIPT: choice.encode("ascii")})
                    else:
                        instance = self.catalog[group.get("unique"), choice]
                        data = dbpf({package.marker_key(instance): bytes(4)})
                    z.writestr(name, data)

    def test_public_manifest_and_catalog(self):
        self.assertEqual(self.manifest.get("unique"), "BetterCiv")
        groups = self.manifest.findall("componentGroup")
        ids = [g.get("unique") for g in groups]
        self.assertEqual(ids, [
            "VehicleCost", "FiringInterval", "VehicleSpeed", "OffensiveUnits", "AIPopulation",
            "AIStartingFunds", "AISpecialties", "CapturedCities", "TurretsAndFleets", "TradeBehavior",
            "AIBehavior", "AIDiagnostics",
        ])
        for group in groups:
            self.assertEqual(sum(c.get("defaultChecked") == "true" for c in group), 1)
        self.assertEqual(len(self.catalog), 34)
        self.assertEqual(len(self.manifest.findall("componentGroup/component")), 37)
        self.assertEqual(self.catalog["TurretsAndFleets", "EightSlotsPerCity"], 0x0720E488)
        self.assertEqual(groups[-1].get("displayName"), "AI Diagnostics (Dev)")

    def test_one_installable_archive_and_repack(self):
        out = self.dir / "test.sporemod"
        rebuilt = self.dir / "repacked.sporemod"
        package.package(self.base, self.dll, out)
        package.package(out, self.dll, rebuilt)
        expected = {c.text for c in self.manifest.findall("componentGroup/component") +
                    self.manifest.findall("prerequisite")} | {"ModInfo.xml"}
        with ZipFile(out) as full, ZipFile(self.base) as source, ZipFile(rebuilt) as repack:
            self.assertEqual(set(full.namelist()), expected)
            self.assertEqual(len(full.namelist()), len(expected))
            self.assertEqual(set(repack.namelist()), expected)
            self.assertIsNone(full.testzip())
            for name in expected:
                self.assertEqual(full.read(name), repack.read(name))
                if name in source.namelist() and name != self.walls_name:
                    self.assertEqual(full.read(name), source.read(name))
            self.assertEqual(full.read("BetterCiv.dll"), self.dll.read_bytes())
            self.assertEqual(full.read("ModInfo.xml"), package.MANIFEST.read_bytes())
            for group in self.manifest.findall("componentGroup"):
                for option in group.findall("component"):
                    if group.get("unique") == "VehicleSpeed":
                        continue
                    data = full.read(option.text)
                    _, rows = package.read_index(data)
                    chosen = self.catalog[group.get("unique"), option.get("unique")]
                    keys = [key for key in rows if key[:2] == (package.PROP, package.MARKER_GROUP)]
                    self.assertEqual(keys, [package.marker_key(chosen)])
                    self.assertTrue(package.empty_marker(data, rows[keys[0]]))
            marked = full.read(self.walls_name)
            _, before = package.read_index(self.walls)
            _, after = package.read_index(marked)
            self.assertEqual(len(before), 12)
            self.assertEqual(len(after), 13)
            for key, row in before.items():
                self.assertEqual(row, after[key])
                self.assertEqual(marked[row[3]:row[3] + row[5]], self.walls[row[3]:row[3] + row[5]])
        with self.assertRaises(FileExistsError):
            package.package(self.base, self.dll, out)

    def test_malformed_payload_or_wrong_choice_is_refused(self):
        instance = self.catalog["TurretsAndFleets", "EightSlotsPerCity"]
        for bad in [b"bad", dbpf({package.marker_key(instance): bytes(4)}), self.walls[:-1]]:
            with self.assertRaises(ValueError):
                package.mark_eight_slots(bad, instance)
        wrong = self.dir / "wrong.sporemod"
        out = self.dir / "out.sporemod"
        with ZipFile(self.base) as source, ZipFile(wrong, "w") as z:
            for name in source.namelist():
                data = source.read(name)
                if name.endswith("PrixModere.package"):
                    data = source.read("CivInsanity-Option-PrixVanilla.package")
                z.writestr(name, data)
        with self.assertRaisesRegex(ValueError, "Invalid settings marker"):
            package.package(wrong, self.dll, out)
        self.assertFalse(out.exists())


if __name__ == "__main__":
    unittest.main()
