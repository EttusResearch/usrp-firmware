/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TMP468 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "tmp432.h"
#include "gpio.h"
#include "i2c.h"
#include "hooks.h"
#include "util.h"
#include "math_util.h"

#include "tmp468.h"


static int fake_temp[TMP468_CHANNEL_COUNT] = {-1, -1, -1, -1, -1, -1, -1 , -1, -1};
static int temp_val[TMP468_CHANNEL_COUNT]  = {0, 0, 0, 0, 0, 0, 0 , 0, 0};
static uint8_t is_sensor_shutdown;

/* idx is the remote number, 1 through 8 */
#define TMP468_REMOTE_OFFSET(idx) (0x40 + 8 * ((idx)-1))
#define TMP468_REMOTE_NFACTOR(idx) (0x41 + 8 * ((idx)-1))

static int has_power(void)
{
	return !is_sensor_shutdown;
}

static int raw_read16(const int offset, int *data_ptr)
{
	int ret = i2c_read16(I2C_PORT_THERMAL, TMP468_I2C_ADDR_FLAGS,
			  offset, data_ptr);

	if (ret != EC_SUCCESS)
		ccprintf("ERROR: TMP468 Temp sensor I2C read16 error.\n");

	return ret;
}

static int raw_write16(const int offset, int data)
{
	int ret = i2c_write16(I2C_PORT_THERMAL, TMP468_I2C_ADDR_FLAGS,
			   offset, data);

	if (ret != EC_SUCCESS)
		ccprintf("ERROR: TMP468 Temp sensor I2C write16 error.\n");

	return ret;
}

static int tmp468_shutdown(uint8_t want_shutdown)
{
	int ret, value;

	if (want_shutdown == is_sensor_shutdown)
		return EC_SUCCESS;

	ret = raw_read16(TMP468_CONFIGURATION, &value);
	if (ret < 0)
		return ret;

	if (want_shutdown)
		value |= TMP468_SHUTDOWN;
	else
		value &= ~TMP468_SHUTDOWN;

	ret = raw_write16(TMP468_CONFIGURATION, value);
	if (ret == EC_SUCCESS)
		is_sensor_shutdown = want_shutdown;

	return EC_SUCCESS;
}

int tmp468_get_val(int idx, int *temp_ptr)
{
	if(!has_power())
		return EC_ERROR_NOT_POWERED;

	if (idx < TMP468_CHANNEL_COUNT) {
		*temp_ptr = C_TO_K(temp_val[idx]);
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}

static void temp_sensor_poll(void)
{
	int i, ret;

	if (!has_power())
		return;

	for (i = 0; i < TMP468_CHANNEL_COUNT; i++)
		if (fake_temp[i] != -1) {
			temp_val[i] = fake_temp[i];
		} else {
			ret = raw_read16(TMP468_LOCAL + i, &temp_val[i]);
			if (ret < 0)
				return;

			temp_val[i] = sign_extend(temp_val[i], 16);
			temp_val[i] >>= TMP468_SHIFT1;
		}
}
DECLARE_HOOK(HOOK_SECOND, temp_sensor_poll, HOOK_PRIO_TEMP_SENSOR);

int tmp468_set_power(enum tmp468_power_state power_on)
{
	uint8_t shutdown = (power_on == TMP468_POWER_OFF) ? 1 : 0;
	return tmp468_shutdown(shutdown);
}

static void tmp468_lock(void)
{
	raw_write16(TMP468_LOCK, 0x5CA6);
}

static void tmp468_unlock(void)
{
	raw_write16(TMP468_LOCK, 0xEB19);
}

int tmp468_set_nfactor(int idx, int8_t nfactor)
{
	int ret, temp;

	if (!has_power())
		return EC_ERROR_NOT_POWERED;

	if (idx < 1 || idx > (TMP468_CHANNEL_COUNT - 1))
		return EC_ERROR_INVAL;

	temp = sign_extend(nfactor, 16);
	temp <<= TMP468_SHIFT2;

	tmp468_unlock();

	ret = raw_write16(TMP468_REMOTE_NFACTOR(idx), temp);

	tmp468_lock();

	return ret;
}

int tmp468_set_offset(int idx, int8_t offset)
{
	int ret, temp;

	if (!has_power())
		return EC_ERROR_NOT_POWERED;

	if (idx < 1 || idx > (TMP468_CHANNEL_COUNT - 1))
		return EC_ERROR_INVAL;

	temp = sign_extend(offset, 16);
	temp <<= TMP468_SHIFT1;

	tmp468_unlock();

	ret = raw_write16(TMP468_REMOTE_OFFSET(idx), temp);

	tmp468_lock();

	return ret;
}

#ifdef CONFIG_CMD_TMP468
static int tmp468_get_offset(int idx, int *offset_ptr)
{
	int ret;

	if (!has_power())
		return EC_ERROR_NOT_POWERED;

	if (idx < 1 || idx > (TMP468_CHANNEL_COUNT - 1))
		return EC_ERROR_INVAL;

	ret = raw_read16(TMP468_REMOTE_OFFSET(idx), offset_ptr);
	if (ret != EC_SUCCESS)
		return ret;

	*offset_ptr = sign_extend(*offset_ptr, 16);
	*offset_ptr >>= TMP468_SHIFT1;

	return EC_SUCCESS;
}

static int tmp468_get_nfactor(int idx, int *nfactor_ptr)
{
	int ret;

	if (!has_power())
		return EC_ERROR_NOT_POWERED;

	if (idx < 1 || idx > (TMP468_CHANNEL_COUNT - 1))
		return EC_ERROR_INVAL;

	ret = raw_read16(TMP468_REMOTE_NFACTOR(idx), nfactor_ptr);
	if (ret != EC_SUCCESS)
		return ret;

	*nfactor_ptr = sign_extend(*nfactor_ptr, 16);
	*nfactor_ptr >>= TMP468_SHIFT2;

	return EC_SUCCESS;
}

static void tmp468_dump(uint8_t idx)
{
	int offset, nfactor;
	tmp468_get_offset(idx, &offset);
	tmp468_get_nfactor(idx, &nfactor);

	ccprintf("offset: %d\n", offset);
	ccprintf("nfactor: %d\n", nfactor);
}

static int command_tmp468(int argc, char **argv)
{
	char *e;
	int idx;
	int val;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	idx = strtoi(argv[1], &e, 10);
	if (*e)
		return EC_ERROR_PARAM1;

	if (idx < 1 || idx > (TMP468_CHANNEL_COUNT - 1)) {
		ccprintf("invalid index value: %d, valid indices %d-%d\n",
			idx, 1, TMP468_CHANNEL_COUNT - 1);
		return EC_ERROR_PARAM1;
	}

	if (2 == argc) {
		tmp468_dump(idx);
		return EC_SUCCESS;
	} else if (4 == argc) {
		val = strtoi(argv[3], &e, 10);
		if (*e)
			return EC_ERROR_PARAM3;

		if (!strcasecmp(argv[2], "offset")) {
			return tmp468_set_offset(idx, val);
		} else if (!strcasecmp(argv[2], "nfactor")) {
			return tmp468_set_nfactor(idx, val);
		} else {
			return EC_ERROR_PARAM2;
		}
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(tmp468, command_tmp468,
			"<index> [offset|nfactor <val>]",
			"TMP468 temperature sensing");
#endif
