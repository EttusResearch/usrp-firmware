/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "clock.h"
#include "common.h"
#include "power.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "hooks.h"
#include "power_button.h"


#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define IN_PGOOD_AP POWER_SIGNAL_MASK(SYS_PS_PWRON)
#define IN_PGOOD_S0 POWER_SIGNAL_MASK(SYS_PS_PWRON)

#define PGOOD_AP_FIRST_TIMEOUT (1 * SECOND)
#define PGOOD_AP_DEBOUNCE_TIMEOUT (1 * SECOND)
#define AP_RST_HOLD_US (1 * MSEC)

#define FORCED_SHUTDOWN_DELAY  (3 * SECOND)

static void ap_set_reset(int asserted)
{
	gpio_set_level(GPIO_PS_POR_RESET_L, !asserted);
}

enum power_state power_chipset_init(void)
{
	if (system_jumped_to_this_image()) {
		if ((power_get_signals() & IN_PGOOD_S0) == IN_PGOOD_S0) {
			disable_sleep(SLEEP_MASK_AP_RUN);
			return POWER_S0;
		}
	} else if (!(system_get_reset_flags() & RESET_FLAG_AP_OFF)) {
		/* make sure we come up if EC crashes */
		if (system_get_reset_flags() & RESET_FLAG_SOFT)
			chipset_exit_hard_off();

		/* make sure we come up if EC crashes */
		if (system_get_reset_flags() & RESET_FLAG_WATCHDOG)
			chipset_exit_hard_off();
	}

	return POWER_G3;
}

static int forcing_shutdown;

void chipset_force_shutdown(void)
{
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
}

void chipset_reset(int cold_reset)
{
	ap_set_reset(1);

	if (in_interrupt_context())
		udelay(AP_RST_HOLD_US);
	else
		usleep(AP_RST_HOLD_US);

	ap_set_reset(0);
}

static void force_shutdown(void)
{
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
}
DECLARE_DEFERRED(force_shutdown);

enum power_state power_handle_state(enum power_state state)
{
	switch (state) {
	case POWER_G3:
		CPRINTS("in G3");
		break;

	case POWER_G3S5:
		CPRINTS("in G3G5");
		forcing_shutdown = 0;
		return POWER_S5;

	case POWER_S5G3:
		CPRINTS("in S5G3");
		return POWER_G3;

	case POWER_S5:
		CPRINTS("in S5");
		if (forcing_shutdown)
			return POWER_S5G3;
		else
			return POWER_S5S3;
	
	case POWER_S5S3:
		CPRINTS("in S5G3");
		ap_set_reset(1);
		gpio_set_level(GPIO_PWR_1V0_EN_L, 0);
		msleep(10);
		hook_notify(HOOK_CHIPSET_PRE_INIT);
		return POWER_S3;

	case POWER_S3S5:
		CPRINTS("in S3G5");
		hook_notify(HOOK_CHIPSET_SHUTDOWN);

		gpio_set_level(GPIO_PWR_1V0_EN_L, 1);

		/* Change EC_INT_L pin to high-Z to reduce power draw. */
		gpio_set_flags(GPIO_EC_INT_L, GPIO_INPUT);

		ap_set_reset(1);

		/* Start shutting down */
		return POWER_S5;

	case POWER_S3:
		CPRINTS("in S3, shutting down: %u", forcing_shutdown);
		if (forcing_shutdown)
			return POWER_S3S5;

		gpio_set_level(GPIO_PWR_1V8_EN, 1);
		msleep(5);
		gpio_set_level(GPIO_PWR_1V3_EN, 1);
		msleep(5);
		gpio_set_level(GPIO_PWR_3V3_EN, 1);
		msleep(5);
		gpio_set_level(GPIO_PWR_MGTVTT_EN, 1);
		gpio_set_level(GPIO_PWR_MGTVCC_EN, 1);
		msleep(5);
		gpio_set_level(GPIO_PWR_3V8_EN, 1);
		msleep(5);
		gpio_set_level(GPIO_PWR_CLK_EN, 1);
		msleep(5);
		gpio_set_level(GPIO_PWR_1V5_EN, 1);
		usleep(15);
		ap_set_reset(0);
		usleep(15);

		return POWER_S3S0;

	case POWER_S3S0:
		if (power_wait_signals_timeout(IN_PGOOD_S0, PGOOD_AP_FIRST_TIMEOUT)
				== EC_ERROR_TIMEOUT) {
				chipset_force_shutdown();
				return POWER_S0S3;
		}

		hook_notify(HOOK_CHIPSET_RESUME);
		disable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S0;

	case POWER_S0S3:
		hook_notify(HOOK_CHIPSET_SUSPEND);

		gpio_set_level(GPIO_PWR_1V5_EN, 0);

		gpio_set_level(GPIO_PWR_CLK_EN, 0);
		msleep(5);
		gpio_set_level(GPIO_PWR_3V8_EN, 0);
		msleep(5);
		gpio_set_level(GPIO_PWR_MGTVCC_EN, 0);
		gpio_set_level(GPIO_PWR_MGTVTT_EN, 0);
		msleep(5);
		gpio_set_level(GPIO_PWR_3V3_EN, 0);
		msleep(5);

		gpio_set_level(GPIO_PWR_1V8_EN, 0);
		msleep(5);
		gpio_set_level(GPIO_PWR_1V3_EN, 0);
		msleep(5);

		/* Enable idle task deep sleep. */
		enable_sleep(SLEEP_MASK_AP_RUN);

		if (power_button_is_pressed()) {
			forcing_shutdown = 1;
			hook_call_deferred(&force_shutdown_data, -1);
		}

		return POWER_S3;

	case POWER_S0:
		if (power_wait_signals_timeout(IN_PGOOD_S0, PGOOD_AP_DEBOUNCE_TIMEOUT)
				== EC_ERROR_TIMEOUT) {
			forcing_shutdown = 1;
			return POWER_S0S3;
		}

		if (!power_has_signals(IN_PGOOD_S0) || forcing_shutdown)
			return POWER_S0S3;

		break;
	};

	return state;
}

static void powerbtn_neon_changed(void)
{
	if (power_button_is_pressed()) {
		CPRINTS("power button is pressed");
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			chipset_exit_hard_off();
		hook_call_deferred(&force_shutdown_data, FORCED_SHUTDOWN_DELAY);
	} else {
		hook_call_deferred(&force_shutdown_data, -1);
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, powerbtn_neon_changed, HOOK_PRIO_DEFAULT);

static int command_power_seq(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	/* Keep AP in Reset */
	ap_set_reset(1);

	gpio_set_level(GPIO_PWR_1V0_EN_L, 0);
	msleep(10);
	gpio_set_level(GPIO_PWR_1V8_EN, 1);
	msleep(5);
	gpio_set_level(GPIO_PWR_1V3_EN, 1);
	msleep(5);
	gpio_set_level(GPIO_PWR_3V3_EN, 1);
	msleep(5);
	gpio_set_level(GPIO_PWR_MGTVTT_EN, 1);
	gpio_set_level(GPIO_PWR_MGTVCC_EN, 1);
	msleep(5);
	gpio_set_level(GPIO_PWR_3V8_EN, 1);
	msleep(5);
	gpio_set_level(GPIO_PWR_CLK_EN, 1);
	msleep(5);

	/* Turn on (AP/FPGA) DDR */
	gpio_set_level(GPIO_PWR_1V5_EN, 1);

	/* Take AP out of reset */
	ap_set_reset(0);
	msleep(50);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(power_on, command_power_seq,
			NULL, "Power on FPGA/ARM");
