/* Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ZynqMP chipset power control module for Chrome EC */

#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Long power key press to force shutdown in S0 */
#define FORCED_SHUTDOWN_DELAY  (8 * SECOND)

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* Data structure for a GPIO operation for power sequencing */
struct power_seq_op {
	/* enum gpio_signal in 8 bits */
	uint8_t signal;
	uint8_t level;
	/* Number of milliseconds to delay after setting signal to level */
	uint8_t delay;
};
BUILD_ASSERT(GPIO_COUNT < 256);

#if 0
/**
 * Step through the power sequence table and do corresponding GPIO operations.
 *
 * @param	power_seq_ops	The pointer to the power sequence table.
 * @param	op_count	The number of entries of power_seq_ops.
 * @return	non-zero if error, 0 otherwise.
 */
static int power_seq_run(const struct power_seq_op *power_seq_ops, int op_count)
{
	int i;

	for (i = 0; i < op_count; i++) {
		gpio_set_level(power_seq_ops[i].signal,
			       power_seq_ops[i].level);
		if (!power_seq_ops[i].delay)
			continue;
		msleep(power_seq_ops[i].delay);
	}
	return 0;
}
#endif

enum power_state power_handle_state(enum power_state state)
{
	return POWER_G3;
}

static int forcing_shutdown;

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
	CPRINTS("%s(%d)", __func__, reason);
	report_ap_reset(reason);

	/*
	 * Force power off. This condition will reset once the state machine
	 * transitions to G3.
	 */
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
}

void chipset_reset(enum chipset_reset_reason reason)
{
#ifdef CONFIG_CMD_RTC
	/* Print out the RTC to help correlate resets in logs. */
	print_system_rtc(CC_CHIPSET);
#endif
	CPRINTS("%s(%d)", __func__, reason);
	report_ap_reset(reason);
}

enum power_state power_chipset_init(void)
{
	if (system_jumped_to_this_image()) {
		if ((power_get_signals() /*& IN_ALL_S0*/)/* == IN_ALL_S0*/) {
			disable_sleep(SLEEP_MASK_AP_RUN);
			CPRINTS("already in S0");
			return POWER_S0;
		}
	} else if (!(system_get_reset_flags() & EC_RESET_FLAG_AP_OFF))
		/* Auto-power on */
		chipset_exit_hard_off();

	return POWER_G3;
}


static void force_shutdown(void)
{
	forcing_shutdown = 1;
	task_wake(TASK_ID_CHIPSET);
}
DECLARE_DEFERRED(force_shutdown);

static void power_button_changed(void)
{
	if (power_button_is_pressed()) {
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			chipset_exit_hard_off();
		/* Delayed power down from S0/S3, cancel on PB release */
		hook_call_deferred(&force_shutdown_data,
				   FORCED_SHUTDOWN_DELAY);
	} else {
		/* Power button released, cancel deferred shutdown */
		hook_call_deferred(&force_shutdown_data, -1);
	}
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, power_button_changed, HOOK_PRIO_DEFAULT);
