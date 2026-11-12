// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef DARWIN_FASTBOOT_H_
#define DARWIN_FASTBOOT_H_

#include <Uefi.h>

EFI_STATUS
EFIAPI
DarwinFastbootMode (
  IN UINT64       ArgsAddress,
  IN UINT64       EntryAddress,
  IN UINT64       MemoryBase,
  IN UINT64       MemorySize,
  IN UINT64       KernelBase,
  IN UINT64       KernelSize,
  IN UINT64       Video[6],
  IN CONST CHAR8  *CommandLine
  );

#endif
