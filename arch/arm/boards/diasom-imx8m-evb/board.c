// SPDX-License-Identifier: GPL-2.0-or-later

#include <bootsource.h>
#include <common.h>
#include <envfs.h>
#include <init.h>
#include <deep-probe.h>
#include <mach/imx/bbu.h>

static int diasom_imx8m_evb_probe(struct device *dev)
{
	int flag = BBU_HANDLER_FLAG_DEFAULT;

	barebox_set_hostname("ds-imx8m-evb");

	switch (bootsource_get()) {
	case BOOTSOURCE_MMC:
		if (bootsource_get_instance() == 1) {
			pr_info("Boot from SD...\n");
			of_device_enable_path("/chosen/environment-sd");
			flag = 0;
			break;
		}
		fallthrough;
	default:
		pr_info("Boot from eMMC...\n");
		of_device_enable_path("/chosen/environment-emmc");
		break;
	}

	imx8m_bbu_internal_mmcboot_register_handler("eMMC", "/dev/mmc2", flag);

	defaultenv_append_directory(defaultenv_diasom_imx8m_evb);

	return 0;
}

static const struct of_device_id diasom_imx8m_evb_of_match[] = {
	{ .compatible = "diasom,ds-imx8m-evb", },
	{ }
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_imx8m_evb_of_match);

static struct driver diasom_imx8m_evb_driver = {
	.name = "board-ds-imx8m-evb",
	.probe = diasom_imx8m_evb_probe,
	.of_compatible = diasom_imx8m_evb_of_match,
};
coredevice_platform_driver(diasom_imx8m_evb_driver);
