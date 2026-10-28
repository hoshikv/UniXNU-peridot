/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef PERIDOT_TOUCH_BOARD_H
#define PERIDOT_TOUCH_BOARD_H

#include <stdint.h>

/* peridot.dts: spi@a90000 on the QUPv3 wrapper at 0xa80000, GENI SE4. */
#define TB_QUP_WRAPPER_BASE 0x00a80000ULL
#define TB_SE_BASE          0x00a90000ULL
#define TB_SE_SIZE          0x4000ULL

/* clock-controller@100000, gcc-sm8650.c: QUPv3 wrapper 1 branches and SE4 RCG. */
#define TB_GCC_BASE          0x00100000ULL
#define TB_GCC_SIZE          0x60000ULL
#define TB_GCC_VOTE_ENABLE   0x52008            /* enable_reg of the QUP branches, BIT(18..21) and BIT(26) */
#define TB_GCC_GPLL0_ENABLE  0x52020
#define TB_GCC_GPLL0_MODE    0x00000
#define TB_VOTE_CORE_2X      (1u << 18)         /* gcc_qupv3_wrap1_core_2x_clk, halt 0x23154 */
#define TB_VOTE_CORE         (1u << 19)         /* gcc_qupv3_wrap1_core_clk, halt 0x23144 */
#define TB_VOTE_M_AHB        (1u << 20)         /* gcc_qupv3_wrap_1_m_ahb_clk, halt 0x2313c */
#define TB_VOTE_S_AHB        (1u << 21)         /* gcc_qupv3_wrap_1_s_ahb_clk, halt 0x23140 */
#define TB_VOTE_SE4          (1u << 26)         /* gcc_qupv3_wrap1_s4_clk, halt 0x183bc */
#define TB_VOTE_ALL          (TB_VOTE_CORE_2X | TB_VOTE_CORE | TB_VOTE_M_AHB | TB_VOTE_S_AHB | TB_VOTE_SE4)
#define TB_HALT_CORE_2X      0x23154
#define TB_HALT_CORE         0x23144
#define TB_HALT_M_AHB        0x2313c
#define TB_HALT_S_AHB        0x23140
#define TB_HALT_SE4          0x183bc
#define TB_CBCR_CLK_OFF      (1u << 31)

/* gcc_qupv3_wrap1_s4_clk_src: cmd_rcgr 0x183c8, the clk-rcg2 DFS layout. */
#define TB_S4_RCG            0x183c8
#define TB_RCG_CMD           0x0
#define TB_RCG_CFG           0x4
#define TB_RCG_DFSR_CMD      0x14
#define TB_RCG_PERF_CFG(l)   (0x1c + 4 * (l))
#define TB_RCG_PERF_M(l)     (0x5c + 4 * (l))
#define TB_RCG_PERF_N(l)     (0x9c + 4 * (l))
#define TB_DFS_LEVELS        10

/* pinctrl@f100000: ctl_reg = 0x1000 * gpio, io_reg = +4, mux bit 2, drive bit 6, output bit 9. */
#define TB_TLMM_BASE     0x0f100000ULL
#define TB_GPIO_STRIDE   0x1000
#define TB_GPIO_MISO     16
#define TB_GPIO_MOSI     17
#define TB_GPIO_CLK      18
#define TB_GPIO_CS       19
#define TB_GPIO_RESET    128
#define TB_GPIO_INT      129
#define TB_FUNC_QUP0_SE4 1
#define TB_DRIVE_6MA     2

/* The touch node gives 1/16-pixel units: 1220 x 16 and 2712 x 16. */
#define TB_TOUCH_MAX_X   19520
#define TB_TOUCH_MAX_Y   43400
#define TB_TOUCH_SLOTS   10
#define TB_SPI_MODE      0
#define TB_SPI_DEFAULT_HZ 1000000u
#define TB_SPI_MAX_HZ    10000000u

/* RPMh RSC v3.0 (Cliffs) and the command database, from hk_goodix.c and rpmh-rsc.c. */
#define TB_RSC_BASE      0x17a20000ULL
#define TB_RSC_SIZE      0x10000ULL
#define TB_CMD_DB_BASE   0x81c60000ULL
#define TB_CMD_DB_SIZE   0x20000ULL

static inline uint32_t tb_pin_cfg(unsigned func, unsigned drive_code)
{
    return (func & 15u) << 2 | (drive_code & 7u) << 6;
}

static inline uint32_t tb_tlmm_ctl(unsigned gpio)
{
    return (uint32_t)gpio * TB_GPIO_STRIDE;
}

/* gcc_parent_map_0: 0 = TCXO, 1 = GPLL0 main (600 MHz), 6 = GPLL0 even (300 MHz). */
static inline uint64_t tb_src_hz(unsigned sel)
{
    return sel == 0 ? 19200000ULL : sel == 1 ? 600000000ULL : sel == 6 ? 300000000ULL : 0;
}

/* clk-rcg2 rate: half-integer divider, then M/N (N stored as ~(N-M)). */
static inline uint64_t tb_rcg_rate(uint32_t cfg, uint32_t mreg, uint32_t nreg)
{
    unsigned src = (cfg >> 8) & 7, mode = (cfg >> 12) & 3, hid = cfg & 0x1f;
    uint64_t hz = tb_src_hz(src);
    uint32_t m = 0, n = 0;

    if (hid) {
        hz = hz * 2 / (hid + 1);
    }
    if (mode) {
        m = mreg & 0xffff;
        n = ((~nreg) & 0xffff) + m;
        if (n) {
            hz = hz * m / n;
        }
    }
    return hz;
}

#endif
