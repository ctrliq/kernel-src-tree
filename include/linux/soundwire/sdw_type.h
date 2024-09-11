/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2015-17 Intel Corporation. */

#ifndef __SOUNDWIRE_TYPES_H
#define __SOUNDWIRE_TYPES_H

extern struct bus_type sdw_bus_type;
extern struct device_type sdw_slave_type;
extern struct device_type sdw_master_type;

static inline int is_sdw_slave(const struct device *dev)
{
	return dev->type == &sdw_slave_type;
}

#define drv_to_sdw_driver(_drv) container_of(_drv, struct sdw_driver, driver)

#define sdw_register_driver(drv) \
	__sdw_register_driver(drv, THIS_MODULE)

int __sdw_register_driver(struct sdw_driver *drv, struct module *);
void sdw_unregister_driver(struct sdw_driver *drv);

int sdw_slave_uevent(struct device *dev, struct kobj_uevent_env *env);

#endif /* __SOUNDWIRE_TYPES_H */
