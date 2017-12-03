/*
 * (C) Copyright 2016 Beniamino Galvani <b.galvani@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Clock rate measuring.
 */

#include <common.h>
#include <asm/arch/clock.h>
#include <asm/io.h>

ulong meson_measure_clk_rate(unsigned int clk)
{
	ulong start;
	ulong mhz;

	writel(0, MSR_CLK_REG0);

	/* Set the measurement gate to 64uS */
	clrsetbits_le32(MSR_CLK_REG0, 0xffff, 64 - 1);
	clrbits_le32(MSR_CLK_REG0,
		     BIT(17) |		/* disable continuous measurement */
		     BIT(18));		/* disable interrupts */
	clrsetbits_le32(MSR_CLK_REG0,
			GENMASK(20, 26),
			clk << 20);	/* select the clock */
	setbits_le32(MSR_CLK_REG0,
		     BIT(19) |		/* enable the clock */
		     BIT(16));		/* enable measuring */

	start = get_timer(0);
	while (readl(MSR_CLK_REG0) & BIT(31)) {
		if (get_timer(start) > 100) {
			debug("could not measure clk %u rate\n", clk);
			return -ETIMEDOUT;
		}
	}

	/* Disable measuring */
	clrbits_le32(MSR_CLK_REG0, BIT(16));

	mhz = ((readl(MSR_CLK_REG2) + 31) & 0xfffff) >> 6;
	return mhz * 1000000;
}
