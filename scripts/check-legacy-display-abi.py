#!/usr/bin/env python3
"""Check a reviewed IOMobileFramebufferLegacy fixture against its kernelcache."""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'kits/XnuPrelink'))
from kc_image import Image


def discriminator(name):
    mask = (1 << 64) - 1
    rotl = lambda x, b: ((x << b) | (x >> (64 - b))) & mask  # noqa: E731
    key = bytes([0xb5, 0xd4, 0xc9, 0xeb, 0x79, 0x10, 0x4a, 0x79,
                 0x6f, 0xec, 0x8b, 0x1b, 0x42, 0x87, 0x81, 0xd4])
    k0, k1 = struct.unpack('<2Q', key)
    v = [0x736f6d6570736575 ^ k0, 0x646f72616e646f6d ^ k1,
         0x6c7967656e657261 ^ k0, 0x7465646279746573 ^ k1]

    def rounds(n):
        for _ in range(n):
            v[0] = (v[0] + v[1]) & mask
            v[1] = rotl(v[1], 13) ^ v[0]
            v[0] = rotl(v[0], 32)
            v[2] = (v[2] + v[3]) & mask
            v[3] = rotl(v[3], 16) ^ v[2]
            v[0] = (v[0] + v[3]) & mask
            v[3] = rotl(v[3], 21) ^ v[0]
            v[2] = (v[2] + v[1]) & mask
            v[1] = rotl(v[1], 17) ^ v[2]
            v[2] = rotl(v[2], 32)

    data = name.encode()
    full = len(data) & ~7
    for i in range(0, full, 8):
        m = struct.unpack_from('<Q', data, i)[0]
        v[3] ^= m
        rounds(2)
        v[0] ^= m
    b = len(data) << 56 & mask
    for i, c in enumerate(data[full:]):
        b |= c << (8 * i)
    v[3] ^= b
    rounds(2)
    v[0] ^= b
    v[2] ^= 0xff
    rounds(4)
    return (v[0] ^ v[1] ^ v[2] ^ v[3]) % 0xffff + 1


def fail(why):
    raise SystemExit(why)


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        fail('usage: check-legacy-display-abi.py <kernelcache> [fixture]')
    kernel = Path(sys.argv[1])
    fixture = Path(sys.argv[2]) if len(sys.argv) == 3 else \
        ROOT / 'scripts/legacy-display-d17ap-24A446.json'
    raw = kernel.read_bytes()
    abi = json.loads(fixture.read_text())
    if hashlib.sha256(raw).hexdigest() != abi['kernelcache_sha256']:
        fail('kernelcache sha256 does not match the reviewed fixture')
    top = Image(raw)
    vtable = int(abi['vtable'], 16)
    slots = [(int(t, 16), int(d, 16)) for t, d in abi['slots']]
    fixups = top.fixups()
    for i, (target, diversity) in enumerate(slots):
        entry = fixups.get(vtable + 8 * i)
        if not entry:
            fail(f'slot {i}: no chained fixup at {vtable + 8 * i:#x}')
        got, auth, key, got_diversity, address_diversity = entry
        if not auth or not address_diversity or key != 0:
            fail(f'slot {i}: not an address-diversified authenticated pointer')
        if got != target:
            fail(f'slot {i}: target {got:#x}, expected {target:#x}')
        if got_diversity != diversity:
            fail(f'slot {i}: diversity {got_diversity:#x}, expected {diversity:#x}')
    methods = abi['methods']
    for base, (name, suffix) in abi['names'].items():
        slot = methods.get(base)
        if slot is None:
            slot = int(base.removeprefix('opaque'))
        mangled = f'_ZN25IOMobileFramebufferLegacy{len(name)}{name}{suffix}'
        if discriminator(mangled) != slots[slot][1]:
            fail(f'{base}: discriminator {discriminator(mangled):#x}, expected {slots[slot][1]:#x}')
    signatures = {'supportedFrameInfo': 'EjPjPcS0_'}
    for method, alias in abi.get('aliases', {}).items():
        slot = methods[method]
        mangled = f'_ZN25IOMobileFramebufferLegacy{len(alias)}{alias}{signatures[method]}'
        if discriminator(mangled) != slots[slot][1]:
            fail(f'{method}: alias {alias} has the wrong discriminator')
    for address, expected in abi['checks']:
        try:
            got = top.read(int(address, 16), len(bytes.fromhex(expected))).hex()
        except ValueError as error:
            fail(f'{address}: {error}')
        if got != expected:
            fail(f'{address}: {got}, expected {expected}')
    print(f'{fixture.name}: {len(slots)} slots, {len(abi["bindings"])} bindings, '
          f'{len(abi["checks"])} checks, {len(abi["names"])} diversities OK')


if __name__ == '__main__':
    main()
