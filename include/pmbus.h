/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PMBUS_H
#define __CROS_EC_PMBUS_H

/* "enum pmbus_id" must be defined for each board in board.h. */
enum pmbus_id;

struct pmbus_dev {
	const char *name;
	int slave_addr;
	int port;
	int exp;
	int cntl_gpio;
};

#define PMBUS_VOUT_EXPONENT_DYNAMIC (0xffffffff)

extern const struct pmbus_dev pmbus_devs[];

/**
 * Function to set the pmbus cntl pin to given level
 *
 * @param PMBUS device index
 * @param level Level, 1 for high 0 for low
 *
 */
int pmbus_set_cntl(enum pmbus_id id, int level);

/**
 * Function to read out the output voltage of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @param data output voltage in mV
 *
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_read_volt_out(enum pmbus_id id, int *data);


/**
 * Function to read out the output voltage of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @param data output voltage in mV
 *
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_read_volt_out(enum pmbus_id id, int *data);

/**
 * Function to set the output voltage of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @param data Output voltage in mV
 *
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_set_volt_out(enum pmbus_id id, int data);

/**
 * Function to read the output current of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @param data Output current in mA
 *
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_read_curr_out(enum pmbus_id id, int *data);

/**
 * Function to read the temperature of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @param data Output temperature in degree celsius
 *
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_read_temp(enum pmbus_id id, int *data);

/**
 * Function to read the IC_DEVICE_ID of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_read_ic_dev_id(enum pmbus_id id, int *data);

/**
 * Function to read the IC_DEVICE_REV of a given PMBUS device
 * on a given port and address
 *
 * @param PMBUS device index
 * @param data Device revision
 * @return EC_SUCCESS or EC_ERROR_UNKNOWN
 */
int pmbus_read_ic_dev_rev(enum pmbus_id id, int *data);

#endif  /* __CROS_EC_PMBUS_H */

