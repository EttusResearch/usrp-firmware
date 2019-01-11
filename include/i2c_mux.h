/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common I2C mux code driver */

#ifndef __CROS_EC_I2C_MUX_H
#define __CROS_EC_I2C_MUX_H

#include "task.h"

struct i2c_mux_t {
	int parent_bus;
	int slave_addr;
	int chan;

	int (*select_chan)(int idx, int chan);
	struct mutex lock;
	int idx;
};

#ifdef CONFIG_I2C_MUX
extern struct i2c_mux_t i2c_muxes[];

struct i2c_mux_mapping {
	int port;
	enum i2c_mux_id id;
	int chan;
};

extern struct i2c_mux_mapping i2c_mux_mappings[];

int i2c_port_is_muxed(int port);

int i2c_mux_get_parent(enum i2c_mux_id id);

int i2c_mux_get_cfg(int port, enum i2c_mux_id *id, int *chan, int *parent);

int i2c_mux_lock(enum i2c_mux_id id);

int i2c_mux_unlock(enum i2c_mux_id id);

int i2c_mux_select_chan(enum i2c_mux_id id, int chan);
#endif /* CONFIG_I2C_MUX */

#endif /* __CROS_EC_I2C_MUX_H */
