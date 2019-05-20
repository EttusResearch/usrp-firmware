/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Driver for TI TCA6408 and TCA6416 I/O expanders
 */

#include "i2c.h"
#include "tca64xx.h"
#include "ioexpander.h"
#include "console.h"

#define TCA64XX_SUPPORT_GPIO_FLAGS (GPIO_INPUT | GPIO_OUTPUT | \
				    GPIO_LOW | GPIO_HIGH)

#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_GPIO, format, ## args)

enum tca64xx_reg {
	TCA64XX_REG_INPUT = 0x0,
	TCA64XX_REG_OUTPUT = 0x1,
	TCA64XX_REG_POL = 0x2,
	TCA64XX_REG_CONFIG = 0x3,
};

struct tca64xx_priv {
	uint8_t ports;
};

static struct tca64xx_priv priv[CONFIG_IO_EXPANDER_PORT_COUNT];

static int tca64xx_read8(int ioex, int port, enum tca64xx_reg reg, int *val)
{
	struct ioexpander_config_t *ioex_p = &ioex_config[ioex];
	const int addr = reg * priv[ioex].ports + port;
	return i2c_read8(ioex_p->i2c_host_port, ioex_p->i2c_slave_addr, addr, val);
}

static int tca64xx_write8(int ioex, int port, enum tca64xx_reg reg, int val)
{
	struct ioexpander_config_t *ioex_p = &ioex_config[ioex];
	const int addr = reg * priv[ioex].ports + port;
	return i2c_write8(ioex_p->i2c_host_port, ioex_p->i2c_slave_addr, addr, val);
}

static int tca64xx_ioex_check_is_valid(int ioex, int port, int mask)
{
	int num_ports = priv[ioex].ports;
	if (port >= num_ports) {
		CPRINTF("ioexpander only has %d port\n", num_ports);
		return EC_ERROR_INVAL;
	}

	if (mask > 256) {
		CPRINTF("tca6416 only has 8 gpios per bank\n");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int tca6408_ioex_init(int ioex)
{
	priv[ioex].ports = 1;
	return EC_SUCCESS;
}

static int tca6416_ioex_init(int ioex)
{
	priv[ioex].ports = 2;
	return EC_SUCCESS;
}

static int tca64xx_ioex_get_level(int ioex, int port, int mask, int *val)
{
	int rv;

	rv = tca64xx_ioex_check_is_valid(ioex, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	rv = tca64xx_read8(ioex, port, TCA64XX_REG_INPUT, val);
	*val = !!(*val & mask);

	return rv;
}

static int tca64xx_ioex_set_level(int ioex, int port, int mask, int value)
{
	int rv, val;

	rv = tca64xx_ioex_check_is_valid(ioex, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	rv = tca64xx_read8(ioex, port, TCA64XX_REG_OUTPUT, &val);

	if (value)
		val |= mask;
	else
		val &= ~mask;

	rv |= tca64xx_write8(ioex, port, TCA64XX_REG_OUTPUT, val);

	return rv;
}

static int tca64xx_ioex_set_flags_by_mask(int ioex, int port, int mask,
					  int flags)
{
	int rv, val;

	rv = tca64xx_ioex_check_is_valid(ioex, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	val = flags & ~TCA64XX_SUPPORT_GPIO_FLAGS;
	if (val) {
		CPRINTF("Flag 0x%08x is not supported\n", val);
		return EC_ERROR_INVAL;
	}

	/* Configure the output level */
	rv |= tca64xx_read8(ioex, port, TCA64XX_REG_OUTPUT, &val);
	if (flags & GPIO_HIGH)
		val |= mask;
	else if (flags & GPIO_LOW)
		val &= ~mask;
	rv |= tca64xx_write8(ioex, port, TCA64XX_REG_OUTPUT, val);

	rv |= tca64xx_read8(ioex, port, TCA64XX_REG_CONFIG, &val);
	if (flags & GPIO_INPUT)
		val |= mask;
	else
		val &= ~mask;
	rv |= tca64xx_write8(ioex, port, TCA64XX_REG_CONFIG, val);

	return rv;
}

static int tca64xx_ioex_get_flags(int ioex, int port, int mask, int *flags)
{
	int rv, val;

	rv = tca64xx_ioex_check_is_valid(ioex, port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	rv =  tca64xx_read8(ioex, port, TCA64XX_REG_CONFIG, &val);
	if (val & mask)
		*flags |= GPIO_INPUT;
	else
		*flags |= GPIO_OUTPUT;

	rv |= tca64xx_read8(ioex, port, TCA64XX_REG_OUTPUT, &val);
	if (val & mask)
		*flags |= GPIO_HIGH;
	else
		*flags |= GPIO_LOW;


	return rv;
}

const struct ioexpander_drv tca6408_ioexpander_drv = {
	.init              = &tca6408_ioex_init,
	.get_level         = &tca64xx_ioex_get_level,
	.set_level         = &tca64xx_ioex_set_level,
	.get_flags_by_mask = &tca64xx_ioex_get_flags,
	.set_flags_by_mask = &tca64xx_ioex_set_flags_by_mask,
};

const struct ioexpander_drv tca6416_ioexpander_drv = {
	.init              = &tca6416_ioex_init,
	.get_level         = &tca64xx_ioex_get_level,
	.set_level         = &tca64xx_ioex_set_level,
	.get_flags_by_mask = &tca64xx_ioex_get_flags,
	.set_flags_by_mask = &tca64xx_ioex_set_flags_by_mask,
};
