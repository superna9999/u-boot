/*
 * (C) Copyright 2017 - Beniamino Galvani <b.galvani@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/clock.h>
#include <asm/io.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dt-bindings/clock/gxbb-clkc.h>

struct meson_clk {
	void __iomem *addr;
	ulong rate;
};

struct meson_gate {
	unsigned int reg;
	unsigned int bit;
};

#define MESON_GATE(id, _reg, _bit)		\
	[id] = {				\
		.reg = (_reg),			\
		.bit = (_bit),			\
	}

struct meson_gate gates[] = {
	/* Everything Else (EE) domain gates */
	MESON_GATE(CLKID_DDR, HHI_GCLK_MPEG0, 0),
	MESON_GATE(CLKID_DOS, HHI_GCLK_MPEG0, 1),
	MESON_GATE(CLKID_ISA, HHI_GCLK_MPEG0, 5),
	MESON_GATE(CLKID_PL301, HHI_GCLK_MPEG0, 6),
	MESON_GATE(CLKID_PERIPHS, HHI_GCLK_MPEG0, 7),
	MESON_GATE(CLKID_SPICC, HHI_GCLK_MPEG0, 8),
	MESON_GATE(CLKID_I2C, HHI_GCLK_MPEG0, 9),
	MESON_GATE(CLKID_SAR_ADC, HHI_GCLK_MPEG0, 10),
	MESON_GATE(CLKID_SMART_CARD, HHI_GCLK_MPEG0, 11),
	MESON_GATE(CLKID_RNG0, HHI_GCLK_MPEG0, 12),
	MESON_GATE(CLKID_UART0, HHI_GCLK_MPEG0, 13),
	MESON_GATE(CLKID_SDHC, HHI_GCLK_MPEG0, 14),
	MESON_GATE(CLKID_STREAM, HHI_GCLK_MPEG0, 15),
	MESON_GATE(CLKID_ASYNC_FIFO, HHI_GCLK_MPEG0, 16),
	MESON_GATE(CLKID_SDIO, HHI_GCLK_MPEG0, 17),
	MESON_GATE(CLKID_ABUF, HHI_GCLK_MPEG0, 18),
	MESON_GATE(CLKID_HIU_IFACE, HHI_GCLK_MPEG0, 19),
	MESON_GATE(CLKID_ASSIST_MISC, HHI_GCLK_MPEG0, 23),
	MESON_GATE(CLKID_SD_EMMC_A, HHI_GCLK_MPEG0, 24),
	MESON_GATE(CLKID_SD_EMMC_B, HHI_GCLK_MPEG0, 25),
	MESON_GATE(CLKID_SD_EMMC_C, HHI_GCLK_MPEG0, 26),
	MESON_GATE(CLKID_SPI, HHI_GCLK_MPEG0, 30),

	MESON_GATE(CLKID_I2S_SPDIF, HHI_GCLK_MPEG1, 2),
	MESON_GATE(CLKID_ETH, HHI_GCLK_MPEG1, 3),
	MESON_GATE(CLKID_DEMUX, HHI_GCLK_MPEG1, 4),
	MESON_GATE(CLKID_AIU_GLUE, HHI_GCLK_MPEG1, 6),
	MESON_GATE(CLKID_IEC958, HHI_GCLK_MPEG1, 7),
	MESON_GATE(CLKID_I2S_OUT, HHI_GCLK_MPEG1, 8),
	MESON_GATE(CLKID_AMCLK, HHI_GCLK_MPEG1, 9),
	MESON_GATE(CLKID_AIFIFO2, HHI_GCLK_MPEG1, 10),
	MESON_GATE(CLKID_MIXER, HHI_GCLK_MPEG1, 11),
	MESON_GATE(CLKID_MIXER_IFACE, HHI_GCLK_MPEG1, 12),
	MESON_GATE(CLKID_ADC, HHI_GCLK_MPEG1, 13),
	MESON_GATE(CLKID_BLKMV, HHI_GCLK_MPEG1, 14),
	MESON_GATE(CLKID_AIU, HHI_GCLK_MPEG1, 15),
	MESON_GATE(CLKID_UART1, HHI_GCLK_MPEG1, 16),
	MESON_GATE(CLKID_G2D, HHI_GCLK_MPEG1, 20),
	MESON_GATE(CLKID_USB0, HHI_GCLK_MPEG1, 21),
	MESON_GATE(CLKID_USB1, HHI_GCLK_MPEG1, 22),
	MESON_GATE(CLKID_RESET, HHI_GCLK_MPEG1, 23),
	MESON_GATE(CLKID_NAND, HHI_GCLK_MPEG1, 24),
	MESON_GATE(CLKID_DOS_PARSER, HHI_GCLK_MPEG1, 25),
	MESON_GATE(CLKID_USB, HHI_GCLK_MPEG1, 26),
	MESON_GATE(CLKID_VDIN1, HHI_GCLK_MPEG1, 28),
	MESON_GATE(CLKID_AHB_ARB0, HHI_GCLK_MPEG1, 29),
	MESON_GATE(CLKID_EFUSE, HHI_GCLK_MPEG1, 30),
	MESON_GATE(CLKID_BOOT_ROM, HHI_GCLK_MPEG1, 31),

	MESON_GATE(CLKID_AHB_DATA_BUS, HHI_GCLK_MPEG2, 1),
	MESON_GATE(CLKID_AHB_CTRL_BUS, HHI_GCLK_MPEG2, 2),
	MESON_GATE(CLKID_HDMI_INTR_SYNC, HHI_GCLK_MPEG2, 3),
	MESON_GATE(CLKID_HDMI_PCLK, HHI_GCLK_MPEG2, 4),
	MESON_GATE(CLKID_USB1_DDR_BRIDGE, HHI_GCLK_MPEG2, 8),
	MESON_GATE(CLKID_USB0_DDR_BRIDGE, HHI_GCLK_MPEG2, 9),
	MESON_GATE(CLKID_MMC_PCLK, HHI_GCLK_MPEG2, 11),
	MESON_GATE(CLKID_DVIN, HHI_GCLK_MPEG2, 12),
	MESON_GATE(CLKID_UART2, HHI_GCLK_MPEG2, 15),
	MESON_GATE(CLKID_SANA, HHI_GCLK_MPEG2, 22),
	MESON_GATE(CLKID_VPU_INTR, HHI_GCLK_MPEG2, 25),
	MESON_GATE(CLKID_SEC_AHB_AHB3_BRIDGE, HHI_GCLK_MPEG2, 26),
	MESON_GATE(CLKID_CLK81_A53, HHI_GCLK_MPEG2, 29),

	MESON_GATE(CLKID_VCLK2_VENCI0, HHI_GCLK_OTHER, 1),
	MESON_GATE(CLKID_VCLK2_VENCI1, HHI_GCLK_OTHER, 2),
	MESON_GATE(CLKID_VCLK2_VENCP0, HHI_GCLK_OTHER, 3),
	MESON_GATE(CLKID_VCLK2_VENCP1, HHI_GCLK_OTHER, 4),
	MESON_GATE(CLKID_GCLK_VENCI_INT0, HHI_GCLK_OTHER, 8),
	MESON_GATE(CLKID_DAC_CLK, HHI_GCLK_OTHER, 10),
	MESON_GATE(CLKID_AOCLK_GATE, HHI_GCLK_OTHER, 14),
	MESON_GATE(CLKID_IEC958_GATE, HHI_GCLK_OTHER, 16),
	MESON_GATE(CLKID_ENC480P, HHI_GCLK_OTHER, 20),
	MESON_GATE(CLKID_RNG1, HHI_GCLK_OTHER, 21),
	MESON_GATE(CLKID_GCLK_VENCI_INT1, HHI_GCLK_OTHER, 22),
	MESON_GATE(CLKID_VCLK2_VENCLMCC, HHI_GCLK_OTHER, 24),
	MESON_GATE(CLKID_VCLK2_VENCL, HHI_GCLK_OTHER, 25),
	MESON_GATE(CLKID_VCLK_OTHER, HHI_GCLK_OTHER, 26),
	MESON_GATE(CLKID_EDP, HHI_GCLK_OTHER, 31),

	/* Always On (AO) domain gates */
	MESON_GATE(CLKID_AO_MEDIA_CPU, HHI_GCLK_AO, 0),
	MESON_GATE(CLKID_AO_AHB_SRAM, HHI_GCLK_AO, 1),
	MESON_GATE(CLKID_AO_AHB_BUS, HHI_GCLK_AO, 2),
	MESON_GATE(CLKID_AO_IFACE, HHI_GCLK_AO, 3),
	MESON_GATE(CLKID_AO_I2C, HHI_GCLK_AO, 4),
};

static int meson_set_gate(struct clk *clk, bool on)
{
	struct meson_clk *priv = dev_get_priv(clk->dev);
	struct meson_gate *gate;

	if (clk->id >= ARRAY_SIZE(gates))
		return -ENOENT;

	gate = &gates[clk->id];

	if (gate->reg == 0)
		return -ENOENT;

	clrsetbits_le32(priv->addr + gate->reg,
			BIT(gate->bit), on ? BIT(gate->bit) : 0);
	return 0;
}

static int meson_clk_enable(struct clk *clk)
{
	return meson_set_gate(clk, true);
}

static int meson_clk_disable(struct clk *clk)
{
	return meson_set_gate(clk, false);
}

static ulong meson_clk_get_rate(struct clk *clk)
{
	struct meson_clk *priv = dev_get_priv(clk->dev);

	if (clk->id != CLKID_CLK81) {
		if (clk->id >= ARRAY_SIZE(gates))
			return -ENOENT;
		if (gates[clk->id].reg == 0)
			return -ENOENT;
	}

	/* Use cached value if available */
	if (priv->rate)
		return priv->rate;

	priv->rate = meson_measure_clk_rate(CLK_81);

	return priv->rate;
}

static int meson_clk_probe(struct udevice *dev)
{
	struct meson_clk *priv = dev_get_priv(dev);

	priv->addr = dev_read_addr_ptr(dev);
	debug("meson-clk: probed at addr %p\n", priv->addr);

	return 0;
}

static struct clk_ops meson_clk_ops = {
	.disable	= meson_clk_disable,
	.enable		= meson_clk_enable,
	.get_rate	= meson_clk_get_rate,
};

static const struct udevice_id meson_clk_ids[] = {
	{ .compatible = "amlogic,gxbb-clkc" },
	{ .compatible = "amlogic,gxl-clkc" },
	{ }
};

U_BOOT_DRIVER(meson_clk) = {
	.name		= "meson_clk",
	.id		= UCLASS_CLK,
	.of_match	= meson_clk_ids,
	.priv_auto_alloc_size = sizeof(struct meson_clk),
	.ops		= &meson_clk_ops,
	.probe		= meson_clk_probe,
};
