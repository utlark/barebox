/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <bootsource.h>
#include <common.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <init.h>
#include <i2c/i2c.h>
#include <mach/rockchip/bbu.h>

static int diasom_rk3568_evb_probe_i2c(struct i2c_adapter *adapter, const int addr)
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

static int diasom_rk3568_evb_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = i2c_get_adapter(4);
	if (!adapter)
		return -ENODEV;

	/* i2c4@0x10 */
	if (diasom_rk3568_evb_probe_i2c(adapter, 0x10)) {
		pr_warn("ES8388 codec not found, disabling soundcard.\n");
		of_register_set_status_fixup("sound0", false);
	}

	return 0;
}

static int __init diasom_rk3568_evb_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	barebox_set_hostname("diasom-evb");

	if (bootsource == BOOTSOURCE_MMC && instance == 0)
		of_device_enable_path("/chosen/environment-sd");
	else
		of_device_enable_path("/chosen/environment-emmc");

	rk3568_bbu_mmc_register("sd", 0, "/dev/mmc0");
	rk3568_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT,
				"/dev/mmc1");

	defaultenv_append_directory(defaultenv_diasom_rk3568_evb);

	of_register_fixup(diasom_rk3568_evb_fixup, NULL);

	return 0;
}

static const struct of_device_id diasom_rk3568_evb_of_match[] = {
	{ .compatible = "diasom,ds-rk3568-evb" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_rk3568_evb_of_match);

static struct driver diasom_rk3568_evb_driver = {
	.name = "board-ds-rk3568-evb",
	.probe = diasom_rk3568_evb_probe,
	.of_compatible = diasom_rk3568_evb_of_match,
};
coredevice_platform_driver(diasom_rk3568_evb_driver);
