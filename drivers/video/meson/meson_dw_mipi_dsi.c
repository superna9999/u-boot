// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com> 
 */

#include <common.h>
#include <display.h>
#include <dm.h>
#include <edid.h>
#include <log.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <dm/lists.h>
#include <linux/bitops.h>
#include <power/regulator.h>
#include <clk.h>
#include <linux/delay.h>
#include <reset.h>
#include <panel.h>
#include <media_bus_format.h>
#include <generic-phy.h>
#include <phy-mipi-dphy.h>
#include <mipi_dsi.h>
#include <dsi_host.h>
#include "meson_dw_mipi_dsi.h"
#include "meson_vpu.h"

struct meson_dw_mipi_dsi {
	struct mipi_dsi_device device;
	struct udevice *dev;
	struct udevice *panel;
	struct udevice *dsi_host;
	struct udevice *dphy;
	void __iomem *base;
	void __iomem *hhi_base;
	struct clk px_clk;
	struct phy phy;
	struct phy_configure_opts_mipi_dphy config;
};

/*  MIPI DSI/VENC Color Format Definitions */
#define MIPI_DSI_VENC_COLOR_30B   0x0
#define MIPI_DSI_VENC_COLOR_24B   0x1
#define MIPI_DSI_VENC_COLOR_18B   0x2
#define MIPI_DSI_VENC_COLOR_16B   0x3

#define COLOR_16BIT_CFG_1         0x0
#define COLOR_16BIT_CFG_2         0x1
#define COLOR_16BIT_CFG_3         0x2
#define COLOR_18BIT_CFG_1         0x3
#define COLOR_18BIT_CFG_2         0x4
#define COLOR_24BIT               0x5
#define COLOR_20BIT_LOOSE         0x6
#define COLOR_24_BIT_YCBCR        0x7
#define COLOR_16BIT_YCBCR         0x8
#define COLOR_30BIT               0x9
#define COLOR_36BIT               0xa
#define COLOR_12BIT               0xb
#define COLOR_RGB_111             0xc
#define COLOR_RGB_332             0xd
#define COLOR_RGB_444             0xe

/*  MIPI DSI Relative REGISTERs Definitions */
/* For MIPI_DSI_TOP_CNTL */
#define BIT_DPI_COLOR_MODE        20
#define BIT_IN_COLOR_MODE         16
#define BIT_CHROMA_SUBSAMPLE      14
#define BIT_COMP2_SEL             12
#define BIT_COMP1_SEL             10
#define BIT_COMP0_SEL              8
#define BIT_DE_POL                 6
#define BIT_HSYNC_POL              5
#define BIT_VSYNC_POL              4
#define BIT_DPICOLORM              3
#define BIT_DPISHUTDN              2
#define BIT_EDPITE_INTR_PULSE      1
#define BIT_ERR_INTR_PULSE         0

/* HHI Registers */
#define HHI_VIID_CLK_DIV	0x128 /* 0x4a offset in data sheet */
#define VCLK2_DIV_MASK		0xff
#define VCLK2_DIV_EN		BIT(16)
#define VCLK2_DIV_RESET		BIT(17)
#define CTS_ENCL_SEL_MASK	(0xf << 12)
#define CTS_ENCL_SEL_SHIFT	12
#define HHI_VIID_CLK_CNTL	0x12c /* 0x4b offset in data sheet */
#define VCLK2_EN		BIT(19)
#define VCLK2_SEL_MASK		(0x7 << 16)
#define VCLK2_SEL_SHIFT		16
#define VCLK2_SOFT_RESET	BIT(15)
#define VCLK2_DIV1_EN		BIT(0)
#define HHI_VID_CLK_CNTL2	0x194 /* 0x65 offset in data sheet */
#define CTS_ENCL_EN		BIT(3)

static int meson_dw_mipi_dsi_read_timing(struct udevice *dev, struct display_timing *timing)
{
	struct meson_dw_mipi_dsi *priv = dev_get_priv(dev);

	return panel_get_display_timing(priv->panel, timing);
}

static int meson_dw_mipi_dsi_set_vclk(struct meson_dw_mipi_dsi *priv,
				      unsigned long clock)
{
	unsigned int vclk2_div;
	unsigned int pll_rate;
	int ret;

	pll_rate = priv->config.hs_clk_rate;
	vclk2_div = pll_rate / clock;

	ret = clk_set_rate(&priv->px_clk, pll_rate);
	if (ret) {
		printf("Failed to set DSI PLL rate %lu\n",
		       priv->config.hs_clk_rate);

		return ret;
	}

	ret = clk_enable(&priv->px_clk);
	if (ret) {
		printf("Failed to enable DSI PLL\n");
		return ret;
	}

	/* Disable VCLK2 */
	hhi_update_bits(HHI_VIID_CLK_CNTL, VCLK2_EN, 0);

	/* Setup the VCLK2 divider value */
	hhi_update_bits(HHI_VIID_CLK_DIV, VCLK2_DIV_MASK, (vclk2_div - 1));

	/* select gp0 for vclk2 */
	hhi_update_bits(HHI_VIID_CLK_CNTL, VCLK2_SEL_MASK, (0 << VCLK2_SEL_SHIFT));

	/* enable vclk2 gate */
	hhi_update_bits(HHI_VIID_CLK_CNTL, VCLK2_EN, VCLK2_EN);

	/* select vclk2_div1 for encl */
	hhi_update_bits(HHI_VIID_CLK_DIV, CTS_ENCL_SEL_MASK, (8 << CTS_ENCL_SEL_SHIFT));

	/* release vclk2_div_reset and enable vclk2_div */
	hhi_update_bits(HHI_VIID_CLK_DIV, VCLK2_DIV_EN | VCLK2_DIV_RESET, VCLK2_DIV_EN);

	/* enable vclk2_div1 gate */
	hhi_update_bits(HHI_VIID_CLK_CNTL, VCLK2_DIV1_EN, VCLK2_DIV1_EN);

	/* reset vclk2 */
	hhi_update_bits(HHI_VIID_CLK_CNTL, VCLK2_SOFT_RESET, VCLK2_SOFT_RESET);
	hhi_update_bits(HHI_VIID_CLK_CNTL, VCLK2_SOFT_RESET, 0);

	/* enable encl_clk */
	hhi_update_bits(HHI_VID_CLK_CNTL2, CTS_ENCL_EN, CTS_ENCL_EN);

	mdelay(10);

	return 0;
}

static int dsi_phy_init(void *priv_data)
{
	struct meson_dw_mipi_dsi *priv = priv_data;

	return generic_phy_power_on(&priv->phy);
}

static int dsi_get_lane_mbps(void *priv_data, struct display_timing *timings,
			     u32 lanes, u32 format, unsigned int *lane_mbps)
{
	struct meson_dw_mipi_dsi *priv = priv_data;

	*lane_mbps = priv->config.hs_clk_rate / 1000000;

	return 0;
}

static int
dw_mipi_dsi_phy_get_timing(void *priv_data, unsigned int lane_mbps,
			   struct mipi_dsi_phy_timing *timing)
{
	/* TOFIX handle other cases */

	timing->clk_lp2hs = 37;
	timing->clk_hs2lp = 135;
	timing->data_lp2hs = 50;
	timing->data_hs2lp = 3;

	return 0;
}

static void
dw_mipi_dsi_get_esc_clk_rate(void *priv_data, unsigned int *esc_clk_rate)
{
	*esc_clk_rate = 4; /* Mhz */
}

static const struct mipi_dsi_phy_ops meson_dw_mipi_dsi_phy_ops = {
	.init = dsi_phy_init,
	.get_lane_mbps = dsi_get_lane_mbps,
	.get_timing = dw_mipi_dsi_phy_get_timing,
	.get_esc_clk_rate = dw_mipi_dsi_get_esc_clk_rate,
};

static void meson_dw_mipi_dsi_init(struct meson_dw_mipi_dsi *priv)
{
	writel((1 << 4) | (1 << 5) | (0 << 6),
		priv->base + MIPI_DSI_TOP_CNTL);

	writel_bits(0xf, 0xf, priv->base + MIPI_DSI_TOP_SW_RESET);
	writel_bits(0xf, 0, priv->base + MIPI_DSI_TOP_SW_RESET);

	writel_bits(0x3, 0x3, priv->base + MIPI_DSI_TOP_CLK_CNTL);

	writel(0, priv->base + MIPI_DSI_TOP_MEM_PD);
}

static int meson_dw_mipi_dsi_enable(struct udevice *dev, int panel_bpp,
				    const struct display_timing *timings)
{
	struct meson_dw_mipi_dsi *priv = dev_get_priv(dev);
	unsigned int dpi_data_format, venc_data_width;	
	struct mipi_dsi_panel_plat *mplat;
	struct display_timing mode;
	int bpp, ret;
	u32 reg;

	memcpy(&mode, timings, sizeof(struct display_timing));

	mplat = dev_get_platdata(priv->panel);
	mplat->device = &priv->device;
	priv->device.lanes = mplat->lanes;
	priv->device.format = mplat->format;
	priv->device.mode_flags = mplat->mode_flags;
	strncpy(priv->device.name, priv->panel->name, DSI_DEV_NAME_SIZE);

	bpp = mipi_dsi_pixel_format_to_bpp(priv->device.format);

	debug("Display timing:\n");
	debug(" hactive %04d, hfrontp %04d, hbackp %04d hsync %04d\n"
	      " vactive %04d, vfrontp %04d, vbackp %04d vsync %04d\n",
	       mode.hactive.typ, mode.hfront_porch.typ,
	       mode.hback_porch.typ, mode.hsync_len.typ,
	       mode.vactive.typ, mode.vfront_porch.typ,
	       mode.vback_porch.typ, mode.vsync_len.typ);
	debug(" flags: ");
	if (mode.flags & DISPLAY_FLAGS_HSYNC_LOW)
		debug("hsync_low ");
	if (mode.flags & DISPLAY_FLAGS_HSYNC_HIGH)
		debug("hsync_high ");
	if (mode.flags & DISPLAY_FLAGS_VSYNC_LOW)
		debug("vsync_low ");
	if (mode.flags & DISPLAY_FLAGS_VSYNC_HIGH)
		debug("vsync_high ");
	debug("\n");
	debug("Panel '%s' info:\n", priv->device.name);
	debug(" lanes: %d\n", priv->device.lanes);
	debug(" format: %d bpp: %d\n", priv->device.format, bpp);
	debug(" flags: %08lx\n", priv->device.mode_flags);

	phy_mipi_dphy_get_default_config(mode.pixelclock.typ,
					 bpp, priv->device.lanes,
					 &priv->config);

	ret = generic_phy_configure(&priv->phy, &priv->config);
	if (ret)
		return ret;

	switch (priv->device.format) {
	case MIPI_DSI_FMT_RGB888:
		dpi_data_format = COLOR_24BIT;
		venc_data_width = MIPI_DSI_VENC_COLOR_24B;
		break;
	case MIPI_DSI_FMT_RGB666:
		dpi_data_format = COLOR_18BIT_CFG_2;
		venc_data_width = MIPI_DSI_VENC_COLOR_18B;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
	case MIPI_DSI_FMT_RGB565:
		/* invalid */
		break;
	};

	meson_dw_mipi_dsi_set_vclk(priv, mode.pixelclock.typ);
	if (ret)
		return ret;

	meson_dw_mipi_dsi_init(priv);

	/* Configure Set color format for DPI register */
	reg = readl(priv->base + MIPI_DSI_TOP_CNTL) &
		~(0xf<<BIT_DPI_COLOR_MODE) &
		~(0x7<<BIT_IN_COLOR_MODE) &
		~(0x3<<BIT_CHROMA_SUBSAMPLE);

	writel(reg |
		(dpi_data_format  << BIT_DPI_COLOR_MODE)  |
		(venc_data_width  << BIT_IN_COLOR_MODE) |
		0 << BIT_COMP0_SEL |
		1 << BIT_COMP1_SEL |
		2 << BIT_COMP2_SEL |
		(timings->flags & DISPLAY_FLAGS_HSYNC_LOW ? 0 : BIT(BIT_HSYNC_POL)) |
		(timings->flags & DISPLAY_FLAGS_VSYNC_LOW ? 0 : BIT(BIT_VSYNC_POL)),
		priv->base + MIPI_DSI_TOP_CNTL);

	ret = dsi_host_init(priv->dsi_host, &priv->device, &mode, 4, &meson_dw_mipi_dsi_phy_ops);
	if (ret) {
		dev_err(dev, "failed to initialize mipi dsi host\n");
		return ret;
	}

        ret = panel_enable_backlight(priv->panel);
	if (ret) {
		dev_err(dev, "panel %s enable backlight error %d\n",
				priv->panel->name, ret);
		return ret;
	}

        ret = dsi_host_enable(priv->dsi_host);
	if (ret) {
		dev_err(dev, "failed to enable mipi dsi host\n");
		return ret;
	}

	return 0;
}

static int meson_dw_mipi_dsi_bind(struct udevice *dev)
{
	int ret;

	ret = device_bind_driver_to_node(dev, "dw_mipi_dsi", "dsihost",
					 dev_ofnode(dev), NULL);
	if (ret)
		return ret;

	return dm_scan_fdt_dev(dev);
}

static void __iomem * meson_dw_mipi_dsi_get_hhi(struct udevice *dev)
{
	struct udevice *vpudev;
	struct uclass *uc;
	int err;

	err = uclass_get(UCLASS_VIDEO, &uc);
	if (err)
		return NULL;

	uclass_foreach_dev(vpudev, uc) {
		if (strstr(vpudev->driver->name, "meson_vpu"))
			return dev_remap_addr_index(vpudev, 1);
	}

	return NULL;
}

static int meson_dw_mipi_dsi_probe(struct udevice *dev)
{
	struct meson_dw_mipi_dsi *priv = dev_get_priv(dev);
	struct reset_ctl_bulk resets;
	struct clk clk;
	int ret;

	priv->dev = dev;
	priv->device.dev = dev;

	priv->base = dev_remap_addr_index(dev, 0);
	if (!priv->base)
		return -EINVAL;

	/* Get HHI from vpu */
	priv->hhi_base = meson_dw_mipi_dsi_get_hhi(dev);
	if (!priv->hhi_base)
		return -EINVAL;

	ret = uclass_first_device(UCLASS_PANEL, &priv->panel);
	if (ret) {
		dev_err(dev, "panel device error %d\n", ret);
		return ret;
	}

	ret = uclass_get_device(UCLASS_DSI_HOST, 0, &priv->dsi_host);
	if (ret) {
		dev_err(dev, "No video dsi host detected %d\n", ret);
		return ret;
	}

	ret = generic_phy_get_by_name(dev, "dphy", &priv->phy);
	if (ret)
		return ret;

	ret = reset_get_bulk(dev, &resets);
	if (ret)
		return ret;

	ret = clk_get_by_name(dev, "pclk", &clk);
	if (ret) {
		dev_err(dev, "peripheral clock get error %d\n", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "px_clk", &priv->px_clk);
	if (ret) {
		dev_err(dev, "pixel clock get error %d\n", ret);
		return ret;
	}

	ret = clk_enable(&clk);
	if (ret)
		return ret;

	ret = reset_deassert_bulk(&resets);
	if (ret)
		return ret;

	ret = reset_assert_bulk(&resets);
	if (ret)
		return ret;

	ret = reset_deassert_bulk(&resets);
	if (ret)
		return ret;

	return 0;
}

static const struct dm_display_ops meson_dw_mipi_dsi_ops = {
	.read_timing = meson_dw_mipi_dsi_read_timing,
	.enable = meson_dw_mipi_dsi_enable,
};

static const struct udevice_id meson_dw_mipi_dsi_ids[] = {
	{ .compatible = "amlogic,meson-axg-dw-mipi-dsi", },
	{ }
};

U_BOOT_DRIVER(meson_dw_mipi_dsi) = {
	.name = "meson_dw_mipi_dsi",
	.id = UCLASS_DISPLAY,
	.of_match = meson_dw_mipi_dsi_ids,
	.ops = &meson_dw_mipi_dsi_ops,
	.bind = meson_dw_mipi_dsi_bind,
	.probe = meson_dw_mipi_dsi_probe,
	.priv_auto_alloc_size = sizeof(struct meson_dw_mipi_dsi),
};
