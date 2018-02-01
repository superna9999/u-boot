/*
 * Copyright (C) 2018 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <cpu.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include <regmap.h>

#define AO_SEC_SD_CFG8          0xe0
#define AO_SEC_SOCINFO_OFFSET   AO_SEC_SD_CFG8

#define SOCINFO_MAJOR	GENMASK(31, 24)
#define SOCINFO_PACK	GENMASK(23, 16)
#define SOCINFO_MINOR	GENMASK(15, 8)
#define SOCINFO_MISC	GENMASK(7, 0)

static const struct meson_gx_soc_id {
	const char *name;
	unsigned int id;
} soc_ids[] = {
	{ "GXBB", 0x1f },
	{ "GXTVBB", 0x20 },
	{ "GXL", 0x21 },
	{ "GXM", 0x22 },
	{ "TXL", 0x23 },
	{ "TXLX", 0x24 },
	{ "AXG", 0x25 },
	{ "GXLX", 0x26 },
	{ "TXHD", 0x27 },
};

static const struct meson_gx_package_id {
	const char *name;
	unsigned int major_id;
	unsigned int pack_id;
} soc_packages[] = {
	{ "S905", 0x1f, 0 },
	{ "S905H", 0x1f, 0x13 },
	{ "S905M", 0x1f, 0x20 },
	{ "S905D", 0x21, 0 },
	{ "S905X", 0x21, 0x80 },
	{ "S905W", 0x21, 0xa0 },
	{ "S905L", 0x21, 0xc0 },
	{ "S905M2", 0x21, 0xe0 },
	{ "S912", 0x22, 0 },
	{ "962X", 0x24, 0x10 },
	{ "962E", 0x24, 0x20 },
	{ "A113X", 0x25, 0x37 },
	{ "A113D", 0x25, 0x22 },
};

static inline unsigned int socinfo_to_major(u32 socinfo)
{
	return FIELD_GET(SOCINFO_MAJOR, socinfo);
}

static inline unsigned int socinfo_to_minor(u32 socinfo)
{
	return FIELD_GET(SOCINFO_MINOR, socinfo);
}

static inline unsigned int socinfo_to_pack(u32 socinfo)
{
	return FIELD_GET(SOCINFO_PACK, socinfo);
}

static inline unsigned int socinfo_to_misc(u32 socinfo)
{
	return FIELD_GET(SOCINFO_MISC, socinfo);
}

static const char *socinfo_to_package_id(u32 socinfo)
{
	unsigned int pack = socinfo_to_pack(socinfo) & 0xf0;
	unsigned int major = socinfo_to_major(socinfo);
	int i;

	for (i = 0 ; i < ARRAY_SIZE(soc_packages) ; ++i) {
		if (soc_packages[i].major_id == major &&
		    soc_packages[i].pack_id == pack)
			return soc_packages[i].name;
	}

	return "Unknown";
}

static const char *socinfo_to_soc_id(u32 socinfo)
{
	unsigned int id = socinfo_to_major(socinfo);
	int i;

	for (i = 0 ; i < ARRAY_SIZE(soc_ids) ; ++i) {
		if (soc_ids[i].id == id)
			return soc_ids[i].name;
	}

	return "Unknown";
}

struct meson_gx_socinfo_priv {
	struct regmap *regmap;
};

static int meson_gx_socinfo_get_desc(struct udevice *dev, char *buf, int size)
{
	struct meson_gx_socinfo_priv *priv = dev_get_priv(dev);
	uint socinfo;
	
	regmap_read(priv->regmap, AO_SEC_SOCINFO_OFFSET, &socinfo);

	snprintf(buf, size, "Amlogic Meson %s (%s) rev %x:%x (%x:%x)",
		socinfo_to_soc_id(socinfo),
		socinfo_to_package_id(socinfo),
		socinfo_to_major(socinfo),
		socinfo_to_minor(socinfo),
		socinfo_to_pack(socinfo),
		socinfo_to_misc(socinfo));

	return 0;
}

static int meson_gx_socinfo_get_vendor(struct udevice *dev, char *buf, int size)
{
	snprintf(buf, size, "Amlogic");

	return 0;
}

static const struct cpu_ops meson_gx_socinfo_ops = {
	.get_desc = meson_gx_socinfo_get_desc,
	.get_vendor = meson_gx_socinfo_get_vendor,
};

int meson_gx_socinfo_probe(struct udevice *dev)
{
	struct meson_gx_socinfo_priv *priv = dev_get_priv(dev);

	return regmap_init_mem(dev, &priv->regmap);
}

static const struct udevice_id meson_gx_socinfo_ids[] = {
	{ .compatible = "amlogic,meson-gx-ao-secure", }, 
	{ /* sentinel */ }
};

U_BOOT_DRIVER(meson_gx_socinfo_drv) = {
	.name = "meson-gx-socinfo",
	.id = UCLASS_CPU,
	.of_match = meson_gx_socinfo_ids,
	.probe = meson_gx_socinfo_probe,
	.priv_auto_alloc_size = sizeof(struct meson_gx_socinfo_priv),
	.ops = &meson_gx_socinfo_ops,
};

#ifdef CONFIG_DISPLAY_CPUINFO
int print_cpuinfo(void)
{
	struct udevice *dev;
	int err;
	char desc[100];

	err = uclass_get_device(UCLASS_CPU, 0, &dev);
	if (err)
		return 0;

	err = cpu_get_desc(dev, desc, sizeof(desc));
	if (err)
		return 0;

	printf("CPU: %s", desc);

	return 0;
}
#endif /* CONFIG_DISPLAY_CPUINFO */
