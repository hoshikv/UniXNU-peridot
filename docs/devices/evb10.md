# Rockchip RK3588 EVB10 V10

[Ken-u/UniXNU-evb10](https://github.com/Ken-u/UniXNU-evb10)

<div v-pre>

Rockchip RK3588 EVB10 V10 runs the iPadOS 17.7.11 userland on a source-built XNU, started from edk2-rk3588 firmware \(uefi-evb10\). Shared SoC support is in UniXNU-rk3588-common; this repository owns the memory layout, the eMMC root, the VOP2 display, touch, the Mali-G610 integration and the packaging. Areas in scope are recorded as implemented: the results below were observed with the previous repository layout; a rebuild of everything from these repositories was booted on 2026-10-29 \(see docs/reproduce.md\), without a look at the panel or touch.

Source record: [porting.toml](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/porting.toml), `d14b8f389b39d3e5fca1430108f91aa969d08534`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `implemented` |
| [Firmware and boot](#firmware) | `implemented` |
| [CPU and interrupts](#cpu) | `implemented` |
| [Memory and DMA](#memory) | `implemented` |
| [Storage and root](#storage) | `implemented` |
| [Display](#display) | `implemented` |
| [Input](#input) | `implemented` |
| [GPU](#gpu) | `implemented` |
| [Network](#network) | `not-applicable` |
| [Audio](#audio) | `not-applicable` |
| [Power](#power) | `not-applicable` |
| [Image assembly](#packaging) | `implemented` |
| [Diagnostics](#diagnostics) | `implemented` |

## Configuration {#configuration}

`implemented`

### Hardware and dependencies

- ARM64 profile, rk3588 SoC source pinned in feeds.toml, iPadOS 17.7.11 build 21H461 profile, evb10\_defconfig and evb10\_display\_defconfig, submodules package/Mali and package/Rk3588Emmc.

### Configuration

- board.soc\_source = rk3588; board.os\_profile = ipados-17.7.11-21H461; schema\_version = 2.
- kernel.config\_fragments = arch common, soc rk3588, os ipados; the defconfig applies last and switches off what the OS profile enables for other hardware \(NATIVE\_ADRENO, SOURCE\_MOCKFS\).
- kernel.patch\_series = arch, soc, os, device in that order.

### Procedure

- From the UniXNU checkout run ./unixnu defconfig evb10\_defconfig -D ../UniXNU-evb10, then ./unixnu fetch and ./unixnu status --json with the same -D.
- For a local SoC checkout copy local.toml.example to local.toml.
- Run scripts/build-evb10-kernel.sh for the two-pass kernel build.

### Development findings

- The \[adt.defaults.u32\] table of board.toml must carry sleep-disabled = 1 and vmm-present = 1; the OS profile only supplies kern.maxvnodes. vmm-present backs kern.hv\_vmm\_present: without it watchdogd traps with SIGTRAP three times and the kernel panics when the third instance exits \(seen in a rebuild from a clean tree, fixed in the clean-directory rerun\).
- Arrays replace inherited values, so board.toml lists every archive, fragment and trust-cache digest it needs.
- The display archive is made from the kernel of an earlier display-less pass, which is why two defconfigs exist and why the build script runs them in order.
- A device script named like a UniXNU script shadows it in the build tree; the board driver builder is therefore named build-evb10-drivers.py and does not replace build-arm-driver.py.

### Completion criteria

- Every source pin resolves, ./unixnu status lists the intended patch order, and both defconfigs materialise.

### Current limits

- The format 2 configuration has been materialised and checked with status and tree; a full kernel build with it is recorded under Image assembly.

### Next action

- Pin the Mali and Rk3588Emmc submodules to released commits when they change.

### Evidence

- `source`: [Implementation and configuration described in this section](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/board.toml)

## Firmware and boot {#firmware}

`implemented`

### Hardware and dependencies

- edk2-rk3588 firmware built by uefi-evb10 \(SPI NOR image\), TF-A at EL3 with PSCI over SMC, UEFI GOP frame buffer and the Mali CSF firmware listed in proprietary-files.txt.

### Configuration

- board.toml has no \[firmware\] section: the unixnu firmware step builds Mu-Silicium, and this board boots edk2-rk3588 built by uefi-evb10.
- payload.loader = firmware: DarwinBoot is inside the firmware volume.
- proprietary-files.txt pins mali\_csffw.bin by SHA-256; scripts/extract-files.py places it in .cache/firmware.

### Procedure

- Build the firmware in uefi-evb10 \(build.sh\) and flash or boot it as that repository describes.
- Run scripts/extract-files.py SOURCE, then the kernel step; scripts/build-mali-firmware.py writes package/Mali/mali\_csffw.inc.

### Development findings

- The firmware hands the kernel EL1 with the MMU off and PSCI on SMC; secondary cores started by CPU\_ON arrive at EL2 and the SoC patch drops them to EL1.
- The firmware leaves the eMMC in HS400 and UART2 at 1.5 Mbaud; the drivers rely on both.

### Completion criteria

- Firmware enters DarwinBoot, publishes the GOP frame buffer and keeps the memory reservations of the board.

### Current limits

- Firmware and its boot flow are documented in uefi-evb10; this repository only pins the inputs.

### Next action

- Record the firmware commit next to each image verification.

### Evidence

- `source`: [Firmware inputs and their digests](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/proprietary-files.txt)

## CPU and interrupts {#cpu}

`implemented`

Components: `ArmCpu`, `ArmDTPlatform`.

### Hardware and dependencies

- Four Cortex-A55 and four Cortex-A76 at affinities 0x000 to 0x700, GICv3 at 0xfe600000 with redistributors at 0xfe680000 \(128 KiB stride\), 24 MHz architected timer, PSCI over SMC.

### Configuration

- The CPU, GIC and PSCI nodes come from the SoC dtsi \(rk3588-common dts/rk3588.dtsi\); the board DTS enables nothing extra.
- Kernel arguments sheng-smp=1, armcpu-self-ipi=1 and armcpu-mmio-sgi=0 select the secondary-core and IPI paths.

### Procedure

- Compile dts/evb10.dts through ./unixnu tree and inspect /cpus, /psci and /arm-io/intc in the generated device tree.
- Boot and read the per-core entry lines on the serial console.

### Development findings

- Inline assembly in this target goes through the Darwin assembler, where ';' starts a comment: "dsb ishst; msr icc\_sgi1r\_el1, %0; isb" assembled to a lone dsb. Earlier conclusions that the GIC ignores ICC\_SGI1R, that the A55 CNTFRQ is zero and that counters differ per core all came from this mistake and were withdrawn. The SoC repository carries the ArmCpu fix; scripts/build-evb10-kernel.sh refuses new inline assembly with ';'.
- With the fix all eight cores run and inter-processor interrupts go through ICC\_SGI1R\_EL1.

### Completion criteria

- Every enabled core reaches its entry checkpoint and receives timer and inter-processor interrupts.

### Current limits

- CPU frequency is not managed: cores run at the clock the firmware left. No cluster-type properties are set, so the scheduler sees one cluster type.

### Next action

- Add CPU frequency control and decide whether to describe the A55 and A76 clusters to the scheduler.

### Evidence

- `source`: [Board device tree on top of the shared SoC description](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/dts/evb10.dts)

## Memory and DMA {#memory}

`implemented`

### Hardware and dependencies

- DRAM bank 0x1\_00000000 with size 0x1\_00000000 in the firmware memory map, 2 GiB owned window for the kernel, eMMC controller with 32-bit DMA.

### Configuration

- dts/evb10.dts declares the memory node; payload.dram\_size must equal it.
- payload.adt.owned\_memory\_size = 0x80000000: DarwinBoot allocates this contiguous window from the low bank.

### Procedure

- Compare the firmware memory map, dts/evb10.dts and the owned window reported by DarwinBoot.
- Check DMA buffers of the eMMC driver against the 4 GiB address limit.

### Development findings

- The 4 GiB bank above 4 GiB is the one large region without TF-A, OP-TEE, firmware or Rockchip carve-outs.
- A 256 MiB window left too little memory for the iPadOS userland; 2 GiB fits the firmware's low bank, 3 GiB was refused with Out of Resources. All pages sit below 4 GiB, where the eMMC DMA reaches.
- No IOMMU is used for the eMMC controller; the driver allocates a bounce window below 4 GiB.

### Completion criteria

- All handoff regions are accounted for and every DMA engine works inside its allocated range.

### Current limits

- Memory above the 2 GiB window is unused.

### Next action

- Test a larger window by placing the kernel window above 4 GiB.

### Evidence

- `source`: [Memory node and handoff window](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/dts/evb10.dts)

## Storage and root {#storage}

`implemented`

Components: `AppleKeyStoreState`.

### Hardware and dependencies

- DesignWare MSHC at 0xfe2e0000, 8-bit eMMC, GPT with userdata at LBA 8459264 to 122142655 \(512-byte blocks\), SHNGROOT manifest image written by unixnu build userdata.

### Configuration

- userdata.first\_lba, last\_lba and windows \(OS, SystemOS\) describe the root image.
- Kernel arguments: emmc=4, emmc-root-trust=1, emmc-systemos-union=1, emmc-data-mount=1, emmc-data-size=2048, emmc-data-persist=1.
- payload.adt.root\_match\_volume\_uuid names the OS volume; packed\_data\_ramdisk = false.

### Procedure

- Build with build-evb10-drivers.py Rk3588Emmc followed by bind-emmc-storage.py; regenerate scripts/data/evb10-storage-interface-audit.json with audit-evb10-storage.py when the kernelcache pin changes.
- scripts/peek-root-image.py reads the manifest of a userdata image without a board.

### Development findings

- The driver subclasses the original IOBlockStorageDevice, so its archive is made by binding the object to the original code at audited addresses.
- Only the last 2049 MiB of userdata, the Data window, is ever written: the first start formats it and the kernel copies /private/var once, after which restarts mount it. Writes showed zero failures in the observed runs.
- A 256 MiB Data volume ran out of space \(ENOSPC\) and stalled SpringBoard; 512 MiB was enough to reach the lock screen, 2048 MiB is the configured size. vnode jetsam killed daemons until /defaults/kern.maxvnodes was set to 66560, which the OS profile now provides.

### Completion criteria

- The root volume mounts read-only from the OS window and the Data volume mounts read-write across restarts.

### Current limits

- Only eMMC is supported; PCIe NVMe is not used. The kernel of the format 2 layout builds, but a payload from it has not been written to the board.

### Next action

- Assemble a payload from the rebuilt kernel and repeat the persistence check across restarts.

### Evidence

- `source`: [Binding of the eMMC block device to the original storage class](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/scripts/bind-emmc-storage.py)

## Display {#display}

`implemented`

Components: `GopFramebuffer`.

### Hardware and dependencies

- VOP2 with the Esmart3 window on the DSI panel, 1080x1920; UEFI GOP frame buffer; original IOMobileFramebufferLegacy bound to NativeLegacyDisplay.

### Configuration

- Kernel arguments evb10-vop=2, evb10-vop-es2=1, evb10-fb-w=1072, evb10-fb-h=1904, offline-legacy-\* and sheng-vsync=60.
- evb10\_display\_defconfig links the display archive and the link stubs.

### Procedure

- Build both passes with scripts/build-evb10-kernel.sh and boot with a monitor or panel attached.

### Development findings

- The firmware assigns Esmart3 to VP2; writing back the layer-mixer policy of U-Boot makes the window take effect.
- Underruns \(POST\_BUF\_EMPTY\) on the panel path came from the firmware's HDMI0 window \(Esmart2\) fetching at the same time; disabling it with evb10-vop-es2=1 gave zero underruns.
- A frame size of exactly 1080x1920 makes backboardd render the 1242x2208 geometry of other devices and ask for scaling, which the full-frame swap validation rejects; 1072x1904 avoids it.
- Tearing in scanout was removed by waiting for the GPU jobs of a frame before the page flip.

### Completion criteria

- The panel shows the lock screen without flicker or tearing.

### Current limits

- Frame rate while scrolling is limited by the GPU frame time. The panel timing is the firmware's.

### Next action

- Shorten the GPU frame time \(see GPU\) and move presentation to vsync interrupts.

### Evidence

- `source`: [VOP2 scanout presenter](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/package/Evb10Display/NativeLegacyDisplay.cpp)

## Input {#input}

`implemented`

### Hardware and dependencies

- Goodix GT1158 on I2C5, polled; coordinate grid 720x1280 on a 1080x1920 panel.

### Configuration

- Kernel arguments evb10-touch=1, evb10-touch-xmax=720, evb10-touch-ymax=1280, evb10-hid=1, evb10-hid-input=1, sheng-touch-hid=1.

### Procedure

- Boot and watch the evb10: touch lines on the console; touch the panel and check the pointer in SpringBoard.

### Development findings

- The controller reports a configured 800x480 but delivers X 9..696 and Y 10..1270: the real grid is 720x1280.
- The input device is one HID device on the IOHIDResource; the original IOHIDEventDriver attaches to it and feeds backboardd.

### Completion criteria

- Touch coordinates map to the panel and SpringBoard reacts.

### Current limits

- No keyboard, buttons or pen; the controller is polled every 10 ms.

### Next action

- Use the controller interrupt instead of polling.

### Evidence

- `source`: [I2C master and touch controller driver](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/package/Evb10Touch/Evb10Touch.cpp)

## GPU {#gpu}

`implemented`

### Hardware and dependencies

- Mali-G610 \(Valhall CSF\) at 0xfb000000, CSF firmware from proprietary-files.txt, MaliMetal user-space stack \(PanVK, metal2vulkan, Metal plug-in\).

### Configuration

- Kernel arguments mali=8 and mali-metal=1; evb10-gpu-addon=1 makes the payload tool carry the Metal plug-in.
- package/Mali is a submodule; scripts/build-mali-firmware.py generates the firmware include.

### Procedure

- Provide the firmware, build the kernel, then build the user-space stack from MaliMetal and the add-on it describes.

### Development findings

- The stack composites the iPadOS lock screen on the Mali: backboardd, AGXAccelerator, NativeMaliMetal, metal2vulkan, PanVK and the Mali driver.
- PanVK counts a colour attachment with write mask 0 as written; a Mesa patch in MaliMetal fixes the black rectangle behind the lock screen clock.
- GPU time per frame is about 100 ms while scrolling \(8 to 10 frames per second\); the clock is fixed at 594 MHz.

### Completion criteria

- The lock screen and its animations render on the Mali at a steady frame rate.

### Current limits

- No DVFS, no interrupt-driven completion \(waits poll\), at most eight groups; frame time is the open item.

### Next action

- Profile the frame time of a scroll and move completion to the GPU interrupts.

### Evidence

- `source`: [Mali driver source is the package/Mali submodule](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/.gitmodules)

## Network {#network}

`not-applicable`

### Hardware and dependencies

- No network driver is part of this port.

### Configuration

- None.

### Procedure

- None.

### Development findings

- The board's Ethernet and Wi-Fi controllers are not used by the userland.

### Completion criteria

- Out of scope.

### Current limits

- Network is outside the current scope of the board.

### Next action

- Choose a network device when the scope extends.

### Evidence

Collect evidence using the next action.

## Audio {#audio}

`not-applicable`

### Hardware and dependencies

- No audio driver is part of this port.

### Configuration

- None.

### Procedure

- None.

### Development findings

- The userland runs without an audio device.

### Completion criteria

- Out of scope.

### Current limits

- Audio is outside the current scope of the board.

### Next action

- Choose a codec path when the scope extends.

### Evidence

Collect evidence using the next action.

## Power {#power}

`not-applicable`

### Hardware and dependencies

- The board is mains powered.

### Configuration

- The ADT sets sleep-disabled through the OS profile defaults.

### Procedure

- None.

### Development findings

- Suspend and resume are not implemented. The GPU runs at a fixed clock and the CPUs at the firmware clock.

### Completion criteria

- Out of scope.

### Current limits

- No battery, regulator or clock management beyond what the drivers need to start.

### Next action

- Add CPU and GPU frequency control.

### Evidence

Collect evidence using the next action.

## Image assembly {#packaging}

`implemented`

Components: `XnuPrelink`, `DarwinBoot`.

### Hardware and dependencies

- XNU of the OS profile, ARM64, SoC and device patch series, generated Apple device tree, payload volume of 96 MiB, owner trust-cache digests.

### Configuration

- payload.\* in board.toml and the \[adt\] tables; the device tree is generated from dts/evb10.dts.
- payload.kernel\_args lists the boot arguments grouped by purpose.

### Procedure

- Run scripts/build-evb10-kernel.sh, then ./unixnu build payload with --unique-chip-id.

### Development findings

- A rebuild from the six pushed repositories in an empty directory \(docs/reproduce.md\) reaches launchd with about 110 processes and Mali-composited frames; the feeds.toml pin of the SoC repository has to include the dts change that stops exporting interrupts on the eMMC and UART nodes, because an exported interrupts property without AAPL,phandle makes IOKit start-up take a data abort. The second trust\_cache\_sha256 entry changes with every rebuild because the user-space binaries are not bit-for-bit reproducible.
- The device series must not touch the context of the hunks of earlier series: the userland-mounts patch is generated by patches/tools/make-patches.py from a tree that has the earlier series applied.
- The Mali driver carries weak fallbacks for the display driver's GPU hand-off, because the display-less pass links it too \(kernel patches call its test hooks\); the display archive's strong definitions replace them.
- Each defconfig name is its own output directory \(out/evb10/&lt;os&gt;/evb10 and .../evb10\_display\), so the display scripts read the display-less kernel from the sibling directory.
- The payload rides in a boot image through U-Boot's fastboot, whose download buffer is 112 MiB; 96 MiB of payload fits, 128 MiB was refused.

### Completion criteria

- The payload boots on the board to the lock screen.

### Current limits

- The two-pass kernel build of this layout completed on Linux \(evb10\_defconfig, then evb10\_display\_defconfig; kernel.macho 51.7 MB, sha256 56cac3bc9d5384005ec2b2919d6ce37eb8e6a72832568b51c0f6eb784dadf210\). A payload was not assembled from it and the image was last booted with the previous layout.

### Next action

- Boot an image built from this layout.

### Evidence

- `source`: [Device patch order](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/patches/xnu/series)

## Diagnostics {#diagnostics}

`implemented`

### Hardware and dependencies

- UART2 serial console at 1.5 Mbaud; DEVELOPMENT kernel.

### Configuration

- Kernel arguments -v, serial=3, vc\_mirror=1, debug=0x10014e, -enable\_kprintf\_spam, evb10-ps=10, evb10-bt, evb10-bt2, evb10-bt3, evb10-bt4.

### Procedure

- Read the serial console. evb10-ps prints the process table, evb10-bt prints user stacks of the named processes for symbolication on the host.

### Development findings

- Console lines longer than about 255 characters lose their tail.
- The serial input of XNU is dead on this board, so there is no command channel; judge the kernel's progress from the serial output only.

### Completion criteria

- Every boot stage prints a checkpoint to the serial console.

### Current limits

- Panics reset the board after the text is out; no panic screen is drawn.

### Next action

- Add a panic screen on the frame buffer.

### Evidence

- `source`: [Process exit trace](https://github.com/Ken-u/UniXNU-evb10/blob/d14b8f389b39d3e5fca1430108f91aa969d08534/patches/xnu/evb10-exit-trace.patch)

</div>
