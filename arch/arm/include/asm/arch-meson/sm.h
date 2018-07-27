/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2016 - Beniamino Galvani <b.galvani@gmail.com>
 */

#ifndef __MESON_SM_H__
#define __MESON_SM_H__

ssize_t meson_sm_read_efuse(uintptr_t offset, void *buffer, size_t size);
int meson_sm_get_serial(void *buffer, size_t size);

enum {
	SM_USB_BOOT_CLEAR = 1,
	SM_USB_BOOT_FORCE = 2,
	SM_USB_BOOT_RECOVERY = 3,
	SM_USB_BOOT_PANIC_DUMP = 4,
};

int meson_sm_set_usb_boot(unsigned int func);

#endif /* __MESON_SM_H__ */
