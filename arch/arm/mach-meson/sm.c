// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Beniamino Galvani <b.galvani@gmail.com>
 *
 * Secure monitor calls.
 */

#include <common.h>
#include <asm/arch/gx.h>
#include <asm/arch/sm.h>
#include <linux/kernel.h>

#define FN_GET_SHARE_MEM_INPUT_BASE	0x82000020
#define FN_GET_SHARE_MEM_OUTPUT_BASE	0x82000021
#define FN_EFUSE_READ			0x82000030
#define FN_EFUSE_WRITE			0x82000031
#define FN_JTAG_ON			0x82000040
#define FN_JTAG_OFF			0x82000041
#define FN_CHIP_ID			0x82000044
#define FN_USB_BOOT_FUNC		0x82000043
#define FN_CHIP_ID			0x82000044

static void *shmem_input;
static void *shmem_output;

static void meson_init_shmem(void)
{
	struct pt_regs regs;

	if (shmem_input && shmem_output)
		return;

	regs.regs[0] = FN_GET_SHARE_MEM_INPUT_BASE;
	smc_call(&regs);
	shmem_input = (void *)regs.regs[0];

	regs.regs[0] = FN_GET_SHARE_MEM_OUTPUT_BASE;
	smc_call(&regs);
	shmem_output = (void *)regs.regs[0];

	debug("Secure Monitor shmem: 0x%p 0x%p\n", shmem_input, shmem_output);
}

ssize_t meson_sm_read_efuse(uintptr_t offset, void *buffer, size_t size)
{
	struct pt_regs regs;

	meson_init_shmem();

	regs.regs[0] = FN_EFUSE_READ;
	regs.regs[1] = offset;
	regs.regs[2] = size;

	smc_call(&regs);

	if (regs.regs[0] == 0)
		return -1;

	memcpy(buffer, shmem_output, min(size, regs.regs[0]));

	return regs.regs[0];
}

#define SM_CHIP_ID_LENGTH	119
#define SM_CHIP_ID_OFFSET	4
#define SM_CHIP_ID_SIZE		12

int meson_sm_get_serial(void *buffer, size_t size)
{
	struct pt_regs regs;

	meson_init_shmem();

	regs.regs[0] = FN_CHIP_ID;
	regs.regs[1] = 0;
	regs.regs[2] = 0;

	smc_call(&regs);

	if (regs.regs[0] == 0)
		return -1;

	memcpy(buffer, shmem_output + SM_CHIP_ID_OFFSET,
	       min_t(size_t, size, SM_CHIP_ID_OFFSET));

	return 0;
}

int meson_sm_set_usb_boot(unsigned int func)
{
	struct pt_regs regs;

	if (func < SM_USB_BOOT_CLEAR || func > SM_USB_BOOT_PANIC_DUMP)
		return -EINVAL;

	regs.regs[0] = FN_USB_BOOT_FUNC;
	regs.regs[1] = func;
	regs.regs[2] = 0;

	smc_call(&regs);

	return 0;
}

int meson_sm_setup_jtag(bool enable, unsigned int state)
{
	struct pt_regs regs;

	regs.regs[0] = enable ? FN_JTAG_ON : FN_JTAG_OFF;
	regs.regs[1] = state;
	regs.regs[2] = 0;

	smc_call(&regs);

	return 0;
}

static int do_sm_serial(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	ulong address;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	address = simple_strtoul(argv[1], NULL, 0);

	ret = meson_sm_get_serial((void *)address, SM_CHIP_ID_SIZE);
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_sm_usb_boot(cmd_tbl_t *cmdtp, int flag, int argc,
			  char *const argv[])
{
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = meson_sm_set_usb_boot(simple_strtoul(argv[1], NULL, 0));
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_sm_jtag(cmd_tbl_t *cmdtp, int flag, int argc,
		      char *const argv[])
{
	int ret;
	int state = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (strcmp(argv[1], "enable") && strcmp(argv[1], "disable"))
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "enable")) {
		if (argc < 3) {
			printf("enable command needs a state parameter\n");
			return CMD_RET_FAILURE;
		}
		state = simple_strtoul(argv[2], NULL, 0);
	}

	ret = meson_sm_setup_jtag(!strcmp(argv[1], "enable") ? true : false,
				  state);
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int do_efuse(cmd_tbl_t *cmdtp, int flag, int argc,
		    char *const argv[])
{
	ulong offset;
	ulong size;
	ulong dest;

	if (argc < 5 || strcmp(argv[1], "read"))
		return CMD_RET_FAILURE;

	offset = simple_strtoul(argv[2], NULL, 0);
	size = simple_strtoul(argv[3], NULL, 0);
	dest = simple_strtoul(argv[4], NULL, 0);

	if (meson_sm_read_efuse(offset, (void *)dest, size) != -1)
		return CMD_RET_SUCCESS;

	return CMD_RET_FAILURE;
}

static cmd_tbl_t cmd_sm_sub[] = {
	U_BOOT_CMD_MKENT(serial, 1, 1, do_sm_serial, "", ""),
	U_BOOT_CMD_MKENT(usb_boot, 2, 1, do_sm_usb_boot, "", ""),
	U_BOOT_CMD_MKENT(jtag, 2, 2, do_sm_jtag, "", ""),
};

static int do_sm(cmd_tbl_t *cmdtp, int flag, int argc,
		 char *const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'sm' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_sm_sub[0], ARRAY_SIZE(cmd_sm_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

U_BOOT_CMD(
	efuse, 5, 0, do_efuse,
	"OTP fuse read",
	"efuse read <offset> <length> <address> - read length bytes from efuse offset to memory address"
);

U_BOOT_CMD(
	sm, 5, 0, do_sm,
	"Secure Monitor Control",
	"serial <address> - read chip unique id to memory address\n"
	"sm usb_boot <func> - setup usb boot mode for next boot\n"
	"sm jtag enable/disable [state] - enable of disable JTAG interface state"
);
