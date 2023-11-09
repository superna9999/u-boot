// SPDX-License-Identifier: GPL-2.0+
/*
 * Amlogic G12A MIPI analog PHY driver
 *
 * Based on Linux phy-meson-g12a-mipi-dphy-analog:
 * Copyright (C) 2022 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#include <common.h>
#include <log.h>
#include <malloc.h>
#include <asm/io.h>
#include <bitfield.h>
#include <dm.h>
#include <errno.h>
#include <generic-phy.h>
#include <regmap.h>
#include <syscon.h>
#include <phy-mipi-dphy.h>

#include <linux/bitops.h>
#include <linux/compat.h>
#include <linux/bitfield.h>

#define HHI_MIPI_CNTL0 0x00
#define		HHI_MIPI_CNTL0_DIF_REF_CTL1	GENMASK(31, 16)
#define		HHI_MIPI_CNTL0_DIF_REF_CTL0	GENMASK(15, 0)

#define HHI_MIPI_CNTL1 0x04
#define		HHI_MIPI_CNTL1_BANDGAP		BIT(16)
#define		HHI_MIPI_CNTL2_DIF_REF_CTL2	GENMASK(15, 0)

#define HHI_MIPI_CNTL2 0x08
#define		HHI_MIPI_CNTL2_DIF_TX_CTL1	GENMASK(31, 16)
#define		HHI_MIPI_CNTL2_CH_EN		GENMASK(15, 11)
#define		HHI_MIPI_CNTL2_DIF_TX_CTL0	GENMASK(10, 0)

#define DSI_LANE_0				BIT(4)
#define DSI_LANE_1				BIT(3)
#define DSI_LANE_CLK				BIT(2)
#define DSI_LANE_2				BIT(1)
#define DSI_LANE_3				BIT(0)
#define DSI_LANE_MASK				GENMASK(4, 0)

struct phy_meson_g12a_mipi_dphy_analog_priv {
	struct regmap *regmap;
	struct phy_configure_opts_mipi_dphy config;
};

static int phy_meson_g12a_mipi_dphy_analog_configure(struct phy *phy, void *params)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_g12a_mipi_dphy_analog_priv *priv = dev_get_priv(dev);
	struct phy_configure_opts_mipi_dphy *config = params;
	int ret;

	ret = phy_mipi_dphy_config_validate(config);
	if (ret)
		return ret;

	memcpy(&priv->config, config, sizeof(priv->config));

	return 0;
}

static int phy_meson_g12a_mipi_dphy_analog_power_on(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_g12a_mipi_dphy_analog_priv *priv = dev_get_priv(dev);
	u32 reg;

	regmap_write(priv->regmap, HHI_MIPI_CNTL0,
		     FIELD_PREP(HHI_MIPI_CNTL0_DIF_REF_CTL0, 0x8) |
		     FIELD_PREP(HHI_MIPI_CNTL0_DIF_REF_CTL1, 0xa487));

	regmap_write(priv->regmap, HHI_MIPI_CNTL1,
		     FIELD_PREP(HHI_MIPI_CNTL2_DIF_REF_CTL2, 0x2e) |
		     HHI_MIPI_CNTL1_BANDGAP);

	regmap_write(priv->regmap, HHI_MIPI_CNTL2,
		     FIELD_PREP(HHI_MIPI_CNTL2_DIF_TX_CTL0, 0x45a) |
		     FIELD_PREP(HHI_MIPI_CNTL2_DIF_TX_CTL1, 0x2680));

	reg = DSI_LANE_CLK;
	switch (priv->config.lanes) {
	case 4:
		reg |= DSI_LANE_3;
		fallthrough;
	case 3:
		reg |= DSI_LANE_2;
		fallthrough;
	case 2:
		reg |= DSI_LANE_1;
		fallthrough;
	case 1:
		reg |= DSI_LANE_0;
		break;
	default:
		reg = 0;
	}

	regmap_update_bits(priv->regmap, HHI_MIPI_CNTL2,
			   HHI_MIPI_CNTL2_CH_EN,
			   FIELD_PREP(HHI_MIPI_CNTL2_CH_EN, reg));

	return 0;
}

static int phy_meson_g12a_mipi_dphy_analog_power_off(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_g12a_mipi_dphy_analog_priv *priv = dev_get_priv(dev);

	regmap_write(priv->regmap, HHI_MIPI_CNTL0, 0);
	regmap_write(priv->regmap, HHI_MIPI_CNTL1, 0);
	regmap_write(priv->regmap, HHI_MIPI_CNTL2, 0);

	return 0;
}

struct phy_ops meson_g12a_mipi_dphy_analog_ops = {
	.power_on = phy_meson_g12a_mipi_dphy_analog_power_on,
	.power_off = phy_meson_g12a_mipi_dphy_analog_power_off,
	.configure = phy_meson_g12a_mipi_dphy_analog_configure,
};

int meson_g12a_mipi_dphy_analog_probe(struct udevice *dev)
{
	struct phy_meson_g12a_mipi_dphy_analog_priv *priv = dev_get_priv(dev);

	priv->regmap = syscon_node_to_regmap(dev_ofnode(dev_get_parent(dev)));
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	return 0;
}

static const struct udevice_id meson_g12a_mipi_dphy_analog_ids[] = {
	{ .compatible = "amlogic,g12a-mipi-dphy-analog" },
	{ }
};

U_BOOT_DRIVER(meson_g12a_mipi_dphy_analog) = {
	.name = "meson_g12a_mipi_dphy_analog",
	.id = UCLASS_PHY,
	.of_match = meson_g12a_mipi_dphy_analog_ids,
	.probe = meson_g12a_mipi_dphy_analog_probe,
	.ops = &meson_g12a_mipi_dphy_analog_ops,
	.priv_auto = sizeof(struct phy_meson_g12a_mipi_dphy_analog_priv),
};
