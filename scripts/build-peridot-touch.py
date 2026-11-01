#!/usr/bin/env python3
"""Build the peridot touch archive against the actual source-kernel ABI."""
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/peridot-touch'
RECORD = ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json'
C_RECORD = ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT/bsd_init.o.json'

if not RECORD.is_file():
    raise SystemExit(f'{RECORD} missing; build the kernel once first')
record = json.loads(RECORD.read_text().strip().rstrip(','))
OUT.mkdir(parents=True, exist_ok=True)

CXX_ONLY = ('-fno-rtti', '-fno-exceptions', '-fno-threadsafe-statics', '-fno-c++-static-destructors',
            '-nostdinc++')


def command(source, destination, c_mode):
    current = json.loads(C_RECORD.read_text().strip().rstrip(',')) if c_mode else record
    arguments = current['arguments']
    result = []
    index = 0
    while index < len(arguments):
        value = arguments[index]
        if value == record['file']:
            result.append(str(source))
        elif value == '-o':
            result.extend(['-o', str(destination)])
            index += 1
        elif value.startswith('-std='):
            result.append('-std=gnu11' if c_mode else value)
        elif c_mode and (value in CXX_ONLY or value.startswith('-stdlib=')):
            pass
        else:
            result.append(value)
        index += 1
    result.extend(['-I', str(ROOT / 'kits/peridot'), '-I', str(ROOT / 'drivers/QcomGeniSpi'),
                   '-I', str(ROOT / '.cache/xnu-work/osfmk')])
    result.extend(['-Wall', '-Wextra', '-Werror', '-Wno-missing-include-dirs', '-Wno-unused-command-line-argument',
                   '-Wno-unused-parameter', '-Wno-unused-but-set-variable'])
    return current['directory'], result


def run(parts, cwd=ROOT):
    print(' '.join(str(part) for part in parts))
    subprocess.run([str(part) for part in parts], check=True, cwd=cwd)


objects = []
sources = ((ROOT / 'kits/peridot/GoodixBerlin.c', True),
           (ROOT / 'drivers/QcomGeniSpi/GeniSpi.c', True),
           (ROOT / 'kits/peridot/TouchProbe.cpp', False))
for source, c_mode in sources:
    destination = OUT / (source.stem + '.o')
    directory, parts = command(source, destination, c_mode)
    run(parts, cwd=directory)
    objects.append(destination)

archive = OUT / 'libPeridotTouch.a'
if archive.exists():
    archive.unlink()
run(['ar', 'rcs', archive, *objects])
print(archive)
