/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#include "common.h"
#include "console.h"
#include "util.h"
#include "i2c.h"
#include "pmbus.h"

enum pmbus_command {
	PMBUS_CMD_WRITE_PROTECT = 0x10,
	PMBUS_CMD_STORE_DEFAULT_ALL = 0x11,
	PMBUS_CMD_RESTORE_DEFAULT_ALL = 0x12,
	PMBUS_CMD_CAPABILITY = 0x19,
	PMBUS_CMD_VOUT_MODE = 0x20,
	PMBUS_CMD_VOUT_MAX = 0x24,
	PMBUS_CMD_VOUT_MARGIN_HIGH = 0x25,
	PMBUS_CMD_VOUT_MARGIN_LOW = 0x26,
	PMBUS_CMD_VOUT_TRANSITION_RATE = 0x27,
	PMBUS_CMD_VOUT_DROOP = 0x28,
	PMBUS_CMD_VOUT_SCALE_LOOP = 0x29,
	PMBUS_CMD_COEFFICIENTS = 0x30,
	PMBUS_CMD_READ_VOUT = 0x8b,
	PMBUS_CMD_READ_IOUT = 0x8c,
	PMBUS_CMD_READ_TEMPERATURE = 0x8d,
	PMBUS_CMD_READ_ID = 0xad,
	PMBUS_CMD_READ_REV = 0xae,
	PMBUS_CMD_VOUT_COMMAND = 0x21
};

enum pmbus_vout_mode {
	PMBUS_VOUT_MODE_LINEAR = 0,
	PMBUS_VOUT_MODE_DIRECT = 2,
	PMBUS_VOUT_MODE_COUNT
};

const struct pmbus_dev *get_pmbus_dev(enum pmbus_id id)
{
	if (id < 0 || id >= PMBUS_DEV_COUNT)
		return NULL;

	return pmbus_devs + id;
}

#define PMBUS_VOUT_EXP_MASK (0x1f)

static int pmbus_read_vout_mode(const struct pmbus_dev *dev, int8_t *exp)
{
	int v, ret;
	int8_t tmp;

	ret = i2c_read8(dev->port, dev->slave_addr,
			PMBUS_CMD_VOUT_MODE, &v);
	if (ret)
		return ret;

	tmp = (v & PMBUS_VOUT_EXP_MASK);
	if (tmp > 0xf)
		tmp |= 0xe0;

	*exp = tmp;

	return 0;
}

static int pmbus_reg_to_linear(uint16_t v, int scale, int exp)
{
	if (exp >= 0)
		return (v * scale) << exp;
	else
		return (v * scale) >> -exp;
}

static int pmbus_linear_to_reg(uint16_t v, int scale, int exp)
{
	if (exp >= 0)
		return (v >> exp) / scale;
	else
		return (v << -exp) / scale;
}

int pmbus_read_volt_out(enum pmbus_id id, int *data)
{
	const struct pmbus_dev *dev;
	int8_t exp = 0;
	int v, ret;

	dev = get_pmbus_dev(id);
	if (!dev)
		return EC_ERROR_PARAM1;


	ret = i2c_read16(dev->port, dev->slave_addr,
			 PMBUS_CMD_READ_VOUT, &v);
	if (ret)
		return ret;

	if (dev->exp == PMBUS_VOUT_EXPONENT_DYNAMIC)
		pmbus_read_vout_mode(dev, &exp);
	else
		exp = dev->exp;

	*data = pmbus_reg_to_linear(v, 1000, exp);

	return 0;
}

int pmbus_set_volt_out(enum pmbus_id id, int data)
{
	const struct pmbus_dev *dev;
	int8_t exp = 0;
	int v;

	dev = get_pmbus_dev(id);
	if (!dev)
		return EC_ERROR_PARAM1;

	if (dev->exp == PMBUS_VOUT_EXPONENT_DYNAMIC)
		pmbus_read_vout_mode(dev, &exp);
	else
		exp = dev->exp;

	v = pmbus_linear_to_reg(data, 1000, exp);

	return i2c_write16(dev->port, dev->slave_addr,
			   PMBUS_CMD_VOUT_COMMAND, v);
}

int pmbus_read_curr_out(enum pmbus_id id, int *data)
{
	const struct pmbus_dev *dev;
	int8_t exp;
	int mant;
	int v, ret;

	dev = get_pmbus_dev(id);
	if (!dev)
		return EC_ERROR_PARAM1;

	ret = i2c_read16(dev->port, dev->slave_addr,
			 PMBUS_CMD_READ_IOUT, &v);
	if (ret)
		return ret;

	exp = v >> 11;
	if (exp > 0xf)
		exp |= 0xe0;

	mant = ((int16_t)((v & 0x7ff) << 5)) >> 5;

	*data = pmbus_reg_to_linear(mant, 1000, exp);

	return 0;
}

int pmbus_read_temp(enum pmbus_id id, int *data)
{
	const struct pmbus_dev *dev;
	int v, ret;

	dev = get_pmbus_dev(id);
	if (!dev)
		return EC_ERROR_PARAM1;


	ret = i2c_read16(dev->port, dev->slave_addr,
			 PMBUS_CMD_READ_TEMPERATURE, &v);
	if (ret)
		return ret;

	*data = v;

	return 0;
}

static int pmbus_read_block16(enum pmbus_id id, uint8_t cmd, int *data)
{
	const struct pmbus_dev *dev;
	uint8_t buf[4];
	uint16_t *ptr = (uint16_t *)&buf[1];
	int ret;

	dev = get_pmbus_dev(id);
	if (!dev)
		return EC_ERROR_PARAM1;

	ret = i2c_xfer(dev->port, dev->slave_addr, &cmd, 1, buf, 3);
	if (ret)
		return ret;

	*data = *ptr;

	return 0;
}

int pmbus_read_ic_dev_id(enum pmbus_id id, int *data)
{
	return pmbus_read_block16(id, PMBUS_CMD_READ_ID, data);
}

int pmbus_read_ic_dev_rev(enum pmbus_id id, int *data)
{
	return pmbus_read_block16(id, PMBUS_CMD_READ_REV, data);
}

#include "gpio.h"
int pmbus_set_cntl(enum pmbus_id id, int level)
{
	const struct pmbus_dev *dev;

	dev = get_pmbus_dev(id);
	if (!dev)
		return EC_ERROR_PARAM1;

	gpio_set_level(dev->cntl_gpio, level);
	return 0;
}

#ifdef CONFIG_CMD_PMBUS
static int command_pmbus(int argc, char **argv)
{
	int idx;
	int v, v2, rv = 0;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	idx = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	if (argc >= 3) {
		v = strtoi(argv[3], &e, 0);
		if (*e)
			return EC_ERROR_PARAM3;
	}

	if (!strcasecmp(argv[1], "vout")) {
		rv = pmbus_read_volt_out(idx, &v);
		if (!rv)
			ccprintf("%d mV\n", v);
	} else if (!strcasecmp(argv[1], "iout")) {
		rv = pmbus_read_curr_out(idx, &v);
		if (!rv)
			ccprintf("%d mA\n", v);
	} else if (!strcasecmp(argv[1], "temp")) {
		rv = pmbus_read_temp(idx, &v);
		if (!rv)
			ccprintf("%d C\n", v);
	} else if (!strcasecmp(argv[1], "id")) {
		rv = pmbus_read_ic_dev_id(idx, &v);
		if (rv)
			return rv;
		rv = pmbus_read_ic_dev_rev(idx, &v2);
		if (!rv)
			ccprintf("ID: %x Rev: %x\n", v, v2);
	} else if (!strcasecmp(argv[1], "voutset")) {
		if (argc < 4)
			return EC_ERROR_PARAM3;
		rv = pmbus_set_volt_out(idx, v);
		if (rv)
			return rv;
	} else if (!strcasecmp(argv[1], "cntl")) {
		if (argc < 4)
			return EC_ERROR_PARAM3;
		rv = pmbus_set_cntl(idx, !!v);
		if (rv)
			return rv;
	} else {
		return EC_ERROR_PARAM1;
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(pmbus, command_pmbus,
			"vout/voutset/iout/temp/cntl/id idx [value] ",
			"Read/write PMBUS");
#endif
