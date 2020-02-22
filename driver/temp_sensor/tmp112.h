/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_TMP112_H
#define __CROS_EC_TMP112_H

#include "i2c.h"

#define TMP112_REG_TEMP	0x00
#define TMP112_REG_CONF	0x01
#define TMP112_REG_HYST	0x02
#define TMP112_REG_MAX	0x03

/*
 * addr_pin is determined by a0 pin connection:
 * gnd = 0, v+ = 1, sda = 2, scl = 3
 */
#define TMP112_I2C_ADDR(addr_pin) (0x48 | (addr_pin & 0x3))

struct tmp112_t {
	int port;
	int addr;
};

/* ports and addresses of all tmp112 sensors, must equal TMP112_COUNT */
extern const struct tmp112_t tmp112_sensors[];

/**
 * Get the last polled value of a sensor.
 *
 * @param idx		Index to read.
 *
 * @param temp_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int tmp112_get_val(int idx, int *temp_ptr);

#endif /* __CROS_EC_TMP112_H */
