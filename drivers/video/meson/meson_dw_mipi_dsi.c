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
#include <dm/device_compat.h>
#include <dm/uclass-internal.h>
#include <dm/lists.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
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
	struct clk px_clk;
	struct clk bit_clk;
	struct phy phy;
	struct phy_configure_opts_mipi_dphy config;
	struct display_timing mode;
};

#define writel_bits(mask, val, addr) \
		writel((readl(addr) & ~(mask)) | (val), addr)

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

static int dsi_phy_init(void *priv_data)
{
	struct meson_dw_mipi_dsi *priv = priv_data;

	return generic_phy_power_on(&priv->phy);
}

static int dsi_get_lane_mbps(void *priv_data, struct display_timing *timings,
			     u32 lanes, u32 format, unsigned int *lane_mbps)
{
	struct meson_dw_mipi_dsi *priv = priv_data;

	*lane_mbps = DIV_ROUND_UP(priv->config.hs_clk_rate, 1000000);

	return 0;
}

static int
dw_mipi_dsi_phy_get_timing(void *priv_data, unsigned int lane_mbps,
			   struct mipi_dsi_phy_timing *timing)
{
	struct meson_dw_mipi_dsi *priv = priv_data;

	switch (priv->mode.hactive.typ) {
	case 240:
	case 768:
	case 1920:
	case 2560:
		timing->clk_lp2hs = 23;
		timing->clk_hs2lp = 38;
		timing->data_lp2hs = 15;
		timing->data_hs2lp = 9;
		break;

	default:
		timing->clk_lp2hs = 37;
		timing->clk_hs2lp = 135;
		timing->data_lp2hs = 50;
		timing->data_hs2lp = 3;
	}

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
	/* Software reset */
	writel_bits(MIPI_DSI_TOP_SW_RESET_DWC | MIPI_DSI_TOP_SW_RESET_INTR |
		    MIPI_DSI_TOP_SW_RESET_DPI | MIPI_DSI_TOP_SW_RESET_TIMING,
		    MIPI_DSI_TOP_SW_RESET_DWC | MIPI_DSI_TOP_SW_RESET_INTR |
		    MIPI_DSI_TOP_SW_RESET_DPI | MIPI_DSI_TOP_SW_RESET_TIMING,
		    priv->base + MIPI_DSI_TOP_SW_RESET);
	writel_bits(MIPI_DSI_TOP_SW_RESET_DWC | MIPI_DSI_TOP_SW_RESET_INTR |
		    MIPI_DSI_TOP_SW_RESET_DPI | MIPI_DSI_TOP_SW_RESET_TIMING,
		    0, priv->base + MIPI_DSI_TOP_SW_RESET);

	/* Enable clocks */
	writel_bits(MIPI_DSI_TOP_CLK_SYSCLK_EN | MIPI_DSI_TOP_CLK_PIXCLK_EN,
		    MIPI_DSI_TOP_CLK_SYSCLK_EN | MIPI_DSI_TOP_CLK_PIXCLK_EN,
		    priv->base + MIPI_DSI_TOP_CLK_CNTL);

	/* Take memory out of power down */
	writel(0, priv->base + MIPI_DSI_TOP_MEM_PD);
}

static int meson_dw_mipi_dsi_enable(struct udevice *dev, int panel_bpp,
				    const struct display_timing *timings)
{
	struct meson_dw_mipi_dsi *priv = dev_get_priv(dev);
	unsigned int dpi_data_format, venc_data_width;
	struct mipi_dsi_panel_plat *mplat;
	int bpp, ret;

	memcpy(&priv->mode, timings, sizeof(struct display_timing));

	mplat = dev_get_plat(priv->panel);
	mplat->device = &priv->device;
	priv->device.lanes = mplat->lanes;
	priv->device.format = mplat->format;
	priv->device.mode_flags = mplat->mode_flags;
	strncpy(priv->device.name, priv->panel->name, DSI_DEV_NAME_SIZE);

	bpp = mipi_dsi_pixel_format_to_bpp(priv->device.format);

	debug("Display timing:\n");
	debug(" hactive %04d, hfrontp %04d, hbackp %04d hsync %04d\n"
	      " vactive %04d, vfrontp %04d, vbackp %04d vsync %04d\n",
	       priv->mode.hactive.typ, priv->mode.hfront_porch.typ,
	       priv->mode.hback_porch.typ, priv->mode.hsync_len.typ,
	       priv->mode.vactive.typ, priv->mode.vfront_porch.typ,
	       priv->mode.vback_porch.typ, priv->mode.vsync_len.typ);
	debug(" flags: ");
	if (priv->mode.flags & DISPLAY_FLAGS_HSYNC_LOW)
		debug("hsync_low ");
	if (priv->mode.flags & DISPLAY_FLAGS_HSYNC_HIGH)
		debug("hsync_high ");
	if (priv->mode.flags & DISPLAY_FLAGS_VSYNC_LOW)
		debug("vsync_low ");
	if (priv->mode.flags & DISPLAY_FLAGS_VSYNC_HIGH)
		debug("vsync_high ");
	debug("\n");
	debug("Panel '%s' info:\n", priv->device.name);
	debug(" lanes: %d\n", priv->device.lanes);
	debug(" format: %d bpp: %d\n", priv->device.format, bpp);
	debug(" flags: %08lx\n", priv->device.mode_flags);

	phy_mipi_dphy_get_default_config(priv->mode.pixelclock.typ,
					 bpp, priv->device.lanes,
					 &priv->config);

	ret = clk_set_rate(&priv->bit_clk, priv->config.hs_clk_rate);
	if (ret) {
		dev_err(dev, "Failed to set DSI bit clock rate %lu\n",
		       priv->config.hs_clk_rate);
		return ret;
	}

	clk_disable(&priv->px_clk);
	ret = clk_set_rate(&priv->px_clk, priv->mode.pixelclock.typ);
	if (ret) {
		dev_err(dev, "Failed to set DSI Pixel clock rate %u\n",
			priv->mode.pixelclock.typ);
		return ret;
	}

	ret = clk_enable(&priv->px_clk);
	if (ret) {
		dev_err(dev, "Failed to enable DSI Pixel clock\n");
		return ret;
	}

	switch (priv->device.format) {
	case MIPI_DSI_FMT_RGB888:
		dpi_data_format = DPI_COLOR_24BIT;
		venc_data_width = MIPI_DSI_VENC_COLOR_24B;
		break;
	case MIPI_DSI_FMT_RGB666:
		dpi_data_format = DPI_COLOR_18BIT_CFG_2;
		venc_data_width = MIPI_DSI_VENC_COLOR_18B;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
	case MIPI_DSI_FMT_RGB565:
		/* invalid */
		break;
	};

	meson_dw_mipi_dsi_init(priv);

	/* Configure color format for DPI register */
	writel(FIELD_PREP(MIPI_DSI_TOP_DPI_COLOR_MODE, dpi_data_format) |
	       FIELD_PREP(MIPI_DSI_TOP_IN_COLOR_MODE, venc_data_width) |
	       FIELD_PREP(MIPI_DSI_TOP_COMP2_SEL, 2) |
	       FIELD_PREP(MIPI_DSI_TOP_COMP1_SEL, 1) |
	       FIELD_PREP(MIPI_DSI_TOP_COMP0_SEL, 0),
	       priv->base + MIPI_DSI_TOP_CNTL);

	ret = generic_phy_configure(&priv->phy, &priv->config);
	if (ret)
		return ret;

	ret = dsi_host_init(priv->dsi_host, &priv->device, &priv->mode, 4, &meson_dw_mipi_dsi_phy_ops);
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

	ret = uclass_first_device_err(UCLASS_PANEL, &priv->panel);
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

	ret = clk_get_by_name(dev, "px", &priv->px_clk);
	if (ret) {
		dev_err(dev, "pixel clock get error %d\n", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "bit", &priv->bit_clk);
	if (ret) {
		dev_err(dev, "bit clock get error %d\n", ret);
		return ret;
	}

	ret = clk_enable(&priv->bit_clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->px_clk);
	if (ret)
		return ret;

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
	{ .compatible = "amlogic,meson-g12a-dw-mipi-dsi", },
	{ }
};

U_BOOT_DRIVER(meson_dw_mipi_dsi) = {
	.name = "meson_dw_mipi_dsi",
	.id = UCLASS_DISPLAY,
	.of_match = meson_dw_mipi_dsi_ids,
	.ops = &meson_dw_mipi_dsi_ops,
	.bind = meson_dw_mipi_dsi_bind,
	.probe = meson_dw_mipi_dsi_probe,
	.priv_auto = sizeof(struct meson_dw_mipi_dsi),
};
