#!/usr/bin/env python3
"""Put the peridot device package into a Mu-Silicium checkout.

The checkout supplies the SoC, ACPI and vendor driver packages.  This
repository supplies the peridot platform description, the UniXNU boot manager
and the vendor DXE binaries that the phone's firmware does not ship in the
Mu-Silicium tree.  Run it once per checkout, before the firmware build:

    scripts/apply-peridot-firmware.py --uefi ../Mu-Silicium

The build driver calls it as the board preflight, with the checkout in
UNIXNU_UEFI.
"""
from pathlib import Path
import argparse
import os
import shutil
import sys
import tarfile

HERE = Path(__file__).resolve().parent
FILES = HERE / 'firmware'


def copy_tree(source, destination):
    changed = []
    for path in sorted(source.rglob('*')):
        if path.is_dir():
            continue
        target = destination / path.relative_to(source)
        if target.is_file() and target.read_bytes() == path.read_bytes():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        changed.append(target)
    return changed


def extract_binaries(archive, destination):
    if not archive.is_file():
        return None
    marker = destination / 'peridot'
    if marker.is_dir():
        return None
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, 'r:gz') as tar:
        try:
            tar.extractall(destination, filter='data')
        except TypeError:
            tar.extractall(destination)
    return marker


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--uefi', type=Path, default=os.environ.get('UNIXNU_UEFI'),
                        help='Mu-Silicium checkout (default: UNIXNU_UEFI)')
    args = parser.parse_args()
    if not args.uefi:
        parser.error('the Mu-Silicium checkout is not known: pass --uefi DIR or set UNIXNU_UEFI')
    uefi = args.uefi.expanduser().resolve()
    if not (uefi / 'Resources/Scripts/mkbootimg.py').is_file():
        parser.error(f'{uefi} is not a Mu-Silicium checkout')
    changed = copy_tree(FILES / 'Platforms', uefi)
    binaries = extract_binaries(FILES / 'Binaries/peridot.tar.gz', uefi / 'Binaries')
    for path in changed:
        print(f'peridot firmware: {path.relative_to(uefi)}')
    if binaries:
        print(f'peridot firmware: {binaries.relative_to(uefi)} (vendor DXE binaries)')
    if not changed and not binaries:
        print(f'peridot firmware: {uefi} is already current')
    return 0


if __name__ == '__main__':
    sys.exit(main())
