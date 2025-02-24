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

static struct i2c_adapter *diasom_rk3568_i2c_get_adapter(const int nr)
{
	char *alias = basprintf("i2c%i", nr);
	struct device *dev;

	dev = of_device_enable_and_register_by_alias(alias);
	free(alias);
	if (!dev)
		return NULL;

	return i2c_get_adapter(nr);
}

/*
	Cameras mapping:
		camera0 = XC7160/I2C4
		camera1 = IMX335/I2C4
		camera2 = IMX335/I2C7
		camera3 = DS90UB954/I2C7 -> DS90UB953 -> AR0233
		camera4 = IMX415/I2C4
		camera5 = IMX415/I2C7
*/

static int diasom_rk3568_sony_camera_detect(struct i2c_adapter *adapter,
					    const char *imx335,
					    const char *imx415)
{
#define CAMERA_I2C_ADDR		0x1a
	struct i2c_client client;
	u8 buf[1];
	int ret;

	if (diasom_rk3568_probe_i2c(adapter, CAMERA_I2C_ADDR))
		return -ENODEV;

	client.adapter = adapter;
	client.addr = CAMERA_I2C_ADDR;
	/* 0x4001 == 1 or 3 -> IMX415 */
	ret = i2c_read_reg(&client, 0x4001 | I2C_ADDR_16_BIT, buf, sizeof(buf));
	if (ret == sizeof(buf) && (buf[0] == 1 || buf[0] == 3)) {
		pr_info("Camera IMX415 detected.\n");
		of_register_set_status_fixup(imx415, true);
		return 0;
	}

	pr_info("Camera IMX335 detected.\n");
	of_register_set_status_fixup(imx335, true);

	return 0;
}

static int diasom_rk3568_evb_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = diasom_rk3568_i2c_get_adapter(4);
	if (!adapter)
		return -ENODEV;

	if (diasom_rk3568_probe_i2c(adapter, 0x10)) {
		pr_warn("ES8388 codec not found, disabling soundcard.\n");
		of_register_set_status_fixup("sound0", false);
	}

	if (!diasom_rk3568_sony_camera_detect(adapter, "camera1", "camera4"))
		return 0;

	pr_info("Assume camera XC7160 is used.\n");
	of_register_set_status_fixup("camera0", true);

	return 0;
}

static int diasom_rk3568_evb_ver1_3_0_fixup(struct device_node *root,
					    void *unused)
{
	struct i2c_adapter *adapter = diasom_rk3568_i2c_get_adapter(7);
	if (!adapter)
		return -ENODEV;

	if (!diasom_rk3568_probe_i2c(adapter, 0x30)) {
		pr_info("FPD-Link deserializer detected.\n");
		of_register_set_status_fixup("camera3", true);
	} else
		diasom_rk3568_sony_camera_detect(adapter, "camera2", "camera5");

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

static int __init diasom_rk3568_machine_id(void)
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
of_populate_initcall(diasom_rk3568_machine_id);

static bool __init diasom_rk3568_load_overlay(const void *ovl)
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

static int __init diasom_rk3568_init(void)
{
	bool do_probe = false;
	int ret = 0;

	if (of_machine_is_compatible("diasom,ds-rk3568-som")) {
		struct i2c_adapter *adapter =
			diasom_rk3568_i2c_get_adapter(0);
		void *som_ovl;

		if (!adapter) {
			pr_err("Cannot determine SOM version.\n");
			return -ENOTSUPP;
		}

		if (!diasom_rk3568_probe_i2c(adapter, 0x1c)) {
			extern char __dtbo_rk3568_diasom_som_ver2_start[];

			som_ovl = __dtbo_rk3568_diasom_som_ver2_start;
			pr_info("SOM version 2+ detected.\n");
		} else {
			extern char __dtbo_rk3568_diasom_som_ver1_start[];

			som_ovl = __dtbo_rk3568_diasom_som_ver1_start;
			pr_info("SOM version 1 detected.\n");
		}

		if (diasom_rk3568_load_overlay(som_ovl))
			do_probe = true;
	} else
		return 0;

	if (of_machine_is_compatible("diasom,ds-rk3568-som-evb")) {
		struct i2c_adapter *adapter =
			diasom_rk3568_i2c_get_adapter(4);
		void *evb_ovl;

		if (!adapter) {
			pr_err("Cannot determine EVB version.\n");
			ret = -ENOTSUPP;
			goto out;
		}

		if (!diasom_rk3568_probe_i2c(adapter, 0x70)) {
			extern char __dtbo_rk3568_diasom_som_evb_ver1_3_0_start[];

			pr_info("EVB version 1.3.0+ detected.\n");
			evb_ovl = __dtbo_rk3568_diasom_som_evb_ver1_3_0_start;

			of_register_fixup(diasom_rk3568_evb_ver1_3_0_fixup, NULL);
		} else if (!diasom_rk3568_probe_i2c(adapter, 0x50)) {
			extern char __dtbo_rk3568_diasom_som_evb_ver1_2_1_start[];

			pr_info("EVB version 1.2.1+ detected.\n");
			evb_ovl = __dtbo_rk3568_diasom_som_evb_ver1_2_1_start;

			of_register_fixup(diasom_rk3568_evb_fixup, NULL);
		} else {
			extern char __dtbo_rk3568_diasom_som_evb_ver1_1_0_start[];

			pr_info("EVB version 1.2.0 or earlier detected.\n");
			evb_ovl = __dtbo_rk3568_diasom_som_evb_ver1_1_0_start;

			of_register_fixup(diasom_rk3568_evb_fixup, NULL);
		}

		if (diasom_rk3568_load_overlay(evb_ovl))
			do_probe = true;
	};

out:
	if (do_probe) {
		struct device_node *root = of_get_root_node();

		of_probe();

		/* Ensure reload aliases & model name */
		of_set_root_node(NULL);
		of_set_root_node(root);
	}

	return ret;
}
device_initcall(diasom_rk3568_init);

static int __init diasom_rk3568_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	barebox_set_hostname("diasom");

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

	defaultenv_append_directory(defaultenv_diasom_rk3568);

	return 0;
}

static const struct of_device_id __init diasom_rk3568_of_match[] = {
	{ .compatible = "diasom,ds-rk3568-som" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_rk3568_of_match);

static struct driver __init diasom_rk3568_driver = {
	.name = "board-ds-rk3568-som",
	.probe = diasom_rk3568_probe,
	.of_compatible = diasom_rk3568_of_match,
};
coredevice_platform_driver(diasom_rk3568_driver);
