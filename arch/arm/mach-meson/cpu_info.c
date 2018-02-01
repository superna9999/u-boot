/*
 * Copyright (C) 2018 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include <asm/arch/gx.h>

#ifdef CONFIG_DISPLAY_CPUINFO

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
};

static const struct meson_gx_package_id {
	const char *name;
	unsigned int major_id;
	unsigned int pack_id;
} soc_packages[] = {
	{ "S905", 0x1f, 0 },
	{ "S905M", 0x1f, 0x20 },
	{ "S905D", 0x21, 0 },
	{ "S905X", 0x21, 0x80 },
	{ "S905L", 0x21, 0xc0 },
	{ "S905M2", 0x21, 0xe0 },
	{ "S912", 0x22, 0 },
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

int print_cpuinfo(void)
{
	u32 socinfo = readl(GX_AO_SEC_SD_CFG8);
	printf("CPU: Amlogic Meson %s (%s) rev %x:%x (%x:%x)\n",
		socinfo_to_soc_id(socinfo),
		socinfo_to_package_id(socinfo),
		socinfo_to_major(socinfo),
		socinfo_to_minor(socinfo),
		socinfo_to_pack(socinfo),
		socinfo_to_misc(socinfo));
	return 0;
}
#endif /* CONFIG_DISPLAY_CPUINFO */
