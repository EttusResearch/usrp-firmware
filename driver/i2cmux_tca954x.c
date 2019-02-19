/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PCA954x I2C Switch driver */

#include "common.h"
#include "i2c_mux.h"
#include "task.h"
#include "i2c.h"
#include "i2cmux_tca954x.h"
#include "console.h"
#include "util.h"
#include "stddef.h"

int tca954x_select_chan(int idx, int chan)
{
	struct i2c_mux_t *mux = i2c_muxes + idx;
	uint8_t buf;
	int err;

	buf = (1 << chan);

	err = i2c_xfer_unlocked(mux->parent_bus, mux->slave_addr, &buf, 1, NULL, 0, I2C_XFER_SINGLE);
	if (err)
		return err;

	return 0;
}
