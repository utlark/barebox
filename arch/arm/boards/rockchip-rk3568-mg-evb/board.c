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

static void rk3568_mg_evb_disable_device(struct device_node *root,
					 const char *label)
{
	struct device_node *np = of_find_node_by_name(root, label);
	if (np)
		of_device_disable(np);
}

static int rk3568_mg_evb_probe_i2c(struct i2c_adapter *adapter, int addr)
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

static int rk3568_mg_evb_i2c_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = i2c_get_adapter(4);
	if (!adapter)
		return -ENODEV;

	/* i2c4@0x10 */
	if (rk3568_mg_evb_probe_i2c(adapter, 0x10)) {
		pr_warn("ES8388 not found, disabling sound node.\n");
		rk3568_mg_evb_disable_device(root, "sound_es8388");
	}

	return 0;
}

static __init int rk3568_mg_evb_fixup(void)
{
	if (of_machine_is_compatible("macro,rk3568-evb"))
		of_register_fixup(rk3568_mg_evb_i2c_fixup, NULL);

	return 0;
}
postcore_initcall(rk3568_mg_evb_fixup);

static int __init rk3568_mg_evb_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	barebox_set_model("MG RK3568 EVB");
	barebox_set_hostname("rk3568-mg-evb");

	if (bootsource == BOOTSOURCE_MMC && instance == 0)
		of_device_enable_path("/chosen/environment-sd");
	else
		of_device_enable_path("/chosen/environment-emmc");

	rk3568_bbu_mmc_register("sd", 0, "/dev/mmc0");
	rk3568_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT,
				"/dev/mmc1");

	if (IS_ENABLED(CONFIG_DEFAULT_ENVIRONMENT))
		defaultenv_append_directory(defaultenv_rk3568_mg_evb);

	return 0;
}

static const struct of_device_id rk3568_mg_evb_of_match[] = {
	{ .compatible = "macro,rk3568-evb" },
	{ },
};

static struct driver rk3568_mg_evb_board_driver = {
	.name = "board-rk3568-mg-evb",
	.probe = rk3568_mg_evb_probe,
	.of_compatible = rk3568_mg_evb_of_match,
};
coredevice_platform_driver(rk3568_mg_evb_board_driver);

BAREBOX_DEEP_PROBE_ENABLE(rk3568_mg_evb_of_match);
