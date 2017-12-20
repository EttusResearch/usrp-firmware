/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"  /* This module implements chipset functions too */
#include "clock.h"
#include "common.h"
#include "console.h"
#include "eeprom.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "power_led.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define IN_PGOOD_AP  POWER_SIGNAL_MASK(SYS_PWRON_33)
#define IN_PGOOD_1V  POWER_SIGNAL_MASK(POWER_PG_1V)
#define IN_PGOOD_5V  POWER_SIGNAL_MASK(POWER_PG_5V)
#define IN_PGOOD_MGT POWER_SIGNAL_MASK(POWER_PG_MGT)
#define IN_PGOOD_1V5 POWER_SIGNAL_MASK(POWER_PG_1V5)
#define IN_PGOOD_IO  POWER_SIGNAL_MASK(POWER_PG_IO)
#define IN_PGOOD_3V7 POWER_SIGNAL_MASK(POWER_PG_3V7)


#ifdef CONFIG_SULFUR_5V_WORKAROUND
 #define IN_PGOOD_S5 IN_PGOOD_1V
#else
 #define IN_PGOOD_S5 (IN_PGOOD_5V | IN_PGOOD_1V)
#endif /* CONFIG_SULFUR_5V_WORKAROUND */

#define IN_PGOOD_S3 (IN_PGOOD_S5 | IN_PGOOD_1V5 | IN_PGOOD_MGT | IN_PGOOD_IO | IN_PGOOD_3V7)

#define IN_PGOOD_S0 (IN_PGOOD_S3 | IN_PGOOD_AP)

#define FORCED_SHUTDOWN_DELAY  (3 * SECOND)
#define FORCED_RESET_DELAY  (50 * MSEC)

#define PGOOD_AP_DEBOUNCE_TIMEOUT (1 * SECOND)
#define PGOOD_AP_DEBOUNCE_TIMEOUT_REV5 (6 * SECOND)
#define PGOOD_AP_FIRST_TIMEOUT (1 * SECOND)
#define PGOOD_AP_FIRST_TIMEOUT_REV5 (3 * SECOND)

static int forcing_shutdown;
static int wdt_reset;
static int is_rev5;

static void power_dump_signals(void)
{
	int i;
	uint32_t signals;
	const struct power_signal_info *s = power_signal_list;

	signals = power_get_signals();

	CPRINTS("power in:   0x%04x\n", signals);

	/* Print the decode */
	CPRINTS("bit meanings:");
	for (i = 0; i < POWER_SIGNAL_COUNT; i++, s++) {
		int mask = 1 << i;
		ccprintf("  0x%04x %d %s\n",
			 mask, signals & mask ? 1 : 0, s->name);
	}
}

static void ap_set_reset(int asserted)
{
	/* reset is low active */
	gpio_set_level(GPIO_PS_POR_RESET_L, !asserted);
}

void chipset_force_shutdown(void)
{
	/*
	 * Force power off. This condition will reset once the state machine
	 * transitions to G3.
	 */
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
}

#define AP_RST_HOLD_US (1 * MSEC)
void chipset_reset(int cold_reset)
{
#ifdef CONFIG_CMD_RTC
	/* Print out the RTC to help correlate resets in logs. */
	print_system_rtc(CC_CHIPSET);
#endif

	CPRINTS("%s(%d)", __func__, cold_reset);

	ap_set_reset(1);
	gpio_set_flags(GPIO_PS_BOOTSEL, GPIO_OUTPUT);
	gpio_set_level(GPIO_PS_BOOTSEL, 1);
	gpio_set_flags(GPIO_JTAG_SEL, GPIO_OUTPUT);
	gpio_set_level(GPIO_JTAG_SEL, 0);

	if (in_interrupt_context())
		udelay(AP_RST_HOLD_US);
	else
		usleep(AP_RST_HOLD_US);
	ap_set_reset(0);
	gpio_set_flags(GPIO_PS_BOOTSEL, GPIO_INPUT);
	gpio_set_flags(GPIO_JTAG_SEL, GPIO_INPUT);
}

enum power_state power_chipset_init(void)
{
	int rev;

	/* if error, i.e. not initialized, or 5, behave like 5 */
	rev = eeprom_get_board_rev();
	if (rev < 0 || (rev + 1) == 5) {
		CPRINTS("Enabling rev5 workaround");
		is_rev5 = 1;
	}

	if (system_jumped_to_this_image()) {
		if ((power_get_signals() & IN_PGOOD_S0) == IN_PGOOD_S0) {
			disable_sleep(SLEEP_MASK_AP_RUN);
			CPRINTS("already in S0");
			return POWER_S0;
		}
	} else if (!(system_get_reset_flags() & RESET_FLAG_AP_OFF)) {
		/* Auto-power on ? */
		if (eeprom_get_autoboot())
			chipset_exit_hard_off();

		/* make sure we come up if EC crashes */
		if (system_get_reset_flags() & RESET_FLAG_SOFT)
			chipset_exit_hard_off();

		/* make sure we come up if EC crashes */
		if (system_get_reset_flags() & RESET_FLAG_WATCHDOG)
			chipset_exit_hard_off();

	}

	return POWER_G3;
}

static void force_shutdown(void)
{
	forcing_shutdown = 1;
	CPRINTS("Forcing shutdown ...");
	task_wake(TASK_ID_CHIPSET);
}
DECLARE_DEFERRED(force_shutdown);

enum power_state power_handle_state(enum power_state state)
{
	switch (state) {
	case POWER_G3:
		break;
	case POWER_S5:
		if (forcing_shutdown)
			return POWER_S5G3;
		else
			return POWER_S5S3;
		break;

	case POWER_S3:
		if(!power_has_signals(IN_PGOOD_S5) || forcing_shutdown)
			return POWER_S3S5;

		gpio_set_level(GPIO_POWER_EN_MGT, 1);
		usleep(5);
		gpio_set_level(GPIO_POWER_EN_1V5, 1);
		usleep(5);
		gpio_set_level(GPIO_POWER_EN_IO, 1);
		usleep(5);
		gpio_set_level(GPIO_POWER_EN_3V7, 1);
		usleep(5);

		if (power_wait_signals_timeout(IN_PGOOD_S3, 100 * MSEC)
		    == EC_ERROR_TIMEOUT) {
			if (!power_has_signals(IN_PGOOD_S3)) {
				power_dump_signals();
				chipset_force_shutdown();
				return POWER_S3S5;
			}
		}

		gpio_set_flags(GPIO_PS_BOOTSEL, GPIO_OUTPUT);
		gpio_set_level(GPIO_PS_BOOTSEL, 1);
		gpio_set_flags(GPIO_JTAG_SEL, GPIO_OUTPUT);
		gpio_set_level(GPIO_JTAG_SEL, 0);
		usleep(5);
		ap_set_reset(0);
		usleep(15);
		gpio_set_flags(GPIO_PS_BOOTSEL, GPIO_INPUT);
		gpio_set_flags(GPIO_JTAG_SEL, GPIO_INPUT);

		return POWER_S3S0;

	case POWER_S0:
		if (!power_has_signals(IN_PGOOD_S3) || forcing_shutdown)
			return POWER_S0S3;

		/*
		 * Wait up to PGOOD_AP_DEBOUNCE_TIMEOUT for IN_PGOOD_AP to
		 * come back before transitioning back to S3. PGOOD_SYS can
		 * also glitch, with a glitch duration < 1ms, so debounce
		 * it here as well.
		 */
		if (power_wait_signals_timeout(IN_PGOOD_AP,
					       is_rev5 ? PGOOD_AP_DEBOUNCE_TIMEOUT_REV5
					       : PGOOD_AP_DEBOUNCE_TIMEOUT)
					       == EC_ERROR_TIMEOUT) {
			forcing_shutdown = 1;
			return POWER_S0S3;
		}

		/*
		 * power_wait_signals_timeout() can block and consume task
		 * wake events, so re-verify the state of the world.
		 */
		if (!power_has_signals(IN_PGOOD_S3) || forcing_shutdown)
			return POWER_S0S3;

		break;

	case POWER_G3S5:
		forcing_shutdown = 0;

		return POWER_S5;

	case POWER_S5G3:
		return POWER_G3;

	case POWER_S5S3:
		ap_set_reset(1);

		gpio_set_level(GPIO_POWER_EN_1V_L, 0);
		usleep(5);
		gpio_set_level(GPIO_POWER_EN_5V, 1);

		if (power_wait_signals_timeout(IN_PGOOD_S5,
					       5 * MSEC) == EC_ERROR_TIMEOUT)
			return POWER_S0S3;

		hook_notify(HOOK_CHIPSET_PRE_INIT);
		return POWER_S3;

	case POWER_S3S5:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN);

		gpio_set_level(GPIO_POWER_EN_5V, 0);
		usleep(5);
		gpio_set_level(GPIO_POWER_EN_1V_L, 1);

		/* Change EC_INT_L pin to high-Z to reduce power draw. */
		gpio_set_flags(GPIO_EC_INT_L, GPIO_INPUT);

		/* Start shutting down */
		return POWER_S5;

	case POWER_S0S3:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SUSPEND);

		gpio_set_level(GPIO_POWER_EN_MGT, 0);
		gpio_set_level(GPIO_POWER_EN_1V5, 0);
		gpio_set_level(GPIO_POWER_EN_IO, 0);
		gpio_set_level(GPIO_POWER_EN_3V7, 0);

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S3 or lower.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);

		/*
		 * In case the power button is held awaiting power-off timeout,
		 * power off immediately now that we're entering S3.
		 */
		if (power_button_is_pressed()) {
			forcing_shutdown = 1;
			hook_call_deferred(&force_shutdown_data, -1);
		}

		return POWER_S3;

	case POWER_S3S0:
		if (power_wait_signals_timeout(IN_PGOOD_S0,
					       is_rev5 ? PGOOD_AP_FIRST_TIMEOUT_REV5
					       : PGOOD_AP_FIRST_TIMEOUT)
		    == EC_ERROR_TIMEOUT) {
			chipset_force_shutdown();
			return POWER_S0S3;
		}

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_RESUME);

		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S0;
	default:
		break;
	}

	return state;
}

static void powerbtn_sulfur_changed(void)
{
	if (power_button_is_pressed()) {
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			/* Power up from off */
			chipset_exit_hard_off();

		/* Delayed power down from S0/S3, cancel on PB release */
		hook_call_deferred(&force_shutdown_data,
				   FORCED_SHUTDOWN_DELAY);
	} else {
		/* Power button released, cancel deferred shutdown */
		hook_call_deferred(&force_shutdown_data, -1);
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, powerbtn_sulfur_changed,
	     HOOK_PRIO_DEFAULT);

static void force_reset(void)
{
	chipset_reset(1);
	if (wdt_reset) {
		wdt_reset = 0;
		chipset_exit_hard_off();
	}
}
DECLARE_DEFERRED(force_reset);

static int reset_button_is_pressed(void)
{
	return !gpio_get_level(GPIO_RESET_BUTTON_L);
}

static void reset_button_poll(void)
{
	if (reset_button_is_pressed())
		hook_call_deferred(&force_reset_data, 50 * MSEC);
	else
		hook_call_deferred(&force_reset_data, -1);
}
DECLARE_HOOK(HOOK_TICK, reset_button_poll, HOOK_PRIO_DEFAULT);

void wdt_reset_event(enum gpio_signal signal)
{
	CPRINTS("Watchdog timeout, warm reset the AP");
	wdt_reset = 1;
	host_set_single_event(EC_HOST_EVENT_HANG_REBOOT);
	hook_call_deferred(&force_reset_data, 10 * MSEC);
}

