/*
 * RZ/N1 sysctrl access API
 *
 * Copyright (C) 2014-2016 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/sysctrl-rzn1.h>

static void __iomem *sysctrl_base_addr;


static void __iomem *watchdog_base_addr;

void __init rzn1_sysctrl_init(void)
{
	if (sysctrl_base_addr)
		return;
	sysctrl_base_addr = ioremap(RZN1_SYSTEM_CTRL_BASE,
					RZN1_SYSTEM_CTRL_SIZE);
	BUG_ON(!sysctrl_base_addr);

	watchdog_base_addr = ioremap(RZN1_WATCHDOG0_BASE,
					3 * RZN1_WATCHDOG0_SIZE);
	BUG_ON(!watchdog_base_addr);
}

void __iomem *rzn1_sysctrl_base(void)
{
	BUG_ON(!sysctrl_base_addr);
	return sysctrl_base_addr;
}
