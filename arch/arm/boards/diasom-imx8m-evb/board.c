// SPDX-License-Identifier: GPL-2.0-or-later

#include <bootsource.h>
#include <common.h>
#include <deep-probe.h>
#include <envfs.h>
#include <globalvar.h>
#include <init.h>
#include <i2c/i2c.h>
#include <mach/imx/bbu.h>

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

static int diasom_imx8m_evb_fixup(struct device_node *, void *)
{
	struct i2c_adapter *adapter;

	adapter = i2c_get_adapter(2);
	if (adapter) {
		if (!diasom_imx8m_evb_probe_i2c(adapter, 0x3d)) {
			pr_info("Camera AR0234 detected.\n");
			of_register_set_status_fixup("camera1", true);
			return 0;
		} else {
			pr_info("Assume camera OV5640 is used.\n");
			of_register_set_status_fixup("camera0", true);
			return 0;
		}
	}

	pr_warn("Camera not detected. All camera nodes are disabled.\n");

	return 0;
}

static int diasom_imx8m_evb_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();
	int flag = BBU_HANDLER_FLAG_DEFAULT;

	barebox_set_hostname("diasom-evb");

	if (bootsource != BOOTSOURCE_MMC || !instance) {
		if (bootsource != BOOTSOURCE_MMC) {
			pr_info("Boot source: %s, instance %i\n",
				bootsource_to_string(bootsource),
				instance);
			globalvar_add_simple("board.bootsource",
					     bootsource_to_string(bootsource));
		} else
			of_device_enable_path("/chosen/environment-emmc");
	} else {
		of_device_enable_path("/chosen/environment-sd");
		flag = 0;
	}

	imx8m_bbu_internal_mmcboot_register_handler("eMMC", "/dev/mmc0", flag);

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
