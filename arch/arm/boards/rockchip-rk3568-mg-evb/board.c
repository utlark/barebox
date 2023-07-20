/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <bootsource.h>
#include <common.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <init.h>
#include <mach/rockchip/bbu.h>

static int rk3568_mg_evb_probe(struct device *dev)
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
