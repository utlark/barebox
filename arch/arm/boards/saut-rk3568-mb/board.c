/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */
/* SPDX-FileCopyrightText: Yuri Zubkov <utlark@ya.ru> */

#include <bootsource.h>
#include <common.h>
#include <aiodev.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <globalvar.h>
#include <init.h>
#include <machine_id.h>
#include <mci.h>
#include <i2c/i2c.h>
#include <mach/rockchip/bbu.h>

#define KEY_DOWN_MIN_VAL	0
#define KEY_DOWN_MAX_VAL	40

static int saut_rk3568_probe_i2c(struct i2c_adapter *adapter, const int addr)
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

static struct i2c_adapter *saut_rk3568_i2c_get_adapter(const int nr)
{
	char *alias = basprintf("i2c%i", nr);
	struct device *dev;

	dev = of_device_enable_and_register_by_alias(alias);
	free(alias);
	if (!dev)
		return NULL;

	return i2c_get_adapter(nr);
}

static int __init saut_rk3568_check_recovery(void)
{
	struct aiochannel *aio_ch0;
	struct device *aio_dev;
	int ret, val;

	if (!of_machine_is_compatible("diasom,ds-rk3568-som"))
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
device_initcall(saut_rk3568_check_recovery);

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

static unsigned __init extract_psn(struct mci *mci)
{
	if (!IS_SD(mci)) {
		if (mci->version > MMC_VERSION_1_4)
			return UNSTUFF_BITS(mci->cid, 16, 32);
		else
			return UNSTUFF_BITS(mci->cid, 16, 24);
	}

	return UNSTUFF_BITS(mci->csd, 24, 32);
}

static int __init saut_rk3568_machine_id(void)
{
	struct mci *mci;
	unsigned serial;

	if (!of_machine_is_compatible("rockchip,rk3568"))
		return 0;

	mci = mci_get_device_by_name("mmc0");
	if (!mci) {
		pr_err("Unable to get MCI device!\n");
		return -ENODEV;
	}

	serial = extract_psn(mci);

	pr_info("Setup Machine ID from EMMC serial: %u\n", serial);

	machine_id_set_hashable(&serial, sizeof(serial));

	return 0;
}
of_populate_initcall(saut_rk3568_machine_id);

static bool __init saut_rk3568_load_overlay(const void *ovl)
{
	if (ovl) {
		int ret;

		ret = of_overlay_apply_dtbo(of_get_root_node(), ovl);
		if (!ret)
			return true;

		pr_err("Cannot apply overlay: %pe!\n", ERR_PTR(ret));
	}

	return false;
}

static int __init saut_rk3568_init(void)
{
	bool do_probe = false;
	int ret = 0;

	if (of_machine_is_compatible("diasom,ds-rk3568-som")) {
		struct i2c_adapter *adapter =
			saut_rk3568_i2c_get_adapter(0);
		void *som_ovl;

		if (!adapter) {
			pr_err("Cannot determine SOM version.\n");
			return -ENOTSUPP;
		}

		if (!saut_rk3568_probe_i2c(adapter, 0x1c)) {
			extern char __dtbo_rk3568_diasom_som_ver2_start[];

			som_ovl = __dtbo_rk3568_diasom_som_ver2_start;
			pr_info("SOM version 2+ detected.\n");
		} else {
			extern char __dtbo_rk3568_diasom_som_ver1_start[];

			som_ovl = __dtbo_rk3568_diasom_som_ver1_start;
			pr_info("SOM version 1 detected.\n");
		}

		if (saut_rk3568_load_overlay(som_ovl))
			do_probe = true;
	} else
		return 0;

	if (do_probe) {
		struct device_node *root = of_get_root_node();

		of_probe();

		/* Ensure reload aliases & model name */
		of_set_root_node(NULL);
		of_set_root_node(root);
	}

	return ret;
}
device_initcall(saut_rk3568_init);

static int __init saut_rk3568_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	barebox_set_hostname("saut");

	if (bootsource != BOOTSOURCE_MMC || !instance) {
		if (bootsource != BOOTSOURCE_MMC) {
			pr_info("Boot source: %s, instance %i\n",
				bootsource_to_string(bootsource),
				instance);
			globalvar_add_simple("board.bootsource",
					     bootsource_to_string(bootsource));
		} else
			of_device_enable_path("/chosen/environment-emmc");
	} else
		of_device_enable_path("/chosen/environment-sd");

	rk3568_bbu_mmc_register("sd", 0, "/dev/mmc1");
	rk3568_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT,
				"/dev/mmc0");

	defaultenv_append_directory(defaultenv_saut_rk3568);

	return 0;
}

static const struct of_device_id __init saut_rk3568_of_match[] = {
	{ .compatible = "diasom,ds-rk3568-som" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(saut_rk3568_of_match);

static struct driver __init saut_rk3568_driver = {
	.name = "board-ds-rk3568-som",
	.probe = saut_rk3568_probe,
	.of_compatible = saut_rk3568_of_match,
};
coredevice_platform_driver(saut_rk3568_driver);
