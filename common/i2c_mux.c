/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "i2c.h"
#include "i2c_mux.h"
#include "system.h"
#include "util.h"

int i2c_port_is_muxed(int port)
{
	return port > I2C_PORT_COUNT;
}

int i2c_mux_get_parent(enum i2c_mux_id id)
{
	struct i2c_mux_t *mux;

	if (id < 0 || id >= I2C_MUX_COUNT)
		return EC_ERROR_INVAL;

	mux  = i2c_muxes + id;

	return mux->parent_bus;
}

int i2c_mux_lock(enum i2c_mux_id id)
{
	struct i2c_mux_t *mux;

	if (id < 0 || id >= I2C_MUX_COUNT)
		return EC_ERROR_INVAL;

	mux  = i2c_muxes + id;
	mutex_lock(&mux->lock);

	return 0;
}

int i2c_mux_unlock(enum i2c_mux_id id)
{
	struct i2c_mux_t *mux;

	if (id < 0 || id >= I2C_MUX_COUNT)
		return EC_ERROR_INVAL;

	mux  = i2c_muxes + id;
	mutex_unlock(&mux->lock);

	return 0;
}

int i2c_mux_select_chan(enum i2c_mux_id id, int chan)
{
	struct i2c_mux_t *mux;
	int ret;

	if (id < 0 || id >= I2C_MUX_COUNT)
		return EC_ERROR_INVAL;

	mux  = i2c_muxes + id;

	if (mux->chan == chan)
		return 0;

	ret = mux->select_chan(mux->idx, chan);
	if (ret)
		return ret;

	mux->chan = chan;

	return 0;
}

#ifdef CONFIG_CMD_I2C_MUX
static int cmd_i2c_mux(int argc, char **argv)
{
	int id, ret, v;
	char *e;


	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	id = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (argc >= 2) {
		v = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
	}

	if (id < 0 || id >= I2C_MUX_COUNT)
		return EC_ERROR_INVAL;

	ret = i2c_mux_lock(id);
	if (ret)
		return ret;

	ret = i2c_mux_select_chan(id, v);
	if (ret)
		return ret;

	ret = i2c_mux_unlock(id);
	if (ret)
		return ret;

	return 0;
}
DECLARE_CONSOLE_COMMAND(i2c_mux, cmd_i2c_mux,
			"idx [value] ",
			"Control I2C MUX");
#endif
