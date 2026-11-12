#!/usr/bin/env python3
"""Compile the production Mach-O parser and check architectures and bounds."""
import ctypes
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

source = (Path(__file__).resolve().parents[1] / 'DarwinBootPkg/Driver/DarwinBoot.c').read_text()
types = source[source.index('typedef struct {'):source.index('STATIC EFI_HANDLE')]
parser = source[source.index('STATIC\nEFI_STATUS\nParseKernelHeader'):source.index('STATIC\nBOOLEAN\nAdtNode')]
preamble = '''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint32_t UINT32; typedef uint64_t UINT64;
typedef size_t UINTN; typedef int BOOLEAN; typedef int EFI_STATUS;
#define STATIC static
#define CONST const
#define TRUE 1
#define FALSE 0
#define EFI_SUCCESS 0
#define EFI_UNSUPPORTED 3
#define EFI_LOAD_ERROR 1
#define MAX_UINT64 UINT64_MAX
#define WINDOW_SIZE 0x10000000ULL
#define SIZE_2MB 0x200000ULL
#define EFI_PAGE_MASK 4095
#define MAX_SEGMENTS 64
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ZeroMem(p,n) memset(p,0,n)
#define CompareMem(a,b,n) memcmp(a,b,n)
#define EFI_ERROR(s) ((s) != EFI_SUCCESS)
static UINT32 U32(const void *p) { UINT32 v; memcpy(&v,p,4); return v; }
static UINT64 U64(const void *p) { UINT64 v; memcpy(&v,p,8); return v; }
'''
with tempfile.TemporaryDirectory(prefix='darwinboot-kernel-') as tmp:
    work = Path(tmp)
    (work / 'parser.c').write_text(preamble + types + parser + '\nint check(const UINT8 *p, UINTN n) { KERNEL k; return ParseKernel(p,n,&k); }\n')
    subprocess.run(['clang', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', str(work / 'parser.c'), '-o', str(work / 'parser.dylib')], check=True)
    lib = ctypes.CDLL(str(work / 'parser.dylib'))
    lib.check.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    def check(blob):
        return lib.check(ctypes.create_string_buffer(bytes(blob)), len(blob))
    base = 0xfffffff007004000
    segment = struct.pack('<II16sQQQQiiII', 0x19, 72, b'__TEXT', base, 4096, 0, 4096, 5, 5, 0, 0)
    thread = bytearray(288)
    struct.pack_into('<4I', thread, 0, 5, 288, 6, 68)
    struct.pack_into('<Q', thread, 272, base + 1024)
    blob = bytearray(4096)
    struct.pack_into('<8I', blob, 0, 0xfeedfacf, 0x100000c, 0, 2, 2, 360, 0, 0)
    blob[32:392] = segment + thread
    for subtype in (0, 2, 0x80000002, 0xc0000002):
        struct.pack_into('<I', blob, 8, subtype)
        assert check(blob) == 0, hex(subtype)
    for offset, fmt, value, status in ((8, '<I', 3, 3), (8, '<I', 1, 3), (12, '<I', 12, 1),
                                     (20, '<I', 4096, 1), (32+48, '<Q', 4097, 1),
                                     (32+72+272, '<Q', base+4096, 1)):
        bad = bytearray(blob)
        struct.pack_into(fmt, bad, offset, value)
        assert check(bad) == status, (offset, value)
    assert check(blob[:31]) == 3
    for filename in sys.argv[1:]:
        assert check(Path(filename).read_bytes()) == 0, filename
        print('PASS: actual kernel', filename)
    print('PASS: arm64/arm64e capability bits, malformed ranges, fileset membership and entry')
