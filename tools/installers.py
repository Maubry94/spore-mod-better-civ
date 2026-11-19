"""Generate the public installer from the shared developer option catalog."""

import argparse
from copy import deepcopy
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEVELOPER = ROOT / 'installer/ModInfo.dev.xml'
PUBLIC = ROOT / 'ModInfo.xml'


def choice(group, identifier, label, description, default=False):
    node = ET.SubElement(group, 'component', game='GalacticAdventures',
                         displayName=label, description=description, unique=identifier,
                         defaultChecked=str(default).lower())
    node.text = f'BetterCiv-Option-{identifier}.package'


def public_manifest():
    source = ET.parse(DEVELOPER).getroot()
    root = ET.Element('mod', source.attrib)
    root.set('displayName', 'Better Civ')
    groups = source.findall('componentGroup')
    for group in groups[:9]:
        root.append(deepcopy(group))
    trade = ET.SubElement(root, 'componentGroup', displayName='Trade', unique='TradeBehavior')
    choice(trade, 'TradeVanilla', 'Vanilla',
           'Use the original trade-partner conditions and retreat behavior when traders are hit.')
    choice(trade, 'BetterTrade', 'Better Trade',
           'Help economic rivals open their first route to a reachable rival city when both nations are at least neutral. '
           'Land, sea and air traders keep their course under fire, but still take damage. '
           'Deliveries and city purchases follow the original rules.', True)
    ai = ET.SubElement(root, 'componentGroup', displayName='AI Behavior', unique='AIBehavior')
    choice(ai, 'AIVanilla', 'Vanilla',
           'Keep the original offensive targets, movement and vehicle-retirement decisions. '
           'Core city, conversion and diplomacy fixes remain active.', True)
    choice(ai, 'BetterAI', 'Better AI',
           'EXPERIMENTAL: Reconsider failed offensives; abandon engaged sieges after 3 minutes without a new conquest or conversion high, '
           'then avoid that city for 2 minutes. Help blocked vehicles find nearby space. '
           'Gradually sell idle surplus AI land units to make room for ships, and idle AI ships after all ports are lost. '
           'Protect active missions and player fleets from sales.')
    diagnostics = deepcopy(next(g for g in groups if g.get('unique') == 'AIDiagnostics'))
    diagnostics.set('displayName', 'AI Diagnostics (Dev)')
    root.append(diagnostics)
    for prerequisite in source.findall('prerequisite'):
        root.append(deepcopy(prerequisite))
    ET.indent(root, space='  ')
    return ET.tostring(root, encoding='utf-8') + b'\n'


def check_public():
    if not PUBLIC.exists() or PUBLIC.read_bytes() != public_manifest():
        raise ValueError('Public installer is stale. Run python tools/installers.py.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.check:
        check_public()
    else:
        PUBLIC.write_bytes(public_manifest())
