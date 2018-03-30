/*
 * dwc3-of-simple.c - OF glue layer for simple integrations
 *
 * Copyright (c) 2015 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Felipe Balbi <balbi@ti.com>
 *
 * Copyright (C) 2018 BayLibre, SAS
 * Author: Neil Armstrong <narmstron@baylibre.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <reset.h>
#include <clk.h>

DECLARE_GLOBAL_DATA_PTR;

struct dwc3_of_simple {
#if CONFIG_IS_ENABLED(CLK)
	struct clk_bulk		clks;
#endif
#if CONFIG_IS_ENABLED(DM_RESET)
	struct reset_ctl_bulk	resets;
#endif
};

#if CONFIG_IS_ENABLED(DM_RESET)
static int dwc3_of_simple_reset_init(struct udevice *dev,
				     struct dwc3_of_simple *simple)
{
	int ret;

	ret = reset_get_bulk(dev, &simple->resets);
	if (ret)
		return ret;

	ret = reset_deassert_bulk(&simple->resets);
	if (ret) {
		reset_release_bulk(&simple->resets);
		return ret;
	}

	return 0;
}
#endif

#if CONFIG_IS_ENABLED(CLK)
static int dwc3_of_simple_clk_init(struct udevice *dev,
				   struct dwc3_of_simple *simple)
{
	int ret;

	ret = clk_get_bulk(dev, &simple->clks);
	if (ret)
		return ret;

	ret = clk_enable_bulk(&simple->clks);
	if (ret) {
		clk_release_bulk(&simple->clks);
		return ret;
	}

	return 0;
}
#endif

static int dwc3_of_simple_probe(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(DM_RESET) || CONFIG_IS_ENABLED(CLK)
	struct dwc3_of_simple *simple = dev_get_platdata(dev);
	int ret;
#endif

#if CONFIG_IS_ENABLED(CLK)
	ret = dwc3_of_simple_clk_init(dev, simple);
	if (ret)
		return ret;
#endif

#if CONFIG_IS_ENABLED(DM_RESET)
	ret = dwc3_of_simple_reset_init(dev, simple);
	if (ret)
		return ret;
#endif

	return 0;
}

static int dwc3_of_simple_remove(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(DM_RESET) || CONFIG_IS_ENABLED(CLK)
	struct dwc3_of_simple *simple = dev_get_platdata(dev);
#endif

#if CONFIG_IS_ENABLED(DM_RESET)
	reset_release_bulk(&simple->resets);
#endif

#if CONFIG_IS_ENABLED(CLK)
	clk_release_bulk(&simple->clks);
#endif

	return dm_scan_fdt_dev(dev);
}

static const struct udevice_id dwc3_of_simple_ids[] = {
	{ .compatible = "amlogic,meson-gxl-dwc3" },
	{ }
};

U_BOOT_DRIVER(dwc3_of_simple) = {
	.name = "dwc3-of-simple",
	.id = UCLASS_SIMPLE_BUS,
	.of_match = dwc3_of_simple_ids,
	.probe = dwc3_of_simple_probe,
	.remove = dwc3_of_simple_remove,
	.platdata_auto_alloc_size = sizeof(struct dwc3_of_simple),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
