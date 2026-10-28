// SPDX-License-Identifier: BSD-2-Clause-Patent
// Default-off peridot touch bring-up: GENI SE4 SPI, Goodix Berlin-D, RPMh supplies, TLMM pins.
// `peridot-touch=<n>`: 1 power and pins, 2 clocks, 3 GENI identity and chip probe, 4 report processing.
// `peridot-touch-hz=N`, `peridot-touch-flip=N` (1 mirror x, 2 mirror y), `peridot-touch-ms=N`.
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <string.h>
#include <libkern/c++/OSSharedPtr.h>
#include <kern/thread.h>
#include <pexpert/pexpert.h>
extern "C" {
#include "GeniSpi.h"
#include "GoodixBerlin.h"
#include "TouchBoard.h"
}

extern "C" {
volatile uint32_t native_touch_seq, native_touch_count, native_touch_stage_req, native_touch_stage_done,
    native_touch_frames, native_touch_valid;
volatile int native_touch_error;
volatile int native_touch_last_x = -1, native_touch_last_y = -1;
volatile int native_touch_xs[TB_TOUCH_SLOTS], native_touch_ys[TB_TOUCH_SLOTS], native_touch_ids[TB_TOUCH_SLOTS];
}

namespace {

struct Region {
    OSSharedPtr<IOMemoryMap> keep;
    uintptr_t va = 0;
};

bool mapRegion(uint64_t phys, uint64_t length, Region &region)
{
    IODeviceMemory *memory = IODeviceMemory::withRange(phys, length);

    if (!memory) {
        return false;
    }
    region.keep = OSSharedPtr<IOMemoryMap>(memory->map(kIOMapAnywhere | kIOMapInhibitCache), OSNoRetain);
    memory->release();
    region.va = region.keep ? static_cast<uintptr_t>(region.keep->getVirtualAddress()) : 0;
    return region.va != 0;
}

uint32_t rd32(uintptr_t base, uint32_t reg)
{
    return *reinterpret_cast<volatile uint32_t *>(base + reg);
}

void wr32(uintptr_t base, uint32_t reg, uint32_t value)
{
    *reinterpret_cast<volatile uint32_t *>(base + reg) = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

Region gGcc, gTlmm, gWrapper, gSe, gRsc, gCmdDb;
uintptr_t gcc, tlmm, se, rsc, cmd_db;
struct geni_spi gSpi;
struct goodix_berlin_chip gChip;
uint32_t gSeSourceHz, gSeClockSelect;
unsigned gFlip;
void (*gTouchSink)(void *);
void *gTouchSinkContext;

constexpr uint32_t kGccVote = TB_GCC_VOTE_ENABLE;
constexpr uint32_t kGccGpll0Enable = TB_GCC_GPLL0_ENABLE;
constexpr uint32_t kGccGpll0Mode = TB_GCC_GPLL0_MODE;
constexpr uint32_t kS4Rcg = TB_S4_RCG;

uint32_t ioRd(void *ctx, uint32_t reg) { return rd32(reinterpret_cast<uintptr_t>(ctx), reg); }
void ioWr(void *ctx, uint32_t reg, uint32_t value) { wr32(reinterpret_cast<uintptr_t>(ctx), reg, value); }
void ioDelayUs(void *, unsigned us) { IODelay(us); }

void gpioConfigure(unsigned pin, unsigned function, unsigned pull, unsigned ma, bool output)
{
    wr32(tlmm, tb_tlmm_ctl(pin), pull | function << 2 | (ma / 2 - 1) << 6 | (output ? 1u << 9 : 0));
}

void gpioSet(unsigned pin, bool high) { wr32(tlmm, tb_tlmm_ctl(pin) + 4, high ? 2 : 0); }

// ---- RPMh (RSC v3.0) -----------------------------------------------------------------------------

#define TCS_ID                  0
#define TCS(reg)                (rsc + 0xd00 + 672 * TCS_ID + (reg))
#define TCS_COMMON(reg)         (rsc + 0xd00 + (reg))
#define TCS_IRQ_ENABLE          0x00
#define TCS_IRQ_STATUS          0x04
#define TCS_IRQ_CLEAR           0x08
#define TCS_WAIT_FOR_COMPLETION 0x20
#define TCS_CONTROL             0x24
#define TCS_CONTROL_ENABLE      (1u << 16)
#define TCS_CONTROL_TRIGGER     (1u << 24)
#define TCS_CMD_ENABLE          0x2c
#define TCS_CMD_MSGID(n)        (0x34 + 24 * (n))
#define TCS_CMD_ADDR(n)         (TCS_CMD_MSGID(n) + 4)
#define TCS_CMD_DATA(n)         (TCS_CMD_MSGID(n) + 8)
#define MSGID_WRITE_4_BYTES     0x10008
#define MSGID_RESPONSE          0x100
#define VRM_VOLTAGE             0
#define VRM_ENABLE              4
#define VRM_MODE                8
#define VRM_MODE_LDO_HPM        7

struct cmd_db_slave {
    uint16_t id, header_offset, data_offset, count, version, reserved[3];
};
struct cmd_db_header {
    uint32_t version;
    uint8_t magic[4];
    struct cmd_db_slave slave[8];
    uint32_t checksum, reserved;
};
struct cmd_db_entry {
    char id[8];
    uint32_t priority[2];
    uint32_t address;
    uint16_t length, offset;
};

uint32_t rpmhAddress(const char *name)
{
    const struct cmd_db_header *db = reinterpret_cast<const struct cmd_db_header *>(cmd_db);
    const uint8_t *data = reinterpret_cast<const uint8_t *>(db + 1);
    char id[8] = {};

    if (db->magic[0] != 0xdb || db->magic[1] != 0x30 || db->magic[2] != 0x03 || db->magic[3] != 0x0c) {
        printf("peridot-touch: no command database (magic %02x %02x %02x %02x)\n", db->magic[0], db->magic[1],
            db->magic[2], db->magic[3]);
        return 0;
    }
    for (unsigned i = 0; i < 7 && name[i]; i++) {
        id[i] = name[i];
    }
    for (unsigned s = 0; s < 8; s++) {
        const struct cmd_db_slave *slave = &db->slave[s];
        const struct cmd_db_entry *entry;

        if (slave->id == 0 || slave->header_offset >= TB_CMD_DB_SIZE - 0x1000 || slave->count > 512) {
            continue;
        }
        entry = reinterpret_cast<const struct cmd_db_entry *>(data + slave->header_offset);
        for (unsigned i = 0; i < slave->count; i++) {
            if (memcmp(entry[i].id, id, sizeof(id)) == 0) {
                return entry[i].address;
            }
        }
    }
    return 0;
}

bool rpmhWait(uint32_t reg, uint32_t mask, uint32_t value, const char *what)
{
    for (int i = 0; i < 2000; i++) {
        if ((rd32(TCS_COMMON(reg), 0) & mask) == value) {
            return true;
        }
        IODelay(100);
    }
    printf("peridot-touch: RPMh timeout waiting for %s (register %#x = %#x)\n", what, reg, rd32(TCS_COMMON(reg), 0));
    return false;
}

bool rpmhWrite(const uint32_t *address, const uint32_t *data, unsigned count)
{
    uint32_t irqEnable = rd32(TCS_COMMON(TCS_IRQ_ENABLE), 0);
    uint32_t irqBit = 1u << TCS_ID;
    bool done;

    wr32(TCS(TCS_CMD_ENABLE), 0, 0);
    wr32(TCS(TCS_WAIT_FOR_COMPLETION), 0, 0);
    wr32(TCS(TCS_CONTROL), 0, rd32(TCS(TCS_CONTROL), 0) & ~TCS_CONTROL_TRIGGER);
    wr32(TCS(TCS_CONTROL), 0, rd32(TCS(TCS_CONTROL), 0) & ~TCS_CONTROL_ENABLE);
    wr32(TCS_COMMON(TCS_IRQ_CLEAR), 0, irqBit);
    for (unsigned i = 0; i < count; i++) {
        wr32(TCS(TCS_CMD_MSGID(i)), 0, MSGID_WRITE_4_BYTES | MSGID_RESPONSE);
        wr32(TCS(TCS_CMD_ADDR(i)), 0, address[i]);
        wr32(TCS(TCS_CMD_DATA(i)), 0, data[i]);
    }
    wr32(TCS(TCS_WAIT_FOR_COMPLETION), 0, (1u << count) - 1);
    wr32(TCS(TCS_CMD_ENABLE), 0, (1u << count) - 1);
    wr32(TCS_COMMON(TCS_IRQ_ENABLE), 0, irqEnable | irqBit);
    wr32(TCS(TCS_CONTROL), 0, TCS_CONTROL_ENABLE);
    wr32(TCS(TCS_CONTROL), 0, TCS_CONTROL_ENABLE | TCS_CONTROL_TRIGGER);
    done = rpmhWait(TCS_IRQ_STATUS, irqBit, irqBit, "the request");
    wr32(TCS(TCS_CONTROL), 0, rd32(TCS(TCS_CONTROL), 0) & ~TCS_CONTROL_TRIGGER);
    wr32(TCS(TCS_CONTROL), 0, rd32(TCS(TCS_CONTROL), 0) & ~TCS_CONTROL_ENABLE);
    wr32(TCS(TCS_CMD_ENABLE), 0, 0);
    wr32(TCS(TCS_WAIT_FOR_COMPLETION), 0, 0);
    wr32(TCS_COMMON(TCS_IRQ_CLEAR), 0, irqBit);
    wr32(TCS_COMMON(TCS_IRQ_ENABLE), 0, irqEnable);
    return done;
}

bool regulatorOn(const char *name, uint32_t millivolts)
{
    uint32_t base = rpmhAddress(name);
    uint32_t address[3] = { base + VRM_VOLTAGE, base + VRM_ENABLE, base + VRM_MODE };
    uint32_t data[3] = { millivolts, 1, VRM_MODE_LDO_HPM };

    if (base == 0 || (base >> 16 & 0xf) != 4) {
        printf("peridot-touch: RPMh has no regulator \"%s\" (address %#x)\n", name, base);
        return false;
    }
    if (!rpmhWrite(address, data, 3)) {
        return false;
    }
    printf("peridot-touch: %s on (%u mV)\n", name, millivolts);
    return true;
}

bool rpmhMaps()
{
    if (!rsc && !mapRegion(TB_RSC_BASE, TB_RSC_SIZE, gRsc)) {
        return false;
    }
    if (!cmd_db && !mapRegion(TB_CMD_DB_BASE, TB_CMD_DB_SIZE, gCmdDb)) {
        return false;
    }
    rsc = gRsc.va;
    cmd_db = gCmdDb.va;
    return rsc != 0 && cmd_db != 0;
}

// ---- board power and clocks ---------------------------------------------------------------------

bool powerOn()
{
    gpioSet(TB_GPIO_RESET, false);
    gpioConfigure(TB_GPIO_RESET, 0, 3, 8, true);
    gpioConfigure(TB_GPIO_INT, 0, 3, 8, false);
    if (!rpmhMaps()) {
        printf("peridot-touch: cannot map the RPMh\n");
        return false;
    }
    if (!regulatorOn("ldob23", 1800)) {
        return false;
    }
    IODelay(3000);
    if (!regulatorOn("ldob13", 3300)) {
        return false;
    }
    IODelay(10000);
    gpioSet(TB_GPIO_RESET, false);
    IODelay(15000);
    gpioSet(TB_GPIO_RESET, true);
    IODelay(100000);
    return true;
}

bool clocksOn()
{
    wr32(gcc, kGccGpll0Enable, rd32(gcc, kGccGpll0Enable) | 1u);
    for (int i = 0; i < 1000 && !(rd32(gcc, kGccGpll0Mode) & (1u << 31)); i++) {
        IODelay(10);
    }
    wr32(gcc, kGccVote, rd32(gcc, kGccVote) | TB_VOTE_CORE_2X | TB_VOTE_CORE | TB_VOTE_M_AHB | TB_VOTE_S_AHB);
    if (rd32(gcc, kS4Rcg + TB_RCG_DFSR_CMD) & 1u) {
        for (uint32_t level = 0; level < TB_DFS_LEVELS; level++) {
            uint32_t cfg = rd32(gcc, kS4Rcg + TB_RCG_PERF_CFG(level));
            uint32_t m = rd32(gcc, kS4Rcg + TB_RCG_PERF_M(level));
            uint32_t n = rd32(gcc, kS4Rcg + TB_RCG_PERF_N(level));
            uint64_t hz = tb_rcg_rate(cfg, m, n);

            if (hz >= 4000000 && level < 8 && (gSeSourceHz == 0 || hz < gSeSourceHz)) {
                gSeSourceHz = static_cast<uint32_t>(hz);
                gSeClockSelect = level;
            }
        }
        if (gSeSourceHz == 0) {
            printf("peridot-touch: the SE4 DFS table has no usable rate\n");
            return false;
        }
    } else {
        wr32(gcc, kS4Rcg + TB_RCG_CFG, (rd32(gcc, kS4Rcg + TB_RCG_CFG) & ~0x371fu) | 1u);
        wr32(gcc, kS4Rcg + TB_RCG_CMD, rd32(gcc, kS4Rcg + TB_RCG_CMD) | 1u);
        for (int i = 0; i < 500 && (rd32(gcc, kS4Rcg + TB_RCG_CMD) & 1u); i++) {
            IODelay(10);
        }
        if (rd32(gcc, kS4Rcg + TB_RCG_CMD) & 1u) {
            printf("peridot-touch: the SE4 clock does not take its configuration\n");
            return false;
        }
        gSeSourceHz = 19200000;
        gSeClockSelect = 0;
    }
    wr32(gcc, kGccVote, rd32(gcc, kGccVote) | TB_VOTE_SE4);
    for (int i = 0; i < 1000; i++) {
        if (!(rd32(gcc, TB_HALT_SE4) & TB_CBCR_CLK_OFF)) {
            printf("peridot-touch: SE4 source %u Hz, DFS level %u\n", gSeSourceHz, gSeClockSelect);
            return true;
        }
        IODelay(10);
    }
    printf("peridot-touch: SE4 clock stays off (%#x), wrapper %#x %#x %#x %#x\n", rd32(gcc, TB_HALT_SE4),
        rd32(gcc, TB_HALT_CORE_2X), rd32(gcc, TB_HALT_CORE), rd32(gcc, TB_HALT_M_AHB), rd32(gcc, TB_HALT_S_AHB));
    return false;
}

bool geniSetup(unsigned hz)
{
    struct geni_io seIo = { reinterpret_cast<void *>(gSe.va), ioRd, ioWr, ioDelayUs };
    struct geni_io wrapperIo = { reinterpret_cast<void *>(gWrapper.va), ioRd, ioWr, ioDelayUs };

    for (unsigned pin = TB_GPIO_MISO; pin <= TB_GPIO_CS; pin++) {
        gpioConfigure(pin, TB_FUNC_QUP0_SE4, 0, 6, false);
    }
    gSpi = {};
    gSpi.se = seIo;
    gSpi.wrapper = wrapperIo;
    gSpi.poll_limit = 400000;
    gSpi.poll_only = 1;
    int rc = geni_spi_read_info(&gSpi);

    printf("peridot-touch: GENI firmware %#x, protocol %u, FIFO disable %#x, params %#x, QUP %#x\n",
        gSpi.info.fw_revision, gSpi.info.protocol, gSpi.info.if_disable, gSpi.info.hw_param0, gSpi.info.qup_hw_version);
    if (rc) {
        printf("peridot-touch: GENI read_info failed (%d)\n", rc);
        return false;
    }
    if (gSpi.info.protocol != GENI_PROTO_SPI) {
        printf("peridot-touch: SE4 is not configured for SPI (%u)\n", gSpi.info.protocol);
        return false;
    }
    if (gSpi.info.fifo_disabled) {
        printf("peridot-touch: the SE4 FIFO interface is disabled; DMA is not implemented here\n");
        return false;
    }
    rc = geni_spi_init(&gSpi);
    if (!rc) {
        rc = geni_spi_set_clock(&gSpi, hz, gSeSourceHz, gSeClockSelect);
    }
    if (!rc) {
        rc = geni_spi_setup(&gSpi, TB_SPI_MODE, 0);
    }
    if (rc) {
        printf("peridot-touch: GENI SPI setup failed (%d)\n", rc);
        return false;
    }
    printf("peridot-touch: GENI SPI at %u Hz\n", hz);
    return true;
}

bool busTransfer(void *, const uint8_t *tx, uint8_t *rx, size_t length)
{
    return geni_spi_transfer(&gSpi, tx, rx, length, 0) == 0;
}

void busDelay(void *, unsigned ms) { IODelay(ms * 1000); }

void chipReset(void *)
{
    gpioSet(TB_GPIO_RESET, false);
    IODelay(10000);
    gpioSet(TB_GPIO_RESET, true);
    IODelay(120000);
}

struct goodix_berlin_bus gBus = {
    nullptr, busTransfer, busDelay, chipReset,
};

void publish(unsigned count, const struct goodix_berlin_contact *contacts)
{
    int xs[TB_TOUCH_SLOTS], ys[TB_TOUCH_SLOTS], ids[TB_TOUCH_SLOTS];

    for (unsigned i = 0; i < count && i < TB_TOUCH_SLOTS; i++) {
        uint32_t x = contacts[i].x, y = contacts[i].y;
        uint32_t maxX = gChip.max_x ? gChip.max_x : TB_TOUCH_MAX_X;
        uint32_t maxY = gChip.max_y ? gChip.max_y : TB_TOUCH_MAX_Y;

        x = x >= maxX ? 65535 : static_cast<uint32_t>(static_cast<uint64_t>(x) * 65535 / maxX);
        y = y >= maxY ? 65535 : static_cast<uint32_t>(static_cast<uint64_t>(y) * 65535 / maxY);
        if (gFlip & 1) {
            x = 65535 - x;
        }
        if (gFlip & 2) {
            y = 65535 - y;
        }
        xs[i] = static_cast<int>(x);
        ys[i] = static_cast<int>(y);
        ids[i] = contacts[i].id;
    }
    native_touch_seq = native_touch_seq + 1;
    __asm__ volatile("dmb ish" ::: "memory");
    native_touch_count = count;
    for (unsigned i = 0; i < count && i < TB_TOUCH_SLOTS; i++) {
        native_touch_xs[i] = xs[i];
        native_touch_ys[i] = ys[i];
        native_touch_ids[i] = ids[i];
    }
    native_touch_last_x = count ? xs[0] : -1;
    native_touch_last_y = count ? ys[0] : -1;
    __asm__ volatile("dmb ish" ::: "memory");
    native_touch_seq = native_touch_seq + 1;
    if (void (*sink)(void *) = __atomic_load_n(&gTouchSink, __ATOMIC_ACQUIRE)) {
        sink(gTouchSinkContext);
    }
}

bool probeChip()
{
    gChip = {};
    gChip.bus = &gBus;
    gChip.max_x = TB_TOUCH_MAX_X;
    gChip.max_y = TB_TOUCH_MAX_Y;
    for (int attempt = 0; attempt < 3; attempt++) {
        chipReset(nullptr);
        if (goodix_berlin_confirm(&gChip) && goodix_berlin_probe(&gChip, true)) {
            if (gChip.max_x < 4096 || gChip.max_y < 4096) {
                gChip.max_x = TB_TOUCH_MAX_X;
                gChip.max_y = TB_TOUCH_MAX_Y;
            }
            native_touch_stage_done = 3;
            return true;
        }
        printf("peridot-touch: probe %d failed\n", attempt + 1);
    }
    return false;
}

void touchThread(void *, wait_result_t)
{
    thread_set_thread_name(current_thread(), "peridot-touch");
    unsigned stage = native_touch_stage_req;
    unsigned hz = TB_SPI_DEFAULT_HZ, flip = 0, interval = 8;

    PE_parse_boot_argn("peridot-touch-hz", &hz, sizeof hz);
    PE_parse_boot_argn("peridot-touch-flip", &flip, sizeof flip);
    PE_parse_boot_argn("peridot-touch-ms", &interval, sizeof interval);
    if (hz < 100000 || hz > TB_SPI_MAX_HZ) {
        hz = TB_SPI_DEFAULT_HZ;
    }
    if (interval < 2 || interval > 100) {
        interval = 8;
    }
    gFlip = flip;
    if (!mapRegion(TB_GCC_BASE, TB_GCC_SIZE, gGcc) || !mapRegion(TB_TLMM_BASE, 0x90000, gTlmm)) {
        native_touch_error = -12;
        return;
    }
    gcc = gGcc.va;
    tlmm = gTlmm.va;
    if (!powerOn()) {
        native_touch_error = -5;
        return;
    }
    native_touch_stage_done = 1;
    if (stage < 2) {
        return;
    }
    if (!clocksOn()) {
        native_touch_error = -6;
        return;
    }
    native_touch_stage_done = 2;
    if (stage < 3) {
        return;
    }
    if (!mapRegion(TB_QUP_WRAPPER_BASE, 0x1000, gWrapper) || !mapRegion(TB_SE_BASE, TB_SE_SIZE, gSe)) {
        native_touch_error = -12;
        return;
    }
    se = gSe.va;
    if (!geniSetup(hz)) {
        native_touch_error = -7;
        return;
    }
    if (!probeChip()) {
        native_touch_error = -8;
        return;
    }
    if (stage < 4) {
        return;
    }
    unsigned lastCount = ~0u, errors = 0;
    for (;;) {
        struct goodix_berlin_contact contacts[TB_TOUCH_SLOTS] = {};
        unsigned count = 0;
        enum goodix_berlin_poll poll = goodix_berlin_poll(&gChip, contacts, &count);

        if (poll == GOODIX_BERLIN_POLL_TOUCH) {
            native_touch_valid = native_touch_valid + 1;
            native_touch_frames = native_touch_frames + 1;
            publish(count, contacts);
            lastCount = count;
            errors = 0;
        } else if (poll == GOODIX_BERLIN_POLL_IDLE) {
            if (lastCount != 0) {
                publish(0, contacts);
                lastCount = 0;
            }
            errors = 0;
        } else if (poll == GOODIX_BERLIN_POLL_RESET) {
            printf("peridot-touch: the chip asked for a reset\n");
            chipReset(nullptr);
            if (!probeChip()) {
                native_touch_error = -9;
                return;
            }
            lastCount = ~0u;
        } else if (++errors >= 8) {
            printf("peridot-touch: %u read errors; resetting\n", errors);
            chipReset(nullptr);
            if (!probeChip()) {
                native_touch_error = -10;
                return;
            }
            lastCount = ~0u;
            errors = 0;
        }
        IOSleep(interval);
    }
}

}  // namespace

extern "C" void native_touch_set_sink(void (*sink)(void *), void *context)
{
    gTouchSinkContext = context;
    __atomic_store_n(&gTouchSink, sink, __ATOMIC_RELEASE);
}

extern "C" void native_touch_probe(void)
{
    unsigned stage = 0;

    if (!PE_parse_boot_argn("peridot-touch", &stage, sizeof stage) || stage < 1 || native_touch_stage_req) {
        return;
    }
    if (stage > 4) {
        stage = 4;
    }
    native_touch_stage_req = stage;
    thread_t thread = nullptr;
    if (kernel_thread_start(touchThread, nullptr, &thread) == KERN_SUCCESS) {
        thread_deallocate(thread);
    } else {
        native_touch_error = -12;
    }
}
