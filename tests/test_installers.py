"""Portable installer checks; no game executable or SDK required."""
import sys
import tempfile
import unittest
from pathlib import Path
from struct import pack_into, unpack_from
from xml.etree import ElementTree as ET
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import installers
import package


class InstallerTests(unittest.TestCase):
    def test_catalog_and_generated_public_manifest(self):
        installers.check_public()
        public = ET.parse(installers.PUBLIC).getroot()
        dev = ET.parse(installers.DEVELOPER).getroot()
        self.assertEqual(public.get('unique'), dev.get('unique'))
        for root, count in ((public, 12), (dev, 17)):
            groups = root.findall('componentGroup')
            self.assertEqual(len(groups), count)
            ids = [g.get('unique') for g in groups]
            self.assertEqual(len(ids), len(set(ids)))
            for group in groups:
                self.assertEqual(sum(c.get('defaultChecked') == 'true' for c in group), 1)
                self.assertEqual(len(group), len({c.get('unique') for c in group}))
            diagnostics = root.find("componentGroup[@unique='AIDiagnostics']")
            self.assertEqual(diagnostics.get('displayName'), 'AI Diagnostics (Dev)')
            self.assertEqual(diagnostics.find("component[@unique='DiagnosticsOff']").get('defaultChecked'), 'true')
        for left, right in zip(public.findall('componentGroup')[:9], dev.findall('componentGroup')[:9]):
            # Ignore indentation tails introduced by generation.
            self.assertEqual(left.attrib, right.attrib)
            self.assertEqual([(c.attrib, c.text) for c in left], [(c.attrib, c.text) for c in right])
        self.assertEqual([g.get('unique') for g in public.findall('componentGroup')[9:]],
                         ['TradeBehavior', 'AIBehavior', 'AIDiagnostics'])

    def test_preset_markers_have_distinct_resources(self):
        template = self.template()
        markers = [package.ai_marker(template, c) for c in package.AI_MARKERS]
        self.assertEqual(len(markers), len({unpack_from('<I', m, 116)[0] for m in markers}))
        with self.assertRaises(ValueError):
            package.ai_marker(b'bad', 'BetterAI')

    @staticmethod
    def template():
        # Empty-property DBPF marker, with the same layout as the shipped markers.
        value = bytearray(136)
        value[:4] = b'DBPF'
        pack_into('<I', value, 36, 1)
        pack_into('<I', value, 64, 100)
        pack_into('<4I', value, 120, 96, 0x80000004, 4, 0x10000)
        return bytes(value)

    def test_each_archive_contains_exact_manifest_dependencies_and_same_dll(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            base, dll = folder / 'base.zip', folder / 'BetterCiv.dll'
            image = bytearray(128)
            image[:2] = b'MZ'
            pack_into('<I', image, 0x3c, 64)
            image[64:70] = b'PE\0\0\x4c\x01'
            dll.write_bytes(image)
            dev = ET.parse(installers.DEVELOPER).getroot()
            names = {c.text for c in dev.findall('componentGroup/component') + dev.findall('prerequisite')
                     if c.text.endswith('.package')}
            with ZipFile(base, 'w') as z:
                for name in names:
                    if not name.startswith('BetterCiv-Option-'):
                        z.writestr(name, self.template() if name.endswith('PrixVanilla.package') else b'unchanged game data')
            for profile in ('public', 'dev'):
                output = folder / (profile + '.sporemod')
                package.package(base, dll, output, profile)
                with ZipFile(output) as z:
                    root = ET.fromstring(z.read('ModInfo.xml'))
                    required = {c.text for c in root.findall('componentGroup/component') + root.findall('prerequisite')}
                    self.assertEqual(set(z.namelist()), required | {'ModInfo.xml'})
                    self.assertEqual(len(z.namelist()), len(set(z.namelist())))
                    self.assertEqual(z.read('BetterCiv.dll'), image)
                    self.assertIsNone(z.testzip())
                    self.assertEqual(z.read('CivInsanity-Specialites.package'), b'unchanged game data')
                with self.assertRaises(FileExistsError):
                    package.package(base, dll, output, profile)


if __name__ == '__main__':
    unittest.main()
