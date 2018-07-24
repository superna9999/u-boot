// SPDX-License-Identifier: GPL-2.0
/*
 * Amlogic Meson Video Processing Unit driver
 *
 * Copyright (c) 2018 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#include "meson_vpu.h"
#include <power-domain.h>
#include <efi_loader.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include "meson_registers.h"

static int meson_vpu_setup_mode(struct udevice *dev, struct udevice *disp)
{
	struct video_uc_platdata *uc_plat = dev_get_uclass_platdata(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct display_timing timing;
	bool is_cvbs;
	int ret;

	debug("%s:%d\n", __func__, __LINE__);

	/* CVBS has a fixed 720x480 mode */
	if (disp) {
		ret = device_probe(disp);
		if (ret) {
			debug("%s: device '%s' display won't probe (ret=%d)\n",
			      __func__, dev->name, ret);
			return ret;
		}

		ret = display_read_timing(disp, &timing);
		if (ret) {
			debug("%s: Failed to read timings\n", __func__);
			return ret;
		}

		uc_priv->xsize = timing.hactive.typ;
		uc_priv->ysize = timing.vactive.typ;
		uc_priv->bpix = VPU_MAX_LOG2_BPP;
	} else {
		timing.flags = DISPLAY_FLAGS_INTERLACED;

		uc_priv->xsize = 720;
		uc_priv->ysize = 480;
		uc_priv->bpix = VPU_MAX_LOG2_BPP;

		is_cvbs = true;
	}

	debug("%s:%d\n", __func__, __LINE__);

	meson_vpu_init(dev);

	debug("%s:%d\n", __func__, __LINE__);

	meson_vpu_setup_plane(dev, timing.flags & DISPLAY_FLAGS_INTERLACED);

	debug("%s:%d\n", __func__, __LINE__);

	meson_vpu_setup_venc(dev, &timing, is_cvbs);

	debug("%s:%d\n", __func__, __LINE__);

	meson_vpu_setup_vclk(dev, &timing, is_cvbs);

	debug("%s:%d\n", __func__, __LINE__);

#ifdef CONFIG_EFI_LOADER
	efi_add_memory_map(uc_plat->base,
			   ALIGN(timing.hactive.typ * timing.vactive.typ *
			   (1 << VPU_MAX_LOG2_BPP) / 8, EFI_PAGE_SIZE)
			   				>> EFI_PAGE_SHIFT,
			   EFI_RESERVED_MEMORY_TYPE, false);
#endif

	debug("%s:%d\n", __func__, __LINE__);

	video_set_flush_dcache(dev, 1);

	debug("%s:%d\n", __func__, __LINE__);

	return 0;
}

static const struct udevice_id meson_vpu_ids[] = {                          
	{ .compatible = "amlogic,meson-gxbb-vpu", .data = VPU_COMPATIBLE_GXBB },
	{ .compatible = "amlogic,meson-gxl-vpu", .data = VPU_COMPATIBLE_GXL },
	{ .compatible = "amlogic,meson-gxm-vpu", .data = VPU_COMPATIBLE_GXM },
	{ }                                                                     
};

static int meson_vpu_probe(struct udevice *dev)
{
	struct meson_vpu_priv *priv = dev_get_priv(dev);
	struct power_domain pd;
	struct udevice *disp;
	int ret;

	/* Before relocation we don't need to do anything */
	if (!(gd->flags & GD_FLG_RELOC))
		return 0;

	debug("%s:%d\n", __func__, __LINE__);

	priv->dev = dev;

	priv->io_base = dev_remap_addr_index(dev, 0);
	if (!priv->io_base)
		return -EINVAL;

	debug("%s:%d\n", __func__, __LINE__);

	priv->hhi_base = dev_remap_addr_index(dev, 1);
	if (!priv->hhi_base)
		return -EINVAL;

	debug("%s:%d\n", __func__, __LINE__);

	priv->dmc_base = dev_remap_addr_index(dev, 2);
	if (!priv->dmc_base)
		return -EINVAL;

	debug("%s:%d\n", __func__, __LINE__);

	ret = power_domain_get(dev, &pd);
	if (ret)
		return ret;

	debug("%s:%d\n", __func__, __LINE__);

	ret = power_domain_on(&pd);
	if (ret)
		return ret;

	debug("%s:%d\n", __func__, __LINE__);

	/* Try to use HDMI */
	ret = uclass_find_device_by_name(UCLASS_DISPLAY,
					 "meson_dw_hdmi", &disp);
	if (!ret) {
		ret = meson_vpu_setup_mode(dev, disp);
		if (ret) {
			debug("%s: hdmi display not found (ret=%d)\n",
			      __func__, ret);
		}
	}

	debug("%s:%d\n", __func__, __LINE__);

	/* Fallback to CVBS */
	if (ret)
		ret = meson_vpu_setup_mode(dev, NULL);

	debug("%s:%d\n", __func__, __LINE__);

	return ret;
}

static int meson_vpu_bind(struct udevice *dev)
{
	struct video_uc_platdata *plat = dev_get_uclass_platdata(dev);

	plat->size = VPU_MAX_WIDTH * VPU_MAX_HEIGHT *
		(1 << VPU_MAX_LOG2_BPP) / 8;

	return 0;
}

U_BOOT_DRIVER(meson_vpu) = {
	.name = "meson_vpu",
	.id = UCLASS_VIDEO,
	.of_match = meson_vpu_ids,
	.probe = meson_vpu_probe,
	.bind = meson_vpu_bind,
	.priv_auto_alloc_size = sizeof(struct meson_vpu_priv),
	.flags  = DM_FLAG_PRE_RELOC,
};
