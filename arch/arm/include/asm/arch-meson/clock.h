/*
 * Copyright 2017 - Beniamino Galvani <b.galvani@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef _MESON_CLOCK_H_
#define _MESON_CLOCK_H_

/* CBUS clock measure registers */
#define MSR_CLK_DUTY		0xc1108758
#define MSR_CLK_REG0		0xc110875c
#define MSR_CLK_REG1		0xc1108760
#define MSR_CLK_REG2		0xc1108764

#define CLK_GP0_PLL		4
#define CLK_GP1_PLL		5
#define CLK_81			7
#define CLK_MMC			23
#define CLK_MOD_ETH_TX		40
#define CLK_MOD_ETH_RX_RMII	41
#define CLK_FCLK_DIV5		43
#define CLK_SD_EMMC_CLK_C	51
#define CLK_SD_EMMC_CLK_B	52

/* Clock gates */
#define HHI_GCLK_MPEG0		0x140
#define HHI_GCLK_MPEG1		0x144
#define HHI_GCLK_MPEG2		0x148
#define HHI_GCLK_OTHER		0x150
#define HHI_GCLK_AO		0x154

ulong meson_measure_clk_rate(unsigned int clk);

#endif
