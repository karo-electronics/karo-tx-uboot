/*
 * SPMI bus uclass driver
 *
 * (C) Copyright 2015 Mateusz Kulikowski <mateusz.kulikowski@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <spmi/spmi.h>
#include <linux/ctype.h>

DECLARE_GLOBAL_DATA_PTR;

int spmi_reg_read(struct udevice *dev, int usid, int pid, int reg)
{
	int ret;
	const struct dm_spmi_ops *ops = dev_get_driver_ops(dev);
	uint8_t value;

	if (!ops || !ops->read)
		return -ENOSYS;

	ret = ops->read(dev, &value, usid, pid, reg, sizeof(value));
	return ret < 0 ? ret : value;
}

int spmi_reg_write(struct udevice *dev, int usid, int pid, int reg,
		   uint8_t value)
{
	const struct dm_spmi_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->write)
		return -ENOSYS;

	return ops->write(dev, &value, usid, pid, reg, sizeof(value));
}

int spmi_read(struct udevice *dev, void *buf, int usid, int pid, int reg,
	      uint8_t bc)
{
	const struct dm_spmi_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->read)
		return -ENOSYS;

	return ops->read(dev, buf, usid, pid, reg, bc);
}

int spmi_write(struct udevice *dev, const void *buf, int usid, int pid,
	       int reg, uint8_t bc)
{
	const struct dm_spmi_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->write)
		return -ENOSYS;

	return ops->write(dev, buf, usid, pid, reg, bc);
}

UCLASS_DRIVER(spmi) = {
	.id		= UCLASS_SPMI,
	.name		= "spmi",
	.post_bind	= dm_scan_fdt_dev,
};
