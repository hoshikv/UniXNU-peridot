# OnePlus 8T \(stevedang67889\)

[stevedang67889/UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab)

<div v-pre>

OnePlus 8T \(KB2000\). Firmware, menu and payload volume work. The kernel runs the whole of kernel\_bootstrap, starts the scheduler and its first thread, and with PE\_init\_iokit, initialize\_screen, kperf\_init\_early, ml\_wait\_max\_cpus and platform\_stall\_panic\_or\_spin skipped it stops after PE\_init\_iokit and before bsd\_init. A 32-checkpoint flag log is implemented, but the DRAM flag page does not survive the reset; the pram mailbox is the next measurement.

Source record: [porting.toml](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/porting.toml), `5ba9d471c59a893ef40868190d73d88edb1af453`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `implemented` |
| [Firmware and boot](#firmware) | `device-verified` |
| [CPU and interrupts](#cpu) | `implemented` |
| [Memory and DMA](#memory) | `implemented` |
| [Storage and root](#storage) | `planned` |
| [Display](#display) | `device-verified` |
| [Input](#input) | `device-verified` |
| [GPU](#gpu) | `planned` |
| [Network](#network) | `planned` |
| [Audio](#audio) | `planned` |
| [Power](#power) | `planned` |
| [Image assembly](#packaging) | `device-verified` |
| [Diagnostics](#diagnostics) | `device-verified` |

## Configuration {#configuration}

`implemented`

Components: `ArmDTPlatform`, `ArmCpu`, `SoftwareAES`, `XnuPrelink`, `DarwinBoot`.

### Hardware and dependencies

- UniXNU format 2 device repository
- UniXNU-sm8250-common SoC feed pinned in feeds.toml
- iPad7,1 17.7.11 21H461 donor IPSW

### Configuration

- board.toml schema\_version = 2
- components include ArmDTPlatform, ArmCpu, SoftwareAES, XnuPrelink and DarwinBoot
- kernel.prelink slide -0x1000000 against kernelcache f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7
- kernel archives add build-arm-driver.py ArmDTPlatform and ArmCpu

### Procedure

- Clone UniXNU, UniXNU-kebab and UniXNU-sm8250-common as siblings
- Run ./unixnu fetch and ./unixnu defconfig kebab\_defconfig
- ./unixnu status --json reports the resolved pins

### Development findings

- The format 2 builder exports an arm-io child only for device tree nodes that carry a unixnu,adt-name property, so an untagged tree yields an empty arm-io and the kernel has no interrupt controller.
- The boot image has to use the phone own device tree; the tree shipped with Mu-Silicium lacks cont\_splash\_region@9c000000 and ABL then keeps the panel dark.
- Two inputs are not in any repository and must be captured per handset: the live device tree from /sys/firmware/fdt and the kernel built on a macOS runner. proprietary-files.txt names both with their hashes and consumers.
- Every change to a file an area owns has to update that area findings or evidence in the same commit; the catalog workflow fails otherwise and names the path.
- The device summary is what the generated page shows first, so it has to name the current stage rather than the first blocker found.
- The README status table names the current PE\_init\_iokit to bsd\_init gap and links the 32-checkpoint capture in docs/log-capture.md.

### Completion criteria

- Every source pin resolves and the selected components have records; unixnu status reports the intended overrides.

### Current limits

- No device-side repository record was published before this file; the entries below are the first complete record.

### Next action

- Keep the record current as firmware and kernel patches land.

### Evidence

- `source`: [Format 2 board description with the SM8250 SoC feed and the five components above.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/board.toml)
- `source`: [Thirteen areas with measured facts; python3 scripts/porting-record.py check --device ../UniXNU-kebab passes.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/porting.toml)

## Firmware and boot {#firmware}

`device-verified`

Components: `DarwinBoot`.

### Hardware and dependencies

- Mu-Silicium Platforms/OnePlus/kebabPkg
- Phone's own live device tree as the boot image DTB
- boot\_a, 96 MiB, header v2 with 4096-byte pages

### Configuration

- FD\_BASE 0x9FC00000, FD\_SIZE 0x4600000 \(3 MiB firmware volume plus a raw payload region at 0x00300000\)
- UFSDxe removed from DXE.inc; SEC disables the APSS watchdog at 0x17C10008
- ScmDxeLA and SdccDxe are included; TzDxeLA is commented out
- Framebuffer PCDs 1080x2400x32
- unixnu bootimg passes the live DTB; ABL refuses images without the splash region node

### Procedure

- Build firmware with ./unixnu build firmware -D ../UniXNU-kebab --uefi &lt;Mu-Silicium&gt;
- ./unixnu bootimg -D ../UniXNU-kebab --uefi &lt;Mu-Silicium&gt; --fd &lt;SILICIUM\_UEFI.fd&gt; --dtb &lt;live.dtb&gt; --ramdisk none
- fastboot flash boot\_a &lt;image&gt;; fastboot reboot

### Development findings

- ABL hands the framebuffer over only when the boot image carries the phone's own device tree; the tree shipped with Mu-Silicium has no cont\_splash\_region@9c000000 and the panel stays black.
- A payload file inside the firmware volume is impossible: the FFS size field is 24 bits, so the 64 MiB payload is a raw FDF region instead.
- FrameBufferSerialPortLib handled newline and carriage return but ignored form feed, so DarwinBoot's clear screen did nothing and the left half of every log line was cut off.
- The payload builder injects the nodes XNU looks up by property \(gic with interrupt-controller master, a one-cell redistributor stride, virtual-timer-intid, a timer node, pram and the panic log size\); without them the kernel stops before PE\_init\_platform returns.
- KebabBootLog reads the 32 checkpoint bytes at 0xB9000010, prints the set list and clears the flags; CacheMaintenanceLib flushes the clear.
- The current firmware has SCM and SDCC but no UFS block device: UFSDxe is removed for the vendor assert, TzDxeLA is disabled and SdccDxe does not replace UFS.

### Completion criteria

- Firmware boots from boot\_a, the menu is rendered on the panel, the keys navigate it and the log is readable.

### Current limits

- USB is not brought up in the firmware; the only log transport is the panel.

### Next action

- Publish a firmware image revision alongside each kernel change.

### Evidence

- `device`: [UniXNU menu on the KB2000 panel listing the payload entry, keys working, full firmware and DarwinBoot log readable.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>fastboot flash boot_a out/darwin-boot.img; fastboot reboot</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## CPU and interrupts {#cpu}

`implemented`

Components: `ArmCpu`.

### Hardware and dependencies

- Eight cores at MPIDR affinities 0x000 through 0x700
- PSCI method smc
- 19.2 MHz timebase

### Configuration

- cpus node with one cpu per affinity and state running/waiting
- timebase-frequency 0x124f800
- arm-io/gic interrupt-controller master, virtual-timer-intid 27

### Procedure

- Install the kernel entry probe with entry\_probe\_reset\_delay and a symbol to bisect
- Read the stage bars: blue is kernel entry with the MMU off, magenta is just before the MMU is on
- A reboot six seconds after the bars proves the probe point was reached
- Run scripts/patch-xnu-log-probe.py on a private kernel copy, build the payload and read the 32 flags in KebabBootLog

### Development findings

- The kernel is entered and enables the MMU once ExitBootServices is allowed to finish; the timed reset after the MMU enable proves it.
- With PE\_create\_console replaced by a ret, the kernel completes PE\_init\_printf, PE\_init\_cpu, machine\_conf, vm\_mem\_bootstrap, console\_init, sched\_init and kernel\_thread\_create, and reaches the bootstrap thread with its first context switch.
- \_machine\_startup is four instructions and tail-branches into \_kernel\_bootstrap, so a probe placed after that call can never fire; earlier readings of it measured nothing.
- \_arm\_init calls PE\_init\_platform twice and kernel\_startup\_initialize\_upto is called once per init stage, so a probe needs entry\_probe\_reset\_occurrence when the call appears more than once.
- The kernel clears the screen when it creates the console, so a black panel after the stage bars does not mean the kernel died.
- The kernel removes its identity map in arm\_vm\_init and the physical aperture starts at gPhysBase. The ring is below both window configurations' kernel base, so the stub uses ml\_phys\_write\_byte, which maps the unmanaged page through the uncached CPU copy window instead of phystokv.
- A call-site stub can save x0-x8 and LR, call ml\_phys\_write\_byte, restore and tail-call the original callee, so arguments and return address are unchanged.
- The 32 calls between PE\_init\_iokit and the first bsd\_init locks name the gap: the last set byte is the call entered, and the next unset byte is the callee that did not return.
- The 4 GiB window loads the kernel at physical 0x20007C000 \(base A, measured by a temporary DarwinBoot colour band\). A one-stub BSS write at 0x7B281B0, dc cvac and PSCI reset reboots, but the next reader still sees no prefix, so normal DRAM does not carry the flag across the reset; pram 0x17F00FE0 is the persistent page.

### Completion criteria

- Every enabled CPU reaches its entry checkpoint and timer and IPI handling works on each enabled CPU.

### Current limits

- Only the boot CPU runs; secondary cores are not started and \_machine\_conf does not complete.

### Next action

- Confirm the pram one-stub at 0x17F00FE0 \(out/prammap-boot.img\), then move the 32 checkpoint bytes from the ring to pram and read last/next in KebabBootLog.

### Evidence

- `device`: [Reboot six seconds after the stage bars with the probe after the MMU enable; no reboot with the probe after \_machine\_conf, so the kernel reached \_machine\_startup and stopped inside it.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>flash the reset-probe image and observe the panel</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.
- `source`: [Resolves 32 call sites, verifies every original bl, places 80-byte stubs in \_\_TEXT\_EXEC padding and checks each branch.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/scripts/patch-xnu-log-probe.py)

## Memory and DMA {#memory}

`implemented`

Components: `ArmDTPlatform`.

### Hardware and dependencies

- DRAM banks 0x80000000+0x39900000, 0xC0000000+0xC0000000 and 0x200000000+0x200000000
- DarwinBoot window request of 3 GiB

### Configuration

- dram-base, dram-size and darwinboot-memory-size all 0xC0000000
- /memory node in dts/kebab.dts restricted to the 0xC0000000 window
- DarwinBoot skips conventional-memory runs below 0x40000000

### Procedure

- Build the payload so the generated ADT carries the window, then boot and read the DarwinBoot log line with the kernel virtual and physical base

### Development findings

- The kernel first-fit allocated at 0x7D000 and faulted during relocation; loading above 0x40000000 moved it to DRAM.
- A 256 MiB window leaves too little room in arm\_vm\_init's early I/O page-table reservation for the 0x2300000-byte framebuffer region; only after the window became 3 GiB did the kernel reach the MMU enable and \_machine\_startup.
- The physical aperture starts at gPhysBase and maps upward, so the 0xB9000000 ring has no phystokv mapping. ml\_phys\_write\_data falls back to the uncached CPU copy window for this unmanaged page, which is how the stage-1 stub writes it.
- The 4 GiB test window put the kernel physical base at 0x20007C000, measured by a temporary DarwinBoot colour band; the 3 GiB build's 0xC007C000 is not the current local base.
- Normal DRAM does not carry the checkpoint flags across the reset: the 0xB9000010 ring, 0xC0001000/0x200001000 and a real \_\_DATA BSS page all lose the byte after a reboot. The pram page at 0x17F00FE0 is the reset-persistent page; UEFI must map it before it can read the flags.

### Completion criteria

- All handoff regions are accounted for and each enabled DMA engine uses the documented cache policy.

### Current limits

- Only 3 GiB of the 12 GiB is handed to the kernel; no DMA or SMMU work is done yet.

### Next action

- Widen the window or describe all three banks once the platform expert completes.

### Evidence

- `device`: [DarwinBoot reports the kernel at physical 0xC007C000 inside the 3 GiB window, and the kernel reaches the MMU enable.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>flash the entry-probe image and read the DarwinBoot physical base line</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## Storage and root {#storage}

`planned`

### Hardware and dependencies

- UFS host 0x1D84000
- userdata first\_lba 4748320, last\_lba 61748338 at 4096-byte blocks

### Configuration

- UFSDxe removed from the firmware because the vendor driver asserts; ConfigurationMapLib sets EnableUfsIOC = 1, which removes the assert but still yields no block device

### Procedure

- Not started; the kernel has no storage driver yet

### Development findings

- The vendor UEFI UFS driver asserts in UFS.c and must stay out of the firmware image.
- With no block device, the firmware cannot expose the root volume and the kernel cannot mount anything.
- DXE.inc includes ScmDxeLA and SdccDxe, comments out UFSDxe and TzDxeLA. The phone has no SD slot, so SDCC does not replace UFS; the missing UFS block device is still what blocks userdata/root.

### Completion criteria

- The root volume mounts read-only from the OS window and Data is persistent.

### Current limits

- No storage driver in firmware or kernel; userdata geometry is recorded but unverified against this handset.

### Next action

- Decide between a native UFS driver and a RAM root image for the first userspace milestone.

### Evidence

Collect evidence using the next action.

## Display {#display}

`device-verified`

Components: `ArmDTPlatform`.

### Hardware and dependencies

- Samsung AMB655X panel oplus20828samsung\_amb655x\_1080\_2400\_cmd
- Framebuffer base 0x9C000000, 1080x2400, row 4320 bytes, 32 bpp

### Configuration

- PcdFrameBufferWidth 1080, PcdFrameBufferHeight 2400, PcdFrameBufferColorDepth 32
- DarwinEnter paints the hand-off band from the GOP base and flushes it from the data cache

### Procedure

- Read the DarwinBoot video line, then confirm the panel shows text at the expected coordinates

### Development findings

- The stride used while drawing a glyph row must remain \(FB\_WIDTH - FONT\_WIDTH \* FontScale\) \* FB\_BPP; using a full scanline there shears every glyph.
- The framebuffer is a DSC command-mode panel; the firmware console renders correctly with the inherited GOP.

### Completion criteria

- The kernel presents the boot logo and later the SpringBoard frame through the controller or GOP.

### Current limits

- Only the firmware text console is verified; the kernel has no display driver, so kernel output is limited to the entry probe bands.

### Next action

- Add a kernel-side framebuffer console or the native display driver once \_machine\_conf completes.

### Evidence

- `device`: [Menu, firmware log and DarwinBoot log render at 1080x2400 with correct geometry.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>boot the firmware and read the panel</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## Input {#input}

`device-verified`

### Hardware and dependencies

- Synaptics S3908 touch controller on i2c 0xa94000, address 0x4B, reset GPIO 38, IRQ GPIO 39
- Power and volume keys

### Configuration

- Volume and power keys drive the UniXNU menu

### Procedure

- Move the selection with the volume keys and confirm with the power key

### Development findings

- The keys navigate the menu and start the payload, which is enough to drive the bring-up without touch.

### Completion criteria

- Touch reports contacts under the kernel and the menu is usable without the physical keys.

### Current limits

- Only the UEFI key path is verified; touch has no driver in firmware or kernel.

### Next action

- Port the S3908 driver once the kernel reaches the driver-matching stage.

### Evidence

- `device`: [Menu selection and confirmation work with the physical keys.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>boot the firmware and press volume and power</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## GPU {#gpu}

`planned`

### Hardware and dependencies

- Adreno 650 with GMU firmware from the phone's own Android partitions

### Configuration

- No GPU driver is built or loaded

### Procedure

- Not started

### Development findings

- No GPU work has been attempted; the bring-up stops before IOKit matching.

### Completion criteria

- A Metal or OpenGL ES surface is presented through the available scanout path.

### Current limits

- No GPU firmware is collected and no driver component exists.

### Next action

- Collect the Adreno firmware after the platform expert completes.

### Evidence

Collect evidence using the next action.

## Network {#network}

`planned`

### Hardware and dependencies

- WCN6855 Wi-Fi and Bluetooth combo

### Configuration

- No network driver is built or loaded

### Procedure

- Not started

### Development findings

- No network work has been attempted.

### Completion criteria

- Association, DHCP and packet transfer work under the kernel.

### Current limits

- Firmware, calibration data and driver are all missing.

### Next action

- Defer until userspace starts.

### Evidence

Collect evidence using the next action.

## Audio {#audio}

`planned`

### Hardware and dependencies

- WCD9385 codec and the ADSP firmware

### Configuration

- No audio driver is built or loaded

### Procedure

- Not started

### Development findings

- No audio work has been attempted.

### Completion criteria

- Playback and capture work through the DSP.

### Current limits

- DSP firmware and driver are missing.

### Next action

- Defer until userspace starts.

### Evidence

Collect evidence using the next action.

## Power {#power}

`planned`

### Hardware and dependencies

- PM8150 and PM8150B PMICs
- APSS watchdog at 0x17C10008

### Configuration

- SEC disables the APSS watchdog so a fault leaves the panel readable

### Procedure

- Not started beyond the watchdog disable

### Development findings

- Without the watchdog disable a fault reboots the phone before the log can be read.

### Completion criteria

- Regulators, clocks and battery reporting match the board.

### Current limits

- No PMIC driver; only the watchdog is handled.

### Next action

- Defer until the kernel reaches the platform driver stage.

### Evidence

Collect evidence using the next action.

## Image assembly {#packaging}

`device-verified`

Components: `XnuPrelink`, `DarwinBoot`.

### Hardware and dependencies

- XNU built from source on a macOS runner against kernelcache f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7
- iPadOS 17.7.11 21H461 userland image
- 64 MiB payload FAT image

### Configuration

- header\_version 2, pagesize 4096, kernel\_offset 0x8000, tags\_offset 0x100, dtb section from the live device tree
- payload as a raw FDF region at 0x00300000, published by KebabRamDisk over BlockIo
- kernel args -v keepsyms=1 kt\_fixed\_zones=120

### Procedure

- ./unixnu build payload, then wrap the FAT image in an MBR container, point the FDF FILE region at it, rebuild the firmware and run ./unixnu bootimg

### Development findings

- The payload cannot be an FFS file, and RamDiskDxe refused the buffer with EFI\_INVALID\_PARAMETER, so KebabRamDisk publishes the region as a BlockIo device with a device path for PartitionDxe.
- unixnu bootimg needs the patched mkbootimg call that passes the DTB for header version 2.
- The payload builder now injects the device tree nodes XNU looks up by property, and scripts/patch-func-ret.py replaces a single kernel function with a ret when one call has to be skipped without a kernel rebuild.
- The exported firmware patch keeps every edited firmware file, including the two kebab drivers and the BootShim, so the tree can be rebuilt from the patch alone.

### Completion criteria

- The boot image loads, the menu appears and the kernel is entered.

### Current limits

- The payload image must be rebuilt whenever the ADT node injection changes.

### Next action

- Fold the ADT node injection into the payload step so the wrapper step disappears.

### Evidence

- `device`: [The image boots to the menu, the payload volume mounts and the kernel entry bar appears after the DarwinBoot hand-off.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>fastboot flash boot_a out/probe-final-boot.img; fastboot reboot</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## Diagnostics {#diagnostics}

`device-verified`

Components: `DarwinBoot`.

### Hardware and dependencies

- Panel as the only output
- kernel entry probe and timed reset from patch-xnu-entry-probe.py

### Configuration

- DEBUG output routed to the framebuffer console
- entry\_probe\_reset\_delay with entry\_probe\_reset\_after and entry\_probe\_reset\_in selects the bisect point

### Procedure

- Build the payload with the probe, flash, boot and read the panel
- A reboot after the delay means the probe point was reached

### Development findings

- Stage bars mark kernel entry and the point just before the MMU is on; a timed reset is the only signal available once the kernel runs without a console.
- A probe registered after the exit-boot-services notifications proved those notifications never returned; skipping them made the hand-off complete.
- Probes can mislead: an inline-assembly probe hardcoded to an identity mapping crashes after arm\_vm\_init closes that mapping and looks like a kernel hang.
- A checkpoint ring at 0xB9000000 and a BSS probe were tried. A one-stub BSS write + dc cvac + PSCI reset reboots, but the next reader still reads no prefix, so normal DRAM does not carry the flag across the reset; the 0xB9000010, 0xC0001000 and 0x200001000 probes behaved the same way.
- pram 0x17F00FE0 is the panic-log page that survives the reset. UEFI leaves it unmapped; a reader that adds it to the GCD and sets it uncached with gDS-&gt;AddMemorySpace and gDS-&gt;SetMemorySpaceAttributes was built, together with a one-stub ml\_phys\_write\_byte\(0x17F00FE0,1\) + reset image \(out/prammap-boot.img\).
- Do not clear the flag page from the firmware: pram may be read-only to UEFI and the kernel clears its own BSS. The prefix check must accept only the value 1, not any non-zero byte.

### Completion criteria

- A failed boot leaves enough on the panel or over a transport to identify the failed stage.

### Current limits

- No USB or storage log transport yet; the RAM ring did not survive the reset in the 2026-10-28 test; the pram reader mapping is the current work.

### Next action

- Boot out/prammap-boot.img and read the pram one-stub: if it reports last=0, move the 32 checkpoint bytes to pram.

### Evidence

- `device`: [Seventeen exit-boot-services notifications, a hung notification phase and a passing hand-off after skipping them; kernel entry, MMU enable and \_machine\_startup all confirmed by bars and timed resets.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>flash each bisect image and photograph the panel</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

</div>
