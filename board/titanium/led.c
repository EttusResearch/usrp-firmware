/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NI Project Titanium Firmware */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "ioexpander.h"
#include "led.h"
#include "system.h"
#include "util.h"

struct pwrdb_led {
	enum ioex_signal red_signal;
	enum ioex_signal green_signal;
};

static const struct pwrdb_led supported_leds[] = {
	[LED_ID_PCIE] =		{ IOEX_PWRDB_LED0R_L, IOEX_PWRDB_LED0G_L },
	[LED_ID_SYS] =		{ IOEX_PWRDB_LED1R_L, IOEX_PWRDB_LED1G_L },
	[LED_ID_USER] = 	{ IOEX_PWRDB_LED2R_L, IOEX_PWRDB_LED2G_L },
	[LED_ID_PWR] =		{ IOEX_PWRDB_LED3R_L, IOEX_PWRDB_LED3G_L },
	[LED_ID_PWR_BUTTON] =	{ IOEX_PWRDB_PWRLEDB_L, IOEX_PWRDB_PWRLEDA_L }
};
BUILD_ASSERT(ARRAY_SIZE(supported_leds) == LED_ID_COUNT);

enum pwrdb_led_color led_color_states[] = {
	[LED_ID_PCIE] =	LED_OFF,
	[LED_ID_SYS] =	LED_OFF,
	[LED_ID_USER] = LED_OFF,
	[LED_ID_PWR] =	LED_OFF,
	[LED_ID_PWR_BUTTON] = LED_OFF
};
BUILD_ASSERT(ARRAY_SIZE(led_color_states) == LED_ID_COUNT);

void init_pwrdb_led_states(void)
{
	int is_jumped = system_jumped_to_this_image();
	if (is_jumped == 0) {
		for (int i = 0; i < LED_ID_COUNT; i++) {
			led_color_states[i] = LED_OFF;
			set_pwrdb_led_color(i, LED_OFF, 1 /* force */);
		}
	}
}
DECLARE_HOOK(HOOK_INIT, init_pwrdb_led_states, HOOK_PRIO_DEFAULT);

int set_pwrdb_led_color(enum pwrdb_led_id led, enum pwrdb_led_color color,
			int force)
{
	int red = 1, green = 1;

	if (led_color_states[led] == color && !force)
		return EC_SUCCESS;

	/*
	 * Power button LED drive circuitry does not support both LEDs
	 * in the power button unit to be turned on at the same time.
	 */
	if (led == LED_ID_PWR_BUTTON && color == LED_AMBER) {
		ccprintf("Power Button LED does not support amber color.\n");
		return EC_ERROR_INVAL;
	}

	switch (color) {
	case LED_OFF:
		break;
	case LED_RED:
		red = 0;
		break;
	case LED_GREEN:
		green = 0;
		break;
	case LED_AMBER:
	/* Amber is derived by mixing red and green. */
		red = 0;
		green = 0;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	ioex_set_level(supported_leds[led].red_signal, red);
	ioex_set_level(supported_leds[led].green_signal, green);

	led_color_states[led] = color;

	return EC_SUCCESS;
}

#ifdef CONFIG_CMD_LED
static int command_led(int argc, char **argv)
{
	enum pwrdb_led_id id;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "pcie"))
		id = LED_ID_PCIE;
	else if (!strcasecmp(argv[1], "sys"))
		id = LED_ID_SYS;
	else if (!strcasecmp(argv[1], "user"))
		id = LED_ID_USER;
	else if (!strcasecmp(argv[1], "pwr"))
		id = LED_ID_PWR;
	else if (!strcasecmp(argv[1], "pwrbutton"))
		id = LED_ID_PWR_BUTTON;
	else
		return EC_ERROR_PARAM1;

	if (!strcasecmp(argv[2], "off"))
		set_pwrdb_led_color(id, LED_OFF, 1);
	else if (!strcasecmp(argv[2], "red"))
		set_pwrdb_led_color(id, LED_RED, 1);
	else if (!strcasecmp(argv[2], "green"))
		set_pwrdb_led_color(id, LED_GREEN, 1);
	else if (!strcasecmp(argv[2], "amber"))
		set_pwrdb_led_color(id, LED_AMBER, 1);
	else
		return EC_ERROR_PARAM2;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led,
			"<pcie|sys|user|pwr|pwrbutton> <red|green|amber|off>",
			"Configure LED.");
#endif
