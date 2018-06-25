// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Beniamino Galvani <b.galvani@gmail.com>
 *
 * Secure monitor calls.
 */

#include <common.h>
#include <asm/arch/gx.h>
#include <linux/kernel.h>

#define FN_GET_SHARE_MEM_INPUT_BASE	0x82000020
#define FN_GET_SHARE_MEM_OUTPUT_BASE	0x82000021
#define FN_EFUSE_READ			0x82000030
#define FN_EFUSE_WRITE			0x82000031

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

static do_efuse(cmd_tbl_t *cmdtp, int flag, int argc,
		char *const argv[])
{
	unsigned offset;
	unsigned size;
	unsigned dest;

	if (argc < 5 || strcmp(argv[1], "read"))
		return CMD_RET_FAILURE;

	offset = simple_strtol(argv[2], NULL, 0);
	size = simple_strtol(argv[3], NULL, 0);
	dest = simple_strtol(argv[4], NULL, 0);

	if (meson_sm_read_efuse(offset, dest, size) != -1)
		return CMD_RET_SUCCESS;

	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	efuse, 5, 0, do_efuse,
	"OTP fuse read",
	"efuse read <offset> <length> <address> - read length bytes from efuse offset to memory address"
);
