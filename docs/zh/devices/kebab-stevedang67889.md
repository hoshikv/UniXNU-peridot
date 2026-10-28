# OnePlus 8T \(stevedang67889\)

[stevedang67889/UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab)

<div v-pre>

OnePlus 8T \(KB2000\). Firmware, menu and payload volume work. The kernel runs the whole of kernel\_bootstrap, starts the scheduler and its first thread, and with PE\_init\_iokit, initialize\_screen, kperf\_init\_early, ml\_wait\_max\_cpus and platform\_stall\_panic\_or\_spin skipped it stops after PE\_init\_iokit and before bsd\_init. A 32-checkpoint flag log is implemented, but the DRAM flag page does not survive the reset; the pram mailbox is the next measurement.

源记录: [porting.toml](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/porting.toml), `5ba9d471c59a893ef40868190d73d88edb1af453`.

本页按设备记录自动生成。

| 部分 | 状态 |
| --- | --- |
| [配置](#configuration) | `implemented` |
| [固件与启动](#firmware) | `device-verified` |
| [CPU 与中断](#cpu) | `implemented` |
| [内存与 DMA](#memory) | `implemented` |
| [存储与根卷](#storage) | `planned` |
| [显示](#display) | `device-verified` |
| [输入](#input) | `device-verified` |
| [GPU](#gpu) | `planned` |
| [网络](#network) | `planned` |
| [音频](#audio) | `planned` |
| [电源](#power) | `planned` |
| [镜像组装](#packaging) | `device-verified` |
| [诊断](#diagnostics) | `device-verified` |

## 配置 {#configuration}

`implemented`

组件: `ArmDTPlatform`, `ArmCpu`, `SoftwareAES`, `XnuPrelink`, `DarwinBoot`.

### 硬件与依赖

- UniXNU format 2 device repository
- UniXNU-sm8250-common SoC feed pinned in feeds.toml
- iPad7,1 17.7.11 21H461 donor IPSW

### 配置

- board.toml schema\_version = 2
- components include ArmDTPlatform, ArmCpu, SoftwareAES, XnuPrelink and DarwinBoot
- kernel.prelink slide -0x1000000 against kernelcache f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7
- kernel archives add build-arm-driver.py ArmDTPlatform and ArmCpu

### 操作步骤

- Clone UniXNU, UniXNU-kebab and UniXNU-sm8250-common as siblings
- Run ./unixnu fetch and ./unixnu defconfig kebab\_defconfig
- ./unixnu status --json reports the resolved pins

### 开发经验

- The format 2 builder exports an arm-io child only for device tree nodes that carry a unixnu,adt-name property, so an untagged tree yields an empty arm-io and the kernel has no interrupt controller.
- The boot image has to use the phone own device tree; the tree shipped with Mu-Silicium lacks cont\_splash\_region@9c000000 and ABL then keeps the panel dark.
- Two inputs are not in any repository and must be captured per handset: the live device tree from /sys/firmware/fdt and the kernel built on a macOS runner. proprietary-files.txt names both with their hashes and consumers.
- Every change to a file an area owns has to update that area findings or evidence in the same commit; the catalog workflow fails otherwise and names the path.
- The device summary is what the generated page shows first, so it has to name the current stage rather than the first blocker found.
- The README status table names the current PE\_init\_iokit to bsd\_init gap and links the 32-checkpoint capture in docs/log-capture.md.

### 完成条件

- Every source pin resolves and the selected components have records; unixnu status reports the intended overrides.

### 当前范围

- No device-side repository record was published before this file; the entries below are the first complete record.

### 下一步

- Keep the record current as firmware and kernel patches land.

### 证据

- `source`: [Format 2 board description with the SM8250 SoC feed and the five components above.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/board.toml)
- `source`: [Thirteen areas with measured facts; python3 scripts/porting-record.py check --device ../UniXNU-kebab passes.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/porting.toml)

## 固件与启动 {#firmware}

`device-verified`

组件: `DarwinBoot`.

### 硬件与依赖

- Mu-Silicium Platforms/OnePlus/kebabPkg
- Phone's own live device tree as the boot image DTB
- boot\_a, 96 MiB, header v2 with 4096-byte pages

### 配置

- FD\_BASE 0x9FC00000, FD\_SIZE 0x4600000 \(3 MiB firmware volume plus a raw payload region at 0x00300000\)
- UFSDxe removed from DXE.inc; SEC disables the APSS watchdog at 0x17C10008
- ScmDxeLA and SdccDxe are included; TzDxeLA is commented out
- Framebuffer PCDs 1080x2400x32
- unixnu bootimg passes the live DTB; ABL refuses images without the splash region node

### 操作步骤

- Build firmware with ./unixnu build firmware -D ../UniXNU-kebab --uefi &lt;Mu-Silicium&gt;
- ./unixnu bootimg -D ../UniXNU-kebab --uefi &lt;Mu-Silicium&gt; --fd &lt;SILICIUM\_UEFI.fd&gt; --dtb &lt;live.dtb&gt; --ramdisk none
- fastboot flash boot\_a &lt;image&gt;; fastboot reboot

### 开发经验

- ABL hands the framebuffer over only when the boot image carries the phone's own device tree; the tree shipped with Mu-Silicium has no cont\_splash\_region@9c000000 and the panel stays black.
- A payload file inside the firmware volume is impossible: the FFS size field is 24 bits, so the 64 MiB payload is a raw FDF region instead.
- FrameBufferSerialPortLib handled newline and carriage return but ignored form feed, so DarwinBoot's clear screen did nothing and the left half of every log line was cut off.
- The payload builder injects the nodes XNU looks up by property \(gic with interrupt-controller master, a one-cell redistributor stride, virtual-timer-intid, a timer node, pram and the panic log size\); without them the kernel stops before PE\_init\_platform returns.
- KebabBootLog reads the 32 checkpoint bytes at 0xB9000010, prints the set list and clears the flags; CacheMaintenanceLib flushes the clear.
- The current firmware has SCM and SDCC but no UFS block device: UFSDxe is removed for the vendor assert, TzDxeLA is disabled and SdccDxe does not replace UFS.

### 完成条件

- Firmware boots from boot\_a, the menu is rendered on the panel, the keys navigate it and the log is readable.

### 当前范围

- USB is not brought up in the firmware; the only log transport is the panel.

### 下一步

- Publish a firmware image revision alongside each kernel change.

### 证据

- `device`: [UniXNU menu on the KB2000 panel listing the payload entry, keys working, full firmware and DarwinBoot log readable.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>fastboot flash boot_a out/darwin-boot.img; fastboot reboot</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## CPU 与中断 {#cpu}

`implemented`

组件: `ArmCpu`.

### 硬件与依赖

- Eight cores at MPIDR affinities 0x000 through 0x700
- PSCI method smc
- 19.2 MHz timebase

### 配置

- cpus node with one cpu per affinity and state running/waiting
- timebase-frequency 0x124f800
- arm-io/gic interrupt-controller master, virtual-timer-intid 27

### 操作步骤

- Install the kernel entry probe with entry\_probe\_reset\_delay and a symbol to bisect
- Read the stage bars: blue is kernel entry with the MMU off, magenta is just before the MMU is on
- A reboot six seconds after the bars proves the probe point was reached
- Run scripts/patch-xnu-log-probe.py on a private kernel copy, build the payload and read the 32 flags in KebabBootLog

### 开发经验

- The kernel is entered and enables the MMU once ExitBootServices is allowed to finish; the timed reset after the MMU enable proves it.
- With PE\_create\_console replaced by a ret, the kernel completes PE\_init\_printf, PE\_init\_cpu, machine\_conf, vm\_mem\_bootstrap, console\_init, sched\_init and kernel\_thread\_create, and reaches the bootstrap thread with its first context switch.
- \_machine\_startup is four instructions and tail-branches into \_kernel\_bootstrap, so a probe placed after that call can never fire; earlier readings of it measured nothing.
- \_arm\_init calls PE\_init\_platform twice and kernel\_startup\_initialize\_upto is called once per init stage, so a probe needs entry\_probe\_reset\_occurrence when the call appears more than once.
- The kernel clears the screen when it creates the console, so a black panel after the stage bars does not mean the kernel died.
- The kernel removes its identity map in arm\_vm\_init and the physical aperture starts at gPhysBase. The ring is below both window configurations' kernel base, so the stub uses ml\_phys\_write\_byte, which maps the unmanaged page through the uncached CPU copy window instead of phystokv.
- A call-site stub can save x0-x8 and LR, call ml\_phys\_write\_byte, restore and tail-call the original callee, so arguments and return address are unchanged.
- The 32 calls between PE\_init\_iokit and the first bsd\_init locks name the gap: the last set byte is the call entered, and the next unset byte is the callee that did not return.
- The 4 GiB window loads the kernel at physical 0x20007C000 \(base A, measured by a temporary DarwinBoot colour band\). A one-stub BSS write at 0x7B281B0, dc cvac and PSCI reset reboots, but the next reader still sees no prefix, so normal DRAM does not carry the flag across the reset; pram 0x17F00FE0 is the persistent page.

### 完成条件

- Every enabled CPU reaches its entry checkpoint and timer and IPI handling works on each enabled CPU.

### 当前范围

- Only the boot CPU runs; secondary cores are not started and \_machine\_conf does not complete.

### 下一步

- Confirm the pram one-stub at 0x17F00FE0 \(out/prammap-boot.img\), then move the 32 checkpoint bytes from the ring to pram and read last/next in KebabBootLog.

### 证据

- `device`: [Reboot six seconds after the stage bars with the probe after the MMU enable; no reboot with the probe after \_machine\_conf, so the kernel reached \_machine\_startup and stopped inside it.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>flash the reset-probe image and observe the panel</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.
- `source`: [Resolves 32 call sites, verifies every original bl, places 80-byte stubs in \_\_TEXT\_EXEC padding and checks each branch.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/scripts/patch-xnu-log-probe.py)

## 内存与 DMA {#memory}

`implemented`

组件: `ArmDTPlatform`.

### 硬件与依赖

- DRAM banks 0x80000000+0x39900000, 0xC0000000+0xC0000000 and 0x200000000+0x200000000
- DarwinBoot window request of 3 GiB

### 配置

- dram-base, dram-size and darwinboot-memory-size all 0xC0000000
- /memory node in dts/kebab.dts restricted to the 0xC0000000 window
- DarwinBoot skips conventional-memory runs below 0x40000000

### 操作步骤

- Build the payload so the generated ADT carries the window, then boot and read the DarwinBoot log line with the kernel virtual and physical base

### 开发经验

- The kernel first-fit allocated at 0x7D000 and faulted during relocation; loading above 0x40000000 moved it to DRAM.
- A 256 MiB window leaves too little room in arm\_vm\_init's early I/O page-table reservation for the 0x2300000-byte framebuffer region; only after the window became 3 GiB did the kernel reach the MMU enable and \_machine\_startup.
- The physical aperture starts at gPhysBase and maps upward, so the 0xB9000000 ring has no phystokv mapping. ml\_phys\_write\_data falls back to the uncached CPU copy window for this unmanaged page, which is how the stage-1 stub writes it.
- The 4 GiB test window put the kernel physical base at 0x20007C000, measured by a temporary DarwinBoot colour band; the 3 GiB build's 0xC007C000 is not the current local base.
- Normal DRAM does not carry the checkpoint flags across the reset: the 0xB9000010 ring, 0xC0001000/0x200001000 and a real \_\_DATA BSS page all lose the byte after a reboot. The pram page at 0x17F00FE0 is the reset-persistent page; UEFI must map it before it can read the flags.

### 完成条件

- All handoff regions are accounted for and each enabled DMA engine uses the documented cache policy.

### 当前范围

- Only 3 GiB of the 12 GiB is handed to the kernel; no DMA or SMMU work is done yet.

### 下一步

- Widen the window or describe all three banks once the platform expert completes.

### 证据

- `device`: [DarwinBoot reports the kernel at physical 0xC007C000 inside the 3 GiB window, and the kernel reaches the MMU enable.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>flash the entry-probe image and read the DarwinBoot physical base line</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## 存储与根卷 {#storage}

`planned`

### 硬件与依赖

- UFS host 0x1D84000
- userdata first\_lba 4748320, last\_lba 61748338 at 4096-byte blocks

### 配置

- UFSDxe removed from the firmware because the vendor driver asserts; ConfigurationMapLib sets EnableUfsIOC = 1, which removes the assert but still yields no block device

### 操作步骤

- Not started; the kernel has no storage driver yet

### 开发经验

- The vendor UEFI UFS driver asserts in UFS.c and must stay out of the firmware image.
- With no block device, the firmware cannot expose the root volume and the kernel cannot mount anything.
- DXE.inc includes ScmDxeLA and SdccDxe, comments out UFSDxe and TzDxeLA. The phone has no SD slot, so SDCC does not replace UFS; the missing UFS block device is still what blocks userdata/root.

### 完成条件

- The root volume mounts read-only from the OS window and Data is persistent.

### 当前范围

- No storage driver in firmware or kernel; userdata geometry is recorded but unverified against this handset.

### 下一步

- Decide between a native UFS driver and a RAM root image for the first userspace milestone.

### 证据

按下一步操作收集证据。

## 显示 {#display}

`device-verified`

组件: `ArmDTPlatform`.

### 硬件与依赖

- Samsung AMB655X panel oplus20828samsung\_amb655x\_1080\_2400\_cmd
- Framebuffer base 0x9C000000, 1080x2400, row 4320 bytes, 32 bpp

### 配置

- PcdFrameBufferWidth 1080, PcdFrameBufferHeight 2400, PcdFrameBufferColorDepth 32
- DarwinEnter paints the hand-off band from the GOP base and flushes it from the data cache

### 操作步骤

- Read the DarwinBoot video line, then confirm the panel shows text at the expected coordinates

### 开发经验

- The stride used while drawing a glyph row must remain \(FB\_WIDTH - FONT\_WIDTH \* FontScale\) \* FB\_BPP; using a full scanline there shears every glyph.
- The framebuffer is a DSC command-mode panel; the firmware console renders correctly with the inherited GOP.

### 完成条件

- The kernel presents the boot logo and later the SpringBoard frame through the controller or GOP.

### 当前范围

- Only the firmware text console is verified; the kernel has no display driver, so kernel output is limited to the entry probe bands.

### 下一步

- Add a kernel-side framebuffer console or the native display driver once \_machine\_conf completes.

### 证据

- `device`: [Menu, firmware log and DarwinBoot log render at 1080x2400 with correct geometry.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>boot the firmware and read the panel</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## 输入 {#input}

`device-verified`

### 硬件与依赖

- Synaptics S3908 touch controller on i2c 0xa94000, address 0x4B, reset GPIO 38, IRQ GPIO 39
- Power and volume keys

### 配置

- Volume and power keys drive the UniXNU menu

### 操作步骤

- Move the selection with the volume keys and confirm with the power key

### 开发经验

- The keys navigate the menu and start the payload, which is enough to drive the bring-up without touch.

### 完成条件

- Touch reports contacts under the kernel and the menu is usable without the physical keys.

### 当前范围

- Only the UEFI key path is verified; touch has no driver in firmware or kernel.

### 下一步

- Port the S3908 driver once the kernel reaches the driver-matching stage.

### 证据

- `device`: [Menu selection and confirmation work with the physical keys.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>boot the firmware and press volume and power</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## GPU {#gpu}

`planned`

### 硬件与依赖

- Adreno 650 with GMU firmware from the phone's own Android partitions

### 配置

- No GPU driver is built or loaded

### 操作步骤

- Not started

### 开发经验

- No GPU work has been attempted; the bring-up stops before IOKit matching.

### 完成条件

- A Metal or OpenGL ES surface is presented through the available scanout path.

### 当前范围

- No GPU firmware is collected and no driver component exists.

### 下一步

- Collect the Adreno firmware after the platform expert completes.

### 证据

按下一步操作收集证据。

## 网络 {#network}

`planned`

### 硬件与依赖

- WCN6855 Wi-Fi and Bluetooth combo

### 配置

- No network driver is built or loaded

### 操作步骤

- Not started

### 开发经验

- No network work has been attempted.

### 完成条件

- Association, DHCP and packet transfer work under the kernel.

### 当前范围

- Firmware, calibration data and driver are all missing.

### 下一步

- Defer until userspace starts.

### 证据

按下一步操作收集证据。

## 音频 {#audio}

`planned`

### 硬件与依赖

- WCD9385 codec and the ADSP firmware

### 配置

- No audio driver is built or loaded

### 操作步骤

- Not started

### 开发经验

- No audio work has been attempted.

### 完成条件

- Playback and capture work through the DSP.

### 当前范围

- DSP firmware and driver are missing.

### 下一步

- Defer until userspace starts.

### 证据

按下一步操作收集证据。

## 电源 {#power}

`planned`

### 硬件与依赖

- PM8150 and PM8150B PMICs
- APSS watchdog at 0x17C10008

### 配置

- SEC disables the APSS watchdog so a fault leaves the panel readable

### 操作步骤

- Not started beyond the watchdog disable

### 开发经验

- Without the watchdog disable a fault reboots the phone before the log can be read.

### 完成条件

- Regulators, clocks and battery reporting match the board.

### 当前范围

- No PMIC driver; only the watchdog is handled.

### 下一步

- Defer until the kernel reaches the platform driver stage.

### 证据

按下一步操作收集证据。

## 镜像组装 {#packaging}

`device-verified`

组件: `XnuPrelink`, `DarwinBoot`.

### 硬件与依赖

- XNU built from source on a macOS runner against kernelcache f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7
- iPadOS 17.7.11 21H461 userland image
- 64 MiB payload FAT image

### 配置

- header\_version 2, pagesize 4096, kernel\_offset 0x8000, tags\_offset 0x100, dtb section from the live device tree
- payload as a raw FDF region at 0x00300000, published by KebabRamDisk over BlockIo
- kernel args -v keepsyms=1 kt\_fixed\_zones=120

### 操作步骤

- ./unixnu build payload, then wrap the FAT image in an MBR container, point the FDF FILE region at it, rebuild the firmware and run ./unixnu bootimg

### 开发经验

- The payload cannot be an FFS file, and RamDiskDxe refused the buffer with EFI\_INVALID\_PARAMETER, so KebabRamDisk publishes the region as a BlockIo device with a device path for PartitionDxe.
- unixnu bootimg needs the patched mkbootimg call that passes the DTB for header version 2.
- The payload builder now injects the device tree nodes XNU looks up by property, and scripts/patch-func-ret.py replaces a single kernel function with a ret when one call has to be skipped without a kernel rebuild.
- The exported firmware patch keeps every edited firmware file, including the two kebab drivers and the BootShim, so the tree can be rebuilt from the patch alone.

### 完成条件

- The boot image loads, the menu appears and the kernel is entered.

### 当前范围

- The payload image must be rebuilt whenever the ADT node injection changes.

### 下一步

- Fold the ADT node injection into the payload step so the wrapper step disappears.

### 证据

- `device`: [The image boots to the menu, the payload volume mounts and the kernel entry bar appears after the DarwinBoot hand-off.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>fastboot flash boot_a out/probe-final-boot.img; fastboot reboot</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

## 诊断 {#diagnostics}

`device-verified`

组件: `DarwinBoot`.

### 硬件与依赖

- Panel as the only output
- kernel entry probe and timed reset from patch-xnu-entry-probe.py

### 配置

- DEBUG output routed to the framebuffer console
- entry\_probe\_reset\_delay with entry\_probe\_reset\_after and entry\_probe\_reset\_in selects the bisect point

### 操作步骤

- Build the payload with the probe, flash, boot and read the panel
- A reboot after the delay means the probe point was reached

### 开发经验

- Stage bars mark kernel entry and the point just before the MMU is on; a timed reset is the only signal available once the kernel runs without a console.
- A probe registered after the exit-boot-services notifications proved those notifications never returned; skipping them made the hand-off complete.
- Probes can mislead: an inline-assembly probe hardcoded to an identity mapping crashes after arm\_vm\_init closes that mapping and looks like a kernel hang.
- A checkpoint ring at 0xB9000000 and a BSS probe were tried. A one-stub BSS write + dc cvac + PSCI reset reboots, but the next reader still reads no prefix, so normal DRAM does not carry the flag across the reset; the 0xB9000010, 0xC0001000 and 0x200001000 probes behaved the same way.
- pram 0x17F00FE0 is the panic-log page that survives the reset. UEFI leaves it unmapped; a reader that adds it to the GCD and sets it uncached with gDS-&gt;AddMemorySpace and gDS-&gt;SetMemorySpaceAttributes was built, together with a one-stub ml\_phys\_write\_byte\(0x17F00FE0,1\) + reset image \(out/prammap-boot.img\).
- Do not clear the flag page from the firmware: pram may be read-only to UEFI and the kernel clears its own BSS. The prefix check must accept only the value 1, not any non-zero byte.

### 完成条件

- A failed boot leaves enough on the panel or over a transport to identify the failed stage.

### 当前范围

- No USB or storage log transport yet; the RAM ring did not survive the reset in the 2026-10-28 test; the pram reader mapping is the current work.

### 下一步

- Boot out/prammap-boot.img and read the pram one-stub: if it reports last=0, move the 32 checkpoint bytes to pram.

### 证据

- `device`: [Seventeen exit-boot-services notifications, a hung notification phase and a passing hand-off after skipping them; kernel entry, MMU enable and \_machine\_startup all confirmed by bars and timed resets.](https://github.com/stevedang67889/UniXNU-kebab/blob/5ba9d471c59a893ef40868190d73d88edb1af453/docs/bring-up.md)
  Command: <code>flash each bisect image and photograph the panel</code>. Revision: `97f6fea12dae49a1bbc30ccd22c098b82b97dbe4`.

</div>
