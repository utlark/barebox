/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <bootsource.h>
#include <common.h>
#include <aiodev.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <globalvar.h>
#include <init.h>
#include <i2c/i2c.h>
#include <mach/rockchip/bbu.h>

#define KEY_DOWN_MIN_VAL	0
#define KEY_DOWN_MAX_VAL	40

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

static int __init diasom_rk3568_evb_check_recovery(void)
{
	struct aiochannel *aio_ch0;
	struct device *aio_dev;
	int ret, val;

	if (!IS_ENABLED(CONFIG_AIODEV))
		return 0;

	if (!of_machine_is_compatible("diasom,ds-rk3568-evb"))
		return 0;

	aio_dev = of_device_enable_and_register_by_name("saradc@fe720000");
	if (!aio_dev) {
		pr_err("Unable to get ADC device!\n");
		return -ENODEV;
	}

	aio_ch0 = aiochannel_by_name("aiodev0.in_value0_mV");
	if (IS_ERR(aio_ch0)) {
		ret = PTR_ERR(aio_ch0);
		pr_err("Could not find ADC channel: %i!\n", ret);
		return ret;
	}

	ret = aiochannel_get_value(aio_ch0, &val);
	if (ret) {
		pr_err("Could not get ADC value: %i!\n", ret);
		return ret;
	}

	if ((val >= KEY_DOWN_MIN_VAL) && (val <= KEY_DOWN_MAX_VAL)) {
		pr_info("Recovery key pressed, enforce gadget mode...\n");
		globalvar_add_simple("board.recovery", "true");
	}

	return 0;
}
device_initcall(diasom_rk3568_evb_check_recovery);

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
