/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TMP431 temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TMP431_H
#define __CROS_EC_TMP431_H

#define TMP431_I2C_ADDR		0x98 /* 7-bit address is 0x4C */

#define TMP431_IDX_LOCAL	0
#define TMP431_IDX_REMOTE1	1
#define TMP431_IDX_COUNT	2

/* Chip-specific registers */
#define TMP431_LOCAL			0x00
#define TMP431_REMOTE1			0x01
#define TMP431_STATUS			0x02
#define TMP431_CONFIGURATION1_R		0x03
#define TMP431_CONVERSION_RATE_R	0x04
#define TMP431_LOCAL_HIGH_LIMIT_R	0x05
#define TMP431_LOCAL_LOW_LIMIT_R	0x06
#define TMP431_REMOTE1_HIGH_LIMIT_R	0x07
#define TMP431_REMOTE1_LOW_LIMIT_R	0x08
#define TMP431_CONFIGURATION1_W		0x09
#define TMP431_CONVERSION_RATE_W	0x0a
#define TMP431_LOCAL_HIGH_LIMIT_W	0x0b
#define TMP431_LOCAL_LOW_LIMIT_W	0x0c
#define TMP431_REMOTE1_HIGH_LIMIT_W	0x0d
#define TMP431_REMOTE1_LOW_LIMIT_W	0x0e
#define TMP431_ONESHOT			0x0f
#define TMP431_REMOTE1_EXTD		0x10
#define TMP431_REMOTE1_HIGH_LIMIT_EXTD	0x13
#define TMP431_REMOTE1_LOW_LIMIT_EXTD	0x14
#define TMP431_REMOTE2_HIGH_LIMIT_R	0x15
#define TMP431_REMOTE2_HIGH_LIMIT_W	0x15
#define TMP431_REMOTE2_LOW_LIMIT_R	0x16
#define TMP431_REMOTE2_LOW_LIMIT_W	0x16
#define TMP431_REMOTE2_HIGH_LIMIT_EXTD	0x17
#define TMP431_REMOTE2_LOW_LIMIT_EXTD	0x18
#define TMP431_REMOTE1_THERM_LIMIT	0x19
#define TMP431_REMOTE2_THERM_LIMIT	0x1a
#define TMP431_STATUS_FAULT		0x1b
#define TMP431_CHANNEL_MASK		0x1f
#define TMP431_LOCAL_THERM_LIMIT	0x20
#define TMP431_THERM_HYSTERESIS		0x21
#define TMP431_CONSECUTIVE_ALERT	0x22
#define TMP431_REMOTE2			0x23
#define TMP431_REMOTE2_EXTD		0x24
#define TMP431_BETA_RANGE_CH1		0x25
#define TMP431_BETA_RANGE_CH2		0x26
#define TMP431_NFACTOR_REMOTE1		0x27
#define TMP431_NFACTOR_REMOTE2		0x28
#define TMP431_LOCAL_EXTD		0x29
#define TMP431_STATUS_LIMIT_HIGH	0x35
#define TMP431_STATUS_LIMIT_LOW		0x36
#define TMP431_STATUS_THERM		0x37
#define TMP431_LOCAL_HIGH_LIMIT_EXTD	0x3d
#define TMP431_LOCAL_LOW_LIMIT_EXTD	0x3e
#define TMP431_CONFIGURATION2_R		0x3f
#define TMP431_CONFIGURATION2_W		0x3f
#define TMP431_RESET_W			0xfc
#define TMP431_DEVICE_ID		0xfd
#define TMP431_MANUFACTURER_ID		0xfe

/* Config register bits */
#define TMP431_CONFIG1_TEMP_RANGE	(1 << 2)
/* TMP431_CONFIG1_MODE bit is use to enable THERM mode */
#define TMP431_CONFIG1_MODE		(1 << 5)
#define TMP431_CONFIG1_RUN_L		(1 << 6)
#define TMP431_CONFIG1_ALERT_MASK_L	(1 << 7)
#define TMP431_CONFIG2_RESISTANCE_CORRECTION	(1 << 2)
#define TMP431_CONFIG2_LOCAL_ENABLE	(1 << 3)
#define TMP431_CONFIG2_REMOTE1_ENABLE	(1 << 4)
#define TMP431_CONFIG2_REMOTE2_ENABLE	(1 << 5)

/* Status register bits */
#define TMP431_STATUS_TEMP_THERM_ALARM	(1 << 1)
#define TMP431_STATUS_OPEN		(1 << 2)
#define TMP431_STATUS_TEMP_LOW_ALARM	(1 << 3)
#define TMP431_STATUS_TEMP_HIGH_ALARM	(1 << 4)
#define TMP431_STATUS_BUSY		(1 << 7)

/* Limintaions */
#define TMP431_HYSTERESIS_HIGH_LIMIT	255
#define TMP431_HYSTERESIS_LOW_LIMIT	0

enum tmp431_power_state {
	TMP431_POWER_OFF = 0,
	TMP431_POWER_ON,
	TMP431_POWER_COUNT
};

enum tmp431_channel_id {
	TMP431_CHANNEL_LOCAL,
	TMP431_CHANNEL_REMOTE1,

	TMP431_CHANNEL_COUNT
};

/**
 * Get the last polled value of a sensor.
 *
 * @param idx		Index to read. Idx indicates whether to read die
 *			temperature or external temperature.
 * @param temp_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int tmp431_get_val(int idx, int *temp_ptr);

/**
 * Power control function of tmp431 temperature sensor.
 *
 * @param power_on	TMP431_POWER_ON: turn tmp431 sensor on.
 *			TMP431_POWER_OFF: shut tmp431 sensor down.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int tmp431_set_power(enum tmp431_power_state power_on);

/*
 * Set TMP431 ALERT#/THERM2# pin to THERM mode, and give a limit
 * for a specific channel.
 *
 * @param channel	specific a channel
 *
 * @param limit_c	High limit temperature, default: 85C
 *
 * @param hysteresis	Hysteresis temperature, default: 10C
 *			All channels share the same hysteresis
 *
 * In THERM mode, ALERT# pin will trigger(Low) by itself when any
 * channel's temperature is greater( >= )than channel's limit_c,
 * and release(High) by itself when channel's temperature is lower
 * than (limit_c - hysteresis)
 */
int tmp431_set_therm_limit(int channel, int limit_c, int hysteresis);
#endif /* __CROS_EC_TMP431_H */
