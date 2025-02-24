/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <common.h>
#include <asm/barebox-arm.h>
#include <mach/rockchip/hardware.h>
#include <mach/rockchip/atf.h>
#include <debug_ll.h>

static __noreturn inline void start_rk3588_diasom(void *fdt)
{
	putc_ll('>');

	if (current_el() == 3)
		relocate_to_adr_full(RK3588_BAREBOX_LOAD_ADDRESS);
	else
		relocate_to_current_adr();

	setup_c();

	rk3588_barebox_entry(fdt);
}

ENTRY_FUNCTION(start_rk3588_diasom_btb_evb, r0, r1, r2)
{
	extern char __dtb_rk3588_diasom_btb_evb_socket_start[];

	start_rk3588_diasom(__dtb_rk3588_diasom_btb_evb_socket_start);
}
