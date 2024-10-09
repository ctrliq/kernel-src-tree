/*
 * xHCI host controller driver
 *
 * Copyright (C) 2008 Intel Corp.
 *
 * Author: Sarah Sharp
 * Some code borrowed from the Linux EHCI driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "xhci.h"

void xhci_dbg_erst(struct xhci_hcd *xhci, struct xhci_erst *erst)
{
	u64 addr = erst->erst_dma_addr;
	int i;
	struct xhci_erst_entry *entry;

	for (i = 0; i < erst->num_entries; i++) {
		entry = &erst->entries[i];
		xhci_dbg(xhci, "@%016llx %08x %08x %08x %08x\n",
			 addr,
			 lower_32_bits(le64_to_cpu(entry->seg_addr)),
			 upper_32_bits(le64_to_cpu(entry->seg_addr)),
			 le32_to_cpu(entry->seg_size),
			 le32_to_cpu(entry->rsvd));
		addr += sizeof(*entry);
	}
}

char *xhci_get_slot_state(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx)
{
	struct xhci_slot_ctx *slot_ctx = xhci_get_slot_ctx(xhci, ctx);
	int state = GET_SLOT_STATE(le32_to_cpu(slot_ctx->dev_state));

	return xhci_slot_state_string(state);
}

void xhci_dbg_trace(struct xhci_hcd *xhci, void (*trace)(struct va_format *),
			const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	xhci_dbg(xhci, "%pV\n", &vaf);
	trace(&vaf);
	va_end(args);
}
EXPORT_SYMBOL_GPL(xhci_dbg_trace);
