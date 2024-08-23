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
#include <machine_id.h>
#include <mci.h>
#include <i2c/i2c.h>
#include <mach/rockchip/bbu.h>

#define KEY_DOWN_MIN_VAL	0
#define KEY_DOWN_MAX_VAL	40

static int diasom_rk3568_probe_i2c(struct i2c_adapter *adapter, const int addr)
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

	if (diasom_rk3568_probe_i2c(adapter, 0x10)) {
		pr_warn("ES8388 codec not found, disabling soundcard.\n");
		of_register_set_status_fixup("sound0", false);
	}

	if (!diasom_rk3568_probe_i2c(adapter, 0x1a)) {
		pr_info("Camera IMX335 detected.\n");
		of_register_set_status_fixup("camera1", true);
		return 0;
	}

	pr_info("Assume camera XC7160 is used.\n");
	of_register_set_status_fixup("camera0", true);

	return 0;
}

static int diasom_rk3568_evb_ver3_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = i2c_get_adapter(7);
	if (!adapter)
		return -ENODEV;

	if (!diasom_rk3568_probe_i2c(adapter, 0x1a)) {
		pr_info("Camera IMX335 detected.\n");
		of_register_set_status_fixup("camera2", true);
	}

	return 0;
}

static int __init diasom_rk3568_check_recovery(void)
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
device_initcall(diasom_rk3568_check_recovery);

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
		__res |= resp[__off-1] << ((32 - __shft) % 32);		\
		__res & __mask;						\
	})

static unsigned extract_psn(struct mci *mci)
{
	if (!IS_SD(mci)) {
		if (mci->version > MMC_VERSION_1_4)
			return UNSTUFF_BITS(mci->cid, 16, 32);
		else
			return UNSTUFF_BITS(mci->cid, 16, 24);
	}

	return UNSTUFF_BITS(mci->csd, 24, 32);
}

static int __init diasom_rk3568_machine_id(void)
{
	struct mci *mci;
	unsigned serial;

	if (!of_machine_is_compatible("diasom,ds-rk3568-som"))
		return 0;

	mci = mci_get_device_by_name("mmc1");
	if (!mci) {
		pr_err("Unable to get MCI device!\n");
		return -ENODEV;
	}

	serial = extract_psn(mci);

	pr_info("Setup Machine ID from EMMC serial: %u\n", serial);

	machine_id_set_hashable(&serial, sizeof(serial));

	return 0;
}
of_populate_initcall(diasom_rk3568_machine_id);

static int __init diasom_rk3568_late_init(void)
{
	if (of_machine_is_compatible("diasom,ds-rk3568-som")) {
		struct i2c_adapter *adapter = i2c_get_adapter(0);

		if (!adapter) {
			pr_err("Cannot determine SOM version.\n");
			return 0;
		}

		if (!diasom_rk3568_probe_i2c(adapter, 0x1c)) {
			extern char __dtbo_rk3568_diasom_som_ver2_start[];
			struct device_node *overlay;

			pr_info("SOM version 2 or above detected.\n");

			overlay = of_unflatten_dtb(__dtbo_rk3568_diasom_som_ver2_start, INT_MAX);
			of_overlay_apply_tree(of_get_root_node(), overlay);
			of_probe();
		} else
			pr_info("SOM version 1 detected.\n");
	}

	if (of_machine_is_compatible("diasom,ds-rk3568-som-evb")) {
		struct i2c_adapter *adapter = i2c_get_adapter(4);

		if (!adapter) {
			pr_err("Cannot determine board version.\n");
			return 0;
		}

		if (!diasom_rk3568_probe_i2c(adapter, 0x70)) {
			extern char __dtbo_rk3568_diasom_evb_ver3_start[];
			struct device_node *overlay;

			pr_info("EVB version 3 or above detected.\n");

			overlay = of_unflatten_dtb(__dtbo_rk3568_diasom_evb_ver3_start, INT_MAX);
			of_overlay_apply_tree(of_get_root_node(), overlay);
			of_probe();

			of_register_fixup(diasom_rk3568_evb_ver3_fixup, NULL);
		} else {
			pr_info("EVB version 2 or earlier detected.\n");
			of_register_fixup(diasom_rk3568_evb_fixup, NULL);
		}
	};

	return 0;
}
late_initcall(diasom_rk3568_late_init);

static int __init diasom_rk3568_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	barebox_set_hostname("diasom");

	if (bootsource != BOOTSOURCE_MMC || instance) {
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

	rk3568_bbu_mmc_register("sd", 0, "/dev/mmc0");
	rk3568_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT,
				"/dev/mmc1");

	defaultenv_append_directory(defaultenv_diasom_rk3568);

	return 0;
}

static const struct of_device_id diasom_rk3568_of_match[] = {
	{ .compatible = "diasom,ds-rk3568-som" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_rk3568_of_match);

static struct driver diasom_rk3568_driver = {
	.name = "board-ds-rk3568-som",
	.probe = diasom_rk3568_probe,
	.of_compatible = diasom_rk3568_of_match,
};
coredevice_platform_driver(diasom_rk3568_driver);
