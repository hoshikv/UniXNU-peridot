// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// Secondary-CPU entry and kernel-return stubs.  These run with the MMU and
// caches off, so every address they touch is an identity-mapped UEFI address.

.text
.p2align 6
.global FastbootOtherCpuEntry
FastbootOtherCpuEntry:
  msr daifset, #0xf
  msr spsel, #1
  adrp x9, gFastbootSecondaryStack
  add x9, x9, :lo12:gFastbootSecondaryStack
  add x9, x9, #0x4000
  mov sp, x9

  adrp x9, FastbootOtherCpuVectors
  add x9, x9, :lo12:FastbootOtherCpuVectors
  msr vbar_el1, x9
  isb

  ldr x9, [x0, #16]
  msr cpacr_el1, x9
  isb

  ldr x9, [x0, #24]
  bic x9, x9, #1
  bic x9, x9, #4
  bic x9, x9, #0x1000
  msr sctlr_el1, x9
  isb

  ldr x9, [x0, #0]
  ldr x1, [x0, #8]
  mov x0, x1
  mov x29, #0
  mov x30, #0
  br x9

.p2align 6
.global FastbootOtherCpuFault
FastbootOtherCpuFault:
  mrs x3, esr_el1
  mrs x4, elr_el1
  mrs x5, far_el1
  mov x6, x30
  mov x7, sp
  mov x0, #0
  mov x1, #0
  mov x2, #1
  b FastbootOtherCpuReturnStub

.p2align 11
.global FastbootOtherCpuVectors
FastbootOtherCpuVectors:
  .rept 16
  b FastbootOtherCpuFault
  .space 0x7c, 0
  .endr

.p2align 6
.global FastbootOtherCpuReturnStub
FastbootOtherCpuReturnStub:
  msr daifset, #0xf
  .inst 0xd500409f
  .inst 0xd500407f
  mrs x9, sctlr_el1
  bic x9, x9, #1
  bic x9, x9, #4
  bic x9, x9, #0x1000
  msr sctlr_el1, x9
  isb

  adrp x9, gFastbootContext
  add x9, x9, :lo12:gFastbootContext
  stp x0, x1, [x9, #0]
  stp x2, x3, [x9, #16]
  stp x4, x5, [x9, #32]
  stp x6, x7, [x9, #48]
  dsb sy

  adrp x9, gFastbootDone
  add x9, x9, :lo12:gFastbootDone
  mov x10, #1
  str x10, [x9]
  dsb sy
  sev

  movz x0, #0x8400, lsl #16
  movk x0, #2
  smc #0
1:
  wfi
  b 1b
