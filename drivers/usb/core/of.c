/*
 * of.c		The helpers for hcd device tree support
 *
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Author: Peter Chen <peter.chen@freescale.com>
 */

#include <linux/of.h>
#include <linux/usb/of.h>

/**
 * usb_of_get_child_node - Find the device node match port number
 * @parent: the parent device node
 * @portnum: the port number which device is connecting
 *
 * Find the node from device tree according to its port number.
 *
 * Return: A pointer to the node with incremented refcount if found, or
 * %NULL otherwise.
 */
struct device_node *usb_of_get_child_node(struct device_node *parent,
					int portnum)
{
	struct device_node *node;
	u32 port;

	for_each_child_of_node(parent, node) {
		if (!of_property_read_u32(node, "reg", &port)) {
			if (port == portnum)
				return node;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(usb_of_get_child_node);

