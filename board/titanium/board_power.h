/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"

enum power_status {
	POWER_OFF = 0,
	POWER_INPUT_GOOD,
	POWER_INPUT_BAD,
	POWER_GOOD,
	POWER_BAD,
};

void set_board_power_status(enum power_status status);

enum power_status get_board_power_status(void);

void power_signal_changed_interrupt(enum gpio_signal signal);
