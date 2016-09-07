/*
 * RZ/N1 GPIO IRQ Muxer interface
 *
 * Copyright (C) 2016 Renesas Electronics Europe Limited
 *
 * Michel Pollet <michel.pollet@bp.renesas.com>, <buserror@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <asm/mach/arch.h>
#include <dt-bindings/interrupt-controller/rzn1-irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl-rzn1.h>
#include <linux/sysctrl-rzn1.h>

/*
 * This driver configures and redirects the RZ/N1 8 GPIO IRQs to their
 * respective GPIO blocks.
 * It also provides a 'direct' way to use these interrupts for drivers who are
 * not compatible with GPIO based interupts and need a raw one.
 *
 * So the driver acts as an interrupt controller with 8 'direct' forward IRQs
 * and 3 that are hooked to the GPIO blocks.
 */
/*
   This example shows the propagation for a normal GPIO IRQ, for example the
   gpio-key driver. The 'direct' interrupt isn't needed, it's passed on to the
   GPIO block, and forward on.
	GIC		gpioirq				Client Driver
	GPIO_0	+->	0				<no direct client>
		|->	8/9/10	->	DWAPB1/2/3  ->	gpio-key

   This second use case is for a driver that requires an IRQ, not a GPIO. here
   the gpioirq will have to claim the corresponding gpio to make sure DWAPB
   configures it properly.
   The direct interrupt is passed on to the client driver, and the 'gpio'
   interrupt is just passed on to the dummy handler.
	GPIO_1  +->	1       ->                      Ethernet MAC Port 1
		|->     8/9/10  ->      DWAPB1/2/3
		        dummy   <-          <-|
 */
#define DRIVER_DESC     "Renesas RZ/N1 GPIO IRQ Mux Driver"
#define DRIVER_AUTHOR   "Michel Pollet <michel.pollet@bp.renesas.com>,<buserror@gmail.com>"
#define DRIVER_VERSION  "0.1"

static const char driver_name[] = "girq_renesas";
static const char driver_desc[] = DRIVER_DESC;

enum {
	GIRQ_IRQ_FORWARD = 0,
	GIRQ_IRQ_GPIO0 = 8,
	GIRQ_IRQ_GPIO1,
	GIRQ_IRQ_GPIO2,
	GIRQ_IRQ_COUNT
};

struct girq {
	struct device *dev;
	struct device_node *np;
	struct irq_domain *irq_domain;
	struct {
		int irq;	/* this is the one from the GIC */
		int gpio_irq;	/* this is the one from the gpio_chip */
		unsigned enabled: 1, masked: 1, configured: 1, claimed: 1;
		unsigned bank : 3;
	} irq[GIRQ_IRQ_COUNT];
};

static struct girq *g_irq;

static irqreturn_t girq_handler(
	int inirq, void *dev_id)
{
	struct girq *g = dev_id;
	struct irq_desc *desc = irq_to_desc(inirq);
	unsigned long hwirq = desc->irq_data.hwirq;
	int irq;

	irq = hwirq - 32 - RZN1_IRQ_GPIO_0; /* convert to local hw_irq */

	/* this clearly shouldn't happend unless the devtree is rotten */
	BUG_ON(irq < 0 || irq > 7);

	/*
	 * If both that GPIO and that bank are mapped and ready, fire these
	 * interrupts. We first fire the GPIO bank one so it has a chance to
	 * clear the GPIO interrupt itself.
	 * This will in turn call girq_dummy_handler() somehow, however, we
	 * also then propagate the GIC interrupt we've just received by
	 * passing it on to our own hw_irq handler
	 */
	if (g->irq[irq].configured &&
	    g->irq[GIRQ_IRQ_GPIO0 + g->irq[irq].bank].enabled) {
		int gpio_block = irq_find_mapping(g->irq_domain,
			GIRQ_IRQ_GPIO0 + g->irq[irq].bank);

		/* tell the GPIO bank driver to clear the GPIO irq and forward
		 * to whomever might have requested it as an IRQ */
		generic_handle_irq(gpio_block);
	}
	if (g->irq[irq].enabled) {
		int forward = irq_find_mapping(g->irq_domain, irq);
		/* Also forward it to any chained sub-irq with that number */
		generic_handle_irq(forward);
	}
	return IRQ_HANDLED;
}

static irqreturn_t girq_dummy_handler(
	int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

/*
 * 'configuring' one of these 8 GPIO irq involves claiming it from the GIC,
 * finding which GPIO it want it muxed to, which bank of GPIO it is, then set
 * it up so the interrupt can be forwarded to the GPIO block, which in turn
 * will pass it on to the normal GPIO handler down the line.
 */
static int girq_configure_hwirq(
	struct girq *g, int hw_irq)
{
	struct gpio_chip *chip;
	u32 gpio_resbase;
	char prop[16];
	int i, gpio, bank;

	if (g->irq[hw_irq].configured)
		return 0;

	sprintf(prop, "gpioirq-%c", '0' + hw_irq);
	/* need to do that, otherwise the gpiochip of code complains on the
	 * console about the property not being present etc, even tho these
	 * are optional anyway */
	if (!of_get_property(g->np, prop, NULL))
		return -1;
	gpio = of_get_named_gpio_flags(g->np, prop, 0, 0);
	if (gpio < 0) {
		dev_err(g->dev, "%s is not available\n", prop);
		return -1;
	}
	/*
	 * Take the Linux GPIO number (assume 'random') and figure out
	 * which of the GPIO blocks it belongs to, first get the gpio_chip, then
	 * compare their 'res' property address with the gpio blocks we have in
	 * out own DT 'res' node for comparison. It is a little bit involved,
	 * but it's the cleanest way I could find to do this.
	 */
	chip = gpio_to_chip(gpio);
	if (!chip) {
		dev_err(g->dev, "no chip for GPIO %d\n", gpio);
		return -1;
	}
	/* no need to convert from be32, as we compare the same order anyway */
	gpio_resbase = *of_get_address(chip->parent->of_node, 0, NULL, NULL);

	for (bank = -1, i = 0; i < 3 && bank == -1; i++)
		if (*of_get_address(g->np, i, NULL, NULL) == gpio_resbase)
			bank = i;
	if (bank == -1) {
		dev_err(g->dev, "gpio %d is not in a suitable bank\n", gpio);
		return -1;
	}
	dev_info(g->dev, "gpio %d aka GPIO%cA[%d] mapped to irq %d\n",
		gpio, '0' + bank, gpio - chip->base, hw_irq);

	g->irq[hw_irq].irq = of_irq_get(g->np, hw_irq);
	g->irq[hw_irq].gpio_irq = gpio_to_irq(gpio);
	g->irq[hw_irq].bank = bank;
	g->irq[hw_irq].configured = 1;

	rzn1_pinctrl_gpioint_select(hw_irq, (bank * 32) + (gpio - chip->base));

	if (devm_request_irq(g->dev, g->irq[hw_irq].irq, girq_handler, 0,
			dev_name(g->dev), g)) {
		dev_err(g->dev, "failed to request GIC IRQ %d\n",
			g->irq[hw_irq].irq);
		return -1;
	}

	return 0;
}

/*
 * Enabling the IRQ is a bit tricker -- this means someone wants to use the
 * GPIO_X interrupt directly (without using it as a GPIO) so we need to /claim/
 * it as a SHARED interrupt handler (doing nothing) so the downstream driver can
 * also claim it without having to worry about the fact the interrupt is a GPIO
 */
static void girq_enable(
	struct irq_data *d)
{
	struct girq *g = g_irq;
	int hw_irq = irqd_to_hwirq(d);

	BUG_ON(hw_irq >= GIRQ_IRQ_COUNT);

	/* GPIO block interrupts don't need any special handling */
	if (hw_irq >= GIRQ_IRQ_GPIO0) {
		/* enable the bank interrupt, to make sure we don't forward
		 * IRQs to a missing handler in case it's misconfigured */
		g->irq[hw_irq].enabled = 1;
		return;
	}
	/* not really an error here, log it. */
	if (g->irq[hw_irq].enabled) {
		dev_err(g->dev, "IRQ %d is already enabled\n", hw_irq);
		return;
	}
	if (!g->irq[hw_irq].configured) {
		if (girq_configure_hwirq(g, hw_irq)) {
			dev_err(g->dev, "IRQ %d can't be configured\n", hw_irq);
			return;
		}
	}
	if (!g->irq[hw_irq].claimed) {
		if (devm_request_irq(g->dev, g->irq[hw_irq].gpio_irq,
				girq_dummy_handler, IRQF_SHARED | IRQF_TRIGGER_FALLING,
				dev_name(g->dev), g)) {
			dev_err(g->dev, "failed to request GPIO IRQ %d\n",
				g->irq[hw_irq].irq);
			return;
		}
		g->irq[hw_irq].claimed = 1;
	}
	g->irq[hw_irq].enabled = 1;
}

/* Don't /really/ need to release anything here; perhaps we should? */
static void
girq_disable(
	struct irq_data *d)
{
	struct girq *g = g_irq;
	int hw_irq = irqd_to_hwirq(d);

	/* dev_info(g->dev, "IRQ %d disable ignored\n", hw_irq); */
	/* not really an error here, log it. */
	if (!g->irq[hw_irq].enabled)
		dev_err(g->dev, "IRQ %d is already disabled\n", hw_irq);
	g->irq[hw_irq].enabled = 0;
}

/* Don't /really/ need this, but the irq_chip code will crash if not there */
static void
girq_mask(
	struct irq_data *d)
{
	struct girq *g = g_irq;
	int hw_irq = irqd_to_hwirq(d);

	g->irq[hw_irq].masked = 1;
}
/* Don't /really/ need this, but the irq_chip code will crash if not there */
static void
girq_unmask(
	struct irq_data *d)
{
	struct girq *g = g_irq;
	int hw_irq = irqd_to_hwirq(d);

	g->irq[hw_irq].masked = 0;
}

/*
 * Preconfigure IRQs if they haven't been explicitely asked by a handler
 * already (at boot time). This allows drivers to use the old method of claiming
 * a GPIO irq and it will work, as long as the mux is configured properly
 * with the qpioirq-X DT property
 */
static int __init girq_late_init(void)
{
	struct girq *g = g_irq;
	int i;

	if (!g)	/* probe failed perhaps? */
		return 0;

	dev_info(g->dev, "Initializing mapping\n");

	for (i = 0; i < 8; i++)
		girq_configure_hwirq(g, i);

	return 0;
}
late_initcall(girq_late_init);

static int girq_probe(
	struct platform_device *ofdev)
{
	int ret = -ENOMEM;
	struct device *dev = &ofdev->dev;
	struct device_node *np = ofdev->dev.of_node;
	struct girq *g;
	struct irq_chip_generic	*gc = NULL;

	g = devm_kzalloc(dev, sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;
	g->dev = dev;
	g->np = np;

	g->irq_domain = irq_domain_add_linear(np, GIRQ_IRQ_COUNT,
					      &irq_generic_chip_ops, g);
	if (!g->irq_domain) {
		ret = -ENXIO;
		dev_err(dev, "cannot initialize irq domain\n");
		goto invalid_mapping;
	}
	if (irq_alloc_domain_generic_chips(g->irq_domain, GIRQ_IRQ_COUNT, 1,
				"rzn1-gpioirq", handle_level_irq,
				IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN,
				0, 0)) {
		dev_err(dev, "irq_alloc_domain_generic_chips failed\n");
		goto post_domain;
	}
	gc = irq_get_domain_generic_chip(g->irq_domain, 0);

	gc->chip_types[0].chip.irq_mask = girq_mask; /* required */
	gc->chip_types[0].chip.irq_unmask = girq_unmask;
	gc->chip_types[0].chip.irq_disable = girq_disable;
	gc->chip_types[0].chip.irq_enable = girq_enable;

	irq_setup_generic_chip(gc, IRQ_MSK(GIRQ_IRQ_COUNT),
				IRQ_GC_INIT_NESTED_LOCK, IRQ_NOREQUEST, 0);

	g_irq = g;
	return 0;

post_domain:
	irq_domain_remove(g->irq_domain);
invalid_mapping:
	return ret;
}

static int girq_remove(
	struct platform_device *pdev)
{
	struct girq *g = platform_get_drvdata(pdev);

	irq_domain_remove(g->irq_domain);
	return 0;
}

/*-------------------------------------------------------------------------*/
static const struct of_device_id girq_match[] = {
	{ .compatible = "renesas,rzn1-gpioirq", },
	{},
};

MODULE_DEVICE_TABLE(of, girq_match);

static struct platform_driver girq_driver = {
	.driver = {
		.name = driver_name,
		.owner = THIS_MODULE,
		.of_match_table = girq_match,
	},
	.probe          = girq_probe,
	.remove         = girq_remove,
};

module_platform_driver(girq_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");

