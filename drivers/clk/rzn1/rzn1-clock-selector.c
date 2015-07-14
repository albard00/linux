/*
 * Copyright (C) 2014 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/of_address.h>

#include "rzn1-clock.h"


static uint32_t ioreg[32];
static int ioregcnt;

static void __init rzn1_clock_selector_init(struct device_node *node)
{
	const char *clk_name = node->name;
	struct clk *clk;
	int nparents  = 0;
	const char **parent_names;
	void __iomem *reg = NULL;
	u32 mask = 0;
	u32 shift = 0;
	int i;
	u8 clk_mux_flags = 0;

	nparents = of_count_phandle_with_args(node, "clocks", "#clock-cells");
	pr_devel("%s %s %d parents\n", __func__, node->name, nparents);

	parent_names = kzalloc((sizeof(char *) * nparents),
			GFP_KERNEL);
	if (!parent_names)
		return;

	for (i = 0; i < nparents; i++)
		parent_names[i] = of_clk_get_parent_name(node, i);
	/*
	 * TODO Find clock selector register & bits.. unspecified for
	 * ALL of them for the minute
	 */
	reg = of_iomap(node, 0);
	if (!reg) {
		reg = &ioreg[ioregcnt++];
		pr_debug("%s: %s MISSING REG property, faking it\n", __func__,
			node->name);
	}
	if (of_property_read_u32(node, "renesas,rzn1-bit-mask", &mask)) {
		mask = 0xf;
		pr_debug("%s: missing bit-mask property for %s\n",
			__func__, node->name);
	}

	if (of_property_read_u32(node, "renesas,rzn1-bit-shift", &shift)) {
		shift = __ffs(mask);
		mask >>= shift;
		pr_debug("%s: bit-shift property defaults to 0x%x for %s\n",
				__func__, shift, node->name);
	}
	clk = clk_register_mux_table(NULL, clk_name, parent_names, nparents,
			0, reg, shift, mask, clk_mux_flags,
			NULL, NULL);
	if (!IS_ERR(clk))
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
}

CLK_OF_DECLARE(rzn1_selector_clk,
		"renesas,rzn1-clock-selector", rzn1_clock_selector_init);
