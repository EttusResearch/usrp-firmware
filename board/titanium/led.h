/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NI Project Titanium Firmware */

enum pwrdb_led_color {
	LED_OFF = 0,
	LED_RED,
	LED_GREEN,
	LED_AMBER,

	LED_COLOR_COUNT
};

enum pwrdb_led_id {
	LED_ID_PWR,
	LED_ID_PWR_BUTTON,

	LED_ID_COUNT
};

int set_pwrdb_led_color(enum pwrdb_led_id led, enum pwrdb_led_color color,
			int force);
