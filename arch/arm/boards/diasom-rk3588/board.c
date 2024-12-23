/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <common.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <init.h>

static int __init diasom_rk3588_probe(struct device *dev)
{
	barebox_set_hostname("diasom");

	defaultenv_append_directory(defaultenv_diasom_rk3588);

	return 0;
}

static const struct of_device_id diasom_rk3588_of_match[] = {
	{ .compatible = "diasom,ds-rk3588-btb" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_rk3588_of_match);

static struct driver diasom_rk3588_driver = {
	.name = "board-ds-rk3588",
	.probe = diasom_rk3588_probe,
	.of_compatible = diasom_rk3588_of_match,
};
coredevice_platform_driver(diasom_rk3588_driver);
