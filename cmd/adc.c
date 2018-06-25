// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */
#include <common.h>
#include <command.h>
#include <dm.h>
#include <adc.h>
#include <div64.h>

static int do_adc_list(cmd_tbl_t *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_ADC, &dev);
	if (ret) {
		printf("No available ADC device\n");
		return CMD_RET_FAILURE;
	}

	do {
		printf("- %s\n", dev->name);

		ret = uclass_next_device(&dev);
		if (ret)
			return CMD_RET_FAILURE;
	} while (dev);

	return CMD_RET_SUCCESS;
}

static int do_adc_info(cmd_tbl_t *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct udevice *dev;
	unsigned int data_mask;
	int ret, vss, vdd;

	if (argc < 2)
		return CMD_RET_USAGE;

	ret = uclass_get_device_by_name(UCLASS_ADC, argv[1], &dev);
	if (ret) {
		printf("Unknown ADC device %s\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	printf("ADC Device '%s' :\n", argv[1]);

	ret = adc_data_mask(dev, &data_mask);
	if (!ret)
		printf("data mask: %x\n", data_mask);

	ret = adc_vdd_value(dev, &vdd);
	if (!ret)
		printf("vdd: %duV\n", vdd);

	ret = adc_vss_value(dev, &vss);
	if (!ret)
		printf("vss: %duV\n", vss);

	return CMD_RET_SUCCESS;
}

static int do_adc_single(cmd_tbl_t *cmdtp, int flag, int argc,
			 char *const argv[])
{
	unsigned int data;
	int ret;

	if (argc < 3)
		return CMD_RET_USAGE;

	ret = adc_channel_single_shot(argv[1], simple_strtol(argv[2], NULL, 0),
				      &data);
	if (ret) {
		printf("Error getting single shot for device %s channel %s\n",
		       argv[1], argv[2]);
		return CMD_RET_FAILURE;
	}

	printf("%u\n", data);

	return CMD_RET_SUCCESS;
}

static int do_adc_range(cmd_tbl_t *cmdtp, int flag, int argc,
			 char *const argv[])
{
	unsigned int samples = 1;
	unsigned int min, max;
	uint64_t data = 0;
	int ret, i;

	if (argc < 5)
		return CMD_RET_USAGE;

	min = simple_strtol(argv[3], NULL, 0);
	max = simple_strtol(argv[4], NULL, 0);
	if (argc > 5)
		samples = simple_strtol(argv[5], NULL, 0);

	for (i = 0 ; i < samples ; ++i) {
		unsigned shot;
		ret = adc_channel_single_shot(argv[1],
					      simple_strtol(argv[2], NULL, 0),
					      &shot);
		if (ret) {
			printf("Error getting single shot for device %s channel %s\n",
			       argv[1], argv[2]);
			return CMD_RET_FAILURE;
		}

		data += shot;
	}

	data /= samples;

	if (data < min || data > max)
		return CMD_RET_FAILURE;

	printf("%u < %llu < %u\n", min, data, max);

	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_adc_sub[] = {
	U_BOOT_CMD_MKENT(list, 1, 1, do_adc_list, "", ""),
	U_BOOT_CMD_MKENT(info, 2, 1, do_adc_info, "", ""),
	U_BOOT_CMD_MKENT(single, 3, 1, do_adc_single, "", ""),
	U_BOOT_CMD_MKENT(range, 6, 1, do_adc_range, "", ""),
};

static int do_adc(cmd_tbl_t *cmdtp, int flag, int argc,
		  char *const argv[])
{
	cmd_tbl_t *c;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'adc' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_adc_sub[0], ARRAY_SIZE(cmd_adc_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

static char adc_help_text[] =
	"list - list ADC devices\n"
	"adc info <name> - Get ADC device info\n"
	"adc single <name> <channel> - Get Single data of ADC device channel\n"
	"adc range <name> <channel> <min> <max> [samples] - Check ADC device channel value is in a specified range";

U_BOOT_CMD(adc, 7, 1, do_adc, "ADC sub-system", adc_help_text);
