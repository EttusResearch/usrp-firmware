/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TMP112 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "tmp112.h"
#include "i2c.h"
#include "hooks.h"
#include "util.h"

#define TMP112_RESOLUTION 12
#define TMP112_SHIFT1 (16 - TMP112_RESOLUTION)
#define TMP112_SHIFT2 (TMP112_RESOLUTION - 8)

#ifndef TMP112_COUNT
#define TMP112_COUNT 1
const struct tmp112_t tmp112_sensors[] = {
	{ .port = I2C_PORT_THERMAL, .addr = TMP112_I2C_ADDR(0) },
};
#endif

static int temp_val_local[TMP112_COUNT];

static int raw_read16(const struct tmp112_t *dev, const int offset,
		      int *data_ptr)
{
	return i2c_read16(dev->port, dev->addr | I2C_FLAG_BIG_ENDIAN, offset,
			  data_ptr);
}

static int raw_write16(const struct tmp112_t *dev, const int offset, int data)
{
	return i2c_write16(dev->port, dev->addr | I2C_FLAG_BIG_ENDIAN, offset,
			   data);
}

static int get_temp(const struct tmp112_t *dev, int *temp_ptr)
{
	int rv;
	int temp_raw = 0;

	rv = raw_read16(dev, TMP112_REG_TEMP, &temp_raw);
	if (rv < 0)
		return rv;

	*temp_ptr = (int)(int16_t)temp_raw;
	return EC_SUCCESS;
}

static inline int tmp112_reg_to_c(int16_t reg)
{
	int tmp;

	tmp = (((reg >> TMP112_SHIFT1) * 1000 ) >> TMP112_SHIFT2);

	return tmp / 1000;
}

int tmp112_get_val(int idx, int *temp_ptr)
{
	if (idx >= TMP112_COUNT)
		return EC_ERROR_INVAL;

	*temp_ptr = temp_val_local[idx];
	return EC_SUCCESS;
}

static void tmp112_poll(void)
{
	int temp_c = 0;
	int i;

	for (i = 0; i < TMP112_COUNT; i++) {
		if (get_temp(tmp112_sensors + i, &temp_c) == EC_SUCCESS)
			temp_val_local[i] = C_TO_K(tmp112_reg_to_c(temp_c));
	}
}
DECLARE_HOOK(HOOK_SECOND, tmp112_poll, HOOK_PRIO_TEMP_SENSOR);

static void tmp112_init(void)
{
	int tmp, i;
	int set_mask, clr_mask;
	const struct tmp112_t *dev;

	/* 12 bit mode */
	set_mask = (3 << 5);

	/* not oneshot mode */
	clr_mask = BIT(7);

	for (i = 0; i < TMP112_COUNT; i++) {
		dev = tmp112_sensors + i;
		raw_read16(dev, TMP112_REG_CONF, &tmp);
		raw_write16(dev, TMP112_REG_CONF, (tmp & ~clr_mask) | set_mask);
	}
}
DECLARE_HOOK(HOOK_INIT, tmp112_init, HOOK_PRIO_DEFAULT);
