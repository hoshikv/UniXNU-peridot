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

ENTRY_ARG_HOB_GUID = '0xDAEE82E6, 0x8090, 0x4E72, { 0xBF, 0xB9, 0x78, 0xC1, 0x3C, 0xD4, 0xE3, 0x59 }'


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


def patch_text(path, replacements):
    raw = path.read_bytes()
    crlf = b'\r\n' in raw
    text = raw.decode().replace('\r\n', '\n')
    for old, new in replacements:
        if new in text:
            continue
        if old not in text:
            raise SystemExit(f'{path}: expected patch site is missing')
        text = text.replace(old, new, 1)
    if crlf:
        text = text.replace('\n', '\r\n')
    path.write_bytes(text.encode())
    print(f'peridot firmware: patched {path}')


def patch_sec(uefi):
    sec = uefi / 'Silicon/Silicium/SiliciumPkg/Sec'
    patch_text(sec / 'AArch64/ModuleEntryPoint.S', [
        ("ASM_FUNC (_ModuleEntryPoint)\n  // Do Platform Specific Initialization",
         "ASM_FUNC (_ModuleEntryPoint)\n  mov   x19, x0\n\n  // Do Platform Specific Initialization"),
        ("  // Set Jump Location\n  ldr   x0, =ASM_PFX (SecEntry)",
         "  // Set Jump Location\n  mov   x0, x19\n  ldr   x16, =ASM_PFX (SecEntry)"),
        ("  // Jump to C Code\n  blr   x0", "  // Jump to C Code\n  blr   x16"),
    ])
    patch_text(sec / 'Sec.c', [
        ('#include "Sec.h"\n', '#include "Sec.h"\n\nSTATIC CONST EFI_GUID mEntryArgHobGuid = { ' + ENTRY_ARG_HOB_GUID + ' };\n\nSTATIC UINT64 mBootArg;\n'),
        ("  PrePeiSetHobList (HobList);\n",
         "  PrePeiSetHobList (HobList);\n\n  if (mBootArg != 0) {\n    VOID *Hob = BuildGuidHob (&mEntryArgHobGuid, sizeof (mBootArg));\n    if (Hob != NULL) {\n      CopyMem (Hob, &mBootArg, sizeof (mBootArg));\n    }\n  }\n"),
        ("VOID\nSecEntry (VOID)\n{\n  // Disable Data Cache",
         "VOID\nSecEntry (IN UINT64 BootArg)\n{\n  mBootArg = BootArg;\n\n  // Disable Data Cache"),
    ])


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
    changed = copy_tree(FILES / 'Platforms', uefi / 'Platforms')
    changed += copy_tree(FILES / 'Resources', uefi)
    patch_sec(uefi)
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
