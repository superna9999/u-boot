/*
 * Configuration for NANOPI-K2
 * (C) Copyright 2018 Thomas McKahan 
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_MISC_INIT_R

/* Serial setup */
#define CONFIG_CONS_INDEX		0

#define MESON_FDTFILE_SETTING "fdtfile=amlogic/meson-gxbb-nanopi-k2.dtb\0"

#include <configs/meson-gx-common.h>

#endif /* __CONFIG_H */
