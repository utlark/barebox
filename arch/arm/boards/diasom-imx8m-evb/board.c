// SPDX-License-Identifier: GPL-2.0-or-later

#include <bootsource.h>
#include <common.h>
#include <deep-probe.h>
#include <envfs.h>
#include <init.h>
#include <i2c/i2c.h>
#include <mach/imx/bbu.h>

static void diasom_imx8m_evb_enable_device(struct device_node *root,
					   const char *label)
{
	struct device_node *np = of_find_node_by_name(root, label);
	if (np)
		of_device_enable(np);
}

static int diasom_imx8m_evb_probe_i2c(struct i2c_adapter *adapter, const int addr)
{
	u8 buf[1];
	struct i2c_msg msg = {
		.addr = addr,
		.buf = buf,
		.len = sizeof(buf),
		.flags = I2C_M_RD,
	};

	return (i2c_transfer(adapter, &msg, 1) == 1) ? 0: -ENODEV;
}

static int diasom_imx8m_evb_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = i2c_get_adapter(2);
	if (!adapter)
		return -ENODEV;

	if (!diasom_imx8m_evb_probe_i2c(adapter, 0x54)) {
		pr_info("Camera AR0234 detected.\n");
		diasom_imx8m_evb_enable_device(root, "camera@3d");
	} else {
		pr_info("Camera AR0234 not detected. Assume using OV5640.\n");
		diasom_imx8m_evb_enable_device(root, "camera@3c");
	}

	return 0;
}

static int diasom_imx8m_evb_probe(struct device *dev)
{
	int flag = BBU_HANDLER_FLAG_DEFAULT;

	barebox_set_hostname("diasom-evb");

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

	of_register_fixup(diasom_imx8m_evb_fixup, NULL);

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
